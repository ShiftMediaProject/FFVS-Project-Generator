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
typedef unsigned int uint;
#endif

namespace project_generate {
/**
 * Loads from a file.
 * @param       sFileName  Filename of the file.
 * @param [out] sRetString The returned string containing file contents.
 * @param       bBinary    (Optional) True to read in binary mode.
 * @param       bOutError  (Optional) True to output any detected errors.
 * @return True if it succeeds, false if it fails.
 */
bool loadFromFile(const string& sFileName, string& sRetString, bool bBinary = false, bool bOutError = true);

/**
 * Loads from an internal embedded resource.
 * @param       iResourceID Identifier for the resource.
 * @param [out] sRetString  The returned string containg the loaded resource.
 * @return True if it succeeds, false if it fails.
 */
bool loadFromResourceFile(int iResourceID, string& sRetString);

/**
 * Writes to file.
 * @param sFileName Filename of the file.
 * @param sString   The string to write.
 * @param bBinary   (Optional) True to write in binary mode (in normal text mode line ending are converted to OS
 *                  specific).
 * @return True if it succeeds, false if it fails.
 */
bool writeToFile(const string& sFileName, const string& sString, bool bBinary = false);

/**
 * Copies an internal embedded resource to a file.
 * @param iResourceID      Identifier for the resource.
 * @param sDestinationFile Destination file.
 * @return True if it succeeds, false if it fails.
 */
bool copyResourceFile(int iResourceID, const string & sDestinationFile);

/**
 * Deletes s file.
 * @param sDestinationFile Pathname of the file to delete.
 */
void deleteFile(const string & sDestinationFile);

/**
 * Deletes a folder.
 * @param sDestinationFolder Pathname of the folder to delete.
 */
void deleteFolder(const string & sDestinationFolder);

/**
 * Copies a file.
 * @param sSourceFolder      Pathname of the source folder.
 * @param sDestinationFolder Pathname of the destination folder.
 * @return True if it succeeds, false if it fails.
 */
bool copyFile(const string & sSourceFolder, const string & sDestinationFolder);

/**
 * Gets default copywrite header.
 * @param sDecription The decription of the current file to add to the header.
 * @return The copywrite header.
 */
string getCopywriteHeader(const string& sDecription);

/**
 * Makes a directory.
 * @param sDirectory Pathname of the directory.
 * @return True if it succeeds, false if it fails.
 */
bool makeDirectory(const string& sDirectory);

/**
 * Searches for the first file by name (supports wildcards).
 * @param       sFileName    The file name to search for.
 * @param [out] sRetFileName Filename of the found file.
 * @return True if it succeeds, false if it fails.
 */
bool findFile(const string & sFileName, string & sRetFileName);

/**
 * Searches for files by name (supports wildcards).
 * @param          sFileSearch The file name to search for.
 * @param [in,out] vRetFiles   The returned list of found files.
 * @param          bRecursive    (Optional) True to process any sub-directories.
 * @return True if it succeeds, false if it fails.
 */
bool findFiles(const string & sFileSearch, vector<string> & vRetFiles, bool bRecursive = true);

/**
 * Searches for folders by name (supports wildcards).
 * @param          sFolderSearch The folder name to search for.
 * @param [in,out] vRetFolders   The returned list of found folders.
 * @param          bRecursive    (Optional) True to process any sub-directories.
 * @return True if it succeeds, false if it fails.
 */
bool findFolders(const string & sFolderSearch, vector<string>& vRetFolders, bool bRecursive = true);

/**
 * Makes a file path relative to another.
 * @param       sPath           Input path.
 * @param       sMakeRelativeTo The path to make relative to.
 * @param [out] sRetPath        The returned path.
 */
void makePathsRelative(const string& sPath, const string& sMakeRelativeTo, string& sRetPath);

/**
 * Removes all whitespace from a string in place.
 * @param [in,out] sString The string to operate on.
 */
void removeWhiteSpace(string & sString);

/**
 * Searches through a string and replaces all occurances of the search tag in place.
 * @param [in,out] sString  The string to perform find and replace operation on.
 * @param          sSearch  The search string.
 * @param          sReplace The string used for replacement.
 */
void findAndReplace(string & sString, const string & sSearch, const string & sReplace);

const string sEndLine = "\n\r\f\v";
const string sWhiteSpace = " \t" + sEndLine;
const string sOperators = "+-*/=<>;()[]{}!^%|&~\'\"#";
const string sNonName = sOperators + sWhiteSpace;
const string sPreProcessor = "&|()!";
};

using namespace project_generate;

#endif
