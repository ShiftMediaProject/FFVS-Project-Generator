/*
 * copyright (c) 2017 Matthew Oliver
 *
 * This file is part of ShiftMediaProject.
 *
 * ShiftMediaProject is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ShiftMediaProject is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ShiftMediaProject; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "projectGenerator.h"

#include <algorithm>
#include <utility>

bool ProjectGenerator::runCompiler(
    const vector<string>& includeDirs, map<string, vector<string>>& directoryObjects, const int runType) const
{
#ifdef _MSC_VER
    // If compiled by msvc then only msvc builds are supported
    return runMSVC(includeDirs, directoryObjects, runType);
#else
    // Otherwise only gcc and mingw are supported
    return runGCC(includeDirs, directoryObjects, runType);
#endif
}

bool ProjectGenerator::runMSVC(
    const vector<string>& includeDirs, map<string, vector<string>>& directoryObjects, int runType) const
{
    // Create a test file to read in definitions
    string outDir = m_configHelper.m_outDirectory;
    m_configHelper.makeFileGeneratorRelative(outDir, outDir);
    string projectDir = m_configHelper.m_solutionDirectory;
    m_configHelper.makeFileGeneratorRelative(projectDir, projectDir);
    vector<string> includeDirs2 = includeDirs;
    includeDirs2.insert(includeDirs2.begin(), outDir + "include/");
    includeDirs2.insert(includeDirs2.begin(), m_configHelper.m_solutionDirectory);
    includeDirs2.insert(includeDirs2.begin(), m_configHelper.m_rootDirectory);
    string extraCl;
    for (auto& i : includeDirs2) {
        uint findPos2 = i.find("$(OutDir)");
        if (findPos2 != string::npos) {
            i.replace(findPos2, 9, outDir);
        }
        findPos2 = i.find("$(ProjectDir)");
        if (findPos2 != string::npos) {
            i.replace(findPos2, 13, projectDir);
        }
        findPos2 = i.find("$(");
        if (findPos2 != string::npos) {
            i.replace(findPos2, 2, "%");
        }
        findPos2 = i.find(')');
        if (findPos2 != string::npos) {
            i.replace(findPos2, 1, "%");
        }
        if (i.length() == 0) {
            i = "./";
        } else if ((i.find(':') == string::npos) && (i.find_first_of("./%") != 0)) {
            i.insert(0, "./");
        }
        // Clean duplicate '//'
        findPos2 = i.find("//");
        while (findPos2 != string::npos) {
            i.erase(findPos2, 1);
            // get next
            findPos2 = i.find("//");
        }
        // Remove ../ not at start of path
        findPos2 = i.find("/../");
        while (findPos2 != string::npos) {
            // Get the previous '/'
            uint findPos3 = i.rfind('/', findPos2 - 1);
            findPos3 = (findPos3 != string::npos) ? findPos3 : 0;
            if (i.find_first_not_of('.', findPos3 + 1) < findPos2) {
                i.erase(findPos3, findPos2 - findPos3 + 3);
                // get next
                findPos2 = i.find("/../", findPos3);
            } else {
                // get next
                findPos2 = i.find("/../", findPos2 + 1);
            }
        }
        extraCl += " /I\"" + i + '\"';
    }
    string tempFolder = m_tempDirectory + m_projectName;

    // Use Microsoft compiler to pass the test file and retrieve declarations
    string launchBat = "@echo off\nsetlocal enabledelayedexpansion\nset CALLDIR=%CD%\n";
    launchBat += "if \"%PROCESSOR_ARCHITECTURE%\"==\"AMD64\" (\n\
    set SYSARCH=64\n\
) else if \"%PROCESSOR_ARCHITECTURE%\"==\"x86\" (\n\
    if \"%PROCESSOR_ARCHITEW6432%\"==\"AMD64\" (\n\
        set SYSARCH=64\n\
    ) else (\n\
        set SYSARCH=32\n\
    )\n\
) else (\n\
    echo fatal error : Current Platform Architecture could not be detected. > ffvs_log.txt\n\
    exit /b 1\n\
)\n\
if \"%SYSARCH%\"==\"32\" (\n\
    set MSVCVARSDIR=\n\
    set VSWHERE=\"%ProgramFiles%\\Microsoft Visual Studio\\Installer\\vswhere.exe\"\n\
) else if \"%SYSARCH%\"==\"64\" (\n\
    set MSVCVARSDIR=\\amd64\n\
    set VSWHERE=\"%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe\"\n\
)\n\
if exist %VSWHERE% (\n\
    for /f \"usebackq delims=\" %%i in (`%VSWHERE% -prerelease -latest -property installationPath`) do (set VSINSTDIR=%%i)\n\
    if exist \"!VSINSTDIR!\\VC\\Auxiliary\\Build\\vcvars%SYSARCH%.bat\" (\n\
        call \"!VSINSTDIR!\\VC\\Auxiliary\\Build\\vcvars%SYSARCH%.bat\" >nul 2>&1\n\
        goto MSVCVarsDone\n\
    )\n\
    if exist \"!VSINSTDIR!\\..\\..\\VC\\bin%MSVCVARSDIR%\\vcvars%SYSARCH%.bat\" (\n\
        call \"!VSINSTDIR!\\..\\..\\VC\\bin%MSVCVARSDIR%\\vcvars%SYSARCH%.bat\" >nul 2>&1\n\
        goto MSVCVarsDone\n\
    )\n\
)\n\
if \"%SYSARCH%\"==\"32\" (\n\
    set WOWNODE=\n\
) else if \"%SYSARCH%\"==\"64\" (\n\
    set WOWNODE=\\WOW6432Node\n\
)\n\
pushd %CD%\n\
reg.exe query \"HKLM\\SOFTWARE%WOWNODE%\\Microsoft\\VisualStudio\\SxS\\VS7\" /v 15.0 >nul 2>&1\n\
if not ERRORLEVEL 1 (\n\
    for /f \"skip=2 tokens=2,*\" %%a in ('reg.exe query \"HKLM\\SOFTWARE%WOWNODE%\\Microsoft\\VisualStudio\\SxS\\VS7\" /v 15.0') do (set VSINSTDIR=%%b)\n\
    call \"!VSINSTDIR!VC\\Auxiliary\\Build\\vcvars%SYSARCH%.bat\" >nul 2>&1\n\
    goto MSVCVarsDone\n\
)\n\
reg.exe query \"HKLM\\Software%WOWNODE%\\Microsoft\\VisualStudio\\14.0\" /v \"InstallDir\" >nul 2>&1\n\
if not ERRORLEVEL 1 (\n\
    for /f \"skip=2 tokens=2,*\" %%a in ('reg.exe query \"HKLM\\Software%WOWNODE%\\Microsoft\\VisualStudio\\14.0\" /v \"InstallDir\"') do (set VSINSTDIR=%%b)\n\
    call \"!VSINSTDIR!\\..\\..\\VC\\bin%MSVCVARSDIR%\\vcvars%SYSARCH%.bat\" >nul 2>&1\n\
    goto MSVCVarsDone\n\
)\n\
reg.exe query \"HKLM\\Software%WOWNODE%\\Microsoft\\VisualStudio\\12.0\" /v \"InstallDir\" >nul 2>&1\n\
if not ERRORLEVEL 1 (\n\
    for /f \"skip=2 tokens=2,*\" %%a in ('reg.exe query \"HKLM\\Software%WOWNODE%\\Microsoft\\VisualStudio\\12.0\" /v \"InstallDir\"') do (set VSINSTDIR=%%b)\n\
    call \"!VSINSTDIR!\\..\\..\\VC\\bin%MSVCVARSDIR%\\vcvars%SYSARCH%.bat\" >nul 2>&1\n\
    goto MSVCVarsDone\n\
)\n\
echo fatal error : An installed version of Visual Studio could not be detected. > ffvs_log.txt\n\
exit /b 1\n\
:MSVCVarsDone\n\
popd\n";
    launchBat += "mkdir \"" + m_tempDirectory + "\" > nul 2>&1\n";
    launchBat += "mkdir \"" + tempFolder + "\" > nul 2>&1\n";
    for (auto& j : directoryObjects) {
        const uint rowSize = 32;
        uint numClCalls = static_cast<uint>(ceilf(static_cast<float>(j.second.size()) / static_cast<float>(rowSize)));
        uint totalPos = 0;
        string dirName = tempFolder + "/" + j.first;
        if (j.first.length() > 0) {
            // Need to make output directory so compile doesn't fail outputting
            launchBat += "mkdir \"" + dirName + "\" > nul 2>&1\n";
        }
        string runCommands;
        // Check type of compiler call
        if (runType == 0) {
            runCommands = "/FR\"" + dirName + "/\"" + " /Fo\"" + dirName + "/\"";
        } else if (runType == 1) {
            runCommands = "/EP /P";
        }

        // Split calls into groups of 50 to prevent batch file length limit
        for (uint i = 0; i < numClCalls; i++) {
            launchBat += "cl.exe ";
            launchBat += extraCl + R"( /D"_DEBUG" /D"WIN32" /D"_WINDOWS" /D"HAVE_AV_CONFIG_H" /D"_USE_MATH_DEFINES" )" +
                runCommands + " /c /MP /w /nologo";
            uint uiStartPos = totalPos;
            for (; totalPos < min(uiStartPos + rowSize, j.second.size()); totalPos++) {
                if (runType == 0) {
                    m_configHelper.makeFileGeneratorRelative(j.second[totalPos], j.second[totalPos]);
                }
                launchBat += " \"" + j.second[totalPos] + "\"";
            }
            launchBat += " > ffvs_log.txt 2>&1\nif %errorlevel% neq 0 goto exitFail\n";
        }
        if (runType == 1) {
            launchBat += "move *.i " + dirName + "/ >nul 2>&1\n";
        }
    }
    if (runType == 0) {
        launchBat += "del /F /S /Q *.obj >nul 2>&1\n";
    }
    launchBat += "del ffvs_log.txt >nul 2>&1\n";
    launchBat += "exit /b 0\n:exitFail\n";
    if (runType == 1) {
        launchBat += "del /F /S /Q *.i >nul 2>&1\n";
    }
    launchBat += "rmdir /S /Q " + m_tempDirectory + "\nexit /b 1";
    if (!writeToFile("ffvs_compile.bat", launchBat)) {
        return false;
    }

    if (0 != system("ffvs_compile.bat")) {
        outputError("Errors detected during compilation :-");
        string testOutput;
        if (loadFromFile("ffvs_log.txt", testOutput)) {
            // Output errors from ffvs_log.txt
            bool error = false;
            bool missingVs = false;
            bool missingDeps = false;
            uint findPos = testOutput.find(" error ");
            while (findPos != string::npos) {
                // find end of line
                uint findPos2 = testOutput.find_first_of("\n(", findPos + 1);
                string temp = testOutput.substr(findPos + 1, findPos2 - findPos - 1);
                outputError(temp, false);
                findPos = testOutput.find(" error ", findPos2 + 1);
                // Check what type of error was found
                if (!missingDeps && (temp.find("open include file") != string::npos)) {
                    missingDeps = true;
                } else if (!missingVs && (temp.find("Visual Studio could not be detected") != string::npos)) {
                    missingVs = true;
                } else {
                    error = true;
                }
            }
            findPos = testOutput.find("internal or external command");
            if (findPos != string::npos) {
                uint findPos2 = testOutput.find('\n', findPos + 1);
                findPos = testOutput.rfind('\n', findPos);
                findPos = (findPos == string::npos) ? 0 : findPos;
                outputError(testOutput.substr(findPos, findPos2 - findPos), false);
                missingVs = true;
            }
            if (missingVs) {
                outputError(
                    "Based on the above error(s) Visual Studio is not installed correctly on the host system.", false);
                outputError("Install a compatible version of Visual Studio before trying again.", false);
                deleteFile("ffvs_log.txt");
            } else if (missingDeps) {
                outputError(
                    "Based on the above error(s) there are files required for dependency libraries that are not available",
                    false);
                outputError(
                    "Ensure that any required dependencies are available in 'OutDir' based on the supplied configuration options before trying again.",
                    false);
                outputError("Consult the supplied readme for instructions for installing varying dependencies.", false);
                outputError(
                    "If a dependency has been cloned from a ShiftMediaProject repository then ensure it has been successfully built before trying again.",
                    false);
                outputError(
                    "  Removing the offending configuration option can also be used to remove the error.", false);
                deleteFile("ffvs_log.txt");
            } else if (error) {
                outputError("Unknown error detected. See ffvs_log.txt for further details.", false);
            }
        }
        // Remove the compile files
        deleteFile("ffvs_compile.bat");
        deleteFolder(m_tempDirectory);
        return false;
    }

    // Remove the compilation objects
    deleteFile("ffvs_compile.bat");
    return true;
}

bool ProjectGenerator::runGCC(
    const vector<string>& includeDirs, map<string, vector<string>>& directoryObjects, int runType) const
{
    // Create a test file to read in definitions
    string outDir = m_configHelper.m_outDirectory;
    m_configHelper.makeFileGeneratorRelative(outDir, outDir);
    vector<string> includeDirs2 = includeDirs;
    includeDirs2.insert(includeDirs2.begin(), outDir + "include/");
    includeDirs2.insert(includeDirs2.begin(), m_configHelper.m_solutionDirectory);
    includeDirs2.insert(includeDirs2.begin(), m_configHelper.m_rootDirectory);
    string extraCl;
    for (auto& i : includeDirs2) {
        uint uiFindPos2 = i.find("$(OutDir)");
        if (uiFindPos2 != string::npos) {
            i.replace(uiFindPos2, 9, outDir);
        }
        uiFindPos2 = i.find("$(");
        if (uiFindPos2 != string::npos) {
            i.replace(uiFindPos2, 2, "%");
        }
        uiFindPos2 = i.find(')');
        if (uiFindPos2 != string::npos) {
            i.replace(uiFindPos2, 1, "%");
        }
        if (i.length() == 0) {
            i = "./";
        } else if (i.find_first_of("./%") != 0) {
            i = "./" + i;
        }
        extraCl += " /I\"" + i + '\"';
    }
    string tempFolder = m_tempDirectory + m_projectName;

    // Use GNU compiler to pass the test file and retrieve declarations
    string launchBat = "#!/bin/bash\n";
    launchBat += "exitFail() {\n";
    if (runType == 1) {
        launchBat += "rm -rf *.i > /dev/null 2>&1\n";
    }
    launchBat += "rm -rf " + m_tempDirectory + " > /dev/null 2>&1\nexit 1\n}\n";
    launchBat += "mkdir \"" + m_tempDirectory + "\" > /dev/null 2>&1\n";
    launchBat += "mkdir \"" + tempFolder + "\" > /dev/null 2>&1\n";
    for (auto& i : directoryObjects) {
        string dirName = tempFolder + "/" + i.first;
        if (i.first.length() > 0) {
            // Need to make output directory so compile doesn't fail outputting
            launchBat += "mkdir \"" + dirName + "\" > /dev/null 2>&1\n";
        }
        string runCommands;
        // Check type of compiler call
        if (runType == 0) {
            outputError("Generation of definitions is not supported using gcc.");
            return false;
        }
        if (runType == 1) {
            runCommands = "-E -P";
        }
        // Check if gcc or mingw
#ifdef _WIN32
        extraCl += R"(-D"WIN32" -D"_WINDOWS")";
#endif

        // Split calls as gcc outputs a single file at a time
        for (auto& j : i.second) {
            launchBat += "gcc ";
            launchBat += extraCl + " -D_DEBUG " + runCommands + " -c -w";
            if (runType == 0) {
                m_configHelper.makeFileGeneratorRelative(j, j);
            }
            launchBat += " \"" + j + "\"";
            if (runType == 0) {
                launchBat += " -o " + j + ".o";
            } else if (runType == 1) {
                launchBat += " -o " + j + ".i";
            }
            launchBat += " > ffvs_log.txt 2>&1\nif (( $? )); then\nexitFail\nfi\n";
        }
    }
    if (runType == 0) {
        launchBat += "rm -rf *.o > /dev/null 2>&1\n";
    }
    launchBat += "rm ffvs_log.txt > /dev/null 2>&1\n";
    launchBat += "exit 0\n";
    if (!writeToFile("ffvs_compile.sh", launchBat)) {
        return false;
    }
    if (0 != system("ffvs_compile.sh")) {
        outputError("Errors detected during compilation :-");
        outputError("Unknown error detected. See ffvs_log.txt for further details.", false);
        // Remove the compilation files
        deleteFile("ffvs_compile.sh");
        deleteFolder(m_tempDirectory);
        return false;
    }

    // Remove the compilation objects
    deleteFile("ffvs_compile.sh");
    return true;
}
