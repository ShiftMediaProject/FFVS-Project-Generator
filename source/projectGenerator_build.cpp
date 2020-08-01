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

    // Hard coded configuration checks for inter dependencies between different source libs.
    if (m_projectName == "libavfilter") {
        buildInterDependenciesHelper({"afftfilt_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"afir_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"amovie_filter"}, {"avformat", "avcodec"}, libs);
        buildInterDependenciesHelper({"aresample_filter"}, {"swresample"}, libs);
        buildInterDependenciesHelper({"asyncts_filter"}, {"avresample"}, libs);
        buildInterDependenciesHelper({"atempo_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"cover_rect_filter"}, {"avformat", "avcodec"}, libs);
        buildInterDependenciesHelper({"ebur128_filter", "swresample"}, {"swresample"}, libs);
        buildInterDependenciesHelper({"elbg_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"fftfilt_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"find_rect_filter"}, {"avformat", "avcodec"}, libs);
        buildInterDependenciesHelper({"firequalizer_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"mcdeint_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"movie_filter"}, {"avformat", "avcodec"}, libs);
        buildInterDependenciesHelper({"pan_filter"}, {"swresample"}, libs);
        buildInterDependenciesHelper({"pp_filter"}, {"postproc"}, libs);
        buildInterDependenciesHelper({"removelogo_filter"}, {"avformat", "avcodec", "swscale"}, libs);
        buildInterDependenciesHelper({"resample_filter"}, {"avresample"}, libs);
        buildInterDependenciesHelper({"sab_filter"}, {"swscale"}, libs);
        buildInterDependenciesHelper({"scale_filter"}, {"swscale"}, libs);
        buildInterDependenciesHelper({"scale2ref_filter"}, {"swscale"}, libs);
        buildInterDependenciesHelper({"sofalizer_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"showcqt_filter"}, {"avformat", "avcodec", "swscale"}, libs);
        buildInterDependenciesHelper({"showfreqs_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"showspectrum_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"signature_filter"}, {"avformat", "avcodec"}, libs);
        buildInterDependenciesHelper({"smartblur_filter"}, {"swscale"}, libs);
        buildInterDependenciesHelper({"spectrumsynth_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"spp_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"subtitles_filter"}, {"avformat", "avcodec"}, libs);
        buildInterDependenciesHelper({"uspp_filter"}, {"avcodec"}, libs);
        buildInterDependenciesHelper({"zoompan_filter"}, {"swscale"}, libs);
    } else if (m_projectName == "libavdevice") {
        buildInterDependenciesHelper({"lavfi_indev"}, {"avfilter"}, libs);
    } else if (m_projectName == "libavcodec") {
        buildInterDependenciesHelper({"opus_decoder"}, {"swresample"}, libs);
    }
}

void ProjectGenerator::buildDependencies(StaticList& libs, StaticList& addLibs)
{
    // Add any forced dependencies
    if (m_projectName == "libavformat") {
        addLibs.push_back("ws2_32");    // Add the additional required libs
    }

    // Determine only those dependencies that are valid for current project
    map<string, bool> projectDeps;
    buildProjectDependencies(projectDeps);

    // Loop through each known configuration option and add the required dependencies
    vector<string> externLibs;
    m_configHelper.getConfigList("EXTERNAL_AUTODETECT_LIBRARY_LIST", externLibs, false);
    m_configHelper.getConfigList("EXTERNAL_LIBRARY_LIST", externLibs);
    m_configHelper.getConfigList("HW_CODECS_LIST", externLibs, false);    // used on some older ffmpeg versions
    m_configHelper.getConfigList("HWACCEL_AUTODETECT_LIBRARY_LIST", externLibs, false);
    m_configHelper.getConfigList("HWACCEL_LIBRARY_LIST", externLibs, false);
    m_configHelper.getConfigList("SYSTEM_LIBRARIES", externLibs, false);
    for (const auto& i : externLibs) {
        // Check if enabled
        if (m_configHelper.getConfigOption(i)->m_value == "1") {
            // Check if this dependency is valid for this project (if the dependency is not known default to enable)
            if (projectDeps.find(i) == projectDeps.end()) {
                outputInfo("Unknown dependency found (" + i + ")");
            } else if (!projectDeps[i]) {
                // This dependency is not valid for this project so skip it
                continue;
            }

            string lib;
            if (i == "amf") {
                // doesn't need any additional libs
            } else if (i == "avisynth") {
                // doesn't need any additional libs
            } else if (i == "bcrypt") {
                addLibs.push_back("Bcrypt");    // Add the additional required libs
            } else if (i == "bzlib") {
                lib = "libbz2";
            } else if (i == "libcdio") {
                lib = "libcdio_paranoia";
            } else if (i == "libfdk_aac") {
                lib = "libfdk-aac";
            } else if (i == "libnpp") {
                addLibs.push_back("nppi");    // Add the additional required libs
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
                    libs.push_back("libcrypto");
                }
            } else if (i == "decklink") {
                // Doesn't need any additional libs
            } else if (i == "opengl") {
                addLibs.push_back("Opengl32");    // Add the additional required libs
            } else if (i == "opencl") {
                string fileName;
                if (!findFile(m_configHelper.m_rootDirectory + "compat/opencl/cl.h", fileName)) {
                    addLibs.push_back("OpenCL");    // Add the additional required libs
                }
            } else if (i == "openal") {
                addLibs.push_back("OpenAL32");    // Add the additional required libs
            } else if (i == "ffnvcodec") {
                // Doesn't require any additional libs
            } else if (i == "nvenc") {
                // Doesn't require any additional libs
            } else if (i == "cuda") {
                string fileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/cuda/dynlink_cuda.h", fileName)) {
                    addLibs.push_back("cuda");    // Add the additional required libs
                }
            } else if (i == "cuda_sdk" || i == "cuda_nvcc") {
                // Doesn't require any additional libs
            } else if (i == "cuvid") {
                string fileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/cuda/dynlink_nvcuvid.h", fileName)) {
                    addLibs.push_back("nvcuvid");    // Add the additional required libs
                }
            } else if ((i == "nvdec") || (i == "nvenc")) {
                // Doesn't need any additional libs
            } else if (i == "mediafoundation") {
                addLibs.push_back("mfplat");
                addLibs.push_back("mfuuid");
                addLibs.push_back("strmiids");
            } else if (i == "schannel") {
                addLibs.push_back("Secur32");    // Add the additional required libs
            } else if (i == "sdl") {
                if (!m_configHelper.isConfigOptionValid("sdl2")) {
                    libs.push_back("libsdl");    // Only add if not sdl2
                }
            } else if (i == "wincrypt") {
                addLibs.push_back("Advapi32");    // Add the additional required libs
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
    if (m_projectName == "libavdevice") {
        externLibs.resize(0);
        m_configHelper.getConfigList("OUTDEV_LIST", externLibs);
        m_configHelper.getConfigList("INDEV_LIST", externLibs);
        for (const auto& i : externLibs) {
            // Check if enabled
            if (m_configHelper.getConfigOption(i)->m_value == "1") {
                // Add the additional required libs
                if (i == "dshow_indev") {
                    addLibs.push_back("strmiids");
                } else if (i == "vfwcap_indev") {
                    addLibs.push_back("vfw32");
                    addLibs.push_back("shlwapi");
                } else if ((i == "wasapi_indev") || (i == "wasapi_outdev")) {
                    addLibs.push_back("ksuser");
                }
            }
        }
    }
}

void ProjectGenerator::buildDependenciesWinRT(StaticList& libs, StaticList& addLibs)
{
    // Search through dependency list and remove any not supported by WinRT
    for (auto i = libs.begin(); i < libs.end(); ++i) {
        if (*i == "libmfx") {
            libs.erase(i--);
        }
    }
    // Remove any additional windows libs
    for (auto i = addLibs.begin(); i < addLibs.end(); ++i) {
        if (*i == "cuda") {
            libs.erase(i--);
        } else if (*i == "nvcuvid") {
            libs.erase(i--);
        } else if (*i == "ws2_32") {
            libs.erase(i--);
        } else if (*i == "Bcrypt") {
            libs.erase(i--);
        } else if (*i == "Advapi32") {
            libs.erase(i--);
        } else if (*i == "strmiids") {
            libs.erase(i--);
        } else if (*i == "vfw32") {
            libs.erase(i--);
        } else if (*i == "shlwapi") {
            libs.erase(i--);
        } else if (*i == "ksuser") {
            libs.erase(i--);
        }
    }
    // Add additional windows libs
    if (m_configHelper.getConfigOption("CONFIG_D3D11VA")->m_value == "1") {
        addLibs.push_back("dxgi");
        addLibs.push_back("d3d11");
    }
}

void ProjectGenerator::buildDependencyValues(
    StaticList& includeDirs, StaticList& lib32Dirs, StaticList& lib64Dirs, StaticList& defines) const
{
    // Determine only those dependencies that are valid for current project
    map<string, bool> projectDeps;
    buildProjectDependencies(projectDeps);

    // Loop through each known configuration option and add the required dependencies
    for (const auto& i : projectDeps) {
        // Check if enabled//Check if optimised value is valid for current configuration
        if (i.second && m_configHelper.isConfigOptionEnabled(i.first)) {
            // Add in the additional include directories
            if (i.first == "libopus") {
                includeDirs.push_back("$(OutDir)/include/opus");
                includeDirs.push_back("$(ProjectDir)/../../prebuilt/include/opus");
            } else if (i.first == "libfreetype") {
                includeDirs.push_back("$(OutDir)/include/freetype2");
                includeDirs.push_back("$(ProjectDir)/../../prebuilt/include/freetype2");
            } else if (i.first == "libfribidi") {
                includeDirs.push_back("$(OutDir)/include/fribidi");
                includeDirs.push_back("$(ProjectDir)/../../prebuilt/include/fribidi");
                defines.push_back("FRIBIDI_LIB_STATIC");
            } else if (i.first == "libxml2") {
                includeDirs.push_back("$(OutDir)/include/libxml2");
                includeDirs.push_back("$(ProjectDir)/../../prebuilt/include/libxml2");
                defines.push_back("LIBXML_STATIC");
            } else if ((i.first == "sdl") && !m_configHelper.isConfigOptionValid("sdl2")) {
                includeDirs.push_back("$(OutDir)/include/SDL");
                includeDirs.push_back("$(ProjectDir)/../../prebuilt/include/SDL");
            } else if (i.first == "sdl2") {
                includeDirs.push_back("$(OutDir)/include/SDL");
                includeDirs.push_back("$(ProjectDir)/../../prebuilt/include/SDL");
            } else if (i.first == "opengl") {
                // Requires glext headers to be installed in include dir (does not require the libs)
            } else if (i.first == "opencl") {
                string fileName;
                if (!findFile(m_configHelper.m_rootDirectory + "compat/opencl/cl.h", fileName)) {
                    // Need to check for the existence of environment variables
                    if (findEnvironmentVariable("AMDAPPSDKROOT")) {
                        includeDirs.push_back("$(AMDAPPSDKROOT)/include/");
                        lib32Dirs.push_back("$(AMDAPPSDKROOT)/lib/Win32");
                        lib64Dirs.push_back("$(AMDAPPSDKROOT)/lib/x64");
                    } else if (findEnvironmentVariable("INTELOCLSDKROOT")) {
                        includeDirs.push_back("$(INTELOCLSDKROOT)/include/");
                        lib32Dirs.push_back("$(INTELOCLSDKROOT)/lib/x86");
                        lib64Dirs.push_back("$(INTELOCLSDKROOT)/lib/x64");
                    } else if (findEnvironmentVariable("CUDA_PATH")) {
                        includeDirs.push_back("$(CUDA_PATH)/include/");
                        lib32Dirs.push_back("$(CUDA_PATH)/lib/Win32");
                        lib64Dirs.push_back("$(CUDA_PATH)/lib/x64");
                    } else {
                        outputWarning("Could not find an OpenCl SDK environment variable.");
                        outputWarning(
                            "Either an OpenCL SDK is not installed or the environment variables are missing.", false);
                        outputWarning(
                            "The location of the OpenCl files will have to be manually specified as otherwise the project will not compile.",
                            false);
                    }
                }
            } else if (i.first == "openal") {
                if (!findEnvironmentVariable("OPENAL_SDK")) {
                    outputWarning("Could not find the OpenAl SDK environment variable.");
                    outputWarning(
                        "Either the OpenAL SDK is not installed or the environment variable is missing.", false);
                    outputWarning("Using the default environment variable of 'OPENAL_SDK'.", false);
                }
                includeDirs.push_back("$(OPENAL_SDK)/include/");
                lib32Dirs.push_back("$(OPENAL_SDK)/libs/Win32");
                lib64Dirs.push_back("$(OPENAL_SDK)/lib/Win64");
            } else if (i.first == "nvenc") {
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
                    // Only add if it hasn’t already been added
                    if (find(includeDirs.begin(), includeDirs.end(), "$(CUDA_PATH)/include/") == includeDirs.end()) {
                        includeDirs.push_back("$(CUDA_PATH)/include/");
                    }
                }
            } else if ((i.first == "cuda") || (i.first == "cuvid")) {
                string fileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/cuda/dynlink_cuda.h", fileName)) {
                    // Need to check for the existence of environment variables
                    if (!findEnvironmentVariable("CUDA_PATH")) {
                        outputWarning("Could not find the CUDA SDK environment variable.");
                        outputWarning(
                            "Either the CUDA SDK is not installed or the environment variable is missing.", false);
                    }
                    // Only add if it hasn’t already been added
                    if (find(includeDirs.begin(), includeDirs.end(), "$(CUDA_PATH)/include/") == includeDirs.end()) {
                        includeDirs.push_back("$(CUDA_PATH)/include/");
                        lib32Dirs.push_back("$(CUDA_PATH)/lib/Win32");
                        lib64Dirs.push_back("$(CUDA_PATH)/lib/x64");
                    }
                }
            }
        }
    }
}

void ProjectGenerator::buildProjectDependencies(map<string, bool>& projectDeps) const
{
    string notUsed;
    projectDeps["amf"] = false;         // no dependencies ever needed
    projectDeps["avisynth"] = false;    // no dependencies ever needed
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
    projectDeps["d3d11va"] = false;    // supplied by windows sdk
    projectDeps["dxva2"] = false;
    projectDeps["decklink"] = (m_projectName == "libavdevice");
    projectDeps["libfontconfig"] = (m_projectName == "libavfilter");
    projectDeps["ffnvcodec"] = false;
    projectDeps["frei0r"] = (m_projectName == "libavfilter");
    projectDeps["gcrypt"] = (m_projectName == "libavformat");
    projectDeps["gmp"] = (m_projectName == "libavformat");
    projectDeps["gnutls"] = (m_projectName == "libavformat");
    projectDeps["iconv"] = (m_projectName == "libavformat") || (m_projectName == "libavcodec");
    projectDeps["ladspa"] = (m_projectName == "libavfilter");
    projectDeps["libaacplus"] = (m_projectName == "libavcodec");
    projectDeps["libass"] = (m_projectName == "libavfilter");
    projectDeps["libbluray"] = (m_projectName == "libavformat");
    projectDeps["libbs2b"] = (m_projectName == "libavfilter");
    projectDeps["libcaca"] = (m_projectName == "libavdevice");
    projectDeps["libcdio"] = (m_projectName == "libavdevice");
    projectDeps["libcelt"] = (m_projectName == "libavcodec");
    projectDeps["libdc1394"] = (m_projectName == "libavdevice");
    projectDeps["libdcadec"] = (m_projectName == "libavcodec");
    projectDeps["libfaac"] = (m_projectName == "libavcodec");
    projectDeps["libfdk_aac"] = (m_projectName == "libavcodec");
    projectDeps["libflite"] = (m_projectName == "libavfilter");
    projectDeps["libfreetype"] = (m_projectName == "libavfilter");
    projectDeps["libfribidi"] = (m_projectName == "libavfilter");
    projectDeps["libgme"] = (m_projectName == "libavformat");
    projectDeps["libgsm"] = (m_projectName == "libavcodec");
    projectDeps["libiec61883"] = (m_projectName == "libavdevice");
    projectDeps["libilbc"] = (m_projectName == "libavcodec");
    projectDeps["libkvazaar"] = (m_projectName == "libavcodec");
    projectDeps["libmfx"] = ((m_projectName == "libavutil") && findSourceFile("hwcontext_qsv", ".h", notUsed)) ||
        (m_projectName == "libavcodec") ||
        ((m_projectName == "libavfilter") && findSourceFile("vf_deinterlace_qsv", ".c", notUsed)) ||
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
    projectDeps["libopus"] = (m_projectName == "libavcodec");
    projectDeps["libpulse"] = (m_projectName == "libavdevice");
    projectDeps["librubberband"] = (m_projectName == "libavfilter");
    projectDeps["libquvi"] = (m_projectName == "libavformat");
    projectDeps["librtmp"] = (m_projectName == "libavformat");
    projectDeps["libschroedinger"] = (m_projectName == "libavcodec");
    projectDeps["libshine"] = (m_projectName == "libavcodec");
    projectDeps["libsmbclient"] = (m_projectName == "libavformat");
    projectDeps["libsnappy"] = (m_projectName == "libavcodec");
    projectDeps["libsoxr"] = (m_projectName == "libswresample");
    projectDeps["libspeex"] = (m_projectName == "libavcodec");
    projectDeps["libssh"] = (m_projectName == "libavformat");
    projectDeps["libstagefright_h264"] = (m_projectName == "libavcodec");
    projectDeps["libtesseract"] = (m_projectName == "libavfilter");
    projectDeps["libtheora"] = (m_projectName == "libavcodec");
    projectDeps["libtwolame"] = (m_projectName == "libavcodec");
    projectDeps["libutvideo"] = (m_projectName == "libavcodec");
    projectDeps["libv4l2"] = (m_projectName == "libavdevice");
    projectDeps["libvidstab"] = (m_projectName == "libavfilter");
    projectDeps["libvo_aacenc"] = (m_projectName == "libavcodec");
    projectDeps["libvo_amrwbenc"] = (m_projectName == "libavcodec");
    projectDeps["libvorbis"] = (m_projectName == "libavcodec");
    projectDeps["libvpx"] = (m_projectName == "libavcodec");
    projectDeps["libwavpack"] = (m_projectName == "libavcodec");
    projectDeps["libwebp"] = (m_projectName == "libavcodec");
    projectDeps["libx264"] = (m_projectName == "libavcodec");
    projectDeps["libx265"] = (m_projectName == "libavcodec");
    projectDeps["libxavs"] = (m_projectName == "libavcodec");
    projectDeps["libxml2"] = (m_projectName == "libavformat");
    projectDeps["libxvid"] = (m_projectName == "libavcodec");
    projectDeps["libzimg"] = (m_projectName == "libavfilter");
    projectDeps["libzmq"] = (m_projectName == "libavfilter");
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
    projectDeps["openssl"] = (m_projectName == "libavformat");
    projectDeps["schannel"] = (m_projectName == "libavformat");
    projectDeps["sdl"] = (m_projectName == "libavdevice") || (m_projectName == "ffplay") || (m_projectName == "avplay");
    projectDeps["sdl2"] =
        (m_projectName == "libavdevice") || (m_projectName == "ffplay") || (m_projectName == "avplay");
    // projectDeps["x11grab"] = ( m_projectName.compare("libavdevice") == 0 );//Always disabled on Win32
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