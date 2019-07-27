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
    const StaticList& vConfigOptions, const StaticList& vAddDeps, StaticList& vLibs)
{
    bool bFound = false;
    for (auto itI = vConfigOptions.begin(); itI < vConfigOptions.end(); ++itI) {
        bFound = m_configHelper.isConfigOptionEnabled(*itI);
        if (!bFound) {
            break;
        }
    }
    if (bFound) {
        for (auto itI = vAddDeps.begin(); itI < vAddDeps.end(); ++itI) {
            string sSearchTag = "lib" + *itI;
            if (find(vLibs.begin(), vLibs.end(), sSearchTag) == vLibs.end()) {
                vLibs.push_back(sSearchTag);
            }
        }
    }
}

void ProjectGenerator::buildInterDependencies(StaticList& vLibs)
{
    // Get the lib dependencies from the configure file
    string sLibName = m_projectName.substr(3) + "_deps";
    vector<string> vLibDeps;
    if (m_configHelper.getConfigList(sLibName, vLibDeps, false)) {
        for (auto itI = vLibDeps.begin(); itI < vLibDeps.end(); ++itI) {
            string sSearchTag = "lib" + *itI;
            if (find(vLibs.begin(), vLibs.end(), sSearchTag) == vLibs.end()) {
                vLibs.push_back(sSearchTag);
            }
        }
    }

    // Hard coded configuration checks for inter dependencies between different source libs.
    if (m_projectName == "libavfilter") {
        buildInterDependenciesHelper({"afftfilt_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"afir_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"amovie_filter"}, {"avformat", "avcodec"}, vLibs);
        buildInterDependenciesHelper({"aresample_filter"}, {"swresample"}, vLibs);
        buildInterDependenciesHelper({"asyncts_filter"}, {"avresample"}, vLibs);
        buildInterDependenciesHelper({"atempo_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"cover_rect_filter"}, {"avformat", "avcodec"}, vLibs);
        buildInterDependenciesHelper({"ebur128_filter", "swresample"}, {"swresample"}, vLibs);
        buildInterDependenciesHelper({"elbg_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"fftfilt_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"find_rect_filter"}, {"avformat", "avcodec"}, vLibs);
        buildInterDependenciesHelper({"firequalizer_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"mcdeint_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"movie_filter"}, {"avformat", "avcodec"}, vLibs);
        buildInterDependenciesHelper({"pan_filter"}, {"swresample"}, vLibs);
        buildInterDependenciesHelper({"pp_filter"}, {"postproc"}, vLibs);
        buildInterDependenciesHelper({"removelogo_filter"}, {"avformat", "avcodec", "swscale"}, vLibs);
        buildInterDependenciesHelper({"resample_filter"}, {"avresample"}, vLibs);
        buildInterDependenciesHelper({"sab_filter"}, {"swscale"}, vLibs);
        buildInterDependenciesHelper({"scale_filter"}, {"swscale"}, vLibs);
        buildInterDependenciesHelper({"scale2ref_filter"}, {"swscale"}, vLibs);
        buildInterDependenciesHelper({"sofalizer_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"showcqt_filter"}, {"avformat", "avcodec", "swscale"}, vLibs);
        buildInterDependenciesHelper({"showfreqs_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"showspectrum_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"signature_filter"}, {"avformat", "avcodec"}, vLibs);
        buildInterDependenciesHelper({"smartblur_filter"}, {"swscale"}, vLibs);
        buildInterDependenciesHelper({"spectrumsynth_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"spp_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"subtitles_filter"}, {"avformat", "avcodec"}, vLibs);
        buildInterDependenciesHelper({"uspp_filter"}, {"avcodec"}, vLibs);
        buildInterDependenciesHelper({"zoompan_filter"}, {"swscale"}, vLibs);
    } else if (m_projectName == "libavdevice") {
        buildInterDependenciesHelper({"lavfi_indev"}, {"avfilter"}, vLibs);
    } else if (m_projectName == "libavcodec") {
        buildInterDependenciesHelper({"opus_decoder"}, {"swresample"}, vLibs);
    }
}

void ProjectGenerator::buildDependencies(StaticList& vLibs, StaticList& vAddLibs)
{
    // Add any forced dependencies
    if (m_projectName == "libavformat") {
        vAddLibs.push_back("ws2_32");    // Add the additional required libs
    }

    // Determine only those dependencies that are valid for current project
    map<string, bool> mProjectDeps;
    buildProjectDependencies(mProjectDeps);

    // Loop through each known configuration option and add the required dependencies
    vector<string> vExternLibs;
    m_configHelper.getConfigList("EXTERNAL_AUTODETECT_LIBRARY_LIST", vExternLibs, false);
    m_configHelper.getConfigList("EXTERNAL_LIBRARY_LIST", vExternLibs);
    m_configHelper.getConfigList("HW_CODECS_LIST", vExternLibs, false);    // used on some older ffmpeg versions
    m_configHelper.getConfigList("HWACCEL_AUTODETECT_LIBRARY_LIST", vExternLibs, false);
    m_configHelper.getConfigList("HWACCEL_LIBRARY_LIST", vExternLibs, false);
    m_configHelper.getConfigList("SYSTEM_LIBRARIES", vExternLibs, false);
    for (auto vitLib = vExternLibs.begin(); vitLib < vExternLibs.end(); ++vitLib) {
        // Check if enabled
        if (m_configHelper.getConfigOption(*vitLib)->m_value == "1") {
            // Check if this dependency is valid for this project (if the dependency is not known default to enable)
            if (mProjectDeps.find(*vitLib) == mProjectDeps.end()) {
                outputInfo("Unknown dependency found (" + *vitLib + ")");
            } else if (!mProjectDeps[*vitLib]) {
                // This dependency is not valid for this project so skip it
                continue;
            }

            string sLib;
            if (*vitLib == "amf") {
                // doesn't need any additional libs
            } else if (*vitLib == "avisynth") {
                // doesn't need any additional libs
            } else if (*vitLib == "bcrypt") {
                vAddLibs.push_back("Bcrypt");    // Add the additional required libs
            } else if (*vitLib == "bzlib") {
                sLib = "libbz2";
            } else if (*vitLib == "libcdio") {
                sLib = "libcdio_paranoia";
            } else if (*vitLib == "libfdk_aac") {
                sLib = "libfdk-aac";
            } else if (*vitLib == "libnpp") {
                vAddLibs.push_back("nppi");    // Add the additional required libs
                // CUDA 7.5 onwards only provides npp for x64
            } else if (*vitLib == "libxvid") {
                sLib = "libxvidcore";
            } else if (*vitLib == "openssl") {
                // Needs ws2_32 but libavformat needs this even if not using openssl so it is already included
                sLib = "libssl";
                // Also need crypto
                auto vitList = vLibs.begin();
                for (vitList; vitList < vLibs.end(); ++vitList) {
                    if (*vitList == "libcrypto") {
                        break;
                    }
                }
                if (vitList == vLibs.end()) {
                    vLibs.push_back("libcrypto");
                }
            } else if (*vitLib == "decklink") {
                // Doesn't need any additional libs
            } else if (*vitLib == "opengl") {
                vAddLibs.push_back("Opengl32");    // Add the additional required libs
            } else if (*vitLib == "opencl") {
                string sFileName;
                if (!findFile(m_configHelper.m_rootDirectory + "compat/opencl/cl.h", sFileName)) {
                    vAddLibs.push_back("OpenCL");    // Add the additional required libs
                }
            } else if (*vitLib == "openal") {
                vAddLibs.push_back("OpenAL32");    // Add the additional required libs
            } else if (*vitLib == "ffnvcodec") {
                // Doesn't require any additional libs
            } else if (*vitLib == "nvenc") {
                // Doesn't require any additional libs
            } else if (*vitLib == "cuda") {
                string sFileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/cuda/dynlink_cuda.h", sFileName)) {
                    vAddLibs.push_back("cuda");    // Add the additional required libs
                }
            } else if (*vitLib == "cuda_sdk" || *vitLib == "cuda_nvcc") {
                // Doesn't require any additional libs
            } else if (*vitLib == "cuvid") {
                string sFileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/cuda/dynlink_nvcuvid.h", sFileName)) {
                    vAddLibs.push_back("nvcuvid");    // Add the additional required libs
                }
            } else if ((*vitLib == "nvdec") || (*vitLib == "nvenc")) {
                // Doesn't need any additional libs
            } else if (*vitLib == "schannel") {
                vAddLibs.push_back("Secur32");    // Add the additional required libs
            } else if (*vitLib == "sdl") {
                if (!m_configHelper.isConfigOptionValid("sdl2")) {
                    vLibs.push_back("libsdl");    // Only add if not sdl2
                }
            } else if (*vitLib == "wincrypt") {
                vAddLibs.push_back("Advapi32");    // Add the additional required libs
            } else {
                // By default just use the lib name and prefix with lib if not already
                if (vitLib->find("lib") == 0) {
                    sLib = *vitLib;
                } else {
                    sLib = "lib" + *vitLib;
                }
            }
            if (sLib.length() > 0) {
                // Check already not in list
                auto vitList = vLibs.begin();
                for (vitList; vitList < vLibs.end(); ++vitList) {
                    if (*vitList == sLib) {
                        break;
                    }
                }
                if (vitList == vLibs.end()) {
                    vLibs.push_back(sLib);
                }
            }
        }
    }

    // Add in extralibs used for various devices
    if (m_projectName == "libavdevice") {
        vExternLibs.resize(0);
        m_configHelper.getConfigList("OUTDEV_LIST", vExternLibs);
        m_configHelper.getConfigList("INDEV_LIST", vExternLibs);
        for (auto vitLib = vExternLibs.begin(); vitLib < vExternLibs.end(); ++vitLib) {
            // Check if enabled
            if (m_configHelper.getConfigOption(*vitLib)->m_value == "1") {
                // Add the additional required libs
                if (*vitLib == "dshow_indev") {
                    vAddLibs.push_back("strmiids");
                } else if (*vitLib == "vfwcap_indev") {
                    vAddLibs.push_back("vfw32");
                    vAddLibs.push_back("shlwapi");
                } else if ((*vitLib == "wasapi_indev") || (*vitLib == "wasapi_outdev")) {
                    vAddLibs.push_back("ksuser");
                }
            }
        }
    }
}

void ProjectGenerator::buildDependenciesWinRT(StaticList& vLibs, StaticList& vAddLibs)
{
    // Search through dependency list and remove any not supported by WinRT
    for (auto vitLib = vLibs.begin(); vitLib < vLibs.end(); ++vitLib) {
        if (*vitLib == "libmfx") {
            vLibs.erase(vitLib--);
        }
    }
    // Remove any additional windows libs
    for (auto vitLib = vAddLibs.begin(); vitLib < vAddLibs.end(); ++vitLib) {
        if (*vitLib == "cuda") {
            vLibs.erase(vitLib--);
        } else if (*vitLib == "nvcuvid") {
            vLibs.erase(vitLib--);
        } else if (*vitLib == "ws2_32") {
            vLibs.erase(vitLib--);
        } else if (*vitLib == "Bcrypt") {
            vLibs.erase(vitLib--);
        } else if (*vitLib == "Advapi32") {
            vLibs.erase(vitLib--);
        } else if (*vitLib == "strmiids") {
            vLibs.erase(vitLib--);
        } else if (*vitLib == "vfw32") {
            vLibs.erase(vitLib--);
        } else if (*vitLib == "shlwapi") {
            vLibs.erase(vitLib--);
        } else if (*vitLib == "ksuser") {
            vLibs.erase(vitLib--);
        }
    }
    // Add additional windows libs
    if (m_configHelper.getConfigOption("CONFIG_D3D11VA")->m_value == "1") {
        vAddLibs.push_back("dxgi");
        vAddLibs.push_back("d3d11");
    }
}

void ProjectGenerator::buildDependencyValues(
    StaticList& vIncludeDirs, StaticList& vLib32Dirs, StaticList& vLib64Dirs, StaticList& vDefines)
{
    // Determine only those dependencies that are valid for current project
    map<string, bool> mProjectDeps;
    buildProjectDependencies(mProjectDeps);

    // Loop through each known configuration option and add the required dependencies
    for (map<string, bool>::iterator mitLib = mProjectDeps.begin(); mitLib != mProjectDeps.end(); ++mitLib) {
        // Check if enabled//Check if optimised value is valid for current configuration
        if (mitLib->second && m_configHelper.isConfigOptionEnabled(mitLib->first)) {
            // Add in the additional include directories
            if (mitLib->first == "libopus") {
                vIncludeDirs.push_back("$(OutDir)/include/opus");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/opus");
            } else if (mitLib->first == "libfreetype") {
                vIncludeDirs.push_back("$(OutDir)/include/freetype2");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/freetype2");
            } else if (mitLib->first == "libfribidi") {
                vIncludeDirs.push_back("$(OutDir)/include/fribidi");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/fribidi");
            } else if (mitLib->first == "libxml2") {
                vIncludeDirs.push_back("$(OutDir)/include/libxml2");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/libxml2");
                vDefines.push_back("LIBXML_STATIC");
            } else if ((mitLib->first == "sdl") && !m_configHelper.isConfigOptionValid("sdl2")) {
                vIncludeDirs.push_back("$(OutDir)/include/SDL");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/SDL");
            } else if (mitLib->first == "sdl2") {
                vIncludeDirs.push_back("$(OutDir)/include/SDL");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/SDL");
            } else if (mitLib->first == "opengl") {
                // Requires glext headers to be installed in include dir (does not require the libs)
            } else if (mitLib->first == "opencl") {
                string sFileName;
                if (!findFile(m_configHelper.m_rootDirectory + "compat/opencl/cl.h", sFileName)) {
                    // Need to check for the existence of environment variables
                    if (findEnvironmentVariable("AMDAPPSDKROOT")) {
                        vIncludeDirs.push_back("$(AMDAPPSDKROOT)/include/");
                        vLib32Dirs.push_back("$(AMDAPPSDKROOT)/lib/Win32");
                        vLib64Dirs.push_back("$(AMDAPPSDKROOT)/lib/x64");
                    } else if (findEnvironmentVariable("INTELOCLSDKROOT")) {
                        vIncludeDirs.push_back("$(INTELOCLSDKROOT)/include/");
                        vLib32Dirs.push_back("$(INTELOCLSDKROOT)/lib/x86");
                        vLib64Dirs.push_back("$(INTELOCLSDKROOT)/lib/x64");
                    } else if (findEnvironmentVariable("CUDA_PATH")) {
                        vIncludeDirs.push_back("$(CUDA_PATH)/include/");
                        vLib32Dirs.push_back("$(CUDA_PATH)/lib/Win32");
                        vLib64Dirs.push_back("$(CUDA_PATH)/lib/x64");
                    } else {
                        outputWarning("Could not find an OpenCl SDK environment variable.");
                        outputWarning(
                            "Either an OpenCL SDK is not installed or the environment variables are missing.", false);
                        outputWarning(
                            "The location of the OpenCl files will have to be manually specified as otherwise the project will not compile.",
                            false);
                    }
                }
            } else if (mitLib->first == "openal") {
                if (!findEnvironmentVariable("OPENAL_SDK")) {
                    outputWarning("Could not find the OpenAl SDK environment variable.");
                    outputWarning(
                        "Either the OpenAL SDK is not installed or the environment variable is missing.", false);
                    outputWarning("Using the default environment variable of 'OPENAL_SDK'.", false);
                }
                vIncludeDirs.push_back("$(OPENAL_SDK)/include/");
                vLib32Dirs.push_back("$(OPENAL_SDK)/libs/Win32");
                vLib64Dirs.push_back("$(OPENAL_SDK)/lib/Win64");
            } else if (mitLib->first == "nvenc") {
                string sFileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/nvenc/nvEncodeAPI.h", sFileName)) {
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
                    if (find(vIncludeDirs.begin(), vIncludeDirs.end(), "$(CUDA_PATH)/include/") == vIncludeDirs.end()) {
                        vIncludeDirs.push_back("$(CUDA_PATH)/include/");
                    }
                }
            } else if ((mitLib->first == "cuda") || (mitLib->first == "cuvid")) {
                string sFileName;
                if (!m_configHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_configHelper.m_rootDirectory + "compat/cuda/dynlink_cuda.h", sFileName)) {
                    // Need to check for the existence of environment variables
                    if (!findEnvironmentVariable("CUDA_PATH")) {
                        outputWarning("Could not find the CUDA SDK environment variable.");
                        outputWarning(
                            "Either the CUDA SDK is not installed or the environment variable is missing.", false);
                    }
                    // Only add if it hasn’t already been added
                    if (find(vIncludeDirs.begin(), vIncludeDirs.end(), "$(CUDA_PATH)/include/") == vIncludeDirs.end()) {
                        vIncludeDirs.push_back("$(CUDA_PATH)/include/");
                        vLib32Dirs.push_back("$(CUDA_PATH)/lib/Win32");
                        vLib64Dirs.push_back("$(CUDA_PATH)/lib/x64");
                    }
                }
            }
        }
    }
}

void ProjectGenerator::buildProjectDependencies(map<string, bool>& mProjectDeps)
{
    string sNotUsed;
    mProjectDeps["amf"] = false;         // no dependencies ever needed
    mProjectDeps["avisynth"] = false;    // no dependencies ever needed
    mProjectDeps["bcrypt"] = (m_projectName == "libavutil");
    mProjectDeps["bzlib"] = (m_projectName == "libavformat") || (m_projectName == "libavcodec");
    mProjectDeps["crystalhd"] = (m_projectName == "libavcodec");
    mProjectDeps["chromaprint"] = (m_projectName == "libavformat");
    mProjectDeps["cuda"] = ((m_projectName == "libavutil") && findSourceFile("hwcontext_cuda", ".h", sNotUsed)) ||
        (m_projectName == "libavfilter") ||
        (m_configHelper.isConfigOptionEnabled("nvenc") && (m_projectName == "libavcodec")) ||
        (m_configHelper.isConfigOptionEnabled("cuvid") &&
            ((m_projectName == "libavcodec") || (m_projectName == "ffmpeg") || (m_projectName == "avconv")));
    mProjectDeps["cuvid"] =
        (m_projectName == "libavcodec") || (m_projectName == "ffmpeg") || (m_projectName == "avconv");
    mProjectDeps["d3d11va"] = false;    // supplied by windows sdk
    mProjectDeps["dxva2"] = false;
    mProjectDeps["decklink"] = (m_projectName == "libavdevice");
    mProjectDeps["libfontconfig"] = (m_projectName == "libavfilter");
    mProjectDeps["ffnvcodec"] = false;
    mProjectDeps["frei0r"] = (m_projectName == "libavfilter");
    mProjectDeps["gcrypt"] = (m_projectName == "libavformat");
    mProjectDeps["gmp"] = (m_projectName == "libavformat");
    mProjectDeps["gnutls"] = (m_projectName == "libavformat");
    mProjectDeps["iconv"] = (m_projectName == "libavformat") || (m_projectName == "libavcodec");
    mProjectDeps["ladspa"] = (m_projectName == "libavfilter");
    mProjectDeps["libaacplus"] = (m_projectName == "libavcodec");
    mProjectDeps["libass"] = (m_projectName == "libavfilter");
    mProjectDeps["libbluray"] = (m_projectName == "libavformat");
    mProjectDeps["libbs2b"] = (m_projectName == "libavfilter");
    mProjectDeps["libcaca"] = (m_projectName == "libavdevice");
    mProjectDeps["libcdio"] = (m_projectName == "libavdevice");
    mProjectDeps["libcelt"] = (m_projectName == "libavcodec");
    mProjectDeps["libdc1394"] = (m_projectName == "libavdevice");
    mProjectDeps["libdcadec"] = (m_projectName == "libavcodec");
    mProjectDeps["libfaac"] = (m_projectName == "libavcodec");
    mProjectDeps["libfdk_aac"] = (m_projectName == "libavcodec");
    mProjectDeps["libflite"] = (m_projectName == "libavfilter");
    mProjectDeps["libfreetype"] = (m_projectName == "libavfilter");
    mProjectDeps["libfribidi"] = (m_projectName == "libavfilter");
    mProjectDeps["libgme"] = (m_projectName == "libavformat");
    mProjectDeps["libgsm"] = (m_projectName == "libavcodec");
    mProjectDeps["libiec61883"] = (m_projectName == "libavdevice");
    mProjectDeps["libilbc"] = (m_projectName == "libavcodec");
    mProjectDeps["libkvazaar"] = (m_projectName == "libavcodec");
    mProjectDeps["libmfx"] = ((m_projectName == "libavutil") && findSourceFile("hwcontext_qsv", ".h", sNotUsed)) ||
        (m_projectName == "libavcodec") ||
        ((m_projectName == "libavfilter") && findSourceFile("vf_deinterlace_qsv", ".c", sNotUsed)) ||
        (m_projectName == "ffmpeg") || (m_projectName == "avconv");
    mProjectDeps["libmodplug"] = (m_projectName == "libavformat");
    mProjectDeps["libmp3lame"] = (m_projectName == "libavcodec");
    mProjectDeps["libnpp"] = (m_projectName == "libavfilter");
    mProjectDeps["libnut"] = (m_projectName == "libformat");
    mProjectDeps["libopencore_amrnb"] = (m_projectName == "libavcodec");
    mProjectDeps["libopencore_amrwb"] = (m_projectName == "libavcodec");
    mProjectDeps["libopencv"] = (m_projectName == "libavfilter");
    mProjectDeps["libopenjpeg"] = (m_projectName == "libavcodec");
    mProjectDeps["libopenh264"] = (m_projectName == "libavcodec");
    mProjectDeps["libopus"] = (m_projectName == "libavcodec");
    mProjectDeps["libpulse"] = (m_projectName == "libavdevice");
    mProjectDeps["librubberband"] = (m_projectName == "libavfilter");
    mProjectDeps["libquvi"] = (m_projectName == "libavformat");
    mProjectDeps["librtmp"] = (m_projectName == "libavformat");
    mProjectDeps["libschroedinger"] = (m_projectName == "libavcodec");
    mProjectDeps["libshine"] = (m_projectName == "libavcodec");
    mProjectDeps["libsmbclient"] = (m_projectName == "libavformat");
    mProjectDeps["libsnappy"] = (m_projectName == "libavcodec");
    mProjectDeps["libsoxr"] = (m_projectName == "libswresample");
    mProjectDeps["libspeex"] = (m_projectName == "libavcodec");
    mProjectDeps["libssh"] = (m_projectName == "libavformat");
    mProjectDeps["libstagefright_h264"] = (m_projectName == "libavcodec");
    mProjectDeps["libtesseract"] = (m_projectName == "libavfilter");
    mProjectDeps["libtheora"] = (m_projectName == "libavcodec");
    mProjectDeps["libtwolame"] = (m_projectName == "libavcodec");
    mProjectDeps["libutvideo"] = (m_projectName == "libavcodec");
    mProjectDeps["libv4l2"] = (m_projectName == "libavdevice");
    mProjectDeps["libvidstab"] = (m_projectName == "libavfilter");
    mProjectDeps["libvo_aacenc"] = (m_projectName == "libavcodec");
    mProjectDeps["libvo_amrwbenc"] = (m_projectName == "libavcodec");
    mProjectDeps["libvorbis"] = (m_projectName == "libavcodec");
    mProjectDeps["libvpx"] = (m_projectName == "libavcodec");
    mProjectDeps["libwavpack"] = (m_projectName == "libavcodec");
    mProjectDeps["libwebp"] = (m_projectName == "libavcodec");
    mProjectDeps["libx264"] = (m_projectName == "libavcodec");
    mProjectDeps["libx265"] = (m_projectName == "libavcodec");
    mProjectDeps["libxavs"] = (m_projectName == "libavcodec");
    mProjectDeps["libxml2"] = (m_projectName == "libavformat");
    mProjectDeps["libxvid"] = (m_projectName == "libavcodec");
    mProjectDeps["libzimg"] = (m_projectName == "libavfilter");
    mProjectDeps["libzmq"] = (m_projectName == "libavfilter");
    mProjectDeps["libzvbi"] = (m_projectName == "libavcodec");
    mProjectDeps["lzma"] = (m_projectName == "libavcodec");
    mProjectDeps["nvdec"] = (m_projectName == "libavcodec");
    mProjectDeps["nvenc"] = (m_projectName == "libavcodec");
    mProjectDeps["openal"] = (m_projectName == "libavdevice");
    mProjectDeps["opencl"] = (m_projectName == "libavutil") || (m_projectName == "libavfilter") ||
        (m_projectName == "ffmpeg") || (m_projectName == "avconv") || (m_projectName == "ffplay") ||
        (m_projectName == "avplay") || (m_projectName == "ffprobe") || (m_projectName == "avprobe");
    mProjectDeps["opengl"] = (m_projectName == "libavdevice");
    mProjectDeps["openssl"] = (m_projectName == "libavformat");
    mProjectDeps["schannel"] = (m_projectName == "libavformat");
    mProjectDeps["sdl"] =
        (m_projectName == "libavdevice") || (m_projectName == "ffplay") || (m_projectName == "avplay");
    mProjectDeps["sdl2"] =
        (m_projectName == "libavdevice") || (m_projectName == "ffplay") || (m_projectName == "avplay");
    // mProjectDeps["x11grab"] = ( m_projectName.compare("libavdevice") == 0 );//Always disabled on Win32
    mProjectDeps["zlib"] = (m_projectName == "libavformat") || (m_projectName == "libavcodec");
}

void ProjectGenerator::buildProjectGUIDs(map<string, string>& mKeys) const
{
    mKeys["libavcodec"] = "B4824EFF-C340-425D-A4A8-E2E02A71A7AE";
    mKeys["libavdevice"] = "6E165FA4-44EB-4330-8394-9F0D76D8E03E";
    mKeys["libavfilter"] = "BC2E1028-66CD-41A0-AF90-EEBD8CC52787";
    mKeys["libavformat"] = "30A96E9B-8061-4F19-BD71-FDE7EA8F7929";
    mKeys["libavresample"] = "0096CB8C-3B04-462B-BF4F-0A9970A57C91";
    mKeys["libavutil"] = "CE6C44DD-6E38-4293-8AB3-04EE28CCA972";
    mKeys["libswresample"] = "3CE4A9EF-98B6-4454-B76E-3AD9C03A2114";
    mKeys["libswscale"] = "6D8A6330-8EBE-49FD-9281-0A396F9F28F2";
    mKeys["libpostproc"] = "4D9C457D-9ADA-4A12-9D06-42D80124C5AB";

    if (!m_configHelper.m_isLibav) {
        mKeys["ffmpeg"] = "4081C77E-F1F7-49FA-9BD8-A4D267C83716";
        mKeys["ffplay"] = "E2A6865D-BD68-45B4-8130-EFD620F2C7EB";
        mKeys["ffprobe"] = "147A422A-FA63-4724-A5D9-08B1CAFDAB59";
    } else {
        mKeys["avconv"] = "4081C77E-F1F7-49FA-9BD8-A4D267C83716";
        mKeys["avplay"] = "E2A6865D-BD68-45B4-8130-EFD620F2C7EB";
        mKeys["avprobe"] = "147A422A-FA63-4724-A5D9-08B1CAFDAB59";
    }
}

void ProjectGenerator::buildProjectDCEs(
    map<string, DCEParams>& /*mDCEDefinitions*/, map<string, DCEParams>& mDCEVariables) const
{
    // TODO: Detect these automatically
    // Next we need to check for all the configurations that are project specific
    struct FindThingsVars
    {
        string sList;
        string sSearch;
        string sFile;
        string sHeader;
    };
    vector<FindThingsVars> vSearchLists;
    if (m_projectName == "libavcodec") {
        vSearchLists.push_back({"encoder", "ENC", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        vSearchLists.push_back({"decoder", "DEC", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        vSearchLists.push_back({"hwaccel", "HWACCEL", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        vSearchLists.push_back({"parser", "PARSER", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
    } else if (m_projectName == "libavformat") {
        vSearchLists.push_back({"muxer", "_MUX", "libavformat/allformats.c", "libavformat/avformat.h"});
        vSearchLists.push_back({"demuxer", "DEMUX", "libavformat/allformats.c", "libavformat/avformat.h"});
    } else if (m_projectName == "libavfilter") {
        vSearchLists.push_back({"filter", "FILTER", "libavfilter/allfilters.c", "libavfilter/avfilter.h"});
    } else if (m_projectName == "libavdevice") {
        vSearchLists.push_back({"outdev", "OUTDEV", "libavdevice/alldevices.c", "libavdevice/avdevice.h"});
        vSearchLists.push_back({"indev", "_IN", "libavdevice/alldevices.c", "libavdevice/avdevice.h"});
    }

    for (auto itLists = vSearchLists.begin(); itLists < vSearchLists.end(); ++itLists) {
        // We need to get the values from each config list
        StaticList vRet;
        StaticList vRetExterns;
        string sList = (*itLists).sList;
        transform(sList.begin(), sList.end(), sList.begin(), toupper);
        m_configHelper.passFindThings(itLists->sList, itLists->sSearch, itLists->sFile, vRet, &vRetExterns);
        for (auto itRet = vRet.begin(), itRet2 = vRetExterns.begin(); itRet < vRet.end(); ++itRet, ++itRet2) {
            string sType = *itRet2;
            transform(itRet->begin(), itRet->end(), itRet->begin(), toupper);
            mDCEVariables[sType] = {"CONFIG_" + *itRet, itLists->sHeader};
        }
    }
}