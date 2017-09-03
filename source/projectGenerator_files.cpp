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

bool ProjectGenerator::findSourceFile(const string & sFile, const string & sExtension, string & sRetFileName)
{
    string sFileName;
    sRetFileName = m_sProjectDir + sFile + sExtension;
    if (!findFile(sRetFileName, sFileName)) {
        // Check if this is a built file
        uint uiSPos = m_sProjectDir.rfind('/', m_sProjectDir.length() - 2);
        uiSPos = (uiSPos == string::npos) ? 0 : uiSPos;
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

bool ProjectGenerator::checkProjectFiles()
{
    //Check that all headers are correct
    for (StaticList::iterator itIt = m_vHIncludes.begin(); itIt != m_vHIncludes.end(); itIt++) {
        string sRetFileName;
        if (!findSourceFile(*itIt, ".h", sRetFileName)) {
            outputError("Could not find input header file for object (" + *itIt + ")");
            return false;
        }
        //Update the entry with the found file with complete path
        m_ConfigHelper.makeFileProjectRelative(sRetFileName, *itIt);
    }

    //Check that all C Source are correct
    for (StaticList::iterator itIt = m_vCIncludes.begin(); itIt != m_vCIncludes.end(); itIt++) {
        string sRetFileName;
        if (!findSourceFile(*itIt, ".c", sRetFileName)) {
            outputError("Could not find input C source file for object (" + *itIt + ")");
            return false;
        }
        //Update the entry with the found file with complete path
        m_ConfigHelper.makeFileProjectRelative(sRetFileName, *itIt);
    }

    //Check that all CPP Source are correct
    for (StaticList::iterator itIt = m_vCPPIncludes.begin(); itIt != m_vCPPIncludes.end(); itIt++) {
        string sRetFileName;
        if (!findSourceFile(*itIt, ".cpp", sRetFileName)) {
            outputError("Could not find input C++ source file for object (" + *itIt + ")");
            return false;
        }
        //Update the entry with the found file with complete path
        m_ConfigHelper.makeFileProjectRelative(sRetFileName, *itIt);
    }

    //Check that all ASM Source are correct
    for (StaticList::iterator itIt = m_vASMIncludes.begin(); itIt != m_vASMIncludes.end(); itIt++) {
        string sRetFileName;
        if (!findSourceFile(*itIt, ".asm", sRetFileName)) {
            outputError("Could not find input ASM source file for object (" + *itIt + ")");
            return false;
        }
        //Update the entry with the found file with complete path
        m_ConfigHelper.makeFileProjectRelative(sRetFileName, *itIt);
    }

    //Check the output Unknown Includes and find there corresponding file
    if (!findProjectFiles(m_vIncludes, m_vCIncludes, m_vCPPIncludes, m_vASMIncludes, m_vHIncludes)) {
        return false;
    }

    if (m_ConfigHelper.m_bDCEOnly) {
        //Don't need to check for replace files
        return true;
    }

    //Check all source files associated with replaced config values
    StaticList vReplaceIncludes, vReplaceCPPIncludes, vReplaceCIncludes, vReplaceASMIncludes;
    for (UnknownList::iterator itIt = m_mReplaceIncludes.begin(); itIt != m_mReplaceIncludes.end(); itIt++) {
        vReplaceIncludes.push_back(itIt->first);
    }
    if (!findProjectFiles(vReplaceIncludes, vReplaceCIncludes, vReplaceCPPIncludes, vReplaceASMIncludes, m_vHIncludes)) {
        return false;
    } else {
        //Need to create local files for any replace objects
        if (!createReplaceFiles(vReplaceCIncludes, m_vCIncludes)) {
            return false;
        }
        if (!createReplaceFiles(vReplaceCPPIncludes, m_vCPPIncludes)) {
            return false;
        }
        if (!createReplaceFiles(vReplaceASMIncludes, m_vASMIncludes)) {
            return false;
        }
    }

    return true;
}

bool ProjectGenerator::createReplaceFiles(const StaticList& vReplaceIncludes, StaticList& vExistingIncludes)
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
        string sNewFile = getCopywriteHeader(sFilename + sExtension + " file wrapper for " + m_sProjectName);
        sNewFile += "\n\
\n\
#include \"config.h\"\n\
#if " + sIdents + "\n\
#   include \"" + sPrettyFile + "\"\n\
#endif";
        //Write output project
        if (!makeDirectory(m_ConfigHelper.m_sProjectDirectory + m_sProjectName)) {
            outputError("Failed creating local " + m_sProjectName + " directory");
            return false;
        }
        string sOutFile = m_ConfigHelper.m_sProjectDirectory + m_sProjectName + "/" + sFilename + "_wrap" + sExtension;
        if (!writeToFile(sOutFile, sNewFile)) {
            return false;
        }
        //Add the new file to list of objects
        m_ConfigHelper.makeFileProjectRelative(sOutFile, sOutFile);
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
            m_ConfigHelper.makeFileProjectRelative(sRetFileName, sRetFileName);
            vCIncludes.push_back(sRetFileName);
        } else if (findSourceFile(*itIt, ".cpp", sRetFileName)) {
            //Found a C++ File to include
            if (find(vCPPIncludes.begin(), vCPPIncludes.end(), sRetFileName) != vCPPIncludes.end()) {
                //skip this item
                continue;
            }
            m_ConfigHelper.makeFileProjectRelative(sRetFileName, sRetFileName);
            vCPPIncludes.push_back(sRetFileName);
        } else if (findSourceFile(*itIt, ".asm", sRetFileName)) {
            //Found a ASM File to include
            if (find(vASMIncludes.begin(), vASMIncludes.end(), sRetFileName) != vASMIncludes.end()) {
                //skip this item
                continue;
            }
            m_ConfigHelper.makeFileProjectRelative(sRetFileName, sRetFileName);
            vASMIncludes.push_back(sRetFileName);
        } else if (findSourceFile(*itIt, ".h", sRetFileName)) {
            //Found a H File to include
            if (find(vHIncludes.begin(), vHIncludes.end(), sRetFileName) != vHIncludes.end()) {
                //skip this item
                continue;
            }
            m_ConfigHelper.makeFileProjectRelative(sRetFileName, sRetFileName);
            vHIncludes.push_back(sRetFileName);
        } else {
            outputError("Could not find valid source file for object (" + *itIt + ")");
            return false;
        }
    }
    return true;
}