/*
 * copyright (c) 2014 Matthew Oliver
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

#ifndef _PROJECTGENERATOR_H_
#define _PROJECTGENERATOR_H_

#include "configGenerator.h"

#include <fstream>
#include <set>

class ProjectGenerator
{
private:
    typedef vector<string> StaticList;
    typedef map<string, StaticList> UnknownList;
    ifstream        m_ifInputFile;
    string          m_sInLine;
    StaticList      m_vIncludes;
    StaticList      m_vCPPIncludes;
    StaticList      m_vCIncludes;
    StaticList      m_vASMIncludes;
    StaticList      m_vHIncludes;
    UnknownList     m_mReplaceIncludes;
    StaticList      m_vLibs;
    UnknownList     m_mUnknowns;
    string          m_sProjectDir;

    map<string, StaticList> m_mProjectLibs;

    ConfigGenerator::DefaultValuesList m_ReplaceValues;

    string m_sTemplateDirectory;

public:

    ConfigGenerator m_ConfigHelper;

    bool passAllMake();

    void deleteCreatedFiles();

private:

    bool outputProject();

    bool outputProgramProject(const string& sProjectName, const string& sDestinationFile, const string& sDestinationFilterFile);

    bool outputSolution();

    bool passStaticIncludeObject(uint & uiStartPos, uint & uiEndPos, StaticList & vStaticIncludes);

    bool passStaticIncludeLine(uint uiStartPos, StaticList & vStaticIncludes);

    bool passStaticInclude(uint uiILength, StaticList & vStaticIncludes);

    bool passDynamicIncludeObject(uint & uiStartPos, uint & uiEndPos, string & sIdent, StaticList & vIncludes);

    bool passDynamicIncludeLine(uint uiStartPos, string & sIdent, StaticList & vIncludes);

    bool passDynamicInclude(uint uiILength, StaticList & vIncludes);

    bool passCInclude();

    bool passDCInclude();

    bool passYASMInclude();

    bool passDYASMInclude();

    bool passMMXInclude();

    bool passDMMXInclude();

    bool passHInclude(uint uiCutPos = 7);

    bool passDHInclude();

    bool passLibInclude();

    bool passDLibInclude();

    bool passDUnknown();

    bool passDLibUnknown();

    bool passMake();

    bool passProgramMake(const string& sProjectName);

    bool findSourceFile(const string & sFile, const string & sExtension, string & sRetFileName);

    bool findSourceFiles(const string & sFile, const string & sExtension, vector<string> & vRetFiles);

    /**
     * Makes a files path relative to the project directory.
     * @remark Assumes input file path is relative to the generator.
     * @param       sFileName    Filename of the file.
     * @param [out] sRetFileName Filename with the path modified.
     */
    void makeFileProjectRelative(const string & sFileName, string & sRetFileName);

    /**
     * Makes a files path relative to the generator directory.
     * @remark Assumes input file path is relative to the project.
     * @param       sFileName    Filename of the file.
     * @param [out] sRetFileName Filename with the path modified.
     */
    void makeFileGeneratorRelative(const string & sFileName, string & sRetFileName);

    void buildInterDependenciesHelper(const StaticList & vConfigOptions, const StaticList & vAddDeps, StaticList & vLibs);

    void buildInterDependencies(const string & sProjectName, StaticList & vLibs);

    void buildDependencies(const string & sProjectName, StaticList & vLibs, StaticList & vAddLibs);

    void buildDependencyDirs(const string & sProjectName, StaticList & vIncludeDirs, StaticList & vLib32Dirs, StaticList & vLib64Dirs);

    void buildProjectDependencies(const string & sProjectName, map<string, bool> & mProjectDeps);

    void buildProjectGUIDs(map<string, string> & mKeys);

    struct DCEParams
    {
        string sDefine;
        string sFile;

        bool operator==(const string & sCompare) { return (sFile.compare(sCompare) == 0); }
    };

    /**
     * Builds project specific DCE functions and variables that are not automatically detected.
     * @param       sProjectName    Name of the project.
     * @param [out] mDCEDefinitions The return list of built DCE functions.
     * @param [out] mDCEVariables   The return list of built DCE variables.
     */
    void buildProjectDCEs(const string & sProjectName, map<string, DCEParams> & mDCEDefinitions, map<string, DCEParams> & mDCEVariables);

    bool checkProjectFiles(const string& sProjectName);

    bool createReplaceFiles(const StaticList& vReplaceIncludes, StaticList& vExistingIncludes, const string& sProjectName);

    bool findProjectFiles(const StaticList& vIncludes, StaticList& vCIncludes, StaticList& vCPPIncludes, StaticList& vASMIncludes, StaticList& vHIncludes);

    void outputTemplateTags(const string& sProjectName, string& sProjectTemplate, string& sFilterTemplate);

    void outputSourceFileType(StaticList& vFileList, const string& sType, const string& sFilterType, string & sProjectTemplate, string & sFilterTemplate, StaticList& vFoundObjects, set<string>& vFoundFilters, bool bCheckExisting, bool bStaticOnly = false, bool bSharedOnly = false);

    void outputSourceFiles(const string& sProjectName, string& sProjectTemplate, string& sFilterTemplate);

    bool outputProjectExports(const string& sProjectName, const StaticList& vIncludeDirs);

    /**
     * Executes a batch script to perform operations using a compiler based on current configuration.
     * @param          vIncludeDirs      The list of current directories to look for included files.
     * @param          sProjectName      Name of the current project.
     * @param [in,out] mDirectoryObjects A list of subdirectories with each one containing a vector of files contained
     *                                   within it.
     * @param          iRunType          The type of operation to run on input files (0=generate an sbr file, 1=pre-process
     *                                   to .i file).
     * @return True if it succeeds, false if it fails.
     */
    bool runCompiler(const vector<string> & vIncludeDirs, const string & sProjectName, map<string, vector<string>> &mDirectoryObjects, int iRunType);

    /**
     * Executes a batch script to perform operations using the msvc compiler.
     * @param          vIncludeDirs      The list of current directories to look for included files.
     * @param          sProjectName      Name of the current project.
     * @param [in,out] mDirectoryObjects A list of subdirectories with each one containing a vector of files contained
     *                                   within it.
     * @param          iRunType          The type of operation to run on input files (0=generate an sbr file, 1=pre-process
     *                                   to .i file).
     * @return True if it succeeds, false if it fails.
     */
    bool runMSVC(const vector<string> & vIncludeDirs, const string & sProjectName, map<string, vector<string>> &mDirectoryObjects, int iRunType);

    /**
     * Executes a bash script to perform operations using the gcc compiler.
     * @param          vIncludeDirs      The list of current directories to look for included files.
     * @param          sProjectName      Name of the current project.
     * @param [in,out] mDirectoryObjects A list of subdirectories with each one containing a vector of files contained
     *                                   within it.
     * @param          iRunType          The type of operation to run on input files (1=pre-process to .i file).
     * @return True if it succeeds, false if it fails.
     */
    bool runGCC(const vector<string> & vIncludeDirs, const string & sProjectName, map<string, vector<string>> &mDirectoryObjects, int iRunType);

    void outputBuildEvents(const string& sProjectName, string & sProjectTemplate);

    void outputIncludeDirs(const StaticList& vIncludeDirs, string & sProjectTemplate);

    void outputLibDirs(const StaticList& vLib32Dirs, const StaticList& vLib64Dirs, string & sProjectTemplate);

    /**
     * Output yasm tools to project template.
     * @param [in,out] sProjectTemplate The project template.
     */
    void outputYASMTools(string & sProjectTemplate);

    /**
     * Output nasm tools to project template.
     * @param [in,out] sProjectTemplate The project template.
     */
    void outputNASMTools(string & sProjectTemplate);

    /**
     * Output asm tools to project template.
     * @remark Either yasm or nasm tools will be used based on current configuration.
     * @param [in,out] sProjectTemplate The project template.
     */
    void outputASMTools(string & sProjectTemplate);

    bool outputDependencyLibs(const string& sProjectName, string & sProjectTemplate, bool bProgram = false);

    /**
     * Search through files in the current project and finds any undefined elements that are used in DCE blocks. A new
     * file os then created and added to the project that contains hull definitions for any missing functions.
     * @param sProjectName Name of the current project.
     * @param vIncludeDirs The list of current directories to look for included files.
     * @return True if it succeeds, false if it fails.
     */
    bool outputProjectDCE(string sProjectName, const StaticList& vIncludeDirs);

    /**
     * Passes an input file and looks for any function usage within a block of code elimated by DCE.
     * @param       sFile               The loaded file to search for DCE usage in.
     * @param       sProjectName        Name of the current project.
     * @param       sFileName           Filename of the file currently being searched.
     * @param [out] mFoundDCEFunctions  The return list of found DCE functions.
     * @param [out] bRequiresPreProcess The requires pre process.
     */
    void outputProjectDCEFindFunctions(const string & sFile, const string & sProjectName, const string & sFileName, map<string, DCEParams> & mFoundDCEFunctions, bool & bRequiresPreProcess);

    /**
     * Resolves a pre-processor define conditional string by replacing with current configuration settings.
     * @param [in,out] sDefine   The pre-processor define string.
     * @param          mReserved Pre-generated list of reserved configure values.
     */
    void outputProgramDCEsResolveDefine(string & sDefine, ConfigGenerator::DefaultValuesList mReserved);

    /**
     * Find any declaration of a specified function. Can also find a definition of the function if no declaration as
     * found first.
     * @param       sFile           The loaded file to search for function in.
     * @param       sFunction       The name of the function to search for.
     * @param       sFileName       Filename of the file being searched through.
     * @param [out] sRetDeclaration Returns the complete declaration for the found function.
     * @param [out] bIsFunction     Returns if the found declaration was actually for a function or an incorrectly
     *                              identified table/array declaration.
     * @return True if it succeeds finding the function, false if it fails.
     */
    bool outputProjectDCEsFindDeclarations(const string & sFile, const string & sFunction, const string & sFileName, string & sRetDeclaration, bool & bIsFunction);

    /**
     * Cleans a pre-processor define conditional string to remove any invalid values.
     * @param [in,out] sDefine The pre-processor define string to clean.
     */
    void outputProjectDCECleanDefine(string & sDefine);

    const string asDCETags[6] = {"ARCH_", "HAVE_", "CONFIG_", "EXTERNAL_", "INTERNAL_", "INLINE_"};
};

#endif
