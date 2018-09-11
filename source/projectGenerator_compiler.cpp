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

bool ProjectGenerator::runCompiler(const vector<string> & vIncludeDirs, map<string, vector<string>> &mDirectoryObjects, int iRunType)
{
#ifdef _MSC_VER
    //If compiled by msvc then only msvc builds are supported
    return runMSVC(vIncludeDirs, mDirectoryObjects, iRunType);
#else
    //Otherwise only gcc and mingw are supported
    return runGCC(vIncludeDirs, mDirectoryObjects, iRunType);
#endif
}

bool ProjectGenerator::runMSVC(const vector<string> & vIncludeDirs, map<string, vector<string>> &mDirectoryObjects, int iRunType)
{
    //Create a test file to read in definitions
    string sOutDir = m_ConfigHelper.m_sOutDirectory;
    m_ConfigHelper.makeFileGeneratorRelative(sOutDir, sOutDir);
    vector<string> vIncludeDirs2 = vIncludeDirs;
    vIncludeDirs2.insert(vIncludeDirs2.begin(), sOutDir + "include/");
    vIncludeDirs2.insert(vIncludeDirs2.begin(), m_ConfigHelper.m_sSolutionDirectory);
    vIncludeDirs2.insert(vIncludeDirs2.begin(), m_ConfigHelper.m_sRootDirectory);
    string sCLExtra;
    for (StaticList::const_iterator vitIt = vIncludeDirs2.cbegin(); vitIt < vIncludeDirs2.cend(); vitIt++) {
        string sIncludeDir = *vitIt;
        uint uiFindPos2 = sIncludeDir.find("$(OutDir)");
        if (uiFindPos2 != string::npos) {
            sIncludeDir.replace(uiFindPos2, 9, sOutDir);
        }
        uiFindPos2 = sIncludeDir.find("$(");
        if (uiFindPos2 != string::npos) {
            sIncludeDir.replace(uiFindPos2, 2, "%");
        }
        uiFindPos2 = sIncludeDir.find(')');
        if (uiFindPos2 != string::npos) {
            sIncludeDir.replace(uiFindPos2, 1, "%");
        }
        if (sIncludeDir.length() == 0) {
            sIncludeDir = "./";
        } else if ((sIncludeDir.find(':') == string::npos) && (sIncludeDir.find_first_of("./%") != 0)) {
            sIncludeDir.insert(0, "./");
        }
        sCLExtra += " /I\"" + sIncludeDir + '\"';
    }
    string sTempFolder = sTempDirectory + m_sProjectName;

    //Use Microsoft compiler to pass the test file and retrieve declarations
    string sCLLaunchBat = "@echo off\nsetlocal enabledelayedexpansion\nset CALLDIR=%CD%\n";
    sCLLaunchBat += "if \"%PROCESSOR_ARCHITECTURE%\"==\"AMD64\" (\n\
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
    sCLLaunchBat += "mkdir \"" + sTempDirectory + "\" > nul 2>&1\n";
    sCLLaunchBat += "mkdir \"" + sTempFolder + "\" > nul 2>&1\n";
    for (map<string, StaticList>::iterator itI = mDirectoryObjects.begin(); itI != mDirectoryObjects.end(); itI++) {
        const uint uiRowSize = 32;
        uint uiNumCLCalls = (uint)ceilf((float)itI->second.size() / (float)uiRowSize);
        uint uiTotalPos = 0;
        string sDirName = sTempFolder + "/" + itI->first;
        if (itI->first.length() > 0) {
            //Need to make output directory so compile doesn't fail outputting
            sCLLaunchBat += "mkdir \"" + sDirName + "\" > nul 2>&1\n";
        }
        string sRuntype;
        //Check type of compiler call
        if (iRunType == 0) {
            sRuntype = "/FR\"" + sDirName + "/\"" + " /Fo\"" + sDirName + "/\"";
        } else if (iRunType == 1) {
            sRuntype = "/EP /P";
        }

        //Split calls into groups of 50 to prevent batch file length limit
        for (uint uiI = 0; uiI < uiNumCLCalls; uiI++) {
            sCLLaunchBat += "cl.exe ";
            sCLLaunchBat += sCLExtra + " /D\"_DEBUG\" /D\"WIN32\" /D\"_WINDOWS\" /D\"HAVE_AV_CONFIG_H\" /FI\"compat.h\" " + sRuntype + " /c /MP /w /nologo";
            uint uiStartPos = uiTotalPos;
            for (uiTotalPos; uiTotalPos < min(uiStartPos + uiRowSize, itI->second.size()); uiTotalPos++) {
                if (iRunType == 0) {
                    m_ConfigHelper.makeFileGeneratorRelative(itI->second[uiTotalPos], itI->second[uiTotalPos]);
                }
                sCLLaunchBat += " \"" + itI->second[uiTotalPos] + "\"";
            }
            sCLLaunchBat += " > ffvs_log.txt 2>&1\nif %errorlevel% neq 0 goto exitFail\n";
        }
        if (iRunType == 1) {
            sCLLaunchBat += "move *.i " + sDirName + "/ >nul 2>&1\n";
        }
    }
    if (iRunType == 0) {
        sCLLaunchBat += "del /F /S /Q *.obj >nul 2>&1\n";
    }
    sCLLaunchBat += "del ffvs_log.txt >nul 2>&1\n";
    sCLLaunchBat += "exit /b 0\n:exitFail\n";
    if (iRunType == 1) {
        sCLLaunchBat += "del /F /S /Q *.i >nul 2>&1\n";
    }
    sCLLaunchBat += "rmdir /S /Q " + sTempDirectory + "\nexit /b 1";
    if (!writeToFile("ffvs_compile.bat", sCLLaunchBat)) {
        return false;
    }

    if (0 != system("ffvs_compile.bat")) {
        outputError("Errors detected during compilation :-");
        string sTestOutput;
        if (loadFromFile("ffvs_log.txt", sTestOutput)) {
            //Output errors from ffvs_log.txt
            bool bError = false;
            bool bMissingVS = false;
            bool bMissingDeps = false;
            uint uiFindPos = sTestOutput.find(" error ");
            while (uiFindPos != string::npos) {
                //find end of line
                uint uiFindPos2 = sTestOutput.find_first_of("\n(", uiFindPos + 1);
                string sTemp = sTestOutput.substr(uiFindPos + 1, uiFindPos2 - uiFindPos - 1);
                outputError(sTemp, false);
                uiFindPos = sTestOutput.find(" error ", uiFindPos2 + 1);
                //Check what type of error was found
                if (!bMissingDeps && (sTemp.find("open include file") != string::npos)) {
                    bMissingDeps = true;
                } else if (!bMissingVS && (sTemp.find("Visual Studio could not be detected") != string::npos)) {
                    bMissingVS = true;
                } else {
                    bError = true;
                }
            }
            uiFindPos = sTestOutput.find("internal or external command");
            if (uiFindPos != string::npos) {
                uint uiFindPos2 = sTestOutput.find('\n', uiFindPos + 1);
                uiFindPos = sTestOutput.rfind('\n', uiFindPos);
                uiFindPos = (uiFindPos == string::npos) ? 0 : uiFindPos;
                outputError(sTestOutput.substr(uiFindPos, uiFindPos2 - uiFindPos), false);
                bMissingVS = true;
            }
            if (bMissingVS) {
                outputError("Based on the above error(s) Visual Studio is not installed correctly on the host system.", false);
                outputError("Install a compatible version of Visual Studio before trying again.", false);
                deleteFile("ffvs_log.txt");
            } else if (bMissingDeps) {
                outputError("Based on the above error(s) there are files required for dependency libraries that are not available", false);
                outputError("Ensure that any required dependencies are available in 'OutDir' based on the supplied configuration options before trying again.", false);
                outputError("Consult the supplied readme for instructions for installing varying dependencies.", false);
                outputError("If a dependency has been cloned from a ShiftMediaProject repository then ensure it has been successfully built before trying again.", false);
                outputError("  Removing the offending configuration option can also be used to remove the error.", false);
                deleteFile("ffvs_log.txt");
            } else if (bError) {
                outputError("Unknown error detected. See ffvs_log.txt for further details.", false);
            }
        }
        //Remove the compile files
        deleteFile("ffvs_compile.bat");
        deleteFolder(sTempDirectory);
        return false;
    }

    //Remove the compilation objects
    deleteFile("ffvs_compile.bat");
    return true;
}

bool ProjectGenerator::runGCC(const vector<string> & vIncludeDirs, map<string, vector<string>> &mDirectoryObjects, int iRunType)
{
    //Create a test file to read in definitions
    string sOutDir = m_ConfigHelper.m_sOutDirectory;
    m_ConfigHelper.makeFileGeneratorRelative(sOutDir, sOutDir);
    vector<string> vIncludeDirs2 = vIncludeDirs;
    vIncludeDirs2.insert(vIncludeDirs2.begin(), sOutDir + "include/");
    vIncludeDirs2.insert(vIncludeDirs2.begin(), m_ConfigHelper.m_sSolutionDirectory);
    vIncludeDirs2.insert(vIncludeDirs2.begin(), m_ConfigHelper.m_sRootDirectory);
    string sCLExtra;
    for (StaticList::const_iterator vitIt = vIncludeDirs2.cbegin(); vitIt < vIncludeDirs2.cend(); vitIt++) {
        string sIncludeDir = *vitIt;
        uint uiFindPos2 = sIncludeDir.find("$(OutDir)");
        if (uiFindPos2 != string::npos) {
            sIncludeDir.replace(uiFindPos2, 9, sOutDir);
        }
        uiFindPos2 = sIncludeDir.find("$(");
        if (uiFindPos2 != string::npos) {
            sIncludeDir.replace(uiFindPos2, 2, "%");
        }
        uiFindPos2 = sIncludeDir.find(")");
        if (uiFindPos2 != string::npos) {
            sIncludeDir.replace(uiFindPos2, 1, "%");
        }
        if (sIncludeDir.length() == 0) {
            sIncludeDir = "./";
        } else if (sIncludeDir.find_first_of("./%") != 0) {
            sIncludeDir = "./" + sIncludeDir;
        }
        sCLExtra += " /I\"" + sIncludeDir + '\"';
    }
    string sTempFolder = sTempDirectory + m_sProjectName;

    //Use GNU compiler to pass the test file and retrieve declarations
    string sCLLaunchBat = "#!/bin/bash\n";
    sCLLaunchBat += "exitFail() {\n";
    if (iRunType == 1) {
        sCLLaunchBat += "rm -rf *.i > /dev/null 2>&1\n";
    }
    sCLLaunchBat += "rm -rf " + sTempDirectory + " > /dev/null 2>&1\nexit 1\n}\n";
    sCLLaunchBat += "mkdir \"" + sTempDirectory + "\" > /dev/null 2>&1\n";
    sCLLaunchBat += "mkdir \"" + sTempFolder + "\" > /dev/null 2>&1\n";
    for (map<string, StaticList>::iterator itI = mDirectoryObjects.begin(); itI != mDirectoryObjects.end(); itI++) {
        string sDirName = sTempFolder + "/" + itI->first;
        if (itI->first.length() > 0) {
            //Need to make output directory so compile doesn't fail outputting
            sCLLaunchBat += "mkdir \"" + sDirName + "\" > /dev/null 2>&1\n";
        }
        string sRuntype;
        //Check type of compiler call
        if (iRunType == 0) {
            outputError("Generation of definitions is not supported using gcc.");
            return false;
        } else if (iRunType == 1) {
            sRuntype = "-E -P";
        }
        //Check if gcc or mingw
        if (m_ConfigHelper.m_sToolchain.find("mingw") != string::npos) {
            sCLExtra += "-D\"WIN32\" -D\"_WINDOWS\"";
        }

        //Split calls as gcc outputs a single file at a time
        for (StaticList::iterator vitJ = itI->second.begin(); vitJ < itI->second.end(); vitJ++) {
            sCLLaunchBat += "gcc ";
            sCLLaunchBat += sCLExtra + " -D_DEBUG " + sRuntype + " -c -w";
            if (iRunType == 0) {
                m_ConfigHelper.makeFileGeneratorRelative(*vitJ, *vitJ);
            }
            sCLLaunchBat += " \"" + *vitJ + "\"";
            if (iRunType == 0) {
                sCLLaunchBat += " -o " + *vitJ + ".o";
            } else if (iRunType == 1) {
                sCLLaunchBat += " -o " + *vitJ + ".i";
            }
            sCLLaunchBat += " > ffvs_log.txt 2>&1\nif (( $? )); then\nexitFail\nfi\n";
        }
    }
    if (iRunType == 0) {
        sCLLaunchBat += "rm -rf *.o > /dev/null 2>&1\n";
    }
    sCLLaunchBat += "rm ffvs_log.txt > /dev/null 2>&1\n";
    sCLLaunchBat += "exit 0\n";
    if (!writeToFile("ffvs_compile.sh", sCLLaunchBat)) {
        return false;
    }
    if (0 != system("ffvs_compile.sh")) {
        outputError("Errors detected during compilation :-");
        outputError("Unknown error detected. See ffvs_log.txt for further details.", false);
        //Remove the compilation files
        deleteFile("ffvs_compile.sh");
        deleteFolder(sTempDirectory);
        return false;
    }

    //Remove the compilation objects
    deleteFile("ffvs_compile.sh");
    return true;
}