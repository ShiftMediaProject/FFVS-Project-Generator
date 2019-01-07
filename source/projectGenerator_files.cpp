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

bool ProjectGenerator::findSourceFile(const string& sFile, const string& sExtension, string& sRetFileName) const
{
    string sFileName;
    sRetFileName = m_sProjectDir + sFile + sExtension;
    if (!findFile(sRetFileName, sFileName)) {
        // Check if this is a built file
        uint uiSPos = m_sProjectDir.rfind('/', m_sProjectDir.length() - 2);
        uiSPos = (uiSPos == string::npos) ? 0 : uiSPos + 1;
        string sProjectName = m_sProjectDir.substr(uiSPos);
        sProjectName = (m_sProjectDir != "./") ? sProjectName : "";
        sRetFileName = m_ConfigHelper.m_solutionDirectory + sProjectName + sFile + sExtension;
        if (!findFile(sRetFileName, sFileName)) {
            // Check if this file already includes the project folder in its name
            if (sFile.find(sProjectName) != string::npos) {
                sRetFileName =
                    m_sProjectDir + sFile.substr(sFile.find(sProjectName) + sProjectName.length()) + sExtension;
                return findFile(sRetFileName, sFileName);
            }
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::findSourceFiles(const string& sFile, const string& sExtension, vector<string>& vRetFiles) const
{
    string sFileName = m_sProjectDir + sFile + sExtension;
    return findFiles(sFileName, vRetFiles);
}

bool ProjectGenerator::checkProjectFiles()
{
    // Check that all headers are correct
    for (auto& m_vHInclude : m_vHIncludes) {
        string sRetFileName;
        if (!findSourceFile(m_vHInclude, ".h", sRetFileName)) {
            outputError("Could not find input header file for object (" + m_vHInclude + ")");
            return false;
        }
        // Update the entry with the found file with complete path
        m_ConfigHelper.makeFileProjectRelative(sRetFileName, m_vHInclude);
    }

    // Check that all C Source are correct
    for (auto& m_vCInclude : m_vCIncludes) {
        string sRetFileName;
        if (!findSourceFile(m_vCInclude, ".c", sRetFileName)) {
            outputError("Could not find input C source file for object (" + m_vCInclude + ")");
            return false;
        }
        // Update the entry with the found file with complete path
        m_ConfigHelper.makeFileProjectRelative(sRetFileName, m_vCInclude);
    }

    // Check that all CPP Source are correct
    for (auto& m_vCPPInclude : m_vCPPIncludes) {
        string sRetFileName;
        if (!findSourceFile(m_vCPPInclude, ".cpp", sRetFileName)) {
            outputError("Could not find input C++ source file for object (" + m_vCPPInclude + ")");
            return false;
        }
        // Update the entry with the found file with complete path
        m_ConfigHelper.makeFileProjectRelative(sRetFileName, m_vCPPInclude);
    }

    // Check that all ASM Source are correct
    for (auto& m_vASMInclude : m_vASMIncludes) {
        string sRetFileName;
        if (!findSourceFile(m_vASMInclude, ".asm", sRetFileName)) {
            outputError("Could not find input ASM source file for object (" + m_vASMInclude + ")");
            return false;
        }
        // Update the entry with the found file with complete path
        m_ConfigHelper.makeFileProjectRelative(sRetFileName, m_vASMInclude);
    }

    // Check the output Unknown Includes and find there corresponding file
    if (!findProjectFiles(m_vIncludes, m_vCIncludes, m_vCPPIncludes, m_vASMIncludes, m_vHIncludes)) {
        return false;
    }

    if (m_ConfigHelper.m_onlyDCE) {
        // Don't need to check for replace files
        return true;
    }

    // Check all source files associated with replaced config values
    StaticList vReplaceIncludes, vReplaceCPPIncludes, vReplaceCIncludes, vReplaceASMIncludes;
    for (auto& m_mReplaceInclude : m_mReplaceIncludes) {
        vReplaceIncludes.push_back(m_mReplaceInclude.first);
    }
    if (!findProjectFiles(
            vReplaceIncludes, vReplaceCIncludes, vReplaceCPPIncludes, vReplaceASMIncludes, m_vHIncludes)) {
        return false;
    }
    // Need to create local files for any replace objects
    if (!createReplaceFiles(vReplaceCIncludes, m_vCIncludes)) {
        return false;
    }
    if (!createReplaceFiles(vReplaceCPPIncludes, m_vCPPIncludes)) {
        return false;
    }
    if (!createReplaceFiles(vReplaceASMIncludes, m_vASMIncludes)) {
        return false;
    }
    return true;
}

bool ProjectGenerator::createReplaceFiles(const StaticList& vReplaceIncludes, StaticList& vExistingIncludes)
{
    for (const auto& vReplaceInclude : vReplaceIncludes) {
        // Check hasnt already been included as a fixed object
        if (find(vExistingIncludes.begin(), vExistingIncludes.end(), vReplaceInclude) != vExistingIncludes.end()) {
            // skip this item
            continue;
        }
        // Convert file to format required to search ReplaceIncludes
        uint uiExtPos = vReplaceInclude.rfind('.');
        uint uiCutPos = vReplaceInclude.rfind('/') + 1;
        string sFilename = vReplaceInclude.substr(uiCutPos, uiExtPos - uiCutPos);
        string sExtension = vReplaceInclude.substr(uiExtPos);
        string sOutFile = m_ConfigHelper.m_solutionDirectory + m_sProjectName + "/" + sFilename + "_wrap" + sExtension;
        string sNewOutFile;
        m_ConfigHelper.makeFileProjectRelative(sOutFile, sNewOutFile);
        // Check hasnt already been included as a wrapped object
        if (find(vExistingIncludes.begin(), vExistingIncludes.end(), sNewOutFile) != vExistingIncludes.end()) {
            // skip this item
            outputInfo(sNewOutFile);
            continue;
        }
        // Find the file in the original list
        string sOrigName;
        for (auto& m_mReplaceInclude : m_mReplaceIncludes) {
            if (m_mReplaceInclude.first.find(sFilename) != string::npos) {
                sOrigName = m_mReplaceInclude.first;
                break;
            }
        }
        if (sOrigName.length() == 0) {
            outputError("Could not find original file name for source file (" + sFilename + ")");
            return false;
        }
        // Get the files dynamic config requirement
        string sIdents;
        for (auto itIdents = m_mReplaceIncludes[sOrigName].begin(); itIdents < m_mReplaceIncludes[sOrigName].end();
             ++itIdents) {
            sIdents += *itIdents;
            if ((itIdents + 1) < m_mReplaceIncludes[sOrigName].end()) {
                sIdents += " || ";
            }
        }
        // Create new file to wrap input object
        string sPrettyFile = "../" + vReplaceInclude;
        string sNewFile = getCopywriteHeader(sFilename + sExtension + " file wrapper for " + m_sProjectName);
        sNewFile += "\n\
\n\
#include \"config.h\"\n\
#if " + sIdents +
            "\n\
#   include \"" +
            sPrettyFile + "\"\n\
#endif";
        // Write output project
        if (!makeDirectory(m_ConfigHelper.m_solutionDirectory + m_sProjectName)) {
            outputError("Failed creating local " + m_sProjectName + " directory");
            return false;
        }
        if (!writeToFile(sOutFile, sNewFile)) {
            return false;
        }
        // Add the new file to list of objects
        vExistingIncludes.push_back(sNewOutFile);
    }
    return true;
}

bool ProjectGenerator::findProjectFiles(const StaticList& vIncludes, StaticList& vCIncludes, StaticList& vCPPIncludes,
    StaticList& vASMIncludes, StaticList& vHIncludes) const
{
    for (const auto& vInclude : vIncludes) {
        string sRetFileName;
        if (findSourceFile(vInclude, ".c", sRetFileName)) {
            // Found a C File to include
            if (find(vCIncludes.begin(), vCIncludes.end(), sRetFileName) != vCIncludes.end()) {
                // skip this item
                continue;
            }
            m_ConfigHelper.makeFileProjectRelative(sRetFileName, sRetFileName);
            vCIncludes.push_back(sRetFileName);
        } else if (findSourceFile(vInclude, ".cpp", sRetFileName)) {
            // Found a C++ File to include
            if (find(vCPPIncludes.begin(), vCPPIncludes.end(), sRetFileName) != vCPPIncludes.end()) {
                // skip this item
                continue;
            }
            m_ConfigHelper.makeFileProjectRelative(sRetFileName, sRetFileName);
            vCPPIncludes.push_back(sRetFileName);
        } else if (findSourceFile(vInclude, ".asm", sRetFileName)) {
            // Found a ASM File to include
            if (find(vASMIncludes.begin(), vASMIncludes.end(), sRetFileName) != vASMIncludes.end()) {
                // skip this item
                continue;
            }
            m_ConfigHelper.makeFileProjectRelative(sRetFileName, sRetFileName);
            vASMIncludes.push_back(sRetFileName);
        } else if (findSourceFile(vInclude, ".h", sRetFileName)) {
            // Found a H File to include
            if (find(vHIncludes.begin(), vHIncludes.end(), sRetFileName) != vHIncludes.end()) {
                // skip this item
                continue;
            }
            m_ConfigHelper.makeFileProjectRelative(sRetFileName, sRetFileName);
            vHIncludes.push_back(sRetFileName);
        } else {
            outputError("Could not find valid source file for object (" + vInclude + ")");
            return false;
        }
    }
    return true;
}