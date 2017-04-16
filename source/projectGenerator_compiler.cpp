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

#include <iostream>
#include <algorithm>
#include <utility>

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
    string sCLLaunchBat = "@echo off \n";
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

        //Check type of msvc call
        string sDirName = sProjectName + "/" + itI->first;
        //Need to make output directory so cl doesn't fail outputting objs
        sCLLaunchBat += "mkdir \"" + sDirName + "\" > nul 2>&1\n";
        string sRuntype;
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
        cout << "  Error: Errors detected during test compilation :-" << endl;
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
                cout << "    " << sTemp << endl;
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
                cout << "    " << sTestOutput.substr(uiFindPos, uiFindPos2 - uiFindPos) << endl;
                bMissingVS = true;
            }
            if (bMissingVS) {
                cout << endl << "    Based on the above error(s) Visual Studio is not installed correctly on the host system." << endl;
                cout << "    Install a compatible version of Visual Studio before trying again." << endl;
                deleteFile("log.txt");
            } else if (bMissingDeps) {
                cout << endl << "    Based on the above error(s) there are files required for dependency libraries that are not available" << endl;
                cout << "    Ensure that any required dependencies are available in 'OutDir' based on the supplied configuration options before trying again." << endl;
                cout << "    Consult the supplied readme for instructions for installing varying dependencies." << endl;
                cout << "    If a dependency has been cloned from a ShiftMediaProject repository then ensure it has been successfully built before trying again." << endl;
                cout << "    Removing the offending configuration option can also be used to remove the error." << endl;
                deleteFile("log.txt");
            } else if (bError) {
                cout << endl << "    Unknown error detected. See log.txt for further details." << endl;
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