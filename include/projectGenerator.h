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
    string          m_sProjectName;
    string          m_sProjectDir;

    map<string, StaticList> m_mProjectLibs;

public:

    ConfigGenerator m_ConfigHelper;

    /**
     * Checks all found Makefiles based on current configuration and generates project files and a solution files as
     * needed.
     * @return True if it succeeds, false if it fails.
     */
    bool passAllMake();

    /** Deletes any files that may have been created by previous runs. */
    void deleteCreatedFiles();

    /**
     * Error function to cleanly exit.
     * @param bCleanupFiles (Optional) True to delete created files.
     */
    void errorFunc(bool bCleanupFiles = true);

private:
    /**
     * Outputs a project file for the current project directory.
     * @return True if it succeeds, false if it fails.
     */
    bool outputProject();

    /**
     * Output a project file for a program (ffplay etc.).
     * @param sDestinationFile       Destination project file.
     * @param sDestinationFilterFile Destination project filter file.
     * @return True if it succeeds, false if it fails.
     */
    bool outputProgramProject(const string& sDestinationFile, const string& sDestinationFilterFile);

    /**
     * Outputs a solution file (Also calls outputProgramProject for any programs).
     * @return True if it succeeds, false if it fails.
     */
    bool outputSolution();

    bool passStaticIncludeObject(uint & uiStartPos, uint & uiEndPos, StaticList & vStaticIncludes);

    bool passStaticIncludeLine(uint uiStartPos, StaticList & vStaticIncludes);

    bool passStaticInclude(uint uiILength, StaticList & vStaticIncludes);

    bool passDynamicIncludeObject(uint & uiStartPos, uint & uiEndPos, string & sIdent, StaticList & vIncludes);

    bool passDynamicIncludeLine(uint uiStartPos, string & sIdent, StaticList & vIncludes);

    bool passDynamicInclude(uint uiILength, StaticList & vIncludes);

    /**
     * Pass a static source include line from current makefile.
     * @return True if it succeeds, false if it fails.
     */
    bool passCInclude();

    /**
     * Pass a dynamic source include line from current makefile.
     * @return True if it succeeds, false if it fails.
     */
    bool passDCInclude();

    /**
     * Pass a static asm include line from current makefile.
     * @param uiOffset The offset to start passing (used to separate old yasm and x86asm).
     * @return True if it succeeds, false if it fails.
     */
    bool passASMInclude(uint uiOffset);

    /**
     * Pass a dynamic asm include line from current makefile.
     * @param uiOffset The offset to start passing (used to separate old yasm and x86asm).
     * @return True if it succeeds, false if it fails.
     */
    bool passDASMInclude(uint uiOffset);

    /**
     * Pass a static mmx include line from current makefile.
     * @return True if it succeeds, false if it fails.
     */
    bool passMMXInclude();

    /**
     * Pass a dynamic mmx include line from current makefile.
     * @return True if it succeeds, false if it fails.
     */
    bool passDMMXInclude();

    /**
     * Pass a static header include line from current makefile.
     * @return True if it succeeds, false if it fails.
     */
    bool passHInclude(uint uiCutPos = 7);

    /**
     * Pass a dynamic header include line from current makefile.
     * @return True if it succeeds, false if it fails.
     */
    bool passDHInclude();

    /**
     * Pass a static lib include line from current makefile.
     * @return True if it succeeds, false if it fails.
     */
    bool passLibInclude();

    /**
     * Pass a dynamic lib include line from current makefile.
     * @return True if it succeeds, false if it fails.
     */
    bool passDLibInclude();

    /**
     * Pass a static unknown type include line from current makefile.
     * @return True if it succeeds, false if it fails.
     */
    bool passDUnknown();

    /**
     * Pass a dynamic unknown type include line from current makefile.
     * @return True if it succeeds, false if it fails.
     */
    bool passDLibUnknown();

    /**
     * Passes the makefile for the current project directory.
     * @return True if it succeeds, false if it fails.
     */
    bool passMake();

    /**
     * Pass the makefile for a specified program.
     * @return True if it succeeds, false if it fails.
     */
    bool passProgramMake();

    /**
     * Searches for the first source file.
     * @param       sFile        The file name.
     * @param       sExtension   The file extension.
     * @param [out] sRetFileName Filename of the found file.
     * @return True if it succeeds, false if it fails.
     */
    bool findSourceFile(const string & sFile, const string & sExtension, string & sRetFileName);

    /**
     * Searches for matching source files.
     * @param          sFile      The file name.
     * @param          sExtension The file extension.
     * @param [in,out] vRetFiles  The returned list of matching files.
     * @return True if it succeeds, false if it fails.
     */
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

    void buildInterDependencies(StaticList & vLibs);

    void buildDependencies(StaticList & vLibs, StaticList & vAddLibs);

    void buildDependencyDirs(StaticList & vIncludeDirs, StaticList & vLib32Dirs, StaticList & vLib64Dirs);

    void buildProjectDependencies(map<string, bool> & mProjectDeps);

    void buildProjectGUIDs(map<string, string> & mKeys);

    struct DCEParams
    {
        string sDefine;
        string sFile;

        bool operator==(const string & sCompare) { return (sFile.compare(sCompare) == 0); }
    };

    /**
     * Builds project specific DCE functions and variables that are not automatically detected.
     * @param [out] mDCEDefinitions The return list of built DCE functions.
     * @param [out] mDCEVariables   The return list of built DCE variables.
     */
    void buildProjectDCEs(map<string, DCEParams> & mDCEDefinitions, map<string, DCEParams> & mDCEVariables);

    bool checkProjectFiles();

    bool createReplaceFiles(const StaticList& vReplaceIncludes, StaticList& vExistingIncludes);

    bool findProjectFiles(const StaticList& vIncludes, StaticList& vCIncludes, StaticList& vCPPIncludes, StaticList& vASMIncludes, StaticList& vHIncludes);

    void outputTemplateTags(string& sProjectTemplate, string& sFilterTemplate);

    void outputSourceFileType(StaticList& vFileList, const string& sType, const string& sFilterType, string & sProjectTemplate, string & sFilterTemplate, StaticList& vFoundObjects, set<string>& vFoundFilters, bool bCheckExisting, bool bStaticOnly = false, bool bSharedOnly = false);

    void outputSourceFiles(string& sProjectTemplate, string& sFilterTemplate);

    bool outputProjectExports(const StaticList& vIncludeDirs);

    /**
     * Executes a batch script to perform operations using a compiler based on current configuration.
     * @param          vIncludeDirs      The list of current directories to look for included files.
     * @param [in,out] mDirectoryObjects A list of subdirectories with each one containing a vector of files contained
     *                                   within it.
     * @param          iRunType          The type of operation to run on input files (0=generate an sbr file, 1=pre-process
     *                                   to .i file).
     * @return True if it succeeds, false if it fails.
     */
    bool runCompiler(const vector<string> & vIncludeDirs, map<string, vector<string>> &mDirectoryObjects, int iRunType);

    /**
     * Executes a batch script to perform operations using the msvc compiler.
     * @param          vIncludeDirs      The list of current directories to look for included files.
     * @param [in,out] mDirectoryObjects A list of subdirectories with each one containing a vector of files contained
     *                                   within it.
     * @param          iRunType          The type of operation to run on input files (0=generate an sbr file, 1=pre-process
     *                                   to .i file).
     * @return True if it succeeds, false if it fails.
     */
    bool runMSVC(const vector<string> & vIncludeDirs, map<string, vector<string>> &mDirectoryObjects, int iRunType);

    /**
     * Executes a bash script to perform operations using the gcc compiler.
     * @param          vIncludeDirs      The list of current directories to look for included files.
     * @param [in,out] mDirectoryObjects A list of subdirectories with each one containing a vector of files contained
     *                                   within it.
     * @param          iRunType          The type of operation to run on input files (1=pre-process to .i file).
     * @return True if it succeeds, false if it fails.
     */
    bool runGCC(const vector<string> & vIncludeDirs, map<string, vector<string>> &mDirectoryObjects, int iRunType);

    void outputBuildEvents(string & sProjectTemplate);

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

    bool outputDependencyLibs(string & sProjectTemplate, bool bProgram = false);

    /**
     * Search through files in the current project and finds any undefined elements that are used in DCE blocks. A new
     * file is then created and added to the project that contains hull definitions for any missing functions.
     * @param vIncludeDirs The list of current directories to look for included files.
     * @return True if it succeeds, false if it fails.
     */
    bool outputProjectDCE(const StaticList& vIncludeDirs);

    /**
     * Passes an input file and looks for any function usage within a block of code eliminated by DCE.
     * @param          sFile               The loaded file to search for DCE usage in.
     * @param          sFileName           Filename of the file currently being searched.
     * @param [in,out] mFoundDCEUsage      The return list of found DCE functions.
     * @param [out]    bRequiresPreProcess The file requires pre processing.
     * @param [in,out] vUsedFunctions      The return list of found functions not in DCE.
     */
    void outputProjectDCEFindFunctions(const string & sFile, const string & sFileName, map<string, DCEParams> & mFoundDCEUsage, bool & bRequiresPreProcess, set<string> & vNonDCEUsage);

    /**
     * Resolves a pre-processor define conditional string by replacing with current configuration settings.
     * @param [in,out] sDefine   The pre-processor define string.
     * @param          mReserved Pre-generated list of reserved configure values.
     */
    void outputProgramDCEsResolveDefine(string & sDefine);

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

    /**
     * Combines 2 pre-processor define conditional strings.
     * @param       sDefine    The first define.
     * @param       sDefine2   The second define.
     * @param [out] sRetDefine The returned combined define.
     */
    void outputProgramDCEsCombineDefine(const string & sDefine, const string & sDefine2, string & sRetDefine);

    const string sTempDirectory = "FFVSTemp/";
};

#endif
