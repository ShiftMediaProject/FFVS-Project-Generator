/*
 * copyright (c) 2015 Matthew Oliver
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

#ifndef _HELPERFUNCTIONS_H_
#define _HELPERFUNCTIONS_H_

#include <string>
#include <vector>

using namespace std;

#if defined(__x86_64) || defined(_M_X64)
typedef unsigned __int64 uint;
#else
using uint = unsigned int;
#endif

namespace project_generate {
/**
 * Loads from a file.
 * @param       fileName  Filename of the file.
 * @param [out] retString The returned string containing file contents.
 * @param       binary    (Optional) True to read in binary mode.
 * @param       outError  (Optional) True to output any detected errors.
 * @return True if it succeeds, false if it fails.
 */
bool loadFromFile(const string& fileName, string& retString, bool binary = false, bool outError = true);

/**
 * Loads from an internal embedded resource.
 * @param       resourceID Identifier for the resource.
 * @param [out] retString  The returned string containing the loaded resource.
 * @return True if it succeeds, false if it fails.
 */
bool loadFromResourceFile(int resourceID, string& retString);

/**
 * Writes to file.
 * @param fileName Filename of the file.
 * @param inString The inString to write.
 * @param binary   (Optional) True to write in binary mode (in normal text mode line ending are converted to OS
 *                  specific).
 * @return True if it succeeds, false if it fails.
 */
bool writeToFile(const string& fileName, const string& inString, bool binary = false);

/**
 * Copies an internal embedded resource to a file.
 * @param resourceID      Identifier for the resource.
 * @param destinationFile Destination file.
 * @param binary          (Optional) True to write in binary mode (in normal text mode line ending are converted to
 *                         OS specific).
 * @return True if it succeeds, false if it fails.
 */
bool copyResourceFile(int resourceID, const string& destinationFile, bool binary = false);

/**
 * Deletes s file.
 * @param destinationFile Pathname of the file to delete.
 */
void deleteFile(const string& destinationFile);

/**
 * Deletes a folder.
 * @param destinationFolder Pathname of the folder to delete.
 */
void deleteFolder(const string& destinationFolder);

/**
 * Queries if a folder is empty.
 * @param folder Pathname of the folder.
 * @return True if the folder is empty, false if not.
 */
bool isFolderEmpty(const string& folder);

/**
 * Copies a file.
 * @param sourceFolder      Pathname of the source folder.
 * @param destinationFolder Pathname of the destination folder.
 * @return True if it succeeds, false if it fails.
 */
bool copyFile(const string& sourceFolder, const string& destinationFolder);

/**
 * Gets default copywrite header.
 * @param decription The description of the current file to add to the header.
 * @return The copywrite header.
 */
string getCopywriteHeader(const string& decription);

/**
 * Makes a directory.
 * @param directory Pathname of the directory.
 * @return True if it succeeds, false if it fails.
 */
bool makeDirectory(const string& directory);

/**
 * Searches for the first file by name (supports wildcards).
 * @param       fileName    The file name to search for.
 * @param [out] retFileName Filename of the found file.
 * @return True if it succeeds, false if it fails.
 */
bool findFile(const string& fileName, string& retFileName);

/**
 * Searches for files by name (supports wildcards).
 * @param          fileSearch The file name to search for.
 * @param [in,out] retFiles   The returned list of found files.
 * @param          recursive  (Optional) True to process any sub-directories.
 * @return True if it succeeds, false if it fails.
 */
bool findFiles(const string& fileSearch, vector<string>& retFiles, bool recursive = true);

/**
 * Searches for folders by name (supports wildcards).
 * @param          folderSearch The folder name to search for.
 * @param [in,out] retFolders   The returned list of found folders.
 * @param          recursive    (Optional) True to process any sub-directories.
 * @return True if it succeeds, false if it fails.
 */
bool findFolders(const string& folderSearch, vector<string>& retFolders, bool recursive = true);

/**
 * Makes a file path relative to another.
 * @param       path           Input path.
 * @param       makeRelativeTo The path to make relative to.
 * @param [out] retPath        The returned path.
 */
void makePathsRelative(const string& path, const string& makeRelativeTo, string& retPath);

/**
 * Removes all whitespace from a string in place.
 * @param [in,out] inputString The string to operate on.
 */
void removeWhiteSpace(string& inputString);

/**
 * Searches through a string and replaces all occurrences of the search tag in place.
 * @param [in,out] inString The string to perform find and replace operation on.
 * @param          search   The search string.
 * @param          replace  The string used for replacement.
 */
void findAndReplace(string& inString, const string& search, const string& replace);

/**
 * Searches for the existence of an environment variable.
 * @param envVar The environment variable.
 * @return True if it succeeds, false if it fails.
 */
bool findEnvironmentVariable(const string& envVar);

/** Press key to continue terminal prompt. */
void pressKeyToContinue();

/**
 * Output a single line of text.
 * @param message The message.
 */
void outputLine(const string& message);

/**
 * Output information message.
 * @remark Whether this outputs depends on currently set verbosity.
 * @param message The message.
 * @param header  (Optional) True to add the info starting header.
 */
void outputInfo(const string& message, bool header = true);

/**
 * Output warning message.
 * @remark Whether this outputs depends on currently set verbosity.
 * @param message The message.
 * @param header  (Optional) True to add the warning starting header.
 */
void outputWarning(const string& message, bool header = true);

/**
 * Output error message.
 * @remark Whether this outputs depends on currently set verbosity.
 * @param message The message.
 * @param header  (Optional) True to add the error starting header.
 */
void outputError(const string& message, bool header = true);

enum Verbosity
{
    VERBOSITY_INFO,    // Info+Warning+Error
    VERBOSITY_WARNING, // Warning+Error
    VERBOSITY_ERROR,   // Error
};

/**
 * Sets output verbosity for output message functions.
 * @param verbose The verbosity to set.
 */
void setOutputVerbosity(Verbosity verbose);

const string g_endLine = "\n\r\f\v";
const string g_whiteSpace = " \t" + g_endLine;
const string g_operators = "+-*/=<>;()[]{}!^%|&~\'\"#?:";
const string g_nonName = g_operators + g_whiteSpace;
const string g_preProcessor = "&|()!=";
}; // namespace project_generate

using namespace project_generate;

#endif
