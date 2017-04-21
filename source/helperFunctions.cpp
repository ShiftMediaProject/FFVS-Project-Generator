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

#include "helperFunctions.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <direct.h>

#ifdef _WIN32
#include <Windows.h>
#include "Shlwapi.h"
#else
#include <dirent.h>
extern char _binary_compat_h_start[];
extern char _binary_compat_h_end[];
extern char _binary_math_h_start[];
extern char _binary_math_h_end[];
extern char _binary_unistd_h_start[];
extern char _binary_unistd_h_end[];
extern char _binary_template_in_sln_start[];
extern char _binary_template_in_sln_end[];
extern char _binary_template_in_vcxproj_start[];
extern char _binary_template_in_vcxproj_end[];
extern char _binary_template_in_vcxproj_filters_start[];
extern char _binary_template_in_vcxproj_filters_end[];
extern char _binary_templateprogram_in_vcxproj_start[];
extern char _binary_templateprogram_in_vcxproj_end[];
extern char _binary_templateprogram_in_vcxproj_filters_start[];
extern char _binary_templateprogram_in_vcxproj_filters_end[];
extern char _binary_stdatomic_h_start[];
extern char _binary_stdatomic_h_end[];
const char *pp_cStartArray[] = {_binary_compat_h_start, _binary_math_h_start, _binary_unistd_h_start, _binary_template_in_sln_start,
_binary_template_in_vcxproj_start, _binary_template_in_vcxproj_filters_start, _binary_templateprogram_in_vcxproj_start,
_binary_templateprogram_in_vcxproj_filters_start, _binary_stdatomic_h_start};
const char *pp_cEndArray[] = {_binary_compat_h_end, _binary_math_h_end, _binary_unistd_h_end, _binary_template_in_sln_end,
_binary_template_in_vcxproj_end, _binary_template_in_vcxproj_filters_end, _binary_templateprogram_in_vcxproj_end,
_binary_templateprogram_in_vcxproj_filters_end, _binary_stdatomic_h_end};
#endif

namespace project_generate {
bool loadFromFile(const string& sFileName, string & sRetString, bool bBinary, bool bOutError)
{
    //Open the input file
    ifstream ifInputFile(sFileName, (bBinary) ? ios_base::in | ios_base::binary : ios_base::in);
    if (!ifInputFile.is_open()) {
        if (bOutError) {
            cout << "  Error: Failed opening file (" << sFileName << ")" << endl;
        }
        return false;
    }

    //Load whole file into internal string
    ifInputFile.seekg(0, ifInputFile.end);
    uint uiBufferSize = (uint)ifInputFile.tellg();
    ifInputFile.seekg(0, ifInputFile.beg);
    sRetString.resize(uiBufferSize);
    ifInputFile.read(&sRetString[0], uiBufferSize);
    if (uiBufferSize != (uint)ifInputFile.gcount()) {
        sRetString.resize((uint)ifInputFile.gcount());
    }
    ifInputFile.close();
    return true;
}

bool loadFromResourceFile(int iResourceID, string& sRetString)
{
#ifdef _WIN32
    // Can load directly from windows resource file
    HINSTANCE hInst = GetModuleHandle(NULL);
    HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(iResourceID), RT_RCDATA);
    HGLOBAL hMem = LoadResource(hInst, hRes);
    DWORD size = SizeofResource(hInst, hRes);
    char* resText = (char*)LockResource(hMem);

    //Copy across the file
    sRetString.reserve(size);
    sRetString.resize(size);
    memcpy(const_cast<char*>(sRetString.data()), resText, size);

    //Close Resource
    FreeResource(hMem);
    return true;
#else
    //Requires text file to be converted into a binary using either:
    // ld -r -b binary -o resource.file.o resource.file (creates _binary_resource_file_start)
    // objcopy -B i386 -I binary -O elf32-i386 resource.file resource.file.o
    uint iSize = (uint)(&pp_cEndArray[iResourceID - 100] - &pp_cStartArray[iResourceID - 100]);
    sRetString.reserve(iSize);
    sRetString.resize(iSize);
    memcpy(const_cast<char*>(sRetString.data()), pp_cStartArray[iResourceID - 100], iSize);
    return false;
#endif
}

bool writeToFile(const string & sFileName, const string & sString, bool bBinary)
{
    //Check for subdirectories
    uint uiDirPos = sFileName.rfind('/');
    if (uiDirPos != string::npos) {
        string sDir = sFileName.substr(0, uiDirPos);
        if (!makeDirectory(sDir)) {
            cout << "  Error: Failed creating local " << sDir << " directory" << endl;
            return false;
        }
    }
    //Open output file
    ofstream ofOutputFile(sFileName, (bBinary) ? ios_base::out | ios_base::binary : ios_base::out);
    if (!ofOutputFile.is_open()) {
        cout << "  Error: failed opening output file (" << sFileName << ")" << endl;
        return false;
    }

    //Output string to file and close
    ofOutputFile << sString;
    ofOutputFile.close();
    return true;
}

bool copyResourceFile(int iResourceID, const string & sDestinationFile)
{
#ifdef _WIN32
    // Can load directly from windows resource file
    HINSTANCE hInst = GetModuleHandle(NULL);
    HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(iResourceID), RT_RCDATA);
    HGLOBAL hMem = LoadResource(hInst, hRes);
    DWORD size = SizeofResource(hInst, hRes);
    char* resText = (char*)LockResource(hMem);

    //Copy across the file
    ofstream ofDest(sDestinationFile, ios::binary);
    if (!ofDest.is_open()) {
        FreeResource(hMem);
        return false;
    }
    if (!ofDest.write(resText, size)) {
        ofDest.close();
        FreeResource(hMem);
        return false;
    }
    ofDest.close();

    //Close Resource
    FreeResource(hMem);
    return true;
#else
    uint iSize = (uint)(&pp_cEndArray[iResourceID - 100] - &pp_cStartArray[iResourceID - 100]);
    //Copy across the file
    ofstream ofDest(sDestinationFile, ios::binary);
    if (!ofDest.is_open()) {
        return false;
    }
    if (!ofDest.write(pp_cStartArray[iResourceID - 100], iSize)) {
        ofDest.close();
        return false;
    }
    ofDest.close();
    return false;
#endif
}

void deleteFile(const string & sDestinationFile)
{
#ifdef _WIN32
    DeleteFile(sDestinationFile.c_str());
#else
    remove(sDestinationFile.c_str());
#endif
}

void deleteFolder(const string & sDestinationFolder)
{
#ifdef _WIN32
    string delFolder = sDestinationFolder + '\0';
    SHFILEOPSTRUCT file_op = {NULL, FO_DELETE, delFolder.c_str(), "", FOF_NO_UI, false, 0, ""};
    SHFileOperation(&file_op);
#else
    DIR* p_Dir = opendir(sDestinationFolder.c_str());
    size_t path_len = strlen(sDestinationFolder.c_str());

    if (p_Dir) {
        struct dirent* p_Dirent;
        int iRet = 0;
        while (!iRet && (p_Dirent = readdir(p_Dir))) {
            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p_Dirent->d_name, ".") || !strcmp(p_Dirent->d_name, "..")) {
                continue;
            }
            size_t diPathLength = path_len + strlen(p_Dirent->d_name) + 2;
            char* p_cBuffer = malloc(diPathLength);

            int iRet2 = -1;
            if (p_cBuffer) {
                struct stat statbuf;
                snprintf(p_cBuffer, diPathLength, "%s/%s", sDestinationFolder.c_str(), p_Dirent->d_name);
                if (!stat(p_cBuffer, &statbuf)) {
                    if (S_ISDIR(statbuf.st_mode)) {
                        iRet2 = remove_directory(p_cBuffer);
                    } else {
                        iRet2 = unlink(p_cBuffer);
                    }
                }
                free(p_cBuffer);
            }
            iRet = iRet2;
        }
        closedir(p_Dir);
    }
    rmdir(sDestinationFolder.c_str());
#endif
}

bool copyFile(const string & sSourceFolder, const string & sDestinationFolder)
{
#ifdef _WIN32
    return (CopyFile(sSourceFolder.c_str(), sDestinationFolder.c_str(), false) != 0);
#else
    FILE* p_Source = fopen(sSourceFolder.c_str(), "rb");
    if (p_Source == NULL) return false;
    FILE* p_Dest = fopen(sDestinationFolder.c_str(), "wb");
    if (p_Dest == NULL) {
        fclose(p_Source);
        return false;
    }
    size_t size;
    char p_cBuffer[BUFSIZ];
    while (size = fread(p_cBuffer, sizeof(char), sizeof(p_cBuffer), p_Source)) {
        if (fwrite(p_cBuffer, sizeof(char), size, p_Dest) != size) {
            fclose(p_Source);
            fclose(p_Dest);
            return false;
        }
    }
    fclose(p_Source);
    fclose(p_Dest);
#endif
}

string getCopywriteHeader(const string& sDecription)
{
    return "/** " + sDecription + "\n\
 *\n\
 * Permission is hereby granted, free of charge, to any person obtaining a copy\n\
 * of this software and associated documentation files (the \"Software\"), to deal\n\
 * in the Software without restriction, including without limitation the rights\n\
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n\
 * copies of the Software, and to permit persons to whom the Software is\n\
 * furnished to do so, subject to the following conditions:\n\
 *\n\
 * The above copyright notice and this permission notice shall be included in\n\
 * all copies or substantial portions of the Software.\n\
 *\n\
 * THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL\n\
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n\
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n\
 * THE SOFTWARE.\n\
 */";
}

bool makeDirectory(const string & sDirectory)
{
#ifdef _WIN32
    int iRet = _mkdir(sDirectory.c_str());
#else
    int iRet = mkdir(sDirectory.c_str());
#endif
    return ((iRet == 0) || (errno == EEXIST));
}

bool findFile(const string & sFileName, string & sRetFileName)
{
#ifdef _WIN32
    WIN32_FIND_DATA SearchFile;
    HANDLE SearchHandle = FindFirstFile(sFileName.c_str(), &SearchFile);
    if (SearchHandle != INVALID_HANDLE_VALUE) {
        //Update the return filename
        sRetFileName = SearchFile.cFileName;
        FindClose(SearchHandle);
        replace(sRetFileName.begin(), sRetFileName.end(), '\\', '/');
        return true;
    }
    return false;
#else
    glob_t glob_result;
    if (glob(sFileName.c_str(), GLOB_TILDEE | GLOB_NOSORT | GLOB_NOESCAPE, NULL, &glob_result) == 0) {
        sRetFileName = string(glob_result.gl_pathv[0]);
        globfree(&glob_result);
        return true;
    }
    return false;
#endif
}

bool findFiles(const string & sFileSearch, vector<string> & vRetFiles, bool bRecursive)
{
#ifdef _WIN32
    WIN32_FIND_DATA SearchFile;
    uint uiStartSize = vRetFiles.size();
    string sPath;
    string sSearchTerm = sFileSearch;
    uint uiPos = sSearchTerm.rfind('/');
    if (uiPos != string::npos) {
        ++uiPos;
        sPath = sFileSearch.substr(0, uiPos);
        sSearchTerm = sFileSearch.substr(uiPos);
    }
    HANDLE SearchHandle = FindFirstFile(sFileSearch.c_str(), &SearchFile);
    if (SearchHandle != INVALID_HANDLE_VALUE) {
        //Update the return filename list
        vRetFiles.push_back(sPath + SearchFile.cFileName);
        while (FindNextFile(SearchHandle, &SearchFile) != 0) {
            vRetFiles.push_back(sPath + SearchFile.cFileName);
            replace(vRetFiles.back().begin(), vRetFiles.back().end(), '\\', '/');
        }
        FindClose(SearchHandle);
    }
    //Search all sub directories as well
    if (bRecursive) {
        string sSearch = sPath + "*";
        SearchHandle = FindFirstFile(sSearch.c_str(), &SearchFile);
        if (SearchHandle != INVALID_HANDLE_VALUE) {
            BOOL bCont = TRUE;
            while (bCont == TRUE) {
                if (SearchFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // this is a directory
                    if (strcmp(SearchFile.cFileName, ".") != 0 && strcmp(SearchFile.cFileName, "..") != 0) {
                        string sNewPath = sPath + SearchFile.cFileName + '/' + sSearchTerm;
                        findFiles(sNewPath, vRetFiles);
                    }
                }
                bCont = FindNextFile(SearchHandle, &SearchFile);
            }
            FindClose(SearchHandle);
        }
    }
    return (vRetFiles.size() - uiStartSize) > 0;
#else
    glob_t glob_result;
    if (glob(sFileName.c_str(), GLOB_TILDEE | GLOB_NOSORT | GLOB_NOESCAPE, NULL, &glob_result) == 0) {
        for (unsigned int i = 0; i < glob_result.gl_pathc; ++i) {
            vRetFiles.push_back(string(glob_result.gl_pathv[i]));
        }
        globfree(&glob_result);
        return (vRetFiles.size() - uiStartSize) > 0;
    }
    return false;
#endif
}

bool findFolders(const string & sFolderSearch, vector<string>& vRetFolders, bool bRecursive)
{
#ifdef _WIN32
    WIN32_FIND_DATA SearchFile;
    uint uiStartSize = vRetFolders.size();
    string sPath;
    string sSearchTerm = sFolderSearch;
    uint uiPos = sSearchTerm.rfind('/');
    if (uiPos == (sSearchTerm.length() - 1)) {
        uiPos = sSearchTerm.find_last_not_of('/');
        uiPos = sSearchTerm.rfind('/', uiPos - 1);
    }
    if (uiPos != string::npos) {
        ++uiPos;
        sPath = sFolderSearch.substr(0, uiPos);
        sSearchTerm = sFolderSearch.substr(uiPos);
    }
    HANDLE SearchHandle = FindFirstFile(sFolderSearch.c_str(), &SearchFile);
    if (SearchHandle != INVALID_HANDLE_VALUE) {
        //Update the return filename list
        vRetFolders.push_back(sPath + SearchFile.cFileName);
        while (FindNextFile(SearchHandle, &SearchFile) != 0) {
            if (SearchFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                vRetFolders.push_back(sPath + SearchFile.cFileName);
                replace(vRetFolders.back().begin(), vRetFolders.back().end(), '\\', '/');
            }
        }
        FindClose(SearchHandle);
    }
    //Search all sub directories as well
    if (bRecursive) {
        string sSearch = sPath + "*";
        SearchHandle = FindFirstFile(sSearch.c_str(), &SearchFile);
        if (SearchHandle != INVALID_HANDLE_VALUE) {
            BOOL bCont = TRUE;
            while (bCont == TRUE) {
                if (SearchFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // this is a directory
                    if (strcmp(SearchFile.cFileName, ".") != 0 && strcmp(SearchFile.cFileName, "..") != 0) {
                        string sNewPath = sPath + SearchFile.cFileName + '/' + sSearchTerm;
                        findFolders(sNewPath, vRetFolders);
                    }
                }
                bCont = FindNextFile(SearchHandle, &SearchFile);
            }
            FindClose(SearchHandle);
        }
    }
    return (vRetFolders.size() - uiStartSize) > 0;
#else
    glob_t glob_result;
    if (glob(sFileName.c_str(), GLOB_TILDE | GLOB_NOSORT | GLOB_NOESCAPE | GLOB_ONLYDIR, NULL, &glob_result) == 0) {
        for (unsigned int i = 0; i < glob_result.gl_pathc; ++i) {
            vRetFiles.push_back(string(glob_result.gl_pathv[i]));
        }
        globfree(&glob_result);
        return (vRetFiles.size() - uiStartSize) > 0;
    }
    return false;
#endif
}

void makePathsRelative(const string& sPath, const string& sMakeRelativeTo, string& sRetPath)
{
#ifdef _WIN32
    sRetPath.reserve(MAX_PATH);
    sRetPath.resize(MAX_PATH);
    string sFromT, sToT;
    sFromT.reserve(MAX_PATH);
    sToT.reserve(MAX_PATH);
    GetFullPathName(sPath.c_str(), MAX_PATH, const_cast<char*>(sFromT.data()), NULL);
    GetFullPathName(sMakeRelativeTo.c_str(), MAX_PATH, const_cast<char*>(sToT.data()), NULL);
    PathRelativePathTo(const_cast<char*>(sRetPath.data()), sToT.c_str(), FILE_ATTRIBUTE_DIRECTORY, sFromT.c_str(), FILE_ATTRIBUTE_NORMAL);
    sRetPath.resize(strlen(sRetPath.c_str()));
    replace(sRetPath.begin(), sRetPath.end(), '\\', '/');
#else
    sRetPath.reserve(MAX_PATH);
    sRetPath.resize(MAX_PATH);
    string sFromT, sToT;
    sFromT.reserve(MAX_PATH);
    sToT.reserve(MAX_PATH);
    realpath(sPath.c_str(), const_cast<char*>(sFromT.data()));
    realpath(sMakeRelativeTo.c_str(), const_cast<char*>(sToT.data()));
    sRetPath.at(0) = '\0';
    int iPos;
    int iSizeA = strlen(sPath.c_str()) + 1;
    int iSizeR = strlen(sMakeRelativeTo.c_str()) + 1;
    for (iPos = 0; iPos < iSizeA && iPos < iSizeR; iPos += strlen(sPath.c_str() + iPos) + 1) {
        const char* p_PosA = strchr(sPath.c_str() + iPos, '/');
        const char* p_PosR = strchr(sMakeRelativeTo.c_str() + iPos, '/');
        if (p_PosA) const_cast<char*>(p_PosA)[0] = '\0';
        if (p_PosR) const_cast<char*>(p_PosR)[0] = '\0';
        if (strcmp(sPath.c_str() + iPos, sMakeRelativeTo.c_str() + iPos) != 0) break;
    }
    for (int iPos2 = iPos; iPos2 < iSizeR; iPos2 += strlen(sMakeRelativeTo.c_str() + iPos2) + 1) {
        strcat(const_cast<char*>(sRetPath.data()), "../");
        if (!strchr(sRetPath.c_str() + iPos2, '/')) break;
    }
    if (iPos < iSizeA) strcat(const_cast<char*>(sRetPath.data()), sPath.c_str() + iPos);
    sRetPath.resize(strlen(sRetPath.c_str()));
#endif
}

void removeWhiteSpace(string & sString)
{
    struct RemoveDelimiter
    {
        bool operator()(char c)
        {
            return (sWhiteSpace.find(c) != string::npos);
        }
    };

    sString.erase(remove_if(sString.begin(), sString.end(), RemoveDelimiter()), sString.end());
}

void findAndReplace(string & sString, const string & sSearch, const string & sReplace)
{
    uint uiPos = 0;
    while ((uiPos = sString.find(sSearch, uiPos)) != std::string::npos) {
        sString.replace(uiPos, sSearch.length(), sReplace);
        uiPos += sReplace.length();
    }
}

bool findEnvironmentVariable(const string & sEnvVar)
{
#ifdef _WIN32
    return GetEnvironmentVariable(sEnvVar.c_str(), NULL, 0);
#else
    return false;
#endif
}
};