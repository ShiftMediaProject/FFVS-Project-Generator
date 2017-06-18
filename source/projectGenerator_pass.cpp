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

#include <iostream>
#include <algorithm>
#include <utility>

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