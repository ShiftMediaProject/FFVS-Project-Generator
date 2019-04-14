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
    // Add the found string to internal storage
    endPos = m_inLine.find_first_of(". \t", startPos);
    if ((endPos != string::npos) && (m_inLine.at(endPos) == '.')) {
        // Skip any ./ or ../
        const uint endPos2 = m_inLine.find_first_not_of(".\\", endPos + 1);
        if ((endPos2 != string::npos) && (endPos2 > endPos + 1)) {
            endPos = m_inLine.find_first_of(". \t", endPos2 + 1);
        }
    }
    string sTag = m_inLine.substr(startPos, endPos - startPos);
    if (sTag.find('$') != string::npos) {
        // Invalid include. Occurs when include is actually a variable
        startPos += 2;
        sTag = m_inLine.substr(startPos, m_inLine.find(')', startPos) - startPos);
        // Check if additional variable (This happens when a string should be prepended to existing items within tag.)
        string tag2;
        if (sTag.find(':') != string::npos) {
            startPos = sTag.find(":%=");
            const uint startPos2 = startPos + 3;
            endPos = sTag.find('%', startPos2);
            tag2 = sTag.substr(startPos2, endPos - startPos2);
            sTag = sTag.substr(0, startPos);
        }
        // Get variable contents
        vector<string> files;
        m_configHelper.buildObjects(sTag, files);
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
                // outputInfo("Found C Static: '" + *vitFile + "'");
            }
        }
        return true;
    }

    // Check if object already included in internal list
    if (find(staticIncludes.begin(), staticIncludes.end(), sTag) == staticIncludes.end()) {
        staticIncludes.push_back(sTag);
        // outputInfo("Found Static: '" + sTag + "'");
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
    uint startPos = m_inLine.find_first_not_of(" +=:", length);
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
        auto objectList = m_unknowns.find(dynInc);
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
                        // outputInfo("Found Dynamic: '" + *vitObject + "', '" + "( " + ident + " && " + sDynInc + " )"
                        // + "'");
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
        endPos = m_inLine.find_first_of(". \t", startPos);
        if ((endPos != string::npos) && (m_inLine.at(endPos) == '.')) {
            // Skip any ./ or ../
            const uint endPos2 = m_inLine.find_first_not_of(".\\", endPos + 1);
            if ((endPos2 != string::npos) && (endPos2 > endPos + 1)) {
                endPos = m_inLine.find_first_of(". \t", endPos2 + 1);
            }
        }
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
            if (option->m_value == compare) {
                // Check if the config option is for a reserved type
                if (m_configHelper.m_replaceList.find(ident) != m_configHelper.m_replaceList.end()) {
                    m_replaceIncludes[tag].push_back(ident);
                    // outputInfo("Found Dynamic Replace: '" + sTag + "', '" + ident + "'");
                } else {
                    includes.push_back(tag);
                    // outputInfo("Found Dynamic: '" + sTag + "', '" + ident + "'");
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
    startPos = m_inLine.find_first_not_of("+=: \t", endPos + 1);
    if (!passDynamicIncludeLine(startPos, ident, includes)) {
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
        if (!passDynamicIncludeLine(startPos, ident, includes)) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::passCInclude()
{
    return passStaticInclude(4, m_includes);
}

bool ProjectGenerator::passDCInclude()
{
    return passDynamicInclude(5, m_includes);
}

bool ProjectGenerator::passASMInclude(const uint offset)
{
    // Check if supported option
    if (m_configHelper.isASMEnabled()) {
        return passStaticInclude(9 + offset, m_includes);
    }
    return true;
}

bool ProjectGenerator::passDASMInclude(const uint offset)
{
    // Check if supported option
    if (m_configHelper.isASMEnabled()) {
        return passDynamicInclude(10 + offset, m_includes);
    }
    return true;
}

bool ProjectGenerator::passMMXInclude()
{
    // Check if supported option
    if (m_configHelper.getConfigOptionPrefixed("HAVE_MMX")->m_value == "1") {
        return passStaticInclude(8, m_includes);
    }
    return true;
}

bool ProjectGenerator::passDMMXInclude()
{
    // Check if supported option
    if (m_configHelper.getConfigOptionPrefixed("HAVE_MMX")->m_value == "1") {
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
    startPos += 2;    // Skip the $(
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
    startPos += 2;    // Skip the $(
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

bool ProjectGenerator::passMake()
{
    const string makeFile = m_projectDir + "MakeFile";
    outputLine("  Generating from Makefile (" + makeFile + ")...");
    // Open the input Makefile
    m_inputFile.open(makeFile);
    if (m_inputFile.is_open()) {
        // Read each line in the MakeFile
        while (getline(m_inputFile, m_inLine)) {
            // Check what information is included in the current line
            if (m_inLine.substr(0, 4) == "OBJS") {
                // Found some c includes
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
            } else if ((m_inLine.substr(0, 11) == "X86ASM-OBJS") || (m_inLine.substr(0, 9) == "YASM-OBJS")) {
                // Found some YASM includes
                const uint offset = (m_inLine.at(0) == 'X') ? 2 : 0;
                if (m_inLine.at(9 + offset) == '-') {
                    // Found some dynamic ASM includes
                    if (!passDASMInclude(offset)) {
                        m_inputFile.close();
                        return false;
                    }
                } else {
                    // Found some static ASM includes
                    if (!passASMInclude(offset)) {
                        m_inputFile.close();
                        return false;
                    }
                }
            } else if (m_inLine.substr(0, 8) == "MMX-OBJS") {
                // Found some ASM includes
                if (m_inLine.at(8) == '-') {
                    // Found some dynamic MMX includes
                    if (!passDMMXInclude()) {
                        m_inputFile.close();
                        return false;
                    }
                } else {
                    // Found some static MMX includes
                    if (!passMMXInclude()) {
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
                if (option->m_value != "1") {
                    // Skip everything between the ifdefs
                    while (getline(m_inputFile, m_inLine)) {
                        if ((m_inLine.substr(0, 5) == "endif") || (m_inLine.substr(0, 4) == "else")) {
                            break;
                        }
                    }
                }
            }
        }
        m_inputFile.close();
        return true;
    }
    outputError("Could not open open MakeFile (" + makeFile + ")");
    return false;
}

bool ProjectGenerator::passProgramMake()
{
    uint checks = 2;
    while (checks >= 1) {
        // Open the input Makefile
        string makeFile = m_projectDir + "MakeFile";
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
                m_inLine = m_inLine.substr(findPos + 5, m_inLine.find(".o", findPos + 9) + 2 - (findPos + 5));
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
            string ignore;
            const string makeFolder = "fftools/";
            makeFile = m_projectDir + makeFolder + "MakeFile";
            if (findFile(makeFile, ignore)) {
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