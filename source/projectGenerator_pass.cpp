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

bool ProjectGenerator::passStaticIncludeObject(uint& uiStartPos, uint& uiEndPos, StaticList& vStaticIncludes)
{
    // Add the found string to internal storage
    uiEndPos = m_sInLine.find_first_of(". \t", uiStartPos);
    if ((uiEndPos != string::npos) && (m_sInLine.at(uiEndPos) == '.')) {
        // Skip any ./ or ../
        uint uiEndPos2 = m_sInLine.find_first_not_of(".\\", uiEndPos + 1);
        if ((uiEndPos2 != string::npos) && (uiEndPos2 > uiEndPos + 1)) {
            uiEndPos = m_sInLine.find_first_of(". \t", uiEndPos2 + 1);
        }
    }
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
            // Prepend the full library path
            for (auto vitFile = vFiles.begin(); vitFile < vFiles.end(); ++vitFile) {
                *vitFile = sTag2 + *vitFile;
            }
        }
        // Loop through each item and add to list
        for (auto vitFile = vFiles.begin(); vitFile < vFiles.end(); ++vitFile) {
            // Check if object already included in internal list
            if (find(m_vCIncludes.begin(), m_vCIncludes.end(), *vitFile) == m_vCIncludes.end()) {
                vStaticIncludes.push_back(*vitFile);
                // outputInfo("Found C Static: '" + *vitFile + "'");
            }
        }
        return true;
    }

    // Check if object already included in internal list
    if (find(vStaticIncludes.begin(), vStaticIncludes.end(), sTag) == vStaticIncludes.end()) {
        vStaticIncludes.push_back(sTag);
        // outputInfo("Found Static: '" + sTag + "'");
    }
    return true;
}

bool ProjectGenerator::passStaticIncludeLine(uint uiStartPos, StaticList& vStaticIncludes)
{
    uint uiEndPos;
    if (!passStaticIncludeObject(uiStartPos, uiEndPos, vStaticIncludes)) {
        return false;
    }
    // Check if there are multiple files declared on the same line
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

bool ProjectGenerator::passStaticInclude(uint uiILength, StaticList& vStaticIncludes)
{
    // Remove the identifier and '='
    uint uiStartPos = m_sInLine.find_first_not_of(" +=:", uiILength);
    if (!passStaticIncludeLine(uiStartPos, vStaticIncludes)) {
        return true;
    }
    // Check if this is a multi line declaration
    while (m_sInLine.back() == '\\') {
        // Get the next line
        getline(m_ifInputFile, m_sInLine);
        // Remove the whitespace
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

bool ProjectGenerator::passDynamicIncludeObject(uint& uiStartPos, uint& uiEndPos, string& sIdent, StaticList& vIncludes)
{
    // Check if this is a valid File or a past compile option
    if (m_sInLine.at(uiStartPos) == '$') {
        uiEndPos = m_sInLine.find(')', uiStartPos);
        string sDynInc = m_sInLine.substr(uiStartPos + 2, uiEndPos - uiStartPos - 2);
        // Find it in the unknown list
        auto mitObjectList = m_mUnknowns.find(sDynInc);
        if (mitObjectList != m_mUnknowns.end()) {
            // Loop over each internal object
            for (auto vitObject = mitObjectList->second.begin(); vitObject < mitObjectList->second.end(); ++vitObject) {
                // Check if object already included in internal list
                if (find(vIncludes.begin(), vIncludes.end(), *vitObject) == vIncludes.end()) {
                    // Check if the config option is correct
                    auto vitOption = m_ConfigHelper.getConfigOptionPrefixed(sIdent);
                    if (vitOption == m_ConfigHelper.m_configValues.end()) {
                        outputInfo("Unknown dynamic configuration option (" + sIdent + ") used when passing object (" +
                            *vitObject + ")");
                        return true;
                    }
                    if (vitOption->m_value == "1") {
                        vIncludes.push_back(*vitObject);
                        // outputInfo("Found Dynamic: '" + *vitObject + "', '" + "( " + sIdent + " && " + sDynInc + " )"
                        // + "'");
                    }
                }
            }
        } else {
            outputError("Found unknown token (" + sDynInc + ")");
            return false;
        }
    } else if (m_sInLine.at(uiStartPos) == '#') {
        // Found a comment, just skip till end of line
        uiEndPos = m_sInLine.length();
        return true;
    } else {
        // Check for condition
        string sCompare = "1";
        if (sIdent.at(0) == '!') {
            sIdent = sIdent.substr(1);
            sCompare = "0";
        }
        uiEndPos = m_sInLine.find_first_of(". \t", uiStartPos);
        if ((uiEndPos != string::npos) && (m_sInLine.at(uiEndPos) == '.')) {
            // Skip any ./ or ../
            uint uiEndPos2 = m_sInLine.find_first_not_of(".\\", uiEndPos + 1);
            if ((uiEndPos2 != string::npos) && (uiEndPos2 > uiEndPos + 1)) {
                uiEndPos = m_sInLine.find_first_of(". \t", uiEndPos2 + 1);
            }
        }
        // Add the found string to internal storage
        string sTag = m_sInLine.substr(uiStartPos, uiEndPos - uiStartPos);
        // Check if object already included in internal list
        if (find(vIncludes.begin(), vIncludes.end(), sTag) == vIncludes.end()) {
            // Check if the config option is correct
            auto vitOption = m_ConfigHelper.getConfigOptionPrefixed(sIdent);
            if (vitOption == m_ConfigHelper.m_configValues.end()) {
                outputInfo(
                    "Unknown dynamic configuration option (" + sIdent + ") used when passing object (" + sTag + ")");
                return true;
            }
            if (vitOption->m_value == sCompare) {
                // Check if the config option is for a reserved type
                if (m_ConfigHelper.m_replaceList.find(sIdent) != m_ConfigHelper.m_replaceList.end()) {
                    m_mReplaceIncludes[sTag].push_back(sIdent);
                    // outputInfo("Found Dynamic Replace: '" + sTag + "', '" + sIdent + "'");
                } else {
                    vIncludes.push_back(sTag);
                    // outputInfo("Found Dynamic: '" + sTag + "', '" + sIdent + "'");
                }
            }
        }
    }
    return true;
}

bool ProjectGenerator::passDynamicIncludeLine(uint uiStartPos, string& sIdent, StaticList& vIncludes)
{
    uint uiEndPos;
    if (!passDynamicIncludeObject(uiStartPos, uiEndPos, sIdent, vIncludes)) {
        return false;
    }
    // Check if there are multiple files declared on the same line
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

bool ProjectGenerator::passDynamicInclude(uint uiILength, StaticList& vIncludes)
{
    // Find the dynamic identifier
    uint uiStartPos = m_sInLine.find_first_not_of("$( \t", uiILength);
    uint uiEndPos = m_sInLine.find(')', uiStartPos);
    string sIdent = m_sInLine.substr(uiStartPos, uiEndPos - uiStartPos);
    // Find the included obj
    uiStartPos = m_sInLine.find_first_not_of("+=: \t", uiEndPos + 1);
    if (!passDynamicIncludeLine(uiStartPos, sIdent, vIncludes)) {
        return false;
    }
    // Check if this is a multi line declaration
    while (m_sInLine.back() == '\\') {
        // Get the next line
        getline(m_ifInputFile, m_sInLine);
        // Remove the whitespace
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

bool ProjectGenerator::passASMInclude(uint uiOffset)
{
    // Check if supported option
    if (m_ConfigHelper.isASMEnabled()) {
        return passStaticInclude(9 + uiOffset, m_vIncludes);
    }
    return true;
}

bool ProjectGenerator::passDASMInclude(uint uiOffset)
{
    // Check if supported option
    if (m_ConfigHelper.isASMEnabled()) {
        return passDynamicInclude(10 + uiOffset, m_vIncludes);
    }
    return true;
}

bool ProjectGenerator::passMMXInclude()
{
    // Check if supported option
    if (m_ConfigHelper.getConfigOptionPrefixed("HAVE_MMX")->m_value == "1") {
        return passStaticInclude(8, m_vIncludes);
    }
    return true;
}

bool ProjectGenerator::passDMMXInclude()
{
    // Check if supported option
    if (m_ConfigHelper.getConfigOptionPrefixed("HAVE_MMX")->m_value == "1") {
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
    // Find the dynamic identifier
    uint uiStartPos = m_sInLine.find("$(");
    uint uiEndPos = m_sInLine.find(')', uiStartPos);
    string sPrefix = m_sInLine.substr(0, uiStartPos) + "yes";
    uiStartPos += 2;    // Skip the $(
    string sIdent = m_sInLine.substr(uiStartPos, uiEndPos - uiStartPos);
    // Find the included obj
    uiStartPos = m_sInLine.find_first_not_of("+= \t", uiEndPos + 1);
    if (!passDynamicIncludeLine(uiStartPos, sIdent, m_mUnknowns[sPrefix])) {
        return false;
    }
    // Check if this is a multi line declaration
    while (m_sInLine.back() == '\\') {
        // Get the next line
        getline(m_ifInputFile, m_sInLine);
        // Remove the whitespace
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
    // Find the dynamic identifier
    uint uiStartPos = m_sInLine.find("$(");
    uint uiEndPos = m_sInLine.find(')', uiStartPos);
    string sPrefix = m_sInLine.substr(0, uiStartPos) + "yes";
    uiStartPos += 2;    // Skip the $(
    string sIdent = m_sInLine.substr(uiStartPos, uiEndPos - uiStartPos);
    // Find the included obj
    uiStartPos = m_sInLine.find_first_not_of("+= \t", uiEndPos + 1);
    if (!passDynamicIncludeLine(uiStartPos, sIdent, m_mUnknowns[sPrefix])) {
        return false;
    }
    // Check if this is a multi line declaration
    while (m_sInLine.back() == '\\') {
        // Get the next line
        getline(m_ifInputFile, m_sInLine);
        // Remove the whitespace
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
    string sMakeFile = m_sProjectDir + "MakeFile";
    outputLine("  Generating from Makefile (" + sMakeFile + ")...");
    // Open the input Makefile
    m_ifInputFile.open(sMakeFile);
    if (m_ifInputFile.is_open()) {
        // Read each line in the MakeFile
        while (getline(m_ifInputFile, m_sInLine)) {
            // Check what information is included in the current line
            if (m_sInLine.substr(0, 4) == "OBJS") {
                // Found some c includes
                if (m_sInLine.at(4) == '-') {
                    // Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    // Found some static c includes
                    if (!passCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if ((m_sInLine.substr(0, 11) == "X86ASM-OBJS") || (m_sInLine.substr(0, 9) == "YASM-OBJS")) {
                // Found some YASM includes
                uint uiOffset = (m_sInLine.at(0) == 'X') ? 2 : 0;
                if (m_sInLine.at(9 + uiOffset) == '-') {
                    // Found some dynamic ASM includes
                    if (!passDASMInclude(uiOffset)) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    // Found some static ASM includes
                    if (!passASMInclude(uiOffset)) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.substr(0, 8) == "MMX-OBJS") {
                // Found some ASM includes
                if (m_sInLine.at(8) == '-') {
                    // Found some dynamic MMX includes
                    if (!passDMMXInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    // Found some static MMX includes
                    if (!passMMXInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.substr(0, 7) == "HEADERS") {
                // Found some headers
                if (m_sInLine.at(7) == '-') {
                    // Found some dynamic headers
                    if (!passDHInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    // Found some static headers
                    if (!passHInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.find("BUILT_HEADERS") == 0) {
                // Found some static built headers
                if (!passHInclude(13)) {
                    m_ifInputFile.close();
                    return false;
                }
            } else if (m_sInLine.substr(0, 6) == "FFLIBS") {
                // Found some libs
                if (m_sInLine.at(6) == '-') {
                    // Found some dynamic libs
                    if (!passDLibInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    // Found some static libs
                    if (!passLibInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.find("-OBJS-$") != string::npos) {
                // Found unknown
                if (!passDUnknown()) {
                    m_ifInputFile.close();
                    return false;
                }
            } else if (m_sInLine.find("LIBS-$") != string::npos) {
                // Found Lib unknown
                if (!passDLibUnknown()) {
                    m_ifInputFile.close();
                    return false;
                }
            } else if (m_sInLine.substr(0, 5) == "ifdef") {
                // Check for configuration value
                const uint startPos = m_sInLine.find_first_not_of(" \t", 5);
                uint endPos = m_sInLine.find_first_of(" \t\n\r", startPos + 1);
                endPos = (endPos == string::npos) ? endPos : endPos - startPos;
                string config = m_sInLine.substr(startPos, endPos);
                // Check if the config option is correct
                auto option = m_ConfigHelper.getConfigOptionPrefixed(config);
                if (option == m_ConfigHelper.m_configValues.end()) {
                    outputInfo("Unknown ifdef configuration option (" + config + ")");
                    return false;
                }
                if (option->m_value != "1") {
                    // Skip everything between the ifdefs
                    while (getline(m_ifInputFile, m_sInLine)) {
                        if ((m_sInLine.substr(0, 5) == "endif") || (m_sInLine.substr(0, 4) == "else")) {
                            break;
                        }
                    }
                }
            }
        }
        m_ifInputFile.close();
        return true;
    }
    outputError("Could not open open MakeFile (" + sMakeFile + ")");
    return false;
}

bool ProjectGenerator::passProgramMake()
{
    uint uiChecks = 2;
    while (uiChecks >= 1) {
        // Open the input Makefile
        string sMakeFile = m_sProjectDir + "MakeFile";
        m_ifInputFile.open(sMakeFile);
        if (!m_ifInputFile.is_open()) {
            outputError("Could not open open MakeFile (" + sMakeFile + ")");
        }

        outputLine("  Generating from Makefile (" + sMakeFile + ") for project " + m_sProjectName + "...");
        string sObjTag = "OBJS-" + m_sProjectName;
        uint uiFindPos;
        // Read each line in the MakeFile
        while (getline(m_ifInputFile, m_sInLine)) {
            // Check what information is included in the current line
            if (m_sInLine.substr(0, sObjTag.length()) == sObjTag) {
                // Cut the line so it can be used by default passers
                m_sInLine = m_sInLine.substr(sObjTag.length() - 4);
                if (m_sInLine.at(4) == '-') {
                    // Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    // Found some static c includes
                    if (!passCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (m_sInLine.substr(0, 6) == "FFLIBS") {
                // Found some libs
                if (m_sInLine.at(6) == '-') {
                    // Found some dynamic libs
                    if (!passDLibInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    // Found some static libs
                    if (!passLibInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if ((uiFindPos = m_sInLine.find("eval OBJS-$(prog)")) != string::npos) {
                m_sInLine = m_sInLine.substr(uiFindPos + 13);
                if (m_sInLine.at(4) == '-') {
                    // Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    // Found some static c includes
                    if (!passCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if (((uiFindPos = m_sInLine.find("OBJS-$(1)")) != string::npos) && (uiFindPos == 0)) {
                m_sInLine = m_sInLine.substr(uiFindPos + 5, m_sInLine.find(".o", uiFindPos + 9) + 2 - (uiFindPos + 5));
                if (m_sInLine.at(4) == '-') {
                    // Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    // Found some static c includes
                    if (!passCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            } else if ((uiFindPos = m_sInLine.find("eval OBJS-$(P)")) != string::npos) {
                m_sInLine = m_sInLine.substr(uiFindPos + 10);
                if (m_sInLine.at(4) == '-') {
                    // Found some dynamic c includes
                    if (!passDCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                } else {
                    // Found some static c includes
                    if (!passCInclude()) {
                        m_ifInputFile.close();
                        return false;
                    }
                }
            }
        }
        m_ifInputFile.close();
        if (uiChecks == 2) {
            string sIgnore;
            const string sMakeFolder = "fftools/";
            sMakeFile = m_sProjectDir + sMakeFolder + "MakeFile";
            if (findFile(sMakeFile, sIgnore)) {
                // If using the Makefile in fftools then we need to read both it and the root Makefile
                m_sProjectDir += sMakeFolder;
            } else {
                --uiChecks;
            }
        }
        // When passing the fftools folder some objects are added with fftools folder prefixed to file name
        const string sMakeFolder = "fftools/";
        uint uiPos;
        for (auto itI = m_vIncludes.begin(); itI < m_vIncludes.end(); ++itI) {
            if ((uiPos = itI->find(sMakeFolder)) != string::npos) {
                itI->erase(uiPos, sMakeFolder.length());
            }
        }
        --uiChecks;
    }

    // Program always includes a file named after themselves
    m_vIncludes.push_back(m_sProjectName);
    return true;
}