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

bool ProjectGenerator::findSourceFile(const string& file, const string& extension, string& retFileName) const
{
    string fileName;
    retFileName = m_projectDir + file + extension;
    if (!findFile(retFileName, fileName)) {
        // Check if this is a built file
        uint pos = m_projectDir.rfind('/', m_projectDir.length() - 2);
        pos = (pos == string::npos) ? 0 : pos + 1;
        string projectName = m_projectDir.substr(pos);
        projectName = (m_projectDir != "./") ? projectName : "";
        retFileName = m_configHelper.m_solutionDirectory + projectName + file + extension;
        if (!findFile(retFileName, fileName)) {
            // Check if this file already includes the project folder in its name
            if (file.find(projectName) != string::npos) {
                retFileName = m_projectDir + file.substr(file.find(projectName) + projectName.length()) + extension;
                return findFile(retFileName, fileName);
            }
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::findSourceFiles(const string& file, const string& extension, vector<string>& retFiles) const
{
    const string fileName = m_projectDir + file + extension;
    return findFiles(fileName, retFiles);
}

bool ProjectGenerator::checkProjectFiles()
{
    // Check that all headers are correct
    for (auto& include : m_includesH) {
        string retFileName;
        if (!findSourceFile(include, ".h", retFileName)) {
            outputError("Could not find input header file for object (" + include + ")");
            return false;
        }
        // Update the entry with the found file with complete path
        m_configHelper.makeFileProjectRelative(retFileName, include);
    }

    // Check that all C Source are correct
    for (auto& include : m_includesC) {
        string retFileName;
        if (!findSourceFile(include, ".c", retFileName)) {
            outputError("Could not find input C source file for object (" + include + ")");
            return false;
        }
        // Update the entry with the found file with complete path
        m_configHelper.makeFileProjectRelative(retFileName, include);
    }

    // Check that all CPP Source are correct
    for (auto& include : m_includesCPP) {
        string retFileName;
        if (!findSourceFile(include, ".cpp", retFileName)) {
            outputError("Could not find input C++ source file for object (" + include + ")");
            return false;
        }
        // Update the entry with the found file with complete path
        m_configHelper.makeFileProjectRelative(retFileName, include);
    }

    // Check that all ASM Source are correct
    for (auto& include : m_includesASM) {
        string retFileName;
        if (!findSourceFile(include, ".asm", retFileName)) {
            outputError("Could not find input ASM source file for object (" + include + ")");
            return false;
        }
        // Update the entry with the found file with complete path
        m_configHelper.makeFileProjectRelative(retFileName, include);
    }

    // Check the output Unknown Includes and find there corresponding file
    if (!findProjectFiles(
            m_includes, m_includesC, m_includesCPP, m_includesASM, m_includesH, m_includesRC, m_includesCU)) {
        return false;
    }

    if (m_configHelper.m_onlyDCE) {
        // Don't need to check for replace files
        return true;
    }

    // Check all source files associated with replaced config values
    StaticList replaceIncludes, replaceCPPIncludes, replaceCIncludes, replaceASMIncludes;
    for (auto& include : m_replaceIncludes) {
        replaceIncludes.push_back(include.first);
    }
    if (!findProjectFiles(replaceIncludes, replaceCIncludes, replaceCPPIncludes, replaceASMIncludes, m_includesH,
            m_includesRC, m_includesCU)) {
        return false;
    }
    // Need to create local files for any replace objects
    if (!createReplaceFiles(replaceCIncludes, m_includesC, m_includesConditionalC)) {
        return false;
    }
    if (!createReplaceFiles(replaceCPPIncludes, m_includesCPP, m_includesConditionalCPP)) {
        return false;
    }
    if (!createReplaceFiles(replaceASMIncludes, m_includesASM, m_includesConditionalASM)) {
        return false;
    }
    return true;
}

bool ProjectGenerator::createReplaceFiles(
    const StaticList& replaceIncludes, StaticList& existingIncludes, ConditionalList& conditionalIncludes)
{
    for (const auto& replaceInclude : replaceIncludes) {
        // Check hasn't already been included as a fixed object
        if (find(existingIncludes.begin(), existingIncludes.end(), replaceInclude) != existingIncludes.end()) {
            // skip this item
            continue;
        }
        // Convert file to format required to search ReplaceIncludes
        const uint extPos = replaceInclude.rfind('.');
        const uint cutPos = replaceInclude.find('/', 5) + 1;
        string filename = replaceInclude.substr(cutPos, extPos - cutPos);
        string extension = replaceInclude.substr(extPos);
        string outFile = m_configHelper.m_solutionDirectory + m_projectName + "/" + filename + "_wrap" + extension;
        string newOutFile;
        m_configHelper.makeFileProjectRelative(outFile, newOutFile);
        // Check hasn't already been included as a wrapped object
        if (find(existingIncludes.begin(), existingIncludes.end(), newOutFile) != existingIncludes.end()) {
            // skip this item
            outputInfo(newOutFile);
            continue;
        }
        // Find the file in the original list
        string origName;
        for (auto& include : m_replaceIncludes) {
            if (include.first.find(filename) != string::npos) {
                origName = include.first;
                break;
            }
        }
        if (origName.length() == 0) {
            outputError("Could not find original file name for source file (" + filename + ")");
            return false;
        }
        // Get the files dynamic config requirement
        string idents;
        bool isStatic = false;
        bool isShared = false;
        bool is32 = false;
        bool is64 = false;
        bool hasOther = false;
        for (auto include = m_replaceIncludes[origName].begin(); include < m_replaceIncludes[origName].end();
             ++include) {
            idents += *include;
            if ((include + 1) < m_replaceIncludes[origName].end()) {
                idents += " || ";
            }
            if (*include == "ARCH_X86_32" || *include == "!ARCH_X86_64") {
                is32 = true;
            } else if (*include == "ARCH_X86_64" || *include == "!ARCH_X86_32") {
                is64 = true;
            } else if (*include == "CONFIG_SHARED" || *include == "!CONFIG_STATIC") {
                isShared = true;
            } else if (*include == "CONFIG_STATIC" || *include == "!CONFIG_SHARED") {
                isStatic = true;
            } else {
                hasOther = true;
            }
        }
        // Check if already a conditional file
        auto j = conditionalIncludes.find(replaceInclude);
        if (j != conditionalIncludes.end()) {
            if (hasOther) {
                conditionalIncludes.erase(j);
            } else if (j->second.isStatic != isStatic || j->second.isShared != isShared || j->second.is32 != is32 ||
                j->second.is64 != is64) {
                outputError("Duplicate conditional files found with different conditions (" + replaceInclude + ")");
                // TODO: Remove and make wrapped
                return false;
            } else {
                // skip this item
                continue;
            }
        }
        // Check for config requirement that can be handled by VS (i.e. static/shared|32/64bit)
        if ((isShared || isStatic || is32 || is64) && !hasOther) {
            conditionalIncludes.emplace(replaceInclude, ConfigConds{isStatic, isShared, is32, is64});
            continue;
        }
        // Create new file to wrap input object
        string prettyFile = "../" + replaceInclude;
        string newFile = getCopywriteHeader(filename + extension + " file wrapper for " + m_projectName);
        newFile += "\n\
\n\
#include \"config.h\"\n";
        if (m_configHelper.m_configComponentsStart > 0 && extension != ".asm") {
            newFile += "#include \"config_components.h\"\n";
        }
        newFile += "#if " + idents + "\n\
#   include \"" +
            prettyFile + "\"\n\
#endif";
        // Check if assembly file
        if (extension == ".asm") {
            replace(newFile.begin(), newFile.end(), '#', '%');
            findAndReplace(newFile, ".h", ".asm");
            findAndReplace(newFile, "/**", ";");
            findAndReplace(newFile, " */", ";");
            findAndReplace(newFile, " *", ";");
        }
        // Write output project
        if (!makeDirectory(m_configHelper.m_solutionDirectory + m_projectName)) {
            outputError("Failed creating local " + m_projectName + " directory");
            return false;
        }
        if (!writeToFile(outFile, newFile)) {
            return false;
        }
        // Add the new file to list of objects
        existingIncludes.push_back(newOutFile);
    }
    return true;
}

bool ProjectGenerator::findProjectFiles(const StaticList& includes, StaticList& includesC, StaticList& includesCPP,
    StaticList& includesASM, StaticList& includesH, StaticList& includesRC, StaticList& includesCU) const
{
    for (const auto& include : includes) {
        string retFileName;
        if (findSourceFile(include, ".c", retFileName)) {
            // Found a C File to include
            m_configHelper.makeFileProjectRelative(retFileName, retFileName);
            if (find(includesC.begin(), includesC.end(), retFileName) != includesC.end()) {
                // skip this item
                continue;
            }
            includesC.push_back(retFileName);
        } else if (findSourceFile(include, ".cpp", retFileName)) {
            // Found a C++ File to include
            m_configHelper.makeFileProjectRelative(retFileName, retFileName);
            if (find(includesCPP.begin(), includesCPP.end(), retFileName) != includesCPP.end()) {
                // skip this item
                continue;
            }
            includesCPP.push_back(retFileName);
        } else if (findSourceFile(include, ".asm", retFileName)) {
            // Found a ASM File to include
            m_configHelper.makeFileProjectRelative(retFileName, retFileName);
            if (find(includesASM.begin(), includesASM.end(), retFileName) != includesASM.end()) {
                // skip this item
                continue;
            }
            includesASM.push_back(retFileName);
        } else if (findSourceFile(include, ".h", retFileName)) {
            // Found a H File to include
            m_configHelper.makeFileProjectRelative(retFileName, retFileName);
            if (find(includesH.begin(), includesH.end(), retFileName) != includesH.end()) {
                // skip this item
                continue;
            }
            includesH.push_back(retFileName);
        } else if (findSourceFile(include, ".rc", retFileName)) {
            // Found a H File to include
            m_configHelper.makeFileProjectRelative(retFileName, retFileName);
            if (find(includesRC.begin(), includesRC.end(), retFileName) != includesRC.end()) {
                // skip this item
                continue;
            }
            includesRC.push_back(retFileName);
        } else if (include.find(".ptx") != string::npos) {
            // Found a CUDA file
            string fileName = include.substr(0, include.find(".ptx"));
            if (findSourceFile(fileName, ".cu", retFileName)) {
                // Found a H File to include
                m_configHelper.makeFileProjectRelative(retFileName, retFileName);
                if (find(includesCU.begin(), includesCU.end(), retFileName) != includesCU.end()) {
                    // skip this item
                    continue;
                }
                includesCU.push_back(retFileName);
            }
        } else {
            outputError("Could not find valid source file for object (" + include + ")");
            return false;
        }
    }
    return true;
}
