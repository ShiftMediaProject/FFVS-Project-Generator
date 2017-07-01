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

//This can be used to force all detected DCE values to be output to file
// whether they are enabled in current configuration or not
#define FORCEALLDCE 0

const string asDCETags[] = {"ARCH_", "HAVE_", "CONFIG_", "EXTERNAL_", "INTERNAL_", "INLINE_"};

bool ProjectGenerator::outputProjectDCE(string sProjectName, const StaticList& vIncludeDirs)
{
    cout << "  Generating missing DCE symbols (" << sProjectName << ")..." << endl;
    //Create list of source files to scan
#if !FORCEALLDCE
    StaticList vSearchFiles = m_vCIncludes;
    vSearchFiles.insert(vSearchFiles.end(), m_vCPPIncludes.begin(), m_vCPPIncludes.end());
    vSearchFiles.insert(vSearchFiles.end(), m_vHIncludes.begin(), m_vHIncludes.end());
#else
    StaticList vSearchFiles;
    bool bRecurse = (m_sProjectDir.compare(this->m_ConfigHelper.m_sRootDirectory) != 0);
    findFiles(m_sProjectDir + "*.h", vSearchFiles, bRecurse);
    findFiles(m_sProjectDir + "*.c", vSearchFiles, bRecurse);
    findFiles(m_sProjectDir + "*.cpp", vSearchFiles, bRecurse);
#endif
    //Ensure we can add extra items to the list without needing reallocs
    if (vSearchFiles.capacity() < vSearchFiles.size() + 250) {
        vSearchFiles.reserve(vSearchFiles.size() + 250);
    }

    //Check for DCE constructs
    map<string, DCEParams> mFoundDCEUsage;
    set<string> vNonDCEUsage;
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
        outputProjectDCEFindFunctions(sFile, sProjectName, *itFile, mFoundDCEUsage, bRequiresPreProcess, vNonDCEUsage);
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
                string sFound;
                string sBack = sTemplateFile;
                sTemplateFile = m_sProjectDir + sBack;
                if (!findFile(sTemplateFile, sFound)) {
                    sTemplateFile = m_ConfigHelper.m_sRootDirectory + '/' + sBack;
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

#if !FORCEALLDCE
    //Get a list of all files in current project directory (including subdirectories)
    bool bRecurse = (m_sProjectDir.compare(this->m_ConfigHelper.m_sRootDirectory) != 0);
    vSearchFiles.resize(0);
    findFiles(m_sProjectDir + "*.h", vSearchFiles, bRecurse);
    findFiles(m_sProjectDir + "*.c", vSearchFiles, bRecurse);
    findFiles(m_sProjectDir + "*.cpp", vSearchFiles, bRecurse);
    //Ensure we can add extra items to the list without needing reallocs
    if (vSearchFiles.capacity() < vSearchFiles.size() + 250) {
        vSearchFiles.reserve(vSearchFiles.size() + 250);
    }

    //Check all configurations are enabled early to avoid later lookups of unused functions
    ConfigGenerator::DefaultValuesList mReserved;
    ConfigGenerator::DefaultValuesList mIgnored;
    m_ConfigHelper.buildReplaceValues(mReserved, mIgnored);
    for (map<string, DCEParams>::iterator itDCE = mFoundDCEUsage.begin(); itDCE != mFoundDCEUsage.end(); ) {
        outputProgramDCEsResolveDefine(itDCE->second.sDefine, mReserved);
        if (itDCE->second.sDefine.compare("1") == 0) {
            vNonDCEUsage.insert(itDCE->first);
            //remove from the list
            mFoundDCEUsage.erase(itDCE++);
        } else {
            ++itDCE;
        }
    }
#endif

    //Now we need to find the declaration of each function
    map<string, DCEParams> mFoundDCEFunctions;
    map<string, DCEParams> mFoundDCEVariables;
    if (mFoundDCEUsage.size() > 0) {
        //Search through each included file
        for (StaticList::iterator itFile = vSearchFiles.begin(); itFile < vSearchFiles.end(); itFile++) {
            string sFile;
            if (!loadFromFile(*itFile, sFile)) {
                return false;
            }
            for (map<string, DCEParams>::iterator itDCE = mFoundDCEUsage.begin(); itDCE != mFoundDCEUsage.end(); ) {
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
                        mFoundDCEFunctions[sReturn] = {itDCE->second.sDefine, sFileName};
                    } else {
                        mFoundDCEVariables[sReturn] = {itDCE->second.sDefine, sFileName};
                    }

                    //Remove it from the list
                    mFoundDCEUsage.erase(itDCE++);
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
        mFoundDCEUsage[*itPP] = {"#", *itPP};
    }

    //Check if we failed to find any functions
    if (mFoundDCEUsage.size() > 0) {
        vector<string> vIncludeDirs2 = vIncludeDirs;
        makeDirectory(sProjectName);
        //Get all the files that include functions
        map<string, vector<DCEParams>> mFunctionFiles;
        for (map<string, DCEParams>::iterator itDCE = mFoundDCEUsage.begin(); itDCE != mFoundDCEUsage.end(); itDCE++) {
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
                    //Need to add subdirectory to include list
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

        if (!runCompiler(vIncludeDirs2, sProjectName, mDirectoryObjects, 1)) {
            return false;
        }
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
            map<string, DCEParams> mNewDCEUsage;
            bool bCanIgnore = false;
            outputProjectDCEFindFunctions(sFile, sProjectName, itDCE->first, mNewDCEUsage, bCanIgnore, vNonDCEUsage);
#if !FORCEALLDCE
            for (map<string, DCEParams>::iterator itDCE = mNewDCEUsage.begin(); itDCE != mNewDCEUsage.end(); ) {
                outputProgramDCEsResolveDefine(itDCE->second.sDefine, mReserved);
                if (itDCE->second.sDefine.compare("1") == 0) {
                    vNonDCEUsage.insert(itDCE->first);
                    //remove from the list
                    mNewDCEUsage.erase(itDCE++);
                } else {
                    ++itDCE;
                }
            }
#endif
            for (map<string, DCEParams>::iterator itDCE2 = mNewDCEUsage.begin(); itDCE2 != mNewDCEUsage.end(); itDCE2++) {
                //Add the file to the list
                if (find(itDCE->second.begin(), itDCE->second.end(), itDCE2->first) == itDCE->second.end()) {
                    itDCE->second.push_back({itDCE2->second.sDefine, itDCE2->first});
                    mFoundDCEUsage[itDCE2->first] = itDCE2->second;
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
                            if (mFoundDCEFunctions.find(sReturn) == mFoundDCEFunctions.end()) {
                                mFoundDCEFunctions[sReturn] = {itDCE2->sDefine, sFileName};
                            }
                        } else {
                            if (mFoundDCEVariables.find(sReturn) == mFoundDCEVariables.end()) {
                                mFoundDCEVariables[sReturn] = {itDCE2->sDefine, sFileName};
                            }
                        }
                        //Remove the function from list
                        mFoundDCEUsage.erase(itDCE2->sFile);
                    }
                } else {
                    //Remove the function from list as it was just a preproc file
                    mFoundDCEUsage.erase(itDCE2->sFile);
                }
            }
        }

        //Delete the created temp files
        deleteFolder(sProjectName);
    }

    //Get any required hard coded values
    map<string, DCEParams> mBuiltDCEFunctions;
    map<string, DCEParams> mBuiltDCEVariables;
    buildProjectDCEs(sProjectName, mBuiltDCEFunctions, mBuiltDCEVariables);
    for (map<string, DCEParams>::iterator itI = mBuiltDCEFunctions.begin(); itI != mBuiltDCEFunctions.end(); itI++) {
        //Add to found list if not already found
        if (mFoundDCEFunctions.find(itI->first) == mFoundDCEFunctions.end()) {
#if !FORCEALLDCE
            outputProgramDCEsResolveDefine(itI->second.sDefine, mReserved);
            if (itI->second.sDefine.compare("1") == 0) {
                vNonDCEUsage.insert(itI->first);
            } else {
                mFoundDCEFunctions[itI->first] = itI->second;
            }
#else
            mFoundDCEFunctions[itI->first] = itI->second;
#endif
        }
        //Remove from unfound list
        if (mFoundDCEUsage.find(itI->first) == mFoundDCEUsage.end())
            mFoundDCEUsage.erase(itI->first);
    }
    for (map<string, DCEParams>::iterator itI = mBuiltDCEVariables.begin(); itI != mBuiltDCEVariables.end(); itI++) {
        //Add to found list if not already found
        if (mFoundDCEVariables.find(itI->first) == mFoundDCEVariables.end()) {
#if !FORCEALLDCE
            outputProgramDCEsResolveDefine(itI->second.sDefine, mReserved);
            if (itI->second.sDefine.compare("1") == 0) {
                vNonDCEUsage.insert(itI->first);
            } else {
                mFoundDCEVariables[itI->first] = itI->second;
            }
#else
            mFoundDCEVariables[itI->first] = itI->second;
#endif
        }
        //Remove from unfound list
        if (mFoundDCEUsage.find(itI->first) == mFoundDCEUsage.end())
            mFoundDCEUsage.erase(itI->first);
    }

    //Check if we failed to find anything (even after using buildDCEs)
    if (mFoundDCEUsage.size() > 0) {
        for (map<string, DCEParams>::iterator itDCE = mFoundDCEUsage.begin(); itDCE != mFoundDCEUsage.end(); itDCE++) {
            cout << "  Warning: Failed to find function definition for " << itDCE->first << ", " << itDCE->second.sFile << endl;
            //Just output a blank definition and hope it works
            mFoundDCEFunctions["void " + itDCE->first + "()"] = {itDCE->second.sDefine, itDCE->second.sFile};
        }
    }

    //Add definition to new file
    if ((mFoundDCEFunctions.size() > 0) || (mFoundDCEVariables.size() > 0)) {
        vector<DCEParams> vIncludedHeaders;
        string sDCEOutFile;
        //Loop through all functions
        for (map<string, DCEParams>::iterator itDCE = mFoundDCEFunctions.begin(); itDCE != mFoundDCEFunctions.end(); itDCE++) {
            bool bUsePreProc = (itDCE->second.sDefine.length() > 1) && (itDCE->second.sDefine.compare("0") != 0);
            //Only include preprocessor guards if its a reserved option
            if (bUsePreProc) {
                sDCEOutFile += "#if !(" + itDCE->second.sDefine + ")\n";
            }
            if (itDCE->second.sFile.find(".h") != string::npos) {
                //Include header files only once
                if (!bUsePreProc) {
                    itDCE->second.sDefine = "";
                }
                vector<DCEParams>::iterator vitH = find(vIncludedHeaders.begin(), vIncludedHeaders.end(), itDCE->second.sFile);
                if (vitH == vIncludedHeaders.end()) {
                    vIncludedHeaders.push_back({itDCE->second.sDefine, itDCE->second.sFile});
                } else {
                    outputProgramDCEsCombineDefine(vitH->sDefine, itDCE->second.sDefine, vitH->sDefine);
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
            bool bUsePreProc = true; bool bEnabled = false;
#if !FORCEALLDCE
            bUsePreProc = (itDCE->second.sDefine.length() > 1) && (itDCE->second.sDefine.compare("0") != 0);
            ConfigGenerator::ValuesList::iterator ConfigOpt = m_ConfigHelper.getConfigOptionPrefixed(itDCE->second.sDefine);
            if (ConfigOpt == m_ConfigHelper.m_vConfigValues.end()) {
                //This config option doesn't exist so it potentially requires the header file to be included first
            } else {
                bool bReserved = (mReserved.find(ConfigOpt->m_sPrefix + ConfigOpt->m_sOption) != mReserved.end());
                if (!bReserved) {
                    bEnabled = (ConfigOpt->m_sValue.compare("1") == 0);
                }
                bUsePreProc = bUsePreProc || bReserved;
            }
#endif
            //Include only those options that are currently disabled
            if (!bEnabled) {
                //Only include preprocessor guards if its a reserved option
                if (bUsePreProc) {
                    sDCEOutFile += "#if !(" + itDCE->second.sDefine + ")\n";
                } else {
                    itDCE->second.sDefine = "";
                }
                //Include header files only once
                vector<DCEParams>::iterator vitH = find(vIncludedHeaders.begin(), vIncludedHeaders.end(), itDCE->second.sFile);
                if (vitH == vIncludedHeaders.end()) {
                    vIncludedHeaders.push_back({itDCE->second.sDefine, itDCE->second.sFile});
                } else {
                    outputProgramDCEsCombineDefine(vitH->sDefine, itDCE->second.sDefine, vitH->sDefine);
                }
                sDCEOutFile += itDCE->first + " = {0};\n";
                if (bUsePreProc) {
                    sDCEOutFile += "#endif\n";
                }
            }
        }

        string sFinalDCEOutFile = getCopywriteHeader(sProjectName + " DCE definitions") + '\n';
        sFinalDCEOutFile += "\n#include \"config.h\"\n\n";
        //Add all header files (goes backwards to avoid bug in header include order in avcodec between vp9.h and h264pred.h)
        for (vector<DCEParams>::iterator vitHeaders = vIncludedHeaders.end(); vitHeaders > vIncludedHeaders.begin(); ) {
            --vitHeaders;
            if (vitHeaders->sDefine.length() > 0) {
                sFinalDCEOutFile += "#if !(" + vitHeaders->sDefine + ")\n";
            }
            sFinalDCEOutFile += "#include \"" + vitHeaders->sFile + "\"\n";
            if (vitHeaders->sDefine.length() > 0) {
                sFinalDCEOutFile += "#endif\n";
            }
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

void ProjectGenerator::outputProjectDCEFindFunctions(const string & sFile, const string & sProjectName, const string & sFileName, map<string, DCEParams> & mFoundDCEUsage, bool & bRequiresPreProcess, set<string> & vNonDCEUsage)
{
    const string asDCETags2[] = {"if (", "if(", "if ((", "if(("};
    StaticList vFuncIdents = {"ff_"};
    if ((sProjectName.compare("ffmpeg") == 0) || (sProjectName.compare("ffplay") == 0) || (sProjectName.compare("ffprobe") == 0) ||
        (sProjectName.compare("avconv") == 0) || (sProjectName.compare("avplay") == 0) || (sProjectName.compare("avprobe") == 0)) {
        vFuncIdents.push_back("avcodec_");
        vFuncIdents.push_back("avdevice_");
        vFuncIdents.push_back("avfilter_");
        vFuncIdents.push_back("avformat_");
        vFuncIdents.push_back("avutil_");
        vFuncIdents.push_back("av_");
        vFuncIdents.push_back("avresample_");
        vFuncIdents.push_back("postproc_");
        vFuncIdents.push_back("swri_");
        vFuncIdents.push_back("swresample_");
        vFuncIdents.push_back("swscale_");
    } else if (sProjectName.compare("libavcodec") == 0) {
        vFuncIdents.push_back("avcodec_");
    } else if (sProjectName.compare("libavdevice") == 0) {
        vFuncIdents.push_back("avdevice_");
    } else if (sProjectName.compare("libavfilter") == 0) {
        vFuncIdents.push_back("avfilter_");
    } else if (sProjectName.compare("libavformat") == 0) {
        vFuncIdents.push_back("avformat_");
    } else if (sProjectName.compare("libavutil") == 0) {
        vFuncIdents.push_back("avutil_");
        vFuncIdents.push_back("av_");
    } else if (sProjectName.compare("libavresample") == 0) {
        vFuncIdents.push_back("avresample_");
    } else if (sProjectName.compare("libpostproc") == 0) {
        vFuncIdents.push_back("postproc_");
    } else if (sProjectName.compare("libswresample") == 0) {
        vFuncIdents.push_back("swri_");
        vFuncIdents.push_back("swresample_");
    } else if (sProjectName.compare("libswscale") == 0) {
        vFuncIdents.push_back("swscale_");
    }
    struct InternalDCEParams
    {
        DCEParams params;
        vector<uint> vLocations;
    };
    map<string, InternalDCEParams> mInternalList;
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
                    uiFindPos2 = sFile.find_first_of(sEndLine + ';', uiFindPos + 1);
                    if (sFile.at(uiFindPos2) == ';') {
                        ++uiFindPos2;  //must include the ;
                    } else {
                        //Must check if next line was also an if
                        uint uiFindPos5 = uiFindPos;
                        while ((sFile.at(uiFindPos5) == 'i') && (sFile.at(uiFindPos5 + 1) == 'f')) {
                            //Get the define tag
                            uiFindPos5 = sFile.find('(', uiFindPos5 + 2);
                            uiFindPos2 = sFile.find(')', uiFindPos5 + 1);
                            //Skip any '(' found within the parameters itself
                            uiFindPos3 = sFile.find('(', uiFindPos5 + 1);
                            while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiFindPos2)) {
                                uiFindPos3 = sFile.find('(', uiFindPos3 + 1);
                                uiFindPos2 = sFile.find(')', uiFindPos2 + 1);
                            }
                            uiFindPos5 = sFile.find_first_not_of(sWhiteSpace, uiFindPos2 + 1);
                            if (sFile.at(uiFindPos5) == '{') {
                                //Need to get the entire block of code being wrapped
                                uiFindPos2 = sFile.find('}', uiFindPos5 + 1);
                                //Skip any '{' found within the parameters itself
                                uint uiFindPos6 = sFile.find('{', uiFindPos5 + 1);
                                while ((uiFindPos6 != string::npos) && (uiFindPos6 < uiFindPos2)) {
                                    uiFindPos6 = sFile.find('{', uiFindPos6 + 1);
                                    uiFindPos2 = sFile.find('}', uiFindPos2 + 1);
                                }
                                break;
                            } else {
                                //This is a single line of code
                                uiFindPos2 = sFile.find_first_of(sEndLine + ';', uiFindPos5 + 1);
                                if (sFile.at(uiFindPos2) == ';') {
                                    ++uiFindPos2;  //must include the ;
                                    break;
                                }
                            }
                        }
                    }
                }
                sCode = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos);
                uint uiFindBack = uiFindPos;

                //Get name of any functions
                for (StaticList::iterator itIdent = vFuncIdents.begin(); itIdent < vFuncIdents.end(); itIdent++) {
                    uiFindPos = sCode.find(*itIdent);
                    while (uiFindPos != string::npos) {
                        bool bValid = false;
                        //Check if this is a valid function call
                        uint uiFindPos3 = sCode.find_first_of(sNonName, uiFindPos + 1);
                        if ((uiFindPos3 != 0) && (uiFindPos3 != string::npos)) {
                            uint uiFindPos4 = sCode.find_last_of(sNonName, uiFindPos3 - 1);
                            uiFindPos4 = (uiFindPos4 == string::npos) ? 0 : uiFindPos4 + 1;
                            //Check if valid function
                            if (uiFindPos4 == uiFindPos) {
                                uiFindPos4 = sCode.find_first_not_of(sWhiteSpace, uiFindPos3);
                                if (sCode.at(uiFindPos4) == '(') {
                                    bValid = true;
                                } else if (sCode.at(uiFindPos4) == ';') {
                                    uiFindPos4 = sCode.find_last_not_of(sWhiteSpace, uiFindPos - 1);
                                    if (sCode.at(uiFindPos4) == '=') {
                                        bValid = true;
                                    }
                                } else if (sCode.at(uiFindPos4) == '#') {
                                    //Check if this is a macro expansion
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
                            map<string, InternalDCEParams>::iterator itFind = mInternalList.find(sAdd);
                            uint uiValuePosition = uiFindBack + uiFindPos;
                            if (itFind == mInternalList.end()) {
                                //Check that another non DCE instance hasn't been found
                                if (vNonDCEUsage.find(sAdd) == vNonDCEUsage.end()) {
                                    mInternalList[sAdd] = {{sFuncDefine, sFileName}, {uiValuePosition}};
                                }
                            } else {
                                string sRetDefine;
                                outputProgramDCEsCombineDefine(itFind->second.params.sDefine, sFuncDefine, sRetDefine);
                                mInternalList[sAdd].params.sDefine = sRetDefine;
                                mInternalList[sAdd].vLocations.push_back(uiValuePosition);
                            }
                        }
                        //Search for next occurrence
                        uiFindPos = sCode.find(*itIdent, uiFindPos + 1);
                    }
                }

                //Search for next occurrence
                uiFindPos = sFile.find(sSearch, uiFindPos2 + 1);
            }
        }
    }

    //Search for usage that is not effected by DCE
    for (StaticList::iterator itIdent = vFuncIdents.begin(); itIdent < vFuncIdents.end(); itIdent++) {
        uint uiFindPos = sFile.find(*itIdent);
        while (uiFindPos != string::npos) {
            bool bValid = false;
            //Check if this is a valid value
            uint uiFindPos3 = sFile.find_first_of(sNonName, uiFindPos + 1);
            if (uiFindPos3 != string::npos) {
                uint uiFindPos4 = sFile.find_last_of(sNonName, uiFindPos3 - 1);
                uiFindPos4 = (uiFindPos4 == string::npos) ? 0 : uiFindPos4 + 1;
                if (uiFindPos4 == uiFindPos) {
                    string sBS = sFile.substr(uiFindPos, uiFindPos3 - uiFindPos);
                    uiFindPos4 = sFile.find_first_not_of(sWhiteSpace, uiFindPos3);
                    //Check if valid function
                    if (sFile.at(uiFindPos4) == '(') {
                        //Check if function call or declaration (a function call must be inside a function {})
                        uint uiCheck1 = sFile.find_last_of('{', uiFindPos);
                        if (uiCheck1 != string::npos) {
                            uint uiCheck2 = sFile.find_last_of('}', uiFindPos);
                            if ((uiCheck2 == string::npos) || (uiCheck1 > uiCheck2)) {
                                bValid = true;
                            }
                        }
                    } else if (sFile.at(uiFindPos4) == ';') {
                        uiFindPos4 = sFile.find_last_not_of(sWhiteSpace, uiFindPos4 - 1);
                        if (sFile.at(uiFindPos4) == '=') {
                            bValid = true;
                        }
                    }
                }
            }
            if (bValid) {
                string sAdd = sFile.substr(uiFindPos, uiFindPos3 - uiFindPos);
                //Check if already added
                map<string, InternalDCEParams>::iterator itFind = mInternalList.find(sAdd);
                if (itFind == mInternalList.end()) {
                    vNonDCEUsage.insert(sAdd);
                    //Remove from external list as well
                    //WARNING: Assumes that there is not 2 values with the same name but local visibility in 2 different files
                    if (mFoundDCEUsage.find(sAdd) != mFoundDCEUsage.end()) {
                        mFoundDCEUsage.erase(sAdd);
                    }
                } else {
                    //Check if this location was found in a DCE
                    bool bFound = false;
                    for (vector<uint>::iterator itI = mInternalList[sAdd].vLocations.begin(); itI < mInternalList[sAdd].vLocations.end(); itI++) {
                        if (*itI == uiFindPos) {
                            bFound = true;
                        }
                    }
                    if (!bFound) {
                        vNonDCEUsage.insert(sAdd);
                        //Remove from external list as well
                        if (mFoundDCEUsage.find(sAdd) != mFoundDCEUsage.end()) {
                            mFoundDCEUsage.erase(sAdd);
                        }
                        //Needs to remove it from internal list as it is also found in non-DCE section
                        mInternalList.erase(itFind);
                    }
                }
            }
            //Search for next occurrence
            uiFindPos = sFile.find(*itIdent, uiFindPos + 1);
        }
    }

    //Add all the found internal DCE values to the return list
    for (map<string, InternalDCEParams>::iterator itI = mInternalList.begin(); itI != mInternalList.end(); itI++) {
        map<string, DCEParams>::iterator itFind = mFoundDCEUsage.find(itI->first);
        if (itFind == mFoundDCEUsage.end()) {
            mFoundDCEUsage[itI->first] = itI->second.params;
        } else {
            string sRetDefine;
            outputProgramDCEsCombineDefine(itFind->second.sDefine, itI->second.params.sDefine, sRetDefine);
            mFoundDCEUsage[itI->first] = {sRetDefine, itFind->second.sFile};
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

    //Need to search through for !, !=, ==, &&, || in correct order of precendence
    const char acOps[] = {'!', '=', '&', '|'};
    for (unsigned uiOp = 0; uiOp < sizeof(acOps) / sizeof(char); uiOp++) {
        uiStartTag = sDefine.find(acOps[uiOp]);
        while (uiStartTag != string::npos) {
            //Check for != or ==
            if (sDefine.at(uiStartTag + 1) == '=') {
                ++uiStartTag;
            }
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
            if (sDefine.at(uiStartTag) == '!') {
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
                //Check for != or ==
                if (sDefine.at(uiStartTag) == '=') {
                    --uiStartTag;
                }
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
                    uiLeft = (uiLeft == string::npos) ? 0 : uiLeft + 1;
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
                } else if (acOps[uiOp] == '|') {
                    if ((sLeft.compare("1") == 0) || (sRight.compare("1") == 0)) {
                        //1||X or X||1 == 1
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
                } else if (acOps[uiOp] == '!') {
                    if ((sLeft.compare("1") == 0) && (sRight.compare("1") == 0)) {
                        //1!=1 == 0
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '0');
                        uiStartTag = uiLeft;
                    } else if ((sLeft.compare("1") == 0) && (sRight.compare("0") == 0)) {
                        //1!=0 == 1
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '1');
                        uiStartTag = uiLeft;
                    } else if ((sLeft.compare("0") == 0) && (sRight.compare("1") == 0)) {
                        //0!=1 == 1
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '1');
                        uiStartTag = uiLeft;
                    } else {
                        //X!=X = (X!=X)
                        if (uiRight == string::npos) {
                            sDefine += ')';
                        } else {
                            sDefine.insert(uiRight, 1, ')');
                        }
                        sDefine.insert(uiLeft, 1, '(');
                        uiStartTag += 2;
                    }
                } else if (acOps[uiOp] == '=') {
                    if ((sLeft.compare("1") == 0) && (sRight.compare("1") == 0)) {
                        //1==1 == 1
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '1');
                        uiStartTag = uiLeft;
                    } else if ((sLeft.compare("1") == 0) && (sRight.compare("0") == 0)) {
                        //1==0 == 0
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '0');
                        uiStartTag = uiLeft;
                    } else if ((sLeft.compare("0") == 0) && (sRight.compare("1") == 0)) {
                        //0==1 == 0
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '0');
                        uiStartTag = uiLeft;
                    } else {
                        //X==X == (X==X)
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
    uint uiStartTag = sDefine.find_first_not_of(sPreProcessor);
    while (uiStartTag != string::npos) {
        //Check if each conditional is valid
        bool bValid = false;
        for (unsigned uiTagk = 0; uiTagk < sizeof(asDCETags) / sizeof(string); uiTagk++) {
            if (sDefine.find(asDCETags[uiTagk], uiStartTag) == uiStartTag) {
                // We have found a valid additional tag
                bValid = true;
                break;
            }
        }

        if (!bValid) {
            //Get right tag
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

            if (!bValid && isupper(sDefine.at(uiStartTag))) {
                string sRight(sDefine.substr(uiStartTag, uiRight - uiStartTag));
                if ((sRight.find("AV_") != 0) && (sRight.find("FF_") != 0)) {
                    cout << "  Warning: Found unknown macro in DCE condition " << sRight << endl;
                }
            }

            while ((uiStartTag != 0) && ((sDefine.at(uiStartTag - 1) == '(') && (sDefine.at(uiRight) == ')'))) {
                //Remove the ()'s
                --uiStartTag;
                ++uiRight;
            }
            if ((uiStartTag == 0) || ((sDefine.at(uiStartTag - 1) == '(') && (sDefine.at(uiRight) != ')'))) {
                //Trim operators after tag instead of before it
                uiRight = sDefine.find_first_not_of("|&!=", uiRight + 1); //Must not search for ()'s
            } else {
                //Trim operators before tag
                uiStartTag = sDefine.find_last_not_of("|&!=", uiStartTag - 1) + 1; //Must not search for ()'s
            }
            sDefine.erase(uiStartTag, uiRight - uiStartTag);
            uiStartTag = sDefine.find_first_not_of(sPreProcessor, uiStartTag);
        } else {
            uiStartTag = sDefine.find_first_of(sPreProcessor, uiStartTag + 1);
            uiStartTag = (uiStartTag != string::npos) ? sDefine.find_first_not_of(sPreProcessor, uiStartTag + 1) : uiStartTag;
        }
    }
}

void ProjectGenerator::outputProgramDCEsCombineDefine(const string & sDefine, const string & sDefine2, string & sRetDefine)
{
    if (sDefine.compare(sDefine2) != 0) {
        //Check if either string contains the other
        if ((sDefine.find(sDefine2) != string::npos) || (sDefine2.length() == 0)) {
            //Keep the existing one
            sRetDefine = sDefine;
        } else if ((sDefine2.find(sDefine) != string::npos) || (sDefine.length() == 0)) {
            //Use the new one in place of the original
            sRetDefine = sDefine2;
        } else {
            //Add the additional define
            string sRet = "||";
            if (sDefine.find('&') != string::npos) {
                sRet = '(' + sDefine + ')' + sRet;
            } else {
                sRet = sDefine + sRet;
            }
            if (sDefine2.find('&') != string::npos) {
                sRet += '(' + sDefine2 + ')';
            } else {
                sRet += sDefine2;
            }
            sRetDefine = sRet;
        }
    } else {
        sRetDefine = sDefine;
    }
}