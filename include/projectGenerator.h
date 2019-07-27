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
     * @param cleanupFiles (Optional) True to delete created files.
     */
    void errorFunc(bool cleanupFiles = true);

private:
    /**
     * Outputs a project file for the current project directory.
     * @return True if it succeeds, false if it fails.
     */
    bool outputProject();

    /**
     * Output a project file for a program (ffplay etc.).
     * @param destinationFile       Destination project file.
     * @param destinationFilterFile Destination project filter file.
     * @return True if it succeeds, false if it fails.
     */
    bool outputProgramProject(const string& destinationFile, const string& destinationFilterFile);

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

    void buildInterDependenciesHelper(
        const StaticList& configOptions, const StaticList& addDeps, StaticList& libs) const;

    void buildInterDependencies(StaticList& libs);

    void buildDependencies(StaticList& libs, StaticList& addLibs);

    /**
     * Updates existing library dependency lists by adding/removing those required/unavailable by WinRT.
     * @param [in,out] libs    The project dependency libs.
     * @param [in,out] addLibs The windows dependency libs.
     */
    void buildDependenciesWinRT(StaticList& libs, StaticList& addLibs);

    void buildDependencyValues(
        StaticList& includeDirs, StaticList& lib32Dirs, StaticList& lib64Dirs, StaticList& defines) const;

    void buildProjectDependencies(map<string, bool>& projectDeps) const;

    void buildProjectGUIDs(map<string, string>& keys) const;

    struct DCEParams
    {
        string define;
        string file;

        bool operator==(const string& compare) const
        {
            return (file == compare);
        }
    };

    /**
     * Builds project specific DCE functions and variables that are not automatically detected.
     * @param [out] definitionsDCE The return list of built DCE functions.
     * @param [out] variablesDCE   The return list of built DCE variables.
     */
    void buildProjectDCEs(map<string, DCEParams>& definitionsDCE, map<string, DCEParams>& variablesDCE) const;

    bool checkProjectFiles();

    bool createReplaceFiles(const StaticList& replaceIncludes, StaticList& existingIncludes);

    bool findProjectFiles(const StaticList& includes, StaticList& includesC, StaticList& includesCPP,
        StaticList& includesASM, StaticList& includesH) const;

    void outputTemplateTags(string& projectTemplate, string& filtersTemplate) const;

    void outputSourceFileType(StaticList& fileList, const string& type, const string& filterType,
        string& projectTemplate, string& filterTemplate, StaticList& foundObjects, set<string>& foundFilters,
        bool checkExisting, bool staticOnly = false, bool sharedOnly = false) const;

    void outputSourceFiles(string& projectTemplate, string& filterTemplate);

    bool outputProjectExports(const StaticList& includeDirs);

    /**
     * Executes a batch script to perform operations using a compiler based on current configuration.
     * @param          includeDirs      The list of current directories to look for included files.
     * @param [in,out] directoryObjects A list of subdirectories with each one containing a vector of files contained
     *   within it.
     * @param          runType          The type of operation to run on input files (0=generate an sbr file, 1=pre-
     *  process to .i file).
     * @returns True if it succeeds, false if it fails.
     */
    bool runCompiler(
        const vector<string>& includeDirs, map<string, vector<string>>& directoryObjects, int runType) const;

    /**
     * Executes a batch script to perform operations using the msvc compiler.
     * @param          includeDirs      The list of current directories to look for included files.
     * @param [in,out] directoryObjects A list of subdirectories with each one containing a vector of files contained
     *   within it.
     * @param          runType          The type of operation to run on input files (0=generate an sbr file, 1=pre-
     *  process to .i file).
     * @returns True if it succeeds, false if it fails.
     */
    bool runMSVC(const vector<string>& includeDirs, map<string, vector<string>>& directoryObjects, int runType) const;

    /**
     * Executes a bash script to perform operations using the gcc compiler.
     * @param          includeDirs      The list of current directories to look for included files.
     * @param [in,out] directoryObjects A list of subdirectories with each one containing a vector of files contained
     *   within it.
     * @param          runType          The type of operation to run on input files (1=pre-process to .i file).
     * @returns True if it succeeds, false if it fails.
     */
    bool runGCC(const vector<string>& includeDirs, map<string, vector<string>>& directoryObjects, int runType) const;

    /**
     * Output additional build events to the project.
     * @param [in,out] projectTemplate The project template.
     */
    void outputBuildEvents(string& projectTemplate);

    /**
     * Output additional include search directories to project.
     * @param          includeDirs     The include dirs.
     * @param [in,out] projectTemplate The project template.
     */
    static void outputIncludeDirs(const StaticList& includeDirs, string& projectTemplate);

    /**
     * Output additional library search directories to project.
     * @param          lib32Dirs       The library 32b dirs.
     * @param          lib64Dirs       The library 64b dirs.
     * @param [in,out] projectTemplate The project template.
     */
    static void outputLibDirs(const StaticList& lib32Dirs, const StaticList& lib64Dirs, string& projectTemplate);

    /**
     * Output additional defines to the project.
     * @param          defines         The defines.
     * @param [in,out] projectTemplate The project template.
     */
    static void outputDefines(const StaticList& defines, string& projectTemplate);

    /**
     * Output asm tools to project template.
     * @remark Either yasm or nasm tools will be used based on current configuration.
     * @param [in,out] projectTemplate The project template.
     */
    void outputASMTools(string& projectTemplate) const;

    bool outputDependencyLibs(string& projectTemplate, bool program = false);

    /**
     * Removes any WinRT/UWP configurations from the output project template.
     * @param [in,out] projectTemplate The project template.
     */
    static void outputStripWinRT(string& projectTemplate);

    /**
     * Removes any WinRT/UWP configurations from the output solution template.
     * @param [in,out] solutionFile The solution template.
     */
    static void outputStripWinRTSolution(string& solutionFile);

    /**
     * Search through files in the current project and finds any undefined elements that are used in DCE blocks. A new
     * file is then created and added to the project that contains hull definitions for any missing functions.
     * @param includeDirs The list of current directories to look for included files.
     * @return True if it succeeds, false if it fails.
     */
    bool outputProjectDCE(const StaticList& includeDirs);

    /**
     * Passes an input file and looks for any function usage within a block of code eliminated by DCE.
     * @param          file               The loaded file to search for DCE usage in.
     * @param          fileName           Filename of the file currently being searched.
     * @param [in,out] foundDCEUsage      The return list of found DCE functions.
     * @param [out]    requiresPreProcess The file requires pre processing.
     * @param [in,out] nonDCEUsage        The return list of found functions not in DCE.
     */
    void outputProjectDCEFindFunctions(const string& file, const string& fileName,
        map<string, DCEParams>& foundDCEUsage, bool& requiresPreProcess, set<string>& nonDCEUsage) const;

    /**
     * Resolves a pre-processor define conditional string by replacing with current configuration settings.
     * @param [in,out] define The pre-processor define string.
     */
    void outputProgramDCEsResolveDefine(string& define);

    /**
     * Find any declaration of a specified function. Can also find a definition of the function if no declaration as
     * found first.
     * @param       file           The loaded file to search for function in.
     * @param       function       The name of the function to search for.
     * @param       fileName       Filename of the file being searched through.
     * @param [out] retDeclaration Returns the complete declaration for the found function.
     * @param [out] isFunction     Returns if the found declaration was actually for a function or an incorrectly
     *                              identified table/array declaration.
     * @return True if it succeeds finding the function, false if it fails.
     */
    static bool outputProjectDCEsFindDeclarations(
        const string& file, const string& function, const string& fileName, string& retDeclaration, bool& isFunction);

    /**
     * Cleans a pre-processor define conditional string to remove any invalid values.
     * @param [in,out] define The pre-processor define string to clean.
     */
    static void outputProjectDCECleanDefine(string& define);

    /**
     * Combines 2 pre-processor define conditional strings.
     * @param       define    The first define.
     * @param       define2   The second define.
     * @param [out] retDefine The returned combined define.
     */
    static void outputProgramDCEsCombineDefine(const string& define, const string& define2, string& retDefine);
};

#endif
