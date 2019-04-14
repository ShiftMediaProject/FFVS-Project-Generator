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
    using StaticList = vector<string>;
    using UnknownList = map<string, StaticList>;
    ifstream m_inputFile;
    string m_inLine;
    StaticList m_includes;
    StaticList m_includesCPP;
    StaticList m_includesC;
    StaticList m_includesASM;
    StaticList m_includesH;
    UnknownList m_replaceIncludes;
    StaticList m_libs;
    UnknownList m_unknowns;
    string m_projectName;
    string m_projectDir;

    map<string, StaticList> m_projectLibs;

    const string m_tempDirectory = "FFVSTemp/";

public:
    ConfigGenerator m_configHelper;

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

    /** Cleans up any used variables after a project file has been created. */
    void outputProjectCleanup();

    /**
     * Outputs a solution file (Also calls outputProgramProject for any programs).
     * @return True if it succeeds, false if it fails.
     */
    bool outputSolution();

    bool passStaticIncludeObject(uint& startPos, uint& endPos, StaticList& staticIncludes);

    bool passStaticIncludeLine(uint startPos, StaticList& staticIncludes);

    bool passStaticInclude(uint length, StaticList& staticIncludes);

    bool passDynamicIncludeObject(uint& startPos, uint& endPos, string& ident, StaticList& includes);

    bool passDynamicIncludeLine(uint startPos, string& ident, StaticList& includes);

    bool passDynamicInclude(uint length, StaticList& includes);

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
     * @param offset The offset to start passing (used to separate old yasm and x86asm).
     * @return True if it succeeds, false if it fails.
     */
    bool passASMInclude(uint offset);

    /**
     * Pass a dynamic asm include line from current makefile.
     * @param offset The offset to start passing (used to separate old yasm and x86asm).
     * @return True if it succeeds, false if it fails.
     */
    bool passDASMInclude(uint offset);

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
    bool passHInclude(uint cutPos = 7);

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
     * @param       file        The file name.
     * @param       extension   The file extension.
     * @param [out] retFileName Filename of the found file.
     * @return True if it succeeds, false if it fails.
     */
    bool findSourceFile(const string& file, const string& extension, string& retFileName) const;

    /**
     * Searches for matching source files.
     * @param          file      The file name.
     * @param          extension The file extension.
     * @param [in,out] retFiles  The returned list of matching files.
     * @return True if it succeeds, false if it fails.
     */
    bool findSourceFiles(const string& file, const string& extension, vector<string>& retFiles) const;

    void buildInterDependenciesHelper(const StaticList& vConfigOptions, const StaticList& vAddDeps, StaticList& vLibs);

    void buildInterDependencies(StaticList& vLibs);

    void buildDependencies(StaticList& vLibs, StaticList& vAddLibs);

    /**
     * Updates existing library dependency lists by adding/removing those required/unavailable by WinRT.
     * @param [in,out] vLibs    The project dependency libs.
     * @param [in,out] vAddLibs The windows dependency libs.
     */
    void buildDependenciesWinRT(StaticList& vLibs, StaticList& vAddLibs);

    void buildDependencyValues(
        StaticList& vIncludeDirs, StaticList& vLib32Dirs, StaticList& vLib64Dirs, StaticList& vDefines);

    void buildProjectDependencies(map<string, bool>& mProjectDeps);

    void buildProjectGUIDs(map<string, string>& mKeys) const;

    struct DCEParams
    {
        string sDefine;
        string sFile;

        bool operator==(const string& sCompare) const
        {
            return (sFile == sCompare);
        }
    };

    /**
     * Builds project specific DCE functions and variables that are not automatically detected.
     * @param [out] mDCEDefinitions The return list of built DCE functions.
     * @param [out] mDCEVariables   The return list of built DCE variables.
     */
    void buildProjectDCEs(map<string, DCEParams>& mDCEDefinitions, map<string, DCEParams>& mDCEVariables) const;

    bool checkProjectFiles();

    bool createReplaceFiles(const StaticList& replaceIncludes, StaticList& existingIncludes);

    bool findProjectFiles(const StaticList& includes, StaticList& includesC, StaticList& includesCPP,
        StaticList& includesASM, StaticList& includesH) const;

    void outputTemplateTags(string& sProjectTemplate, string& sFiltersTemplate) const;

    void outputSourceFileType(StaticList& vFileList, const string& sType, const string& sFilterType,
        string& sProjectTemplate, string& sFilterTemplate, StaticList& vFoundObjects, set<string>& vFoundFilters,
        bool bCheckExisting, bool bStaticOnly = false, bool bSharedOnly = false) const;

    void outputSourceFiles(string& sProjectTemplate, string& sFilterTemplate);

    bool outputProjectExports(const StaticList& vIncludeDirs);

    /**
     * Executes a batch script to perform operations using a compiler based on current configuration.
     * @param          vIncludeDirs      The list of current directories to look for included files.
     * @param [in,out] mDirectoryObjects A list of subdirectories with each one containing a vector of files contained
     *                                   within it.
     * @param          iRunType          The type of operation to run on input files (0=generate an sbr file,
     * 1=pre-process to .i file).
     * @return True if it succeeds, false if it fails.
     */
    bool runCompiler(
        const vector<string>& vIncludeDirs, map<string, vector<string>>& mDirectoryObjects, int iRunType) const;

    /**
     * Executes a batch script to perform operations using the msvc compiler.
     * @param          vIncludeDirs      The list of current directories to look for included files.
     * @param [in,out] mDirectoryObjects A list of subdirectories with each one containing a vector of files contained
     *                                   within it.
     * @param          iRunType          The type of operation to run on input files (0=generate an sbr file,
     * 1=pre-process to .i file).
     * @return True if it succeeds, false if it fails.
     */
    bool runMSVC(
        const vector<string>& vIncludeDirs, map<string, vector<string>>& mDirectoryObjects, int iRunType) const;

    /**
     * Executes a bash script to perform operations using the gcc compiler.
     * @param          vIncludeDirs      The list of current directories to look for included files.
     * @param [in,out] mDirectoryObjects A list of subdirectories with each one containing a vector of files contained
     *                                   within it.
     * @param          iRunType          The type of operation to run on input files (1=pre-process to .i file).
     * @return True if it succeeds, false if it fails.
     */
    bool runGCC(const vector<string>& vIncludeDirs, map<string, vector<string>>& mDirectoryObjects, int iRunType) const;

    /**
     * Output additional build events to the project.
     * @param [in,out] sProjectTemplate The project template.
     */
    void outputBuildEvents(string& sProjectTemplate);

    /**
     * Output additional include search directories to project.
     * @param          vIncludeDirs     The include dirs.
     * @param [in,out] sProjectTemplate The project template.
     */
    static void outputIncludeDirs(const StaticList& vIncludeDirs, string& sProjectTemplate);

    /**
     * Output additional library search directories to project.
     * @param          vLib32Dirs       The library 32b dirs.
     * @param          vLib64Dirs       The library 64b dirs.
     * @param [in,out] sProjectTemplate The project template.
     */
    static void outputLibDirs(const StaticList& vLib32Dirs, const StaticList& vLib64Dirs, string& sProjectTemplate);

    /**
     * Output additional defines to the project.
     * @param          defines         The defines.
     * @param [in,out] projectTemplate The project template.
     */
    void outputDefines(const StaticList& defines, string& projectTemplate);

    /**
     * Output asm tools to project template.
     * @remark Either yasm or nasm tools will be used based on current configuration.
     * @param [in,out] sProjectTemplate The project template.
     */
    void outputASMTools(string& sProjectTemplate);

    bool outputDependencyLibs(string& sProjectTemplate, bool bProgram = false);

    /**
     * Removes any WinRT/UWP configurations from the output project template.
     * @param [in,out] sProjectTemplate The project template.
     */
    static void outputStripWinRT(string& sProjectTemplate);

    /**
     * Removes any WinRT/UWP configurations from the output solution template.
     * @param [in,out] sSolutionFile The solution template.
     */
    static void outputStripWinRTSolution(string& sSolutionFile);

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
     * @param [in,out] vNonDCEUsage        The return list of found functions not in DCE.
     */
    void outputProjectDCEFindFunctions(const string& sFile, const string& sFileName,
        map<string, DCEParams>& mFoundDCEUsage, bool& bRequiresPreProcess, set<string>& vNonDCEUsage) const;

    /**
     * Resolves a pre-processor define conditional string by replacing with current configuration settings.
     * @param [in,out] sDefine The pre-processor define string.
     */
    void outputProgramDCEsResolveDefine(string& sDefine);

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
    static bool outputProjectDCEsFindDeclarations(const string& sFile, const string& sFunction, const string& sFileName,
        string& sRetDeclaration, bool& bIsFunction);

    /**
     * Cleans a pre-processor define conditional string to remove any invalid values.
     * @param [in,out] sDefine The pre-processor define string to clean.
     */
    static void outputProjectDCECleanDefine(string& sDefine);

    /**
     * Combines 2 pre-processor define conditional strings.
     * @param       sDefine    The first define.
     * @param       sDefine2   The second define.
     * @param [out] sRetDefine The returned combined define.
     */
    static void outputProgramDCEsCombineDefine(const string& sDefine, const string& sDefine2, string& sRetDefine);
};

#endif
