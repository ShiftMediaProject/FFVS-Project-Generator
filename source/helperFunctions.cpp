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

#include <algorithm>
#include <direct.h>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#    include "Shlwapi.h"

#    include <Windows.h>
#else
#    include <dirent.h>
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
const char* pp_cStartArray[] = {_binary_compat_h_start, _binary_math_h_start, _binary_unistd_h_start,
    _binary_template_in_sln_start, _binary_template_in_vcxproj_start, _binary_template_in_vcxproj_filters_start,
    _binary_templateprogram_in_vcxproj_start, _binary_templateprogram_in_vcxproj_filters_start,
    _binary_stdatomic_h_start};
const char* pp_cEndArray[] = {_binary_compat_h_end, _binary_math_h_end, _binary_unistd_h_end,
    _binary_template_in_sln_end, _binary_template_in_vcxproj_end, _binary_template_in_vcxproj_filters_end,
    _binary_templateprogram_in_vcxproj_end, _binary_templateprogram_in_vcxproj_filters_end, _binary_stdatomic_h_end};
#endif

#if _DEBUG
static Verbosity s_outputVerbosity = VERBOSITY_INFO;
#else
static Verbosity s_outputVerbosity = VERBOSITY_WARNING;
#endif

namespace project_generate {
bool loadFromFile(const string& fileName, string& retString, const bool binary, const bool outError)
{
    // Open the input file
    ifstream inputFile(fileName, (binary) ? ios_base::in | ios_base::binary : ios_base::in);
    if (!inputFile.is_open()) {
        if (outError) {
            outputError("Failed opening file (" + fileName + ")");
        }
        return false;
    }

    // Load whole file into internal string
    inputFile.seekg(0, std::ifstream::end);
    const uint bufferSize = static_cast<uint>(inputFile.tellg());
    inputFile.seekg(0, std::ifstream::beg);
    retString.resize(bufferSize);
    inputFile.read(&retString[0], bufferSize);
    if (bufferSize != static_cast<uint>(inputFile.gcount())) {
        retString.resize(static_cast<uint>(inputFile.gcount()));
    }
    inputFile.close();
    return true;
}

bool loadFromResourceFile(const int resourceID, string& retString)
{
#ifdef _WIN32
    // Can load directly from windows resource file
    HINSTANCE hInst = GetModuleHandle(nullptr);
    HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    HGLOBAL hMem = LoadResource(hInst, hRes);
    DWORD size = SizeofResource(hInst, hRes);
    char* resText = static_cast<char*>(LockResource(hMem));

    // Copy across the file
    retString.reserve(size);
    retString.resize(size);
    memcpy(const_cast<char*>(retString.data()), resText, size);

    // Close Resource
    FreeResource(hMem);
    return true;
#else
    // Requires text file to be converted into a binary using either:
    // ld -r -b binary -o resource.file.o resource.file (creates _binary_resource_file_start)
    // objcopy -B i386 -I binary -O elf32-i386 resource.file resource.file.o
    uint size = (uint)(&pp_cEndArray[resourceID - 100] - &pp_cStartArray[resourceID - 100]);
    retString.reserve(size);
    retString.resize(size);
    memcpy(const_cast<char*>(retString.data()), pp_cStartArray[resourceID - 100], size);
    return false;
#endif
}

bool writeToFile(const string& fileName, const string& inString, const bool binary)
{
    // Check for subdirectories
    const uint dirPos = fileName.rfind('/');
    if (dirPos != string::npos) {
        const string cs = fileName.substr(0, dirPos);
        if (!makeDirectory(cs)) {
            outputError("Failed creating local " + cs + " directory");
            return false;
        }
    }
    // Open output file
    ofstream ofOutputFile(fileName, (binary) ? ios_base::out | ios_base::binary : ios_base::out);
    if (!ofOutputFile.is_open()) {
        outputError("Failed opening output file (" + fileName + ")");
        return false;
    }

    // Output inString to file and close
    ofOutputFile << inString;
    ofOutputFile.close();
    return true;
}

bool copyResourceFile(const int resourceID, const string& destinationFile, const bool binary)
{
#ifdef _WIN32
    // Can load directly from windows resource file
    HINSTANCE hInst = GetModuleHandle(nullptr);
    HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    HGLOBAL hMem = LoadResource(hInst, hRes);
    DWORD size = SizeofResource(hInst, hRes);
    char* resText = static_cast<char*>(LockResource(hMem));

    // Copy across the file
    ofstream dest(destinationFile, (binary) ? ios_base::out | ios_base::binary : ios_base::out);
    if (!dest.is_open()) {
        FreeResource(hMem);
        return false;
    }
    if (!dest.write(resText, size)) {
        dest.close();
        FreeResource(hMem);
        return false;
    }
    dest.close();

    // Close Resource
    FreeResource(hMem);
    return true;
#else
    uint size = (uint)(&pp_cEndArray[resourceID - 100] - &pp_cStartArray[resourceID - 100]);
    // Copy across the file
    ofstream dest(destinationFile, (binary) ? ios_base::out | ios_base::binary : ios_base::out);
    if (!dest.is_open()) {
        return false;
    }
    if (!dest.write(pp_cStartArray[resourceID - 100], size)) {
        dest.close();
        return false;
    }
    dest.close();
    return false;
#endif
}

void deleteFile(const string& destinationFile)
{
#ifdef _WIN32
    DeleteFile(destinationFile.c_str());
#else
    remove(destinationFile.c_str());
#endif
}

void deleteFolder(const string& destinationFolder)
{
#ifdef _WIN32
    string delFolder = destinationFolder + '\0';
    SHFILEOPSTRUCT file_op = {NULL, FO_DELETE, delFolder.c_str(), "", FOF_NO_UI, false, 0, ""};
    SHFileOperation(&file_op);
#else
    DIR* p_Dir = opendir(destinationFolder.c_str());
    size_t path_len = strlen(destinationFolder.c_str());

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
                snprintf(p_cBuffer, diPathLength, "%s/%s", destinationFolder.c_str(), p_Dirent->d_name);
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
    rmdir(destinationFolder.c_str());
#endif
}

bool isFolderEmpty(const string& folder)
{
#ifdef _WIN32
    return PathIsDirectoryEmpty(folder.c_str());
#else
    DIR* dir = opendir(folder.c_str());
    if (dir == NULL) {
        // Not a directory or doesn't exist
        return false;
    }
    int n = 0;
    struct dirent* d;
    while ((d = readdir(dir)) != NULL) {
        if (++n > 2) {
            break;
        }
    }
    closedir(dir);
    return (n <= 2);
#endif
}

bool copyFile(const string& sourceFolder, const string& destinationFolder)
{
#ifdef _WIN32
    return (CopyFile(sourceFolder.c_str(), destinationFolder.c_str(), false) != 0);
#else
    FILE* p_Source = fopen(sourceFolder.c_str(), "rb");
    if (p_Source == NULL)
        return false;
    FILE* p_Dest = fopen(destinationFolder.c_str(), "wb");
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

string getCopywriteHeader(const string& decription)
{
    return "/** " + decription + "\n\
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

bool makeDirectory(const string& directory)
{
#ifdef _WIN32
    const int ret = _mkdir(directory.c_str());
#else
    const int ret = mkdir(directory.c_str());
#endif
    if ((ret == 0) || (errno == EEXIST)) {
        return true;
    }
    if (errno == ENOENT) {
        // The parent directory doesnt exist
        uint pos = directory.find_last_of('/');
#if defined(_WIN32)
        if (pos == string::npos) {
            pos = directory.find_last_of('\\');
        }
#endif
        if (pos == string::npos) {
            return false;
        }
        if (!makeDirectory(directory.substr(0, pos))) {
            return false;
        }
        // Try again
        return makeDirectory(directory);
    }
    return false;
}

bool findFile(const string& fileName, string& retFileName)
{
#ifdef _WIN32
    WIN32_FIND_DATA searchFile;
    HANDLE searchHandle = FindFirstFile(fileName.c_str(), &searchFile);
    if (searchHandle != INVALID_HANDLE_VALUE) {
        // Update the return filename
        retFileName = searchFile.cFileName;
        FindClose(searchHandle);
        replace(retFileName.begin(), retFileName.end(), '\\', '/');
        return true;
    }
    return false;
#else
    glob_t glob_result;
    if (glob(fileName.c_str(), GLOB_TILDEE | GLOB_NOSORT | GLOB_NOESCAPE, NULL, &glob_result) == 0) {
        retFileName = string(glob_result.gl_pathv[0]);
        globfree(&glob_result);
        return true;
    }
    return false;
#endif
}

bool findFiles(const string& fileSearch, vector<string>& retFiles, const bool recursive)
{
#ifdef _WIN32
    WIN32_FIND_DATA searchFile;
    const uint startSize = retFiles.size();
    string path;
    string searchTerm = fileSearch;
    uint pos = searchTerm.rfind('/');
    if (pos != string::npos) {
        ++pos;
        path = fileSearch.substr(0, pos);
        searchTerm = fileSearch.substr(pos);
    }
    HANDLE searchHandle = FindFirstFile(fileSearch.c_str(), &searchFile);
    if (searchHandle != INVALID_HANDLE_VALUE) {
        // Update the return filename list
        retFiles.push_back(path + searchFile.cFileName);
        while (FindNextFile(searchHandle, &searchFile) != 0) {
            retFiles.push_back(path + searchFile.cFileName);
            replace(retFiles.back().begin(), retFiles.back().end(), '\\', '/');
        }
        FindClose(searchHandle);
    }
    // Search all sub directories as well
    if (recursive) {
        string search = path + "*";
        searchHandle = FindFirstFile(search.c_str(), &searchFile);
        if (searchHandle != INVALID_HANDLE_VALUE) {
            BOOL cont = TRUE;
            while (cont == TRUE) {
                if (searchFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // this is a directory
                    if (strcmp(searchFile.cFileName, ".") != 0 && strcmp(searchFile.cFileName, "..") != 0) {
                        string newPath = path + searchFile.cFileName + '/' + searchTerm;
                        findFiles(newPath, retFiles);
                    }
                }
                cont = FindNextFile(searchHandle, &searchFile);
            }
            FindClose(searchHandle);
        }
    }
    return (retFiles.size() - startSize) > 0;
#else
    glob_t glob_result;
    if (glob(sFileName.c_str(), GLOB_TILDEE | GLOB_NOSORT | GLOB_NOESCAPE, NULL, &glob_result) == 0) {
        for (unsigned int i = 0; i < glob_result.gl_pathc; ++i) {
            retFiles.push_back(string(glob_result.gl_pathv[i]));
        }
        globfree(&glob_result);
        return (retFiles.size() - uiStartSize) > 0;
    }
    return false;
#endif
}

bool findFolders(const string& folderSearch, vector<string>& retFolders, const bool recursive)
{
#ifdef _WIN32
    WIN32_FIND_DATA searchFile;
    const uint startSize = retFolders.size();
    string path;
    string searchTerm = folderSearch;
    uint pos = searchTerm.rfind('/');
    if (pos == (searchTerm.length() - 1)) {
        pos = searchTerm.find_last_not_of('/');
        pos = searchTerm.rfind('/', pos - 1);
    }
    if (pos != string::npos) {
        ++pos;
        path = folderSearch.substr(0, pos);
        searchTerm = folderSearch.substr(pos);
    }
    HANDLE searchHandle = FindFirstFile(folderSearch.c_str(), &searchFile);
    if (searchHandle != INVALID_HANDLE_VALUE) {
        // Update the return filename list
        retFolders.push_back(path + searchFile.cFileName);
        while (FindNextFile(searchHandle, &searchFile) != 0) {
            if (searchFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                retFolders.push_back(path + searchFile.cFileName);
                replace(retFolders.back().begin(), retFolders.back().end(), '\\', '/');
            }
        }
        FindClose(searchHandle);
    }
    // Search all sub directories as well
    if (recursive) {
        string search = path + "*";
        searchHandle = FindFirstFile(search.c_str(), &searchFile);
        if (searchHandle != INVALID_HANDLE_VALUE) {
            BOOL cont = TRUE;
            while (cont == TRUE) {
                if (searchFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // this is a directory
                    if (strcmp(searchFile.cFileName, ".") != 0 && strcmp(searchFile.cFileName, "..") != 0) {
                        string newPath = path + searchFile.cFileName + '/' + searchTerm;
                        findFolders(newPath, retFolders);
                    }
                }
                cont = FindNextFile(searchHandle, &searchFile);
            }
            FindClose(searchHandle);
        }
    }
    return (retFolders.size() - startSize) > 0;
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

void makePathsRelative(const string& path, const string& makeRelativeTo, string& retPath)
{
#ifdef _WIN32
    retPath.reserve(MAX_PATH);
    retPath.resize(MAX_PATH);
    string fromT, toT;
    fromT.reserve(MAX_PATH);
    toT.reserve(MAX_PATH);
    if (path.length() > 0) {
        GetFullPathName(path.c_str(), MAX_PATH, const_cast<char*>(fromT.data()), NULL);
    } else {
        GetFullPathName("./", MAX_PATH, const_cast<char*>(fromT.data()), NULL);
    }
    if (makeRelativeTo.length() > 0) {
        GetFullPathName(makeRelativeTo.c_str(), MAX_PATH, const_cast<char*>(toT.data()), NULL);
    } else {
        GetFullPathName("./", MAX_PATH, const_cast<char*>(toT.data()), NULL);
    }
    PathRelativePathTo(
        const_cast<char*>(retPath.data()), toT.c_str(), FILE_ATTRIBUTE_DIRECTORY, fromT.c_str(), FILE_ATTRIBUTE_NORMAL);
    retPath.resize(strlen(retPath.c_str()));
    replace(retPath.begin(), retPath.end(), '\\', '/');
#else
    retPath.reserve(MAX_PATH);
    retPath.resize(MAX_PATH);
    string sFromT, sToT;
    sFromT.reserve(MAX_PATH);
    sToT.reserve(MAX_PATH);
    realpath(path.c_str(), const_cast<char*>(sFromT.data()));
    realpath(makeRelativeTo.c_str(), const_cast<char*>(sToT.data()));
    retPath.at(0) = '\0';
    int iPos;
    int iSizeA = strlen(path.c_str()) + 1;
    int iSizeR = strlen(makeRelativeTo.c_str()) + 1;
    for (iPos = 0; iPos < iSizeA && iPos < iSizeR; iPos += strlen(path.c_str() + iPos) + 1) {
        const char* p_PosA = strchr(path.c_str() + iPos, '/');
        const char* p_PosR = strchr(makeRelativeTo.c_str() + iPos, '/');
        if (p_PosA)
            const_cast<char*>(p_PosA)[0] = '\0';
        if (p_PosR)
            const_cast<char*>(p_PosR)[0] = '\0';
        if (strcmp(path.c_str() + iPos, makeRelativeTo.c_str() + iPos) != 0)
            break;
    }
    for (int iPos2 = iPos; iPos2 < iSizeR; iPos2 += strlen(makeRelativeTo.c_str() + iPos2) + 1) {
        strcat(const_cast<char*>(retPath.data()), "../");
        if (!strchr(retPath.c_str() + iPos2, '/'))
            break;
    }
    if (iPos < iSizeA)
        strcat(const_cast<char*>(retPath.data()), path.c_str() + iPos);
    retPath.resize(strlen(retPath.c_str()));
#endif
}

void removeWhiteSpace(string& inputString)
{
    struct RemoveDelimiter
    {
        bool operator()(const char c) const
        {
            return (g_whiteSpace.find(c) != string::npos);
        }
    };

    inputString.erase(remove_if(inputString.begin(), inputString.end(), RemoveDelimiter()), inputString.end());
}

void findAndReplace(string& inString, const string& search, const string& replace)
{
    uint uiPos = 0;
    while ((uiPos = inString.find(search, uiPos)) != std::string::npos) {
        inString.replace(uiPos, search.length(), replace);
        uiPos += replace.length();
    }
}

bool findEnvironmentVariable(const string& envVar)
{
#ifdef _WIN32
    return (GetEnvironmentVariable(envVar.c_str(), NULL, 0) > 0);
#else
    return false;
#endif
}

void pressKeyToContinue()
{
#if _WIN32
    system("pause");
#else
    system("read -rsn 1 -p \"Press any key to continue...\"");
#endif
}

void outputLine(const string& message)
{
    cout << message << endl;
}

void outputInfo(const string& message, const bool header)
{
#if _WIN32
    HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hstdout, &csbi);
    SetConsoleTextAttribute(hstdout, FOREGROUND_GREEN);
#endif
    if (s_outputVerbosity <= VERBOSITY_INFO) {
        if (header) {
            cout << "  Info: ";
        } else {
            cout << "        ";
        }
        cout << message << endl;
    }
#if _WIN32
    SetConsoleTextAttribute(hstdout, csbi.wAttributes);
#endif
}

void outputWarning(const string& message, const bool header)
{
#if _WIN32
    HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hstdout, &csbi);
    SetConsoleTextAttribute(hstdout, FOREGROUND_RED | FOREGROUND_GREEN);
#endif
    if (s_outputVerbosity <= VERBOSITY_WARNING) {
        if (header) {
            cout << "  Warning: ";
        } else {
            cout << "           ";
        }
        cout << message << endl;
    }
#if _WIN32
    SetConsoleTextAttribute(hstdout, csbi.wAttributes);
#endif
}

void outputError(const string& message, const bool header)
{
#if _WIN32
    HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hstdout, &csbi);
    SetConsoleTextAttribute(hstdout, FOREGROUND_RED);
#endif
    if (header) {
        cout << "  Error: ";
    } else {
        cout << "         ";
    }
    cout << message << endl;
#if _WIN32
    SetConsoleTextAttribute(hstdout, csbi.wAttributes);
#endif
}

void setOutputVerbosity(const Verbosity verbose)
{
    s_outputVerbosity = verbose;
}
};    // namespace project_generate