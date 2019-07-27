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
#include <iterator>
#include <sstream>
#include <utility>

// This can be used to force all detected DCE values to be output to file
// whether they are enabled in current configuration or not
#define FORCEALLDCE 0
static const string g_tagsDCE[] = {"ARCH_", "HAVE_", "CONFIG_", "EXTERNAL_", "INTERNAL_", "INLINE_"};

bool ProjectGenerator::outputProjectDCE(const StaticList& includeDirs)
{
    outputLine("  Generating missing DCE symbols (" + m_projectName + ")...");
    // Create list of source files to scan
#if !FORCEALLDCE
    StaticList searchFiles = m_includesC;
    searchFiles.insert(searchFiles.end(), m_includesCPP.begin(), m_includesCPP.end());
    searchFiles.insert(searchFiles.end(), m_includesH.begin(), m_includesH.end());
#else
    StaticList searchFiles;
    bool recurse = (m_projectDir.compare(this->m_configHelper.m_rootDirectory) != 0);
    findFiles(m_projectDir + "*.h", searchFiles, recurse);
    findFiles(m_projectDir + "*.c", searchFiles, recurse);
    findFiles(m_projectDir + "*.cpp", searchFiles, recurse);
#endif
    // Ensure we can add extra items to the list without needing reallocs
    if (searchFiles.capacity() < searchFiles.size() + 250) {
        searchFiles.reserve(searchFiles.size() + 250);
    }

    // Check for DCE constructs
    map<string, DCEParams> foundDCEUsage;
    set<string> nonDCEUsage;
    StaticList preProcFiles;
    // Search through each included file
    for (auto itFile = searchFiles.begin(); itFile < searchFiles.end(); ++itFile) {
        // Open the input file
        m_configHelper.makeFileGeneratorRelative(*itFile, *itFile);
        string file;
        if (!loadFromFile(*itFile, file)) {
            return false;
        }
        bool requiresPreProcess = false;
        outputProjectDCEFindFunctions(file, *itFile, foundDCEUsage, requiresPreProcess, nonDCEUsage);
        if (requiresPreProcess) {
            preProcFiles.push_back(*itFile);
        }

        // Check if this file includes additional source files
        uint findPos = file.find(".c\"");
        while (findPos != string::npos) {
            // Check if this is an include
            uint findPos2 = file.rfind("#include \"", findPos);
            if ((findPos2 != string::npos) && (findPos - findPos2 < 50)) {
                // Get the name of the file
                findPos2 += 10;
                findPos += 2;
                string templateFile = file.substr(findPos2, findPos - findPos2);
                // check if file contains current project
                uint projName = templateFile.find(m_projectName);
                if (projName != string::npos) {
                    templateFile = templateFile.substr(projName + m_projectName.length() + 1);
                }
                string found;
                string back = templateFile;
                templateFile = m_projectDir + back;
                if (!findFile(templateFile, found)) {
                    templateFile = (m_configHelper.m_rootDirectory.length() > 0) ?
                        m_configHelper.m_rootDirectory + '/' + back :
                        back;
                    if (!findFile(templateFile, found)) {
                        templateFile = m_configHelper.m_solutionDirectory + m_projectName + '/' + back;
                        if (!findFile(templateFile, found)) {
                            templateFile = itFile->substr(0, itFile->rfind('/') + 1) + back;
                            if (!findFile(templateFile, found)) {
                                outputError("Failed to find included file " + back);
                                return false;
                            }
                        }
                    }
                }
                // Add the file to the list
                if (find(searchFiles.begin(), searchFiles.end(), templateFile) == searchFiles.end()) {
                    m_configHelper.makeFileProjectRelative(templateFile, templateFile);
                    searchFiles.push_back(templateFile);
                }
            }
            // Check for more
            findPos = file.find(".c\"", findPos + 1);
        }
    }
#if !FORCEALLDCE
    // Get a list of all files in current project directory (including subdirectories)
    bool recurse = (m_projectDir != this->m_configHelper.m_rootDirectory);
    searchFiles.resize(0);
    findFiles(m_projectDir + "*.h", searchFiles, recurse);
    findFiles(m_projectDir + "*.c", searchFiles, recurse);
    findFiles(m_projectDir + "*.cpp", searchFiles, recurse);
    // Ensure we can add extra items to the list without needing reallocs
    if (searchFiles.capacity() < searchFiles.size() + 250) {
        searchFiles.reserve(searchFiles.size() + 250);
    }

    // Check all configurations are enabled early to avoid later lookups of unused functions
    for (auto i = foundDCEUsage.begin(); i != foundDCEUsage.end();) {
        outputProgramDCEsResolveDefine(i->second.define);
        if (i->second.define == "1") {
            nonDCEUsage.insert(i->first);
            // remove from the list
            foundDCEUsage.erase(i++);
        } else {
            ++i;
        }
    }
#endif

    // Now we need to find the declaration of each function
    map<string, DCEParams> foundDCEFunctions;
    map<string, DCEParams> foundDCEVariables;
    if (!foundDCEUsage.empty()) {
        // Search through each included file
        for (const auto& i : searchFiles) {
            string file;
            if (!loadFromFile(i, file)) {
                return false;
            }
            for (auto itDCE = foundDCEUsage.begin(); itDCE != foundDCEUsage.end();) {
                string return2;
                bool isFunc;
                if (outputProjectDCEsFindDeclarations(file, itDCE->first, i, return2, isFunc)) {
                    // Get the declaration file
                    string fileName;
                    makePathsRelative(i, m_configHelper.m_rootDirectory, fileName);
                    if (fileName.at(0) == '.') {
                        fileName = fileName.substr(2);
                    }
                    if (isFunc) {
                        foundDCEFunctions[return2] = {itDCE->second.define, fileName};
                    } else {
                        foundDCEVariables[return2] = {itDCE->second.define, fileName};
                    }

                    // Remove it from the list
                    foundDCEUsage.erase(itDCE++);
                } else {
                    // Only increment the iterator when nothing has been found
                    // when we did find something we erased a value from the list so the iterator is still valid
                    ++itDCE;
                }
            }
        }
    }

    // Add any files requiring pre-processing to unfound list
    for (const auto& i : preProcFiles) {
        foundDCEUsage[i] = {"#", i};
    }

    // Check if we failed to find any functions
    if (!foundDCEUsage.empty()) {
        vector<string> includeDirs2 = includeDirs;
        string tempFolder = m_tempDirectory + m_projectName;
        if (!makeDirectory(m_tempDirectory) || !makeDirectory(tempFolder)) {
            outputError("Failed to create temporary working directory (" + tempFolder + ")");
            return false;
        }
        // Get all the files that include functions
        map<string, vector<DCEParams>> functionFiles;
        // Remove project dir from start of file
        string projectDirCut;
        makePathsRelative(m_projectDir, m_configHelper.m_rootDirectory, projectDirCut);
        projectDirCut = (projectDirCut.find('.') == 0) ? projectDirCut.substr(2) : projectDirCut;
        for (auto& i : foundDCEUsage) {
            // Make source file relative to root
            string file;
            makePathsRelative(i.second.file, m_configHelper.m_rootDirectory, file);
            file = (file.find('.') == 0) ? file.substr(2) : file;
            uint pos = (projectDirCut.length() > 0) ? file.find(projectDirCut) : string::npos;
            pos = (pos == string::npos) ? 0 : pos + projectDirCut.length();
            file = tempFolder + '/' + file.substr(pos);
            if (file.find('/', tempFolder.length() + 1) != string::npos) {
                string sFolder = file.substr(0, file.rfind('/'));
                if (!makeDirectory(sFolder)) {
                    outputError("Failed to create temporary working sub-directory (" + sFolder + ")");
                    return false;
                }
            }
            // Copy file to local working directory
            if (!copyFile(i.second.file, file)) {
                outputError(
                    "Failed to copy dce file (" + i.second.file + ") to temporary directory (" + file + ")");
                return false;
            }
            functionFiles[file].push_back({i.second.define, i.first});
        }
        map<string, vector<string>> directoryObjects;
        const string tags2[] = {"if (", "if(", "& ", "&", "| ", "|"};
        for (auto& i : functionFiles) {
            // Get subdirectory
            string subFolder;
            if (i.first.find('/', tempFolder.length() + 1) != string::npos) {
                subFolder = m_projectDir +
                    i.first.substr(
                        tempFolder.length() + 1, i.first.rfind('/') - tempFolder.length() - 1);
                if (find(includeDirs2.begin(), includeDirs2.end(), subFolder) == includeDirs2.end()) {
                    // Need to add subdirectory to include list
                    includeDirs2.push_back(subFolder);
                }
                subFolder = subFolder.substr(m_projectDir.length());
            }
            directoryObjects[subFolder].push_back(i.first);
            // Modify existing tags so that they are still valid after preprocessing
            string file;
            if (!loadFromFile(i.first, file)) {
                return false;
            }
            for (const auto& j : g_tagsDCE) {
                for (const auto& k : tags2) {
                    const string search = k + j;
                    // Search for all occurrences
                    uint findPos = 0;
                    string replace = k + "XXX" + j;
                    while ((findPos = file.find(search, findPos)) != string::npos) {
                        file.replace(findPos, search.length(), replace);
                        findPos += replace.length() + 1;
                    }
                }
            }
            if (!writeToFile(i.first, file)) {
                return false;
            }
        }
        // Add current directory to include list (must be done last to ensure correct include order)
        if (find(includeDirs2.begin(), includeDirs2.end(), m_projectDir) == includeDirs2.end()) {
            includeDirs2.push_back(m_projectDir);
        }
        if (!runCompiler(includeDirs2, directoryObjects, 1)) {
            return false;
        }
        // Check the file that the function usage was found in to see if it was declared using macro expansion
        for (auto& i : functionFiles) {
            string file = i.first;
            file.at(file.length() - 1) = 'i';
            if (!loadFromFile(file, file)) {
                return false;
            }

            // Restore the initial macro names
            for (const auto& j : g_tagsDCE) {
                for (const auto& k : tags2) {
                    const string search = k + j;
                    // Search for all occurrences
                    uint findPos = 0;
                    string find = k + "XXX" + j;
                    while ((findPos = file.find(find, findPos)) != string::npos) {
                        file.replace(findPos, find.length(), search);
                        findPos += search.length() + 1;
                    }
                }
            }

            // Check for any un-found function usage
            map<string, DCEParams> newDCEUsage;
            bool canIgnore = false;
            outputProjectDCEFindFunctions(file, i.first, newDCEUsage, canIgnore, nonDCEUsage);
#if !FORCEALLDCE
            for (auto j = newDCEUsage.begin(); j != newDCEUsage.end();) {
                outputProgramDCEsResolveDefine(j->second.define);
                if (j->second.define == "1") {
                    nonDCEUsage.insert(j->first);
                    // remove from the list
                    newDCEUsage.erase(j++);
                } else {
                    ++j;
                }
            }
#endif
            for (auto& j : newDCEUsage) {
                // Add the file to the list
                if (find(i.second.begin(), i.second.end(), j.first) ==
                    i.second.end()) {
                    i.second.push_back({j.second.define, j.first});
                    foundDCEUsage[j.first] = j.second;
                }
            }

            // Search through each function in the current file
            for (const auto& j : i.second) {
                if (j.define != "#") {
                    string return2;
                    bool isFunc;
                    if (outputProjectDCEsFindDeclarations(file, j.file, i.first, return2, isFunc)) {
                        // Get the declaration file
                        string fileName;
                        makePathsRelative(i.first, m_configHelper.m_rootDirectory, fileName);
                        if (fileName.at(0) == '.') {
                            fileName = fileName.substr(2);
                        }
                        // Add the declaration (ensure not to stomp a function found before needing pre-processing)
                        if (isFunc) {
                            if (foundDCEFunctions.find(return2) == foundDCEFunctions.end()) {
                                foundDCEFunctions[return2] = {j.define, fileName};
                            }
                        } else {
                            if (foundDCEVariables.find(return2) == foundDCEVariables.end()) {
                                foundDCEVariables[return2] = {j.define, fileName};
                            }
                        }
                        // Remove the function from list
                        foundDCEUsage.erase(j.file);
                    }
                } else {
                    // Remove the function from list as it was just a preproc file
                    foundDCEUsage.erase(j.file);
                }
            }
        }

        // Delete the created temp files
        deleteFolder(m_tempDirectory);
    }

    // Get any required hard coded values
    map<string, DCEParams> builtDCEFunctions;
    map<string, DCEParams> builtDCEVariables;
    buildProjectDCEs(builtDCEFunctions, builtDCEVariables);
    for (auto& i : builtDCEFunctions) {
        // Add to found list if not already found
        if (foundDCEFunctions.find(i.first) == foundDCEFunctions.end()) {
#if !FORCEALLDCE
            outputProgramDCEsResolveDefine(i.second.define);
            if (i.second.define == "1") {
                nonDCEUsage.insert(i.first);
            } else {
                foundDCEFunctions[i.first] = i.second;
            }
#else
            foundDCEFunctions[i.first] = i.second;
#endif
        }
        // Remove from unfound list
        if (foundDCEUsage.find(i.first) == foundDCEUsage.end()) {
            foundDCEUsage.erase(i.first);
        }
    }
    for (auto& i : builtDCEVariables) {
        // Add to found list if not already found
        if (foundDCEVariables.find(i.first) == foundDCEVariables.end()) {
            const string name = i.first.substr(i.first.rfind(' ') + 1);
            if (nonDCEUsage.find(name) == nonDCEUsage.end()) {
#if !FORCEALLDCE
                outputProgramDCEsResolveDefine(i.second.define);
                if (i.second.define == "1") {
                    nonDCEUsage.insert(i.first);
                } else {
                    foundDCEVariables[i.first] = i.second;
                }
#else
                mFoundDCEVariables[i.first] = i.second;
#endif
            }
        }
        // Remove from unfound list
        if (foundDCEUsage.find(i.first) == foundDCEUsage.end()) {
            foundDCEUsage.erase(i.first);
        }
    }

    // Check if we failed to find anything (even after using buildDCEs)
    if (!foundDCEUsage.empty()) {
        for (const auto& i : foundDCEUsage) {
            outputInfo("Failed to find function definition for " + i.first + ", " + i.second.file);
            // Just output a blank definition and hope it works
            foundDCEFunctions["void " + i.first + "()"] = {i.second.define, i.second.file};
        }
    }

    // Add definition to new file
    if ((!foundDCEFunctions.empty()) || (!foundDCEVariables.empty())) {
        vector<DCEParams> includedHeaders;
        string outFile;
        // Loop through all functions
        for (auto& i : foundDCEFunctions) {
            bool usePreProc =
                (i.second.define.length() > 1) && (i.second.define != "0");
            if (usePreProc) {
                outFile += "#if !(" + i.second.define + ")\n";
            }
            if (i.second.file.find(".h") != string::npos) {
                // Include header files only once
                if (!usePreProc) {
                    i.second.define = "";
                }
                auto header = find(includedHeaders.begin(), includedHeaders.end(), i.second.file);
                if (header == includedHeaders.end()) {
                    includedHeaders.push_back({i.second.define, i.second.file});
                } else {
                    outputProgramDCEsCombineDefine(header->define, i.second.define, header->define);
                }
            }
            // Check to ensure the function correctly declares parameter names.
            string function = i.first;
            uint pos = function.find('(');
            uint count = 0;
            while (pos != string::npos) {
                uint pos2 = function.find(',', pos + 1);
                uint posBack = pos2;
                pos2 = (pos2 != string::npos) ? pos2 : function.rfind(')');
                pos2 = function.find_last_not_of(g_whiteSpace, pos2 - 1);
                // Check the type of the last tag in case it is only a type name
                bool needsName = false;
                string param = function.substr(pos + 1, pos2 - pos);
                if (param.back() == '*') {
                    needsName = true;
                } else {
                    // Split parameter string up and ensure there are at least a type and a name
                    istringstream ss(param);
                    vector<string> tokens{istream_iterator<string>{ss}, istream_iterator<string>{}};
                    if (*tokens.begin() == "const") {
                        tokens.erase(tokens.begin());
                    }
                    if (tokens.size() >= 2) {
                        if ((tokens.at(1) == "int") || (tokens.at(1) == "long")) {
                            tokens.erase(tokens.begin());
                        }
                    }
                    if (tokens.size() < 2) {
                        needsName = true;
                    }
                }
                if (needsName && (param.find("void") != 0)) {
                    ++count;
                    string insert = " param" + to_string(count);
                    function.insert(pos2 + 1, insert);
                    posBack = (posBack != string::npos) ? posBack + insert.length() : posBack;
                }
                // Get next
                pos = posBack;
            }
            outFile += function + " {";
            // Need to check return type
            string return2 = function.substr(0, function.find_first_of(g_whiteSpace));
            if (return2 == "void") {
                outFile += "return;";
            } else if (return2 == "int") {
                outFile += "return 0;";
            } else if (return2 == "unsigned") {
                outFile += "return 0;";
            } else if (return2 == "long") {
                outFile += "return 0;";
            } else if (return2 == "short") {
                outFile += "return 0;";
            } else if (return2 == "float") {
                outFile += "return 0.0f;";
            } else if (return2 == "double") {
                outFile += "return 0.0;";
            } else if (return2.find('*') != string::npos) {
                outFile += "return 0;";
            } else {
                outFile += "return *(" + return2 + "*)(0);";
            }
            outFile += "}\n";
            if (usePreProc) {
                outFile += "#endif\n";
            }
        }

        // Loop through all variables
        for (auto& i : foundDCEVariables) {
            bool usePreProc = true;
            bool enabled = false;
#if !FORCEALLDCE
            usePreProc = (i.second.define.length() > 1) && (i.second.define != "0");
            auto configOpt = m_configHelper.getConfigOptionPrefixed(i.second.define);
            if (configOpt == m_configHelper.m_configValues.end()) {
                // This config option doesn't exist so it potentially requires the header file to be included first
            } else {
                bool bReserved = (m_configHelper.m_replaceList.find(configOpt->m_prefix + configOpt->m_option) !=
                    m_configHelper.m_replaceList.end());
                if (!bReserved) {
                    enabled = (configOpt->m_value == "1");
                }
                usePreProc = usePreProc || bReserved;
            }
#endif
            // Include only those options that are currently disabled
            if (!enabled) {
                // Only include preprocessor guards if its a reserved option
                if (usePreProc) {
                    outFile += "#if !(" + i.second.define + ")\n";
                } else {
                    i.second.define = "";
                }
                // Include header files only once
                auto header = find(includedHeaders.begin(), includedHeaders.end(), i.second.file);
                if (header == includedHeaders.end()) {
                    includedHeaders.push_back({i.second.define, i.second.file});
                } else {
                    outputProgramDCEsCombineDefine(header->define, i.second.define, header->define);
                }
                outFile += "const " + i.first + " = {0};\n";
                if (usePreProc) {
                    outFile += "#endif\n";
                }
            }
        }
        string finalDCEOutFile = getCopywriteHeader(m_projectName + " DCE definitions") + '\n';
        finalDCEOutFile += "\n#include \"config.h\"\n#include \"stdint.h\"\n\n";
        // Add all header files (goes backwards to avoid bug in header include order in avcodec between vp9.h and
        // h264pred.h)
        for (auto i = includedHeaders.end(); i > includedHeaders.begin();) {
            --i;
            if (i->define.length() > 0) {
                finalDCEOutFile += "#if !(" + i->define + ")\n";
            }
            finalDCEOutFile += "#include \"" + i->file + "\"\n";
            if (i->define.length() > 0) {
                finalDCEOutFile += "#endif\n";
            }
        }
        finalDCEOutFile += '\n' + outFile;

        // Output the new file
        string outName = m_configHelper.m_solutionDirectory + '/' + m_projectName + '/' + "dce_defs.c";
        writeToFile(outName, finalDCEOutFile);
        m_configHelper.makeFileProjectRelative(outName, outName);
        m_includesC.push_back(outName);
    }
    return true;
}

void ProjectGenerator::outputProjectDCEFindFunctions(const string& file, const string& fileName,
    map<string, DCEParams>& foundDCEUsage, bool& requiresPreProcess, set<string>& nonDCEUsage) const
{
    const string tags2[] = {"if (", "if(", "if ((", "if(("};
    StaticList funcIdents = {"ff_"};
    if ((m_projectName == "ffmpeg") || (m_projectName == "ffplay") || (m_projectName == "ffprobe") ||
        (m_projectName == "avconv") || (m_projectName == "avplay") || (m_projectName == "avprobe")) {
        funcIdents.push_back("avcodec_");
        funcIdents.push_back("avdevice_");
        funcIdents.push_back("avfilter_");
        funcIdents.push_back("avformat_");
        funcIdents.push_back("avutil_");
        funcIdents.push_back("av_");
        funcIdents.push_back("avresample_");
        funcIdents.push_back("postproc_");
        funcIdents.push_back("swri_");
        funcIdents.push_back("swresample_");
        funcIdents.push_back("swscale_");
    } else if (m_projectName == "libavcodec") {
        funcIdents.push_back("avcodec_");
    } else if (m_projectName == "libavdevice") {
        funcIdents.push_back("avdevice_");
    } else if (m_projectName == "libavfilter") {
        funcIdents.push_back("avfilter_");
    } else if (m_projectName == "libavformat") {
        funcIdents.push_back("avformat_");
    } else if (m_projectName == "libavutil") {
        funcIdents.push_back("avutil_");
        funcIdents.push_back("av_");
    } else if (m_projectName == "libavresample") {
        funcIdents.push_back("avresample_");
    } else if (m_projectName == "libpostproc") {
        funcIdents.push_back("postproc_");
    } else if (m_projectName == "libswresample") {
        funcIdents.push_back("swri_");
        funcIdents.push_back("swresample_");
    } else if (m_projectName == "libswscale") {
        funcIdents.push_back("swscale_");
    }
    struct InternalDCEParams
    {
        DCEParams m_params;
        vector<uint> m_locations;
    };
    map<string, InternalDCEParams> internalList;
    for (const auto& i : g_tagsDCE) {
        for (unsigned j = 0; j < sizeof(tags2) / sizeof(string); j++) {
            const string sSearch = tags2[j] + i;

            // Search for all occurrences
            uint findPos = file.find(sSearch);
            while (findPos != string::npos) {
                // Get the define tag
                uint findPos2 = file.find(')', findPos + sSearch.length());
                findPos = findPos + tags2[j].length();
                if (j >= 2) {
                    --findPos;
                }
                // Skip any '(' found within the parameters itself
                uint findPos3 = file.find('(', findPos);
                while ((findPos3 != string::npos) && (findPos3 < findPos2)) {
                    findPos3 = file.find('(', findPos3 + 1);
                    findPos2 = file.find(')', findPos2 + 1);
                }
                string define = file.substr(findPos, findPos2 - findPos);

                // Check if define contains pre-processor tags
                if (define.find("##") != string::npos) {
                    requiresPreProcess = true;
                    return;
                }
                outputProjectDCECleanDefine(define);

                // Get the block of code being wrapped
                string code;
                findPos = file.find_first_not_of(g_whiteSpace, findPos2 + 1);
                if (file.at(findPos) == '{') {
                    // Need to get the entire block of code being wrapped
                    findPos2 = file.find('}', findPos + 1);
                    // Skip any '{' found within the parameters itself
                    uint findPos5 = file.find('{', findPos + 1);
                    while ((findPos5 != string::npos) && (findPos5 < findPos2)) {
                        findPos5 = file.find('{', findPos5 + 1);
                        findPos2 = file.find('}', findPos2 + 1);
                    }
                } else {
                    // This is a single line of code
                    findPos2 = file.find_first_of(g_endLine + ';', findPos + 1);
                    if (file.at(findPos2) == ';') {
                        ++findPos2;    // must include the ;
                    } else {
                        // Must check if next line was also an if
                        uint findPos5 = findPos;
                        while ((file.at(findPos5) == 'i') && (file.at(findPos5 + 1) == 'f')) {
                            // Get the define tag
                            findPos5 = file.find('(', findPos5 + 2);
                            findPos2 = file.find(')', findPos5 + 1);
                            // Skip any '(' found within the parameters itself
                            findPos3 = file.find('(', findPos5 + 1);
                            while ((findPos3 != string::npos) && (findPos3 < findPos2)) {
                                findPos3 = file.find('(', findPos3 + 1);
                                findPos2 = file.find(')', findPos2 + 1);
                            }
                            findPos5 = file.find_first_not_of(g_whiteSpace, findPos2 + 1);
                            if (file.at(findPos5) == '{') {
                                // Need to get the entire block of code being wrapped
                                findPos2 = file.find('}', findPos5 + 1);
                                // Skip any '{' found within the parameters itself
                                uint findPos6 = file.find('{', findPos5 + 1);
                                while ((findPos6 != string::npos) && (findPos6 < findPos2)) {
                                    findPos6 = file.find('{', findPos6 + 1);
                                    findPos2 = file.find('}', findPos2 + 1);
                                }
                                break;
                            }
                            // This is a single line of code
                            findPos2 = file.find_first_of(g_endLine + ';', findPos5 + 1);
                            if (file.at(findPos2) == ';') {
                                ++findPos2;    // must include the ;
                                break;
                            }
                        }
                    }
                }
                code = file.substr(findPos, findPos2 - findPos);
                uint findBack = findPos;

                // Get name of any functions
                for (const auto& k : funcIdents) {
                    findPos = code.find(k);
                    while (findPos != string::npos) {
                        bool valid = false;
                        // Check if this is a valid function call
                        uint findPos5 = code.find_first_of(g_nonName, findPos + 1);
                        if ((findPos5 != 0) && (findPos5 != string::npos)) {
                            uint findPos4 = code.find_last_of(g_nonName, findPos5 - 1);
                            findPos4 = (findPos4 == string::npos) ? 0 : findPos4 + 1;
                            // Check if valid function
                            if (findPos4 == findPos) {
                                findPos4 = code.find_first_not_of(g_whiteSpace, findPos5);
                                if (code.at(findPos4) == '(') {
                                    valid = true;
                                } else if (code.at(findPos4) == ';') {
                                    findPos4 = code.find_last_not_of(g_whiteSpace, findPos - 1);
                                    if (code.at(findPos4) == '=') {
                                        valid = true;
                                    }
                                } else if (code.at(findPos4) == '#') {
                                    // Check if this is a macro expansion
                                    requiresPreProcess = true;
                                    return;
                                }
                            }
                        }
                        if (valid) {
                            string add = code.substr(findPos, findPos5 - findPos);
                            // Check if there are any other DCE conditions
                            string funcDefine = define;
                            for (const auto& m : g_tagsDCE) {
                                for (unsigned n = 0; n < sizeof(tags2) / sizeof(string); n++) {
                                    const string search2 = tags2[n] + m;

                                    // Search for all occurrences
                                    uint findPos7 = code.rfind(search2, findPos);
                                    while (findPos7 != string::npos) {
                                        // Get the define tag
                                        uint findPos4 = code.find(')', findPos7 + sSearch.length());
                                        uint findPos8 = findPos7 + tags2[n].length();
                                        if (n >= 2) {
                                            --findPos8;
                                        }
                                        // Skip any '(' found within the parameters itself
                                        uint findPos9 = code.find('(', findPos8);
                                        while ((findPos9 != string::npos) && (findPos9 < findPos4)) {
                                            findPos9 = code.find('(', findPos9 + 1);
                                            findPos4 = code.find(')', findPos4 + 1);
                                        }
                                        string define2 = code.substr(findPos8, findPos4 - findPos8);
                                        outputProjectDCECleanDefine(define2);

                                        // Get the block of code being wrapped
                                        string code2;
                                        findPos8 = code.find_first_not_of(g_whiteSpace, findPos4 + 1);
                                        if (code.at(findPos8) == '{') {
                                            // Need to get the entire block of code being wrapped
                                            findPos4 = code.find('}', findPos8 + 1);
                                            // Skip any '{' found within the parameters itself
                                            uint findPos10 = code.find('{', findPos8 + 1);
                                            while ((findPos10 != string::npos) && (findPos10 < findPos4)) {
                                                findPos10 = code.find('{', findPos10 + 1);
                                                findPos4 = code.find('}', findPos4 + 1);
                                            }
                                            code2 = code.substr(findPos8, findPos4 - findPos8);
                                        } else {
                                            // This is a single line of code
                                            findPos4 = code.find_first_of(g_endLine, findPos8 + 1);
                                            code2 = code.substr(findPos8, findPos4 - findPos8);
                                        }

                                        // Check if function is actually effected by this DCE
                                        if (code2.find(add) != string::npos) {
                                            // Add the additional define
                                            string cond = "&&";
                                            if (define2.find_first_of("&|") != string::npos) {
                                                cond = '(' + define2 + ')' + cond;
                                            } else {
                                                cond = define2 + cond;
                                            }
                                            if (funcDefine.find_first_of("&|") != string::npos) {
                                                cond += '(' + funcDefine + ')';
                                            } else {
                                                cond += funcDefine;
                                            }
                                            funcDefine = cond;
                                        }

                                        // Search for next occurrence
                                        findPos7 = code.rfind(sSearch, findPos7 - 1);
                                    }
                                }
                            }

                            // Check if not already added
                            auto find = internalList.find(add);
                            uint valuePosition = findBack + findPos;
                            if (find == internalList.end()) {
                                // Check that another non DCE instance hasn't been found
                                if (nonDCEUsage.find(add) == nonDCEUsage.end()) {
                                    internalList[add] = {{funcDefine, fileName}, {valuePosition}};
                                }
                            } else {
                                string retDefine;
                                outputProgramDCEsCombineDefine(find->second.m_params.define, funcDefine, retDefine);
                                internalList[add].m_params.define = retDefine;
                                internalList[add].m_locations.push_back(valuePosition);
                            }
                        }
                        // Search for next occurrence
                        findPos = code.find(k, findPos + 1);
                    }
                }

                // Search for next occurrence
                findPos = file.find(sSearch, findPos2 + 1);
            }
        }
    }

    // Search for usage that is not effected by DCE
    for (const auto& i : funcIdents) {
        uint findPos = file.find(i);
        while (findPos != string::npos) {
            bool valid = false;
            // Check if this is a valid value
            uint findPos3 = file.find_first_of(g_nonName, findPos + 1);
            if (findPos3 != string::npos) {
                uint findPos4 = file.find_last_of(g_nonName, findPos3 - 1);
                findPos4 = (findPos4 == string::npos) ? 0 : findPos4 + 1;
                if (findPos4 == findPos) {
                    findPos4 = file.find_first_not_of(g_whiteSpace, findPos3);
                    // Check if declared inside a preprocessor block
                    uint findPos5 = file.find('#', findPos4 + 1);
                    if ((findPos5 == string::npos) || (file.at(findPos5 + 1) != 'e')) {
                        // Check if valid function
                        if (file.at(findPos4) == '(') {
                            // Check if function call or declaration (a function call must be inside a function {})
                            uint check1 = file.rfind('{', findPos);
                            if (check1 != string::npos) {
                                uint check2 = file.rfind('}', findPos);
                                if ((check2 == string::npos) || (check1 > check2)) {
                                    valid = true;
                                }
                            }
                            // Check if function definition
                            check1 = file.find(')', findPos4 + 1);
                            // Skip any '(' found within the function parameters itself
                            uint check2 = file.find('(', findPos4 + 1);
                            while ((check2 != string::npos) && (check2 < check1)) {
                                check2 = file.find('(', check2 + 1);
                                check1 = file.find(')', check1 + 1) + 1;
                            }
                            check2 = file.find_first_not_of(g_whiteSpace, check1 + 1);
                            if (file.at(check2) == '{') {
                                valid = true;
                            }
                        } else if (file.at(findPos4) == ';') {
                            findPos4 = file.find_last_not_of(g_whiteSpace, findPos4 - 1);
                            if (file.at(findPos4) == '=') {
                                valid = true;
                            }
                        } else if (file.at(findPos4) == '[') {
                            // Check if function is a table declaration
                            findPos4 = file.find(']', findPos4 + 1);
                            findPos4 = file.find_first_not_of(g_whiteSpace, findPos4 + 1);
                            if (file.at(findPos4) == '=') {
                                valid = true;
                            }
                        } else if (file.at(findPos4) == '=') {
                            valid = true;
                        }
                    }
                }
            }
            if (valid) {
                string add = file.substr(findPos, findPos3 - findPos);
                // Check if already added
                auto find = internalList.find(add);
                if (find == internalList.end()) {
                    nonDCEUsage.insert(add);
                    // Remove from external list as well
                    // WARNING: Assumes that there is not 2 values with the same name but local visibility in 2
                    // different files
                    if (foundDCEUsage.find(add) != foundDCEUsage.end()) {
                        foundDCEUsage.erase(add);
                    }
                } else {
                    // Check if this location was found in a DCE
                    bool found = false;
                    for (auto& j : internalList[add].m_locations) {
                        if (j == findPos) {
                            found = true;
                        }
                    }
                    if (!found) {
                        nonDCEUsage.insert(add);
                        // Remove from external list as well
                        if (foundDCEUsage.find(add) != foundDCEUsage.end()) {
                            foundDCEUsage.erase(add);
                        }
                        // Needs to remove it from internal list as it is also found in non-DCE section
                        internalList.erase(find);
                    }
                }
            }
            // Search for next occurrence
            findPos = file.find(i, findPos + 1);
        }
    }

    // Add all the found internal DCE values to the return list
    for (const auto& i : internalList) {
        auto find = foundDCEUsage.find(i.first);
        if (find == foundDCEUsage.end()) {
            foundDCEUsage[i.first] = i.second.m_params;
        } else {
            string retDefine;
            outputProgramDCEsCombineDefine(find->second.define, i.second.m_params.define, retDefine);
            foundDCEUsage[i.first] = {retDefine, find->second.file};
        }
    }
}

void ProjectGenerator::outputProgramDCEsResolveDefine(string& define)
{
    // Complex combinations of config options require determining exact values
    uint startTag = define.find_first_not_of(g_preProcessor);
    while (startTag != string::npos) {
        // Get the next tag
        uint div = define.find_first_of(g_preProcessor, startTag);
        string tag = define.substr(startTag, div - startTag);
        // Check if tag is enabled
        auto configOpt = m_configHelper.getConfigOptionPrefixed(tag);
        if ((configOpt == m_configHelper.m_configValues.end()) ||
            (m_configHelper.m_replaceList.find(configOpt->m_prefix + configOpt->m_option) !=
                m_configHelper.m_replaceList.end())) {
            // This config option doesn't exist but it is potentially included in its corresponding header file
            // Or this is a reserved value
        } else {
            // Replace the option with its value
            define.replace(startTag, div - startTag, configOpt->m_value);
            div = define.find_first_of(g_preProcessor, startTag);
        }

        // Get next
        startTag = define.find_first_not_of(g_preProcessor, div);
    }
    // Process the string to combine values
    findAndReplace(define, "&&", "&");
    findAndReplace(define, "||", "|");

    // Need to search through for !, !=, ==, &&, || in correct order of precendence
    const char ops[] = {'!', '=', '&', '|'};
    for (char i : ops) {
        startTag = define.find(i);
        while (startTag != string::npos) {
            // Check for != or ==
            if (define.at(startTag + 1) == '=') {
                ++startTag;
            }
            // Get right tag
            ++startTag;
            uint rightPos = define.find_first_of(g_preProcessor, startTag);
            // Skip any '(' found within the function parameters itself
            if ((rightPos != string::npos) && (define.at(rightPos) == '(')) {
                const uint back = rightPos + 1;
                rightPos = define.find(')', back) + 1;
                uint findPos3 = define.find('(', back);
                while ((findPos3 != string::npos) && (findPos3 < rightPos)) {
                    findPos3 = define.find('(', findPos3 + 1);
                    rightPos = define.find(')', rightPos + 1) + 1;
                }
            }
            string right = define.substr(startTag, rightPos - startTag);
            --startTag;

            // Check current operation
            if (define.at(startTag) == '!') {
                if (right == "0") {
                    //! 0 = 1
                    define.replace(startTag, rightPos - startTag, 1, '1');
                } else if (right == "1") {
                    //! 1 = 0
                    define.replace(startTag, rightPos - startTag, 1, '0');
                } else {
                    //! X = (!X)
                    if (rightPos == string::npos) {
                        define += ')';
                    } else {
                        define.insert(rightPos, 1, ')');
                    }
                    define.insert(startTag, 1, '(');
                    startTag += 2;
                }
            } else {
                // Check for != or ==
                if (define.at(startTag) == '=') {
                    --startTag;
                }
                // Get left tag
                uint leftPos = define.find_last_of(g_preProcessor, startTag - 1);
                // Skip any ')' found within the function parameters itself
                if ((leftPos != string::npos) && (define.at(leftPos) == ')')) {
                    const uint back = leftPos - 1;
                    leftPos = define.rfind('(', back);
                    uint findPos3 = define.rfind(')', back);
                    while ((findPos3 != string::npos) && (findPos3 > leftPos)) {
                        findPos3 = define.rfind(')', findPos3 - 1);
                        leftPos = define.rfind('(', leftPos - 1);
                    }
                } else {
                    leftPos = (leftPos == string::npos) ? 0 : leftPos + 1;
                }
                string left = define.substr(leftPos, startTag - leftPos);

                // Check current operation
                if (i == '&') {
                    if ((left == "0") || (right == "0")) {
                        // 0&&X or X&&0 == 0
                        define.replace(leftPos, rightPos - leftPos, 1, '0');
                        startTag = leftPos;
                    } else if (left == "1") {
                        // 1&&X = X
                        ++startTag;
                        define.erase(leftPos, startTag - leftPos);
                        startTag = leftPos;
                    } else if (right == "1") {
                        // X&&1 = X
                        define.erase(startTag, rightPos - startTag);
                    } else {
                        // X&&X = (X&&X)
                        if (rightPos == string::npos) {
                            define += ')';
                        } else {
                            define.insert(rightPos, 1, ')');
                        }
                        define.insert(leftPos, 1, '(');
                        startTag += 2;
                    }
                } else if (i == '|') {
                    if ((left == "1") || (right == "1")) {
                        // 1||X or X||1 == 1
                        define.replace(leftPos, rightPos - leftPos, 1, '1');
                        startTag = leftPos;
                    } else if (left == "0") {
                        // 0||X = X
                        ++startTag;
                        define.erase(leftPos, startTag - leftPos);
                        startTag = leftPos;
                    } else if (right == "0") {
                        // X||0 == X
                        define.erase(startTag, rightPos - startTag);
                    } else {
                        // X||X = (X||X)
                        if (rightPos == string::npos) {
                            define += ')';
                        } else {
                            define.insert(rightPos, 1, ')');
                        }
                        define.insert(leftPos, 1, '(');
                        startTag += 2;
                    }
                } else if (i == '!') {
                    if ((left == "1") && (right == "1")) {
                        // 1!=1 == 0
                        define.replace(leftPos, rightPos - leftPos, 1, '0');
                        startTag = leftPos;
                    } else if ((left == "1") && (right == "0")) {
                        // 1!=0 == 1
                        define.replace(leftPos, rightPos - leftPos, 1, '1');
                        startTag = leftPos;
                    } else if ((left == "0") && (right == "1")) {
                        // 0!=1 == 1
                        define.replace(leftPos, rightPos - leftPos, 1, '1');
                        startTag = leftPos;
                    } else {
                        // X!=X = (X!=X)
                        if (rightPos == string::npos) {
                            define += ')';
                        } else {
                            define.insert(rightPos, 1, ')');
                        }
                        define.insert(leftPos, 1, '(');
                        startTag += 2;
                    }
                } else if (i == '=') {
                    if ((left == "1") && (right == "1")) {
                        // 1==1 == 1
                        define.replace(leftPos, rightPos - leftPos, 1, '1');
                        startTag = leftPos;
                    } else if ((left == "1") && (right == "0")) {
                        // 1==0 == 0
                        define.replace(leftPos, rightPos - leftPos, 1, '0');
                        startTag = leftPos;
                    } else if ((left == "0") && (right == "1")) {
                        // 0==1 == 0
                        define.replace(leftPos, rightPos - leftPos, 1, '0');
                        startTag = leftPos;
                    } else {
                        // X==X == (X==X)
                        if (rightPos == string::npos) {
                            define += ')';
                        } else {
                            define.insert(rightPos, 1, ')');
                        }
                        define.insert(leftPos, 1, '(');
                        startTag += 2;
                    }
                }
            }
            findAndReplace(define, "(0)", "0");
            findAndReplace(define, "(1)", "1");

            // Get next
            startTag = define.find(i, startTag);
        }
    }
    // Remove any (RESERV)
    startTag = define.find('(');
    while (startTag != string::npos) {
        uint endTag = define.find(')', startTag);
        ++startTag;
        // Skip any '(' found within the function parameters itself
        uint findPos3 = define.find('(', startTag);
        while ((findPos3 != string::npos) && (findPos3 < endTag)) {
            findPos3 = define.find('(', findPos3 + 1);
            endTag = define.find(')', endTag + 1);
        }
        string tag = define.substr(startTag, endTag - startTag);
        if ((tag.find_first_of("&|()") == string::npos) || ((startTag == 1) && (endTag == define.length() - 1))) {
            define.erase(endTag, 1);
            define.erase(--startTag, 1);
        }
        startTag = define.find('(', startTag);
    }
    findAndReplace(define, "&", " && ");
    findAndReplace(define, "|", " || ");
}

bool ProjectGenerator::outputProjectDCEsFindDeclarations(
    const string& file, const string& function, const string&, string& retDeclaration, bool& isFunction)
{
    uint findPos = file.find(function);
    while (findPos != string::npos) {
        const uint findPos4 = file.find_first_not_of(g_whiteSpace, findPos + function.length());
        if (file.at(findPos4) == '(') {
            // Check if this is a function call or an actual declaration
            uint findPos2 = file.find(')', findPos4 + 1);
            if (findPos2 != string::npos) {
                // Skip any '(' found within the function parameters itself
                uint findPos3 = file.find('(', findPos4 + 1);
                while ((findPos3 != string::npos) && (findPos3 < findPos2)) {
                    findPos3 = file.find('(', findPos3 + 1);
                    findPos2 = file.find(')', findPos2 + 1);
                }
                findPos3 = file.find_first_not_of(g_whiteSpace, findPos2 + 1);
                // If this is a definition (i.e. '{') then that means no declaration could be found (headers are
                // searched before code files)
                if ((file.at(findPos3) == ';') || (file.at(findPos3) == '{')) {
                    findPos3 = file.find_last_not_of(g_whiteSpace, findPos - 1);
                    if (g_nonName.find(file.at(findPos3)) == string::npos) {
                        // Get the return type
                        findPos = file.find_last_of(g_whiteSpace, findPos3 - 1);
                        findPos = (findPos == string::npos) ? 0 : findPos + 1;
                        // Return the declaration
                        retDeclaration = file.substr(findPos, findPos2 - findPos + 1);
                        isFunction = true;
                        return true;
                    }
                    if (file.at(findPos3) == '*') {
                        // Return potentially contains a pointer
                        --findPos3;
                        findPos3 = file.find_last_not_of(g_whiteSpace, findPos3 - 1);
                        if (g_nonName.find(file.at(findPos3)) == string::npos) {
                            // Get the return type
                            findPos = file.find_last_of(g_whiteSpace, findPos3 - 1);
                            findPos = (findPos == string::npos) ? 0 : findPos + 1;
                            // Return the declaration
                            retDeclaration = file.substr(findPos, findPos2 - findPos + 1);
                            isFunction = true;
                            return true;
                        }
                    }
                }
            }
        } else if (file.at(findPos4) == '[') {
            // This is an array/table
            // Check if this is an definition or an declaration
            uint findPos2 = file.find(']', findPos4 + 1);
            if (findPos2 != string::npos) {
                // Skip multidimensional array
                while ((findPos2 + 1 < file.length()) && (file.at(findPos2 + 1) == '[')) {
                    findPos2 = file.find(']', findPos2 + 1);
                }
                uint findPos3 = file.find_first_not_of(g_whiteSpace, findPos2 + 1);
                if (file.at(findPos3) == '=') {
                    findPos3 = file.find_last_not_of(g_whiteSpace, findPos - 1);
                    if (g_nonName.find(file.at(findPos3)) == string::npos) {
                        // Get the array type
                        findPos = file.find_last_of(g_whiteSpace, findPos3 - 1);
                        findPos = (findPos == string::npos) ? 0 : findPos + 1;
                        // Return the definition
                        retDeclaration = file.substr(findPos, findPos2 - findPos + 1);
                        isFunction = false;
                        return true;
                    }
                    if (file.at(findPos3) == '*') {
                        // Type potentially contains a pointer
                        --findPos3;
                        findPos3 = file.find_last_not_of(g_whiteSpace, findPos3 - 1);
                        if (g_nonName.find(file.at(findPos3)) == string::npos) {
                            // Get the array type
                            findPos = file.find_last_of(g_whiteSpace, findPos3 - 1);
                            findPos = (findPos == string::npos) ? 0 : findPos + 1;
                            // Return the definition
                            retDeclaration = file.substr(findPos, findPos2 - findPos + 1);
                            isFunction = false;
                            return true;
                        }
                    }
                }
            }
        }

        // Search for next occurrence
        findPos = file.find(function, findPos + function.length() + 1);
    }
    return false;
}

void ProjectGenerator::outputProjectDCECleanDefine(string& define)
{
    const string tagReplace[] = {"EXTERNAL", "INTERNAL", "INLINE"};
    const string tagReplaceRemove[] = {"_FAST", "_SLOW"};
    // There are some macro tags that require conversion
    for (const auto& i : tagReplace) {
        string sSearch = i + '_';
        uint findPos = 0;
        while ((findPos = define.find(sSearch, findPos)) != string::npos) {
            const uint findPos4 = define.find_first_of('(', findPos + 1);
            const uint findPosBack = findPos;
            findPos += sSearch.length();
            string tagPart = define.substr(findPos, findPos4 - findPos);
            // Remove conversion values
            for (const auto& j : tagReplaceRemove) {
                uint uiFindRem = 0;
                while ((uiFindRem = tagPart.find(j, uiFindRem)) != string::npos) {
                    tagPart.erase(uiFindRem, j.length());
                }
            }
            tagPart = "HAVE_" + tagPart + '_' + i;
            uint findPos6 = define.find_first_of(')', findPos4 + 1);
            // Skip any '(' found within the parameters itself
            uint findPos5 = define.find('(', findPos4 + 1);
            while ((findPos5 != string::npos) && (findPos5 < findPos6)) {
                findPos5 = define.find('(', findPos5 + 1);
                findPos6 = define.find(')', findPos6 + 1);
            }
            // Update tag with replacement
            const uint repLength = findPos6 - findPosBack + 1;
            define.replace(findPosBack, repLength, tagPart);
            findPos = findPosBack + tagPart.length();
        }
    }

    // Check if the tag contains multiple conditionals
    removeWhiteSpace(define);
    uint startTag = define.find_first_not_of(g_preProcessor);
    while (startTag != string::npos) {
        // Check if each conditional is valid
        bool valid = false;
        for (const auto& i : g_tagsDCE) {
            if (define.find(i, startTag) == startTag) {
                // We have found a valid additional tag
                valid = true;
                break;
            }
        }
        if (!valid) {
            // Get right tag
            uint rightPos = define.find_first_of(g_preProcessor, startTag);
            // Skip any '(' found within the function parameters itself
            if ((rightPos != string::npos) && (define.at(rightPos) == '(')) {
                const uint back = rightPos + 1;
                rightPos = define.find(')', back) + 1;
                uint findPos3 = define.find('(', back);
                while ((findPos3 != string::npos) && (findPos3 < rightPos)) {
                    findPos3 = define.find('(', findPos3 + 1);
                    rightPos = define.find(')', rightPos + 1) + 1;
                }
            }
            if (!valid && (isupper(define.at(startTag)) != 0)) {
                string right(define.substr(startTag, rightPos - startTag));
                if ((right.find("AV_") != 0) && (right.find("FF_") != 0)) {
                    outputInfo("Found unknown macro in DCE condition " + right);
                }
            }
            while ((startTag != 0) && ((define.at(startTag - 1) == '(') && (define.at(rightPos) == ')'))) {
                // Remove the ()'s
                --startTag;
                ++rightPos;
            }
            if ((startTag == 0) || ((define.at(startTag - 1) == '(') && (define.at(rightPos) != ')'))) {
                // Trim operators after tag instead of before it
                rightPos = define.find_first_not_of("|&!=", rightPos + 1);    // Must not search for ()'s
            } else {
                // Trim operators before tag
                startTag = define.find_last_not_of("|&!=", startTag - 1) + 1;    // Must not search for ()'s
            }
            define.erase(startTag, rightPos - startTag);
            startTag = define.find_first_not_of(g_preProcessor, startTag);
        } else {
            startTag = define.find_first_of(g_preProcessor, startTag + 1);
            startTag = (startTag != string::npos) ? define.find_first_not_of(g_preProcessor, startTag + 1) : startTag;
        }
    }
}

void ProjectGenerator::outputProgramDCEsCombineDefine(const string& define, const string& define2, string& retDefine)
{
    if (define != define2) {
        // Check if either string contains the other
        if ((define.find(define2) != string::npos) || (define2.length() == 0)) {
            // Keep the existing one
            retDefine = define;
        } else if ((define2.find(define) != string::npos) || (define.length() == 0)) {
            // Use the new one in place of the original
            retDefine = define2;
        } else {
            // Add the additional define
            string ret = "||";
            if (define.find('&') != string::npos) {
                ret = '(' + define + ')' + ret;
            } else {
                ret = define + ret;
            }
            if (define2.find('&') != string::npos) {
                ret += '(' + define2 + ')';
            } else {
                ret += define2;
            }
            retDefine = ret;
        }
    } else {
        retDefine = define;
    }
}