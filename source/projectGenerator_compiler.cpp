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

bool ProjectGenerator::runCompiler(const vector<string> & vIncludeDirs, const string & sProjectName, map<string, vector<string>> &mDirectoryObjects, int iRunType)
{
#ifdef _MSC_VER
    //If compiled by msvc then only msvc builds are supported
    return runMSVC(vIncludeDirs, sProjectName, mDirectoryObjects, iRunType);
#else
    //Otherwise only gcc and mingw are supported
    return runGCC(vIncludeDirs, sProjectName, mDirectoryObjects, iRunType);
#endif
}

bool ProjectGenerator::runMSVC(const vector<string> & vIncludeDirs, const string & sProjectName, map<string, vector<string>> &mDirectoryObjects, int iRunType)
{
    //Create a test file to read in definitions
    string sOutDir = m_ConfigHelper.m_sOutDirectory;
    makeFileGeneratorRelative(sOutDir, sOutDir);
    string sCLExtra = "/I\"" + sOutDir + "include/\"";
    for (StaticList::const_iterator vitIt = vIncludeDirs.cbegin(); vitIt < vIncludeDirs.cend(); vitIt++) {
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
        sCLExtra += " /I\"" + sIncludeDir + '\"';
    }

    //Use Microsoft compiler to pass the test file and retrieve declarations
    string sCLLaunchBat = "@echo off\n";
    sCLLaunchBat += "if exist \"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat\" ( \n\
call \"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat\" >NUL 2>NUL \n\
goto MSVCVarsDone \n\
) else if exist \"%VS140COMNTOOLS%\\vsvars32.bat\" ( \n\
call \"%VS140COMNTOOLS%\\vsvars32.bat\" \n\
goto MSVCVarsDone \n\
) else if exist \"%VS120COMNTOOLS%\\vsvars32.bat\" ( \n\
call \"%VS120COMNTOOLS%\\vsvars32.bat\" \n\
goto MSVCVarsDone \n\
) else if exist \"%VS110COMNTOOLS%\\vsvars32.bat\" ( \n\
call \"%VS110COMNTOOLS%\\vsvars32.bat\" \n\
goto MSVCVarsDone \n\
) else ( \n\
echo fatal error : An installed version of Visual Studio could not be detected. \n\
exit /b 1 \n\
) \n\
:MSVCVarsDone \n";
    sCLLaunchBat += "mkdir \"" + sProjectName + "\" > nul 2>&1\n";
    for (map<string, StaticList>::iterator itI = mDirectoryObjects.begin(); itI != mDirectoryObjects.end(); itI++) {
        const uint uiRowSize = 32;
        uint uiNumCLCalls = (uint)ceilf((float)itI->second.size() / (float)uiRowSize);
        uint uiTotalPos = 0;
        string sDirName = sProjectName + "/" + itI->first;
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
            sCLLaunchBat += "cl.exe";
            sCLLaunchBat += " /I\"" + m_ConfigHelper.m_sRootDirectory + "\" /I\"" + m_ConfigHelper.m_sProjectDirectory + "\" " + sCLExtra + " /D\"_DEBUG\" /D\"WIN32\" /D\"_WINDOWS\" /D\"HAVE_AV_CONFIG_H\" /D\"inline=__inline\" /FI\"compat.h\" " + sRuntype + " /c /MP /w /nologo";
            uint uiStartPos = uiTotalPos;
            for (uiTotalPos; uiTotalPos < min(uiStartPos + uiRowSize, itI->second.size()); uiTotalPos++) {
                if (iRunType == 0) {
                    makeFileGeneratorRelative(itI->second[uiTotalPos], itI->second[uiTotalPos]);
                }
                sCLLaunchBat += " \"" + itI->second[uiTotalPos] + "\"";
            }
            sCLLaunchBat += " > log.txt 2>&1\nif %errorlevel% neq 0 goto exitFail\n";
        }
        if (iRunType == 1) {
            sCLLaunchBat += "move *.i " + sDirName + "/ >nul 2>&1\n";
        }
    }
    if (iRunType == 0) {
        sCLLaunchBat += "del /F /S /Q *.obj >nul 2>&1\n";
    }
    sCLLaunchBat += "del log.txt >nul 2>&1\n";
    sCLLaunchBat += "exit /b 0\n:exitFail\n";
    if (iRunType == 1) {
        sCLLaunchBat += "del /F /S /Q *.i >nul 2>&1\n";
    }
    sCLLaunchBat += "rmdir /S /Q " + sProjectName + "\nexit /b 1";
    if (!writeToFile("test.bat", sCLLaunchBat)) {
        return false;
    }

    if (0 != system("test.bat")) {
        outputError("Errors detected during test compilation :-");
        string sTestOutput;
        if (loadFromFile("log.txt", sTestOutput)) {
            //Output errors from log.txt
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
                deleteFile("log.txt");
            } else if (bMissingDeps) {
                outputError("Based on the above error(s) there are files required for dependency libraries that are not available", false);
                outputError("Ensure that any required dependencies are available in 'OutDir' based on the supplied configuration options before trying again.", false);
                outputError("Consult the supplied readme for instructions for installing varying dependencies.", false);
                outputError("If a dependency has been cloned from a ShiftMediaProject repository then ensure it has been successfully built before trying again.", false);
                outputError("  Removing the offending configuration option can also be used to remove the error.", false);
                deleteFile("log.txt");
            } else if (bError) {
                outputError("Unknown error detected. See log.txt for further details.", false);
            }
        }
        //Remove the test header files
        deleteFile("test.bat");
        deleteFolder(sProjectName);
        return false;
    }

    //Remove the compilation objects
    deleteFile("test.bat");
    return true;
}

bool ProjectGenerator::runGCC(const vector<string> & vIncludeDirs, const string & sProjectName, map<string, vector<string>> &mDirectoryObjects, int iRunType)
{
    //Create a test file to read in definitions
    string sOutDir = m_ConfigHelper.m_sOutDirectory;
    makeFileGeneratorRelative(sOutDir, sOutDir);
    string sCLExtra = "-I\"" + sOutDir + "include/\"";
    for (StaticList::const_iterator vitIt = vIncludeDirs.cbegin(); vitIt < vIncludeDirs.cend(); vitIt++) {
        string sIncludeDir = *vitIt;
        uint uiFindPos2 = sIncludeDir.find("$(OutDir)");
        if (uiFindPos2 != string::npos) {
            sIncludeDir.replace(uiFindPos2, 9, sOutDir);
        }
        uiFindPos2 = sIncludeDir.find("$(");
        if (uiFindPos2 != string::npos) {
            sIncludeDir.replace(uiFindPos2, 2, "$");
        }
        uiFindPos2 = sIncludeDir.find(")");
        if (uiFindPos2 != string::npos) {
            sIncludeDir.erase(uiFindPos2, 1);
        }
        sCLExtra += " -I\"" + sIncludeDir + '\"';
    }

    //Use GNU compiler to pass the test file and retrieve declarations
    string sCLLaunchBat = "#!/bin/bash\n";
    sCLLaunchBat += "exitFail() {\n";
    if (iRunType == 1) {
        sCLLaunchBat += "rm -rf *.i > /dev/null 2>&1\n";
    }
    sCLLaunchBat += "rm -rf " + sProjectName + " > /dev/null 2>&1\nexit 1\n}\n";
    sCLLaunchBat += "mkdir \"" + sProjectName + "\" > /dev/null 2>&1\n";
    for (map<string, StaticList>::iterator itI = mDirectoryObjects.begin(); itI != mDirectoryObjects.end(); itI++) {
        string sDirName = sProjectName + "/" + itI->first;
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
            sCLLaunchBat += "gcc";
            sCLLaunchBat += " -I\"" + m_ConfigHelper.m_sRootDirectory + "\" -I\"" + m_ConfigHelper.m_sProjectDirectory + "\" " + sCLExtra + " -D_DEBUG " + sRuntype + " -c -w";
            if (iRunType == 0) {
                makeFileGeneratorRelative(*vitJ, *vitJ);
            }
            sCLLaunchBat += " \"" + *vitJ + "\"";
            if (iRunType == 0) {
                sCLLaunchBat += " -o " + *vitJ + ".o";
            } else if (iRunType == 1) {
                sCLLaunchBat += " -o " + *vitJ + ".i";
            }
            sCLLaunchBat += " > log.txt 2>&1\nif (( $? )); then\nexitFail\nfi\n";
        }
    }
    if (iRunType == 0) {
        sCLLaunchBat += "rm -rf *.o > /dev/null 2>&1\n";
    }
    sCLLaunchBat += "rm log.txt > /dev/null 2>&1\n";
    sCLLaunchBat += "exit 0\n";
    if (!writeToFile("test.sh", sCLLaunchBat)) {
        return false;
    }
    if (0 != system("test.sh")) {
        outputError("Errors detected during test compilation :-");
        outputError("Unknown error detected. See log.txt for further details.", false);
        //Remove the test header files
        deleteFile("test.sh");
        deleteFolder(sProjectName);
        return false;
    }

    //Remove the compilation objects
    deleteFile("test.sh");
    return true;
}