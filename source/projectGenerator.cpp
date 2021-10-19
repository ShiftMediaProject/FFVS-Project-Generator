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
#include <iomanip>
#include <sstream>
#include <utility>

#define TEMPLATE_COMPAT_ID 100
#define TEMPLATE_MATH_ID 101
#define TEMPLATE_UNISTD_ID 102
#define TEMPLATE_SLN_ID 103
#define TEMPLATE_VCXPROJ_ID 104
#define TEMPLATE_FILTERS_ID 105
#define TEMPLATE_PROG_VCXPROJ_ID 106
#define TEMPLATE_PROG_FILTERS_ID 107
#define TEMPLATE_STDATOMIC_ID 108
#define TEMPLATE_BAT_ID 109

bool ProjectGenerator::passAllMake()
{
    if ((m_configHelper.m_toolchain == "msvc") || (m_configHelper.m_toolchain == "icl")) {
        // Copy the required header files to output directory
        const bool copy = copyResourceFile(TEMPLATE_COMPAT_ID, m_configHelper.m_solutionDirectory + "compat.h", true);
        if (!copy) {
            outputError("Failed writing to output location. Make sure you have the appropriate user permissions.");
            return false;
        }
        copyResourceFile(TEMPLATE_MATH_ID, m_configHelper.m_solutionDirectory + "math.h", true);
        copyResourceFile(TEMPLATE_UNISTD_ID, m_configHelper.m_solutionDirectory + "unistd.h", true);
        string fileName;
        if (findFile(m_configHelper.m_rootDirectory + "compat/atomics/win32/stdatomic.h", fileName)) {
            copyResourceFile(TEMPLATE_STDATOMIC_ID, m_configHelper.m_solutionDirectory + "stdatomic.h", true);
        }
    }

    // Loop through each library make file
    vector<string> libraries;
    m_configHelper.getConfigList("LIBRARY_LIST", libraries);
    for (const auto& i : libraries) {
        // Check if library is enabled
        if (m_configHelper.isConfigOptionEnabled(i)) {
            m_projectDir = m_configHelper.m_rootDirectory + "lib" + i + "/";
            // Locate the project dir for specified library
            string retFileName;
            if (!findFile(m_projectDir + "MakeFile", retFileName)) {
                outputError("Could not locate directory for library (" + i + ")");
                return false;
            }
            // Run passMake on default Makefile
            if (!passMake()) {
                return false;
            }
            // Check for any sub directories
            m_projectDir += "x86/";
            if (findFile(m_projectDir + "MakeFile", retFileName)) {
                // Pass the sub directory
                if (!passMake()) {
                    return false;
                }
            }
            // Reset project dir so it does not include additions
            m_projectDir.resize(m_projectDir.length() - 4);
            // Output the project
            if (!outputProject()) {
                return false;
            }
            outputProjectCleanup();
        }
    }

    // Output the solution file
    if (!outputSolution()) {
        return false;
    }

    if (m_configHelper.m_onlyDCE) {
        // Delete no longer needed compilation files
        deleteCreatedFiles();
    }
    return true;
}

void ProjectGenerator::deleteCreatedFiles()
{
    // Get list of libraries and programs
    vector<string> libraries;
    m_configHelper.getConfigList("LIBRARY_LIST", libraries);
    vector<string> programs;
    m_configHelper.getConfigList("PROGRAM_LIST", programs);

    // Delete any previously generated files
    vector<string> existingFiles;
    findFiles(m_configHelper.m_solutionDirectory + "ffmpeg.sln", existingFiles, false);
    findFiles(m_configHelper.m_solutionDirectory + "libav.sln", existingFiles, false);
    findFiles(m_configHelper.m_solutionDirectory + "compat.h", existingFiles, false);
    findFiles(m_configHelper.m_solutionDirectory + "math.h", existingFiles, false);
    findFiles(m_configHelper.m_solutionDirectory + "unistd.h", existingFiles, false);
    findFiles(m_configHelper.m_solutionDirectory + "stdatomic.h", existingFiles, false);
    findFiles(m_configHelper.m_solutionDirectory + "ffmpeg_with_latest_sdk.bat", existingFiles, false);
    findFiles(m_configHelper.m_solutionDirectory + "libav_with_latest_sdk.bat", existingFiles, false);
    for (auto& i : libraries) {
        i = "lib" + i;
        findFiles(m_configHelper.m_solutionDirectory + i + ".vcxproj", existingFiles, false);
        findFiles(m_configHelper.m_solutionDirectory + i + ".vcxproj.filters", existingFiles, false);
        findFiles(m_configHelper.m_solutionDirectory + i + ".def", existingFiles, false);
    }
    for (const auto& i : programs) {
        findFiles(m_configHelper.m_solutionDirectory + i + ".vcxproj", existingFiles, false);
        findFiles(m_configHelper.m_solutionDirectory + i + ".vcxproj.filters", existingFiles, false);
        findFiles(m_configHelper.m_solutionDirectory + i + ".def", existingFiles, false);
    }
    for (const auto& i : existingFiles) {
        deleteFile(i);
    }

    // Check for any created folders
    vector<string> existingFolders;
    for (const auto& i : libraries) {
        findFolders(m_configHelper.m_solutionDirectory + i, existingFolders, false);
    }
    for (const auto& i : programs) {
        findFolders(m_configHelper.m_solutionDirectory + i, existingFolders, false);
    }
    for (const auto& i : existingFolders) {
        // Search for any generated files in the directories
        existingFiles.resize(0);
        findFiles(i + "/dce_defs.c", existingFiles, false);
        findFiles(i + "/*_wrap.c", existingFiles, false);
        if (!m_configHelper.m_usingExistingConfig) {
            findFiles(i + "/*_list.c", existingFiles, false);
        }
        for (auto& j : existingFiles) {
            deleteFile(j);
        }
        // Check if the directory is now empty and delete if it is
        if (isFolderEmpty(i)) {
            deleteFolder(i);
        }
    }
}

void ProjectGenerator::errorFunc(const bool cleanupFiles)
{
    if (cleanupFiles) {
        // Cleanup any partially created files
        m_configHelper.deleteCreatedFiles();
        deleteCreatedFiles();

        // Delete any temporary file leftovers
        deleteFolder(m_tempDirectory);
    }

    pressKeyToContinue();
    exit(1);
}

bool ProjectGenerator::outputProject()
{
    // Output the generated files
    const uint pos = m_projectDir.rfind('/', m_projectDir.length() - 2) + 1;
    m_projectName = m_projectDir.substr(pos, m_projectDir.length() - 1 - pos);

    // Check all files are correctly located
    if (!checkProjectFiles()) {
        return false;
    }

    // Get dependency directories
    StaticList includeDirs;
    StaticList lib32Dirs;
    StaticList lib64Dirs;
    StaticList definesShared;
    StaticList definesStatic;
    buildDependencyValues(includeDirs, lib32Dirs, lib64Dirs, definesShared, definesStatic);

    // Create missing definitions of functions removed by DCE
    if (!outputProjectDCE(includeDirs)) {
        return false;
    }

    if (m_configHelper.m_onlyDCE) {
        // Exit here to prevent outputting project files
        return true;
    }

    // Generate the exports file
    if (!outputProjectExports(includeDirs)) {
        return false;
    }

    // We now have complete list of all the files that we need
    outputLine("  Generating project file (" + m_projectName + ")...");

    // Open the input temp project file
    string projectFile;
    if (!loadFromResourceFile(TEMPLATE_VCXPROJ_ID, projectFile)) {
        return false;
    }

    // Open the input temp project file filters
    string filtersFile;
    if (!loadFromResourceFile(TEMPLATE_FILTERS_ID, filtersFile)) {
        return false;
    }

    // Remove any winrt configurations if not requested
    if (!m_configHelper.isConfigOptionEnabled("WINRT") && !m_configHelper.isConfigOptionEnabled("UWP")) {
        outputStripWinRT(projectFile);
    }

    // Add all project source files
    outputSourceFiles(projectFile, filtersFile);

    // Add the build events
    outputBuildEvents(projectFile);

    // Add ASM requirements
    outputASMTools(projectFile);

    // Add CUDA requirements
    outputCUDATools(projectFile);

    // Add the dependency libraries
    if (!outputDependencyLibs(projectFile)) {
        return false;
    }

    // Add additional includes to include list
    outputIncludeDirs(includeDirs, projectFile);

    // Add additional lib includes to include list
    outputLibDirs(lib32Dirs, lib64Dirs, projectFile);

    // Add additional defines
    outputDefines(definesShared, definesStatic, projectFile);

    // Replace all template tag arguments
    outputTemplateTags(projectFile, filtersFile);

    // Write output project
    const string outProjectFile = m_configHelper.m_solutionDirectory + m_projectName + ".vcxproj";
    if (!writeToFile(outProjectFile, projectFile, true)) {
        return false;
    }

    // Write output filters
    const string outFiltersFile = m_configHelper.m_solutionDirectory + m_projectName + ".vcxproj.filters";
    return writeToFile(outFiltersFile, filtersFile, true);
}

bool ProjectGenerator::outputProgramProject(const string& destinationFile, const string& destinationFilterFile)
{
    // Pass makefile for program
    if (!passProgramMake()) {
        return false;
    }

    // Check all files are correctly located
    if (!checkProjectFiles()) {
        return false;
    }

    // Get dependency directories
    StaticList includeDirs;
    StaticList lib32Dirs;
    StaticList lib64Dirs;
    StaticList definesShared;
    StaticList definesStatic;
    buildDependencyValues(includeDirs, lib32Dirs, lib64Dirs, definesShared, definesStatic);

    // Create missing definitions of functions removed by DCE
    if (!outputProjectDCE(includeDirs)) {
        return false;
    }

    if (m_configHelper.m_onlyDCE) {
        // Exit here to prevent outputting project files
        return true;
    }

    // We now have complete list of all the files that we need
    outputLine("  Generating project file (" + m_projectName + ")...");

    // Open the template program
    string programFile;
    if (!loadFromResourceFile(TEMPLATE_PROG_VCXPROJ_ID, programFile)) {
        return false;
    }

    // Open the template program filters
    string programFiltersFile;
    if (!loadFromResourceFile(TEMPLATE_PROG_FILTERS_ID, programFiltersFile)) {
        return false;
    }

    // Add all project source files
    outputSourceFiles(programFile, programFiltersFile);

    // Add the build events
    outputBuildEvents(programFile);

    // Add ASM requirements
    outputASMTools(programFile);

    // Add CUDA requirements
    outputCUDATools(programFile);

    // Add the dependency libraries
    if (!outputDependencyLibs(programFile, true)) {
        return false;
    }

    // Add additional includes to include list
    outputIncludeDirs(includeDirs, programFile);

    // Add additional lib includes to include list
    outputLibDirs(lib32Dirs, lib64Dirs, programFile);

    // Add additional defines
    outputDefines(definesShared, definesStatic, programFile, true);

    // Replace all template tag arguments
    outputTemplateTags(programFile, programFiltersFile);

    // Write program file
    if (!writeToFile(destinationFile, programFile, true)) {
        return false;
    }

    // Write output filters
    if (!writeToFile(destinationFilterFile, programFiltersFile, true)) {
        return false;
    }

    outputProjectCleanup();

    return true;
}

void ProjectGenerator::outputProjectCleanup()
{
    // Reset all internal values
    m_inLine.clear();
    m_includes.clear();
    m_replaceIncludes.clear();
    m_includesCPP.clear();
    m_includesC.clear();
    m_includesASM.clear();
    m_includesH.clear();
    m_includesCU.clear();
    m_libs.clear();
    m_unknowns.clear();
    m_projectDir.clear();
}

bool ProjectGenerator::outputSolution()
{
    // Create program list
    map<string, string> programList;
    if (!m_configHelper.m_isLibav) {
        programList["ffmpeg"] = "CONFIG_FFMPEG";
        programList["ffplay"] = "CONFIG_FFPLAY";
        programList["ffprobe"] = "CONFIG_FFPROBE";
    } else {
        programList["avconv"] = "CONFIG_AVCONV";
        programList["avplay"] = "CONFIG_AVPLAY";
        programList["avprobe"] = "CONFIG_AVPROBE";
    }

    // Next add the projects
    for (const auto& i : programList) {
        // Check if program is enabled
        if (m_configHelper.getConfigOptionPrefixed(i.second)->m_value.compare("1") == 0) {
            m_projectDir = m_configHelper.m_rootDirectory;
            // Create project files for program
            m_projectName = i.first;
            const string destinationFile = m_configHelper.m_solutionDirectory + i.first + ".vcxproj";
            const string destinationFilterFile = m_configHelper.m_solutionDirectory + i.first + ".vcxproj.filters";
            if (!outputProgramProject(destinationFile, destinationFilterFile)) {
                return false;
            }
        }
    }

    if (m_configHelper.m_onlyDCE) {
        // Don't output solution and just exit early
        return true;
    }

    outputLine("  Generating solution file...");
    // Open the input temp project file
    string solutionFile;
    if (!loadFromResourceFile(TEMPLATE_SLN_ID, solutionFile)) {
        return false;
    }

    // Remove any winrt configurations if not requested
    if (!m_configHelper.isConfigOptionEnabled("winrt") && !m_configHelper.isConfigOptionEnabled("uwp")) {
        outputStripWinRTSolution(solutionFile);
    }

    map<string, string> keys;
    buildProjectGUIDs(keys);
    string solutionKey = "8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942";

    vector<string> addedKeys;

    const string project = "\r\nProject(\"{";
    const string project2 = "}\") = \"";
    const string project3 = "\", \"";
    const string project4 = ".vcxproj\", \"{";
    const string projectEnd = "}\"";
    const string projectClose = "\r\nEndProject";

    const string depend = "\r\n	ProjectSection(ProjectDependencies) = postProject";
    const string dependClose = "\r\n	EndProjectSection";
    const string subDepend = "\r\n		{";
    const string subDepend2 = "} = {";
    const string subDependEnd = "}";

    // Find the start of the file
    const string fileStart = "Project";
    uint pos = solutionFile.find(fileStart) - 2;
    for (const auto& i : m_projectLibs) {
        // Check if this is a library or a program
        if (programList.find(i.first) == programList.end()) {
            // Check if this library has a known key (to lazy to auto generate at this time)
            if (keys.find(i.first) == keys.end()) {
                outputError("Unknown library. Could not determine solution key (" + i.first + ")");
                return false;
            }
            // Add the library to the solution
            string projectAdd = project;
            projectAdd += solutionKey;
            projectAdd += project2;
            projectAdd += i.first;
            projectAdd += project3;
            projectAdd += i.first;
            projectAdd += project4;
            projectAdd += keys[i.first];
            projectAdd += projectEnd;

            // Add the key to the used key list
            addedKeys.push_back(keys[i.first]);

            // Add the dependencies
            if (i.second.size() > 0) {
                projectAdd += depend;
                for (auto& j : i.second) {
                    // Check if this library has a known key
                    if (keys.find(j) == keys.end()) {
                        outputError("Unknown library dependency. Could not determine solution key (" + j + ")");
                        return false;
                    }
                    projectAdd += subDepend;
                    projectAdd += keys[j];
                    projectAdd += subDepend2;
                    projectAdd += keys[j];
                    projectAdd += subDependEnd;
                }
                projectAdd += dependClose;
            }
            projectAdd += projectClose;

            // Insert into solution string
            solutionFile.insert(pos, projectAdd);
            pos += projectAdd.length();
        }
    }

    // Next add the projects
    string projectAdd;
    vector<string> addedPrograms;
    for (const auto& i : programList) {
        // Check if program is enabled
        if (m_configHelper.getConfigOptionPrefixed(i.second)->m_value.compare("1") == 0) {
            // Add the program to the solution
            projectAdd += project;
            projectAdd += solutionKey;
            projectAdd += project2;
            projectAdd += i.first;
            projectAdd += project3;
            projectAdd += i.first;
            projectAdd += project4;
            projectAdd += keys[i.first];
            projectAdd += projectEnd;

            // Add the key to the used key list
            addedPrograms.push_back(keys[i.first]);

            // Add the dependencies
            projectAdd += depend;
            auto mitLibs = m_projectLibs[i.first].begin();
            while (mitLibs != m_projectLibs[i.first].end()) {
                // Add all project libraries as dependencies
                if (!m_configHelper.m_isLibav) {
                    projectAdd += subDepend;
                    projectAdd += keys[*mitLibs];
                    projectAdd += subDepend2;
                    projectAdd += keys[*mitLibs];
                    projectAdd += subDependEnd;
                }
                // next
                ++mitLibs;
            }
            projectAdd += dependClose;
            projectAdd += projectClose;
        }
    }

    // Check if there were actually any programs added
    string programKey = "8A736DDA-6840-4E65-9DA4-BF65A2A70428";
    if (projectAdd.length() > 0) {
        // Add program key
        projectAdd += "\r\nProject(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"Programs\", \"Programs\", \"{";
        projectAdd += programKey;
        projectAdd += "}\"";
        projectAdd += "\r\nEndProject";

        // Insert into solution string
        solutionFile.insert(pos, projectAdd);
    }

    // Check if winrt builds are enabled
    bool addWinrt = m_configHelper.isConfigOptionEnabled("winrt") || m_configHelper.isConfigOptionEnabled("uwp");

    // Next Add the solution configurations
    string configStart = "GlobalSection(ProjectConfigurationPlatforms) = postSolution";
    pos = solutionFile.find(configStart) + configStart.length();
    string configPlatform = "\r\n		{";
    string configPlatform2 = "}.";
    string configPlatform3 = "|";
    string buildConfigs[10] = {"Debug", "DebugDLL", "DebugDLLWinRT", "DebugWinRT", "Release", "ReleaseDLL",
        "ReleaseDLLStaticDeps", "ReleaseDLLWinRT", "ReleaseDLLWinRTStaticDeps", "ReleaseWinRT"};
    string buildArchsSol[2] = {"x86", "x64"};
    string buildArchs[2] = {"Win32", "x64"};
    string buildTypes[2] = {".ActiveCfg = ", ".Build.0 = "};
    string addPlatform;
    // Add the lib keys
    for (const auto& i : addedKeys) {
        // loop over build configs
        for (uint j = 0; j < 7; j++) {
            // Skip winrt configs if not enabled
            if ((buildConfigs[j].find("WinRT") == string::npos) || addWinrt) {
                // loop over build archs
                for (uint k = 0; k < 2; k++) {
                    // loop over build types
                    for (const auto& aBuildType : buildTypes) {
                        addPlatform += configPlatform;
                        addPlatform += i;
                        addPlatform += configPlatform2;
                        addPlatform += buildConfigs[j];
                        addPlatform += configPlatform3;
                        addPlatform += buildArchsSol[k];
                        addPlatform += aBuildType;
                        addPlatform += buildConfigs[j];
                        addPlatform += configPlatform3;
                        addPlatform += buildArchs[k];
                    }
                }
            }
        }
    }

    // Add the program keys
    for (const auto& i : addedPrograms) {
        // Loop over build configs
        for (uint j = 0; j < sizeof(buildConfigs) / sizeof(buildConfigs[0]); j++) {
            // Skip winrt configs if not enabled
            if ((buildConfigs[j].find("WinRT") == string::npos) || addWinrt) {
                // Loop over build archs
                for (uint k = 0; k < sizeof(buildArchsSol) / sizeof(buildArchsSol[0]); k++) {
                    // Loop over build types
                    for (uint m = 0; m < sizeof(buildTypes) / sizeof(buildTypes[0]); m++) {
                        if ((m == 1) && (j != 4)) {
                            // We dont build programs by default except for Release config
                            continue;
                        }
                        addPlatform += configPlatform;
                        addPlatform += i;
                        addPlatform += configPlatform2;
                        addPlatform += buildConfigs[j];
                        addPlatform += configPlatform3;
                        addPlatform += buildArchsSol[k];
                        addPlatform += buildTypes[m];
                        if (j == 2) {
                            addPlatform += buildConfigs[1];
                        } else if (j == 3) {
                            addPlatform += buildConfigs[0];
                        } else if (j == 6) {
                            addPlatform += buildConfigs[5];
                        } else if (j == 7) {
                            addPlatform += buildConfigs[5];
                        } else if (j == 8) {
                            addPlatform += buildConfigs[5];
                        } else if (j == 9) {
                            addPlatform += buildConfigs[4];
                        } else {
                            addPlatform += buildConfigs[j];
                        }
                        addPlatform += configPlatform3;
                        addPlatform += buildArchs[k];
                    }
                }
            }
        }
    }
    // Insert into solution string
    solutionFile.insert(pos, addPlatform);

    // Add any programs to the nested projects
    if (addedPrograms.size() > 0) {
        string nestedStart = "GlobalSection(NestedProjects) = preSolution";
        uint pos2 = solutionFile.find(nestedStart) + nestedStart.length();
        string nest = "\r\n		{";
        string nest2 = "} = {";
        string nestEnd = "}";
        string nestProg;
        for (const auto& i : addedPrograms) {
            nestProg += nest;
            nestProg += i;
            nestProg += nest2;
            nestProg += programKey;
            nestProg += nestEnd;
        }
        // Insert into solution string
        solutionFile.insert(pos2, nestProg);
    }

    // Write output solution
    string projectName = m_configHelper.m_projectName;
    transform(projectName.begin(), projectName.end(), projectName.begin(), tolower);
    const string outSolutionFile = m_configHelper.m_solutionDirectory + projectName + ".sln";
    if (!writeToFile(outSolutionFile, solutionFile, true)) {
        return false;
    }

    outputLine("  Generating SDK batch file...");
    // Open the input temp project file
    string batFile;
    if (!loadFromResourceFile(TEMPLATE_BAT_ID, batFile)) {
        return false;
    }

    // Change all occurrences of template_in with solution name
    const string searchTag = "template_in";
    uint findPos = batFile.find(searchTag);
    while (findPos != string::npos) {
        // Replace
        batFile.replace(findPos, searchTag.length(), projectName);
        // Get next
        findPos = batFile.find(searchTag, findPos + 1);
    }

    // Write to output
    const string outBatFile = m_configHelper.m_solutionDirectory + projectName + "_with_latest_sdk.bat";
    if (!writeToFile(outBatFile, batFile, true)) {
        return false;
    }

    return true;
}

void ProjectGenerator::outputTemplateTags(string& projectTemplate, string& filtersTemplate) const
{
    // Change all occurrences of template_in with project name
    const string searchTag = "template_in";
    uint findPos = projectTemplate.find(searchTag);
    while (findPos != string::npos) {
        // Replace
        projectTemplate.replace(findPos, searchTag.length(), m_projectName);
        // Get next
        findPos = projectTemplate.find(searchTag, findPos + 1);
    }
    uint findPosFilt = filtersTemplate.find(searchTag);
    while (findPosFilt != string::npos) {
        // Replace
        filtersTemplate.replace(findPosFilt, searchTag.length(), m_projectName);
        // Get next
        findPosFilt = filtersTemplate.find(searchTag, findPosFilt + 1);
    }

    // Change all occurrences of template_shin with short project name
    const string shortSearchTag = "template_shin";
    findPos = projectTemplate.find(shortSearchTag);
    string projectNameShort = m_projectName.substr(3); // The full name minus the lib prefix
    while (findPos != string::npos) {
        // Replace
        projectTemplate.replace(findPos, shortSearchTag.length(), projectNameShort);
        // Get next
        findPos = projectTemplate.find(shortSearchTag, findPos + 1);
    }
    findPosFilt = filtersTemplate.find(shortSearchTag);
    while (findPosFilt != string::npos) {
        // Replace
        filtersTemplate.replace(findPosFilt, shortSearchTag.length(), projectNameShort);
        // Get next
        findPosFilt = filtersTemplate.find(shortSearchTag, findPosFilt + 1);
    }

    // Change all occurrences of template_platform with specified project toolchain
    string toolchain = "<PlatformToolset Condition=\"'$(VisualStudioVersion)'=='12.0'\">v120</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(VisualStudioVersion)'=='14.0'\">v140</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(VisualStudioVersion)'=='15.0'\">v141</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(VisualStudioVersion)'=='16.0'\">v142</PlatformToolset>";
    if (m_configHelper.m_toolchain.compare("msvc") != 0) {
        toolchain +=
            "\r\n    <PlatformToolset Condition=\"'$(ICPP_COMPILER13)'!=''\">Intel C++ Compiler XE 13.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER14)'!=''\">Intel C++ Compiler XE 14.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER15)'!=''\">Intel C++ Compiler XE 15.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER16)'!=''\">Intel C++ Compiler 16.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER17)'!=''\">Intel C++ Compiler 17.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER18)'!=''\">Intel C++ Compiler 18.0</PlatformToolset>\r\n\
    <PlatformToolset Condition=\"'$(ICPP_COMPILER19)'!=''\">Intel C++ Compiler 19.0</PlatformToolset>";
    }

    const string platformSearch = "<PlatformToolset>template_platform</PlatformToolset>";
    findPos = projectTemplate.find(platformSearch);
    while (findPos != string::npos) {
        // Replace
        projectTemplate.replace(findPos, platformSearch.length(), toolchain);
        // Get next
        findPos = projectTemplate.find(platformSearch, findPos + platformSearch.length());
    }

    // Set the project key
    string projectGuid = "<ProjectGuid>{";
    findPos = projectTemplate.find(projectGuid);
    if (findPos != string::npos) {
        map<string, string> keys;
        buildProjectGUIDs(keys);
        findPos += projectGuid.length();
        projectTemplate.replace(findPos, keys[m_projectName].length(), keys[m_projectName]);
    }

    // Change all occurrences of template_outdir with configured output directory
    string outDir = m_configHelper.m_outDirectory;
    replace(outDir.begin(), outDir.end(), '/', '\\');
    if (outDir.at(0) == '.') {
        outDir = "$(ProjectDir)" + outDir; // Make any relative paths based on project dir
    }
    const string outSearchTag = "template_outdir";
    findPos = projectTemplate.find(outSearchTag);
    while (findPos != string::npos) {
        // Replace
        projectTemplate.replace(findPos, outSearchTag.length(), outDir);
        // Get next
        findPos = projectTemplate.find(outSearchTag, findPos + 1);
    }

    // Change all occurrences of template_rootdir with configured output directory
    string rootDir = m_configHelper.m_rootDirectory;
    m_configHelper.makeFileProjectRelative(rootDir, rootDir);
    replace(rootDir.begin(), rootDir.end(), '/', '\\');
    const string rootSearchTag = "template_rootdir";
    findPos = projectTemplate.find(rootSearchTag);
    while (findPos != string::npos) {
        // Replace
        projectTemplate.replace(findPos, rootSearchTag.length(), rootDir);
        // Get next
        findPos = projectTemplate.find(rootSearchTag, findPos + 1);
    }

    // Change all occurrences of template_winver
    uint major, minor;
    if (!m_configHelper.getMinWindowsVersion(major, minor)) {
        outputWarning("Could not detect a supported Windows version. Defaults of WinXP will be used instead.");
        major = 5;
        minor = 1;
    }
    string subsystemVer32 = to_string(major);
    subsystemVer32 += '.';
    subsystemVer32 += to_string(minor);
    // Create 64bit version which must have min of Vista 6.0
    string subsystemVer64;
    if (major < 6) {
        subsystemVer64 = "6.0";
    } else {
        subsystemVer64 = subsystemVer32;
    }
    const string winver32Tag = "template_winver32";
    findPos = projectTemplate.find(winver32Tag);
    while (findPos != string::npos) {
        // Replace
        projectTemplate.replace(findPos, winver32Tag.length(), subsystemVer32);
        // Get next
        findPos = projectTemplate.find(winver32Tag, findPos + 1);
    }
    const string winver64Tag = "template_winver64";
    findPos = projectTemplate.find(winver64Tag);
    while (findPos != string::npos) {
        // Replace
        projectTemplate.replace(findPos, winver64Tag.length(), subsystemVer64);
        // Get next
        findPos = projectTemplate.find(winver64Tag, findPos + 1);
    }

    // Change all occurrences of template_winnt
    string winNtVer, winNtVer32 = "0x";
    stringstream ss;
    ss << std::setfill('0') << std::setw(sizeof(char) * 2) << std::hex << major;
    ss >> winNtVer;
    ss.clear();
    winNtVer32 += winNtVer;
    ss << std::setfill('0') << std::setw(sizeof(char) * 2) << std::hex << minor;
    ss >> winNtVer;
    ss.clear();
    winNtVer32 += winNtVer;
    string winNtVer64;
    if (major < 6) {
        winNtVer64 = "0x0600";
    } else {
        winNtVer64 = winNtVer32;
    }
    const string winnt32Tag = "template_winnt32";
    findPos = projectTemplate.find(winnt32Tag);
    while (findPos != string::npos) {
        // Replace
        projectTemplate.replace(findPos, winnt32Tag.length(), winNtVer32);
        // Get next
        findPos = projectTemplate.find(winnt32Tag, findPos + 1);
    }
    const string winnt64Tag = "template_winnt64";
    findPos = projectTemplate.find(winnt64Tag);
    while (findPos != string::npos) {
        // Replace
        projectTemplate.replace(findPos, winnt64Tag.length(), winNtVer64);
        // Get next
        findPos = projectTemplate.find(winnt64Tag, findPos + 1);
    }
}

void ProjectGenerator::outputSourceFileType(StaticList& fileList, const string& type, const string& filterType,
    string& projectTemplate, string& filterTemplate, StaticList& foundObjects, set<string>& foundFilters,
    bool checkExisting, bool staticOnly, bool sharedOnly) const
{
    // Declare constant strings used in output files
    const string itemGroup = "\r\n  <ItemGroup>";
    const string itemGroupEnd = "\r\n  </ItemGroup>";
    const string includeClose = "\">";
    const string includeEnd = "\" />";
    const string typeInclude = "\r\n    <" + type + " Include=\"";
    const string typeIncludeEnd = "\r\n    </" + type + ">";
    const string includeObject = "\r\n      <ObjectFileName>$(IntDir)\\";
    const string includeObjectClose = ".obj</ObjectFileName>";
    const string filterSource = "\r\n      <Filter>" + filterType + " Files";
    const string source = filterType + " Files";
    const string filterEnd = "</Filter>";
    const string excludeConfig = "\r\n      <ExcludedFromBuild Condition=\"'$(Configuration)'=='";
    const string buildConfigsStatic[] = {"Release", "Debug", "ReleaseWinRT", "DebugWinRT"};
    const string buildConfigsShared[] = {"ReleaseDLL", "ReleaseDLLStaticDeps", "DebugDLL", "ReleaseDLLWinRT",
        "ReleaseDLLWinRTStaticDeps", "DebugDLLWinRT"};
    const string excludeConfigEnd = "'\">true</ExcludedFromBuild>";

    if (fileList.size() > 0) {
        string typeFiles = itemGroup;
        string typeFilesFilt = itemGroup;
        string typeFilesTemp, typeFilesFiltTemp;
        vector<pair<string, string>> tempObjects;

        for (const auto& i : fileList) {
            // Output objects
            typeFilesTemp = typeInclude;
            typeFilesFiltTemp = typeInclude;

            // Add the fileName
            string file = i;
            replace(file.begin(), file.end(), '/', '\\');
            typeFilesTemp += file;
            typeFilesFiltTemp += file;

            // Get object name without path or extension
            uint pos = i.rfind('/') + 1;
            string objectName = i.substr(pos);
            uint pos2 = objectName.rfind('.');
            objectName.resize(pos2);

            // Add the filters Filter
            string sourceDir;
            m_configHelper.makeFileProjectRelative(m_configHelper.m_rootDirectory, sourceDir);
            pos = i.rfind(sourceDir);
            pos = (pos == string::npos) ? 0 : pos + sourceDir.length();
            typeFilesFiltTemp += includeClose;
            typeFilesFiltTemp += filterSource;
            uint folderLength = i.rfind('/') - pos;
            if (static_cast<int>(folderLength) != -1) {
                string folderName = file.substr(pos, folderLength);
                folderName = '\\' + folderName;
                foundFilters.insert(source + folderName);
                typeFilesFiltTemp += folderName;
            }
            typeFilesFiltTemp += filterEnd;
            typeFilesFiltTemp += typeIncludeEnd;

            // Check if this file should be disabled under certain configurations
            bool closed = false;
            if (staticOnly || sharedOnly) {
                typeFilesTemp += includeClose;
                closed = true;
                const string* buildConfig = nullptr;
                uint configs = 0;
                if (staticOnly) {
                    buildConfig = buildConfigsShared;
                    configs = sizeof(buildConfigsShared) / sizeof(buildConfigsShared[0]);
                } else {
                    buildConfig = buildConfigsStatic;
                    configs = sizeof(buildConfigsStatic) / sizeof(buildConfigsStatic[0]);
                }
                if (!m_configHelper.isConfigOptionEnabled("WINRT") && !m_configHelper.isConfigOptionEnabled("UWP")) {
                    configs /= 2;
                }
                for (uint j = 0; j < configs; j++) {
                    typeFilesTemp += excludeConfig;
                    typeFilesTemp += buildConfig[j];
                    typeFilesTemp += excludeConfigEnd;
                }
            }

            // Several input source files have the same name so we need to explicitly specify an output object file
            // otherwise they will clash
            if (checkExisting && (find(foundObjects.begin(), foundObjects.end(), objectName) != foundObjects.end())) {
                objectName = i.substr(pos);
                replace(objectName.begin(), objectName.end(), '/', '_');
                // Replace the extension with obj
                pos2 = objectName.rfind('.');
                objectName.resize(pos2);
                if (!closed) {
                    typeFilesTemp += includeClose;
                }
                typeFilesTemp += includeObject;
                typeFilesTemp += objectName;
                typeFilesTemp += includeObjectClose;
                typeFilesTemp += typeIncludeEnd;
                // Add to temp list of stored objects
                tempObjects.emplace_back(typeFilesTemp, typeFilesFiltTemp);
            } else {
                foundObjects.push_back(objectName);
                // Close the current item
                if (!closed) {
                    typeFilesTemp += includeEnd;
                } else {
                    typeFilesTemp += typeIncludeEnd;
                }
                // Add to output
                typeFiles += typeFilesTemp;
                typeFilesFilt += typeFilesFiltTemp;
            }
        }

        // Add any temporary stored objects (This improves compile performance by grouping objects with different
        // compile options - in this case output name)
        for (const auto& i : tempObjects) {
            // Add to output
            typeFiles += i.first;
            typeFilesFilt += i.second;
        }

        typeFiles += itemGroupEnd;
        typeFilesFilt += itemGroupEnd;

        // After </ItemGroup> add the item groups for each of the include types
        string endTag =
            "</ItemGroup>"; // Uses independent string to sItemGroupEnd to avoid line ending errors due to \r\n
        uint findPos = projectTemplate.rfind(endTag);
        findPos += endTag.length();
        uint findPosFilt = filterTemplate.rfind(endTag);
        findPosFilt += endTag.length();

        // Insert into output file
        projectTemplate.insert(findPos, typeFiles);
        filterTemplate.insert(findPosFilt, typeFilesFilt);
    }
}

void ProjectGenerator::outputSourceFiles(string& projectTemplate, string& filterTemplate)
{
    set<string> foundFilters;
    StaticList foundObjects;

    // Check if there is a resource file
    string resourceFile;
    if (findSourceFile(m_projectName.substr(3) + "res", ".rc", resourceFile)) {
        m_configHelper.makeFileProjectRelative(resourceFile, resourceFile);
        StaticList resources;
        resources.push_back(resourceFile);
        outputSourceFileType(resources, "ResourceCompile", "Resource", projectTemplate, filterTemplate, foundObjects,
            foundFilters, false, false, true);
    }

    // Output ASM files in specific item group (must go first as asm does not allow for custom obj filename)
    if (!m_includesASM.empty()) {
        if (m_configHelper.isASMEnabled()) {
            outputSourceFileType(m_includesASM, (m_configHelper.m_useNASM) ? "NASM" : "YASM", "Source", projectTemplate,
                filterTemplate, foundObjects, foundFilters, false);
        } else {
            outputError("Assembly files found in project but assembly is disabled");
        }
    }

    // Output C files
    outputSourceFileType(
        m_includesC, "ClCompile", "Source", projectTemplate, filterTemplate, foundObjects, foundFilters, true);

    // Output C++ files
    outputSourceFileType(
        m_includesCPP, "ClCompile", "Source", projectTemplate, filterTemplate, foundObjects, foundFilters, true);

    // Output CUDA files
    if (!m_includesCU.empty()) {
        if (m_configHelper.isCUDAEnabled()) {
            // outputSourceFileType(
            //    m_includesCU, "CudaCompile", "Source", projectTemplate, filterTemplate, foundObjects, foundFilters,
            //    true);
            outputError("CUDA files detected in project. CUDA compilation is not currently supported");
        } else {
            outputError("CUDA files found in project but CUDA is disabled");
        }
    }

    // Output header files in new item group
    outputSourceFileType(
        m_includesH, "ClInclude", "Header", projectTemplate, filterTemplate, foundObjects, foundFilters, false);

    // Add any additional Filters to filters file
    const string itemGroupEnd = "\r\n  </ItemGroup>";
    const string filterAdd = "\r\n    <Filter Include=\"";
    const string filterAdd2 = "\">\r\n\
      <UniqueIdentifier>{";
    const string filterAddClose = "}</UniqueIdentifier>\r\n\
    </Filter>";
    const string filterKeys[] = {"cac6df1e-4a60-495c-8daa-5707dc1216ff", "9fee14b2-1b77-463a-bd6b-60efdcf8850f",
        "bf017c32-250d-47da-b7e6-d5a5091cb1e6", "fd9e10e9-18f6-437d-b5d7-17290540c8b8",
        "f026e68e-ff14-4bf4-8758-6384ac7bcfaf", "a2d068fe-f5d5-4b6f-95d4-f15631533341",
        "8a4a673d-2aba-4d8d-a18e-dab035e5c446", "0dcfb38d-54ca-4ceb-b383-4662f006eca9",
        "57bf1423-fb68-441f-b5c1-f41e6ae5fa9c"};

    // get start position in file
    uint findPosFilt = filterTemplate.find("</ItemGroup>");
    findPosFilt = filterTemplate.find_last_not_of(g_whiteSpace, findPosFilt - 1) +
        1; // handle potential differences in line endings
    uint currentKey = 0;
    string addFilters;
    for (const auto& i : foundFilters) {
        addFilters += filterAdd;
        addFilters += i;
        addFilters += filterAdd2;
        addFilters += filterKeys[currentKey];
        currentKey++;
        addFilters += filterAddClose;
    }
    // Add to string
    filterTemplate.insert(findPosFilt, addFilters);
}

bool ProjectGenerator::outputProjectExports(const StaticList& includeDirs)
{
    outputLine("  Generating project exports file (" + m_projectName + ")...");
    string exportList;
    if (!findFile(this->m_projectDir + "/*.v", exportList)) {
        outputError("Failed finding project exports (" + m_projectName + ")");
        return false;
    }

    // Open the input export file
    string exportsFile;
    loadFromFile(this->m_projectDir + exportList, exportsFile);

    // Search for start of global tag
    string global = "global:";
    StaticList exportStrings;
    uint findPos = exportsFile.find(global);
    if (findPos != string::npos) {
        // Remove everything outside the global section
        findPos += global.length();
        uint findPos2 = exportsFile.find("local:", findPos);
        exportsFile = exportsFile.substr(findPos, findPos2 - findPos);

        // Remove any comments
        findPos = exportsFile.find('#');
        while (findPos != string::npos) {
            // find end of line
            findPos2 = exportsFile.find(10, findPos + 1); // 10 is line feed
            exportsFile.erase(findPos, findPos2 - findPos + 1);
            findPos = exportsFile.find('#', findPos + 1);
        }

        // Clean any remaining white space out
        exportsFile.erase(remove_if(exportsFile.begin(), exportsFile.end(), isspace), exportsFile.end());

        // Get any export strings
        findPos = 0;
        findPos2 = exportsFile.find(';');
        while (findPos2 != string::npos) {
            exportStrings.push_back(exportsFile.substr(findPos, findPos2 - findPos));
            findPos = findPos2 + 1;
            findPos2 = exportsFile.find(';', findPos);
        }
    } else {
        outputError("Failed finding global start in project exports (" + exportList + ")");
        return false;
    }

    // Split each source file into different directories to avoid name clashes
    map<string, StaticList> directoryObjects;
    for (const auto& i : m_includesC) {
        // Several input source files have the same name so we need to explicitly specify an output object file
        // otherwise they will clash
        uint pos = i.rfind("../");
        pos = (pos == string::npos) ? 0 : pos + 3;
        uint pos2 = i.rfind('/');
        pos2 = (pos2 == string::npos) ? string::npos : pos2 - pos;
        string folderName = i.substr(pos, pos2);
        directoryObjects[folderName].push_back(i);
    }
    for (const auto& i : m_includesCPP) {
        // Several input source files have the same name so we need to explicitly specify an output object file
        // otherwise they will clash
        uint pos = i.rfind("../");
        pos = (pos == string::npos) ? 0 : pos + 3;
        uint pos2 = i.rfind('/');
        pos2 = (pos2 == string::npos) ? string::npos : pos2 - pos;
        string folderName = i.substr(pos, pos2);
        directoryObjects[folderName].push_back(i);
    }

    if (!runCompiler(includeDirs, directoryObjects, 0)) {
        return false;
    }

    // Loaded in the compiler passed files
    StaticList filesSBR;
    StaticList moduleExports;
    StaticList moduleDataExports;
    string tempFolder = m_tempDirectory + m_projectName;
    findFiles(tempFolder + "/*.sbr", filesSBR);
    for (const auto& i : filesSBR) {
        string fileSBR;
        loadFromFile(i, fileSBR, true);

        // Search through file for module exports
        for (const auto& j : exportStrings) {
            // SBR files contain data in specif formats
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

            // Check if it is a wild card search
            findPos = j.find('*');
            if (findPos != string::npos) {
                // Strip the wild card (Note: assumes wild card is at the end!)
                string search = j.substr(0, findPos);

                // Search for all occurrences
                findPos = fileSBR.find(search);
                while (findPos != string::npos) {
                    // Find end of name signalled by NULL character
                    uint findPos2 = fileSBR.find(static_cast<char>(0x00), findPos + 1);
                    if (findPos2 == string::npos) {
                        findPos = findPos2;
                        break;
                    }

                    // Check if this is a define
                    uint findPos3 = fileSBR.rfind(static_cast<char>(0x00), findPos - 3);
                    while (fileSBR.at(findPos3 - 1) == static_cast<char>(0x00)) {
                        // Skip if there was a NULL in ID
                        --findPos3;
                    }
                    uint findPosDiff = findPos - findPos3;
                    if ((fileSBR.at(findPos3 - 1) == '@') &&
                        (((findPosDiff == 3) && (fileSBR.at(findPos3 - 3) == static_cast<char>(0x03))) ||
                            ((findPosDiff == 4) && (fileSBR.at(findPos3 - 3) == 'C')))) {
                        // Check if this is a data or function name
                        string foundName = fileSBR.substr(findPos, findPos2 - findPos);
                        if (fileSBR.at(findPos3 - 2) == static_cast<char>(0x01)) {
                            // This is a function
                            if (find(moduleExports.begin(), moduleExports.end(), foundName) == moduleExports.end()) {
                                moduleExports.push_back(foundName);
                            }
                        } else if (fileSBR.at(findPos3 - 2) == static_cast<char>(0x04)) {
                            // This is data
                            if (find(moduleDataExports.begin(), moduleDataExports.end(), foundName) ==
                                moduleDataExports.end()) {
                                moduleDataExports.push_back(foundName);
                            }
                        }
                    }

                    // Get next
                    findPos = fileSBR.find(search, findPos2 + 1);
                }
            } else {
                findPos = fileSBR.find(j);
                // Make sure the match is an exact one
                uint findPos3;
                while ((findPos != string::npos)) {
                    if (fileSBR.at(findPos + j.length()) == static_cast<char>(0x00)) {
                        findPos3 = fileSBR.rfind(static_cast<char>(0x00), findPos - 3);
                        while (fileSBR.at(findPos3 - 1) == static_cast<char>(0x00)) {
                            // Skip if there was a NULL in ID
                            --findPos3;
                        }
                        uint findPosDiff = findPos - findPos3;
                        if ((fileSBR.at(findPos3 - 1) == '@') &&
                            (((findPosDiff == 3) && (fileSBR.at(findPos3 - 3) == static_cast<char>(0x03))) ||
                                ((findPosDiff == 4) && (fileSBR.at(findPos3 - 3) == 'C')))) {
                            break;
                        }
                    }
                    findPos = fileSBR.find(j, findPos + 1);
                }
                if (findPos == string::npos) {
                    continue;
                }
                // Check if this is a data or function name
                if (fileSBR.at(findPos3 - 2) == static_cast<char>(0x01)) {
                    // This is a function
                    if (find(moduleExports.begin(), moduleExports.end(), j) == moduleExports.end()) {
                        moduleExports.push_back(j);
                    }
                } else if (fileSBR.at(findPos3 - 2) == static_cast<char>(0x04)) {
                    // This is data
                    if (find(moduleDataExports.begin(), moduleDataExports.end(), j) == moduleDataExports.end()) {
                        moduleDataExports.push_back(j);
                    }
                }
            }
        }
    }
    // Remove the test sbr files
    deleteFolder(tempFolder);

    // Check for any exported functions in asm files
    for (const auto& i : m_includesASM) {
        string fileASM;
        loadFromFile(m_configHelper.m_solutionDirectory + i, fileASM);

        // Search through file for module exports
        for (const auto& j : exportStrings) {
            // Check if it is a wild card search
            findPos = j.find('*');
            const string invalidChars = ",.(){}[]`'\"+-*/!@#$%^&*<>|;\\= \r\n\t";
            if (findPos != string::npos) {
                // Strip the wild card (Note: assumes wild card is at the end!)
                string search = ' ' + j.substr(0, findPos);
                // Search for all occurrences
                findPos = fileASM.find(search);
                while ((findPos != string::npos) && (findPos > 0)) {
                    // Find end of name signaled by first non valid character
                    uint findPos2 = fileASM.find_first_of(invalidChars, findPos + 1);
                    // Check this is valid function definition
                    if ((fileASM.at(findPos2) == '(') && (invalidChars.find(fileASM.at(findPos - 1)) == string::npos)) {
                        string foundName = fileASM.substr(findPos, findPos2 - findPos);
                        if (find(moduleExports.begin(), moduleExports.end(), foundName) == moduleExports.end()) {
                            moduleExports.push_back(foundName.substr(1));
                        }
                    }

                    // Get next
                    findPos = fileASM.find(search, findPos2 + 1);
                }
            } else {
                string search = ' ' + j + '(';
                findPos = fileASM.find(j);
                // Make sure the match is an exact one
                if ((findPos != string::npos) && (findPos > 0) &&
                    (invalidChars.find(fileASM.at(findPos - 1)) == string::npos)) {
                    // Check this is valid function definition
                    if (find(moduleExports.begin(), moduleExports.end(), j) == moduleExports.end()) {
                        moduleExports.push_back(j);
                    }
                }
            }
        }
    }

    // Sort the exports
    sort(moduleExports.begin(), moduleExports.end());
    sort(moduleDataExports.begin(), moduleDataExports.end());

    // Create the export module string
    string moduleFile = "EXPORTS\r\n";
    for (const auto& i : moduleExports) {
        moduleFile += "    " + i + "\r\n";
    }
    for (const auto& i : moduleDataExports) {
        moduleFile += "    " + i + " DATA\r\n";
    }

    string destinationFile = m_configHelper.m_solutionDirectory + m_projectName + ".def";
    if (!writeToFile(destinationFile, moduleFile, true)) {
        return false;
    }
    return true;
}

void ProjectGenerator::outputBuildEvents(string& projectTemplate)
{
    // After </Lib> and </Link> and the post and then pre build events
    const string libLink[2] = {"</Lib>", "</Link>"};
    const string postbuild = "\r\n    <PostBuildEvent>\r\n\
      <Command>";
    const string postbuildClose = "</Command>\r\n\
    </PostBuildEvent>";
    const string include = "mkdir \"$(OutDir)\"\\include\r\n\
mkdir \"$(OutDir)\"\\include\\";
    const string copy = "\r\ncopy ";
    const string copyEnd = " \"$(OutDir)\"\\include\\";
    const string license = "\r\nmkdir \"$(OutDir)\"\\licenses";
    string licenseName = m_configHelper.m_projectName;
    transform(licenseName.begin(), licenseName.end(), licenseName.begin(), tolower);
    const string licenseEnd = " \"$(OutDir)\"\\licenses\\" + licenseName + ".txt";
    const string prebuild = "\r\n    <PreBuildEvent>\r\n\
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
    const string prebuildDir = "\r\nif exist \"$(OutDir)\"\\include\\" + m_projectName + " (\r\n\
rd /s /q \"$(OutDir)\"\\include\\" +
        m_projectName + "\r\n\
cd ../\r\n\
cd $(ProjectDir)\r\n\
)";
    const string prebuildClose = "</Command>\r\n    </PreBuildEvent>";
    // Get the correct license file
    string licenseFile;
    if (m_configHelper.isConfigOptionEnabled("nonfree")) {
        licenseFile = "template_rootdirCOPYING.GPLv3"; // Technically this has no license as it is unredistributable
                                                       // but we get the closest thing for now
    } else if (m_configHelper.isConfigOptionEnabled("gplv3")) {
        licenseFile = "template_rootdirCOPYING.GPLv3";
    } else if (m_configHelper.isConfigOptionEnabled("lgplv3")) {
        licenseFile = "template_rootdirCOPYING.LGPLv3";
    } else if (m_configHelper.isConfigOptionEnabled("gpl")) {
        licenseFile = "template_rootdirCOPYING.GPLv2";
    } else {
        licenseFile = "template_rootdirCOPYING.LGPLv2.1";
    }
    // Generate the pre build and post build string
    string additional;
    // Add the post build event
    additional += postbuild;
    if (m_includesH.size() > 0) {
        additional += include;
        additional += m_projectName;
        for (auto& i : m_includesH) {
            additional += copy;
            string header = i;
            replace(header.begin(), header.end(), '/', '\\');
            additional += header;
            additional += copyEnd;
            additional += m_projectName;
        }
    }
    // Output license
    additional += license;
    additional += copy;
    additional += licenseFile;
    additional += licenseEnd;
    additional += postbuildClose;
    // Add the pre build event
    additional += prebuild;
    if (m_includesH.size() > 0) {
        additional += prebuildDir;
    }
    additional += prebuildClose;

    for (const auto& i : libLink) {
        uint findPos = projectTemplate.find(i);
        while (findPos != string::npos) {
            findPos += i.length();
            // Add to output
            projectTemplate.insert(findPos, additional);
            findPos += additional.length();
            // Get next
            findPos = projectTemplate.find(i, findPos + 1);
        }
    }
}

void ProjectGenerator::outputIncludeDirs(const StaticList& includeDirs, string& projectTemplate)
{
    if (!includeDirs.empty()) {
        string addInclude;
        for (const auto& i : includeDirs) {
            addInclude += i + ";";
        }
        replace(addInclude.begin(), addInclude.end(), '/', '\\');
        const string addIncludeDir = "<AdditionalIncludeDirectories>";
        uint findPos = projectTemplate.find(addIncludeDir);
        while (findPos != string::npos) {
            // Add to output
            findPos += addIncludeDir.length(); // Must be added first so that it is before $(IncludePath) as
                                               // otherwise there are errors
            projectTemplate.insert(findPos, addInclude);
            findPos += addInclude.length();
            // Get next
            findPos = projectTemplate.find(addIncludeDir, findPos + 1);
        }
    }
}

void ProjectGenerator::outputLibDirs(const StaticList& lib32Dirs, const StaticList& lib64Dirs, string& projectTemplate)
{
    if ((!lib32Dirs.empty()) || (!lib64Dirs.empty())) {
        // Add additional lib includes to include list based on current config
        string addLibs[2];
        for (const auto& i : lib32Dirs) {
            addLibs[0] += i + ";";
        }
        for (const auto& i : lib64Dirs) {
            addLibs[1] += i + ";";
        }
        replace(addLibs[0].begin(), addLibs[0].end(), '/', '\\');
        replace(addLibs[1].begin(), addLibs[1].end(), '/', '\\');
        const string addLibDir = "<AdditionalLibraryDirectories>";
        uint arch32Or64 = 0; // start with 32 (assumes projects are ordered 32 then 64 recursive)
        uint findPos = projectTemplate.find(addLibDir);
        while (findPos != string::npos) {
            // Add to output
            findPos += addLibDir.length();
            projectTemplate.insert(findPos, addLibs[arch32Or64]);
            findPos += addLibs[arch32Or64].length();
            // Get next
            findPos = projectTemplate.find(addLibDir, findPos + 1);
            arch32Or64 = !arch32Or64;
        }
    }
}

void ProjectGenerator::outputDefines(
    const StaticList& definesShared, const StaticList& definesStatic, string& projectTemplate, const bool program)
{
    if (!definesShared.empty() || !definesStatic.empty()) {
        string defines2Shared, defines2Static;
        for (const auto& i : definesShared) {
            defines2Shared += i + ";";
        }
        for (const auto& i : definesStatic) {
            defines2Static += i + ";";
        }
        // libraries:
        // Debug, DebugWinRT x2 (0-7), DebugDLL,DebugDLLWinRT x2 (8-15), Release, ReleaseWinRT x2, ReleaseDLL,
        // ReleaseDLLWinRT x2, ReleaseDLLStaticDeps, ReleaseDLLWinRTStaticDeps x2
        // Libraries have 2 PreprocessorDefinitions for each configuration due to NASM section
        // programs:
        // Debug x2, DebugDLL x2, Release x2, ReleaseDLL x2
        const string addDefines = "<PreprocessorDefinitions>";
        uint findPos = projectTemplate.find(addDefines);
        uint count = 0;
        const bool addWinrt =
            m_configHelper.isConfigOptionEnabled("winrt") || m_configHelper.isConfigOptionEnabled("uwp");
        const uint check = (!program) ? (addWinrt ? 8 : 4) : 2;
        while (findPos != string::npos) {
            // Add to output
            findPos += addDefines.length();
            if ((count / check) % 2 == 0) {
                projectTemplate.insert(findPos, defines2Static);
                findPos += defines2Static.length();
            } else {
                projectTemplate.insert(findPos, defines2Shared);
                findPos += defines2Shared.length();
            }
            // Get next
            findPos = projectTemplate.find(addDefines, findPos + 1);
            ++count;
        }
    }
}

void ProjectGenerator::outputASMTools(string& projectTemplate) const
{
    if (m_configHelper.isASMEnabled() && (m_includesASM.size() > 0)) {
        string definesASM = "\r\n\
    <NASM>\r\n\
      <IncludePaths>$(ProjectDir);$(ProjectDir)\\template_rootdir;$(ProjectDir)\\template_rootdir\\$(ProjectName)\\x86;%(IncludePaths)</IncludePaths>\r\n\
      <PreIncludeFiles>config.asm;%(PreIncludeFiles)</PreIncludeFiles>\r\n\
      <GenerateDebugInformation>false</GenerateDebugInformation>\r\n\
    </NASM>";
        string propeASM = "\r\n\
  <ImportGroup Label=\"ExtensionSettings\">\r\n\
    <Import Project=\"$(VCTargetsPath)\\BuildCustomizations\\nasm.props\" />\r\n\
  </ImportGroup>";
        string targetsASM = "\r\n\
  <ImportGroup Label=\"ExtensionTargets\">\r\n\
    <Import Project=\"$(VCTargetsPath)\\BuildCustomizations\\nasm.targets\" />\r\n\
  </ImportGroup>";
        if (!m_configHelper.m_useNASM) {
            // Replace nasm with yasm
            size_t n = 0;
            while ((n = definesASM.find("NASM", n)) != string::npos) {
                definesASM.replace(n, 4, "YASM");
                n += 4;
            }
            propeASM.replace(propeASM.find("nasm"), 4, "yasm");
            targetsASM.replace(targetsASM.find("nasm"), 4, "yasm");
        }
        const string findProps = R"(<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />)";
        const string findTargets = R"(<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />)";

        // Add NASM defines
        const string endPreBuild = "</PreBuildEvent>";
        uint findPos = projectTemplate.find(endPreBuild);
        while (findPos != string::npos) {
            findPos += endPreBuild.length();
            // Add to output
            projectTemplate.insert(findPos, definesASM);
            findPos += definesASM.length();
            // Get next
            findPos = projectTemplate.find(endPreBuild, findPos + 1);
        }

        // Add NASM build customisation
        findPos = projectTemplate.find(findProps) + findProps.length();
        // After <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" /> add asm props
        projectTemplate.insert(findPos, propeASM);
        findPos = projectTemplate.find(findTargets) + findTargets.length();
        // After <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" /> add asm target
        projectTemplate.insert(findPos, targetsASM);
    }
}

void ProjectGenerator::outputCUDATools(string& projectTemplate) const
{
    if (m_configHelper.isCUDAEnabled() && (m_includesCU.size() > 0)) {
        // TODO: Add cuda tools
    }
}

bool ProjectGenerator::outputDependencyLibs(string& projectTemplate, bool program)
{
    // Check current libs list for valid lib names
    for (auto& i : m_libs) {
        // prepend lib if needed
        if (i.find("lib") != 0) {
            i = "lib" + i;
        }
    }

    // Add additional dependencies based on current config to Libs list
    buildInterDependencies(m_libs);
    m_projectLibs[m_projectName] = m_libs; // Backup up current libs for solution
    StaticList addLibs;
    buildDependencies(m_libs, addLibs);
    StaticList libsWinRT;
    StaticList addLibsWinRT;
    for (auto vitLib = m_libs.begin() + m_projectLibs[m_projectName].size(); vitLib < m_libs.end(); ++vitLib) {
        libsWinRT.push_back(*vitLib);
    }
    buildDependenciesWinRT(libsWinRT, addLibsWinRT);

    if ((m_libs.size() > 0) || (addLibs.size() > 0)) {
        // Create list of additional ffmpeg dependencies
        string addFFmpegLibs[4]; // debug, release, debugDll, releaseDll
        for (const auto& i : m_projectLibs[m_projectName]) {
            addFFmpegLibs[0] += i;
            addFFmpegLibs[0] += "d.lib;";
            addFFmpegLibs[1] += i;
            addFFmpegLibs[1] += ".lib;";
            addFFmpegLibs[2] += i.substr(3);
            addFFmpegLibs[2] += "d.lib;";
            addFFmpegLibs[3] += i.substr(3);
            addFFmpegLibs[3] += ".lib;";
        }
        string addFFmpegLibsWinRT[4]; // debugWinRT, releaseWinRT, debugDllWinRT, releaseDllWinRT
        for (const auto& i : m_projectLibs[m_projectName]) {
            addFFmpegLibsWinRT[0] += i;
            addFFmpegLibsWinRT[0] += "d_winrt.lib;";
            addFFmpegLibsWinRT[1] += i;
            addFFmpegLibsWinRT[1] += "_winrt.lib;";
            addFFmpegLibsWinRT[2] += i.substr(3);
            addFFmpegLibsWinRT[2] += "d_winrt.lib;";
            addFFmpegLibsWinRT[3] += i.substr(3);
            addFFmpegLibsWinRT[3] += "_winrt.lib;";
        }
        // Create List of additional dependencies
        string addDeps[4]; // debug, release, debugDll, releaseDll
        for (auto i = m_libs.begin() + m_projectLibs[m_projectName].size(); i < m_libs.end(); ++i) {
            addDeps[0] += *i;
            addDeps[0] += "d.lib;";
            addDeps[1] += *i;
            addDeps[1] += ".lib;";
            addDeps[2] += i->substr(3);
            addDeps[2] += "d.lib;";
            addDeps[3] += i->substr(3);
            addDeps[3] += ".lib;";
        }
        string addDepsWinRT[4]; // debugWinRT, releaseWinRT, debugDllWinRT, releaseDllWinRT
        for (const auto& i : libsWinRT) {
            addDepsWinRT[0] += i;
            addDepsWinRT[0] += "d_winrt.lib;";
            addDepsWinRT[1] += i;
            addDepsWinRT[1] += "_winrt.lib;";
            addDepsWinRT[2] += i.substr(3);
            addDepsWinRT[2] += "d_winrt.lib;";
            addDepsWinRT[3] += i.substr(3);
            addDepsWinRT[3] += "_winrt.lib;";
        }
        // Create List of additional external dependencies
        string addExternDeps;
        for (const auto& i : addLibs) {
            addExternDeps += i;
            addExternDeps += ".lib;";
        }
        string addExternDepsWinRT;
        for (const auto& i : addLibsWinRT) {
            addExternDepsWinRT += i;
            addExternDepsWinRT += ".lib;";
        }
        // Check if winrt builds are enabled
        bool addWinrt = m_configHelper.isConfigOptionEnabled("winrt") || m_configHelper.isConfigOptionEnabled("uwp");
        // Add to Additional Dependencies
        string libLink2[2] = {"<Link>", "<Lib>"};
        for (uint linkLib = 0; linkLib < (!program ? 2 : 1); linkLib++) {
            // loop over each debug/release sequence
            uint findPos = projectTemplate.find(libLink2[linkLib]);
            for (uint debugRelease = 0; debugRelease < 2; debugRelease++) {
                uint max = !program ? (((debugRelease == 1) && (linkLib == 0)) ? 2 : 1) : 2;
                // Libs have:
                // link:
                //  DebugDLL|Win32, DebugDLLWinRT|Win32, DebugDLL|x64, DebugDLLWinRT|x64,
                //  ReleaseDLL|Win32, ReleaseDLLWinRT|Win32, ReleaseDLL|x64, ReleaseDLLWinRT|x64,
                //  ReleaseDLLStaticDeps|Win32, ReleaseDLLWinRTStaticDeps|Win32, ReleaseDLLStaticDeps|x64,
                //  ReleaseDLLWinRTStaticDeps|x64
                // lib:
                //  Debug32, DebugWinRT|Win32, Debug|x64, DebugWinRT|x64,
                //  Release|Win32, ReleaseWinRT|Win32, Release|x64, ReleaseWinRT|x64,
                // Programs have:
                // link:
                //  Debug32, Debug|x64,
                //  DebugDLL|Win32, DebugDLL|x64,
                //  Release|Win32, Release|x64,
                //  ReleaseDLL|Win32, ReleaseDLL|x64,
                for (uint conf = 0; conf < max; conf++) {
                    // Loop over x32/x64
                    for (uint arch = 0; arch < 2; arch++) {
                        // Loop over any WinRT configs
                        for (uint win = 0; win < ((!program && addWinrt) ? 2 : 1); win++) {
                            findPos = projectTemplate.find("%(AdditionalDependencies)", findPos);
                            if (findPos == string::npos) {
                                outputError("Failed finding %(AdditionalDependencies) in template.");
                                return false;
                            }
                            // Add in ffmpeg inter-dependencies
                            uint addIndex = debugRelease;
                            if ((linkLib == 0) && (!program || (conf % 2 != 0))) {
                                // Use DLL libs
                                addIndex += 2;
                            }
                            string addString;
                            // If the dependency is actually for one of the ffmpeg libs then we can ignore it in
                            // static linking mode as this just causes unnecessary code bloat
                            if (linkLib == 0) {
                                if (win == 0) {
                                    addString = addFFmpegLibs[addIndex];
                                } else {
                                    addString = addFFmpegLibsWinRT[addIndex];
                                }
                            }
                            // Add in normal dependencies
                            addIndex = debugRelease;
                            if ((linkLib == 0) && (((!program) && (conf < 1)) || (program && (conf % 2 != 0)))) {
                                // Use DLL libs
                                addIndex += 2;
                            }
                            if (win == 0) {
                                addString += addDeps[addIndex];
                                addString += addExternDeps;
                            } else {
                                addString += addDepsWinRT[addIndex];
                                addString += addExternDepsWinRT;
                            }
                            projectTemplate.insert(findPos, addString);
                            findPos += addString.length();
                            // Get next
                            findPos = projectTemplate.find(libLink2[linkLib], findPos + 1);
                        }
                    }
                }
            }
        }
    }
    return true;
}

void ProjectGenerator::outputStripWinRT(string& projectTemplate)
{
    // Search through template for all instances of WinRT
    const string search = "WinRT";
    uint found = projectTemplate.find(search);
    while (found != string::npos) {
        // Skip erroneous detections
        if (projectTemplate[found + search.length()] == '>') {
            // Find next occurence
            found = projectTemplate.find(search, found + 1);
            continue;
        }
        // Backward search for start of section
        uint startPos = projectTemplate.rfind('<', found);
        startPos = projectTemplate.find_last_of(g_endLine, startPos - 1) + 1;
        // Loop from start until we find the end tag for that section
        uint sectionCount = 1;
        uint endPos = found;
        while (true) {
            endPos = projectTemplate.find_first_of("</", endPos + 1);
            if (projectTemplate[endPos] == '<') {
                ++sectionCount;
            } else {
                if (projectTemplate[endPos + 1] == '>') {
                    --sectionCount;
                } else if (projectTemplate[endPos - 1] == '<') {
                    sectionCount -= 2;
                }
                if (sectionCount == 0) {
                    // Move to end of closing tag
                    endPos = projectTemplate.find('>', endPos + 1);
                    break;
                }
            }
        }
        // Remove the found section
        projectTemplate.erase(startPos, endPos - startPos + 1);
        // Cleanup any left over empty lines
        while (g_endLine.find(projectTemplate[startPos]) != string::npos) {
            projectTemplate.erase(startPos, 1);
        }
        // Find next occurence
        found = projectTemplate.find(search, startPos);
    }
}

void ProjectGenerator::outputStripWinRTSolution(string& solutionFile)
{
    // Search through template for all instances of WinRT
    const string search = "WinRT";
    uint found = solutionFile.find(search);
    while (found != string::npos) {
        // Remove the entire line
        const uint start = solutionFile.find_last_of(g_endLine, found - 1);
        const uint end = solutionFile.find_first_of(g_endLine, start + 1);
        solutionFile.erase(start, end - start + 1);

        // Find next occurence
        found = solutionFile.find(search, found);
    }
}
