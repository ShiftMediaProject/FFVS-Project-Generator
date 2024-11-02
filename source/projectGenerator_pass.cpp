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

bool ProjectGenerator::passStaticIncludeObject(uint& startPos, uint& endPos, StaticList& staticIncludes)
{
    startPos = m_inLine.find_first_not_of(".\\/", startPos); // Skip any ./ or ../
    if (startPos == string::npos) {
        endPos = startPos;
        return true; // skip this line as its empty
    }
    // Add the found string to internal storage
    endPos = m_inLine.find_first_of(". \t", startPos);
    if ((endPos != string::npos) && (m_inLine.at(endPos) == '.')) {
        // Skip any ./ or ../
        const uint endPos2 = m_inLine.find_first_not_of(".\\/", endPos + 1);
        if ((endPos2 != string::npos) && (endPos2 > endPos + 1)) {
            endPos = m_inLine.find_first_of(". \t", endPos2 + 1);
        }
    }
    string tag = m_inLine.substr(startPos, endPos - startPos);
    if (tag.find('$') != string::npos) {
        // Invalid include. Occurs when include is actually a variable
        startPos += 2;
        tag = m_inLine.substr(startPos, m_inLine.find(')', startPos) - startPos);
        // Check if additional variable (This happens when a string should be prepended to existing items within tag.)
        string tag2;
        if (tag.find(':') != string::npos) {
            startPos = tag.find(":%=");
            const uint startPos2 = startPos + 3;
            endPos = tag.find('%', startPos2);
            tag2 = tag.substr(startPos2, endPos - startPos2);
            tag = tag.substr(0, startPos);
        }
        // Get variable contents
        vector<string> files;
        m_configHelper.buildObjects(tag, files);
        if (tag2.length() > 0) {
            // Prepend the full library path
            for (auto file = files.begin(); file < files.end(); ++file) {
                *file = tag2 + *file;
            }
        }
        // Loop through each item and add to list
        for (auto file = files.begin(); file < files.end(); ++file) {
            // Check if object already included in internal list
            if (find(m_includesC.begin(), m_includesC.end(), *file) == m_includesC.end()) {
                staticIncludes.push_back(*file);
                outputInfo("Found C Static: '" + *file + "'");
            }
        }
        return true;
    }

    // Check if object already included in internal list
    if (find(staticIncludes.begin(), staticIncludes.end(), tag) == staticIncludes.end()) {
        staticIncludes.push_back(tag);
        outputInfo("Found Static: '" + tag + "'");
    }
    return true;
}

bool ProjectGenerator::passStaticIncludeLine(uint startPos, StaticList& staticIncludes)
{
    uint endPos;
    if (!passStaticIncludeObject(startPos, endPos, staticIncludes)) {
        return false;
    }
    // Check if there are multiple files declared on the same line
    while (endPos != string::npos) {
        startPos = m_inLine.find_first_of(" \t\\\n\0", endPos);
        startPos = m_inLine.find_first_not_of(" \t\\\n\0", startPos);
        if (startPos == string::npos) {
            break;
        }
        if (!passStaticIncludeObject(startPos, endPos, staticIncludes)) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passStaticInclude(const uint length, StaticList& staticIncludes)
{
    // Remove the identifier and '='
    uint startPos = m_inLine.find_first_not_of("+=: \t", length);
    if (!passStaticIncludeLine(startPos, staticIncludes)) {
        return true;
    }
    // Check if this is a multi line declaration
    while (m_inLine.back() == '\\') {
        // Get the next line
        getline(m_inputFile, m_inLine);
        // Remove the whitespace
        startPos = m_inLine.find_first_not_of(" \t");
        if (startPos == string::npos) {
            break;
        }
        if (!passStaticIncludeLine(startPos, staticIncludes)) {
            return true;
        }
    }
    return true;
}

bool ProjectGenerator::passDynamicIncludeObject(uint& startPos, uint& endPos, string& ident, StaticList& includes)
{
    // Check if this is a valid File or a past compile option
    if (m_inLine.at(startPos) == '$') {
        endPos = m_inLine.find(')', startPos);
        const string dynInc = m_inLine.substr(startPos + 2, endPos - startPos - 2);
        // Find it in the unknown list
        const auto objectList = m_unknowns.find(dynInc);
        if (objectList != m_unknowns.end()) {
            // Loop over each internal object
            for (auto object = objectList->second.begin(); object < objectList->second.end(); ++object) {
                // Check if object already included in internal list
                if (find(includes.begin(), includes.end(), *object) == includes.end()) {
                    // Check if the config option is correct
                    auto option = m_configHelper.getConfigOptionPrefixed(ident);
                    if (option == m_configHelper.m_configValues.end()) {
                        outputInfo("Unknown dynamic configuration option (" + ident + ") used when passing object (" +
                            *object + ")");
                        return true;
                    }
                    if (option->m_value == "1") {
                        includes.push_back(*object);
                        outputInfo("Found Dynamic: '" + *object + "', '" + "( " + ident + " && " + dynInc + " )" + "'");
                    }
                }
            }
        } else {
            outputError("Found unknown token (" + dynInc + ")");
            return false;
        }
    } else if (m_inLine.at(startPos) == '#') {
        // Found a comment, just skip till end of line
        endPos = m_inLine.length();
        return true;
    } else {
        // Check for condition
        string compare = "1";
        if (ident.at(0) == '!') {
            ident = ident.substr(1);
            compare = "0";
        }
        startPos = m_inLine.find_first_not_of(".\\/", startPos); // Skip any ./ or ../
        if (startPos == string::npos) {
            // Multi line statement, well get it on the next line
            return true;
        }
        endPos = m_inLine.find_first_of(" \t", startPos);
        endPos = m_inLine.rfind('.', endPos); // Include any additional extensions
        // Add the found string to internal storage
        const string tag = m_inLine.substr(startPos, endPos - startPos);
        // Check if object already included in internal list
        if (find(includes.begin(), includes.end(), tag) == includes.end()) {
            // Check if the config option is correct
            const auto option = m_configHelper.getConfigOptionPrefixed(ident);
            if (option == m_configHelper.m_configValues.end()) {
                outputInfo(
                    "Unknown dynamic configuration option (" + ident + ") used when passing object (" + tag + ")");
                return true;
            }
            if (option->m_value == compare ||
                m_configHelper.m_replaceList.find(ident) != m_configHelper.m_replaceList.end()) {
                // Check if the config option is for a reserved type
                if (m_configHelper.m_replaceList.find(ident) != m_configHelper.m_replaceList.end()) {
                    m_replaceIncludes[tag].push_back(compare == "1" ? ident : "!" + ident);
                    outputInfo("Found Dynamic Replace: '" + tag + "', '" + ident + "'");
                } else {
                    includes.push_back(tag);
                    outputInfo("Found Dynamic: '" + tag + "', '" + ident + "'");
                }
            }
        }
    }
    return true;
}

bool ProjectGenerator::passDynamicIncludeLine(uint startPos, string& ident, StaticList& includes)
{
    uint endPos;
    if (!passDynamicIncludeObject(startPos, endPos, ident, includes)) {
        return false;
    }
    // Check if there are multiple files declared on the same line
    while (endPos != string::npos) {
        startPos = m_inLine.find_first_of(" \t\\\n\0", endPos);
        startPos = m_inLine.find_first_not_of(" \t\\\n\0", startPos);
        if (startPos == string::npos) {
            break;
        }
        if (!passDynamicIncludeObject(startPos, endPos, ident, includes)) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passDynamicInclude(const uint length, StaticList& includes)
{
    // Find the dynamic identifier
    uint startPos = m_inLine.find_first_not_of("$( \t", length);
    const uint endPos = m_inLine.find(')', startPos);
    string ident = m_inLine.substr(startPos, endPos - startPos);
    // Find the included obj
    startPos = m_inLine.find_first_not_of("+=: \t\\", endPos + 1);
    if (startPos != string::npos) {
        if (!passDynamicIncludeLine(startPos, ident, includes)) {
            return false;
        }
    }
    // Check if this is a multi line declaration
    while (m_inLine.back() == '\\') {
        // Get the next line
        getline(m_inputFile, m_inLine);
        // Remove the whitespace
        startPos = m_inLine.find_first_not_of(" \t");
        if (startPos == string::npos) {
            break;
        }
        if (!passDynamicIncludeLine(startPos, ident, includes)) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passCondition(const string& condition, StaticList& list, UnknownList& replace, const uint offset)
{
    const auto cond = m_configHelper.getConfigOptionPrefixed(condition);
    if (cond == m_configHelper.m_configValues.end()) {
        outputError("Unknown configuration condition (" + condition + ")");
        return false;
    }
    if (cond->m_value == "0") {
        return true;
    }
    StaticList temp;
    if (passStaticInclude(offset, temp)) {
        for (auto& i : temp) {
            // Check if object already included in internal list
            if (find(list.begin(), list.end(), i) == list.end()) {
                // Check if already in replace list
                if (replace.find(i) == replace.end() ||
                    find(replace[i].begin(), replace[i].end(), condition) == replace[i].end()) {
                    replace[i].push_back(condition);
                }
            }
        }
        return true;
    }
    return false;
}

bool ProjectGenerator::passDCondition(
    const string& condition, StaticList& list, UnknownList& replace, const uint offset)
{
    const auto cond = m_configHelper.getConfigOptionPrefixed(condition);
    if (cond == m_configHelper.m_configValues.end()) {
        outputInfo("Unknown configuration condition (" + condition + ")");
        return false;
    }
    if (cond->m_value == "0") {
        return true;
    }
    StaticList temp;
    if (passDynamicInclude(offset, temp)) {
        for (auto& i : temp) {
            // Check if object already included in internal list
            if (find(list.begin(), list.end(), i) == list.end()) {
                // Check if already in replace list
                if (replace.find(i) == replace.end() ||
                    find(replace[i].begin(), replace[i].end(), condition) == replace[i].end()) {
                    replace[i].push_back(condition);
                }
            }
        }
        return true;
    }
    return false;
}

bool ProjectGenerator::passCInclude(const std::string& condition)
{
    if (!condition.empty()) {
        return passCondition(condition, m_includes, m_replaceIncludes, 4);
    }
    return passStaticInclude(4, m_includes);
}

bool ProjectGenerator::passDCInclude(const std::string& condition)
{
    if (!condition.empty()) {
        return passDCondition(condition, m_includes, m_replaceIncludes, 5);
    }
    return passDynamicInclude(5, m_includes);
}

bool ProjectGenerator::passASMInclude(const uint offset, const std::string& condition)
{
    // Check if supported option
    if (m_configHelper.isASMEnabled()) {
        if (!condition.empty()) {
            return passCondition(condition, m_includes, m_replaceIncludes, 9 + offset);
        }
        return passStaticInclude(9 + offset, m_includes);
    }
    return true;
}

bool ProjectGenerator::passDASMInclude(const uint offset, const std::string& condition)
{
    // Check if supported option
    if (m_configHelper.isASMEnabled()) {
        if (!condition.empty()) {
            return passDCondition(condition, m_includes, m_replaceIncludes, 10 + offset);
        }
        return passDynamicInclude(10 + offset, m_includes);
    }
    return true;
}

bool ProjectGenerator::passMMXInclude(const std::string& condition)
{
    // Check if supported option
    if (m_configHelper.getConfigOptionPrefixed("HAVE_MMX")->m_value == "1") {
        if (!condition.empty()) {
            return passDCondition(condition, m_includes, m_replaceIncludes, 8);
        }
        return passStaticInclude(8, m_includes);
    }
    return true;
}

bool ProjectGenerator::passDMMXInclude(const std::string& condition)
{
    // Check if supported option
    if (m_configHelper.getConfigOptionPrefixed("HAVE_MMX")->m_value == "1") {
        if (!condition.empty()) {
            return passDCondition(condition, m_includes, m_replaceIncludes, 9);
        }
        return passDynamicInclude(9, m_includes);
    }
    return true;
}

bool ProjectGenerator::passHInclude(const uint cutPos)
{
    return passStaticInclude(cutPos, m_includesH);
}

bool ProjectGenerator::passDHInclude()
{
    return passDynamicInclude(8, m_includesH);
}

bool ProjectGenerator::passLibInclude()
{
    return passStaticInclude(6, m_libs);
}

bool ProjectGenerator::passDLibInclude()
{
    return passDynamicInclude(7, m_libs);
}

bool ProjectGenerator::passDUnknown()
{
    // Find the dynamic identifier
    uint startPos = m_inLine.find("$(");
    const uint endPos = m_inLine.find(')', startPos);
    const string prefix = m_inLine.substr(0, startPos) + "yes";
    startPos += 2; // Skip the $(
    string ident = m_inLine.substr(startPos, endPos - startPos);
    // Find the included obj
    startPos = m_inLine.find_first_not_of("+= \t", endPos + 1);
    if (!passDynamicIncludeLine(startPos, ident, m_unknowns[prefix])) {
        return false;
    }
    // Check if this is a multi line declaration
    while (m_inLine.back() == '\\') {
        // Get the next line
        getline(m_inputFile, m_inLine);
        // Remove the whitespace
        startPos = m_inLine.find_first_not_of(" \t");
        if (startPos == string::npos) {
            break;
        }
        if (!passDynamicIncludeLine(startPos, ident, m_unknowns[prefix])) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passDLibUnknown()
{
    // Find the dynamic identifier
    uint startPos = m_inLine.find("$(");
    const uint endPos = m_inLine.find(')', startPos);
    const string prefix = m_inLine.substr(0, startPos) + "yes";
    startPos += 2; // Skip the $(
    string ident = m_inLine.substr(startPos, endPos - startPos);
    // Find the included obj
    startPos = m_inLine.find_first_not_of("+= \t", endPos + 1);
    if (!passDynamicIncludeLine(startPos, ident, m_unknowns[prefix])) {
        return false;
    }
    // Check if this is a multi line declaration
    while (m_inLine.back() == '\\') {
        // Get the next line
        getline(m_inputFile, m_inLine);
        // Remove the whitespace
        startPos = m_inLine.find_first_not_of(" \t");
        if (startPos == string::npos) {
            break;
        }
        if (!passDynamicIncludeLine(startPos, ident, m_unknowns[prefix])) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passSharedDCInclude()
{
    return passDCondition("CONFIG_SHARED", m_includes, m_replaceIncludes, 10);
}

bool ProjectGenerator::passSharedCInclude()
{
    return passCondition("CONFIG_SHARED", m_includes, m_replaceIncludes, 10);
}

bool ProjectGenerator::passStaticDCInclude()
{
    return passDCondition("CONFIG_STATIC", m_includes, m_replaceIncludes, 10);
}

bool ProjectGenerator::passStaticCInclude()
{
    return passCondition("CONFIG_STATIC", m_includes, m_replaceIncludes, 10);
}

bool ProjectGenerator::passMake()
{
    const string mainFile = m_projectDir + "MakeFile";
    vector<string> makeFiles{mainFile};
    while (!makeFiles.empty()) {
        const string makeFile = makeFiles.back();
        makeFiles.pop_back();
        outputLine("  Generating from Makefile (" + makeFile + ")...");
        // Open the input Makefile
        m_inputFile.open(makeFile);
        if (m_inputFile.is_open()) {
            // Read each line in the MakeFile
            string condition;
            while (getline(m_inputFile, m_inLine)) {
                // Check what information is included in the current line
                if (m_inLine.substr(0, 4) == "OBJS") {
                    // Found some c includes
                    if (m_inLine.at(4) == '-') {
                        // Found some dynamic c includes
                        if (!passDCInclude(condition)) {
                            m_inputFile.close();
                            return false;
                        }
                    } else {
                        // Found some static c includes
                        if (!passCInclude(condition)) {
                            m_inputFile.close();
                            return false;
                        }
                    }
                } else if ((m_inLine.substr(0, 11) == "X86ASM-OBJS") || (m_inLine.substr(0, 9) == "YASM-OBJS")) {
                    // Found some YASM includes
                    const uint offset = (m_inLine.at(0) == 'X') ? 2 : 0;
                    if (m_inLine.at(9 + offset) == '-') {
                        // Found some dynamic ASM includes
                        if (!passDASMInclude(offset, condition)) {
                            m_inputFile.close();
                            return false;
                        }
                    } else {
                        // Found some static ASM includes
                        if (!passASMInclude(offset, condition)) {
                            m_inputFile.close();
                            return false;
                        }
                    }
                } else if (m_inLine.substr(0, 8) == "MMX-OBJS") {
                    // Found some ASM includes
                    if (m_inLine.at(8) == '-') {
                        // Found some dynamic MMX includes
                        if (!passDMMXInclude(condition)) {
                            m_inputFile.close();
                            return false;
                        }
                    } else {
                        // Found some static MMX includes
                        if (!passMMXInclude(condition)) {
                            m_inputFile.close();
                            return false;
                        }
                    }
                } else if (m_inLine.substr(0, 7) == "HEADERS") {
                    // Found some headers
                    if (m_inLine.at(7) == '-') {
                        // Found some dynamic headers
                        if (!passDHInclude()) {
                            m_inputFile.close();
                            return false;
                        }
                    } else {
                        // Found some static headers
                        if (!passHInclude()) {
                            m_inputFile.close();
                            return false;
                        }
                    }
                } else if (m_inLine.find("BUILT_HEADERS") == 0) {
                    // Found some static built headers
                    if (!passHInclude(13)) {
                        m_inputFile.close();
                        return false;
                    }
                } else if (m_inLine.substr(0, 6) == "FFLIBS") {
                    // Found some libs
                    if (m_inLine.at(6) == '-') {
                        // Found some dynamic libs
                        if (!passDLibInclude()) {
                            m_inputFile.close();
                            return false;
                        }
                    } else {
                        // Found some static libs
                        if (!passLibInclude()) {
                            m_inputFile.close();
                            return false;
                        }
                    }
                } else if (m_inLine.substr(0, 9) == "SHLIBOBJS") {
                    // Found some libs
                    if (m_inLine.at(9) == '-') {
                        // Found some dynamic include
                        if (!passSharedDCInclude()) {
                            m_inputFile.close();
                            return false;
                        }
                    } else {
                        // Found some static include
                        if (!passSharedCInclude()) {
                            m_inputFile.close();
                            return false;
                        }
                    }
                } else if (m_inLine.substr(0, 9) == "STLIBOBJS") {
                    // Found some libs
                    if (m_inLine.at(9) == '-') {
                        // Found some dynamic include
                        if (!passStaticDCInclude()) {
                            m_inputFile.close();
                            return false;
                        }
                    } else {
                        // Found some static include
                        if (!passStaticCInclude()) {
                            m_inputFile.close();
                            return false;
                        }
                    }
                } else if (m_inLine.find("-OBJS-$") != string::npos) {
                    // Found unknown
                    if (!passDUnknown()) {
                        m_inputFile.close();
                        return false;
                    }
                } else if (m_inLine.find("LIBS-$") != string::npos) {
                    // Found Lib unknown
                    if (!passDLibUnknown()) {
                        m_inputFile.close();
                        return false;
                    }
                } else if (m_inLine.substr(0, 5) == "ifdef") {
                    // Check for configuration value
                    const uint startPos = m_inLine.find_first_not_of(" \t", 5);
                    uint endPos = m_inLine.find_first_of(" \t\n\r", startPos + 1);
                    endPos = (endPos == string::npos) ? endPos : endPos - startPos;
                    string config = m_inLine.substr(startPos, endPos);
                    // Check if the config option is correct
                    auto option = m_configHelper.getConfigOptionPrefixed(config);
                    if (option == m_configHelper.m_configValues.end()) {
                        outputInfo("Unknown ifdef configuration option (" + config + ")");
                        return false;
                    }
                    if (option->m_value == "0") {
                        // Skip everything between the ifdefs
                        while (getline(m_inputFile, m_inLine)) {
                            if ((m_inLine.substr(0, 5) == "endif") || (m_inLine.substr(0, 4) == "else")) {
                                break;
                            }
                        }
                    } else {
                        // Check if the config option is for a reserved type
                        if (m_configHelper.m_replaceList.find(config) != m_configHelper.m_replaceList.end()) {
                            condition = config;
                        }
                    }
                } else if (m_inLine.substr(0, 5) == "else") {
                    // If we have no previous reserved condition then we can just pull in everything after the 'else'
                    if (!condition.empty()) {
                        // Condition is now the opposite of what it was
                        if (condition == "ARCH_X86_32") {
                            condition = "ARCH_X86_64";
                        } else if (condition == "ARCH_X86_64") {
                            condition = "ARCH_X86_32";
                        } else if (condition == "CONFIG_SHARED") {
                            condition = "CONFIG_STATIC";
                        } else if (condition == "CONFIG_STATIC") {
                            condition = "CONFIG_SHARED";
                        } else {
                            outputInfo("Unknown ifdef else configuration option (" + condition + ")");
                            // Skip everything between the ifdefs
                            while (getline(m_inputFile, m_inLine)) {
                                if (m_inLine.substr(0, 5) == "endif") {
                                    break;
                                }
                            }
                        }
                    }
                } else if (m_inLine.substr(0, 5) == "endif") {
                    // Reset the current condition
                    condition.clear();
                } else if (m_inLine.substr(0, 7) == "include" || m_inLine.substr(0, 8) == "-include") {
                    // Need to append the included file to makefile list
                    uint startPos = m_inLine.find_first_not_of(" \t", 7 + (m_inLine[0] == '-' ? 1 : 0));
                    uint endPos = m_inLine.find_first_of(" \t\n\r", startPos + 1);
                    endPos = (endPos == string::npos) ? endPos : endPos - startPos;
                    string newMake = m_inLine.substr(startPos, endPos);
                    // Perform token substitution
                    startPos = newMake.find('$');
                    while (startPos != string::npos) {
                        endPos = newMake.find(')', startPos + 1);
                        if (endPos == string::npos) {
                            outputError("Invalid token in include (" + newMake + ")");
                            return false;
                        }
                        ++endPos;
                        string token = newMake.substr(startPos, endPos - startPos);
                        if (token == "$(SRC_PATH)") {
                            newMake.replace(startPos, endPos - startPos, m_configHelper.m_rootDirectory);
                        } else if (token == "$(ARCH)") {
                            newMake.replace(startPos, endPos - startPos, "x86");
                        } else {
                            outputError("Unknown token in include (" + token + ")");
                            return false;
                        }
                        startPos = newMake.find('$', startPos);
                    }
                    makeFiles.push_back(newMake);
                    // Add to internal list of known subdirectories
                    const uint rootPos = newMake.find(m_configHelper.m_rootDirectory); 
                    if (rootPos != string::npos) {
                        newMake.erase(rootPos, m_configHelper.m_rootDirectory.length());
                    }
                    const uint projPos = newMake.find(m_projectName + '/');
                    if (projPos != string::npos) {
                        newMake.erase(projPos, m_projectName.length() + 1);
                    }
                    // Clean duplicate '//'
                    uint findPos2 = newMake.find("//");
                    while (findPos2 != string::npos) {
                        newMake.erase(findPos2, 1);
                        // get next
                        findPos2 = newMake.find("//");
                    }
                    if (newMake[0] == '/') {
                        newMake.erase(0, 1);
                    }
                    findPos2 = newMake.rfind('/');
                    if (findPos2 != string::npos) {
                        newMake.erase(findPos2);
                    }
                    if (!newMake.empty()) {
                        m_subDirs.push_back(newMake);
                    }
                }
            }
            m_inputFile.close();
        } else {
            outputError("Could not open open MakeFile (" + makeFile + ")");
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passProgramMake()
{
    uint checks = 2;
    while (checks >= 1) {
        // Open the input Makefile
        string makeFile = m_projectDir + "MakeFile";
        m_inputFile.close();
        m_inputFile.open(makeFile);
        if (!m_inputFile.is_open()) {
            outputError("Could not open open MakeFile (" + makeFile + ")");
        }
        outputLine("  Generating from Makefile (" + makeFile + ") for project " + m_projectName + "...");
        string objTag = "OBJS-" + m_projectName;
        uint findPos;
        // Read each line in the MakeFile
        while (getline(m_inputFile, m_inLine)) {
            // Check what information is included in the current line
            if (m_inLine.substr(0, objTag.length()) == objTag) {
                // Cut the line so it can be used by default passers
                m_inLine = m_inLine.substr(objTag.length() - 4);
                if (m_inLine.at(4) == '-') {
                    // Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                } else {
                    // Found some static c includes
                    if (!passCInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                }
            } else if (m_inLine.substr(0, 6) == "FFLIBS") {
                // Found some libs
                if (m_inLine.at(6) == '-') {
                    // Found some dynamic libs
                    if (!passDLibInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                } else {
                    // Found some static libs
                    if (!passLibInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                }
            } else if ((findPos = m_inLine.find("eval OBJS-$(prog)")) != string::npos) {
                m_inLine = m_inLine.substr(findPos + 13);
                if (m_inLine.at(4) == '-') {
                    // Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                } else {
                    // Found some static c includes
                    if (!passCInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                }
            } else if (((findPos = m_inLine.find("OBJS-$(1)")) != string::npos) && (findPos == 0)) {
                m_inLine = m_inLine.substr(findPos + 5, m_inLine.rfind(".o") + 2 - (findPos + 5));
                if (m_inLine.at(4) == '-') {
                    // Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                } else {
                    // Found some static c includes
                    if (!passCInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                }
            } else if ((findPos = m_inLine.find("eval OBJS-$(P)")) != string::npos) {
                m_inLine = m_inLine.substr(findPos + 10);
                if (m_inLine.at(4) == '-') {
                    // Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                } else {
                    // Found some static c includes
                    if (!passCInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                }
            }
        }
        m_inputFile.close();
        if (checks == 2) {
            string ignored;
            const string makeFolder = "fftools/";
            makeFile = m_projectDir + makeFolder + "MakeFile";
            if (findFile(makeFile, ignored)) {
                // If using the Makefile in fftools then we need to read both it and the root Makefile
                m_projectDir += makeFolder;
            } else {
                --checks;
            }
        }
        // When passing the fftools folder some objects are added with fftools folder prefixed to file name
        const string makeFolder = "fftools/";
        uint uiPos;
        for (auto i = m_includes.begin(); i < m_includes.end(); ++i) {
            if ((uiPos = i->find(makeFolder)) != string::npos) {
                i->erase(uiPos, makeFolder.length());
            }
        }
        --checks;
    }

    // Program always includes a file named after themselves
    m_includes.push_back(m_projectName);
    return true;
}
