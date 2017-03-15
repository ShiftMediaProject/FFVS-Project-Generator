/*
 * copyright (c) 2014 Matthew Oliver
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

 //This can be used to force all detected DCE values to be output to file
 // whether they are enabled in current configuration or not
#define FORCEALLDCE 0

#define TEMPLATE_COMPAT_ID 100
#define TEMPLATE_MATH_ID 102
#define TEMPLATE_UNISTD_ID 104
#define TEMPLATE_SLN_ID 106
#define TEMPLATE_VCXPROJ_ID 108
#define TEMPLATE_FILTERS_ID 110
#define TEMPLATE_PROG_VCXPROJ_ID 112
#define TEMPLATE_PROG_FILTERS_ID 114
#define TEMPLATE_STDATOMIC_ID 116

bool ProjectGenerator::passAllMake()
{
    //Copy the required header files to output directory
    m_sTemplateDirectory = "./templates/";
    bool bCopy = copyResourceFile(TEMPLATE_COMPAT_ID, m_ConfigHelper.m_sProjectDirectory + "compat.h");
    if (!bCopy) {
        cout << "Error: Failed writing to output location. Make sure you have the appropriate user permissions." << endl;
        return false;
    }
    copyResourceFile(TEMPLATE_MATH_ID, m_ConfigHelper.m_sProjectDirectory + "math.h");
    copyResourceFile(TEMPLATE_UNISTD_ID, m_ConfigHelper.m_sProjectDirectory + "unistd.h");
    string sFileName;
    if (findFile(m_ConfigHelper.m_sRootDirectory + "compat/atomics/win32/stdatomic.h", sFileName)) {
        copyResourceFile(TEMPLATE_STDATOMIC_ID, m_ConfigHelper.m_sProjectDirectory + "stdatomic.h");
    }

    //Initialise internal values
    ConfigGenerator::DefaultValuesList Unneeded;
    m_ConfigHelper.buildReplaceValues(m_ReplaceValues, Unneeded);

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
                cout << "  Error: Could not locate directory for library (" << *vitLib << ")" << endl;
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
            m_vYASMIncludes.clear();
            m_vHIncludes.clear();
            m_vLibs.clear();
            m_mUnknowns.clear();
            m_sProjectDir.clear();
        }
    }

    //Output the solution file
    return outputSolution();
}

void ProjectGenerator::deleteCreatedFiles()
{
    //Delete any previously generated files
    vector<string> vExistingFiles;
    findFiles(m_ConfigHelper.m_sProjectDirectory + "*.vcxproj", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "*.filters", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "*.def", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "ffmpeg.sln", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "libav.sln", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "compat.h", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "math.h", vExistingFiles, false);
    findFiles(m_ConfigHelper.m_sProjectDirectory + "unistd.h", vExistingFiles, false);
    for (vector<string>::iterator itIt = vExistingFiles.begin(); itIt < vExistingFiles.end(); itIt++) {
        deleteFile(*itIt);
    }
    vector<string> vExistingFolders;
    findFolders(m_ConfigHelper.m_sProjectDirectory + "lib*", vExistingFolders);
    for (vector<string>::iterator itIt = vExistingFolders.begin(); itIt < vExistingFolders.end(); itIt++) {
        deleteFolder(*itIt);
    }
}

bool ProjectGenerator::outputProject()
{
    //Output the generated files
    uint uiSPos = m_sProjectDir.rfind('/', m_sProjectDir.length() - 2) + 1;
    string sProjectName = m_sProjectDir.substr(uiSPos, m_sProjectDir.length() - 1 - uiSPos);

    //Check all files are correctly located
    if (!checkProjectFiles(sProjectName)) {
        return false;
    }

    //Get dependency directories
    StaticList vIncludeDirs;
    StaticList vLib32Dirs;
    StaticList vLib64Dirs;
    buildDependencyDirs(sProjectName, vIncludeDirs, vLib32Dirs, vLib64Dirs);

    //Generate the exports file
    if (!outputProjectExports(sProjectName, vIncludeDirs)) {
        return false;
    }

    //Create missing definitions of functions removed by DCE
    if (!outputProjectDCE(sProjectName, vIncludeDirs)) {
        return false;
    }

    //We now have complete list of all the files that we need
    cout << "  Generating project file (" << sProjectName << ")..." << endl;

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
    outputSourceFiles(sProjectName, sProjectFile, sFiltersFile);

    //Add the build events
    outputBuildEvents(sProjectName, sProjectFile);

    //Add YASM requirements
    outputYASMTools(sProjectFile);

    //Add the dependency libraries
    if (!outputDependencyLibs(sProjectName, sProjectFile)) {
        return false;
    }

    //Add additional includes to include list
    outputIncludeDirs(vIncludeDirs, sProjectFile);

    //Add additional lib includes to include list
    outputLibDirs(vLib32Dirs, vLib64Dirs, sProjectFile);

    //Replace all template tag arguments
    outputTemplateTags(sProjectName, sProjectFile, sFiltersFile);

    //Write output project
    string sOutProjectFile = m_ConfigHelper.m_sProjectDirectory + sProjectName + ".vcxproj";
    if (!writeToFile(sOutProjectFile, sProjectFile)) {
        return false;
    }

    //Write output filters
    string sOutFiltersFile = m_ConfigHelper.m_sProjectDirectory + sProjectName + ".vcxproj.filters";
    if (!writeToFile(sOutFiltersFile, sFiltersFile)) {
        return false;
    }

    return true;
}

bool ProjectGenerator::outputProgramProject(const string& sProjectName, const string& sDestinationFile, const string& sDestinationFilterFile)
{
    //Pass makefile for program
    passProgramMake(sProjectName);

    //Check all files are correctly located
    if (!checkProjectFiles(sProjectName)) {
        return false;
    }

    //Get dependency directories
    StaticList vIncludeDirs;
    StaticList vLib32Dirs;
    StaticList vLib64Dirs;
    buildDependencyDirs(sProjectName, vIncludeDirs, vLib32Dirs, vLib64Dirs);

    //Create missing definitions of functions removed by DCE
    if (!outputProjectDCE(sProjectName, vIncludeDirs)) {
        return false;
    }

    //We now have complete list of all the files that we need
    cout << "  Generating project file (" << sProjectName << ")..." << endl;

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
    outputSourceFiles(sProjectName, sProgramFile, sProgramFiltersFile);

    //Add the build events
    outputBuildEvents(sProjectName, sProgramFile);

    //Add YASM requirements
    outputYASMTools(sProgramFile);

    //Add the dependency libraries
    if (!outputDependencyLibs(sProjectName, sProgramFile, true)) {
        return false;
    }

    //Add additional includes to include list
    outputIncludeDirs(vIncludeDirs, sProgramFile);

    //Add additional lib includes to include list
    outputLibDirs(vLib32Dirs, vLib64Dirs, sProgramFile);

    //Replace all template tag arguments
    outputTemplateTags(sProjectName, sProgramFile, sProgramFiltersFile);

    //Write program file
    if (!writeToFile(sDestinationFile, sProgramFile)) {
        return false;
    }

    //Write output filters
    if (!writeToFile(sDestinationFilterFile, sProgramFiltersFile)) {
        return false;
    }

    //Reset all internal values
    m_sInLine.clear();
    m_vIncludes.clear();
    m_vCPPIncludes.clear();
    m_vCIncludes.clear();
    m_vYASMIncludes.clear();
    m_vHIncludes.clear();
    m_vLibs.clear();
    m_mUnknowns.clear();

    return true;
}

bool ProjectGenerator::outputSolution()
{
    cout << "  Generating solution file..." << endl;
    m_sProjectDir = m_ConfigHelper.m_sRootDirectory;
    //Open the input temp project file
    string sSolutionFile;
    if (!loadFromResourceFile(TEMPLATE_SLN_ID, sSolutionFile)) {
        return false;
    }

    map<string, string> mKeys;
    buildProjectGUIDs(mKeys);
    string sSolutionKey = "8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942";

    vector<string> vAddedKeys;

    const string sProject = "\nProject(\"{";
    const string sProject2 = "}\") = \"";
    const string sProject3 = "\", \"";
    const string sProject4 = ".vcxproj\", \"{";
    const string sProjectEnd = "}\"";
    const string sProjectClose = "\nEndProject";

    const string sDepend = "\n	ProjectSection(ProjectDependencies) = postProject";
    const string sDependClose = "\n	EndProjectSection";
    const string sSubDepend = "\n		{";
    const string sSubDepend2 = "} = {";
    const string sSubDependEnd = "}";

    //Find the start of the file
    const string sFileStart = "Project";
    uint uiPos = sSolutionFile.find(sFileStart) - 1;

    map<string, StaticList>::iterator mitLibraries = m_mProjectLibs.begin();
    while (mitLibraries != m_mProjectLibs.end()) {
        //Check if this library has a known key (to lazy to auto generate at this time)
        if (mKeys.find(mitLibraries->first) == mKeys.end()) {
            cout << "  Error: Unknown library. Could not determine solution key (" << mitLibraries->first << ")" << endl;
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
                    cout << "  Error: Unknown library dependency. Could not determine solution key (" << *vitIt << ")" << endl;
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

        //next
        ++mitLibraries;
    }

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
    string sProjectAdd;
    vector<string> vAddedPrograms;
    map<string, string>::iterator mitPrograms = mProgramList.begin();
    while (mitPrograms != mProgramList.end()) {
        //Check if program is enabled
        const string sDestinationFile = m_ConfigHelper.m_sProjectDirectory + mitPrograms->first + ".vcxproj";
        const string sDestinationFilterFile = m_ConfigHelper.m_sProjectDirectory + mitPrograms->first + ".vcxproj.filters";
        if (m_ConfigHelper.getConfigOptionPrefixed(mitPrograms->second)->m_sValue.compare("1") == 0) {
            //Create project files for program
            outputProgramProject(mitPrograms->first, sDestinationFile, sDestinationFilterFile);

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
            StaticList::iterator vitLibs = m_mProjectLibs[mitPrograms->first].begin();
            while (vitLibs != m_mProjectLibs[mitPrograms->first].end()) {
                //Add all project libraries as dependencies (except avresample with ffmpeg)
                if (!m_ConfigHelper.m_bLibav && vitLibs->compare("libavresample") != 0) {
                    sProjectAdd += sSubDepend;
                    sProjectAdd += mKeys[*vitLibs];
                    sProjectAdd += sSubDepend2;
                    sProjectAdd += mKeys[*vitLibs];
                    sProjectAdd += sSubDependEnd;
                }
                //next
                ++vitLibs;
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
        sProjectAdd += "\nProject(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"Programs\", \"Programs\", \"{";
        sProjectAdd += sProgramKey;
        sProjectAdd += "}\"";
        sProjectAdd += "\nEndProject";

        //Insert into solution string
        sSolutionFile.insert(uiPos, sProjectAdd);
        uiPos += sProjectAdd.length();
    }

    //Next Add the solution configurations
    string sConfigStart = "GlobalSection(ProjectConfigurationPlatforms) = postSolution";
    uiPos = sSolutionFile.find(sConfigStart) + sConfigStart.length();
    string sConfigPlatform = "\n		{";
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
                //loop over build types (Don’t build projects by default)
                for (uint uiK = 0; uiK < 1; uiK++) {
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
        string sNest = "\n		{";
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
    if (!writeToFile(sOutSolutionFile, sSolutionFile)) {
        return false;
    }

    return true;
}

bool ProjectGenerator::passStaticIncludeObject(uint & uiStartPos, uint & uiEndPos, StaticList & vStaticIncludes)
{
    //Add the found string to internal storage
    uiEndPos = m_sInLine.find_first_of(". \t", uiStartPos);
    string sTag = m_sInLine.substr(uiStartPos, uiEndPos - uiStartPos);
    if (sTag.find('$') != string::npos) {
        // Invalid include. Occurs when include is actually a variable
        uiStartPos += 2;
        sTag = m_sInLine.substr(uiStartPos, m_sInLine.find(')', uiStartPos) - uiStartPos);
        // Check if additional variable (This happens when a string should be prepended to existing items within tag.)
        string sTag2;
        if (sTag.find(':') != string::npos) {
            uiStartPos = sTag.find(":%=");
            uint uiStartPos2 = uiStartPos + 3;
            uiEndPos = sTag.find('%', uiStartPos2);
            sTag2 = sTag.substr(uiStartPos2, uiEndPos - uiStartPos2);
            sTag = sTag.substr(0, uiStartPos);
        }
        // Get variable contents
        vector<string> vFiles;
        m_ConfigHelper.buildObjects(sTag, vFiles);
        if (sTag2.length() > 0) {
            //Prepend the full library path
            for (vector<string>::iterator vitFile = vFiles.begin(); vitFile < vFiles.end(); vitFile++) {
                *vitFile = sTag2 + *vitFile;
            }
        }
        //Loop through each item and add to list
        for (vector<string>::iterator vitFile = vFiles.begin(); vitFile < vFiles.end(); vitFile++) {
            //Check if object already included in internal list
            if (find(m_vCIncludes.begin(), m_vCIncludes.end(), *vitFile) == m_vCIncludes.end()) {
                vStaticIncludes.push_back(*vitFile);
                //cout << "  Found C Static: '" << *vitFile << "'" << endl;
            }
        }
        return true;
    }

    //Check if object already included in internal list
    if (find(vStaticIncludes.begin(), vStaticIncludes.end(), sTag) == vStaticIncludes.end()) {
        vStaticIncludes.push_back(sTag);
        //cout << "  Found Static: '" << sTag << "'" << endl;
    }
    return true;
}

bool ProjectGenerator::passStaticIncludeLine(uint uiStartPos, StaticList & vStaticIncludes)
{
    uint uiEndPos;
    if (!passStaticIncludeObject(uiStartPos, uiEndPos, vStaticIncludes)) {
        return false;
    }
    //Check if there are multiple files declared on the same line
    while (uiEndPos != string::npos) {
        uiStartPos = m_sInLine.find_first_of(" \t\\\n\0", uiEndPos);
        uiStartPos = m_sInLine.find_first_not_of(" \t\\\n\0", uiStartPos);
        if (uiStartPos == string::npos) {
            break;
        }
        if (!passStaticIncludeObject(uiStartPos, uiEndPos, vStaticIncludes)) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passStaticInclude(uint uiILength, StaticList & vStaticIncludes)
{
    //Remove the identifier and '='
    uint uiStartPos = m_sInLine.find_first_not_of(" +=:", uiILength);
    if (!passStaticIncludeLine(uiStartPos, vStaticIncludes)) {
        return true;
    }
    //Check if this is a multi line declaration
    while (m_sInLine.back() == '\\') {
        //Get the next line
        getline(m_ifInputFile, m_sInLine);
        //Remove the whitespace
        uiStartPos = m_sInLine.find_first_not_of(" \t");
        if (uiStartPos == string::npos) {
            break;
        }
        if (!passStaticIncludeLine(uiStartPos, vStaticIncludes)) {
            return true;
        }
    }
    return true;
}

bool ProjectGenerator::passDynamicIncludeObject(uint & uiStartPos, uint & uiEndPos, string & sIdent, StaticList & vIncludes)
{
    //Check if this is A valid File or a past compile option
    if (m_sInLine.at(uiStartPos) == '$') {
        uiEndPos = m_sInLine.find(')', uiStartPos);
        string sDynInc = m_sInLine.substr(uiStartPos + 2, uiEndPos - uiStartPos - 2);
        //Find it in the unknown list
        UnknownList::iterator mitObjectList = m_mUnknowns.find(sDynInc);
        if (mitObjectList != m_mUnknowns.end()) {
            //Loop over each internal object
            for (StaticList::iterator vitObject = mitObjectList->second.begin(); vitObject < mitObjectList->second.end(); vitObject++) {
                //Check if object already included in internal list
                if (find(vIncludes.begin(), vIncludes.end(), *vitObject) == vIncludes.end()) {
                    //Check if the config option is correct
                    ConfigGenerator::ValuesList::iterator vitOption = m_ConfigHelper.getConfigOptionPrefixed(sIdent);
                    if (vitOption == m_ConfigHelper.m_vConfigValues.end()) {
                        cout << "  Warning: Unknown dynamic configuration option (" << sIdent << ") used when passing object (" << *vitObject << ")" << endl;
                        return true;
                    }
                    if (vitOption->m_sValue.compare("1") == 0) {
                        vIncludes.push_back(*vitObject);
                        //cout << "  Found Dynamic: '" << *vitObject << "', '" << "( " + sIdent + " && " + sDynInc + " )" << "'" << endl;
                    }
                }
            }
        } else {
            cout << "  Error: Found unknown token (" << sDynInc << ")" << endl;
            return false;
        }
    } else if (m_sInLine.at(uiStartPos) == '#') {
        //Found a comment, just skip till end of line
        uiEndPos = m_sInLine.length();
        return true;
    } else {
        //Check for condition
        string sCompare = "1";
        if (sIdent.at(0) == '!') {
            sIdent = sIdent.substr(1);
            sCompare = "0";
        }
        uiEndPos = m_sInLine.find_first_of(". \t", uiStartPos);
        //Add the found string to internal storage
        string sTag = m_sInLine.substr(uiStartPos, uiEndPos - uiStartPos);
        //Check if object already included in internal list
        if (find(vIncludes.begin(), vIncludes.end(), sTag) == vIncludes.end()) {
            //Check if the config option is correct
            ConfigGenerator::ValuesList::iterator vitOption = m_ConfigHelper.getConfigOptionPrefixed(sIdent);
            if (vitOption == m_ConfigHelper.m_vConfigValues.end()) {
                cout << "  Warning: Unknown dynamic configuration option (" << sIdent << ") used when passing object (" << sTag << ")" << endl;
                return true;
            }
            if (vitOption->m_sValue.compare(sCompare) == 0) {
                //Check if the config option is for a reserved type
                if (m_ReplaceValues.find(sIdent) != m_ReplaceValues.end()) {
                    m_mReplaceIncludes[sTag].push_back(sIdent);
                    //cout << "  Found Dynamic Replace: '" << sTag << "', '" << sIdent << "'" << endl;
                } else {
                    vIncludes.push_back(sTag);
                    //cout << "  Found Dynamic: '" << sTag << "', '" << sIdent << "'" << endl;
                }
            }
        }
    }
    return true;
}

bool ProjectGenerator::passDynamicIncludeLine(uint uiStartPos, string & sIdent, StaticList & vIncludes)
{
    uint uiEndPos;
    if (!passDynamicIncludeObject(uiStartPos, uiEndPos, sIdent, vIncludes)) {
        return false;
    }
    //Check if there are multiple files declared on the same line
    while (uiEndPos != string::npos) {
        uiStartPos = m_sInLine.find_first_of(" \t\\\n\0", uiEndPos);
        uiStartPos = m_sInLine.find_first_not_of(" \t\\\n\0", uiStartPos);
        if (uiStartPos == string::npos) {
            break;
        }
        if (!passDynamicIncludeObject(uiStartPos, uiEndPos, sIdent, vIncludes)) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passDynamicInclude(uint uiILength, StaticList & vIncludes)
{
    //Find the dynamic identifier
    uint uiStartPos = m_sInLine.find_first_not_of("$( \t", uiILength);
    uint uiEndPos = m_sInLine.find(')', uiStartPos);
    string sIdent = m_sInLine.substr(uiStartPos, uiEndPos - uiStartPos);
    //Find the included obj
    uiStartPos = m_sInLine.find_first_not_of("+=: \t", uiEndPos + 1);
    if (!passDynamicIncludeLine(uiStartPos, sIdent, vIncludes)) {
        return false;
    }
    //Check if this is a multi line declaration
    while (m_sInLine.back() == '\\') {
        //Get the next line
        getline(m_ifInputFile, m_sInLine);
        //Remove the whitespace
        uiStartPos = m_sInLine.find_first_not_of(" \t");
        if (uiStartPos == string::npos) {
            break;
        }
        if (!passDynamicIncludeLine(uiStartPos, sIdent, vIncludes)) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passCInclude()
{
    return passStaticInclude(4, m_vIncludes);
}

bool ProjectGenerator::passDCInclude()
{
    return passDynamicInclude(5, m_vIncludes);
}

bool ProjectGenerator::passYASMInclude()
{
    //Check if supported option
    if (m_ConfigHelper.getConfigOptionPrefixed("HAVE_YASM")->m_sValue.compare("1") == 0) {
        return passStaticInclude(9, m_vIncludes);
    }
    return true;
}

bool ProjectGenerator::passDYASMInclude()
{
    //Check if supported option
    if (m_ConfigHelper.getConfigOptionPrefixed("HAVE_YASM")->m_sValue.compare("1") == 0) {
        return passDynamicInclude(10, m_vIncludes);
    }
    return true;
}

bool ProjectGenerator::passMMXInclude()
{
    //Check if supported option
    if (m_ConfigHelper.getConfigOptionPrefixed("HAVE_MMX")->m_sValue.compare("1") == 0) {
        return passStaticInclude(8, m_vIncludes);
    }
    return true;
}

bool ProjectGenerator::passDMMXInclude()
{
    //Check if supported option
    if (m_ConfigHelper.getConfigOptionPrefixed("HAVE_MMX")->m_sValue.compare("1") == 0) {
        return passDynamicInclude(9, m_vIncludes);
    }
    return true;
}

bool ProjectGenerator::passHInclude(uint uiCutPos)
{
    return passStaticInclude(uiCutPos, m_vHIncludes);
}

bool ProjectGenerator::passDHInclude()
{
    return passDynamicInclude(8, m_vHIncludes);
}

bool ProjectGenerator::passLibInclude()
{
    return passStaticInclude(6, m_vLibs);
}

bool ProjectGenerator::passDLibInclude()
{
    return passDynamicInclude(7, m_vLibs);
}

bool ProjectGenerator::passDUnknown()
{
    //Find the dynamic identifier
    uint uiStartPos = m_sInLine.find("$(");
    uint uiEndPos = m_sInLine.find(')', uiStartPos);
    string sPrefix = m_sInLine.substr(0, uiStartPos) + "yes";
    uiStartPos += 2; //Skip the $(
    string sIdent = m_sInLine.substr(uiStartPos, uiEndPos - uiStartPos);
    //Find the included obj
    uiStartPos = m_sInLine.find_first_not_of("+= \t", uiEndPos + 1);
    if (!passDynamicIncludeLine(uiStartPos, sIdent, m_mUnknowns[sPrefix])) {
        return false;
    }
    //Check if this is a multi line declaration
    while (m_sInLine.back() == '\\') {
        //Get the next line
        getline(m_ifInputFile, m_sInLine);
        //Remove the whitespace
        uiStartPos = m_sInLine.find_first_not_of(" \t");
        if (uiStartPos == string::npos) {
            break;
        }
        if (!passDynamicIncludeLine(uiStartPos, sIdent, m_mUnknowns[sPrefix])) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passDLibUnknown()
{
    //Find the dynamic identifier
    uint uiStartPos = m_sInLine.find("$(");
    uint uiEndPos = m_sInLine.find(')', uiStartPos);
    string sPrefix = m_sInLine.substr(0, uiStartPos) + "yes";
    uiStartPos += 2; //Skip the $(
    string sIdent = m_sInLine.substr(uiStartPos, uiEndPos - uiStartPos);
    //Find the included obj
    uiStartPos = m_sInLine.find_first_not_of("+= \t", uiEndPos + 1);
    if (!passDynamicIncludeLine(uiStartPos, sIdent, m_mUnknowns[sPrefix])) {
        return false;
    }
    //Check if this is a multi line declaration
    while (m_sInLine.back() == '\\') {
        //Get the next line
        getline(m_ifInputFile, m_sInLine);
        //Remove the whitespace
        uiStartPos = m_sInLine.find_first_not_of(" \t");
        if (uiStartPos == string::npos) {
            break;
        }
        if (!passDynamicIncludeLine(uiStartPos, sIdent, m_mUnknowns[sPrefix])) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passMake()
{
    cout << "  Generating from Makefile (" << m_sProjectDir << ")..." << endl;
    //Open the input Makefile
    string sMakeFile = m_sProjectDir + "/MakeFile";
    m_ifInputFile.open(sMakeFile);
    if (m_ifInputFile.is_open()) {
        //Read each line in the MakeFile
        while (getline(m_ifInputFile, m_sInLine)) {
            //Check what information is included in the current line
            if (m_sInLine.substr(0, 4).compare("OBJS") == 0) {
                //Found some c includes
                if (m_sInLine.at(4) == '-') {
                    //Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    //Found some static c includes
                    if (!passCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.substr(0, 9).compare("YASM-OBJS") == 0) {
                //Found some YASM includes
                if (m_sInLine.at(9) == '-') {
                    //Found some dynamic YASM includes
                    if (!passDYASMInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    //Found some static YASM includes
                    if (!passYASMInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.substr(0, 8).compare("MMX-OBJS") == 0) {
                //Found some YASM includes
                if (m_sInLine.at(8) == '-') {
                    //Found some dynamic YASM includes
                    if (!passDMMXInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    //Found some static YASM includes
                    if (!passMMXInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.substr(0, 7).compare("HEADERS") == 0) {
                //Found some headers
                if (m_sInLine.at(7) == '-') {
                    //Found some dynamic headers
                    if (!passDHInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    //Found some static headers
                    if (!passHInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.find("BUILT_HEADERS") == 0) {
                //Found some static built headers
                if (!passHInclude(13)) {
                    m_ifInputFile.close();
                    return false;
                }
            } else if (m_sInLine.substr(0, 6).compare("FFLIBS") == 0) {
                //Found some libs
                if (m_sInLine.at(6) == '-') {
                    //Found some dynamic libs
                    if (!passDLibInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    //Found some static libs
                    if (!passLibInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.find("-OBJS-$") != string::npos) {
                //Found unknown
                if (!passDUnknown()) {
                    m_ifInputFile.close();
                    return false;
                }
            } else if (m_sInLine.find("LIBS-$") != string::npos) {
                //Found Lib unknown
                if (!passDLibUnknown()) {
                    m_ifInputFile.close();
                    return false;
                }
            }
        }
        m_ifInputFile.close();
        return true;
    }
    cout << "  Error: could not open open MakeFile (" << sMakeFile << ")" << endl;
    return false;
}

bool ProjectGenerator::passProgramMake(const string & sProjectName)
{
    cout << "  Generating from Makefile (" << m_sProjectDir << ") for project " << sProjectName << "..." << endl;
    //Open the input Makefile
    string sMakeFile = m_sProjectDir + "/MakeFile";
    m_ifInputFile.open(sMakeFile);
    if (m_ifInputFile.is_open()) {
        string sObjTag = "OBJS-" + sProjectName;
        uint uiFindPos;
        //Read each line in the MakeFile
        while (getline(m_ifInputFile, m_sInLine)) {
            //Check what information is included in the current line
            if (m_sInLine.substr(0, sObjTag.length()).compare(sObjTag) == 0) {
                //Cut the line so it can be used by default passers
                m_sInLine = m_sInLine.substr(sObjTag.length() - 4);
                if (m_sInLine.at(4) == '-') {
                    //Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    //Found some static c includes
                    if (!passCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.substr(0, 6).compare("FFLIBS") == 0) {
                //Found some libs
                if (m_sInLine.at(6) == '-') {
                    //Found some dynamic libs
                    if (!passDLibInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    //Found some static libs
                    if (!passLibInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if ((uiFindPos = m_sInLine.find("eval OBJS-$(prog)")) != string::npos) {
                m_sInLine = m_sInLine.substr(uiFindPos + 13);
                if (m_sInLine.at(4) == '-') {
                    //Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    //Found some static c includes
                    if (!passCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            }
        }
        m_ifInputFile.close();

        //Program always include a file named after themselves
        m_vIncludes.push_back(sProjectName);
        return true;
    }
    cout << "  Error: could not open open MakeFile (./MakeFile)" << endl;
    return false;
}

bool ProjectGenerator::findSourceFile(const string & sFile, const string & sExtension, string & sRetFileName)
{
    string sFileName;
    sRetFileName = m_sProjectDir + sFile + sExtension;
    if (!findFile(sRetFileName, sFileName)) {
        // Check if this is a built file
        uint uiSPos = m_sProjectDir.rfind('/', m_sProjectDir.length() - 2);
        if (uiSPos == string::npos) {
            return false;
        }
        string sProjectName = m_sProjectDir.substr(uiSPos);
        sRetFileName = m_ConfigHelper.m_sProjectDirectory + sProjectName + sFile + sExtension;
        return findFile(sRetFileName, sFileName);
    }
    return true;
}

bool ProjectGenerator::findSourceFiles(const string & sFile, const string & sExtension, vector<string> & vRetFiles)
{
    string sFileName = m_sProjectDir + sFile + sExtension;
    return findFiles(sFileName, vRetFiles);
}

void ProjectGenerator::makeFileProjectRelative(const string & sFileName, string & sRetFileName)
{
    string sPath;
    string sFile = sFileName;
    uint uiPos = sFile.rfind('/');
    if (uiPos != string::npos) {
        ++uiPos;
        sPath = sFileName.substr(0, uiPos);
        sFile = sFileName.substr(uiPos);
    }
    makePathsRelative(sPath, m_ConfigHelper.m_sProjectDirectory, sRetFileName);
    //Check if relative to project dir
    if (sRetFileName.find("./") == 0) {
        sRetFileName = sRetFileName.substr(2);
    }
    sRetFileName += sFile;
}

void ProjectGenerator::makeFileGeneratorRelative(const string & sFileName, string & sRetFileName)
{
    string sPath;
    string sFile = sFileName;
    uint uiPos = sFile.rfind('/');
    if (uiPos != string::npos) {
        ++uiPos;
        sPath = sFileName.substr(0, uiPos);
        sFile = sFileName.substr(uiPos);
    }
    makePathsRelative(m_ConfigHelper.m_sProjectDirectory + sPath, "./", sRetFileName);
    //Check if relative to current dir
    if (sRetFileName.find("./") == 0) {
        sRetFileName = sRetFileName.substr(2);
    }
    sRetFileName += sFile;
}

bool ProjectGenerator::checkProjectFiles(const string& sProjectName)
{
    //Check that all headers are correct
    for (StaticList::iterator itIt = m_vHIncludes.begin(); itIt != m_vHIncludes.end(); itIt++) {
        string sRetFileName;
        if (!findSourceFile(*itIt, ".h", sRetFileName)) {
            cout << "  Error: could not find input header file for object (" << *itIt << ")" << endl;
            return false;
        }
        //Update the entry with the found file with complete path
        makeFileProjectRelative(sRetFileName, *itIt);
    }

    //Check that all C Source are correct
    for (StaticList::iterator itIt = m_vCIncludes.begin(); itIt != m_vCIncludes.end(); itIt++) {
        string sRetFileName;
        if (!findSourceFile(*itIt, ".c", sRetFileName)) {
            cout << "  Error: could not find input C source file for object (" << *itIt << ")" << endl;
            return false;
        }
        //Update the entry with the found file with complete path
        makeFileProjectRelative(sRetFileName, *itIt);
    }

    //Check that all CPP Source are correct
    for (StaticList::iterator itIt = m_vCPPIncludes.begin(); itIt != m_vCPPIncludes.end(); itIt++) {
        string sRetFileName;
        if (!findSourceFile(*itIt, ".cpp", sRetFileName)) {
            cout << "  Error: could not find input C++ source file for object (" << *itIt << ")" << endl;
            return false;
        }
        //Update the entry with the found file with complete path
        makeFileProjectRelative(sRetFileName, *itIt);
    }

    //Check that all ASM Source are correct
    for (StaticList::iterator itIt = m_vYASMIncludes.begin(); itIt != m_vYASMIncludes.end(); itIt++) {
        string sRetFileName;
        if (!findSourceFile(*itIt, ".asm", sRetFileName)) {
            cout << "  Error: could not find input ASM source file for object (" << *itIt << ")" << endl;
            return false;
        }
        //Update the entry with the found file with complete path
        makeFileProjectRelative(sRetFileName, *itIt);
    }

    //Check the output Unknown Includes and find there corresponding file
    if (!findProjectFiles(m_vIncludes, m_vCIncludes, m_vCPPIncludes, m_vYASMIncludes, m_vHIncludes)) {
        return false;
    }

    //Check all source files associated with replaced config values
    StaticList vReplaceIncludes, vReplaceCPPIncludes, vReplaceCIncludes, vReplaceYASMIncludes;
    for (UnknownList::iterator itIt = m_mReplaceIncludes.begin(); itIt != m_mReplaceIncludes.end(); itIt++) {
        vReplaceIncludes.push_back(itIt->first);
    }
    if (!findProjectFiles(vReplaceIncludes, vReplaceCIncludes, vReplaceCPPIncludes, vReplaceYASMIncludes, m_vHIncludes)) {
        return false;
    } else {
        //Need to create local files for any replace objects
        if (!createReplaceFiles(vReplaceCIncludes, m_vCIncludes, sProjectName)) {
            return false;
        }
        if (!createReplaceFiles(vReplaceCPPIncludes, m_vCPPIncludes, sProjectName)) {
            return false;
        }
        if (!createReplaceFiles(vReplaceYASMIncludes, m_vYASMIncludes, sProjectName)) {
            return false;
        }
    }

    return true;
}

bool ProjectGenerator::createReplaceFiles(const StaticList& vReplaceIncludes, StaticList& vExistingIncludes, const string& sProjectName)
{
    for (StaticList::const_iterator itIt = vReplaceIncludes.cbegin(); itIt != vReplaceIncludes.cend(); itIt++) {
        //Check hasnt already been included as a fixed object
        if (find(vExistingIncludes.begin(), vExistingIncludes.end(), *itIt) != vExistingIncludes.end()) {
            //skip this item
            continue;
        }
        //Convert file to format required to search ReplaceIncludes
        uint uiExtPos = itIt->rfind('.');
        uint uiCutPos = itIt->rfind('/') + 1;
        string sFilename = itIt->substr(uiCutPos, uiExtPos - uiCutPos);
        string sExtension = itIt->substr(uiExtPos);
        //Get the files dynamic config requirement
        string sIdents;
        for (StaticList::iterator itIdents = m_mReplaceIncludes[sFilename].begin(); itIdents < m_mReplaceIncludes[sFilename].end(); itIdents++) {
            sIdents += *itIdents;
            if ((itIdents + 1) < m_mReplaceIncludes[sFilename].end()) {
                sIdents += " || ";
            }
        }
        //Create new file to wrap input object
        string sPrettyFile = "../" + *itIt;
        string sNewFile = getCopywriteHeader(sFilename + sExtension + " file wrapper for " + sProjectName);
        sNewFile += "\n\
\n\
#include \"config.h\"\n\
#if " + sIdents + "\n\
#   include \"" + sPrettyFile + "\"\n\
#endif";
        //Write output project
        if (!makeDirectory(m_ConfigHelper.m_sProjectDirectory + sProjectName)) {
            cout << "  Error: Failed creating local " + sProjectName + " directory" << endl;
            return false;
        }
        string sOutFile = m_ConfigHelper.m_sProjectDirectory + sProjectName + "/" + sFilename + "_wrap" + sExtension;
        if (!writeToFile(sOutFile, sNewFile)) {
            return false;
        }
        //Add the new file to list of objects
        makeFileProjectRelative(sOutFile, sOutFile);
        vExistingIncludes.push_back(sOutFile);
    }
    return true;
}

bool ProjectGenerator::findProjectFiles(const StaticList& vIncludes, StaticList& vCIncludes, StaticList& vCPPIncludes, StaticList& vASMIncludes, StaticList& vHIncludes)
{
    for (StaticList::const_iterator itIt = vIncludes.cbegin(); itIt != vIncludes.cend(); itIt++) {
        string sRetFileName;
        if (findSourceFile(*itIt, ".c", sRetFileName)) {
            //Found a C File to include
            if (find(vCIncludes.begin(), vCIncludes.end(), sRetFileName) != vCIncludes.end()) {
                //skip this item
                continue;
            }
            makeFileProjectRelative(sRetFileName, sRetFileName);
            vCIncludes.push_back(sRetFileName);
        } else if (findSourceFile(*itIt, ".cpp", sRetFileName)) {
            //Found a C++ File to include
            if (find(vCPPIncludes.begin(), vCPPIncludes.end(), sRetFileName) != vCPPIncludes.end()) {
                //skip this item
                continue;
            }
            makeFileProjectRelative(sRetFileName, sRetFileName);
            vCPPIncludes.push_back(sRetFileName);
        } else if (findSourceFile(*itIt, ".asm", sRetFileName)) {
            //Found a ASM File to include
            if (find(vASMIncludes.begin(), vASMIncludes.end(), sRetFileName) != vASMIncludes.end()) {
                //skip this item
                continue;
            }
            makeFileProjectRelative(sRetFileName, sRetFileName);
            vASMIncludes.push_back(sRetFileName);
        } else if (findSourceFile(*itIt, ".h", sRetFileName)) {
            //Found a H File to include
            if (find(vHIncludes.begin(), vHIncludes.end(), sRetFileName) != vHIncludes.end()) {
                //skip this item
                continue;
            }
            makeFileProjectRelative(sRetFileName, sRetFileName);
            vHIncludes.push_back(sRetFileName);
        } else {
            cout << "  Error: Could not find valid source file for object (" << *itIt << ")" << endl;
            return false;
        }
    }
    return true;
}

void ProjectGenerator::outputTemplateTags(const string& sProjectName, string & sProjectTemplate, string& sFiltersTemplate)
{
    //Change all occurrences of template_in with project name
    const string sFFSearchTag = "template_in";
    uint uiFindPos = sProjectTemplate.find(sFFSearchTag);
    while (uiFindPos != string::npos) {
        //Replace
        sProjectTemplate.replace(uiFindPos, sFFSearchTag.length(), sProjectName);
        //Get next
        uiFindPos = sProjectTemplate.find(sFFSearchTag, uiFindPos + 1);
    }
    uint uiFindPosFilt = sFiltersTemplate.find(sFFSearchTag);
    while (uiFindPosFilt != string::npos) {
        //Replace
        sFiltersTemplate.replace(uiFindPosFilt, sFFSearchTag.length(), sProjectName);
        //Get next
        uiFindPosFilt = sFiltersTemplate.find(sFFSearchTag, uiFindPosFilt + 1);
    }

    //Change all occurrences of template_shin with short project name
    const string sFFShortSearchTag = "template_shin";
    uiFindPos = sProjectTemplate.find(sFFShortSearchTag);
    string sProjectNameShort = sProjectName.substr(3); //The full name minus the lib prefix
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
    string sToolchain = "<PlatformToolset Condition=\"'$(VisualStudioVersion)'=='12.0'\">v120</PlatformToolset>\n\
    <PlatformToolset Condition=\"'$(VisualStudioVersion)'=='14.0'\">v140</PlatformToolset>\n\
    <PlatformToolset Condition=\"'$(VisualStudioVersion)'=='15.0'\">v141</PlatformToolset>";
    if (m_ConfigHelper.m_sToolchain.compare("icl") == 0) {
        sToolchain += "\n    <PlatformToolset Condition=\"'$(ICPP_COMPILER13)'!=''\">Intel C++ Compiler XE 13.0</PlatformToolset>\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER14)'!=''\">Intel C++ Compiler XE 14.0</PlatformToolset>\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER15)'!=''\">Intel C++ Compiler XE 15.0</PlatformToolset>\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER16)'!=''\">Intel C++ Compiler 16.0</PlatformToolset>\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER17)'!=''\">Intel C++ Compiler 17.0</PlatformToolset>\n\
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
        sProjectTemplate.replace(uiFindPos, mKeys[sProjectName].length(), mKeys[sProjectName]);
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
    makeFileProjectRelative(sRootDir, sRootDir);
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
    const string sItemGroup = "\n  <ItemGroup>";
    const string sItemGroupEnd = "\n  </ItemGroup>";
    const string sIncludeClose = "\">";
    const string sIncludeEnd = "\" />";
    const string sTypeInclude = "\n    <" + sType + " Include=\"";
    const string sTypeIncludeEnd = "\n    </" + sType + ">";
    const string sIncludeObject = "\n      <ObjectFileName>$(IntDir)\\";
    const string sIncludeObjectClose = ".obj</ObjectFileName>";
    const string sFilterSource = "\n      <Filter>" + sFilterType + " Files";
    const string sSource = sFilterType + " Files";
    const string sFilterEnd = "</Filter>";
    const string sExcludeConfig = "\n      <ExcludedFromBuild Condition=\"'$(Configuration)'=='";
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
            makeFileProjectRelative(this->m_ConfigHelper.m_sRootDirectory, sSourceDir);
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
                uint32_t uiConfigs = 0;
                if (bStaticOnly) {
                    p_Configs = aBuildConfigsShared;
                    uiConfigs = 4;
                } else {
                    p_Configs = aBuildConfigsStatic;
                    uiConfigs = 3;
                }
                for (uint32_t uiI = 0; uiI < uiConfigs; uiI++) {
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
        uint uiFindPos = sProjectTemplate.rfind(sItemGroupEnd);
        uiFindPos += sItemGroupEnd.length();
        uint uiFindPosFilt = sFilterTemplate.rfind(sItemGroupEnd);
        uiFindPosFilt += sItemGroupEnd.length();

        //Insert into output file
        sProjectTemplate.insert(uiFindPos, sTypeFiles);
        uiFindPos += sTypeFiles.length();
        sFilterTemplate.insert(uiFindPosFilt, sTypeFilesFilt);
        uiFindPosFilt += sTypeFilesFilt.length();
    }
}

void ProjectGenerator::outputSourceFiles(const string & sProjectName, string & sProjectTemplate, string & sFilterTemplate)
{
    set<string> vFoundFilters;
    StaticList vFoundObjects;

    //Check if there is a resource file
    string sResourceFile;
    if (findSourceFile(sProjectName.substr(3) + "res", ".rc", sResourceFile)) {
        makeFileProjectRelative(sResourceFile, sResourceFile);
        StaticList vResources;
        vResources.push_back(sResourceFile);
        outputSourceFileType(vResources, "ResourceCompile", "Resource", sProjectTemplate, sFilterTemplate, vFoundObjects, vFoundFilters, false, false, true);
    }

    //Output ASM files in specific item group (must go first as asm does not allow for custom obj filename)
    if (m_ConfigHelper.getConfigOptionPrefixed("HAVE_YASM")->m_sValue.compare("1") == 0) {
        outputSourceFileType(m_vYASMIncludes, "YASM", "Source", sProjectTemplate, sFilterTemplate, vFoundObjects, vFoundFilters, false);
    }

    //Output C files
    outputSourceFileType(m_vCIncludes, "ClCompile", "Source", sProjectTemplate, sFilterTemplate, vFoundObjects, vFoundFilters, true);

    //Output C++ files
    outputSourceFileType(m_vCPPIncludes, "ClCompile", "Source", sProjectTemplate, sFilterTemplate, vFoundObjects, vFoundFilters, true);

    //Output header files in new item group
    outputSourceFileType(m_vHIncludes, "ClInclude", "Header", sProjectTemplate, sFilterTemplate, vFoundObjects, vFoundFilters, false);

    //Add any additional Filters to filters file
    const string sItemGroupEnd = "\n  </ItemGroup>";
    const string sFilterAdd = "\n    <Filter Include=\"";
    const string sFilterAdd2 = "\">\n\
      <UniqueIdentifier>{";
    const string sFilterAddClose = "}</UniqueIdentifier>\n\
    </Filter>";
    const string asFilterKeys[] = {"cac6df1e-4a60-495c-8daa-5707dc1216ff", "9fee14b2-1b77-463a-bd6b-60efdcf8850f",
        "bf017c32-250d-47da-b7e6-d5a5091cb1e6", "fd9e10e9-18f6-437d-b5d7-17290540c8b8", "f026e68e-ff14-4bf4-8758-6384ac7bcfaf",
        "a2d068fe-f5d5-4b6f-95d4-f15631533341", "8a4a673d-2aba-4d8d-a18e-dab035e5c446", "0dcfb38d-54ca-4ceb-b383-4662f006eca9",
        "57bf1423-fb68-441f-b5c1-f41e6ae5fa9c"};

    //get start position in file
    uint uiFindPosFilt = sFilterTemplate.find(sItemGroupEnd);
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

bool ProjectGenerator::outputProjectExports(const string& sProjectName, const StaticList& vIncludeDirs)
{
    cout << "  Generating project exports file (" << sProjectName << ")..." << endl;
    string sExportList;
    if (!findFile(this->m_sProjectDir + "/*.v", sExportList)) {
        cout << "  Error: Failed finding project exports (" << sProjectName << ")" << endl;
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
        cout << "  Error: Failed finding global start in project exports (" << sExportList << ")" << endl;
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

    if (!runMSVC(vIncludeDirs, sProjectName, mDirectoryObjects, 0))
        return false;

    //Loaded in the compiler passed files
    StaticList vSBRFiles;
    StaticList vModuleExports;
    StaticList vModuleDataExports;
    findFiles(sProjectName + "/*.sbr", vSBRFiles);
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
    deleteFolder(sProjectName);

    //Check for any exported functions in asm files
    for (StaticList::iterator itASM = m_vYASMIncludes.begin(); itASM < m_vYASMIncludes.end(); itASM++) {
        string sASMFile;
        loadFromFile(m_ConfigHelper.m_sProjectDirectory + *itASM, sASMFile);

        //Search through file for module exports
        for (StaticList::iterator itI = vExportStrings.begin(); itI < vExportStrings.end(); itI++) {
            //Check if it is a wild card search
            uiFindPos = itI->find('*');
            const string sInvalidChars = ",.(){}[]`'\"+-*/!@#$%^&*<>|;\\= \n\t\0";
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
    string sModuleFile = "EXPORTS\n";
    for (StaticList::iterator itI = vModuleExports.begin(); itI < vModuleExports.end(); itI++) {
        sModuleFile += "    " + *itI + '\n';
    }
    for (StaticList::iterator itI = vModuleDataExports.begin(); itI < vModuleDataExports.end(); itI++) {
        sModuleFile += "    " + *itI + " DATA\n";
    }

    string sDestinationFile = m_ConfigHelper.m_sProjectDirectory + sProjectName + ".def";
    if (!writeToFile(sDestinationFile, sModuleFile)) {
        return false;
    }
    return true;
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
    string sCLLaunchBat = "@echo off \n";
    sCLLaunchBat += "if exist \"%VS150COMNTOOLS%\\vsvars32.bat\" ( \n\
call \"%VS150COMNTOOLS%\\vsvars32.bat\" \n\
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

void ProjectGenerator::outputBuildEvents(const string& sProjectName, string & sProjectTemplate)
{
    //After </Lib> and </Link> and the post and then pre build events
    const string asLibLink[2] = {"</Lib>", "</Link>"};
    const string sPostbuild = "\n    <PostBuildEvent>\n\
      <Command>";
    const string sPostbuildClose = "</Command>\n\
    </PostBuildEvent>";
    const string sInclude = "mkdir $(OutDir)\\include\n\
mkdir $(OutDir)\\include\\";
    const string sCopy = "\ncopy ";
    const string sCopyEnd = " $(OutDir)\\include\\";
    const string sLicense = "\nmkdir $(OutDir)\\licenses";
    string sLicenseName = m_ConfigHelper.m_sProjectName;
    transform(sLicenseName.begin(), sLicenseName.end(), sLicenseName.begin(), ::tolower);
    const string sLicenseEnd = " $(OutDir)\\licenses\\" + sLicenseName + ".txt";
    const string sPrebuild = "\n    <PreBuildEvent>\n\
      <Command>if exist ..\\config.h (\n\
del ..\\config.h\n\
)\n\
if exist ..\\version.h (\n\
del ..\\version.h\n\
)\n\
if exist ..\\config.asm (\n\
del ..\\config.asm\n\
)\n\
if exist ..\\libavutil\\avconfig.h (\n\
del ..\\libavutil\\avconfig.h\n\
)\n\
if exist ..\\libavutil\\ffversion.h (\n\
del ..\\libavutil\\ffversion.h\n\
)";
    const string sPrebuildDir = "\nif exist $(OutDir)\\include\\" + sProjectName + " (\n\
rd /s /q $(OutDir)\\include\\" + sProjectName + "\n\
cd ../\n\
cd $(ProjectDir)\n\
)";
    const string sPrebuildClose = "</Command>\n    </PreBuildEvent>";
    //Get the correct license file
    string sLicenseFile;
    if (m_ConfigHelper.getConfigOption("nonfree")->m_sValue.compare("1") == 0) {
        sLicenseFile = "..\\COPYING.GPLv3"; //Technically this has no license as it is unredistributable but we get the closest thing for now
    } else if (m_ConfigHelper.getConfigOption("gplv3")->m_sValue.compare("1") == 0) {
        sLicenseFile = "..\\COPYING.GPLv3";
    } else if (m_ConfigHelper.getConfigOption("lgplv3")->m_sValue.compare("1") == 0) {
        sLicenseFile = "..\\COPYING.LGPLv3";
    } else if (m_ConfigHelper.getConfigOption("gpl")->m_sValue.compare("1") == 0) {
        sLicenseFile = "..\\COPYING.GPLv2";
    } else {
        sLicenseFile = "..\\COPYING.LGPLv2.1";
    }
    //Generate the pre build and post build string
    string sAdditional;
    //Add the post build event
    sAdditional += sPostbuild;
    if (m_vHIncludes.size() > 0) {
        sAdditional += sInclude;
        sAdditional += sProjectName;
        for (StaticList::iterator vitHeaders = m_vHIncludes.begin(); vitHeaders < m_vHIncludes.end(); vitHeaders++) {
            sAdditional += sCopy;
            string sHeader = *vitHeaders;
            replace(sHeader.begin(), sHeader.end(), '/', '\\');
            sAdditional += sHeader;
            sAdditional += sCopyEnd;
            sAdditional += sProjectName;
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
    if (vIncludeDirs.size() > 0) {
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
    if ((vLib32Dirs.size() > 0) || (vLib64Dirs.size() > 0)) {
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

void ProjectGenerator::outputYASMTools(string & sProjectTemplate)
{
    const string sYASMDefines = "\n\
    <YASM>\n\
      <IncludePaths>.\\;template_rootdir;%(IncludePaths)</IncludePaths>\n\
      <PreIncludeFile>config.asm</PreIncludeFile>\n\
      <Debug>true</Debug>\n\
    </YASM>";
    const string sYASMProps = "\n\
  <ImportGroup Label=\"ExtensionSettings\">\n\
    <Import Project=\"$(VCTargetsPath)\\BuildCustomizations\\vsyasm.props\" />\n\
  </ImportGroup>";
    const string sYASMTargets = "\n\
  <ImportGroup Label=\"ExtensionTargets\">\n\
    <Import Project=\"$(VCTargetsPath)\\BuildCustomizations\\vsyasm.targets\" />\n\
  </ImportGroup>";
    const string sFindProps = "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />";
    const string sFindTargets = "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />";

    if ((m_ConfigHelper.getConfigOptionPrefixed("HAVE_YASM")->m_sValue.compare("1") == 0) && (m_vYASMIncludes.size() > 0)) {
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

        //Add YASM build customization
        uiFindPos = sProjectTemplate.find(sFindProps) + sFindProps.length();
        //After <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" /> add yasm props
        sProjectTemplate.insert(uiFindPos, sYASMProps);
        uiFindPos += sYASMProps.length();
        uiFindPos = sProjectTemplate.find(sFindTargets) + sFindTargets.length();
        //After <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" /> add yasm target
        sProjectTemplate.insert(uiFindPos, sYASMTargets);
        uiFindPos += sYASMTargets.length();
    }
}

bool ProjectGenerator::outputDependencyLibs(const string & sProjectName, string & sProjectTemplate, bool bProgram)
{
    //Check current libs list for valid lib names
    for (StaticList::iterator vitLib = m_vLibs.begin(); vitLib < m_vLibs.end(); vitLib++) {
        //prepend lib if needed
        if (vitLib->find("lib") != 0) {
            *vitLib = "lib" + *vitLib;
        }
    }

    //Add additional dependencies based on current config to Libs list
    buildInterDependencies(sProjectName, m_vLibs);
    m_mProjectLibs[sProjectName] = m_vLibs; //Backup up current libs for solution
    StaticList vAddLibs;
    buildDependencies(sProjectName, m_vLibs, vAddLibs);

    if ((m_vLibs.size() > 0) || (vAddLibs.size() > 0)) {
        //Create list of additional ffmpeg dependencies
        string sAddFFmpegLibs[4]; //debug, release, debugDll, releaseDll
        for (StaticList::iterator vitLib = m_mProjectLibs[sProjectName].begin(); vitLib < m_mProjectLibs[sProjectName].end(); vitLib++) {
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
        for (StaticList::iterator vitLib = m_vLibs.begin() + m_mProjectLibs[sProjectName].size(); vitLib < m_vLibs.end(); vitLib++) {
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
                        cout << "  Error: Failed finding dependencies in template." << endl;
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

bool ProjectGenerator::outputProjectDCE(string sProjectName, const StaticList& vIncludeDirs)
{
    cout << "  Generating missing DCE symbols (" << sProjectName << ")..." << endl;
    //Create list of source files to scan
    StaticList vSearchFiles = m_vCIncludes;
#if !FORCEALLDCE
    vSearchFiles.insert(vSearchFiles.end(), m_vCPPIncludes.begin(), m_vCPPIncludes.end());
    vSearchFiles.insert(vSearchFiles.end(), m_vHIncludes.begin(), m_vHIncludes.end());
#else
    findFiles(m_sProjectDir + "*.h", vSearchFiles, false);
    findFiles(m_sProjectDir + "*.c", vSearchFiles, false);
    findFiles(m_sProjectDir + "*.cpp", vSearchFiles, false);
#endif
    //Ensure we can add extra items to the list without needing reallocs
    if (vSearchFiles.capacity() < vSearchFiles.size() + 250) {
        vSearchFiles.reserve(vSearchFiles.size() + 250);
    }

    //Check for DCE constructs
    map<string, DCEParams> mFoundDCEFunctions;
    StaticList vPreProcFiles;
    //Search through each included file
    for (StaticList::iterator itFile = vSearchFiles.begin(); itFile < vSearchFiles.end(); itFile++) {
        //Open the input file
        makeFileGeneratorRelative(*itFile, *itFile);
        string sFile;
        if (!loadFromFile(*itFile, sFile)) {
            return false;
        }
        bool bRequiresPreProcess = false;
        outputProjectDCEFindFunctions(sFile, sProjectName, *itFile, mFoundDCEFunctions, bRequiresPreProcess);
        if (bRequiresPreProcess) {
            vPreProcFiles.push_back(*itFile);
        }

        //Check if this file includes additional source files
        uint uiFindPos = sFile.find(".c\"");
        while (uiFindPos != string::npos) {
            //Check if this is an include
            uint uiFindPos2 = sFile.rfind("#include \"", uiFindPos);
            if ((uiFindPos2 != string::npos) && (uiFindPos - uiFindPos2 < 50)) {
                //Get the name of the file
                uiFindPos2 += 10;
                uiFindPos += 2;
                string sTemplateFile = sFile.substr(uiFindPos2, uiFindPos - uiFindPos2);
                //check if file contains current project
                uint uiProjName = sTemplateFile.find(sProjectName);
                if (uiProjName != string::npos) {
                    sTemplateFile = sTemplateFile.substr(uiProjName + sProjectName.length() + 1);
                }
                //Check if in sibling directory
                if (sTemplateFile.find('/') != string::npos) {
                    sTemplateFile = "../" + sTemplateFile;
                }
                string sFound;
                string sBack = sTemplateFile;
                sTemplateFile = m_sProjectDir + sBack;
                if (!findFile(sTemplateFile, sFound)) {
                    sTemplateFile = m_ConfigHelper.m_sProjectDirectory + sProjectName + '/' + sBack;
                    if (!findFile(sTemplateFile, sFound)) {
                        sTemplateFile = itFile->substr(0, itFile->rfind('/') + 1) + sBack;
                        if (!findFile(sTemplateFile, sFound)) {
                            cout << "  Error: Failed to find included file " << sBack << "  " << endl;
                            return false;
                        }
                    }
                }
                //Add the file to the list
                if (find(vSearchFiles.begin(), vSearchFiles.end(), sTemplateFile) == vSearchFiles.end()) {
                    makeFileProjectRelative(sTemplateFile, sTemplateFile);
                    vSearchFiles.push_back(sTemplateFile);
                }
            }
            //Check for more
            uiFindPos = sFile.find(".c\"", uiFindPos + 1);
        }
    }

    //Get a list of all files in current project directory (including subdirectories)
    vSearchFiles.resize(0);
    findFiles(m_sProjectDir + "*.h", vSearchFiles);
    findFiles(m_sProjectDir + "*.c", vSearchFiles);
    findFiles(m_sProjectDir + "*.cpp", vSearchFiles);
    //Ensure we can add extra items to the list without needing reallocs
    if (vSearchFiles.capacity() < vSearchFiles.size() + 250) {
        vSearchFiles.reserve(vSearchFiles.size() + 250);
    }

#if !FORCEALLDCE
    //Check all configurations are enabled early to avoid later lookups of unused functions
    ConfigGenerator::DefaultValuesList mReserved;
    ConfigGenerator::DefaultValuesList mIgnored;
    m_ConfigHelper.buildReplaceValues(mReserved, mIgnored);
    for (map<string, DCEParams>::iterator itDCE = mFoundDCEFunctions.begin(); itDCE != mFoundDCEFunctions.end(); ) {
        outputProgramDCEsResolveDefine(itDCE->second.sDefine, mReserved);
        if (itDCE->second.sDefine.compare("1") == 0) {
            //remove from the list
            mFoundDCEFunctions.erase(itDCE++);
        } else {
            ++itDCE;
        }
    }
#endif

    //Now we need to find the declaration of each function
    map<string, DCEParams> mFoundDCEDefinitions;
    map<string, DCEParams> mFoundDCEVariables;
    if (mFoundDCEFunctions.size() > 0) {
        //Search through each included file
        for (StaticList::iterator itFile = vSearchFiles.begin(); itFile < vSearchFiles.end(); itFile++) {
            string sFile;
            if (!loadFromFile(*itFile, sFile)) {
                return false;
            }
            for (map<string, DCEParams>::iterator itDCE = mFoundDCEFunctions.begin(); itDCE != mFoundDCEFunctions.end(); ) {
                string sReturn;
                bool bIsFunc;
                if (outputProjectDCEsFindDeclarations(sFile, itDCE->first, *itFile, sReturn, bIsFunc)) {
                    //Get the declaration file
                    string sFileName;
                    makePathsRelative(*itFile, m_ConfigHelper.m_sRootDirectory, sFileName);
                    if (sFileName.at(0) == '.') {
                        sFileName = sFileName.substr(2);
                    }
                    if (bIsFunc) {
                        mFoundDCEDefinitions[sReturn] = {itDCE->second.sDefine, sFileName};
                    } else {
                        mFoundDCEVariables[sReturn] = {itDCE->second.sDefine, sFileName};
                    }

                    //Remove it from the list
                    mFoundDCEFunctions.erase(itDCE++);
                } else {
                    //Only increment the iterator when nothing has been found
                    // when we did find something we erased a value from the list so the iterator is still valid
                    ++itDCE;
                }
            }
        }
    }

    //Add any files requiring pre-processing to unfound list
    for (StaticList::iterator itPP = vPreProcFiles.begin(); itPP < vPreProcFiles.end(); itPP++) {
        mFoundDCEFunctions[*itPP] = {"#", *itPP};
    }

    //Check if we failed to find any functions
    if (mFoundDCEFunctions.size() > 0) {
        vector<string> vIncludeDirs2 = vIncludeDirs;
        makeDirectory(sProjectName);
        //Get all the files that include functions
        map<string, vector<DCEParams>> mFunctionFiles;
        for (map<string, DCEParams>::iterator itDCE = mFoundDCEFunctions.begin(); itDCE != mFoundDCEFunctions.end(); itDCE++) {
            //Remove project dir from start of file
            uint uiPos = itDCE->second.sFile.find(m_sProjectDir);
            uiPos = (uiPos == string::npos) ? 0 : uiPos + m_sProjectDir.length();
            string sFile = sProjectName + '/' + itDCE->second.sFile.substr(uiPos);
            if (sFile.find('/', sProjectName.length() + 1) != string::npos) {
                makeDirectory(sFile.substr(0, sFile.rfind('/')));
            }
            //Copy file to local working directory
            if (!copyFile(itDCE->second.sFile, sFile)) {
                return false;
            }
            mFunctionFiles[sFile].push_back({itDCE->second.sDefine, itDCE->first});
        }
        map<string, vector<string>> mDirectoryObjects;
        const string asDCETags2[] = {"if (", "if(", "& ", "&", "| ", "|"};
        for (map<string, vector<DCEParams>>::iterator itDCE = mFunctionFiles.begin(); itDCE != mFunctionFiles.end(); itDCE++) {
            //Get subdirectory
            string sSubFolder = "";
            if (itDCE->first.find('/', sProjectName.length() + 1) != string::npos) {
                sSubFolder = m_sProjectDir + itDCE->first.substr(sProjectName.length() + 1, itDCE->first.rfind('/') - sProjectName.length() - 1);
                if (find(vIncludeDirs2.begin(), vIncludeDirs2.end(), sSubFolder) == vIncludeDirs2.end()) {
                    //Need to add subdirectory to inlude list
                    vIncludeDirs2.push_back(sSubFolder);
                }
                sSubFolder = sSubFolder.substr(m_sProjectDir.length());
            }
            mDirectoryObjects[sSubFolder].push_back(itDCE->first);
            //Modify existing tags so that they are still valid after preprocessing
            string sFile;
            if (!loadFromFile(itDCE->first, sFile)) {
                return false;
            }
            for (unsigned uiTag = 0; uiTag < sizeof(asDCETags) / sizeof(string); uiTag++) {
                for (unsigned uiTag2 = 0; uiTag2 < sizeof(asDCETags2) / sizeof(string); uiTag2++) {
                    const string sSearch = asDCETags2[uiTag2] + asDCETags[uiTag];
                    //Search for all occurrences
                    uint uiFindPos = 0;
                    string sReplace = asDCETags2[uiTag2] + "XXX" + asDCETags[uiTag];
                    while ((uiFindPos = sFile.find(sSearch, uiFindPos)) != string::npos) {
                        sFile.replace(uiFindPos, sSearch.length(), sReplace);
                        uiFindPos += sReplace.length() + 1;
                    }
                }
            }
            if (!writeToFile(itDCE->first, sFile)) {
                return false;
            }
        }
        //Add current directory to include list (must be done last to ensure correct include order)
        vIncludeDirs2.push_back(m_sProjectDir);

        if (runMSVC(vIncludeDirs2, sProjectName, mDirectoryObjects, 1)) {
            //Check the file that the function usage was found in to see if it was declared using macro expansion
            for (map<string, vector<DCEParams>>::iterator itDCE = mFunctionFiles.begin(); itDCE != mFunctionFiles.end(); itDCE++) {
                string sFile = itDCE->first;
                sFile.at(sFile.length() - 1) = 'i';
                if (!loadFromFile(sFile, sFile)) {
                    return false;
                }

                //Restore the initial macro names
                for (unsigned uiTag = 0; uiTag < sizeof(asDCETags) / sizeof(string); uiTag++) {
                    for (unsigned uiTag2 = 0; uiTag2 < sizeof(asDCETags2) / sizeof(string); uiTag2++) {
                        const string sSearch = asDCETags2[uiTag2] + asDCETags[uiTag];
                        //Search for all occurrences
                        uint uiFindPos = 0;
                        string sFind = asDCETags2[uiTag2] + "XXX" + asDCETags[uiTag];
                        while ((uiFindPos = sFile.find(sFind, uiFindPos)) != string::npos) {
                            sFile.replace(uiFindPos, sFind.length(), sSearch);
                            uiFindPos += sSearch.length() + 1;
                        }
                    }
                }

                //Check for any un-found function usage
                map<string, DCEParams> mNewDCEFunctions;
                bool bCanIgnore = false;
                outputProjectDCEFindFunctions(sFile, sProjectName, itDCE->first, mNewDCEFunctions, bCanIgnore);
#if !FORCEALLDCE
                for (map<string, DCEParams>::iterator itDCE = mNewDCEFunctions.begin(); itDCE != mNewDCEFunctions.end(); ) {
                    outputProgramDCEsResolveDefine(itDCE->second.sDefine, mReserved);
                    if (itDCE->second.sDefine.compare("1") == 0) {
                        //remove from the list
                        mNewDCEFunctions.erase(itDCE++);
                    } else {
                        ++itDCE;
                    }
                }
#endif
                for (map<string, DCEParams>::iterator itDCE2 = mNewDCEFunctions.begin(); itDCE2 != mNewDCEFunctions.end(); itDCE2++) {
                    //Add the file to the list
                    if (find(itDCE->second.begin(), itDCE->second.end(), itDCE2->first) == itDCE->second.end()) {
                        itDCE->second.push_back({itDCE2->second.sDefine, itDCE2->first});
                        mFoundDCEFunctions[itDCE2->first] = itDCE2->second;
                    }
                }

                //Search through each function in the current file
                for (vector<DCEParams>::iterator itDCE2 = itDCE->second.begin(); itDCE2 < itDCE->second.end(); itDCE2++) {
                    if (itDCE2->sDefine.compare("#") != 0) {
                        string sReturn;
                        bool bIsFunc;
                        if (outputProjectDCEsFindDeclarations(sFile, itDCE2->sFile, itDCE->first, sReturn, bIsFunc)) {
                            //Get the declaration file
                            string sFileName;
                            makePathsRelative(itDCE->first, m_ConfigHelper.m_sRootDirectory, sFileName);
                            if (sFileName.at(0) == '.') {
                                sFileName = sFileName.substr(2);
                            }
                            //Add the declaration (ensure not to stomp a function found before needing pre-processing)
                            if (bIsFunc) {
                                if (mFoundDCEDefinitions.find(sReturn) == mFoundDCEDefinitions.end()) {
                                    mFoundDCEDefinitions[sReturn] = {itDCE2->sDefine, sFileName};
                                }
                            } else {
                                if (mFoundDCEVariables.find(sReturn) == mFoundDCEVariables.end()) {
                                    mFoundDCEVariables[sReturn] = {itDCE2->sDefine, sFileName};
                                }
                            }
                            //Remove the function from list
                            mFoundDCEFunctions.erase(itDCE2->sFile);
                        }
                    } else {
                        //Remove the function from list as it was just a preproc file
                        mFoundDCEFunctions.erase(itDCE2->sFile);
                    }
                }
            }

            //Delete the created temp files
            deleteFolder(sProjectName);
        }
    }

    //Get any required hard coded values
    buildProjectDCEs(sProjectName, mFoundDCEDefinitions, mFoundDCEVariables);

    //Check if we failed to find anything (even after using buildDCEs)
    if (mFoundDCEFunctions.size() > 0) {
        for (map<string, DCEParams>::iterator itDCE = mFoundDCEFunctions.begin(); itDCE != mFoundDCEFunctions.end(); itDCE++) {
            cout << "  Error: Failed to find function definition for " << itDCE->first << ", " << itDCE->second.sFile << endl;
        }
        return false;
    }

    //Add definition to new file
    if ((mFoundDCEDefinitions.size() > 0) || (mFoundDCEVariables.size() > 0)) {
        StaticList vIncludedHeaders;
        string sDCEOutFile;
        //Loop through all functions
        for (map<string, DCEParams>::iterator itDCE = mFoundDCEDefinitions.begin(); itDCE != mFoundDCEDefinitions.end(); itDCE++) {
            bool bUsePreProc = (itDCE->second.sDefine.length() > 1);
            //Only include preprocessor guards if its a reserved option
            if (bUsePreProc) {
                sDCEOutFile += "#if !(" + itDCE->second.sDefine + ")\n";
            }
            if (itDCE->second.sFile.find(".h") != string::npos) {
                //Include header files only once
                if (find(vIncludedHeaders.begin(), vIncludedHeaders.end(), itDCE->second.sFile) == vIncludedHeaders.end()) {
                    vIncludedHeaders.push_back(itDCE->second.sFile);
                }
            }
            sDCEOutFile += itDCE->first + " {";
            //Need to check return type
            string sReturn = itDCE->first.substr(0, itDCE->first.find_first_of(sWhiteSpace));
            if (sReturn.compare("void") == 0) {
                sDCEOutFile += "return;";
            } else if (sReturn.compare("int") == 0) {
                sDCEOutFile += "return 0;";
            } else if (sReturn.compare("unsigned") == 0) {
                sDCEOutFile += "return 0;";
            } else if (sReturn.compare("long") == 0) {
                sDCEOutFile += "return 0;";
            } else if (sReturn.compare("short") == 0) {
                sDCEOutFile += "return 0;";
            } else if (sReturn.compare("float") == 0) {
                sDCEOutFile += "return 0.0f;";
            } else if (sReturn.compare("double") == 0) {
                sDCEOutFile += "return 0.0;";
            } else if (sReturn.find('*') != string::npos) {
                sDCEOutFile += "return 0;";
            } else {
                sDCEOutFile += "return *(" + sReturn + "*)(0);";
            }
            sDCEOutFile += "}\n";
            if (bUsePreProc) {
                sDCEOutFile += "#endif\n";
            }
        }

        //Loop through all variables
        for (map<string, DCEParams>::iterator itDCE = mFoundDCEVariables.begin(); itDCE != mFoundDCEVariables.end(); itDCE++) {
            bool bReserved = true; bool bEnabled = false;
#if !FORCEALLDCE
            ConfigGenerator::ValuesList::iterator ConfigOpt = m_ConfigHelper.getConfigOptionPrefixed(itDCE->second.sDefine);
            if (ConfigOpt == m_ConfigHelper.m_vConfigValues.end()) {
                //This config option doesn't exist so it potentially requires the header file to be included first
            } else {
                bReserved = (mReserved.find(ConfigOpt->m_sPrefix + ConfigOpt->m_sOption) != mReserved.end());
                if (!bReserved) {
                    bEnabled = (ConfigOpt->m_sValue.compare("1") == 0);
                }
            }
#endif
            //Include only those options that are currently disabled
            if (!bEnabled) {
                //Only include preprocessor guards if its a reserved option
                if (bReserved) {
                    sDCEOutFile += "#if !(" + itDCE->second.sDefine + ")\n";
                }
                //Include header files only once
                if (find(vIncludedHeaders.begin(), vIncludedHeaders.end(), itDCE->second.sFile) == vIncludedHeaders.end()) {
                    vIncludedHeaders.push_back(itDCE->second.sFile);
                }
                sDCEOutFile += itDCE->first + " = {0};\n";
                if (bReserved) {
                    sDCEOutFile += "#endif\n";
                }
            }
        }

        string sFinalDCEOutFile = getCopywriteHeader(sProjectName + " DCE definitions") + '\n';
        sFinalDCEOutFile += "\n#include \"config.h\"\n\n";
        //Add all header files (goes backwards to avoid bug in header include order in avcodec between vp9.h and h264pred.h)
        for (StaticList::iterator itHeaders = vIncludedHeaders.end(); itHeaders > vIncludedHeaders.begin(); ) {
            --itHeaders;
            sFinalDCEOutFile += "#include \"" + *itHeaders + "\"\n";
        }
        sFinalDCEOutFile += '\n' + sDCEOutFile;

        //Output the new file
        string sOutName = m_ConfigHelper.m_sProjectDirectory + '/' + sProjectName + '/' + "dce_defs.c";
        writeToFile(sOutName, sFinalDCEOutFile);
        makeFileProjectRelative(sOutName, sOutName);
        m_vCIncludes.push_back(sOutName);
    }

    return true;
}

void ProjectGenerator::outputProjectDCEFindFunctions(const string & sFile, const string & sProjectName, const string & sFileName, map<string, DCEParams> & mFoundDCEFunctions, bool & bRequiresPreProcess)
{
    const string asDCETags2[] = {"if (", "if(", "if ((", "if(("};
    const string asFuncIdents[] = {"ff_", "swri_", "avresample_"};
    for (unsigned uiTag = 0; uiTag < sizeof(asDCETags) / sizeof(string); uiTag++) {
        for (unsigned uiTag2 = 0; uiTag2 < sizeof(asDCETags2) / sizeof(string); uiTag2++) {
            const string sSearch = asDCETags2[uiTag2] + asDCETags[uiTag];

            //Search for all occurrences
            uint uiFindPos = sFile.find(sSearch);
            while (uiFindPos != string::npos) {
                //Get the define tag
                uint uiFindPos2 = sFile.find(')', uiFindPos + sSearch.length());
                uiFindPos = uiFindPos + asDCETags2[uiTag2].length();
                if (uiTag2 >= 2) {
                    --uiFindPos;
                }
                //Skip any '(' found within the parameters itself
                uint uiFindPos3 = sFile.find('(', uiFindPos);
                while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiFindPos2)) {
                    uiFindPos3 = sFile.find('(', uiFindPos3 + 1);
                    uiFindPos2 = sFile.find(')', uiFindPos2 + 1);
                }
                string sDefine = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos);

                //Check if define contains pre-processor tags
                if (sDefine.find("##") != string::npos) {
                    bRequiresPreProcess = true;
                    return;
                }
                outputProjectDCECleanDefine(sDefine);

                //Get the block of code being wrapped
                string sCode;
                uiFindPos = sFile.find_first_not_of(sWhiteSpace, uiFindPos2 + 1);
                if (sFile.at(uiFindPos) == '{') {
                    //Need to get the entire block of code being wrapped
                    uiFindPos2 = sFile.find('}', uiFindPos + 1);
                    //Skip any '{' found within the parameters itself
                    uint uiFindPos5 = sFile.find('{', uiFindPos + 1);
                    while ((uiFindPos5 != string::npos) && (uiFindPos5 < uiFindPos2)) {
                        uiFindPos5 = sFile.find('{', uiFindPos5 + 1);
                        uiFindPos2 = sFile.find('}', uiFindPos2 + 1);
                    }
                } else {
                    //This is a single line of code
                    uiFindPos2 = sFile.find_first_of(sEndLine, uiFindPos + 1);
                }
                sCode = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos);

                //Get name of any functions
                for (unsigned uiIdents = 0; uiIdents < sizeof(asFuncIdents) / sizeof(string); uiIdents++) {
                    uiFindPos = sCode.find(asFuncIdents[uiIdents]);
                    while (uiFindPos != string::npos) {
                        bool bValid = false;
                        //Check if this is a valid function call
                        uint uiFindPos3 = sCode.find_first_of("(;", uiFindPos + 1);
                        if ((uiFindPos3 != 0) && (uiFindPos3 != string::npos)) {
                            uint uiFindPos4 = sCode.find_last_of(sNonName, uiFindPos3 - 1);
                            uiFindPos4 = (uiFindPos4 == string::npos) ? 0 : uiFindPos4 + 1;
                            //Check if valid function
                            if (uiFindPos4 == uiFindPos) {
                                if (sCode.at(uiFindPos3) == '(') {
                                    bValid = true;
                                } else {
                                    uiFindPos4 = sCode.find_last_not_of(sWhiteSpace, uiFindPos4 - 1);
                                    if (sCode.at(uiFindPos4) == '=') {
                                        bValid = true;
                                    }
                                }
                            } else {
                                //Check if this is a macro expansion
                                uiFindPos4 = sCode.find('#', uiFindPos + 1);
                                if (uiFindPos4 < uiFindPos3) {
                                    //We need to add this to the list of files that require preprocessing
                                    bRequiresPreProcess = true;
                                    return;
                                }
                            }
                        }
                        if (bValid) {
                            string sAdd = sCode.substr(uiFindPos, uiFindPos3 - uiFindPos);
                            //Check if there are any other DCE conditions
                            string sFuncDefine = sDefine;
                            for (unsigned uiTagB = 0; uiTagB < sizeof(asDCETags) / sizeof(string); uiTagB++) {
                                for (unsigned uiTag2B = 0; uiTag2B < sizeof(asDCETags2) / sizeof(string); uiTag2B++) {
                                    const string sSearch2 = asDCETags2[uiTag2B] + asDCETags[uiTagB];

                                    //Search for all occurrences
                                    uint uiFindPos7 = sCode.rfind(sSearch2, uiFindPos);
                                    while (uiFindPos7 != string::npos) {
                                        //Get the define tag
                                        uint uiFindPos4 = sCode.find(')', uiFindPos7 + sSearch.length());
                                        uint uiFindPos8 = uiFindPos7 + asDCETags2[uiTag2B].length();
                                        if (uiTag2B >= 2) {
                                            --uiFindPos8;
                                        }
                                        //Skip any '(' found within the parameters itself
                                        uint uiFindPos9 = sCode.find('(', uiFindPos8);
                                        while ((uiFindPos9 != string::npos) && (uiFindPos9 < uiFindPos4)) {
                                            uiFindPos9 = sCode.find('(', uiFindPos9 + 1);
                                            uiFindPos4 = sCode.find(')', uiFindPos4 + 1);
                                        }
                                        string sDefine2 = sCode.substr(uiFindPos8, uiFindPos4 - uiFindPos8);

                                        outputProjectDCECleanDefine(sDefine2);

                                        //Get the block of code being wrapped
                                        string sCode2;
                                        uiFindPos8 = sCode.find_first_not_of(sWhiteSpace, uiFindPos4 + 1);
                                        if (sCode.at(uiFindPos8) == '{') {
                                            //Need to get the entire block of code being wrapped
                                            uiFindPos4 = sCode.find('}', uiFindPos8 + 1);
                                            //Skip any '{' found within the parameters itself
                                            uint uiFindPos5 = sCode.find('{', uiFindPos8 + 1);
                                            while ((uiFindPos5 != string::npos) && (uiFindPos5 < uiFindPos4)) {
                                                uiFindPos5 = sCode.find('{', uiFindPos5 + 1);
                                                uiFindPos4 = sCode.find('}', uiFindPos4 + 1);
                                            }
                                            sCode2 = sCode.substr(uiFindPos8, uiFindPos4 - uiFindPos8);
                                        } else {
                                            //This is a single line of code
                                            uiFindPos4 = sCode.find_first_of(sEndLine, uiFindPos8 + 1);
                                            sCode2 = sCode.substr(uiFindPos8, uiFindPos4 - uiFindPos8);
                                        }

                                        //Check if function is actually effected by this DCE
                                        if (sCode2.find(sAdd) != string::npos) {
                                            //Add the additional define
                                            string sCond = "&&";
                                            if (sDefine2.find_first_of("&|") != string::npos) {
                                                sCond = '(' + sDefine2 + ')' + sCond;
                                            } else {
                                                sCond = sDefine2 + sCond;
                                            }
                                            if (sFuncDefine.find_first_of("&|") != string::npos) {
                                                sCond += '(' + sFuncDefine + ')';
                                            } else {
                                                sCond += sFuncDefine;
                                            }
                                            sFuncDefine = sCond;
                                        }

                                        //Search for next occurrence
                                        uiFindPos7 = sCode.rfind(sSearch, uiFindPos7 - 1);
                                    }
                                }
                            }

                            //Check if not already added
                            map<string, DCEParams>::iterator itFind = mFoundDCEFunctions.find(sAdd);
                            if (itFind == mFoundDCEFunctions.end()) {
                                mFoundDCEFunctions[sAdd] = {sFuncDefine, sFileName};
                            } else if (itFind->second.sDefine.compare(sFuncDefine) != 0) {
                                //Check if either string contains the other
                                if (itFind->second.sDefine.find(sFuncDefine) != string::npos) {
                                    //keep the existing one
                                } else if (sFuncDefine.find(itFind->second.sDefine) != string::npos) {
                                    //use the new one in place of the original
                                    mFoundDCEFunctions[sAdd] = {sFuncDefine, sFileName};
                                } else {
                                    //Add the additional define
                                    string sCond = "||";
                                    if (itFind->second.sDefine.find_first_of("&|") != string::npos) {
                                        sCond = '(' + itFind->second.sDefine + ')' + sCond;
                                    } else {
                                        sCond = itFind->second.sDefine + sCond;
                                    }
                                    if (sFuncDefine.find_first_of("&|") != string::npos) {
                                        sCond += '(' + sFuncDefine + ')';
                                    } else {
                                        sCond += sFuncDefine;
                                    }
                                    mFoundDCEFunctions[sAdd] = {sCond, sFileName};
                                }
                            }
                        }
                        //Search for next occurrence
                        uiFindPos = sCode.find(asFuncIdents[uiIdents], uiFindPos + 1);
                    }
                }

                //Search for next occurrence
                uiFindPos = sFile.find(sSearch, uiFindPos2 + 1);
            }
        }
    }
}

void ProjectGenerator::outputProgramDCEsResolveDefine(string & sDefine, ConfigGenerator::DefaultValuesList mReserved)
{
    //Complex combinations of config options require determining exact values
    uint uiStartTag = sDefine.find_first_not_of(sPreProcessor);
    while (uiStartTag != string::npos) {
        //Get the next tag
        uint uiDiv = sDefine.find_first_of(sPreProcessor, uiStartTag);
        string sTag = sDefine.substr(uiStartTag, uiDiv - uiStartTag);
        //Check if tag is enabled
        ConfigGenerator::ValuesList::iterator ConfigOpt = m_ConfigHelper.getConfigOptionPrefixed(sTag);
        if ((ConfigOpt == m_ConfigHelper.m_vConfigValues.end()) ||
            (mReserved.find(ConfigOpt->m_sPrefix + ConfigOpt->m_sOption) != mReserved.end())) {
            //This config option doesn't exist but it is potentially included in its corresponding header file
            // Or this is a reserved value
        } else {
            //Replace the option with its value
            sDefine.replace(uiStartTag, uiDiv - uiStartTag, ConfigOpt->m_sValue);
            uiDiv = sDefine.find_first_of(sPreProcessor, uiStartTag);
        }

        //Get next
        uiStartTag = sDefine.find_first_not_of(sPreProcessor, uiDiv);
    }
    //Process the string to combine values
    findAndReplace(sDefine, "&&", "&");
    findAndReplace(sDefine, "||", "|");

    //Need to search through for !, &&,  || in correct order of occurrence
    const char acOps[] = {'!', '&', '|'};
    for (unsigned uiOp = 0; uiOp < sizeof(acOps) / sizeof(char); uiOp++) {
        uiStartTag = sDefine.find(acOps[uiOp]);
        while (uiStartTag != string::npos) {
            //Get right tag
            ++uiStartTag;
            uint uiRight = sDefine.find_first_of(sPreProcessor, uiStartTag);
            //Skip any '(' found within the function parameters itself
            if ((uiRight != string::npos) && (sDefine.at(uiRight) == '(')) {
                uint uiBack = uiRight + 1;
                uiRight = sDefine.find(')', uiBack) + 1;
                uint uiFindPos3 = sDefine.find('(', uiBack);
                while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiRight)) {
                    uiFindPos3 = sDefine.find('(', uiFindPos3 + 1);
                    uiRight = sDefine.find(')', uiRight + 1) + 1;
                }
            }
            string sRight = sDefine.substr(uiStartTag, uiRight - uiStartTag);
            --uiStartTag;

            //Check current operation
            if (acOps[uiOp] == '!') {
                if (sRight.compare("0") == 0) {
                    //!0 = 1
                    sDefine.replace(uiStartTag, uiRight - uiStartTag, 1, '1');
                } else if (sRight.compare("1") == 0) {
                    //!1 = 0
                    sDefine.replace(uiStartTag, uiRight - uiStartTag, 1, '0');
                } else {
                    //!X = (!X)
                    if (uiRight == string::npos) {
                        sDefine += ')';
                    } else {
                        sDefine.insert(uiRight, 1, ')');
                    }
                    sDefine.insert(uiStartTag, 1, '(');
                    uiStartTag += 2;
                }
            } else {
                //Get left tag
                uint uiLeft = sDefine.find_last_of(sPreProcessor, uiStartTag - 1);
                //Skip any ')' found within the function parameters itself
                if ((uiLeft != string::npos) && (sDefine.at(uiLeft) == ')')) {
                    uint uiBack = uiLeft - 1;
                    uiLeft = sDefine.rfind('(', uiBack);
                    uint uiFindPos3 = sDefine.rfind(')', uiBack);
                    while ((uiFindPos3 != string::npos) && (uiFindPos3 > uiLeft)) {
                        uiFindPos3 = sDefine.rfind(')', uiFindPos3 - 1);
                        uiLeft = sDefine.rfind('(', uiLeft - 1);
                    }
                } else {
                    uiLeft = (uiLeft == 0) ? 0 : uiLeft + 1;
                }
                string sLeft = sDefine.substr(uiLeft, uiStartTag - uiLeft);

                //Check current operation
                if (acOps[uiOp] == '&') {
                    if ((sLeft.compare("0") == 0) || (sRight.compare("0") == 0)) {
                        //0&&X or X&&0 == 0
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '0');
                        uiStartTag = uiLeft;
                    } else if (sLeft.compare("1") == 0) {
                        //1&&X = X
                        ++uiStartTag;
                        sDefine.erase(uiLeft, uiStartTag - uiLeft);
                        uiStartTag = uiLeft;
                    } else if (sRight.compare("1") == 0) {
                        //X&&1 = X
                        sDefine.erase(uiStartTag, uiRight - uiStartTag);
                    } else {
                        //X&&X = (X&&X)
                        if (uiRight == string::npos) {
                            sDefine += ')';
                        } else {
                            sDefine.insert(uiRight, 1, ')');
                        }
                        sDefine.insert(uiLeft, 1, '(');
                        uiStartTag += 2;
                    }
                } else {
                    if ((sLeft.compare("1") == 0) || (sRight.compare("1") == 0)) {
                        //1||X or X||1 == 1
                        ++uiStartTag;
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '1');
                        uiStartTag = uiLeft;
                    } else if (sLeft.compare("0") == 0) {
                        //0||X = X
                        ++uiStartTag;
                        sDefine.erase(uiLeft, uiStartTag - uiLeft);
                        uiStartTag = uiLeft;
                    } else if (sRight.compare("0") == 0) {
                        //X||0 == X
                        sDefine.erase(uiStartTag, uiRight - uiStartTag);
                    } else {
                        //X||X = (X||X)
                        if (uiRight == string::npos) {
                            sDefine += ')';
                        } else {
                            sDefine.insert(uiRight, 1, ')');
                        }
                        sDefine.insert(uiLeft, 1, '(');
                        uiStartTag += 2;
                    }
                }
            }
            findAndReplace(sDefine, "(0)", "0");
            findAndReplace(sDefine, "(1)", "1");

            //Get next
            uiStartTag = sDefine.find(acOps[uiOp], uiStartTag);
        }
    }
    //Remove any (RESERV)
    uiStartTag = sDefine.find('(');
    while (uiStartTag != string::npos) {
        uint uiEndTag = sDefine.find(')', uiStartTag);
        ++uiStartTag;
        //Skip any '(' found within the function parameters itself
        uint uiFindPos3 = sDefine.find('(', uiStartTag);
        while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiEndTag)) {
            uiFindPos3 = sDefine.find('(', uiFindPos3 + 1);
            uiEndTag = sDefine.find(')', uiEndTag + 1);
        }

        string sTag = sDefine.substr(uiStartTag, uiEndTag - uiStartTag);
        if ((sTag.find_first_of("&|()") == string::npos) ||
            ((uiStartTag == 1) && (uiEndTag == sDefine.length() - 1))) {
            sDefine.erase(uiEndTag, 1);
            sDefine.erase(--uiStartTag, 1);
        }
        uiStartTag = sDefine.find('(', uiStartTag);
    }

    findAndReplace(sDefine, "&", " && ");
    findAndReplace(sDefine, "|", " || ");
}

bool ProjectGenerator::outputProjectDCEsFindDeclarations(const string & sFile, const string & sFunction, const string & sFileName, string & sRetDeclaration, bool & bIsFunction)
{
    uint uiFindPos = sFile.find(sFunction);
    while (uiFindPos != string::npos) {
        uint uiFindPos4 = sFile.find_first_not_of(sWhiteSpace, uiFindPos + sFunction.length());
        if (sFile.at(uiFindPos4) == '(') {
            //Check if this is a function call or an actual declaration
            uint uiFindPos2 = sFile.find(')', uiFindPos4 + 1);
            if (uiFindPos2 != string::npos) {
                //Skip any '(' found within the function parameters itself
                uint uiFindPos3 = sFile.find('(', uiFindPos4 + 1);
                while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiFindPos2)) {
                    uiFindPos3 = sFile.find('(', uiFindPos3 + 1);
                    uiFindPos2 = sFile.find(')', uiFindPos2 + 1);
                }
                uiFindPos3 = sFile.find_first_not_of(sWhiteSpace, uiFindPos2 + 1);
                //If this is a definition (i.e. '{') then that means no declaration could be found (headers are searched before code files)
                if ((sFile.at(uiFindPos3) == ';') || (sFile.at(uiFindPos3) == '{')) {
                    uiFindPos3 = sFile.find_last_not_of(sWhiteSpace, uiFindPos - 1);
                    if (sNonName.find(sFile.at(uiFindPos3)) == string::npos) {
                        //Get the return type
                        uiFindPos = sFile.find_last_of(sWhiteSpace, uiFindPos3 - 1);
                        uiFindPos = (uiFindPos == string::npos) ? 0 : uiFindPos + 1;
                        //Return the declaration
                        sRetDeclaration = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos + 1);
                        bIsFunction = true;
                        return true;
                    } else if (sFile.at(uiFindPos3) == '*') {
                        //Return potentially contains a pointer
                        --uiFindPos3;
                        uiFindPos3 = sFile.find_last_not_of(sWhiteSpace, uiFindPos3 - 1);
                        if (sNonName.find(sFile.at(uiFindPos3)) == string::npos) {
                            //Get the return type
                            uiFindPos = sFile.find_last_of(sWhiteSpace, uiFindPos3 - 1);
                            uiFindPos = (uiFindPos == string::npos) ? 0 : uiFindPos + 1;
                            //Return the declaration
                            sRetDeclaration = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos + 1);
                            bIsFunction = true;
                            return true;
                        }
                    }
                }
            }
        } else if (sFile.at(uiFindPos4) == '[') {
            //This is an array/table
            //Check if this is an definition or an declaration
            uint uiFindPos2 = sFile.find(']', uiFindPos4 + 1);
            if (uiFindPos2 != string::npos) {
                //Skip multidimensional array
                while ((uiFindPos2 + 1 < sFile.length()) && (sFile.at(uiFindPos2 + 1) == '[')) {
                    uiFindPos2 = sFile.find(']', uiFindPos2 + 1);
                }
                uint uiFindPos3 = sFile.find_first_not_of(sWhiteSpace, uiFindPos2 + 1);
                if (sFile.at(uiFindPos3) == '=') {
                    uiFindPos3 = sFile.find_last_not_of(sWhiteSpace, uiFindPos - 1);
                    if (sNonName.find(sFile.at(uiFindPos3)) == string::npos) {
                        //Get the array type
                        uiFindPos = sFile.find_last_of(sWhiteSpace, uiFindPos3 - 1);
                        uiFindPos = (uiFindPos == string::npos) ? 0 : uiFindPos + 1;
                        //Return the definition
                        sRetDeclaration = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos + 1);
                        bIsFunction = false;
                        return true;
                    } else if (sFile.at(uiFindPos3) == '*') {
                        //Type potentially contains a pointer
                        --uiFindPos3;
                        uiFindPos3 = sFile.find_last_not_of(sWhiteSpace, uiFindPos3 - 1);
                        if (sNonName.find(sFile.at(uiFindPos3)) == string::npos) {
                            //Get the array type
                            uiFindPos = sFile.find_last_of(sWhiteSpace, uiFindPos3 - 1);
                            uiFindPos = (uiFindPos == string::npos) ? 0 : uiFindPos + 1;
                            //Return the definition
                            sRetDeclaration = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos + 1);
                            bIsFunction = false;
                            return true;
                        }
                    }
                }
            }
        }

        //Search for next occurrence
        uiFindPos = sFile.find(sFunction, uiFindPos + sFunction.length() + 1);
    }
    return false;
}

void ProjectGenerator::outputProjectDCECleanDefine(string & sDefine)
{
    const string asTagReplace[] = {"EXTERNAL", "INTERNAL", "INLINE"};
    const string asTagReplaceRemove[] = {"_FAST", "_SLOW"};
    //There are some macro tags that require conversion
    for (unsigned uiRep = 0; uiRep < sizeof(asTagReplace) / sizeof(string); uiRep++) {
        string sSearch = asTagReplace[uiRep] + '_';
        uint uiFindPos = 0;
        while ((uiFindPos = sDefine.find(sSearch, uiFindPos)) != string::npos) {
            uint uiFindPos4 = sDefine.find_first_of('(', uiFindPos + 1);
            uint uiFindPosBack = uiFindPos;
            uiFindPos += sSearch.length();
            string sTagPart = sDefine.substr(uiFindPos, uiFindPos4 - uiFindPos);
            //Remove conversion values
            for (unsigned uiRem = 0; uiRem < sizeof(asTagReplaceRemove) / sizeof(string); uiRem++) {
                uint uiFindRem = 0;
                while ((uiFindRem = sTagPart.find(asTagReplaceRemove[uiRem], uiFindRem)) != string::npos) {
                    sTagPart.erase(uiFindRem, asTagReplaceRemove[uiRem].length());
                }
            }
            sTagPart = "HAVE_" + sTagPart + '_' + asTagReplace[uiRep];
            uint uiFindPos6 = sDefine.find_first_of(')', uiFindPos4 + 1);
            //Skip any '(' found within the parameters itself
            uint uiFindPos5 = sDefine.find('(', uiFindPos4 + 1);
            while ((uiFindPos5 != string::npos) && (uiFindPos5 < uiFindPos6)) {
                uiFindPos5 = sDefine.find('(', uiFindPos5 + 1);
                uiFindPos6 = sDefine.find(')', uiFindPos6 + 1);
            }
            //Update tag with replacement
            uint uiRepLength = uiFindPos6 - uiFindPosBack + 1;
            sDefine.replace(uiFindPosBack, uiRepLength, sTagPart);
            uiFindPos = uiFindPosBack + sTagPart.length();
        }
    }

    //Check if the tag contains multiple conditionals
    removeWhiteSpace(sDefine);
    uint uiFindPos = sDefine.find_first_of("&|");
    while (uiFindPos != string::npos) {
        uint uiFindPos4 = uiFindPos++;
        if ((sDefine.at(uiFindPos) == '&') || (sDefine.at(uiFindPos) == '|')) {
            ++uiFindPos;
        }
        if (sDefine.at(uiFindPos) == '!') {
            ++uiFindPos;
        }
        while (sDefine.at(uiFindPos) == '(') {
            ++uiFindPos;
            uiFindPos4 = uiFindPos;
            if (sDefine.at(uiFindPos) == '!') {
                ++uiFindPos;
            }
        }

        //Check if each conditional is valid
        bool bValid = false;
        for (unsigned uiTagk = 0; uiTagk < sizeof(asDCETags) / sizeof(string); uiTagk++) {
            if (sDefine.find(asDCETags[uiTagk], uiFindPos) == uiFindPos) {
                // We have found a valid additional tag
                bValid = true;
                break;
            }
        }

        if (!bValid && isupper(sDefine.at(uiFindPos))) {
            string sUnknown = sDefine.substr(uiFindPos, sDefine.find_first_of("&|)", uiFindPos + 1) - uiFindPos);
            if ((sUnknown.find("AV_") != 0) && (sUnknown.find("FF_") != 0)) {
                cout << "  Warning: Found unknown macro in DCE condition " << sUnknown << endl;
            }
        }

        if (!bValid) {
            //Remove invalid components out of string
            uiFindPos = sDefine.find_first_of("&|)", uiFindPos + 1);
            bool bCutTail = false;
            if ((sDefine.at(uiFindPos4 - 1) == '(') && ((uiFindPos != string::npos) && (sDefine.at(uiFindPos) != ')'))) {
                //Trim operators after tag instead of before it
                uiFindPos = sDefine.find_first_not_of("&|", uiFindPos + 1);
                bCutTail = true;
            } else if ((uiFindPos != string::npos) && (sDefine.at(uiFindPos) == ')') && ((uiFindPos4 == 0) || (sDefine.at(uiFindPos4 - 1) != '('))) {
                //This is a function call that should get cut out
                ++uiFindPos;
                uiFindPos = (uiFindPos >= sDefine.size()) ? string::npos : uiFindPos;
            }
            bool bCutBrackets = false;
            while ((uiFindPos != string::npos) && (sDefine.at(uiFindPos) == ')') && (uiFindPos4 != 0) && (sDefine.at(uiFindPos4 - 1) == '(')) {
                //Remove any ()'s that are no longer needed
                --uiFindPos4;
                ++uiFindPos;
                uiFindPos = (uiFindPos >= sDefine.size()) ? string::npos : uiFindPos;
                bCutBrackets = true;
            }
            if (bCutBrackets && (sDefine.find_first_of("&|!)", uiFindPos) != uiFindPos)) {
                //This was a typecast
                uiFindPos = sDefine.find_first_of("&|!)", uiFindPos);
                if ((uiFindPos != string::npos) && (uiFindPos4 != 0) && (sDefine.at(uiFindPos4 - 1) == '(')) {
                    bCutBrackets = false;
                    uiFindPos = sDefine.find_first_not_of("&|", uiFindPos + 1);
                    bCutTail = true;
                }
            }
            if (bCutBrackets && (uiFindPos4 > 0)) {
                //Need to cut back preceding operators
                uiFindPos4 = sDefine.find_last_not_of("&|!", uiFindPos4 - 1);
                uiFindPos4 = (uiFindPos4 == string::npos) ? 0 : uiFindPos4 + 1;
            }
            uint uiFindPos3 = (uiFindPos == string::npos) ? uiFindPos : uiFindPos - uiFindPos4;
            sDefine.erase(uiFindPos4, uiFindPos3);
            uiFindPos = (uiFindPos4 >= sDefine.size() - 1) ? string::npos : uiFindPos4;
            if (bCutTail) {
                //Set start back at previously conditional operation
                uiFindPos = sDefine.find_last_not_of("!&|(", uiFindPos - 1);
                uiFindPos = (uiFindPos == string::npos) ? 0 : uiFindPos + 1;
            }
        } else {
            uiFindPos = sDefine.find_first_of("&|", uiFindPos + 1);
        }
    }
    //Remove any surrounding braces that may be left
    while (sDefine.at(0) == '(' && sDefine.at(sDefine.size() - 1) == ')') {
        sDefine.erase(0, 1);
        sDefine.pop_back();
    }
}