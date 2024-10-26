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

#include <algorithm>

void ProjectGenerator::buildInterDependenciesHelper(
    const StaticList& configOptions, const StaticList& addDeps, StaticList& libs) const
{
    bool found = false;
    for (const auto& i : configOptions) {
        found = m_configHelper.isConfigOptionEnabled(i);
        if (!found) {
            break;
        }
    }
    if (found) {
        for (const auto& i : addDeps) {
            string sSearchTag = "lib" + i;
            if (find(libs.begin(), libs.end(), sSearchTag) == libs.end()) {
                libs.push_back(sSearchTag);
            }
        }
    }
}

void ProjectGenerator::buildInterDependencies(StaticList& libs)
{
    // Get the lib dependencies from the configure file
    const string libName = m_projectName.substr(3) + "_deps";
    vector<string> libDeps;
    if (m_configHelper.getConfigList(libName, libDeps, false)) {
        for (const auto& i : libDeps) {
            string searchTag = "lib" + i;
            if (find(libs.begin(), libs.end(), searchTag) == libs.end()) {
                libs.push_back(searchTag);
            }
        }
    }

    ConfigGenerator::InterDependencies interDependencies;
    m_configHelper.buildInterDependencies(interDependencies);

    const auto found = interDependencies.find(m_projectName.substr(3));
    if (found != interDependencies.end()) {
        for (const auto& i : found->second) {
            buildInterDependenciesHelper(i.first, i.second, libs);
        }
    }
}

void ProjectGenerator::buildDependencies(StaticList& libs, StaticList& addLibs, const bool winrt)
{
    // Add any forced dependencies
    if (m_projectName == "libavformat" && !winrt) {
        addLibs.emplace_back("ws2_32"); // Add the additional required libs
    }

    // Determine only those dependencies that are valid for current project
    map<string, bool> projectDeps;
    buildProjectDependencies(projectDeps);

    // Loop through each known configuration option and add the required dependencies
    vector<string> externLibs;
    m_configHelper.getConfigList("EXTERNAL_AUTODETECT_LIBRARY_LIST", externLibs, false);
    m_configHelper.getConfigList("EXTERNAL_LIBRARY_LIST", externLibs);
    m_configHelper.getConfigList("HW_CODECS_LIST", externLibs, false); // used on some older ffmpeg versions
    m_configHelper.getConfigList("HWACCEL_AUTODETECT_LIBRARY_LIST", externLibs, false);
    m_configHelper.getConfigList("HWACCEL_LIBRARY_LIST", externLibs, false);
    m_configHelper.getConfigList("SYSTEM_LIBRARIES", externLibs, false);
    for (const auto& i : externLibs) {
        // Check if enabled
        if (m_configHelper.isConfigOptionEnabled(i)) {
            // Check if this dependency is valid for this project (if the dependency is not known default to enable)
            if (projectDeps.find(i) == projectDeps.end()) {
                outputInfo("Unknown dependency found (" + i + ")");
            } else if (!projectDeps[i]) {
                // This dependency is not valid for this project so skip it
                continue;
            }

            // Check if this value is conditional on winrt
            if (winrt) {
                // Convert to full config value
                const auto name =
                    m_configHelper.getConfigOption(i)->m_prefix + m_configHelper.getConfigOption(i)->m_option;
                if (m_configHelper.m_replaceList.find(name) != m_configHelper.m_replaceList.end()) {
                    // Skip this option
                    if (m_configHelper.m_replaceList[name].find("!HAVE_WINRT") != string::npos ||
                        m_configHelper.m_replaceList[name].find("!HAVE_UWP") != string::npos) {
                        continue;
                    }
                }
            }

            string lib;
            if (i == "amf") {
                // doesn't need any additional libs
            } else if (i == "avisynth") {
                // doesn't need any additional libs
            } else if (i == "bcrypt") {
                addLibs.emplace_back("Bcrypt"); // Add the additional required libs
            } else if (i == "bzlib") {
                lib = "libbz2";
            } else if (i == "d3d12va") {
                if (winrt) {
                    if (std::find(addLibs.cbegin(), addLibs.cend(), "dxgi") == addLibs.cend()) {
                        addLibs.emplace_back("dxgi");
                    }
                    addLibs.emplace_back("d3d12");
                }
                // doesn't need any additional libs
            } else if (i == "d3d11va") {
                if (winrt) {
                    if (std::find(addLibs.cbegin(), addLibs.cend(), "dxgi") == addLibs.cend()) {
                        addLibs.emplace_back("dxgi");
                    }
                    addLibs.emplace_back("d3d11");
                }
                // doesn't need any additional libs
            } else if (i == "dxva2") {
                // doesn't need any additional libs
            } else if (i == "libcdio") {
                lib = "libcdio_paranoia";
            } else if (i == "libfdk_aac") {
                lib = "libfdk-aac";
            } else if (i == "libnpp") {
                addLibs.emplace_back("nppi"); // Add the additional required libs
                // CUDA 7.5 onwards only provides npp for x64
            } else if (i == "libxvid") {
                lib = "libxvidcore";
            } else if (i == "openssl") {
                // Needs ws2_32 but libavformat needs this even if not using openssl so it is already included
                lib = "libssl";
                // Also need crypto
                auto list = libs.begin();
                for (; list < libs.end(); ++list) {
                    if (*list == "libcrypto") {
                        break;
                    }
                }
                if (list == libs.end()) {
                    libs.emplace_back("libcrypto");
                }
            } else if (i == "decklink") {
                // Doesn't need any additional libs
            } else if (i == "opengl") {
                addLibs.emplace_back("Opengl32"); // Add the additional required libs
            } else if (i == "opencl") {
                string fileName;
                if (!findFile(m_configHelper.m_rootDirectory + "compat/opencl/cl.h", fileName)) {
                    addLibs.emplace_back("OpenCL"); // Add the additional required libs
                }
            } else if (i == "openal") {
                addLibs.emplace_back("OpenAL32"); // Add the additional required libs
            } else if (i == "ffnvcodec") {
                // Doesn't require any additional libs
            } else if (i == "nvenc") {
                // Doesn't require any additional libs
            } else if (i == "cuda") {
                string fileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/cuda/dynlink_cuda.h", fileName)) {
                    addLibs.emplace_back("cuda"); // Add the additional required libs
                }
            } else if (i == "cuda_sdk" || i == "cuda_nvcc") {
                // Doesn't require any additional libs
            } else if (i == "cuvid") {
                string fileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/cuda/dynlink_nvcuvid.h", fileName)) {
                    addLibs.emplace_back("nvcuvid"); // Add the additional required libs
                }
            } else if ((i == "nvdec") || (i == "nvenc")) {
                // Doesn't need any additional libs
            } else if (i == "mediafoundation") {
                addLibs.emplace_back("mfplat");
                addLibs.emplace_back("mfuuid");
                if (!winrt) {
                    addLibs.emplace_back("strmiids");
                }
            } else if (i == "schannel") {
                addLibs.emplace_back("Secur32"); // Add the additional required libs
            } else if (i == "sdl") {
                if (!m_configHelper.isConfigOptionValid("sdl2")) {
                    libs.emplace_back("libsdl"); // Only add if not sdl2
                }
            } else if (i == "wincrypt") {
                addLibs.emplace_back("Advapi32"); // Add the additional required libs
            } else {
                // By default just use the lib name and prefix with lib if not already
                if (i.find("lib") == 0) {
                    lib = i;
                } else {
                    lib = "lib" + i;
                }
            }
            if (lib.length() > 0) {
                // Check already not in list
                auto list = libs.begin();
                for (; list < libs.end(); ++list) {
                    if (*list == lib) {
                        break;
                    }
                }
                if (list == libs.end()) {
                    libs.push_back(lib);
                }
            }
        }
    }

    // Add in extralibs used for various devices
    if (m_projectName == "libavdevice" && !winrt) {
        externLibs.resize(0);
        m_configHelper.getConfigList("OUTDEV_LIST", externLibs);
        m_configHelper.getConfigList("INDEV_LIST", externLibs);
        for (const auto& i : externLibs) {
            // Check if enabled
            if (m_configHelper.isConfigOptionEnabled(i)) {
                // Add the additional required libs
                if (i == "dshow_indev") {
                    addLibs.emplace_back("strmiids");
                } else if (i == "vfwcap_indev") {
                    addLibs.emplace_back("vfw32");
                    addLibs.emplace_back("shlwapi");
                } else if ((i == "wasapi_indev") || (i == "wasapi_outdev")) {
                    addLibs.emplace_back("ksuser");
                }
            }
        }
    }
}

void ProjectGenerator::buildDependencyValues(StaticList& includeDirs, StaticList& lib32Dirs, StaticList& lib64Dirs,
    StaticList& definesShared, StaticList& definesStatic, const bool winrt) const
{
    // Add hard dependencies
    string projRoot = "$(ProjectDir)/";
    string atomicCompatFile = m_configHelper.m_rootDirectory + "compat/atomics/win32/stdatomic.h";
    string dep;
    if (findFile(atomicCompatFile, dep)) {
        m_configHelper.makeFileProjectRelative(atomicCompatFile, atomicCompatFile);
        uint pos = atomicCompatFile.rfind('/'); // Get path only
        atomicCompatFile = atomicCompatFile.substr(0, ++pos);
        includeDirs.push_back("$(ProjectDir)/" + atomicCompatFile);
    }
    string bitCompatFile = m_configHelper.m_rootDirectory + "compat/stdbit/stdbit.h";
    if (findFile(bitCompatFile, dep)) {
        m_configHelper.makeFileProjectRelative(bitCompatFile, bitCompatFile);
        uint pos = bitCompatFile.rfind('/'); // Get path only
        bitCompatFile = bitCompatFile.substr(0, ++pos);
        includeDirs.push_back("$(ProjectDir)/" + bitCompatFile);
    }

    // Add root directory
    if (m_configHelper.m_rootDirectory != "./" && m_configHelper.m_rootDirectory != "../") {
        m_configHelper.makeFileProjectRelative(m_configHelper.m_rootDirectory, dep);
        projRoot += dep;
        includeDirs.push_back(projRoot);
    }
    
    // Add subdirectory include dirs (m_subDirs cannot be used as it creates clashes with identically named files)
    if (!m_subDirs.empty()) {
        includeDirs.push_back(projRoot + m_projectName + '/');
    }

    // Determine only those dependencies that are valid for current project
    map<string, bool> projectDeps;
    buildProjectDependencies(projectDeps);

    // Loop through each known configuration option and add the required dependencies
    for (const auto& i : projectDeps) {
        // Check if enabled//Check if optimised value is valid for current configuration
        if (i.second && m_configHelper.isConfigOptionEnabled(i.first)) {
            // Add in the additional include directories
            if (i.first == "libopus") {
                includeDirs.emplace_back("$(OutDir)/include/opus/");
                includeDirs.emplace_back("$(ProjectDir)/../../prebuilt/include/opus/");
            } else if (i.first == "libfreetype") {
                includeDirs.emplace_back("$(OutDir)/include/freetype2/");
                includeDirs.emplace_back("$(ProjectDir)/../../prebuilt/include/freetype2/");
            } else if (i.first == "libfribidi") {
                includeDirs.emplace_back("$(OutDir)/include/fribidi/");
                includeDirs.emplace_back("$(ProjectDir)/../../prebuilt/include/fribidi/");
                definesStatic.emplace_back("FRIBIDI_LIB_STATIC");
            } else if (i.first == "libharfbuzz") {
                includeDirs.emplace_back("$(OutDir)/include/harfbuzz/");
                includeDirs.emplace_back("$(ProjectDir)/../../prebuilt/include/harfbuzz/");
            } else if (i.first == "libilbc") {
                definesStatic.emplace_back("ILBC_STATIC_DEFINE");
            } else if (i.first == "libx264") {
                definesShared.emplace_back("X264_API_IMPORTS");
            } else if (i.first == "libxml2") {
                includeDirs.emplace_back("$(OutDir)/include/libxml2/");
                includeDirs.emplace_back("$(ProjectDir)/../../prebuilt/include/libxml2/");
                definesStatic.emplace_back("LIBXML_STATIC");
            } else if (i.first == "libmfx") {
                includeDirs.emplace_back("$(OutDir)/include/mfx/");
                includeDirs.emplace_back("$(ProjectDir)/../../prebuilt/include/mfx/");
            } else if (i.first == "sdl2" || ((i.first == "sdl") && !m_configHelper.isConfigOptionValid("sdl2"))) {
                includeDirs.emplace_back("$(OutDir)/include/SDL/");
                includeDirs.emplace_back("$(ProjectDir)/../../prebuilt/include/SDL/");
            } else if (i.first == "opengl" && !winrt) {
                // Requires glext headers to be installed in include dir (does not require the libs)
            } else if (i.first == "opencl" && !winrt) {
                string fileName;
                if (!findFile(m_configHelper.m_rootDirectory + "compat/opencl/cl.h", fileName)) {
                    // Need to check for the existence of environment variables
                    if (findEnvironmentVariable("AMDAPPSDKROOT")) {
                        includeDirs.emplace_back("$(AMDAPPSDKROOT)/include/");
                        lib32Dirs.emplace_back("$(AMDAPPSDKROOT)/lib/Win32");
                        lib64Dirs.emplace_back("$(AMDAPPSDKROOT)/lib/x64");
                    } else if (findEnvironmentVariable("INTELOCLSDKROOT")) {
                        includeDirs.emplace_back("$(INTELOCLSDKROOT)/include/");
                        lib32Dirs.emplace_back("$(INTELOCLSDKROOT)/lib/x86");
                        lib64Dirs.emplace_back("$(INTELOCLSDKROOT)/lib/x64");
                    } else if (findEnvironmentVariable("CUDA_PATH")) {
                        includeDirs.emplace_back("$(CUDA_PATH)/include/");
                        lib32Dirs.emplace_back("$(CUDA_PATH)/lib/Win32");
                        lib64Dirs.emplace_back("$(CUDA_PATH)/lib/x64");
                    } else {
                        outputWarning("Could not find an OpenCl SDK environment variable.");
                        outputWarning(
                            "Either an OpenCL SDK is not installed or the environment variables are missing.", false);
                        outputWarning(
                            "The location of the OpenCl files will have to be manually specified as otherwise the project will not compile.",
                            false);
                    }
                }
            } else if (i.first == "openal" && !winrt) {
                if (!findEnvironmentVariable("OPENAL_SDK")) {
                    outputWarning("Could not find the OpenAl SDK environment variable.");
                    outputWarning(
                        "Either the OpenAL SDK is not installed or the environment variable is missing.", false);
                    outputWarning("Using the default environment variable of 'OPENAL_SDK'.", false);
                }
                includeDirs.emplace_back("$(OPENAL_SDK)/include/");
                lib32Dirs.emplace_back("$(OPENAL_SDK)/libs/Win32");
                lib64Dirs.emplace_back("$(OPENAL_SDK)/lib/Win64");
            } else if (i.first == "nvenc" && !winrt) {
                string fileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/nvenc/nvEncodeAPI.h", fileName)) {
                    // Need to check for the existence of environment variables
                    if (!findEnvironmentVariable("CUDA_PATH")) {
                        outputWarning("Could not find the CUDA SDK environment variable.");
                        outputWarning(
                            "Either the CUDA SDK is not installed or the environment variable is missing.", false);
                        outputWarning(
                            "NVENC requires CUDA to be installed with NVENC headers made available in the CUDA SDK include path.",
                            false);
                    }
                    // Only add if it hasn't already been added
                    if (find(includeDirs.begin(), includeDirs.end(), "$(CUDA_PATH)/include/") == includeDirs.end()) {
                        includeDirs.emplace_back("$(CUDA_PATH)/include/");
                    }
                }
            } else if (((i.first == "cuda") || (i.first == "cuvid")) && !winrt) {
                string fileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/cuda/dynlink_cuda.h", fileName)) {
                    // Need to check for the existence of environment variables
                    if (!findEnvironmentVariable("CUDA_PATH")) {
                        outputWarning("Could not find the CUDA SDK environment variable.");
                        outputWarning(
                            "Either the CUDA SDK is not installed or the environment variable is missing.", false);
                    }
                    // Only add if it hasn't already been added
                    if (find(includeDirs.begin(), includeDirs.end(), "$(CUDA_PATH)/include/") == includeDirs.end()) {
                        includeDirs.emplace_back("$(CUDA_PATH)/include/");
                        lib32Dirs.emplace_back("$(CUDA_PATH)/lib/Win32");
                        lib64Dirs.emplace_back("$(CUDA_PATH)/lib/x64");
                    }
                }
            }
        }
    }
}

void ProjectGenerator::buildProjectDependencies(map<string, bool>& projectDeps) const
{
    string notUsed;
    projectDeps["amf"] = (m_projectName == "libavutil") || (m_projectName == "libavcodec");
    projectDeps["avisynth"] = (m_projectName == "libavformat");
    projectDeps["bcrypt"] = (m_projectName == "libavutil");
    projectDeps["bzlib"] = (m_projectName == "libavformat") || (m_projectName == "libavcodec");
    projectDeps["crystalhd"] = (m_projectName == "libavcodec");
    projectDeps["chromaprint"] = (m_projectName == "libavformat");
    projectDeps["cuda"] = ((m_projectName == "libavutil") && findSourceFile("hwcontext_cuda", ".h", notUsed)) ||
        (m_projectName == "libavfilter") ||
        (m_configHelper.isConfigOptionEnabled("nvenc") && (m_projectName == "libavcodec")) ||
        (m_configHelper.isConfigOptionEnabled("cuvid") &&
            ((m_projectName == "libavcodec") || (m_projectName == "ffmpeg") || (m_projectName == "avconv")));
    projectDeps["cuda_sdk"] = (m_projectName == "libavfilter");
    projectDeps["cuda_nvcc"] = (m_projectName == "libavfilter");
    projectDeps["cuvid"] =
        (m_projectName == "libavcodec") || (m_projectName == "ffmpeg") || (m_projectName == "avconv");
    projectDeps["d3d12va"] = (m_projectName == "libavutil") || (m_projectName == "libavcodec");
    projectDeps["d3d11va"] = (m_projectName == "libavutil") || (m_projectName == "libavcodec");
    projectDeps["dxva2"] = (m_projectName == "libavutil") || (m_projectName == "libavcodec");
    projectDeps["decklink"] = (m_projectName == "libavdevice");
    projectDeps["libfontconfig"] = (m_projectName == "libavfilter");
    projectDeps["ffnvcodec"] = (m_projectName == "libavutil") || (m_projectName == "libavcodec");
    projectDeps["frei0r"] = (m_projectName == "libavfilter");
    projectDeps["gcrypt"] = (m_projectName == "libavformat") || (m_projectName == "libavutil");
    projectDeps["gmp"] = (m_projectName == "libavformat");
    projectDeps["gnutls"] = (m_projectName == "libavformat");
    projectDeps["iconv"] = (m_projectName == "libavformat") || (m_projectName == "libavcodec");
    projectDeps["ladspa"] = (m_projectName == "libavfilter");
    projectDeps["libaacplus"] = (m_projectName == "libavcodec");
    projectDeps["libaom"] = (m_projectName == "libavcodec");
    projectDeps["libaribb24"] = (m_projectName == "libavcodec");
    projectDeps["libass"] = (m_projectName == "libavfilter");
    projectDeps["libbluray"] = (m_projectName == "libavformat");
    projectDeps["libbs2b"] = (m_projectName == "libavfilter");
    projectDeps["libcaca"] = (m_projectName == "libavdevice");
    projectDeps["libcdio"] = (m_projectName == "libavdevice");
    projectDeps["libcelt"] = (m_projectName == "libavcodec");
    projectDeps["libcodec2"] = (m_projectName == "libavcodec");
    projectDeps["libdav1d"] = (m_projectName == "libavcodec");
    projectDeps["libdavs2"] = (m_projectName == "libavcodec");
    projectDeps["libdc1394"] = (m_projectName == "libavdevice");
    projectDeps["libdcadec"] = (m_projectName == "libavcodec");
    projectDeps["libfaac"] = (m_projectName == "libavcodec");
    projectDeps["libfdk_aac"] = (m_projectName == "libavcodec");
    projectDeps["libflite"] = (m_projectName == "libavfilter");
    projectDeps["libfontconfig"] = (m_projectName == "libavfilter");
    projectDeps["libfreetype"] = (m_projectName == "libavfilter");
    projectDeps["libfribidi"] = (m_projectName == "libavfilter");
    projectDeps["libgme"] = (m_projectName == "libavformat");
    projectDeps["libgsm"] = (m_projectName == "libavcodec");
    projectDeps["libharfbuzz"] = (m_projectName == "libavfilter");
    projectDeps["libiec61883"] = (m_projectName == "libavdevice");
    projectDeps["libilbc"] = (m_projectName == "libavcodec");
    projectDeps["libkvazaar"] = (m_projectName == "libavcodec");
    projectDeps["libmfx"] = ((m_projectName == "libavutil") && findSourceFile("hwcontext_qsv", ".h", notUsed)) ||
        (m_projectName == "libavcodec") ||
        ((m_projectName == "libavfilter") &&
            (findSourceFile("vf_deinterlace_qsv", ".c", notUsed) || findSourceFile("vf_stack_qsv", ".c", notUsed))) ||
        (m_projectName == "ffmpeg") || (m_projectName == "avconv");
    projectDeps["libmodplug"] = (m_projectName == "libavformat");
    projectDeps["libmp3lame"] = (m_projectName == "libavcodec");
    projectDeps["libnpp"] = (m_projectName == "libavfilter");
    projectDeps["libnut"] = (m_projectName == "libformat");
    projectDeps["libopencore_amrnb"] = (m_projectName == "libavcodec");
    projectDeps["libopencore_amrwb"] = (m_projectName == "libavcodec");
    projectDeps["libopencv"] = (m_projectName == "libavfilter");
    projectDeps["libopenjpeg"] = (m_projectName == "libavcodec");
    projectDeps["libopenh264"] = (m_projectName == "libavcodec");
    projectDeps["libopenmpt"] = (m_projectName == "libavformat");
    projectDeps["libopenvino"] = (m_projectName == "libavfilter");
    projectDeps["libopus"] = (m_projectName == "libavcodec");
    projectDeps["libpulse"] = (m_projectName == "libavdevice");
    projectDeps["libquvi"] = (m_projectName == "libavformat");
    projectDeps["librabbitmq"] = (m_projectName == "libavformat");
    projectDeps["librav1e"] = (m_projectName == "libavcodec");
    projectDeps["librist"] = (m_projectName == "libavformat");
    projectDeps["librsvg"] = (m_projectName == "libavcodec");
    projectDeps["librtmp"] = (m_projectName == "libavformat");
    projectDeps["librubberband"] = (m_projectName == "libavfilter");
    projectDeps["libschroedinger"] = (m_projectName == "libavcodec");
    projectDeps["libshine"] = (m_projectName == "libavcodec");
    projectDeps["libsmbclient"] = (m_projectName == "libavformat");
    projectDeps["libsnappy"] = (m_projectName == "libavcodec");
    projectDeps["libsoxr"] = (m_projectName == "libswresample");
    projectDeps["libspeex"] = (m_projectName == "libavcodec");
    projectDeps["libsrt"] = (m_projectName == "libavformat");
    projectDeps["libssh"] = (m_projectName == "libavformat");
    projectDeps["libstagefright_h264"] = (m_projectName == "libavcodec");
    projectDeps["libsvtav1"] = (m_projectName == "libavcodec");
    projectDeps["libtensorflow"] = (m_projectName == "libavfilter");
    projectDeps["libtesseract"] = (m_projectName == "libavfilter");
    projectDeps["libtheora"] = (m_projectName == "libavcodec");
    projectDeps["libtls"] = (m_projectName == "libavformat");
    projectDeps["libtwolame"] = (m_projectName == "libavcodec");
    projectDeps["libuavs3d"] = (m_projectName == "libavcodec");
    projectDeps["libutvideo"] = (m_projectName == "libavcodec");
    projectDeps["libv4l2"] = (m_projectName == "libavdevice");
    projectDeps["libvidstab"] = (m_projectName == "libavfilter");
    projectDeps["libvmaf"] = (m_projectName == "libavfilter");
    projectDeps["libvo_aacenc"] = (m_projectName == "libavcodec");
    projectDeps["libvo_amrwbenc"] = (m_projectName == "libavcodec");
    projectDeps["libvorbis"] = (m_projectName == "libavcodec");
    projectDeps["libvpx"] = (m_projectName == "libavcodec");
    projectDeps["libwavpack"] = (m_projectName == "libavcodec");
    projectDeps["libwebp"] = (m_projectName == "libavcodec");
    projectDeps["libx264"] = (m_projectName == "libavcodec");
    projectDeps["libx265"] = (m_projectName == "libavcodec");
    projectDeps["libxavs"] = (m_projectName == "libavcodec");
    projectDeps["libxavs2"] = (m_projectName == "libavcodec");
    projectDeps["libxml2"] = (m_projectName == "libavformat");
    projectDeps["libxvid"] = (m_projectName == "libavcodec");
    projectDeps["libzimg"] = (m_projectName == "libavfilter");
    projectDeps["libzmq"] = (m_projectName == "libavfilter") || (m_projectName == "libavformat");
    projectDeps["libzvbi"] = (m_projectName == "libavcodec");
    projectDeps["lzma"] = (m_projectName == "libavcodec");
    projectDeps["mediafoundation"] = (m_projectName == "libavcodec");
    projectDeps["nvdec"] = (m_projectName == "libavcodec");
    projectDeps["nvenc"] = (m_projectName == "libavcodec");
    projectDeps["openal"] = (m_projectName == "libavdevice");
    projectDeps["opencl"] = (m_projectName == "libavutil") || (m_projectName == "libavfilter") ||
        (m_projectName == "ffmpeg") || (m_projectName == "avconv") || (m_projectName == "ffplay") ||
        (m_projectName == "avplay") || (m_projectName == "ffprobe") || (m_projectName == "avprobe");
    projectDeps["opengl"] = (m_projectName == "libavdevice");
    projectDeps["openssl"] = (m_projectName == "libavformat") || (m_projectName == "libavutil");
    projectDeps["schannel"] = (m_projectName == "libavformat");
    projectDeps["sdl"] = (m_projectName == "libavdevice") || (m_projectName == "ffplay") || (m_projectName == "avplay");
    projectDeps["sdl2"] =
        (m_projectName == "libavdevice") || (m_projectName == "ffplay") || (m_projectName == "avplay");
    projectDeps["vapoursynth"] = m_projectName.compare("libavformat");
    projectDeps["zlib"] = (m_projectName == "libavformat") || (m_projectName == "libavcodec");
}

void ProjectGenerator::buildProjectGUIDs(map<string, string>& keys) const
{
    keys["libavcodec"] = "B4824EFF-C340-425D-A4A8-E2E02A71A7AE";
    keys["libavdevice"] = "6E165FA4-44EB-4330-8394-9F0D76D8E03E";
    keys["libavfilter"] = "BC2E1028-66CD-41A0-AF90-EEBD8CC52787";
    keys["libavformat"] = "30A96E9B-8061-4F19-BD71-FDE7EA8F7929";
    keys["libavresample"] = "0096CB8C-3B04-462B-BF4F-0A9970A57C91";
    keys["libavutil"] = "CE6C44DD-6E38-4293-8AB3-04EE28CCA972";
    keys["libswresample"] = "3CE4A9EF-98B6-4454-B76E-3AD9C03A2114";
    keys["libswscale"] = "6D8A6330-8EBE-49FD-9281-0A396F9F28F2";
    keys["libpostproc"] = "4D9C457D-9ADA-4A12-9D06-42D80124C5AB";

    keys["libavcodec_winrt"] = "B4824EFF-C340-425D-A4A8-E2E02A71A7AF";
    keys["libavdevice_winrt"] = "6E165FA4-44EB-4330-8394-9F0D76D8E03F";
    keys["libavfilter_winrt"] = "BC2E1028-66CD-41A0-AF90-EEBD8CC5278F";
    keys["libavformat_winrt"] = "30A96E9B-8061-4F19-BD71-FDE7EA8F792F";
    keys["libavresample_winrt"] = "0096CB8C-3B04-462B-BF4F-0A9970A57C9F";
    keys["libavutil_winrt"] = "CE6C44DD-6E38-4293-8AB3-04EE28CCA97F";
    keys["libswresample_winrt"] = "3CE4A9EF-98B6-4454-B76E-3AD9C03A211F";
    keys["libswscale_winrt"] = "6D8A6330-8EBE-49FD-9281-0A396F9F28FF";
    keys["libpostproc_winrt"] = "4D9C457D-9ADA-4A12-9D06-42D80124C5AF";

    if (!m_configHelper.m_isLibav) {
        keys["ffmpeg"] = "4081C77E-F1F7-49FA-9BD8-A4D267C83716";
        keys["ffplay"] = "E2A6865D-BD68-45B4-8130-EFD620F2C7EB";
        keys["ffprobe"] = "147A422A-FA63-4724-A5D9-08B1CAFDAB59";
    } else {
        keys["avconv"] = "4081C77E-F1F7-49FA-9BD8-A4D267C83716";
        keys["avplay"] = "E2A6865D-BD68-45B4-8130-EFD620F2C7EB";
        keys["avprobe"] = "147A422A-FA63-4724-A5D9-08B1CAFDAB59";
    }
}

void ProjectGenerator::buildProjectDCEs(map<string, DCEParams>&, map<string, DCEParams>& variablesDCE) const
{
    // TODO: Detect these automatically
    // Next we need to check for all the configurations that are project specific
    struct FindThingsVars
    {
        string m_list;
        string m_search;
        string m_file;
        string m_header;
    };
    vector<FindThingsVars> searchLists;
    if (m_projectName == "libavcodec") {
        searchLists.push_back({"encoder", "ENC", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        searchLists.push_back({"decoder", "DEC", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        searchLists.push_back({"hwaccel", "HWACCEL", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        searchLists.push_back({"parser", "PARSER", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
    } else if (m_projectName == "libavformat") {
        searchLists.push_back({"muxer", "_MUX", "libavformat/allformats.c", "libavformat/avformat.h"});
        searchLists.push_back({"demuxer", "DEMUX", "libavformat/allformats.c", "libavformat/avformat.h"});
    } else if (m_projectName == "libavfilter") {
        searchLists.push_back({"filter", "FILTER", "libavfilter/allfilters.c", "libavfilter/avfilter.h"});
    } else if (m_projectName == "libavdevice") {
        searchLists.push_back({"outdev", "OUTDEV", "libavdevice/alldevices.c", "libavdevice/avdevice.h"});
        searchLists.push_back({"indev", "_IN", "libavdevice/alldevices.c", "libavdevice/avdevice.h"});
    }

    for (const auto& i : searchLists) {
        // We need to get the values from each config list
        StaticList ret;
        StaticList retExterns;
        string list = i.m_list;
        transform(list.begin(), list.end(), list.begin(), toupper);
        m_configHelper.passFindThings(i.m_list, i.m_search, i.m_file, ret, &retExterns);
        for (auto itRet = ret.begin(), itRet2 = retExterns.begin(); itRet < ret.end(); ++itRet, ++itRet2) {
            string sType = *itRet2;
            transform(itRet->begin(), itRet->end(), itRet->begin(), toupper);
            variablesDCE[sType] = {"CONFIG_" + *itRet, i.m_header};
        }
    }
}
