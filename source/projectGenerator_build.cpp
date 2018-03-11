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

void ProjectGenerator::buildInterDependenciesHelper(const StaticList & vConfigOptions, const StaticList & vAddDeps, StaticList & vLibs)
{
    bool bFound = false;
    for (StaticList::const_iterator itI = vConfigOptions.begin(); itI < vConfigOptions.end(); itI++) {
        ConfigGenerator::ValuesList::iterator itConfOpt = m_ConfigHelper.getConfigOption(*itI);
        bFound = ((itConfOpt != m_ConfigHelper.m_vConfigValues.end()) && (m_ConfigHelper.getConfigOption(*itI)->m_sValue.compare("1") == 0));
        if (!bFound) {
            break;
        }
    }
    if (bFound) {
        for (StaticList::const_iterator itI = vAddDeps.begin(); itI < vAddDeps.end(); itI++) {
            string sSearchTag = "lib" + *itI;
            if (find(vLibs.begin(), vLibs.end(), sSearchTag) == vLibs.end()) {
                vLibs.push_back(sSearchTag);
            }
        }
    }
}

void ProjectGenerator::buildInterDependencies(StaticList & vLibs)
{
    //Get the lib dependencies from the configure file
    string sLibName = m_sProjectName.substr(3) + "_deps";
    vector<string> vLibDeps;
    if (m_ConfigHelper.getConfigList(sLibName, vLibDeps, false)) {
        for (vector<string>::iterator itI = vLibDeps.begin(); itI < vLibDeps.end(); itI++) {
            string sSearchTag = "lib" + *itI;
            if (find(vLibs.begin(), vLibs.end(), sSearchTag) == vLibs.end()) {
                vLibs.push_back(sSearchTag);
            }
        }
    }

    //Hard coded configuration checks for inter dependencies between different source libs.
    if (m_sProjectName.compare("libavfilter") == 0) {
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
    } else if (m_sProjectName.compare("libavdevice") == 0) {
        buildInterDependenciesHelper({"lavfi_indev"}, {"avfilter"}, vLibs);
    } else if (m_sProjectName.compare("libavcodec") == 0) {
        buildInterDependenciesHelper({"opus_decoder"}, {"swresample"}, vLibs);
    }
}

void ProjectGenerator::buildDependencies(StaticList & vLibs, StaticList & vAddLibs)
{
    //Add any forced dependencies
    if (m_sProjectName.compare("libavformat") == 0) {
        vAddLibs.push_back("ws2_32"); //Add the additional required libs
    }

    //Determine only those dependencies that are valid for current project
    map<string, bool> mProjectDeps;
    buildProjectDependencies(mProjectDeps);

    //Loop through each known configuration option and add the required dependencies
    vector<string> vExternLibs;
    m_ConfigHelper.getConfigList("EXTERNAL_AUTODETECT_LIBRARY_LIST", vExternLibs, false);
    m_ConfigHelper.getConfigList("EXTERNAL_LIBRARY_LIST", vExternLibs);
    m_ConfigHelper.getConfigList("HW_CODECS_LIST", vExternLibs, false); //used on some older ffmpeg versions
    m_ConfigHelper.getConfigList("HWACCEL_AUTODETECT_LIBRARY_LIST", vExternLibs, false);
    m_ConfigHelper.getConfigList("HWACCEL_LIBRARY_LIST", vExternLibs, false);
    for (vector<string>::iterator vitLib = vExternLibs.begin(); vitLib < vExternLibs.end(); vitLib++) {
        //Check if enabled
        if (m_ConfigHelper.getConfigOption(*vitLib)->m_sValue.compare("1") == 0) {
            //Check if this dependency is valid for this project (if the dependency is not known default to enable)
            if (mProjectDeps.find(*vitLib) == mProjectDeps.end()) {
                outputInfo("Unknown dependency found (" + *vitLib + ")");
            } else if (!mProjectDeps[*vitLib]) {
                //This dependency is not valid for this project so skip it
                continue;
            }

            string sLib;
            if (vitLib->compare("avisynth") == 0) {
                //doesn't need any additional libs
            } else if (vitLib->compare("bzlib") == 0) {
                sLib = "libbz2";
            } else if (vitLib->compare("libcdio") == 0) {
                sLib = "libcdio_paranoia";
            } else if (vitLib->compare("libfdk_aac") == 0) {
                sLib = "libfdk-aac";
            } else if (vitLib->compare("libnpp") == 0) {
                vAddLibs.push_back("nppi"); //Add the additional required libs
                //CUDA 7.5 onwards only provides npp for x64
            } else if (vitLib->compare("libxvid") == 0) {
                sLib = "libxvidcore";
            } else if (vitLib->compare("openssl") == 0) {
                //Needs ws2_32 but libavformat needs this even if not using openssl so it is already included
                sLib = "libssl";
                //Also need crypto
                vector<string>::iterator vitList = vLibs.begin();
                for (vitList; vitList < vLibs.end(); vitList++) {
                    if (vitList->compare("libcrypto") == 0) {
                        break;
                    }
                }
                if (vitList == vLibs.end()) {
                    vLibs.push_back("libcrypto");
                }
            } else if (vitLib->compare("decklink") == 0) {
                //Doesn't need any additional libs
            } else if (vitLib->compare("opengl") == 0) {
                vAddLibs.push_back("Opengl32"); //Add the additional required libs
            } else if (vitLib->compare("opencl") == 0) {
                string sFileName;
                if (!findFile(m_ConfigHelper.m_sRootDirectory + "compat/opencl/cl.h", sFileName)) {
                    vAddLibs.push_back("OpenCL"); //Add the additional required libs
                }
            } else if (vitLib->compare("openal") == 0) {
                vAddLibs.push_back("OpenAL32"); //Add the additional required libs
            } else if (vitLib->compare("ffnvcodec") == 0) {
                //Doesn't require any additional libs
            } else if (vitLib->compare("nvenc") == 0) {
                //Doesn't require any additional libs
            } else if (vitLib->compare("cuda") == 0) {
                string sFileName;
                if (!m_ConfigHelper.isConfigOptionEnabled("ffnvcodec") && !findFile(m_ConfigHelper.m_sRootDirectory + "compat/cuda/dynlink_cuda.h", sFileName)) {
                    vAddLibs.push_back("cuda"); //Add the additional required libs
                }
            } else if (vitLib->compare("cuvid") == 0) {
                string sFileName;
                if (!m_ConfigHelper.isConfigOptionEnabled("ffnvcodec") && !findFile(m_ConfigHelper.m_sRootDirectory + "compat/cuda/dynlink_nvcuvid.h", sFileName)) {
                    vAddLibs.push_back("nvcuvid"); //Add the additional required libs
                }
            } else if ((vitLib->compare("nvdec") == 0) || (vitLib->compare("nvenc") == 0)) {
                //Doesn't need any additional libs
            } else if (vitLib->compare("schannel") == 0) {
                vAddLibs.push_back("Secur32"); //Add the additional required libs
            } else if (vitLib->compare("sdl") == 0) {
                if (m_ConfigHelper.getConfigOption("sdl2") == m_ConfigHelper.m_vConfigValues.end()) {
                    vLibs.push_back("libsdl"); //Only add if not sdl2
                }
            } else {
                //By default just use the lib name and prefix with lib if not already
                if (vitLib->find("lib") == 0) {
                    sLib = *vitLib;
                } else {
                    sLib = "lib" + *vitLib;
                }
            }
            if (sLib.length() > 0) {
                //Check already not in list
                vector<string>::iterator vitList = vLibs.begin();
                for (vitList; vitList < vLibs.end(); vitList++) {
                    if (vitList->compare(sLib) == 0) {
                        break;
                    }
                }
                if (vitList == vLibs.end()) {
                    vLibs.push_back(sLib);
                }
            }
        }
    }

    //Add in extralibs used for various devices
    if (m_sProjectName.compare("libavdevice") == 0) {
        vExternLibs.resize(0);
        m_ConfigHelper.getConfigList("OUTDEV_LIST", vExternLibs);
        m_ConfigHelper.getConfigList("INDEV_LIST", vExternLibs);
        for (vector<string>::iterator vitLib = vExternLibs.begin(); vitLib < vExternLibs.end(); vitLib++) {
            //Check if enabled
            if (m_ConfigHelper.getConfigOption(*vitLib)->m_sValue.compare("1") == 0) {
                //Add the additional required libs
                if (vitLib->compare("dshow_indev") == 0) {
                    vAddLibs.push_back("strmiids");
                } else if (vitLib->compare("vfwcap_indev") == 0) {
                    vAddLibs.push_back("vfw32");
                    vAddLibs.push_back("shlwapi");
                } else if ((vitLib->compare("wasapi_indev") == 0) || (vitLib->compare("wasapi_outdev") == 0)) {
                    vAddLibs.push_back("ksuser");
                }
            }
        }
    }
}

void ProjectGenerator::buildDependencyValues(StaticList & vIncludeDirs, StaticList & vLib32Dirs, StaticList & vLib64Dirs, StaticList & vDefines)
{
    //Determine only those dependencies that are valid for current project
    map<string, bool> mProjectDeps;
    buildProjectDependencies(mProjectDeps);

    //Loop through each known configuration option and add the required dependencies
    for (map<string, bool>::iterator mitLib = mProjectDeps.begin(); mitLib != mProjectDeps.end(); mitLib++) {
        //Check if enabled//Check if optimised value is valid for current configuration
        ConfigGenerator::ValuesList::iterator vitProjectDep = m_ConfigHelper.getConfigOption(mitLib->first);
        if (mitLib->second && (vitProjectDep != m_ConfigHelper.m_vConfigValues.end()) && (vitProjectDep->m_sValue.compare("1") == 0)) {
            //Add in the additional include directories
            if (mitLib->first.compare("libopus") == 0) {
                vIncludeDirs.push_back("$(OutDir)/include/opus");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/opus");
            } else if (mitLib->first.compare("libfreetype") == 0) {
                vIncludeDirs.push_back("$(OutDir)/include/freetype2");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/freetype2");
            } else if (mitLib->first.compare("libfribidi") == 0) {
                vIncludeDirs.push_back("$(OutDir)/include/fribidi");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/fribidi");
            } else if (mitLib->first.compare("libxml2") == 0) {
                vIncludeDirs.push_back("$(OutDir)/include/libxml2");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/libxml2");
                vDefines.push_back("LIBXML_STATIC");
            } else if ((mitLib->first.compare("sdl") == 0) && (m_ConfigHelper.getConfigOption("sdl2") == m_ConfigHelper.m_vConfigValues.end())) {
                vIncludeDirs.push_back("$(OutDir)/include/SDL");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/SDL");
            } else if (mitLib->first.compare("sdl2") == 0) {
                vIncludeDirs.push_back("$(OutDir)/include/SDL");
                vIncludeDirs.push_back("$(ProjectDir)/../../prebuilt/include/SDL");
            } else if (mitLib->first.compare("opengl") == 0) {
                //Requires glext headers to be installed in include dir (does not require the libs)
            } else if (mitLib->first.compare("opencl") == 0) {
                string sFileName;
                if (!findFile(m_ConfigHelper.m_sRootDirectory + "compat/opencl/cl.h", sFileName)) {
                    //Need to check for the existence of environment variables
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
                        outputWarning("Either an OpenCL SDK is not installed or the environment variables are missing.", false);
                        outputWarning("The location of the OpenCl files will have to be manually specified as otherwise the project will not compile.", false);
                    }
                }
            } else if (mitLib->first.compare("openal") == 0) {
                if (!findEnvironmentVariable("OPENAL_SDK")) {
                    outputWarning("Could not find the OpenAl SDK environment variable.");
                    outputWarning("Either the OpenAL SDK is not installed or the environment variable is missing.", false);
                    outputWarning("Using the default environment variable of 'OPENAL_SDK'.", false);
                }
                vIncludeDirs.push_back("$(OPENAL_SDK)/include/");
                vLib32Dirs.push_back("$(OPENAL_SDK)/libs/Win32");
                vLib64Dirs.push_back("$(OPENAL_SDK)/lib/Win64");
            } else if (mitLib->first.compare("nvenc") == 0) {
                string sFileName;
                if (!m_ConfigHelper.isConfigOptionEnabled("ffnvcodec") && !findFile(m_ConfigHelper.m_sRootDirectory + "compat/nvenc/nvEncodeAPI.h", sFileName)) {
                    //Need to check for the existence of environment variables
                    if (!findEnvironmentVariable("CUDA_PATH")) {
                        outputWarning("Could not find the CUDA SDK environment variable.");
                        outputWarning("Either the CUDA SDK is not installed or the environment variable is missing.", false);
                        outputWarning("NVENC requires CUDA to be installed with NVENC headers made available in the CUDA SDK include path.", false);
                    }
                    //Only add if it hasn’t already been added
                    if (find(vIncludeDirs.begin(), vIncludeDirs.end(), "$(CUDA_PATH)/include/") == vIncludeDirs.end()) {
                        vIncludeDirs.push_back("$(CUDA_PATH)/include/");
                    }
                }
            } else if ((mitLib->first.compare("cuda") == 0) || (mitLib->first.compare("cuvid") == 0)) {
                string sFileName;
                if (!m_ConfigHelper.isConfigOptionEnabled("ffnvcodec") && !findFile(m_ConfigHelper.m_sRootDirectory + "compat/cuda/dynlink_cuda.h", sFileName)) {
                    //Need to check for the existence of environment variables
                    if (!findEnvironmentVariable("CUDA_PATH")) {
                        outputWarning("Could not find the CUDA SDK environment variable.");
                        outputWarning("Either the CUDA SDK is not installed or the environment variable is missing.", false);
                    }
                    //Only add if it hasn’t already been added
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

void ProjectGenerator::buildProjectDependencies(map<string, bool> & mProjectDeps)
{
    string sNotUsed;
    mProjectDeps["avisynth"] = false; //no dependencies ever needed
    mProjectDeps["bzlib"] = (m_sProjectName.compare("libavformat") == 0) || (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["crystalhd"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["chromaprint"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["cuda"] = ((m_sProjectName.compare("libavutil") == 0) && findSourceFile("hwcontext_cuda", ".h", sNotUsed)) || (m_sProjectName.compare("libavfilter") == 0) ||
        (m_ConfigHelper.isConfigOptionEnabled("nvenc") && (m_sProjectName.compare("libavcodec") == 0)) ||
        (m_ConfigHelper.isConfigOptionEnabled("cuvid") && ((m_sProjectName.compare("libavcodec") == 0) ||
        (m_sProjectName.compare("ffmpeg") == 0) || (m_sProjectName.compare("avconv") == 0)));
    mProjectDeps["cuvid"] = (m_sProjectName.compare("libavcodec") == 0) || (m_sProjectName.compare("ffmpeg") == 0) || (m_sProjectName.compare("avconv") == 0);
    mProjectDeps["d3d11va"] = false; //supplied by windows sdk
    mProjectDeps["dxva2"] = false;
    mProjectDeps["decklink"] = (m_sProjectName.compare("libavdevice") == 0);
    mProjectDeps["libfontconfig"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["ffnvcodec"] = (m_sProjectName.compare("libavutil") == 0) || (m_sProjectName.compare("libavfilter") == 0) ||
        (m_ConfigHelper.isConfigOptionEnabled("nvenc") && (m_sProjectName.compare("libavcodec") == 0)) ||
        (m_ConfigHelper.isConfigOptionEnabled("cuvid") && ((m_sProjectName.compare("libavcodec") == 0) ||
        (m_sProjectName.compare("ffmpeg") == 0) || (m_sProjectName.compare("avconv") == 0)));
    mProjectDeps["frei0r"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["gcrypt"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["gmp"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["gnutls"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["iconv"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["ladspa"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libaacplus"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libass"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libbluray"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["libbs2b"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libcaca"] = (m_sProjectName.compare("libavdevice") == 0);
    mProjectDeps["libcdio"] = (m_sProjectName.compare("libavdevice") == 0);
    mProjectDeps["libcelt"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libdc1394"] = (m_sProjectName.compare("libavdevice") == 0);
    mProjectDeps["libdcadec"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libfaac"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libfdk_aac"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libflite"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libfreetype"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libfribidi"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libgme"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["libgsm"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libiec61883"] = (m_sProjectName.compare("libavdevice") == 0);
    mProjectDeps["libilbc"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libkvazaar"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libmfx"] = ((m_sProjectName.compare("libavutil") == 0) && findSourceFile("hwcontext_qsv", ".h", sNotUsed)) ||
        (m_sProjectName.compare("libavcodec") == 0) || ((m_sProjectName.compare("libavfilter") == 0) && findSourceFile("vf_deinterlace_qsv", ".c", sNotUsed)) ||
        (m_sProjectName.compare("ffmpeg") == 0) || (m_sProjectName.compare("avconv") == 0);
    mProjectDeps["libmodplug"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["libmp3lame"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libnpp"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libnut"] = (m_sProjectName.compare("libformat") == 0);
    mProjectDeps["libopencore_amrnb"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libopencore_amrwb"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libopencv"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libopenjpeg"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libopenh264"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libopus"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libpulse"] = (m_sProjectName.compare("libavdevice") == 0);
    mProjectDeps["librubberband"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libquvi"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["librtmp"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["libschroedinger"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libshine"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libsmbclient"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["libsnappy"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libsoxr"] = (m_sProjectName.compare("libswresample") == 0);
    mProjectDeps["libspeex"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libssh"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["libstagefright_h264"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libtesseract"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libtheora"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libtwolame"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libutvideo"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libv4l2"] = (m_sProjectName.compare("libavdevice") == 0);
    mProjectDeps["libvidstab"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libvo_aacenc"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libvo_amrwbenc"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libvorbis"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libvpx"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libwavpack"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libwebp"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libx264"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libx265"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libxavs"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libxml2"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["libxvid"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["libzimg"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libzmq"] = (m_sProjectName.compare("libavfilter") == 0);
    mProjectDeps["libzvbi"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["lzma"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["nvdec"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["nvenc"] = (m_sProjectName.compare("libavcodec") == 0);
    mProjectDeps["openal"] = (m_sProjectName.compare("libavdevice") == 0);
    mProjectDeps["opencl"] = (m_sProjectName.compare("libavutil") == 0) || (m_sProjectName.compare("libavfilter") == 0) || (m_sProjectName.compare("ffmpeg") == 0) || (m_sProjectName.compare("avconv") == 0)
        || (m_sProjectName.compare("ffplay") == 0) || (m_sProjectName.compare("avplay") == 0) || (m_sProjectName.compare("ffprobe") == 0) || (m_sProjectName.compare("avprobe") == 0);
    mProjectDeps["opengl"] = (m_sProjectName.compare("libavdevice") == 0);
    mProjectDeps["openssl"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["schannel"] = (m_sProjectName.compare("libavformat") == 0);
    mProjectDeps["sdl"] = (m_sProjectName.compare("libavdevice") == 0) || (m_sProjectName.compare("ffplay") == 0) || (m_sProjectName.compare("avplay") == 0);
    mProjectDeps["sdl2"] = (m_sProjectName.compare("libavdevice") == 0) || (m_sProjectName.compare("ffplay") == 0) || (m_sProjectName.compare("avplay") == 0);
    //mProjectDeps["x11grab"] = ( m_sProjectName.compare("libavdevice") == 0 );//Always disabled on Win32
    mProjectDeps["zlib"] = (m_sProjectName.compare("libavformat") == 0) || (m_sProjectName.compare("libavcodec") == 0);
}

void ProjectGenerator::buildProjectGUIDs(map<string, string> & mKeys)
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

    if (!m_ConfigHelper.m_bLibav) {
        mKeys["ffmpeg"] = "4081C77E-F1F7-49FA-9BD8-A4D267C83716";
        mKeys["ffplay"] = "E2A6865D-BD68-45B4-8130-EFD620F2C7EB";
        mKeys["ffprobe"] = "147A422A-FA63-4724-A5D9-08B1CAFDAB59";
    } else {
        mKeys["avconv"] = "4081C77E-F1F7-49FA-9BD8-A4D267C83716";
        mKeys["avplay"] = "E2A6865D-BD68-45B4-8130-EFD620F2C7EB";
        mKeys["avprobe"] = "147A422A-FA63-4724-A5D9-08B1CAFDAB59";
    }
}

void ProjectGenerator::buildProjectDCEs(map<string, DCEParams> & mDCEDefinitions, map<string, DCEParams> & mDCEVariables)
{
    //TODO: Detect these automatically
    //Next we need to check for all the configurations that are project specific
    struct FindThingsVars
    {
        string sList;
        string sSearch;
        string sFile;
        string sHeader;
    };
    vector<FindThingsVars> vSearchLists;
    if (m_sProjectName.compare("libavcodec") == 0) {
        vSearchLists.push_back({"encoder", "ENC", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        vSearchLists.push_back({"decoder", "DEC", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        vSearchLists.push_back({"hwaccel", "HWACCEL", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
        vSearchLists.push_back({"parser", "PARSER", "libavcodec/allcodecs.c", "libavcodec/avcodec.h"});
    } else if (m_sProjectName.compare("libavformat") == 0) {
        vSearchLists.push_back({"muxer", "_MUX", "libavformat/allformats.c", "libavformat/avformat.h"});
        vSearchLists.push_back({"demuxer", "DEMUX", "libavformat/allformats.c", "libavformat/avformat.h"});
    } else if (m_sProjectName.compare("libavfilter") == 0) {
        vSearchLists.push_back({"filter", "FILTER", "libavfilter/allfilters.c", "libavfilter/avfilter.h"});
    } else if (m_sProjectName.compare("libavdevice") == 0) {
        vSearchLists.push_back({"outdev", "OUTDEV", "libavdevice/alldevices.c", "libavdevice/avdevice.h"});
        vSearchLists.push_back({"indev", "_IN", "libavdevice/alldevices.c", "libavdevice/avdevice.h"});
    }

    for (vector<FindThingsVars>::iterator itLists = vSearchLists.begin(); itLists < vSearchLists.end(); itLists++) {
        //We need to get the values from each config list
        StaticList vRet;
        StaticList vRetExterns;
        string sList = (*itLists).sList;
        transform(sList.begin(), sList.end(), sList.begin(), ::toupper);
        m_ConfigHelper.passFindThings(itLists->sList, itLists->sSearch, itLists->sFile, vRet, &vRetExterns);
        for (StaticList::iterator itRet = vRet.begin(), itRet2 = vRetExterns.begin(); itRet < vRet.end(); itRet++, itRet2++) {
            string sType = *itRet2;
            transform(itRet->begin(), itRet->end(), itRet->begin(), ::toupper);
            mDCEVariables[sType] = {"CONFIG_" + *itRet, itLists->sHeader};
        }
    }
}