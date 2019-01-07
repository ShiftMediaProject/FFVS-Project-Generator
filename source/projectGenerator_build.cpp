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
        bFound = m_ConfigHelper.isConfigOptionEnabled(*itI);
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
    string sLibName = m_sProjectName.substr(3) + "_deps";
    vector<string> vLibDeps;
    if (m_ConfigHelper.getConfigList(sLibName, vLibDeps, false)) {
        for (auto itI = vLibDeps.begin(); itI < vLibDeps.end(); ++itI) {
            string sSearchTag = "lib" + *itI;
            if (find(vLibs.begin(), vLibs.end(), sSearchTag) == vLibs.end()) {
                vLibs.push_back(sSearchTag);
            }
        }
    }

    // Hard coded configuration checks for inter dependencies between different source libs.
    if (m_sProjectName == "libavfilter") {
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
    } else if (m_sProjectName == "libavdevice") {
        buildInterDependenciesHelper({"lavfi_indev"}, {"avfilter"}, vLibs);
    } else if (m_sProjectName == "libavcodec") {
        buildInterDependenciesHelper({"opus_decoder"}, {"swresample"}, vLibs);
    }
}

void ProjectGenerator::buildDependencies(StaticList& vLibs, StaticList& vAddLibs)
{
    // Add any forced dependencies
    if (m_sProjectName == "libavformat") {
        vAddLibs.push_back("ws2_32");    // Add the additional required libs
    }

    // Determine only those dependencies that are valid for current project
    map<string, bool> mProjectDeps;
    buildProjectDependencies(mProjectDeps);

    // Loop through each known configuration option and add the required dependencies
    vector<string> vExternLibs;
    m_ConfigHelper.getConfigList("EXTERNAL_AUTODETECT_LIBRARY_LIST", vExternLibs, false);
    m_ConfigHelper.getConfigList("EXTERNAL_LIBRARY_LIST", vExternLibs);
    m_ConfigHelper.getConfigList("HW_CODECS_LIST", vExternLibs, false);    // used on some older ffmpeg versions
    m_ConfigHelper.getConfigList("HWACCEL_AUTODETECT_LIBRARY_LIST", vExternLibs, false);
    m_ConfigHelper.getConfigList("HWACCEL_LIBRARY_LIST", vExternLibs, false);
    m_ConfigHelper.getConfigList("SYSTEM_LIBRARIES", vExternLibs, false);
    for (auto vitLib = vExternLibs.begin(); vitLib < vExternLibs.end(); ++vitLib) {
        // Check if enabled
        if (m_ConfigHelper.getConfigOption(*vitLib)->m_value == "1") {
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
                if (!findFile(m_ConfigHelper.m_rootDirectory + "compat/opencl/cl.h", sFileName)) {
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
                if (!m_ConfigHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_ConfigHelper.m_rootDirectory + "compat/cuda/dynlink_cuda.h", sFileName)) {
                    vAddLibs.push_back("cuda");    // Add the additional required libs
                }
            } else if (*vitLib == "cuvid") {
                string sFileName;
                if (!m_ConfigHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_ConfigHelper.m_rootDirectory + "compat/cuda/dynlink_nvcuvid.h", sFileName)) {
                    vAddLibs.push_back("nvcuvid");    // Add the additional required libs
                }
            } else if ((*vitLib == "nvdec") || (*vitLib == "nvenc")) {
                // Doesn't need any additional libs
            } else if (*vitLib == "schannel") {
                vAddLibs.push_back("Secur32");    // Add the additional required libs
            } else if (*vitLib == "sdl") {
                if (!m_ConfigHelper.isConfigOptionValid("sdl2")) {
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
    if (m_sProjectName == "libavdevice") {
        vExternLibs.resize(0);
        m_ConfigHelper.getConfigList("OUTDEV_LIST", vExternLibs);
        m_ConfigHelper.getConfigList("INDEV_LIST", vExternLibs);
        for (auto vitLib = vExternLibs.begin(); vitLib < vExternLibs.end(); ++vitLib) {
            // Check if enabled
            if (m_ConfigHelper.getConfigOption(*vitLib)->m_value == "1") {
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
            --vitLib;
            vLibs.erase(vitLib);
        }
    }
    // Remove any additional windows libs
    for (auto vitLib = vAddLibs.begin(); vitLib < vAddLibs.end(); ++vitLib) {
        if (*vitLib == "cuda") {
            --vitLib;
            vLibs.erase(vitLib);
        } else if (*vitLib == "nvcuvid") {
            --vitLib;
            vLibs.erase(vitLib);
        } else if (*vitLib == "ws2_32") {
            --vitLib;
            vLibs.erase(vitLib);
        } else if (*vitLib == "Bcrypt") {
            --vitLib;
            vLibs.erase(vitLib);
        } else if (*vitLib == "Advapi32") {
            --vitLib;
            vLibs.erase(vitLib);
        } else if (*vitLib == "strmiids") {
            --vitLib;
            vLibs.erase(vitLib);
        } else if (*vitLib == "vfw32") {
            --vitLib;
            vLibs.erase(vitLib);
        } else if (*vitLib == "shlwapi") {
            --vitLib;
            vLibs.erase(vitLib);
        } else if (*vitLib == "ksuser") {
            --vitLib;
            vLibs.erase(vitLib);
        }
    }
    // Add additional windows libs
    if (m_ConfigHelper.getConfigOption("CONFIG_D3D11VA")->m_value == "1") {
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
        if (mitLib->second && m_ConfigHelper.isConfigOptionEnabled(mitLib->first)) {
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
            } else if ((mitLib->first == "sdl") && !m_ConfigHelper.isConfigOptionValid("sdl2")) {
                vIncludeDirs.push_back("$(OutDir)/include/SDL");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/SDL");
            } else if (mitLib->first == "sdl2") {
                vIncludeDirs.push_back("$(OutDir)/include/SDL");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/SDL");
            } else if (mitLib->first == "opengl") {
                // Requires glext headers to be installed in include dir (does not require the libs)
            } else if (mitLib->first == "opencl") {
                string sFileName;
                if (!findFile(m_ConfigHelper.m_rootDirectory + "compat/opencl/cl.h", sFileName)) {
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
                if (!m_ConfigHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_ConfigHelper.m_rootDirectory + "compat/nvenc/nvEncodeAPI.h", sFileName)) {
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
                if (!m_ConfigHelper.isConfigOptionEnabled("ffnvcodec") &&
                    !findFile(m_ConfigHelper.m_rootDirectory + "compat/cuda/dynlink_cuda.h", sFileName)) {
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
    mProjectDeps["bcrypt"] = (m_sProjectName == "libavutil");
    mProjectDeps["bzlib"] = (m_sProjectName == "libavformat") || (m_sProjectName == "libavcodec");
    mProjectDeps["crystalhd"] = (m_sProjectName == "libavcodec");
    mProjectDeps["chromaprint"] = (m_sProjectName == "libavformat");
    mProjectDeps["cuda"] = ((m_sProjectName == "libavutil") && findSourceFile("hwcontext_cuda", ".h", sNotUsed)) ||
        (m_sProjectName == "libavfilter") ||
        (m_ConfigHelper.isConfigOptionEnabled("nvenc") && (m_sProjectName == "libavcodec")) ||
        (m_ConfigHelper.isConfigOptionEnabled("cuvid") &&
            ((m_sProjectName == "libavcodec") || (m_sProjectName == "ffmpeg") || (m_sProjectName == "avconv")));
    mProjectDeps["cuvid"] =
        (m_sProjectName == "libavcodec") || (m_sProjectName == "ffmpeg") || (m_sProjectName == "avconv");
    mProjectDeps["d3d11va"] = false;    // supplied by windows sdk
    mProjectDeps["dxva2"] = false;
    mProjectDeps["decklink"] = (m_sProjectName == "libavdevice");
    mProjectDeps["libfontconfig"] = (m_sProjectName == "libavfilter");
    mProjectDeps["ffnvcodec"] = false;
    mProjectDeps["frei0r"] = (m_sProjectName == "libavfilter");
    mProjectDeps["gcrypt"] = (m_sProjectName == "libavformat");
    mProjectDeps["gmp"] = (m_sProjectName == "libavformat");
    mProjectDeps["gnutls"] = (m_sProjectName == "libavformat");
    mProjectDeps["iconv"] = (m_sProjectName == "libavcodec");
    mProjectDeps["ladspa"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libaacplus"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libass"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libbluray"] = (m_sProjectName == "libavformat");
    mProjectDeps["libbs2b"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libcaca"] = (m_sProjectName == "libavdevice");
    mProjectDeps["libcdio"] = (m_sProjectName == "libavdevice");
    mProjectDeps["libcelt"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libdc1394"] = (m_sProjectName == "libavdevice");
    mProjectDeps["libdcadec"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libfaac"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libfdk_aac"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libflite"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libfreetype"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libfribidi"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libgme"] = (m_sProjectName == "libavformat");
    mProjectDeps["libgsm"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libiec61883"] = (m_sProjectName == "libavdevice");
    mProjectDeps["libilbc"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libkvazaar"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libmfx"] = ((m_sProjectName == "libavutil") && findSourceFile("hwcontext_qsv", ".h", sNotUsed)) ||
        (m_sProjectName == "libavcodec") ||
        ((m_sProjectName == "libavfilter") && findSourceFile("vf_deinterlace_qsv", ".c", sNotUsed)) ||
        (m_sProjectName == "ffmpeg") || (m_sProjectName == "avconv");
    mProjectDeps["libmodplug"] = (m_sProjectName == "libavformat");
    mProjectDeps["libmp3lame"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libnpp"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libnut"] = (m_sProjectName == "libformat");
    mProjectDeps["libopencore_amrnb"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libopencore_amrwb"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libopencv"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libopenjpeg"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libopenh264"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libopus"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libpulse"] = (m_sProjectName == "libavdevice");
    mProjectDeps["librubberband"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libquvi"] = (m_sProjectName == "libavformat");
    mProjectDeps["librtmp"] = (m_sProjectName == "libavformat");
    mProjectDeps["libschroedinger"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libshine"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libsmbclient"] = (m_sProjectName == "libavformat");
    mProjectDeps["libsnappy"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libsoxr"] = (m_sProjectName == "libswresample");
    mProjectDeps["libspeex"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libssh"] = (m_sProjectName == "libavformat");
    mProjectDeps["libstagefright_h264"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libtesseract"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libtheora"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libtwolame"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libutvideo"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libv4l2"] = (m_sProjectName == "libavdevice");
    mProjectDeps["libvidstab"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libvo_aacenc"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libvo_amrwbenc"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libvorbis"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libvpx"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libwavpack"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libwebp"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libx264"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libx265"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libxavs"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libxml2"] = (m_sProjectName == "libavformat");
    mProjectDeps["libxvid"] = (m_sProjectName == "libavcodec");
    mProjectDeps["libzimg"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libzmq"] = (m_sProjectName == "libavfilter");
    mProjectDeps["libzvbi"] = (m_sProjectName == "libavcodec");
    mProjectDeps["lzma"] = (m_sProjectName == "libavcodec");
    mProjectDeps["nvdec"] = (m_sProjectName == "libavcodec");
    mProjectDeps["nvenc"] = (m_sProjectName == "libavcodec");
    mProjectDeps["openal"] = (m_sProjectName == "libavdevice");
    mProjectDeps["opencl"] = (m_sProjectName == "libavutil") || (m_sProjectName == "libavfilter") ||
        (m_sProjectName == "ffmpeg") || (m_sProjectName == "avconv") || (m_sProjectName == "ffplay") ||
        (m_sProjectName == "avplay") || (m_sProjectName == "ffprobe") || (m_sProjectName == "avprobe");
    mProjectDeps["opengl"] = (m_sProjectName == "libavdevice");
    mProjectDeps["openssl"] = (m_sProjectName == "libavformat");
    mProjectDeps["schannel"] = (m_sProjectName == "libavformat");
    mProjectDeps["sdl"] =
        (m_sProjectName == "libavdevice") || (m_sProjectName == "ffplay") || (m_sProjectName == "avplay");
    mProjectDeps["sdl2"] =
        (m_sProjectName == "libavdevice") || (m_sProjectName == "ffplay") || (m_sProjectName == "avplay");
    // mProjectDeps["x11grab"] = ( m_projectName.compare("libavdevice") == 0 );//Always disabled on Win32
    mProjectDeps["zlib"] = (m_sProjectName == "libavformat") || (m_sProjectName == "libavcodec");
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

    if (!m_ConfigHelper.m_isLibav) {
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
    if (m_sProjectName == "libavcodec") {
        vSearchLists.push_back({"encoder", "ENC", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        vSearchLists.push_back({"decoder", "DEC", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        vSearchLists.push_back({"hwaccel", "HWACCEL", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        vSearchLists.push_back({"parser", "PARSER", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
    } else if (m_sProjectName == "libavformat") {
        vSearchLists.push_back({"muxer", "_MUX", "libavformat/allformats.c", "libavformat/avformat.h"});
        vSearchLists.push_back({"demuxer", "DEMUX", "libavformat/allformats.c", "libavformat/avformat.h"});
    } else if (m_sProjectName == "libavfilter") {
        vSearchLists.push_back({"filter", "FILTER", "libavfilter/allfilters.c", "libavfilter/avfilter.h"});
    } else if (m_sProjectName == "libavdevice") {
        vSearchLists.push_back({"outdev", "OUTDEV", "libavdevice/alldevices.c", "libavdevice/avdevice.h"});
        vSearchLists.push_back({"indev", "_IN", "libavdevice/alldevices.c", "libavdevice/avdevice.h"});
    }

    for (auto itLists = vSearchLists.begin(); itLists < vSearchLists.end(); ++itLists) {
        // We need to get the values from each config list
        StaticList vRet;
        StaticList vRetExterns;
        string sList = (*itLists).sList;
        transform(sList.begin(), sList.end(), sList.begin(), toupper);
        m_ConfigHelper.passFindThings(itLists->sList, itLists->sSearch, itLists->sFile, vRet, &vRetExterns);
        for (auto itRet = vRet.begin(), itRet2 = vRetExterns.begin(); itRet < vRet.end(); ++itRet, ++itRet2) {
            string sType = *itRet2;
            transform(itRet->begin(), itRet->end(), itRet->begin(), toupper);
            mDCEVariables[sType] = {"CONFIG_" + *itRet, itLists->sHeader};
        }
    }
}