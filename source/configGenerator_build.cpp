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

#include "configGenerator.h"

#include <algorithm>

bool ConfigGenerator::buildDefaultValues()
{
    // Set any unset project values
    if (m_solutionDirectory.length() == 0) {
        if (!m_onlyDCE) {
            m_solutionDirectory = m_rootDirectory + "SMP/";
        } else {
            m_solutionDirectory = m_rootDirectory;
        }
    }
    if (m_outDirectory.length() == 0) {
        m_outDirectory = "../../../msvc/";
    }

    // Bulk disable most common options. The ones that are actually available will be set later
    vector<string> list;
    vector<string> archLists = {"ARCH_LIST", "HAVE_LIST", "DOCUMENT_LIST", "FEATURE_LIST", "EXAMPLE_LIST"};
    for (const auto& i : archLists) {
        list.resize(0);
        if (getConfigList(i, list, false)) {
            for (const auto& j : list) {
                fastToggleConfigValue(j, false);
                // Also disable _EXTERNAL and _INLINE
                fastToggleConfigValue(i + "_EXTERNAL", false);
                fastToggleConfigValue(i + "_INLINE", false);
            }
        }
    }

    // Enable all programs
    list.resize(0);
    if (getConfigList("PROGRAM_LIST", list, false)) {
        // Enable all programs
        for (const auto& i : list) {
            fastToggleConfigValue(i, true);
        }
    }
    // Enable all libraries
    list.resize(0);
    if (getConfigList("LIBRARY_LIST", list, false)) {
        for (const auto& i : list) {
            if (!m_isLibav && i != "avresample") {
                fastToggleConfigValue(i, true);
            }
        }
    }
    // Enable all components
    list.resize(0);
    vector<string> list2;
    if (getConfigList("COMPONENT_LIST", list, false)) {
        for (auto& i : list) {
            fastToggleConfigValue(i, true);
            // Get the corresponding list and enable all member elements as well
            i.resize(i.length() - 1); // Need to remove the s from end
            transform(i.begin(), i.end(), i.begin(), toupper);
            // Get the specific list
            list2.resize(0);
            if (getConfigList(i + "_LIST", list2)) {
                for (const auto& j : list2) {
                    fastToggleConfigValue(j, true);
                }
            }
        }
    }

    fastToggleConfigValue("debug", true);
    fastToggleConfigValue("optimizations", true);
    fastToggleConfigValue("runtime_cpudetect", true);
    fastToggleConfigValue("safe_bitstream_reader", true);
    fastToggleConfigValue("static", true);
    fastToggleConfigValue("shared", true);
    fastToggleConfigValue("swscale_alpha", true);

    // Enable all extra libs
    list.resize(0);
    if (getConfigList("EXTRALIBS_LIST", list, false)) {
        for (const auto& i : list) {
            fastToggleConfigValue(i, true);
        }
    }

    // Disable all external libs until explicitly enabled
    list.resize(0);
    if (getConfigList("EXTERNAL_LIBRARY_LIST", list, false)) {
        for (const auto& i : list) {
            fastToggleConfigValue(i, false);
        }
    }

    // Disable all hwaccels until explicitly enabled
    list.resize(0);
    if (getConfigList("HWACCEL_LIBRARY_LIST", list, false)) {
        for (const auto& i : list) {
            fastToggleConfigValue(i, false);
        }
    }

    // Enable x86 hardware architectures
    fastToggleConfigValue("x86", true);
    fastToggleConfigValue("i686", true);
    fastToggleConfigValue("fast_64bit", true);
    fastToggleConfigValue("fast_cmov", true);
    fastToggleConfigValue("simd_align_16", true);
    fastToggleConfigValue("simd_align_32", true);
    fastToggleConfigValue("simd_align_64", true);
    fastToggleConfigValue("x86_32", true);
    fastToggleConfigValue("x86_64", true);
    // Enable x86 extensions
    list.resize(0);
    if (getConfigList("ARCH_EXT_LIST_X86", list, false)) {
        for (const auto& i : list) {
            fastToggleConfigValue(i, true);
            // Also enable _EXTERNAL and _INLINE
            fastToggleConfigValue(i + "_EXTERNAL", true);
            fastToggleConfigValue(i + "_INLINE", true);
        }
    }

    // Default we enable asm
    fastToggleConfigValue("yasm", true);
    fastToggleConfigValue("x86asm", true);
    fastToggleConfigValue("asm", true);
    if (m_useNASM) {
        // NASM doesn't support cpunop
        fastToggleConfigValue("cpunop", false);
        fastToggleConfigValue("cpunop_external", false);
    } else {
        // Yasm doesn't support avx512
        fastToggleConfigValue("avx512", false);
        fastToggleConfigValue("avx512_external", false);
        fastToggleConfigValue("simd_align_64", false);
        // Yasm does have cpunop
        fastToggleConfigValue("cpunop", true);
    }

    // msvc specific options
    fastToggleConfigValue("w32threads", true);
    fastToggleConfigValue("threads", true);
    fastToggleConfigValue("atomics_win32", true);

    // math functions
    list.resize(0);
    if (getConfigList("MATH_FUNCS", list, false)) {
        for (const auto& i : list) {
            fastToggleConfigValue(i, true);
        }
    }

    fastToggleConfigValue("access", true);
    fastToggleConfigValue("aligned_malloc", true);
    fastToggleConfigValue("Audioclient_h", true);
    fastToggleConfigValue("bcrypt", true);
    fastToggleConfigValue("clock_gettime", false);
    fastToggleConfigValue("closesocket", true);
    fastToggleConfigValue("CommandLineToArgvW", true);
    fastToggleConfigValue("CoTaskMemFree", true);
    fastToggleConfigValue("CryptGenRandom", true);
    fastToggleConfigValue("d3d11_h", true);
    fastToggleConfigValue("direct_h", true);
    fastToggleConfigValue("dos_paths", true);
    fastToggleConfigValue("dxgidebug_h", true);
    fastToggleConfigValue("dxva_h", true);
    fastToggleConfigValue("dxva2api_cobj", true);
    fastToggleConfigValue("dxva2_lib", true);
    fastToggleConfigValue("ebp_available", true);
    fastToggleConfigValue("ebx_available", true);
    fastToggleConfigValue("fast_clz", true);
    fastToggleConfigValue("flt_lim", true);
    fastToggleConfigValue("getaddrinfo", true);
    fastToggleConfigValue("getopt", false);
    fastToggleConfigValue("GetModuleHandle", true);
    fastToggleConfigValue("GetProcessAffinityMask", true);
    fastToggleConfigValue("GetProcessMemoryInfo", true);
    fastToggleConfigValue("GetStdHandle", true);
    fastToggleConfigValue("GetProcessTimes", true);
    fastToggleConfigValue("GetSystemTimeAsFileTime", true);
    fastToggleConfigValue("io_h", true);
    fastToggleConfigValue("inline_asm_labels", true);
    fastToggleConfigValue("isatty", true);
    fastToggleConfigValue("kbhit", true);
    fastToggleConfigValue("LoadLibrary", true);
    fastToggleConfigValue("libc_msvcrt", true);
    fastToggleConfigValue("local_aligned_32", true);
    fastToggleConfigValue("local_aligned_16", true);
    fastToggleConfigValue("local_aligned_8", true);
    fastToggleConfigValue("local_aligned", true);
    fastToggleConfigValue("malloc_h", true);
    fastToggleConfigValue("MapViewOfFile", true);
    fastToggleConfigValue("MemoryBarrier", true);
    fastToggleConfigValue("mm_empty", true);
    fastToggleConfigValue("PeekNamedPipe", true);
    fastToggleConfigValue("rdtsc", true);
    fastToggleConfigValue("rsync_contimeout", true);
    fastToggleConfigValue("SetConsoleTextAttribute", true);
    fastToggleConfigValue("SetConsoleCtrlHandler", true);
    fastToggleConfigValue("SetDllDirectory", true);
    fastToggleConfigValue("setmode", true);
    fastToggleConfigValue("Sleep", true);
    fastToggleConfigValue("CONDITION_VARIABLE_Ptr", true);
    fastToggleConfigValue("socklen_t", true);
    fastToggleConfigValue("struct_addrinfo", true);
    fastToggleConfigValue("struct_group_source_req", true);
    fastToggleConfigValue("struct_ip_mreq_source", true);
    fastToggleConfigValue("struct_ipv6_mreq", true);
    fastToggleConfigValue("struct_pollfd", true);
    fastToggleConfigValue("struct_sockaddr_in6", true);
    fastToggleConfigValue("struct_sockaddr_storage", true);
    fastToggleConfigValue("unistd_h", false);
    fastToggleConfigValue("uwp", true);
    fastToggleConfigValue("VirtualAlloc", true);
    fastToggleConfigValue("windows_h", true);
    fastToggleConfigValue("winsock2_h", true);
    fastToggleConfigValue("winrt", true);
    fastToggleConfigValue("wglgetprocaddress", true);

    fastToggleConfigValue("aligned_stack", true);
    fastToggleConfigValue("pragma_deprecated", true);
    fastToggleConfigValue("inline_asm", true);
    fastToggleConfigValue("frame_thread_encoder", true);
    fastToggleConfigValue("xmm_clobbers", true);

    // Additional (must be explicitly disabled)
    fastToggleConfigValue("dct", true);
    fastToggleConfigValue("dwt", true);
    fastToggleConfigValue("error_resilience", true);
    fastToggleConfigValue("faan", true);
    fastToggleConfigValue("faandct", true);
    fastToggleConfigValue("faanidct", true);
    fastToggleConfigValue("fast_unaligned", true);
    fastToggleConfigValue("lsp", true);
    fastToggleConfigValue("lzo", true);
    fastToggleConfigValue("mdct", true);
    fastToggleConfigValue("network", true);
    fastToggleConfigValue("rdft", true);
    fastToggleConfigValue("fft", true);
    fastToggleConfigValue("pixelutils", true);

    // Check if auto detection is enabled
    const auto autoDet = getConfigOption("autodetect");
    if ((autoDet == m_configValues.end()) || (autoDet->m_value != "0")) {
        // Enable all the auto detected libs
        vector<string> list;
        if (getConfigList("AUTODETECT_LIBS", list)) {
            fastToggleConfigValue("autodetect", true);
        } else {
            // If no auto list then just use hard enables
            fastToggleConfigValue("bzlib", true);
            fastToggleConfigValue("iconv", true);
            fastToggleConfigValue("lzma", true);
            fastToggleConfigValue("schannel", true);
            fastToggleConfigValue("sdl", true);
            fastToggleConfigValue("sdl2", true);
            fastToggleConfigValue("zlib", true);

            // Enable hwaccels by default.
            fastToggleConfigValue("d3d11va", true);
            fastToggleConfigValue("dxva2", true);

            string sFileName;
            if (findFile(m_rootDirectory + "compat/cuda/dynlink_cuda.h", sFileName)) {
                fastToggleConfigValue("cuda", true);
                fastToggleConfigValue("cuvid", true);
            }
            if (findFile(m_rootDirectory + "compat/nvenc/nvEncodeAPI.h", sFileName)) {
                fastToggleConfigValue("nvenc", true);
            }
        }
    }

    return buildForcedValues();
}

bool ConfigGenerator::buildAutoDetectValues()
{
    // Check if auto detection is enabled
    const auto autoDet = getConfigOption("autodetect");
    if ((autoDet != m_configValues.end())) {
        // Enable/Disable all the auto detected libs
        const bool enableAuto = (autoDet->m_value != "0");
        vector<string> list;
        if (getConfigList("AUTODETECT_LIBS", list)) {
            string sFileName;
            for (const auto& i : list) {
                bool enable;
                // Handle detection of various libs
                if (i == "alsa") {
                    enable = false;
                } else if (i == "amf") {
                    makeFileGeneratorRelative(m_outDirectory + "include/AMF/core/Factory.h", sFileName);
                    enable = findFile(sFileName, sFileName);
                } else if (i == "appkit") {
                    enable = false;
                } else if (i == "audiotoolbox") {
                    enable = false;
                } else if (i == "avfoundation") {
                    enable = false;
                } else if (i == "bzlib") {
                    makeFileGeneratorRelative(m_outDirectory + "include/bzlib.h", sFileName);
                    enable = findFile(sFileName, sFileName);
                } else if (i == "coreimage") {
                    enable = false;
                } else if (i == "crystalhd") {
                    enable = false;
                } else if (i == "cuda" || i == "cuvid") {
                    enable = findFile(m_rootDirectory + "compat/cuda/dynlink_cuda.h", sFileName);
                    if (!enable) {
                        makeFileGeneratorRelative(m_outDirectory + "include/ffnvcodec/dynlink_cuda.h", sFileName);
                        enable = findFile(sFileName, sFileName);
                    }
                } else if (i == "cuda_llvm" || i == "cuda_nvcc") {
                    // Not currently supported
                    enable = false;
                } else if (i == "d3d11va") {
                    enable = true;
                } else if (i == "dxva2") {
                    enable = true;
                } else if (i == "ffnvcodec") {
                    makeFileGeneratorRelative(m_outDirectory + "include/ffnvcodec/dynlink_cuda.h", sFileName);
                    enable = findFile(sFileName, sFileName);
                } else if (i == "iconv") {
                    makeFileGeneratorRelative(m_outDirectory + "include/iconv.h", sFileName);
                    enable = findFile(sFileName, sFileName);
                } else if (i == "jack") {
                    enable = false;
                } else if (i == "libxcb") {
                    enable = false;
                } else if (i == "libxcb_shm") {
                    enable = false;
                } else if (i == "libxcb_shape") {
                    enable = false;
                } else if (i == "libxcb_xfixes") {
                    enable = false;
                } else if (i == "lzma") {
                    makeFileGeneratorRelative(m_outDirectory + "include/lzma.h", sFileName);
                    enable = findFile(sFileName, sFileName);
                } else if (i == "mediafoundation") {
                    enable = true;
                } else if (i == "metal") {
                    enable = false;
                } else if (i == "nvdec") {
                    enable = (findFile(m_rootDirectory + "compat/cuda/dynlink_loader.h", sFileName) &&
                        findFile(m_rootDirectory + "compat/cuda/dynlink_cuda.h", sFileName));
                    if (!enable) {
                        makeFileGeneratorRelative(m_outDirectory + "include/ffnvcodec/dynlink_loader.h", sFileName);
                        enable = findFile(sFileName, sFileName);
                    }
                } else if (i == "nvenc") {
                    enable = findFile(m_rootDirectory + "compat/nvenc/nvEncodeAPI.h", sFileName);
                    if (!enable) {
                        makeFileGeneratorRelative(m_outDirectory + "include/ffnvcodec/nvEncodeAPI.h", sFileName);
                        enable = findFile(sFileName, sFileName);
                    }
                } else if (i == "opencl") {
                    makeFileGeneratorRelative(m_outDirectory + "include/cl/cl.h", sFileName);
                    enable = findFile(sFileName, sFileName);
                    if (enable) {
                        fastToggleConfigValue("opencl_d3d11", true);
                        fastToggleConfigValue("opencl_dxva2", true);
                    }
                } else if (i == "os2threads") {
                    enable = false;
                } else if (i == "pthreads") {
                    enable = false;
                } else if (i == "schannel") {
                    enable = true;
                } else if (i == "sdl2") {
                    makeFileGeneratorRelative(m_outDirectory + "include/SDL/SDL.h", sFileName);
                    enable = findFile(sFileName, sFileName);
                } else if (i == "securetransport") {
                    enable = false;
                } else if (i == "sndio") {
                    enable = false;
                } else if (i == "v4l2_m2m") {
                    enable = false;
                } else if (i == "vaapi") {
                    enable = false;
                } else if (i == "vda") {
                    enable = false;
                } else if (i == "vdpau") {
                    enable = false;
                } else if (i == "videotoolbox") {
                    enable = false;
                } else if (i == "videotoolbox_hwaccel") {
                    enable = false;
                } else if (i == "vulkan") {
                    // Not currently supported
                    enable = false;
                } else if (i == "w32threads") {
                    enable = true;
                } else if (i == "xlib") {
                    enable = false;
                } else if (i == "xvmc") {
                    enable = false;
                } else if (i == "zlib") {
                    makeFileGeneratorRelative(m_outDirectory + "include/zlib.h", sFileName);
                    enable = findFile(sFileName, sFileName);
                } else {
                    // This is an unknown option
                    outputInfo("Found unknown auto detected option " + i);
                    // Just disable
                    enable = false;
                }
                toggleConfigValue(i, enable && enableAuto, true);
            }
        }
    }
    return true;
}

bool ConfigGenerator::buildForcedValues()
{
    // Additional options set for Intel compiler specific inline asm
    fastToggleConfigValue("inline_asm_nonlocal_labels", false);
    fastToggleConfigValue("inline_asm_direct_symbol_refs", false);
    fastToggleConfigValue("inline_asm_non_intel_mnemonic", false);

    fastToggleConfigValue("xlib", false); // enabled by default but is linux only so we force disable
    fastToggleConfigValue("qtkit", false);
    fastToggleConfigValue("avfoundation", false);
    fastToggleConfigValue("mmal", false);
    fastToggleConfigValue("libdrm", false);
    fastToggleConfigValue("libv4l2", false);

    // Values that are not correctly handled by configure
    fastToggleConfigValue("coreimage_filter", false);
    fastToggleConfigValue("coreimagesrc_filter", false);

    // Disabled filters that are were temporarily made available
    if (m_configureFile.find("disable mcdeint_filter") != string::npos) {
        fastToggleConfigValue("mcdeint_filter", false);
        fastToggleConfigValue("uspp_filter", false);
    }
    // Forced enables
    if (m_configureFile.find("enable frame_thread_encoder") != string::npos) {
        fastToggleConfigValue("frame_thread_encoder", true);
        fastToggleConfigValue("w32threads", true);
    }

    return true;
}

void ConfigGenerator::buildFixedValues(DefaultValuesList& fixedValues)
{
    fixedValues.clear();
    fixedValues["$(c_escape $FFMPEG_CONFIGURATION)"] = "";
    fixedValues["$(c_escape $LIBAV_CONFIGURATION)"] = "";
    fixedValues["$(c_escape $license)"] = "lgpl";
    fixedValues["$(eval c_escape $datadir)"] = ".";
    fixedValues["$(c_escape ${cc_ident:-Unknown compiler})"] = "msvc";
    fixedValues["$_restrict"] = "__restrict";
    fixedValues["$target_os"] = "Windows";
    fixedValues["$restrict_keyword"] = "__restrict";
    fixedValues["${extern_prefix}"] = "";
    fixedValues["$build_suffix"] = "";
    fixedValues["$SLIBSUF"] = "";
    fixedValues["$sws_max_filter_size"] = "256";
}

void ConfigGenerator::buildReplaceValues(
    DefaultValuesList& replaceValues, string& header, DefaultValuesList& replaceValuesASM)
{
    header = "#ifdef _WIN32\n\
#   include <sdkddkver.h>\n\
#   include <winapifamily.h>\n\
#endif";
    replaceValues.clear();
    // Add to config.h only list
    replaceValues["CC_IDENT"] = "#if defined(__INTEL_COMPILER)\n\
#   define CC_IDENT \"icl\"\n\
#else\n\
#   define CC_IDENT \"msvc\"\n\
#endif";
    replaceValues["EXTERN_PREFIX"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define EXTERN_PREFIX \"\"\n\
#else\n\
#   define EXTERN_PREFIX \"_\"\n\
#endif";
    replaceValues["EXTERN_ASM"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define EXTERN_ASM\n\
#else\n\
#   define EXTERN_ASM _\n\
#endif";
    replaceValues["SLIBSUF"] = "#if defined(_USRDLL) || defined(_WINDLL)\n\
#   define SLIBSUF \".dll\"\n\
#else\n\
#   define SLIBSUF \".lib\"\n\
#endif";

    replaceValues["ARCH_X86_32"] = "#if !defined(__x86_64) && !defined(_M_X64)\n\
#   define ARCH_X86_32 1\n\
#else\n\
#   define ARCH_X86_32 0\n\
#endif";
    replaceValues["ARCH_X86_64"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define ARCH_X86_64 1\n\
#else\n\
#   define ARCH_X86_64 0\n\
#endif";
    replaceValues["CONFIG_SHARED"] = "#if defined(_USRDLL) || defined(_WINDLL)\n\
#   define CONFIG_SHARED 1\n\
#else\n\
#   define CONFIG_SHARED 0\n\
#endif";
    replaceValues["CONFIG_STATIC"] = "#if !defined(_USRDLL) && !defined(_WINDLL)\n\
#   define CONFIG_STATIC 1\n\
#else\n\
#   define CONFIG_STATIC 0\n\
#endif";
    replaceValues["HAVE_ALIGNED_STACK"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define HAVE_ALIGNED_STACK 1\n\
#else\n\
#   define HAVE_ALIGNED_STACK 0\n\
#endif";
    replaceValues["HAVE_FAST_64BIT"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define HAVE_FAST_64BIT 1\n\
#else\n\
#   define HAVE_FAST_64BIT 0\n\
#endif";
    replaceValues["HAVE_INLINE_ASM"] = "#if defined(__INTEL_COMPILER)\n\
#   define HAVE_INLINE_ASM 1\n\
#else\n\
#   define HAVE_INLINE_ASM 0\n\
#endif";
    replaceValues["HAVE_MM_EMPTY"] = "#if defined(__INTEL_COMPILER) || ARCH_X86_32\n\
#   define HAVE_MM_EMPTY 1\n\
#else\n\
#   define HAVE_MM_EMPTY 0\n\
#endif";
    replaceValues["HAVE_STRUCT_POLLFD"] = "#if !defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x0600\n\
#   define HAVE_STRUCT_POLLFD 1\n\
#else\n\
#   define HAVE_STRUCT_POLLFD 0\n\
#endif";
    replaceValues["CONFIG_D3D11VA"] = "#if defined(NTDDI_WIN8)\n\
#   define CONFIG_D3D11VA 1\n\
#else\n\
#   define CONFIG_D3D11VA 0\n\
#endif";
    replaceValues["CONFIG_VP9_D3D11VA_HWACCEL"] = "#if defined(NTDDI_WIN10_TH2)\n\
#   define CONFIG_VP9_D3D11VA_HWACCEL 1\n\
#else\n\
#   define CONFIG_VP9_D3D11VA_HWACCEL 0\n\
#endif";
    replaceValues["CONFIG_VP9_D3D11VA2_HWACCEL"] = "#if defined(NTDDI_WIN10_TH2)\n\
#   define CONFIG_VP9_D3D11VA2_HWACCEL 1\n\
#else\n\
#   define CONFIG_VP9_D3D11VA2_HWACCEL 0\n\
#endif";
    replaceValues["CONFIG_VP9_DXVA2_HWACCEL"] = "#if defined(NTDDI_WIN10_TH2)\n\
#   define CONFIG_VP9_DXVA2_HWACCEL 1\n\
#else\n\
#   define CONFIG_VP9_DXVA2_HWACCEL 0\n\
#endif";
    replaceValues["CONFIG_AV1_D3D11VA_HWACCEL"] = "#if defined(NTDDI_WIN10_FE)\n\
#   define CONFIG_AV1_D3D11VA_HWACCEL 1\n\
#else\n\
#   define CONFIG_AV1_D3D11VA_HWACCEL 0\n\
#endif";
    replaceValues["CONFIG_AV1_D3D11VA2_HWACCEL"] = "#if defined(NTDDI_WIN10_FE)\n\
#   define CONFIG_AV1_D3D11VA2_HWACCEL 1\n\
#else\n\
#   define CONFIG_AV1_D3D11VA2_HWACCEL 0\n\
#endif";
    replaceValues["CONFIG_AV1_DXVA2_HWACCEL"] = "#if defined(NTDDI_WIN10_FE)\n\
#   define CONFIG_AV1_DXVA2_HWACCEL 1\n\
#else\n\
#   define CONFIG_AV1_DXVA2_HWACCEL 0\n\
#endif";
    replaceValues["HAVE_OPENCL_D3D11"] = "#if defined(NTDDI_WIN8)\n\
#   define HAVE_OPENCL_D3D11 1\n\
#else\n\
#   define HAVE_OPENCL_D3D11 0\n\
#endif";
    replaceValues["HAVE_GETPROCESSAFFINITYMASK"] = "#if defined(NTDDI_WIN10_RS3)\n\
#   define HAVE_GETPROCESSAFFINITYMASK 1\n\
#else\n\
#   define HAVE_GETPROCESSAFFINITYMASK 0\n\
#endif";

    // Build values specific for WinRT builds
    bool winrt = isConfigOptionEnabled("winrt");
    bool uwp = isConfigOptionEnabled("uwp");
    string winrtDefine;
    if (winrt) {
        winrtDefine += "!HAVE_WINRT";
    }
    if (uwp) {
        if (winrtDefine.length() > 0) {
            winrtDefine += " && ";
        }
        winrtDefine += "!HAVE_UWP";
    }

    if (winrt || uwp) {
        replaceValues["HAVE_UWP"] =
            "#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY==WINAPI_FAMILY_PC_APP || WINAPI_FAMILY==WINAPI_FAMILY_PHONE_APP)\n\
#   define HAVE_UWP 1\n\
#else\n\
#   define HAVE_UWP 0\n\
#endif";
        replaceValues["HAVE_WINRT"] =
            "#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY==WINAPI_FAMILY_PC_APP || WINAPI_FAMILY==WINAPI_FAMILY_PHONE_APP)\n\
#   define HAVE_WINRT 1\n\
#else\n\
#   define HAVE_WINRT 0\n\
#endif";
        replaceValues["HAVE_GETMODULEHANDLE"] = "#if " + winrtDefine + "\n\
#   define HAVE_GETMODULEHANDLE 1\n\
#else\n\
#   define HAVE_GETMODULEHANDLE 0\n\
#endif";
        replaceValues["HAVE_LOADLIBRARY"] = "#if " + winrtDefine + "\n\
#   define HAVE_LOADLIBRARY 1\n\
#else\n\
#   define HAVE_LOADLIBRARY 0\n\
#endif";
        replaceValues["HAVE_MAPVIEWOFFILE"] = "#if " + winrtDefine + "\n\
#   define HAVE_MAPVIEWOFFILE 1\n\
#else\n\
#   define HAVE_MAPVIEWOFFILE 0\n\
#endif";
        replaceValues["HAVE_SETCONSOLETEXTATTRIBUTE"] = "#if " + winrtDefine + "\n\
#   define HAVE_SETCONSOLETEXTATTRIBUTE 1\n\
#else\n\
#   define HAVE_SETCONSOLETEXTATTRIBUTE 0\n\
#endif";
        replaceValues["HAVE_SETCONSOLECTRLHANDLER"] = "#if " + winrtDefine + "\n\
#   define HAVE_SETCONSOLECTRLHANDLER 1\n\
#else\n\
#   define HAVE_SETCONSOLECTRLHANDLER 0\n\
#endif";
        replaceValues["HAVE_SETDLLDIRECTORY"] = "#if " + winrtDefine + "\n\
#   define HAVE_SETDLLDIRECTORY 1\n\
#else\n\
#   define HAVE_SETDLLDIRECTORY 0\n\
#endif";
        replaceValues["HAVE_VIRTUALALLOC"] = "#if " + winrtDefine + "\n\
#   define HAVE_VIRTUALALLOC 1\n\
#else\n\
#   define HAVE_VIRTUALALLOC 0\n\
#endif";

        auto opt = getConfigOptionPrefixed("CONFIG_AVISYNTH");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_AVISYNTH"] = "#if " + winrtDefine + "\n\
#   define CONFIG_AVISYNTH 1\n\
#else\n\
#   define CONFIG_AVISYNTH 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_LIBMFX");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_LIBMFX"] = "#if " + winrtDefine + "\n\
#   define CONFIG_LIBMFX 1\n\
#else\n\
#   define CONFIG_LIBMFX 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_AMF");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_AMF"] = "#if " + winrtDefine + "\n\
#   define CONFIG_AMF 1\n\
#else\n\
#   define CONFIG_AMF 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_CUDA");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_CUDA"] = "#if " + winrtDefine + "\n\
#   define CONFIG_CUDA 1\n\
#else\n\
#   define CONFIG_CUDA 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_CUVID");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_CUVID"] = "#if " + winrtDefine + "\n\
#   define CONFIG_CUVID 1\n\
#else\n\
#   define CONFIG_CUVID 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_DECKLINK");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_DECKLINK"] = "#if " + winrtDefine + "\n\
#   define CONFIG_DECKLINK 1\n\
#else\n\
#   define CONFIG_DECKLINK 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_DXVA2");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_DXVA2"] = "#if " + winrtDefine + "\n\
#   define CONFIG_DXVA2 1\n\
#else\n\
#   define CONFIG_DXVA2 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_FFNVCODEC");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_FFNVCODEC"] = "#if " + winrtDefine + "\n\
#   define CONFIG_FFNVCODEC 1\n\
#else\n\
#   define CONFIG_FFNVCODEC 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_NVDEC");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_NVDEC"] = "#if " + winrtDefine + "\n\
#   define CONFIG_NVDEC 1\n\
#else\n\
#   define CONFIG_NVDEC 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_NVENC");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_NVENC"] = "#if " + winrtDefine + "\n\
#   define CONFIG_NVENC 1\n\
#else\n\
#   define CONFIG_NVENC 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_SCHANNEL");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_SCHANNEL"] = "#if " + winrtDefine + "\n\
#   define CONFIG_SCHANNEL 1\n\
#else\n\
#   define CONFIG_SCHANNEL 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_DSHOW_INDEV");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_DSHOW_INDEV"] = "#if " + winrtDefine + "\n\
#   define CONFIG_DSHOW_INDEV 1\n\
#else\n\
#   define CONFIG_DSHOW_INDEV 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_GDIGRAB_INDEV");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_GDIGRAB_INDEV"] = "#if " + winrtDefine + "\n\
#   define CONFIG_GDIGRAB_INDEV 1\n\
#else\n\
#   define CONFIG_GDIGRAB_INDEV 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_VFWCAP_INDEV");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_VFWCAP_INDEV"] = "#if " + winrtDefine + "\n\
#   define CONFIG_VFWCAP_INDEV 1\n\
#else\n\
#   define CONFIG_VFWCAP_INDEV 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_OPENGL");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_OPENGL"] = "#if " + winrtDefine + "\n\
#   define CONFIG_OPENGL 1\n\
#else\n\
#   define CONFIG_OPENGL 0\n\
#endif";
        }
        opt = getConfigOptionPrefixed("CONFIG_OPENAL");
        if ((opt != m_configValues.end()) && opt->m_value == "1") {
            replaceValues["CONFIG_OPENAL"] = "#if " + winrtDefine + "\n\
#   define CONFIG_OPENAL 1\n\
#else\n\
#   define CONFIG_OPENAL 0\n\
#endif";
        }
    }

    // Build replace values for all x86 inline asm
    vector<string> inlineList;
    getConfigList("ARCH_EXT_LIST_X86", inlineList);
    for (auto& i : inlineList) {
        transform(i.begin(), i.end(), i.begin(), toupper);
        string name = "HAVE_" + i + "_INLINE";
        replaceValues[name] = "#define " + name + " ARCH_X86 && HAVE_INLINE_ASM";
    }

    // Sanity checks for inline asm (Needed as some code only checks availability and not inline_asm)
    replaceValues["HAVE_EBP_AVAILABLE"] = "#if HAVE_INLINE_ASM && !defined(_DEBUG)\n\
#   define HAVE_EBP_AVAILABLE 1\n\
#else\n\
#   define HAVE_EBP_AVAILABLE 0\n\
#endif";
    replaceValues["HAVE_EBX_AVAILABLE"] = "#if HAVE_INLINE_ASM && !defined(_DEBUG)\n\
#   define HAVE_EBX_AVAILABLE 1\n\
#else\n\
#   define HAVE_EBX_AVAILABLE 0\n\
#endif";

    // Add any values that may depend on a replace value from above^
    DefaultValuesList newReplaceValues;
    string searchSuffix[] = {"_deps", "_select", "_deps_any"};
    for (const auto& i : m_configValues) {
        string tagName = i.m_prefix + i.m_option;
        // Check for forced replacement (only if attribute is not disabled)
        if ((i.m_value != "0") && (replaceValues.find(tagName) != replaceValues.end())) {
            // Already exists in list so can skip
            continue;
        }
        if (i.m_value == "1") {
            // Check if it depends on a replace value
            string optionLower = i.m_option;
            transform(optionLower.begin(), optionLower.end(), optionLower.begin(), tolower);
            for (const auto& suff : searchSuffix) {
                string checkFunc = optionLower + suff;
                vector<string> checkList;
                if (getConfigList(checkFunc, checkList, false)) {
                    string addConfig;
                    bool reservedDeps = false;
                    checkList.reserve(checkList.size() + 100); // Prevent errors when adding to list below
                    for (auto j = checkList.begin(); j < checkList.end(); ++j) {
                        // Check if this is a not !
                        bool toggle = false;
                        if (j->at(0) == '!') {
                            j->erase(0, 1);
                            toggle = true;
                        }
                        auto temp = getConfigOption(*j);
                        if (temp != m_configValues.end()) {
                            string replaceCheck = temp->m_prefix + temp->m_option;
                            transform(replaceCheck.begin(), replaceCheck.end(), replaceCheck.begin(), toupper);
                            auto dep = replaceValues.find(replaceCheck);
                            if (dep != replaceValues.end()) {
                                if (addConfig.length() == 0) {
                                    addConfig += replaceCheck;
                                } else {
                                    addConfig += " && " + replaceCheck;
                                    addConfig = '(' + addConfig + ')';
                                }
                                if (toggle) {
                                    addConfig = '!' + addConfig;
                                }
                                reservedDeps = true;
                            } else if (toggle ^ (temp->m_value == "1")) {
                                // Check recursively if dep has any deps that are reserved types
                                string optionLower2 = temp->m_option;
                                transform(optionLower2.begin(), optionLower2.end(), optionLower2.begin(), tolower);
                                for (const auto& suff2 : searchSuffix) {
                                    checkFunc = optionLower2 + suff2;
                                    vector<string> checkList2;
                                    if (getConfigList(checkFunc, checkList2, false)) {
                                        uint cPos = j - checkList.begin();
                                        // Check if not already in list
                                        for (auto& k : checkList2) {
                                            // Check if this is a not !
                                            bool toggle2 = toggle;
                                            if (k.at(0) == '!') {
                                                k.erase(0, 1);
                                                toggle2 = !toggle2;
                                            }
                                            string checkVal = k;
                                            if (toggle2) {
                                                checkVal = '!' + checkVal;
                                            }
                                            if (find(checkList.begin(), checkList.end(), checkVal) == checkList.end()) {
                                                checkList.push_back(checkVal);
                                            }
                                            // update iterator position
                                            j = checkList.begin() + cPos;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (reservedDeps) {
                        // Add to list
                        newReplaceValues[tagName] = "#if " + addConfig + "\n\
#   define " + tagName + " 1\n\
#else\n\
#   define " + tagName + " 0\n\
#endif";
                    }
                }
            }
        }
    }
    for (auto& newReplaceValue : newReplaceValues) {
        // Add them to the returned list (done here so that any checks above that test if it is reserved only
        // operate on the unmodified original list)
        replaceValues[newReplaceValue.first] = newReplaceValue.second;
    }

    // Add to config.asm only list
    if (m_useNASM) {
        replaceValuesASM["ARCH_X86_32"] = "%if __BITS__ = 64\n\
%define ARCH_X86_32 0\n\
%elif __BITS__ = 32\n\
%define ARCH_X86_32 1\n\
%define PREFIX\n\
%endif";
        replaceValuesASM["ARCH_X86_64"] = "%if __BITS__ = 64\n\
%define ARCH_X86_64 1\n\
%elif __BITS__ = 32\n\
%define ARCH_X86_64 0\n\
%endif";
        replaceValuesASM["HAVE_ALIGNED_STACK"] = "%if __BITS__ = 64\n\
%define HAVE_ALIGNED_STACK 1\n\
%elif __BITS__ = 32\n\
%define HAVE_ALIGNED_STACK 0\n\
%endif";
        replaceValuesASM["HAVE_FAST_64BIT"] = "%if __BITS__ = 64\n\
%define HAVE_FAST_64BIT 1\n\
%elif __BITS__ = 32\n\
%define HAVE_FAST_64BIT 0\n\
%endif";
    } else {
        replaceValuesASM["ARCH_X86_32"] = "%ifidn __OUTPUT_FORMAT__,x64\n\
%define ARCH_X86_32 0\n\
%elifidn __OUTPUT_FORMAT__,win64\n\
%define ARCH_X86_32 0\n\
%elifidn __OUTPUT_FORMAT__,win32\n\
%define ARCH_X86_32 1\n\
%define PREFIX\n\
%endif";
        replaceValuesASM["ARCH_X86_64"] = "%ifidn __OUTPUT_FORMAT__,x64\n\
%define ARCH_X86_64 1\n\
%elifidn __OUTPUT_FORMAT__,win64\n\
%define ARCH_X86_64 1\n\
%elifidn __OUTPUT_FORMAT__,win32\n\
%define ARCH_X86_64 0\n\
%endif";
        replaceValuesASM["HAVE_ALIGNED_STACK"] = "%ifidn __OUTPUT_FORMAT__,x64\n\
%define HAVE_ALIGNED_STACK 1\n\
%elifidn __OUTPUT_FORMAT__,win64\n\
%define HAVE_ALIGNED_STACK 1\n\
%elifidn __OUTPUT_FORMAT__,win32\n\
%define HAVE_ALIGNED_STACK 0\n\
%endif";
        replaceValuesASM["HAVE_FAST_64BIT"] = "%ifidn __OUTPUT_FORMAT__,x64\n\
%define HAVE_FAST_64BIT 1\n\
%elifidn __OUTPUT_FORMAT__,win64\n\
%define HAVE_FAST_64BIT 1\n\
%elifidn __OUTPUT_FORMAT__,win32\n\
%define HAVE_FAST_64BIT 0\n\
%endif";
    }
}

void ConfigGenerator::buildReservedValues(vector<string>& reservedItems)
{
    reservedItems.resize(0);
    // The following are reserved values that are automatically handled and can not be set explicitly
    reservedItems.emplace_back("x86_32");
    reservedItems.emplace_back("x86_64");
    reservedItems.emplace_back("xmm_clobbers");
    reservedItems.emplace_back("shared");
    reservedItems.emplace_back("static");
    reservedItems.emplace_back("aligned_stack");
    reservedItems.emplace_back("fast_64bit");
    reservedItems.emplace_back("mm_empty");
    reservedItems.emplace_back("ebp_available");
    reservedItems.emplace_back("ebx_available");
    reservedItems.emplace_back("debug");
    reservedItems.emplace_back("hardcoded_tables"); // Not supported
    reservedItems.emplace_back("small");
    reservedItems.emplace_back("lto");
    reservedItems.emplace_back("pic");
    reservedItems.emplace_back("uwp");
    reservedItems.emplace_back("winrt");
}

void ConfigGenerator::buildAdditionalDependencies(DependencyList& additionalDependencies)
{
    additionalDependencies.clear();
    additionalDependencies["android"] = false;
    additionalDependencies["capCreateCaptureWindow"] = true;
    additionalDependencies["const_nan"] = true;
    additionalDependencies["CreateDIBSection"] = true;
    additionalDependencies["dv1394"] = false;
    additionalDependencies["DXVA_PicParams_AV1"] = true; // Technically these require a compatible win sdk version
    // but we check for that in the replace values that use these builtins
    additionalDependencies["DXVA_PicParams_HEVC"] = true;
    additionalDependencies["DXVA_PicParams_VP9"] = true;
    additionalDependencies["dxva2api_h"] = true;
    additionalDependencies["fork"] = false;
    additionalDependencies["jack_jack_h"] = false;
    additionalDependencies["IBaseFilter"] = true;
    additionalDependencies["ID3D11VideoDecoder"] = true;
    additionalDependencies["ID3D11VideoContext"] = true;
    additionalDependencies["libcrystalhd_libcrystalhd_if_h"] = false;
    additionalDependencies["linux_fb_h"] = false;
    additionalDependencies["linux_videodev_h"] = false;
    additionalDependencies["linux_videodev2_h"] = false;
    additionalDependencies["LoadLibrary"] = true;
    additionalDependencies["MFCreateAlignedMemoryBuffer"] = true;
    additionalDependencies["mftransform_h"] = true;
    additionalDependencies["parisc64"] = false;
    additionalDependencies["DXVA2_ConfigPictureDecode"] = true;
    additionalDependencies["snd_pcm_htimestamp"] = false;
    string temp;
    additionalDependencies["stdatomic"] = findFile(m_rootDirectory + "compat/atomics/win32/stdatomic.h", temp);
    additionalDependencies["va_va_h"] = false;
    additionalDependencies["vdpau_vdpau_h"] = false;
    additionalDependencies["vdpau_vdpau_x11_h"] = false;
    additionalDependencies["vfw32"] = true;
    additionalDependencies["vfwcap_defines"] = true;
    additionalDependencies["VADecPictureParameterBufferAV1_bit_depth_idx"] = false;
    additionalDependencies["VideoDecodeAcceleration_VDADecoder_h"] = false;
    additionalDependencies["X11_extensions_Xvlib_h"] = false;
    additionalDependencies["X11_extensions_XvMClib_h"] = false;
    additionalDependencies["x264_csp_bgr"] = isConfigOptionEnabled("libx264");
    bool bCuvid = isConfigOptionEnabled("cuvid");
    additionalDependencies["CUVIDAV1PICPARAMS"] = bCuvid;
    additionalDependencies["CUVIDH264PICPARAMS"] = bCuvid;
    additionalDependencies["CUVIDHEVCPICPARAMS"] = bCuvid;
    additionalDependencies["CUVIDVC1PICPARAMS"] = bCuvid;
    additionalDependencies["CUVIDVP9PICPARAMS"] = bCuvid;
    additionalDependencies["VAEncPictureParameterBufferH264"] = false;
    additionalDependencies["videotoolbox_encoder"] = false;
    additionalDependencies["VAEncPictureParameterBufferHEVC"] = false;
    additionalDependencies["VAEncPictureParameterBufferJPEG"] = false;
    additionalDependencies["VAEncPictureParameterBufferMPEG2"] = false;
    additionalDependencies["VAEncPictureParameterBufferVP8"] = false;
    additionalDependencies["VAEncPictureParameterBufferVP9"] = false;
    additionalDependencies["ole32"] = true;
    additionalDependencies["shell32"] = true;
    additionalDependencies["wincrypt"] = true;
    additionalDependencies["psapi"] = true;
    additionalDependencies["user32"] = true;
    additionalDependencies["qtkit"] = false;
    additionalDependencies["coreservices"] = false;
    additionalDependencies["corefoundation"] = false;
    additionalDependencies["corevideo"] = false;
    additionalDependencies["coremedia"] = false;
    additionalDependencies["coregraphics"] = false;
    additionalDependencies["applicationservices"] = false;
    additionalDependencies["libdl"] = false;
    additionalDependencies["libm"] = false;
    additionalDependencies["libvorbisenc"] = isConfigOptionEnabled("libvorbis");
    if (!isConfigOptionValid("atomics_native")) {
        additionalDependencies["atomics_native"] = true;
    }
    additionalDependencies["MFX_CODEC_VP9"] = isConfigOptionEnabled("libmfx");
}

void ConfigGenerator::buildInterDependencies(InterDependencies& interDependencies)
{
    // Dynamically scan the configureFile for prepend {component}_deps and add
    vector<string> libraries;
    if (!getConfigList("LIBRARY_LIST", libraries)) {
        return;
    }
    for (const auto& i : libraries) {
        string prependString = "prepend " + i + "_deps";
        uint prependPos = m_configureFile.find(prependString);
        while (prependPos != string::npos) {
            const uint endPos = m_configureFile.rfind("&&", prependPos);
            const uint startPos = m_configureFile.rfind('\n', endPos) + 1;
            string enable = m_configureFile.substr(startPos, endPos - startPos);
            // Get enabled flags
            vector<string> required;
            uint enabled = enable.find("enabled ");
            while (enabled != string::npos) {
                const uint enableStart = enable.find_first_not_of(' ', enabled + 8);
                const uint cutPos = enable.find(' ', enableStart + 1);
                required.emplace_back(enable.substr(enableStart, cutPos - enableStart));
                // Get next
                enabled = enable.find("enabled ", enabled + 8);
            }

            // Get dependencies
            const uint prependStart = m_configureFile.find('"', prependPos + prependString.length()) + 1;
            const uint prependEnd = m_configureFile.find('"', prependStart);
            string prepends = m_configureFile.substr(prependStart, prependEnd - prependStart);
            vector<string> depends;
            uint cutPos = 0;
            do {
                cutPos = prepends.find_first_not_of(' ', cutPos);
                const uint cutPos2 = prepends.find(' ', cutPos);
                depends.emplace_back(prepends.substr(cutPos, cutPos2 - cutPos));
                cutPos = cutPos2;
            } while (cutPos != string::npos);

            // Add to list
            interDependencies[i].emplace_back(make_pair(required, depends));

            // Get next
            prependPos = m_configureFile.find(prependString, prependEnd + 1);
        }
    }
}

void ConfigGenerator::buildOptimisedDisables(ConfigList& optimisedDisables)
{
    // This used is to return prioritised version of different config options
    //  For instance If enabling the decoder from an passed in library that is better than the inbuilt one
    //  then simply disable the inbuilt so as to avoid unnecessary compilation

    optimisedDisables.clear();
    // From trac.ffmpeg.org/wiki/GuidelinesHighQualityAudio
    // Dolby Digital: ac3
    // Dolby Digital Plus: eac3
    // MP2: libtwolame, mp2
    // Windows Media Audio 1: wmav1
    // Windows Media Audio 2: wmav2
    // LC-AAC: libfdk_aac, libfaac, aac, libvo_aacenc
    // HE-AAC: libfdk_aac, libaacplus
    // Vorbis: libvorbis, vorbis
    // MP3: libmp3lame, libshine
    // Opus: libopus
    // libopus >= libvorbis >= libfdk_aac > libmp3lame > libfaac >= eac3/ac3 > aac > libtwolame > vorbis > mp2 >
    // wmav2/wmav1 > libvo_aacenc

#ifdef OPTIMISE_ENCODERS
    optimisedDisables["LIBTWOLAME_ENCODER"].push_back("MP2_ENCODER");
    optimisedDisables["LIBFDK_AAC_ENCODER"].push_back("LIBFAAC_ENCODER");
    optimisedDisables["LIBFDK_AAC_ENCODER"].push_back("AAC_ENCODER");
    optimisedDisables["LIBFDK_AAC_ENCODER"].push_back("LIBVO_AACENC_ENCODER");
    optimisedDisables["LIBFDK_AAC_ENCODER"].push_back("LIBAACPLUS_ENCODER");
    optimisedDisables["LIBFAAC_ENCODER"].push_back("AAC_ENCODER");
    optimisedDisables["LIBFAAC_ENCODER"].push_back("LIBVO_AACENC_ENCODER");
    optimisedDisables["AAC_ENCODER"].push_back("LIBVO_AACENC_ENCODER");
    optimisedDisables["LIBVORBIS_ENCODER"].push_back("VORBIS_ENCODER");
    optimisedDisables["LIBMP3LAME_ENCODER"].push_back("LIBSHINE_ENCODER");
    optimisedDisables["LIBOPENJPEG_ENCODER"].push_back("JPEG2000_ENCODER"); //???
    optimisedDisables["LIBUTVIDEO_ENCODER"].push_back("UTVIDEO_ENCODER");   //???
    optimisedDisables["LIBWAVPACK_ENCODER"].push_back("WAVPACK_ENCODER");   //???
#endif

#ifdef OPTIMISE_DECODERS
    optimisedDisables["LIBGSM_DECODER"].push_back("GSM_DECODER");       //???
    optimisedDisables["LIBGSM_MS_DECODER"].push_back("GSM_MS_DECODER"); //???
    optimisedDisables["LIBNUT_MUXER"].push_back("NUT_MUXER");
    optimisedDisables["LIBNUT_DEMUXER"].push_back("NUT_DEMUXER");
    optimisedDisables["LIBOPENCORE_AMRNB_DECODER"].push_back("AMRNB_DECODER"); //???
    optimisedDisables["LIBOPENCORE_AMRWB_DECODER"].push_back("AMRWB_DECODER"); //???
    optimisedDisables["LIBOPENJPEG_DECODER"].push_back("JPEG2000_DECODER");    //???
    optimisedDisables["LIBSCHROEDINGER_DECODER"].push_back("DIRAC_DECODER");
    optimisedDisables["LIBUTVIDEO_DECODER"].push_back("UTVIDEO_DECODER"); //???
    optimisedDisables["VP8_DECODER"].push_back("LIBVPX_VP8_DECODER");     // Inbuilt native decoder is apparently faster
    optimisedDisables["VP9_DECODER"].push_back("LIBVPX_VP9_DECODER");
    optimisedDisables["OPUS_DECODER"].push_back("LIBOPUS_DECODER"); //??? Not sure which is better
#endif
}

#define CHECKFORCEDENABLES(Opt)                             \
    {                                                       \
        if (getConfigOption(Opt) != m_configValues.end()) { \
            forceEnable.emplace_back(Opt);                  \
        }                                                   \
    }

void ConfigGenerator::buildForcedEnables(const string& optionLower, vector<string>& forceEnable)
{
    if (optionLower == "fontconfig") {
        CHECKFORCEDENABLES("libfontconfig");
    } else if (optionLower == "dxva2") {
        CHECKFORCEDENABLES("dxva2_lib");
    } else if (optionLower == "libcdio") {
        CHECKFORCEDENABLES("cdio_paranoia_paranoia_h");
    } else if (optionLower == "libmfx") {
        CHECKFORCEDENABLES("qsv");
    } else if (optionLower == "dcadec") {
        CHECKFORCEDENABLES("struct_dcadec_exss_info_matrix_encoding");
    } else if (optionLower == "sdl") {
        fastToggleConfigValue("sdl2", true); // must use fastToggle to prevent infinite cycle
    } else if (optionLower == "sdl2") {
        fastToggleConfigValue("sdl", true);
    } else if (optionLower == "libvorbis") {
        CHECKFORCEDENABLES("libvorbisenc");
    } else if (optionLower == "opencl") {
        CHECKFORCEDENABLES("opencl_d3d11");
        CHECKFORCEDENABLES("opencl_dxva2");
    } else if (optionLower == "ffnvcodec") {
        CHECKFORCEDENABLES("cuda");
    } else if (optionLower == "cuda") {
        CHECKFORCEDENABLES("ffnvcodec");
    } else if (optionLower == "winrt") {
        fastToggleConfigValue("uwp", true); // must use fastToggle to prevent infinite cycle
    } else if (optionLower == "uwp") {
        fastToggleConfigValue("winrt", true);
    } else if (optionLower == "threads") {
        CHECKFORCEDENABLES("w32threads");
    } else if (optionLower == "w32threads") {
        CHECKFORCEDENABLES("threads");
    }
}

void ConfigGenerator::buildForcedDisables(const string& optionLower, vector<string>& forceDisable)
{
    if (optionLower == "sdl") {
        fastToggleConfigValue("sdl2", false); // must use fastToggle to prevent infinite cycle
    } else if (optionLower == "sdl2") {
        fastToggleConfigValue("sdl", false);
    } else if (optionLower == "winrt") {
        fastToggleConfigValue("uwp", false);
    } else if (optionLower == "uwp") {
        fastToggleConfigValue("winrt", false);
    } else {
        // Currently disable values are exact opposite of the corresponding enable ones
        buildForcedEnables(optionLower, forceDisable);
    }
}

void ConfigGenerator::buildEarlyConfigArgs(vector<string>& earlyArgs)
{
    earlyArgs.resize(0);
    earlyArgs.emplace_back("--rootdir");
    earlyArgs.emplace_back("--projdir");
    earlyArgs.emplace_back("--prefix");
    earlyArgs.emplace_back("--loud");
    earlyArgs.emplace_back("--quiet");
    earlyArgs.emplace_back("--autodetect");
    earlyArgs.emplace_back("--use-yasm");
}

void ConfigGenerator::buildObjects(const string& tag, vector<string>& objects)
{
    if (tag == "COMPAT_OBJS") {
        objects.emplace_back(
            "msvcrt/snprintf");         // msvc only provides _snprintf which does not conform to snprintf standard
        objects.emplace_back("strtod"); // msvc contains a strtod but it does not handle NaN's correctly
        objects.emplace_back("getopt");
    } else if (tag == "EMMS_OBJS__yes_") {
        if (isConfigOptionEnabled("MMX_EXTERNAL")) {
            objects.emplace_back("x86/emms"); // asm emms is not required in 32b but is for 64bit unless with icl
        }
    }
}
