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

//This can be used to change between outputting projects using either nasm
// or yasm assembler
#define USENASM 0

#define TEMPLATE_COMPAT_ID 100
#define TEMPLATE_MATH_ID 101
#define TEMPLATE_UNISTD_ID 102
#define TEMPLATE_SLN_ID 103
#define TEMPLATE_VCXPROJ_ID 104
#define TEMPLATE_FILTERS_ID 105
#define TEMPLATE_PROG_VCXPROJ_ID 106
#define TEMPLATE_PROG_FILTERS_ID 107
#define TEMPLATE_STDATOMIC_ID 108

bool ProjectGenerator::passAllMake()
{
    if ((m_ConfigHelper.m_sToolchain.compare("msvc") == 0) || (m_ConfigHelper.m_sToolchain.compare("icl") == 0)) {
        //Copy the required header files to output directory
        bool bCopy = copyResourceFile(TEMPLATE_COMPAT_ID, m_ConfigHelper.m_sProjectDirectory + "compat.h", true);
        if (!bCopy) {
            outputError("Failed writing to output location. Make sure you have the appropriate user permissions.");
            return false;
        }
        copyResourceFile(TEMPLATE_MATH_ID, m_ConfigHelper.m_sProjectDirectory + "math.h", true);
        copyResourceFile(TEMPLATE_UNISTD_ID, m_ConfigHelper.m_sProjectDirectory + "unistd.h", true);
        string sFileName;
        if (findFile(m_ConfigHelper.m_sRootDirectory + "compat/atomics/win32/stdatomic.h", sFileName)) {
            copyResourceFile(TEMPLATE_STDATOMIC_ID, m_ConfigHelper.m_sProjectDirectory + "stdatomic.h", true);
        }
    }

    //Loop through each library make file
    vector<string> vLibraries;
    m_ConfigHelper.getConfigList("LIBRARY_LIST", vLibraries);
    for (vector<string>::iterator vitLib = vLibraries.begin(); vitLib < vLibraries.end(); vitLib++) {
        //Check if library is enabled
        if (m_ConfigHelper.getConfigOption(*vitLib)->m_sValue.compare("1") == 0) {
            m_sProjectDir = m_ConfigHelper.m_sRootDirectory + "lib" + *vitLib + "/";
            //Locate the project dir for specified library
            string sRetFileName;
            if (!findFile(m_sProjectDir + "MakeFile", sRetFileName)) {
                outputError("Could not locate directory for library (" + *vitLib + ")");
                return false;
            }
            //Run passMake on default Makefile
            if (!passMake()) {
                return false;
            }
            //Check for any sub directories
            m_sProjectDir += "x86/";
            if (findFile(m_sProjectDir + "MakeFile", sRetFileName)) {
                //Pass the sub directory
                if (!passMake()) {
                    return false;
                }
            }
            //Reset project dir so it does not include additions
            m_sProjectDir.resize(m_sProjectDir.length() - 4);
            //Output the project
            if (!outputProject()) {
                return false;
            }
            //Reset all internal values
            m_sInLine.clear();
            m_vIncludes.clear();
            m_mReplaceIncludes.clear();
            m_vCPPIncludes.clear();
            m_vCIncludes.clear();
            m_vASMIncludes.clear();
            m_vHIncludes.clear();
            m_vLibs.clear();
            m_mUnknowns.clear();
            m_sProjectDir.clear();
        }
    }

    //Output the solution file
    return outputSolution();

    if (m_ConfigHelper.m_bDCEOnly) {
        //Delete no longer needed compilation files
        deleteCreatedFiles();
    }
}

void ProjectGenerator::deleteCreatedFiles()
{
    //Get list of libraries and programs
    vector<string> vLibraries;
    m_ConfigHelper.getConfigList("LIBRARY_LIST", vLibraries);
    vector<string> vPrograms;
    m_ConfigHelper.getConfigList("PROGRAM_LIST", vPrograms);

    //Delete any previously generated files
    vector<string> vExistingFiles;
    findFiles(m_ConfigHelper.m_sProjectDirectory + "ffmpeg.sln", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "libav.sln", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "compat.h", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "math.h", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "unistd.h", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "stdatomic.h", vExistingFiles, false);
    for (vector<string>::iterator vitLib = vLibraries.begin(); vitLib < vLibraries.end(); vitLib++) {
        *vitLib = "lib" + *vitLib;
        findFiles(m_ConfigHelper.m_sProjectDirectory + *vitLib + ".vcxproj", vExistingFiles, false);
        findFiles(m_ConfigHelper.m_sProjectDirectory + *vitLib + ".vcxproj.filters", vExistingFiles, false);
        findFiles(m_ConfigHelper.m_sProjectDirectory + *vitLib + ".def", vExistingFiles, false);
    }
    for (vector<string>::iterator vitProg = vPrograms.begin(); vitProg < vPrograms.end(); vitProg++) {
        findFiles(m_ConfigHelper.m_sProjectDirectory + *vitProg + ".vcxproj", vExistingFiles, false);
        findFiles(m_ConfigHelper.m_sProjectDirectory + *vitProg + ".vcxproj.filters", vExistingFiles, false);
        findFiles(m_ConfigHelper.m_sProjectDirectory + *vitProg + ".def", vExistingFiles, false);
    }
    for (vector<string>::iterator itIt = vExistingFiles.begin(); itIt < vExistingFiles.end(); itIt++) {
        deleteFile(*itIt);
    }

    //Check for any created folders
    vector<string> vExistingFolders;
    for (vector<string>::iterator vitLib = vLibraries.begin(); vitLib < vLibraries.end(); vitLib++) {
        findFolders(m_ConfigHelper.m_sProjectDirectory + *vitLib, vExistingFolders, false);
    }
    for (vector<string>::iterator vitProg = vPrograms.begin(); vitProg < vPrograms.end(); vitProg++) {
        findFolders(m_ConfigHelper.m_sProjectDirectory + *vitProg, vExistingFolders, false);
    }
    for (vector<string>::iterator itIt = vExistingFolders.begin(); itIt < vExistingFolders.end(); itIt++) {
        //Search for any generated files in the directories
        vExistingFiles.resize(0);
        findFiles(*itIt + "/dce_defs.c", vExistingFiles, false);
        findFiles(*itIt + "/*_wrap.c", vExistingFiles, false);
        if (!m_ConfigHelper.m_bUsingExistingConfig) {
            findFiles(*itIt + "/*_list.c", vExistingFiles, false);
        }
        for (vector<string>::iterator itIt2 = vExistingFiles.begin(); itIt2 < vExistingFiles.end(); itIt2++) {
            deleteFile(*itIt2);
        }
        //Check if the directory is now empty and delete if it is
        if (isFolderEmpty(*itIt)) {
            deleteFolder(*itIt);
        }
    }
}

void ProjectGenerator::errorFunc(bool bCleanupFiles)
{
    if (bCleanupFiles) {
        //Cleanup any partially created files
        m_ConfigHelper.deleteCreatedFiles();
        deleteCreatedFiles();

        //Delete any temporary file leftovers
        deleteFolder(sTempDirectory);
    }

    pressKeyToContinue();
    exit(1);
}

bool ProjectGenerator::outputProject()
{
    //Output the generated files
    uint uiSPos = m_sProjectDir.rfind('/', m_sProjectDir.length() - 2) + 1;
    m_sProjectName = m_sProjectDir.substr(uiSPos, m_sProjectDir.length() - 1 - uiSPos);

    //Check all files are correctly located
    if (!checkProjectFiles()) {
        return false;
    }

    //Get dependency directories
    StaticList vIncludeDirs;
    StaticList vLib32Dirs;
    StaticList vLib64Dirs;
    StaticList vDefines;
    buildDependencyValues(vIncludeDirs, vLib32Dirs, vLib64Dirs, vDefines);

    //Create missing definitions of functions removed by DCE
    if (!outputProjectDCE(vIncludeDirs)) {
        return false;
    }

    if (m_ConfigHelper.m_bDCEOnly) {
        //Exit here to prevent outputting project files
        return true;
    }

    //Generate the exports file
    if (!outputProjectExports(vIncludeDirs)) {
        return false;
    }

    //We now have complete list of all the files that we need
    outputLine("  Generating project file (" + m_sProjectName + ")...");

    //Open the input temp project file
    string sProjectFile;
    if (!loadFromResourceFile(TEMPLATE_VCXPROJ_ID, sProjectFile)) {
        return false;
    }

    //Open the input temp project file filters
    string sFiltersFile;
    if (!loadFromResourceFile(TEMPLATE_FILTERS_ID, sFiltersFile)) {
        return false;
    }

    //Add all project source files
    outputSourceFiles(sProjectFile, sFiltersFile);

    //Add the build events
    outputBuildEvents(sProjectFile);

    //Add ASM requirements
    outputASMTools(sProjectFile);

    //Add the dependency libraries
    if (!outputDependencyLibs(sProjectFile)) {
        return false;
    }

    //Add additional includes to include list
    outputIncludeDirs(vIncludeDirs, sProjectFile);

    //Add additional lib includes to include list
    outputLibDirs(vLib32Dirs, vLib64Dirs, sProjectFile);

    //Add additional defines
    outputDefines(vDefines, sProjectFile);

    //Replace all template tag arguments
    outputTemplateTags(sProjectFile, sFiltersFile);

    //Write output project
    string sOutProjectFile = m_ConfigHelper.m_sProjectDirectory + m_sProjectName + ".vcxproj";
    if (!writeToFile(sOutProjectFile, sProjectFile, true)) {
        return false;
    }

    //Write output filters
    string sOutFiltersFile = m_ConfigHelper.m_sProjectDirectory + m_sProjectName + ".vcxproj.filters";
    if (!writeToFile(sOutFiltersFile, sFiltersFile, true)) {
        return false;
    }

    return true;
}

bool ProjectGenerator::outputProgramProject(const string& sDestinationFile, const string& sDestinationFilterFile)
{
    //Pass makefile for program
    if (!passProgramMake()) {
        return false;
    }

    //Check all files are correctly located
    if (!checkProjectFiles()) {
        return false;
    }

    //Get dependency directories
    StaticList vIncludeDirs;
    StaticList vLib32Dirs;
    StaticList vLib64Dirs;
    StaticList vDefines;
    buildDependencyValues(vIncludeDirs, vLib32Dirs, vLib64Dirs, vDefines);

    //Create missing definitions of functions removed by DCE
    if (!outputProjectDCE(vIncludeDirs)) {
        return false;
    }

    if (m_ConfigHelper.m_bDCEOnly) {
        //Exit here to prevent outputting project files
        return true;
    }

    //We now have complete list of all the files that we need
    outputLine("  Generating project file (" + m_sProjectName + ")...");

    //Open the template program
    string sProgramFile;
    if (!loadFromResourceFile(TEMPLATE_PROG_VCXPROJ_ID, sProgramFile)) {
        return false;
    }

    //Open the template program filters
    string sProgramFiltersFile;
    if (!loadFromResourceFile(TEMPLATE_PROG_FILTERS_ID, sProgramFiltersFile)) {
        return false;
    }

    //Add all project source files
    outputSourceFiles(sProgramFile, sProgramFiltersFile);

    //Add the build events
    outputBuildEvents(sProgramFile);

    //Add ASM requirements
    outputASMTools(sProgramFile);

    //Add the dependency libraries
    if (!outputDependencyLibs(sProgramFile, true)) {
        return false;
    }

    //Add additional includes to include list
    outputIncludeDirs(vIncludeDirs, sProgramFile);

    //Add additional lib includes to include list
    outputLibDirs(vLib32Dirs, vLib64Dirs, sProgramFile);

    //Add additional defines
    outputDefines(vDefines, sProgramFile);

    //Replace all template tag arguments
    outputTemplateTags(sProgramFile, sProgramFiltersFile);

    //Write program file
    if (!writeToFile(sDestinationFile, sProgramFile, true)) {
        return false;
    }

    //Write output filters
    if (!writeToFile(sDestinationFilterFile, sProgramFiltersFile, true)) {
        return false;
    }

    //Reset all internal values
    m_sInLine.clear();
    m_vIncludes.clear();
    m_vCPPIncludes.clear();
    m_vCIncludes.clear();
    m_vASMIncludes.clear();
    m_vHIncludes.clear();
    m_vLibs.clear();
    m_mUnknowns.clear();

    return true;
}

bool ProjectGenerator::outputSolution()
{
    //Create program list
    map<string, string> mProgramList;
    if (!m_ConfigHelper.m_bLibav) {
        mProgramList["ffmpeg"] = "CONFIG_FFMPEG";
        mProgramList["ffplay"] = "CONFIG_FFPLAY";
        mProgramList["ffprobe"] = "CONFIG_FFPROBE";
    } else {
        mProgramList["avconv"] = "CONFIG_AVCONV";
        mProgramList["avplay"] = "CONFIG_AVPLAY";
        mProgramList["avprobe"] = "CONFIG_AVPROBE";
    }

    //Next add the projects
    map<string, string>::iterator mitPrograms = mProgramList.begin();
    while (mitPrograms != mProgramList.end()) {
        //Check if program is enabled
        if (m_ConfigHelper.getConfigOptionPrefixed(mitPrograms->second)->m_sValue.compare("1") == 0) {
            m_sProjectDir = m_ConfigHelper.m_sRootDirectory;
            //Create project files for program
            m_sProjectName = mitPrograms->first;
            const string sDestinationFile = m_ConfigHelper.m_sProjectDirectory + mitPrograms->first + ".vcxproj";
            const string sDestinationFilterFile = m_ConfigHelper.m_sProjectDirectory + mitPrograms->first + ".vcxproj.filters";
            if (!outputProgramProject(sDestinationFile, sDestinationFilterFile)) {
                return false;
            }
        }
        //next
        ++mitPrograms;
    }

    if (m_ConfigHelper.m_bDCEOnly) {
        //Don't output solution and just exit early
        return true;
    }

    outputLine("  Generating solution file...");
    //Open the input temp project file
    string sSolutionFile;
    if (!loadFromResourceFile(TEMPLATE_SLN_ID, sSolutionFile)) {
        return false;
    }

    map<string, string> mKeys;
    buildProjectGUIDs(mKeys);
    string sSolutionKey = "8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942";

    vector<string> vAddedKeys;

    const string sProject = "\r\nProject(\"{";
    const string sProject2 = "}\") = \"";
    const string sProject3 = "\", \"";
    const string sProject4 = ".vcxproj\", \"{";
    const string sProjectEnd = "}\"";
    const string sProjectClose = "\r\nEndProject";

    const string sDepend = "\r\n	ProjectSection(ProjectDependencies) = postProject";
    const string sDependClose = "\r\n	EndProjectSection";
    const string sSubDepend = "\r\n		{";
    const string sSubDepend2 = "} = {";
    const string sSubDependEnd = "}";

    //Find the start of the file
    const string sFileStart = "Project";
    uint uiPos = sSolutionFile.find(sFileStart) - 2;

    map<string, StaticList>::iterator mitLibraries = m_mProjectLibs.begin();
    while (mitLibraries != m_mProjectLibs.end()) {
        //Check if this is a library or a program
        if (mProgramList.find(mitLibraries->first) == mProgramList.end()) {
            //Check if this library has a known key (to lazy to auto generate at this time)
            if (mKeys.find(mitLibraries->first) == mKeys.end()) {
                outputError("Unknown library. Could not determine solution key (" + mitLibraries->first + ")");
                return false;
            }
            //Add the library to the solution
            string sProjectAdd = sProject;
            sProjectAdd += sSolutionKey;
            sProjectAdd += sProject2;
            sProjectAdd += mitLibraries->first;
            sProjectAdd += sProject3;
            sProjectAdd += mitLibraries->first;
            sProjectAdd += sProject4;
            sProjectAdd += mKeys[mitLibraries->first];
            sProjectAdd += sProjectEnd;

            //Add the key to the used key list
            vAddedKeys.push_back(mKeys[mitLibraries->first]);

            //Add the dependencies
            if (mitLibraries->second.size() > 0) {
                sProjectAdd += sDepend;
                for (StaticList::iterator vitIt = mitLibraries->second.begin(); vitIt < mitLibraries->second.end(); vitIt++) {
                    //Check if this library has a known key
                    if (mKeys.find(*vitIt) == mKeys.end()) {
                        outputError("Unknown library dependency. Could not determine solution key (" + *vitIt + ")");
                        return false;
                    }
                    sProjectAdd += sSubDepend;
                    sProjectAdd += mKeys[*vitIt];
                    sProjectAdd += sSubDepend2;
                    sProjectAdd += mKeys[*vitIt];
                    sProjectAdd += sSubDependEnd;
                }
                sProjectAdd += sDependClose;
            }
            sProjectAdd += sProjectClose;

            //Insert into solution string
            sSolutionFile.insert(uiPos, sProjectAdd);
            uiPos += sProjectAdd.length();
        }

        //next
        ++mitLibraries;
    }

    //Next add the projects
    string sProjectAdd;
    vector<string> vAddedPrograms;
    mitPrograms = mProgramList.begin();
    while (mitPrograms != mProgramList.end()) {
        //Check if program is enabled
        if (m_ConfigHelper.getConfigOptionPrefixed(mitPrograms->second)->m_sValue.compare("1") == 0) {
            //Add the program to the solution
            sProjectAdd += sProject;
            sProjectAdd += sSolutionKey;
            sProjectAdd += sProject2;
            sProjectAdd += mitPrograms->first;
            sProjectAdd += sProject3;
            sProjectAdd += mitPrograms->first;
            sProjectAdd += sProject4;
            sProjectAdd += mKeys[mitPrograms->first];
            sProjectAdd += sProjectEnd;

            //Add the key to the used key list
            vAddedPrograms.push_back(mKeys[mitPrograms->first]);

            //Add the dependencies
            sProjectAdd += sDepend;
            StaticList::iterator mitLibs = m_mProjectLibs[mitPrograms->first].begin();
            while (mitLibs != m_mProjectLibs[mitPrograms->first].end()) {
                //Add all project libraries as dependencies
                if (!m_ConfigHelper.m_bLibav) {
                    sProjectAdd += sSubDepend;
                    sProjectAdd += mKeys[*mitLibs];
                    sProjectAdd += sSubDepend2;
                    sProjectAdd += mKeys[*mitLibs];
                    sProjectAdd += sSubDependEnd;
                }
                //next
                ++mitLibs;
            }
            sProjectAdd += sDependClose;
            sProjectAdd += sProjectClose;
        }
        //next
        ++mitPrograms;
    }

    //Check if there were actually any programs added
    string sProgramKey = "8A736DDA-6840-4E65-9DA4-BF65A2A70428";
    if (sProjectAdd.length() > 0) {
        //Add program key
        sProjectAdd += "\r\nProject(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"Programs\", \"Programs\", \"{";
        sProjectAdd += sProgramKey;
        sProjectAdd += "}\"";
        sProjectAdd += "\r\nEndProject";

        //Insert into solution string
        sSolutionFile.insert(uiPos, sProjectAdd);
        uiPos += sProjectAdd.length();
    }

    //Next Add the solution configurations
    string sConfigStart = "GlobalSection(ProjectConfigurationPlatforms) = postSolution";
    uiPos = sSolutionFile.find(sConfigStart) + sConfigStart.length();
    string sConfigPlatform = "\r\n		{";
    string sConfigPlatform2 = "}.";
    string sConfigPlatform3 = "|";
    string aBuildConfigs[7] = {"Debug", "DebugDLL", "DebugDLLStaticDeps", "Release", "ReleaseDLL", "ReleaseDLLStaticDeps", "ReleaseLTO"};
    string aBuildArchsSol[2] = {"x86", "x64"};
    string aBuildArchs[2] = {"Win32", "x64"};
    string aBuildTypes[2] = {".ActiveCfg = ", ".Build.0 = "};
    string sAddPlatform;
    //Add the lib keys
    for (vector<string>::iterator vitIt = vAddedKeys.begin(); vitIt < vAddedKeys.end(); vitIt++) {
        //loop over build configs
        for (uint uiI = 0; uiI < 7; uiI++) {
            //loop over build archs
            for (uint uiJ = 0; uiJ < 2; uiJ++) {
                //loop over build types
                for (uint uiK = 0; uiK < 2; uiK++) {
                    sAddPlatform += sConfigPlatform;
                    sAddPlatform += *vitIt;
                    sAddPlatform += sConfigPlatform2;
                    sAddPlatform += aBuildConfigs[uiI];
                    sAddPlatform += sConfigPlatform3;
                    sAddPlatform += aBuildArchsSol[uiJ];
                    sAddPlatform += aBuildTypes[uiK];
                    sAddPlatform += aBuildConfigs[uiI];
                    sAddPlatform += sConfigPlatform3;
                    sAddPlatform += aBuildArchs[uiJ];
                }
            }
        }
    }
    //Add the program keys
    for (vector<string>::iterator vitIt = vAddedPrograms.begin(); vitIt < vAddedPrograms.end(); vitIt++) {
        //loop over build configs
        for (uint uiI = 0; uiI < 7; uiI++) {
            //loop over build archs
            for (uint uiJ = 0; uiJ < 2; uiJ++) {
                //loop over build types
                for (uint uiK = 0; uiK < 2; uiK++) {
                    if ((uiK == 1) && (uiI != 3)) {
                        //We dont build programs by default except for Release config
                        continue;
                    }
                    sAddPlatform += sConfigPlatform;
                    sAddPlatform += *vitIt;
                    sAddPlatform += sConfigPlatform2;
                    sAddPlatform += aBuildConfigs[uiI];
                    sAddPlatform += sConfigPlatform3;
                    sAddPlatform += aBuildArchsSol[uiJ];
                    sAddPlatform += aBuildTypes[uiK];
                    if (uiI == 2) {
                        sAddPlatform += aBuildConfigs[1];
                    } else if (uiI == 5) {
                        sAddPlatform += aBuildConfigs[4];
                    } else if (uiI == 6) {
                        //there is no program lto build so use release instead
                        sAddPlatform += aBuildConfigs[3];
                    } else {
                        sAddPlatform += aBuildConfigs[uiI];
                    }
                    sAddPlatform += sConfigPlatform3;
                    sAddPlatform += aBuildArchs[uiJ];
                }
            }
        }
    }
    //Insert into solution string
    sSolutionFile.insert(uiPos, sAddPlatform);
    uiPos += sAddPlatform.length();

    //Add any programs to the nested projects
    if (vAddedPrograms.size() > 0) {
        string sNestedStart = "GlobalSection(NestedProjects) = preSolution";
        uint uiPos = sSolutionFile.find(sNestedStart) + sNestedStart.length();
        string sNest = "\r\n		{";
        string sNest2 = "} = {";
        string sNestEnd = "}";
        string sNestProg;
        for (vector<string>::iterator vitIt = vAddedPrograms.begin(); vitIt < vAddedPrograms.end(); vitIt++) {
            sNestProg += sNest;
            sNestProg += *vitIt;
            sNestProg += sNest2;
            sNestProg += sProgramKey;
            sNestProg += sNestEnd;
        }
        //Insert into solution string
        sSolutionFile.insert(uiPos, sNestProg);
        uiPos += sNestProg.length();
    }

    //Write output solution
    string sProjectName = m_ConfigHelper.m_sProjectName;
    transform(sProjectName.begin(), sProjectName.end(), sProjectName.begin(), ::tolower);
    const string sOutSolutionFile = m_ConfigHelper.m_sProjectDirectory + sProjectName + ".sln";
    if (!writeToFile(sOutSolutionFile, sSolutionFile, true)) {
        return false;
    }

    return true;
}

void ProjectGenerator::outputTemplateTags(string & sProjectTemplate, string& sFiltersTemplate)
{
    //Change all occurrences of template_in with project name
    const string sFFSearchTag = "template_in";
    uint uiFindPos = sProjectTemplate.find(sFFSearchTag);
    while (uiFindPos != string::npos) {
        //Replace
        sProjectTemplate.replace(uiFindPos, sFFSearchTag.length(), m_sProjectName);
        //Get next
        uiFindPos = sProjectTemplate.find(sFFSearchTag, uiFindPos + 1);
    }
    uint uiFindPosFilt = sFiltersTemplate.find(sFFSearchTag);
    while (uiFindPosFilt != string::npos) {
        //Replace
        sFiltersTemplate.replace(uiFindPosFilt, sFFSearchTag.length(), m_sProjectName);
        //Get next
        uiFindPosFilt = sFiltersTemplate.find(sFFSearchTag, uiFindPosFilt + 1);
    }

    //Change all occurrences of template_shin with short project name
    const string sFFShortSearchTag = "template_shin";
    uiFindPos = sProjectTemplate.find(sFFShortSearchTag);
    string sProjectNameShort = m_sProjectName.substr(3); //The full name minus the lib prefix
    while (uiFindPos != string::npos) {
        //Replace
        sProjectTemplate.replace(uiFindPos, sFFShortSearchTag.length(), sProjectNameShort);
        //Get next
        uiFindPos = sProjectTemplate.find(sFFShortSearchTag, uiFindPos + 1);
    }
    uiFindPosFilt = sFiltersTemplate.find(sFFShortSearchTag);
    while (uiFindPosFilt != string::npos) {
        //Replace
        sFiltersTemplate.replace(uiFindPosFilt, sFFShortSearchTag.length(), sProjectNameShort);
        //Get next
        uiFindPosFilt = sFiltersTemplate.find(sFFShortSearchTag, uiFindPosFilt + 1);
    }

    //Change all occurrences of template_platform with specified project toolchain
    string sToolchain = "<PlatformToolset Condition=\"'$(VisualStudioVersion)'=='12.0'\">v120</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(VisualStudioVersion)'=='14.0'\">v140</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(VisualStudioVersion)'=='15.0'\">v141</PlatformToolset>";
    if (m_ConfigHelper.m_sToolchain.compare("msvc") != 0) {
        sToolchain += "\r\n    <PlatformToolset Condition=\"'$(ICPP_COMPILER13)'!=''\">Intel C++ Compiler XE 13.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER14)'!=''\">Intel C++ Compiler XE 14.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER15)'!=''\">Intel C++ Compiler XE 15.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER16)'!=''\">Intel C++ Compiler 16.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER17)'!=''\">Intel C++ Compiler 17.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER18)'!=''\">Intel C++ Compiler 18.0</PlatformToolset>";
    }

    const string sPlatformSearch = "<PlatformToolset>template_platform</PlatformToolset>";
    uiFindPos = sProjectTemplate.find(sPlatformSearch);
    while (uiFindPos != string::npos) {
        //Replace
        sProjectTemplate.replace(uiFindPos, sPlatformSearch.length(), sToolchain);
        //Get next
        uiFindPos = sProjectTemplate.find(sPlatformSearch, uiFindPos + sPlatformSearch.length());
    }

    //Set the project key
    string sGUID = "<ProjectGuid>{";
    uiFindPos = sProjectTemplate.find(sGUID);
    if (uiFindPos != string::npos) {
        map<string, string> mKeys;
        buildProjectGUIDs(mKeys);
        uiFindPos += sGUID.length();
        sProjectTemplate.replace(uiFindPos, mKeys[m_sProjectName].length(), mKeys[m_sProjectName]);
    }

    //Change all occurrences of template_outdir with configured output directory
    string sOutDir = m_ConfigHelper.m_sOutDirectory;
    replace(sOutDir.begin(), sOutDir.end(), '/', '\\');
    if (sOutDir.at(0) == '.') {
        sOutDir = "$(ProjectDir)" + sOutDir; //Make any relative paths based on project dir
    }
    const string sFFOutSearchTag = "template_outdir";
    uiFindPos = sProjectTemplate.find(sFFOutSearchTag);
    while (uiFindPos != string::npos) {
        //Replace
        sProjectTemplate.replace(uiFindPos, sFFOutSearchTag.length(), sOutDir);
        //Get next
        uiFindPos = sProjectTemplate.find(sFFOutSearchTag, uiFindPos + 1);
    }

    //Change all occurrences of template_rootdir with configured output directory
    string sRootDir = m_ConfigHelper.m_sRootDirectory;
    m_ConfigHelper.makeFileProjectRelative(sRootDir, sRootDir);
    replace(sRootDir.begin(), sRootDir.end(), '/', '\\');
    const string sFFRootSearchTag = "template_rootdir";
    uiFindPos = sProjectTemplate.find(sFFRootSearchTag);
    while (uiFindPos != string::npos) {
        //Replace
        sProjectTemplate.replace(uiFindPos, sFFRootSearchTag.length(), sRootDir);
        //Get next
        uiFindPos = sProjectTemplate.find(sFFRootSearchTag, uiFindPos + 1);
    }
}

void ProjectGenerator::outputSourceFileType(StaticList& vFileList, const string& sType, const string& sFilterType, string & sProjectTemplate, string & sFilterTemplate, StaticList& vFoundObjects, set<string>& vFoundFilters, bool bCheckExisting, bool bStaticOnly, bool bSharedOnly)
{
    //Declare constant strings used in output files
    const string sItemGroup = "\r\n  <ItemGroup>";
    const string sItemGroupEnd = "\r\n  </ItemGroup>";
    const string sIncludeClose = "\">";
    const string sIncludeEnd = "\" />";
    const string sTypeInclude = "\r\n    <" + sType + " Include=\"";
    const string sTypeIncludeEnd = "\r\n    </" + sType + ">";
    const string sIncludeObject = "\r\n      <ObjectFileName>$(IntDir)\\";
    const string sIncludeObjectClose = ".obj</ObjectFileName>";
    const string sFilterSource = "\r\n      <Filter>" + sFilterType + " Files";
    const string sSource = sFilterType + " Files";
    const string sFilterEnd = "</Filter>";
    const string sExcludeConfig = "\r\n      <ExcludedFromBuild Condition=\"'$(Configuration)'=='";
    const string aBuildConfigsStatic[3] = {"Release", "ReleaseLTO", "Debug"};
    const string aBuildConfigsShared[4] = {"ReleaseDLL", "ReleaseDLLStaticDeps", "DebugDLL", "DebugDLLStaticDeps"};
    const string sExcludeConfigEnd = "'\">true</ExcludedFromBuild>";

    if (vFileList.size() > 0) {
        string sTypeFiles = sItemGroup;
        string sTypeFilesFilt = sItemGroup;
        string sTypeFilesTemp, sTypeFilesFiltTemp;
        vector<pair<string, string>> vTempObjects;

        for (StaticList::iterator vitInclude = vFileList.begin(); vitInclude < vFileList.end(); vitInclude++) {
            //Output objects
            sTypeFilesTemp = sTypeInclude;
            sTypeFilesFiltTemp = sTypeInclude;

            //Add the fileName
            string sFile = *vitInclude;
            replace(sFile.begin(), sFile.end(), '/', '\\');
            sTypeFilesTemp += sFile;
            sTypeFilesFiltTemp += sFile;

            //Get object name without path or extension
            uint uiPos = vitInclude->rfind('/') + 1;
            string sObjectName = vitInclude->substr(uiPos);
            uint uiPos2 = sObjectName.rfind('.');
            sObjectName.resize(uiPos2);

            //Add the filters Filter
            string sSourceDir;
            m_ConfigHelper.makeFileProjectRelative(this->m_ConfigHelper.m_sRootDirectory, sSourceDir);
            uiPos = vitInclude->rfind(sSourceDir);
            uiPos = (uiPos == string::npos) ? 0 : uiPos + sSourceDir.length();
            sTypeFilesFiltTemp += sIncludeClose;
            sTypeFilesFiltTemp += sFilterSource;
            uint uiFolderLength = vitInclude->rfind('/') - uiPos;
            if ((int)uiFolderLength != -1) {
                string sFolderName = sFile.substr(uiPos, uiFolderLength);
                sFolderName = "\\" + sFolderName;
                vFoundFilters.insert(sSource + sFolderName);
                sTypeFilesFiltTemp += sFolderName;
            }
            sTypeFilesFiltTemp += sFilterEnd;
            sTypeFilesFiltTemp += sTypeIncludeEnd;

            //Check if this file should be disabled under certain configurations
            bool bClosed = false;
            if (bStaticOnly || bSharedOnly) {
                sTypeFilesTemp += sIncludeClose;
                bClosed = true;
                const string* p_Configs = NULL;
                uint uiConfigs = 0;
                if (bStaticOnly) {
                    p_Configs = aBuildConfigsShared;
                    uiConfigs = 4;
                } else {
                    p_Configs = aBuildConfigsStatic;
                    uiConfigs = 3;
                }
                for (uint uiI = 0; uiI < uiConfigs; uiI++) {
                    sTypeFilesTemp += sExcludeConfig;
                    sTypeFilesTemp += p_Configs[uiI];
                    sTypeFilesTemp += sExcludeConfigEnd;
                }
            }

            //Several input source files have the same name so we need to explicitly specify an output object file otherwise they will clash
            if (bCheckExisting && (find(vFoundObjects.begin(), vFoundObjects.end(), sObjectName) != vFoundObjects.end())) {
                sObjectName = vitInclude->substr(uiPos);
                replace(sObjectName.begin(), sObjectName.end(), '/', '_');
                //Replace the extension with obj
                uiPos2 = sObjectName.rfind('.');
                sObjectName.resize(uiPos2);
                if (!bClosed) {
                    sTypeFilesTemp += sIncludeClose;
                }
                sTypeFilesTemp += sIncludeObject;
                sTypeFilesTemp += sObjectName;
                sTypeFilesTemp += sIncludeObjectClose;
                sTypeFilesTemp += sTypeIncludeEnd;
                //Add to temp list of stored objects
                vTempObjects.push_back(pair<string, string>(sTypeFilesTemp, sTypeFilesFiltTemp));
            } else {
                vFoundObjects.push_back(sObjectName);
                //Close the current item
                if (!bClosed) {
                    sTypeFilesTemp += sIncludeEnd;
                } else {
                    sTypeFilesTemp += sTypeIncludeEnd;
                }
                //Add to output
                sTypeFiles += sTypeFilesTemp;
                sTypeFilesFilt += sTypeFilesFiltTemp;
            }
        }

        //Add any temporary stored objects (This improves compile performance by grouping objects with different compile options - in this case output name)
        for (vector<pair<string, string>>::iterator vitObject = vTempObjects.begin(); vitObject < vTempObjects.end(); vitObject++) {
            //Add to output
            sTypeFiles += vitObject->first;
            sTypeFilesFilt += vitObject->second;
        }

        sTypeFiles += sItemGroupEnd;
        sTypeFilesFilt += sItemGroupEnd;

        //After </ItemGroup> add the item groups for each of the include types
        string sEndTag = "</ItemGroup>"; //Uses independent string to sItemGroupEnd to avoid line ending errors due to \r\n
        uint uiFindPos = sProjectTemplate.rfind(sEndTag);
        uiFindPos += sEndTag.length();
        uint uiFindPosFilt = sFilterTemplate.rfind(sEndTag);
        uiFindPosFilt += sEndTag.length();

        //Insert into output file
        sProjectTemplate.insert(uiFindPos, sTypeFiles);
        sFilterTemplate.insert(uiFindPosFilt, sTypeFilesFilt);
    }
}

void ProjectGenerator::outputSourceFiles(string & sProjectTemplate, string & sFilterTemplate)
{
    set<string> vFoundFilters;
    StaticList vFoundObjects;

    //Check if there is a resource file
    string sResourceFile;
    if (findSourceFile(m_sProjectName.substr(3) + "res", ".rc", sResourceFile)) {
        m_ConfigHelper.makeFileProjectRelative(sResourceFile, sResourceFile);
        StaticList vResources;
        vResources.push_back(sResourceFile);
        outputSourceFileType(vResources, "ResourceCompile", "Resource", sProjectTemplate, sFilterTemplate, vFoundObjects, vFoundFilters, false, false, true);
    }

    //Output ASM files in specific item group (must go first as asm does not allow for custom obj filename)
    if (m_ConfigHelper.isASMEnabled()) {
        outputSourceFileType(m_vASMIncludes, (USENASM) ? "NASM" : "YASM", "Source", sProjectTemplate, sFilterTemplate, vFoundObjects, vFoundFilters, false);
    }

    //Output C files
    outputSourceFileType(m_vCIncludes, "ClCompile", "Source", sProjectTemplate, sFilterTemplate, vFoundObjects, vFoundFilters, true);

    //Output C++ files
    outputSourceFileType(m_vCPPIncludes, "ClCompile", "Source", sProjectTemplate, sFilterTemplate, vFoundObjects, vFoundFilters, true);

    //Output header files in new item group
    outputSourceFileType(m_vHIncludes, "ClInclude", "Header", sProjectTemplate, sFilterTemplate, vFoundObjects, vFoundFilters, false);

    //Add any additional Filters to filters file
    const string sItemGroupEnd = "\r\n  </ItemGroup>";
    const string sFilterAdd = "\r\n    <Filter Include=\"";
    const string sFilterAdd2 = "\">\r\n\
      <UniqueIdentifier>{";
    const string sFilterAddClose = "}</UniqueIdentifier>\r\n\
    </Filter>";
    const string asFilterKeys[] = {"cac6df1e-4a60-495c-8daa-5707dc1216ff", "9fee14b2-1b77-463a-bd6b-60efdcf8850f",
        "bf017c32-250d-47da-b7e6-d5a5091cb1e6", "fd9e10e9-18f6-437d-b5d7-17290540c8b8", "f026e68e-ff14-4bf4-8758-6384ac7bcfaf",
        "a2d068fe-f5d5-4b6f-95d4-f15631533341", "8a4a673d-2aba-4d8d-a18e-dab035e5c446", "0dcfb38d-54ca-4ceb-b383-4662f006eca9",
        "57bf1423-fb68-441f-b5c1-f41e6ae5fa9c"};

    //get start position in file
    uint uiFindPosFilt = sFilterTemplate.find("</ItemGroup>");
    uiFindPosFilt = sFilterTemplate.find_last_not_of(sWhiteSpace, uiFindPosFilt - 1) + 1; //handle potential differences in line endings
    set<string>::iterator sitIt = vFoundFilters.begin();
    uint uiCurrentKey = 0;
    string sAddFilters;
    while (sitIt != vFoundFilters.end()) {
        sAddFilters += sFilterAdd;
        sAddFilters += *sitIt;
        sAddFilters += sFilterAdd2;
        sAddFilters += asFilterKeys[uiCurrentKey];
        uiCurrentKey++;
        sAddFilters += sFilterAddClose;
        //next
        sitIt++;
    }
    //Add to string
    sFilterTemplate.insert(uiFindPosFilt, sAddFilters);
}

bool ProjectGenerator::outputProjectExports(const StaticList& vIncludeDirs)
{
    outputLine("  Generating project exports file (" + m_sProjectName + ")...");
    string sExportList;
    if (!findFile(this->m_sProjectDir + "/*.v", sExportList)) {
        outputError("Failed finding project exports (" + m_sProjectName + ")");
        return false;
    }

    //Open the input export file
    string sExportsFile;
    loadFromFile(this->m_sProjectDir + sExportList, sExportsFile);

    //Search for start of global tag
    string sGlobal = "global:";
    StaticList vExportStrings;
    uint uiFindPos = sExportsFile.find(sGlobal);
    if (uiFindPos != string::npos) {
        //Remove everything outside the global section
        uiFindPos += sGlobal.length();
        uint uiFindPos2 = sExportsFile.find("local:", uiFindPos);
        sExportsFile = sExportsFile.substr(uiFindPos, uiFindPos2 - uiFindPos);

        //Remove any comments
        uiFindPos = sExportsFile.find('#');
        while (uiFindPos != string::npos) {
            //find end of line
            uiFindPos2 = sExportsFile.find(10, uiFindPos + 1); //10 is line feed
            sExportsFile.erase(uiFindPos, uiFindPos2 - uiFindPos + 1);
            uiFindPos = sExportsFile.find('#', uiFindPos + 1);
        }

        //Clean any remaining white space out
        sExportsFile.erase(remove_if(sExportsFile.begin(), sExportsFile.end(), ::isspace), sExportsFile.end());

        //Get any export strings
        uiFindPos = 0;
        uiFindPos2 = sExportsFile.find(';');
        while (uiFindPos2 != string::npos) {
            vExportStrings.push_back(sExportsFile.substr(uiFindPos, uiFindPos2 - uiFindPos));
            uiFindPos = uiFindPos2 + 1;
            uiFindPos2 = sExportsFile.find(';', uiFindPos);
        }
    } else {
        outputError("Failed finding global start in project exports (" + sExportList + ")");
        return false;
    }

    //Split each source file into different directories to avoid name clashes
    map<string, StaticList> mDirectoryObjects;
    for (StaticList::iterator itI = m_vCIncludes.begin(); itI < m_vCIncludes.end(); itI++) {
        //Several input source files have the same name so we need to explicitly specify an output object file otherwise they will clash
        uint uiPos = itI->rfind("../");
        uiPos = (uiPos == string::npos) ? 0 : uiPos + 3;
        uint uiPos2 = itI->rfind('/');
        uiPos2 = (uiPos2 == string::npos) ? string::npos : uiPos2 - uiPos;
        string sFolderName = itI->substr(uiPos, uiPos2);
        mDirectoryObjects[sFolderName].push_back(*itI);
    }
    for (StaticList::iterator itI = m_vCPPIncludes.begin(); itI < m_vCPPIncludes.end(); itI++) {
        //Several input source files have the same name so we need to explicitly specify an output object file otherwise they will clash
        uint uiPos = itI->rfind("../");
        uiPos = (uiPos == string::npos) ? 0 : uiPos + 3;
        uint uiPos2 = itI->rfind('/');
        uiPos2 = (uiPos2 == string::npos) ? string::npos : uiPos2 - uiPos;
        string sFolderName = itI->substr(uiPos, uiPos2);
        mDirectoryObjects[sFolderName].push_back(*itI);
    }

    if (!runCompiler(vIncludeDirs, mDirectoryObjects, 0))
        return false;

    //Loaded in the compiler passed files
    StaticList vSBRFiles;
    StaticList vModuleExports;
    StaticList vModuleDataExports;
    string sTempFolder = sTempDirectory + m_sProjectName;
    findFiles(sTempFolder + "/*.sbr", vSBRFiles);
    for (StaticList::iterator itSBR = vSBRFiles.begin(); itSBR < vSBRFiles.end(); itSBR++) {
        string sSBRFile;
        loadFromFile(*itSBR, sSBRFile, true);

        //Search through file for module exports
        for (StaticList::iterator itI = vExportStrings.begin(); itI < vExportStrings.end(); itI++) {
            //SBR files contain data in specif formats
            // NULL SizeOfID Type Imp NULL ID Name NULL
            // where:
            // SizeOfID specifies how many characters are in the ID
            //  ETX=2
            //  C=3
            // Type specifies the type of the entry (function, data, define etc.)
            //  BEL=typedef
            //  EOT=data
            //  SOH=function
            //  ENQ=pre-processor define
            // Imp specifies if this is an declaration or a definition
            //  `=declaration
            //  @=definition
            //  STX=static or inline
            //  NULL=pre-processor
            // ID is a 2 or 3 character sequence used to uniquely identify the object

            //Check if it is a wild card search
            uiFindPos = itI->find('*');
            if (uiFindPos != string::npos) {
                //Strip the wild card (Note: assumes wild card is at the end!)
                string sSearch = itI->substr(0, uiFindPos);

                //Search for all occurrences
                uiFindPos = sSBRFile.find(sSearch);
                while (uiFindPos != string::npos) {
                    //Find end of name signalled by NULL character
                    uint uiFindPos2 = sSBRFile.find((char)0x00, uiFindPos + 1);
                    if (uiFindPos2 == string::npos) {
                        uiFindPos = uiFindPos2;
                        break;
                    }

                    //Check if this is a define
                    uint uiFindPos3 = sSBRFile.rfind((char)0x00, uiFindPos - 3);
                    while (sSBRFile.at(uiFindPos3 - 1) == (char)0x00) {
                        //Skip if there was a NULL in ID
                        --uiFindPos3;
                    }
                    uint uiFindPosDiff = uiFindPos - uiFindPos3;
                    if ((sSBRFile.at(uiFindPos3 - 1) == '@') &&
                        (((uiFindPosDiff == 3) && (sSBRFile.at(uiFindPos3 - 3) == (char)0x03)) ||
                        ((uiFindPosDiff == 4) && (sSBRFile.at(uiFindPos3 - 3) == 'C')))) {
                        //Check if this is a data or function name
                        string sFoundName = sSBRFile.substr(uiFindPos, uiFindPos2 - uiFindPos);
                        if ((sSBRFile.at(uiFindPos3 - 2) == (char)0x01)) {
                            //This is a function
                            if (find(vModuleExports.begin(), vModuleExports.end(), sFoundName) == vModuleExports.end()) {
                                vModuleExports.push_back(sFoundName);
                            }
                        } else if ((sSBRFile.at(uiFindPos3 - 2) == (char)0x04)) {
                            //This is data
                            if (find(vModuleDataExports.begin(), vModuleDataExports.end(), sFoundName) == vModuleDataExports.end()) {
                                vModuleDataExports.push_back(sFoundName);
                            }
                        }
                    }

                    //Get next
                    uiFindPos = sSBRFile.find(sSearch, uiFindPos2 + 1);
                }
            } else {
                uiFindPos = sSBRFile.find(*itI);
                //Make sure the match is an exact one
                uint uiFindPos3;
                while ((uiFindPos != string::npos)) {
                    if (sSBRFile.at(uiFindPos + itI->length()) == (char)0x00) {
                        uiFindPos3 = sSBRFile.rfind((char)0x00, uiFindPos - 3);
                        while (sSBRFile.at(uiFindPos3 - 1) == (char)0x00) {
                            //Skip if there was a NULL in ID
                            --uiFindPos3;
                        }
                        uint uiFindPosDiff = uiFindPos - uiFindPos3;
                        if ((sSBRFile.at(uiFindPos3 - 1) == '@') &&
                            (((uiFindPosDiff == 3) && (sSBRFile.at(uiFindPos3 - 3) == (char)0x03)) ||
                            ((uiFindPosDiff == 4) && (sSBRFile.at(uiFindPos3 - 3) == 'C')))) {
                            break;
                        }
                    }
                    uiFindPos = sSBRFile.find(*itI, uiFindPos + 1);
                }
                if (uiFindPos == string::npos) {
                    continue;
                }
                //Check if this is a data or function name
                if ((sSBRFile.at(uiFindPos3 - 2) == (char)0x01)) {
                    //This is a function
                    if (find(vModuleExports.begin(), vModuleExports.end(), *itI) == vModuleExports.end()) {
                        vModuleExports.push_back(*itI);
                    }
                } else if ((sSBRFile.at(uiFindPos3 - 2) == (char)0x04)) {
                    //This is data
                    if (find(vModuleDataExports.begin(), vModuleDataExports.end(), *itI) == vModuleDataExports.end()) {
                        vModuleDataExports.push_back(*itI);
                    }
                }
            }
        }
    }
    //Remove the test sbr files
    deleteFolder(sTempFolder);

    //Check for any exported functions in asm files
    for (StaticList::iterator itASM = m_vASMIncludes.begin(); itASM < m_vASMIncludes.end(); itASM++) {
        string sASMFile;
        loadFromFile(m_ConfigHelper.m_sProjectDirectory + *itASM, sASMFile);

        //Search through file for module exports
        for (StaticList::iterator itI = vExportStrings.begin(); itI < vExportStrings.end(); itI++) {
            //Check if it is a wild card search
            uiFindPos = itI->find('*');
            const string sInvalidChars = ",.(){}[]`'\"+-*/!@#$%^&*<>|;\\= \r\n\t\0";
            if (uiFindPos != string::npos) {
                //Strip the wild card (Note: assumes wild card is at the end!)
                string sSearch = ' ' + itI->substr(0, uiFindPos);
                //Search for all occurrences
                uiFindPos = sASMFile.find(sSearch);
                while ((uiFindPos != string::npos) && (uiFindPos > 0)) {
                    //Find end of name signaled by first non valid character
                    uint uiFindPos2 = sASMFile.find_first_of(sInvalidChars, uiFindPos + 1);
                    //Check this is valid function definition
                    if ((sASMFile.at(uiFindPos2) == '(') && (sInvalidChars.find(sASMFile.at(uiFindPos - 1)) == string::npos)) {
                        string sFoundName = sASMFile.substr(uiFindPos, uiFindPos2 - uiFindPos);
                        if (find(vModuleExports.begin(), vModuleExports.end(), sFoundName) == vModuleExports.end()) {
                            vModuleExports.push_back(sFoundName.substr(1));
                        }
                    }

                    //Get next
                    uiFindPos = sASMFile.find(sSearch, uiFindPos2 + 1);
                }
            } else {
                string sSearch = ' ' + *itI + '(';
                uiFindPos = sASMFile.find(*itI);
                //Make sure the match is an exact one
                if ((uiFindPos != string::npos) && (uiFindPos > 0) && (sInvalidChars.find(sASMFile.at(uiFindPos - 1)) == string::npos)) {
                    //Check this is valid function definition
                    if (find(vModuleExports.begin(), vModuleExports.end(), *itI) == vModuleExports.end()) {
                        vModuleExports.push_back(*itI);
                    }
                }
            }
        }
    }

    //Sort the exports
    sort(vModuleExports.begin(), vModuleExports.end());
    sort(vModuleDataExports.begin(), vModuleDataExports.end());

    //Create the export module string
    string sModuleFile = "EXPORTS\r\n";
    for (StaticList::iterator itI = vModuleExports.begin(); itI < vModuleExports.end(); itI++) {
        sModuleFile += "    " + *itI + "\r\n";
    }
    for (StaticList::iterator itI = vModuleDataExports.begin(); itI < vModuleDataExports.end(); itI++) {
        sModuleFile += "    " + *itI + " DATA\r\n";
    }

    string sDestinationFile = m_ConfigHelper.m_sProjectDirectory + m_sProjectName + ".def";
    if (!writeToFile(sDestinationFile, sModuleFile, true)) {
        return false;
    }
    return true;
}

void ProjectGenerator::outputBuildEvents(string & sProjectTemplate)
{
    //After </Lib> and </Link> and the post and then pre build events
    const string asLibLink[2] = {"</Lib>", "</Link>"};
    const string sPostbuild = "\r\n    <PostBuildEvent>\r\n\
      <Command>";
    const string sPostbuildClose = "</Command>\r\n\
    </PostBuildEvent>";
    const string sInclude = "mkdir \"$(OutDir)\"\\include\r\n\
mkdir \"$(OutDir)\"\\include\\";
    const string sCopy = "\r\ncopy ";
    const string sCopyEnd = " \"$(OutDir)\"\\include\\";
    const string sLicense = "\r\nmkdir \"$(OutDir)\"\\licenses";
    string sLicenseName = m_ConfigHelper.m_sProjectName;
    transform(sLicenseName.begin(), sLicenseName.end(), sLicenseName.begin(), ::tolower);
    const string sLicenseEnd = " \"$(OutDir)\"\\licenses\\" + sLicenseName + ".txt";
    const string sPrebuild = "\r\n    <PreBuildEvent>\r\n\
      <Command>if exist template_rootdirconfig.h (\r\n\
del template_rootdirconfig.h\r\n\
)\r\n\
if exist template_rootdirversion.h (\r\n\
del template_rootdirversion.h\r\n\
)\r\n\
if exist template_rootdirconfig.asm (\r\n\
del template_rootdirconfig.asm\r\n\
)\r\n\
if exist template_rootdirlibavutil\\avconfig.h (\r\n\
del template_rootdirlibavutil\\avconfig.h\r\n\
)\r\n\
if exist template_rootdirlibavutil\\ffversion.h (\r\n\
del template_rootdirlibavutil\\ffversion.h\r\n\
)";
    const string sPrebuildDir = "\r\nif exist \"$(OutDir)\"\\include\\" + m_sProjectName + " (\r\n\
rd /s /q \"$(OutDir)\"\\include\\" + m_sProjectName + "\r\n\
cd ../\r\n\
cd $(ProjectDir)\r\n\
)";
    const string sPrebuildClose = "</Command>\r\n    </PreBuildEvent>";
    //Get the correct license file
    string sLicenseFile;
    if (m_ConfigHelper.getConfigOption("nonfree")->m_sValue.compare("1") == 0) {
        sLicenseFile = "template_rootdirCOPYING.GPLv3"; //Technically this has no license as it is unredistributable but we get the closest thing for now
    } else if (m_ConfigHelper.getConfigOption("gplv3")->m_sValue.compare("1") == 0) {
        sLicenseFile = "template_rootdirCOPYING.GPLv3";
    } else if (m_ConfigHelper.getConfigOption("lgplv3")->m_sValue.compare("1") == 0) {
        sLicenseFile = "template_rootdirCOPYING.LGPLv3";
    } else if (m_ConfigHelper.getConfigOption("gpl")->m_sValue.compare("1") == 0) {
        sLicenseFile = "template_rootdirCOPYING.GPLv2";
    } else {
        sLicenseFile = "template_rootdirCOPYING.LGPLv2.1";
    }
    //Generate the pre build and post build string
    string sAdditional;
    //Add the post build event
    sAdditional += sPostbuild;
    if (m_vHIncludes.size() > 0) {
        sAdditional += sInclude;
        sAdditional += m_sProjectName;
        for (StaticList::iterator vitHeaders = m_vHIncludes.begin(); vitHeaders < m_vHIncludes.end(); vitHeaders++) {
            sAdditional += sCopy;
            string sHeader = *vitHeaders;
            replace(sHeader.begin(), sHeader.end(), '/', '\\');
            sAdditional += sHeader;
            sAdditional += sCopyEnd;
            sAdditional += m_sProjectName;
        }
    }
    //Output license
    sAdditional += sLicense;
    sAdditional += sCopy;
    sAdditional += sLicenseFile;
    sAdditional += sLicenseEnd;
    sAdditional += sPostbuildClose;
    //Add the pre build event
    sAdditional += sPrebuild;
    if (m_vHIncludes.size() > 0) {
        sAdditional += sPrebuildDir;
    }
    sAdditional += sPrebuildClose;

    for (uint uiI = 0; uiI < 2; uiI++) {
        uint uiFindPos = sProjectTemplate.find(asLibLink[uiI]);
        while (uiFindPos != string::npos) {
            uiFindPos += asLibLink[uiI].length();
            //Add to output
            sProjectTemplate.insert(uiFindPos, sAdditional);
            uiFindPos += sAdditional.length();
            //Get next
            uiFindPos = sProjectTemplate.find(asLibLink[uiI], uiFindPos + 1);
        }
    }
}

void ProjectGenerator::outputIncludeDirs(const StaticList & vIncludeDirs, string & sProjectTemplate)
{
    if (!vIncludeDirs.empty()) {
        string sAddInclude;
        for (StaticList::const_iterator vitIt = vIncludeDirs.cbegin(); vitIt < vIncludeDirs.cend(); vitIt++) {
            sAddInclude += *vitIt + ";";
        }
        replace(sAddInclude.begin(), sAddInclude.end(), '/', '\\');
        const string sAddIncludeDir = "<AdditionalIncludeDirectories>";
        uint uiFindPos = sProjectTemplate.find(sAddIncludeDir);
        while (uiFindPos != string::npos) {
            //Add to output
            uiFindPos += sAddIncludeDir.length(); //Must be added first so that it is before $(IncludePath) as otherwise there are errors
            sProjectTemplate.insert(uiFindPos, sAddInclude);
            uiFindPos += sAddInclude.length();
            //Get next
            uiFindPos = sProjectTemplate.find(sAddIncludeDir, uiFindPos + 1);
        }
    }
}

void ProjectGenerator::outputLibDirs(const StaticList & vLib32Dirs, const StaticList & vLib64Dirs, string & sProjectTemplate)
{
    if ((!vLib32Dirs.empty()) || (!vLib64Dirs.empty())) {
        //Add additional lib includes to include list based on current config
        string sAddLibs[2];
        for (StaticList::const_iterator vitIt = vLib32Dirs.cbegin(); vitIt < vLib32Dirs.cend(); vitIt++) {
            sAddLibs[0] += *vitIt + ";";
        }
        for (StaticList::const_iterator vitIt = vLib64Dirs.cbegin(); vitIt < vLib64Dirs.cend(); vitIt++) {
            sAddLibs[1] += *vitIt + ";";
        }
        replace(sAddLibs[0].begin(), sAddLibs[0].end(), '/', '\\');
        replace(sAddLibs[1].begin(), sAddLibs[1].end(), '/', '\\');
        const string sAddLibDir = "<AdditionalLibraryDirectories>";
        uint ui32Or64 = 0; //start with 32 (assumes projects are ordered 32 then 64 recursive)
        uint uiFindPos = sProjectTemplate.find(sAddLibDir);
        while (uiFindPos != string::npos) {
            //Add to output
            uiFindPos += sAddLibDir.length();
            sProjectTemplate.insert(uiFindPos, sAddLibs[ui32Or64]);
            uiFindPos += sAddLibs[ui32Or64].length();
            //Get next
            uiFindPos = sProjectTemplate.find(sAddLibDir, uiFindPos + 1);
            ui32Or64 = !ui32Or64;
        }
    }
}

void ProjectGenerator::outputDefines(const StaticList & vDefines, string & sProjectTemplate)
{
    if (!vDefines.empty()) {
        string sDefines;
        for (StaticList::const_iterator vitIt = vDefines.cbegin(); vitIt < vDefines.cend(); vitIt++) {
            sDefines += *vitIt + ";";
        }
        const string sAddDefines = "<PreprocessorDefinitions>";
        uint uiFindPos = sProjectTemplate.find(sAddDefines);
        while (uiFindPos != string::npos) {
            //Add to output
            uiFindPos += sAddDefines.length();
            sProjectTemplate.insert(uiFindPos, sDefines);
            uiFindPos += sDefines.length();
            //Get next
            uiFindPos = sProjectTemplate.find(sDefines, uiFindPos + 1);
        }
    }
}

void ProjectGenerator::outputYASMTools(string & sProjectTemplate)
{
    const string sYASMDefines = "\r\n\
    <YASM>\r\n\
      <IncludePaths>$(ProjectDir);$(ProjectDir)\\template_rootdir;%(IncludePaths)</IncludePaths>\r\n\
      <PreIncludeFiles>config.asm;%(PreIncludeFiles)</PreIncludeFiles>\r\n\
      <GenerateDebugInformation>true</GenerateDebugInformation>\r\n\
    </YASM>";
    const string sYASMProps = "\r\n\
  <ImportGroup Label=\"ExtensionSettings\">\r\n\
    <Import Project=\"$(VCTargetsPath)\\BuildCustomizations\\yasm.props\" />\r\n\
  </ImportGroup>";
    const string sYASMTargets = "\r\n\
  <ImportGroup Label=\"ExtensionTargets\">\r\n\
    <Import Project=\"$(VCTargetsPath)\\BuildCustomizations\\yasm.targets\" />\r\n\
  </ImportGroup>";
    const string sFindProps = "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />";
    const string sFindTargets = "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />";

        //Add YASM defines
    const string sEndPreBuild = "</PreBuildEvent>";
    uint uiFindPos = sProjectTemplate.find(sEndPreBuild);
    while (uiFindPos != string::npos) {
        uiFindPos += sEndPreBuild.length();
        //Add to output
        sProjectTemplate.insert(uiFindPos, sYASMDefines);
        uiFindPos += sYASMDefines.length();
        //Get next
        uiFindPos = sProjectTemplate.find(sEndPreBuild, uiFindPos + 1);
    }

    //Add YASM build customisation
    uiFindPos = sProjectTemplate.find(sFindProps) + sFindProps.length();
    //After <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" /> add yasm props
    sProjectTemplate.insert(uiFindPos, sYASMProps);
    uiFindPos += sYASMProps.length();
    uiFindPos = sProjectTemplate.find(sFindTargets) + sFindTargets.length();
    //After <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" /> add yasm target
    sProjectTemplate.insert(uiFindPos, sYASMTargets);
    uiFindPos += sYASMTargets.length();
}

void ProjectGenerator::outputNASMTools(string & sProjectTemplate)
{
    const string sNASMDefines = "\r\n\
    <NASM>\r\n\
      <IncludePaths>$(ProjectDir);$(ProjectDir)\\template_rootdir;$(ProjectDir)\\template_rootdir\\libavutil\\x86;%(IncludePaths)</IncludePaths>\r\n\
      <PreIncludeFiles>config.asm;%(PreIncludeFiles)</PreIncludeFiles>\r\n\
      <GenerateDebugInformation>true</GenerateDebugInformation>\r\n\
    </NASM>";
    const string sNASMProps = "\r\n\
  <ImportGroup Label=\"ExtensionSettings\">\r\n\
    <Import Project=\"$(VCTargetsPath)\\BuildCustomizations\\r\nasm.props\" />\r\n\
  </ImportGroup>";
    const string sNASMTargets = "\r\n\
  <ImportGroup Label=\"ExtensionTargets\">\r\n\
    <Import Project=\"$(VCTargetsPath)\\BuildCustomizations\\r\nasm.targets\" />\r\n\
  </ImportGroup>";
    const string sFindProps = "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />";
    const string sFindTargets = "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />";

        //Add NASM defines
    const string sEndPreBuild = "</PreBuildEvent>";
    uint uiFindPos = sProjectTemplate.find(sEndPreBuild);
    while (uiFindPos != string::npos) {
        uiFindPos += sEndPreBuild.length();
        //Add to output
        sProjectTemplate.insert(uiFindPos, sNASMDefines);
        uiFindPos += sNASMDefines.length();
        //Get next
        uiFindPos = sProjectTemplate.find(sEndPreBuild, uiFindPos + 1);
    }

    //Add NASM build customisation
    uiFindPos = sProjectTemplate.find(sFindProps) + sFindProps.length();
    //After <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" /> add nasm props
    sProjectTemplate.insert(uiFindPos, sNASMProps);
    uiFindPos += sNASMProps.length();
    uiFindPos = sProjectTemplate.find(sFindTargets) + sFindTargets.length();
    //After <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" /> add nasm target
    sProjectTemplate.insert(uiFindPos, sNASMTargets);
    uiFindPos += sNASMTargets.length();
}

void ProjectGenerator::outputASMTools(string & sProjectTemplate)
{
    if (m_ConfigHelper.isASMEnabled() && (m_vASMIncludes.size() > 0)) {
#if USENASM
        outputNASMTools(sProjectTemplate);
#else
        outputYASMTools(sProjectTemplate);
#endif
    }
}

bool ProjectGenerator::outputDependencyLibs(string & sProjectTemplate, bool bProgram)
{
    //Check current libs list for valid lib names
    for (StaticList::iterator vitLib = m_vLibs.begin(); vitLib < m_vLibs.end(); vitLib++) {
        //prepend lib if needed
        if (vitLib->find("lib") != 0) {
            *vitLib = "lib" + *vitLib;
        }
    }

    //Add additional dependencies based on current config to Libs list
    buildInterDependencies(m_vLibs);
    m_mProjectLibs[m_sProjectName] = m_vLibs; //Backup up current libs for solution
    StaticList vAddLibs;
    buildDependencies(m_vLibs, vAddLibs);

    if ((m_vLibs.size() > 0) || (vAddLibs.size() > 0)) {
        //Create list of additional ffmpeg dependencies
        string sAddFFmpegLibs[4]; //debug, release, debugDll, releaseDll
        for (StaticList::iterator vitLib = m_mProjectLibs[m_sProjectName].begin(); vitLib < m_mProjectLibs[m_sProjectName].end(); vitLib++) {
            sAddFFmpegLibs[0] += *vitLib;
            sAddFFmpegLibs[0] += "d.lib;";
            sAddFFmpegLibs[1] += *vitLib;
            sAddFFmpegLibs[1] += ".lib;";
            sAddFFmpegLibs[2] += vitLib->substr(3);
            sAddFFmpegLibs[2] += "d.lib;";
            sAddFFmpegLibs[3] += vitLib->substr(3);
            sAddFFmpegLibs[3] += ".lib;";
        }
        //Create List of additional dependencies
        string sAddDeps[4]; //debug, release, debugDll, releaseDll
        for (StaticList::iterator vitLib = m_vLibs.begin() + m_mProjectLibs[m_sProjectName].size(); vitLib < m_vLibs.end(); vitLib++) {
            sAddDeps[0] += *vitLib;
            sAddDeps[0] += "d.lib;";
            sAddDeps[1] += *vitLib;
            sAddDeps[1] += ".lib;";
            sAddDeps[2] += vitLib->substr(3);
            sAddDeps[2] += "d.lib;";
            sAddDeps[3] += vitLib->substr(3);
            sAddDeps[3] += ".lib;";
        }
        //Create List of additional external dependencies
        string sAddExternDeps;
        for (StaticList::iterator vitLib = vAddLibs.begin(); vitLib < vAddLibs.end(); vitLib++) {
            sAddExternDeps += *vitLib;
            sAddExternDeps += ".lib;";
        }
        //Add to Additional Dependencies
        string asLibLink2[2] = {"<Link>", "<Lib>"};
        for (uint uiLinkLib = 0; uiLinkLib < ((bProgram) ? 1u : 2u); uiLinkLib++) {
            //loop over each debug/release sequence
            uint uiFindPos = sProjectTemplate.find(asLibLink2[uiLinkLib]);
            for (uint uiDebugRelease = 0; uiDebugRelease < 2; uiDebugRelease++) {
                uint uiMax = ((uiDebugRelease == 0) && (uiLinkLib == 1)) ? 2 : 4; //No LTO option in debug
                //x86, x64, x86LTO/Static, x64LTO/Static -- x86, x64, x86DLL, x64DLL (projects)
                for (uint uiConf = 0; uiConf < uiMax; uiConf++) {
                    uiFindPos = sProjectTemplate.find("%(AdditionalDependencies)", uiFindPos);
                    if (uiFindPos == string::npos) {
                        outputError("Failed finding dependencies in template.");
                        return false;
                    }
                    uint uiAddIndex = uiDebugRelease;
                    if ((uiLinkLib == 0) && (((!bProgram) && (uiConf < 2)) || ((bProgram) && (uiConf >= 2)))) {
                        //Use DLL libs
                        uiAddIndex += 2;
                    }
                    string sAddString;
                    if (uiLinkLib == 0) {
                        //If the dependency is actually for one of the ffmpeg libs then we can ignore it in static linking mode
                        //  as this just causes unnecessary code bloat
                        if ((!bProgram) || (uiConf >= 2)) {
                            sAddString = sAddFFmpegLibs[uiDebugRelease + 2]; //Always link ffmpeg libs to the dll even in DLLStatic
                        } else if (bProgram) {
                            sAddString = sAddFFmpegLibs[uiDebugRelease];
                        }
                    }
                    //Add to output
                    sAddString += sAddDeps[uiAddIndex] + sAddExternDeps;
                    sProjectTemplate.insert(uiFindPos, sAddString);
                    uiFindPos += sAddString.length();
                    //Get next
                    uiFindPos = sProjectTemplate.find(asLibLink2[uiLinkLib], uiFindPos + 1);
                }
            }
        }
    }
    return true;
}