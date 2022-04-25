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
#include "configGenerator.h"

#include <algorithm>
#include <fstream>
#include <regex>

ConfigGenerator::ConfigGenerator()
    : m_projectName("FFMPEG")
{}

bool ConfigGenerator::passConfig(const int argc, char** argv)
{
    // Check for initial input arguments
    vector<string> earlyArgs;
    buildEarlyConfigArgs(earlyArgs);
    for (int i = 1; i < argc; i++) {
        string option = string(argv[i]);
        string command = option;
        const uint pos = option.find('=');
        if (pos != string::npos) {
            command = option.substr(0, pos);
        }
        if (find(earlyArgs.begin(), earlyArgs.end(), command) != earlyArgs.end()) {
            if (!changeConfig(option)) {
                return false;
            }
        }
    }
    if (!passConfigureFile()) {
        return false;
    }
    // Load with default values
    if (!buildDefaultValues()) {
        return false;
    }
    // Pass input arguments
    for (int i = 1; i < argc; i++) {
        // Check that option hasn't already been processed
        string option = string(argv[i]);
        if (find(earlyArgs.begin(), earlyArgs.end(), option) == earlyArgs.end()) {
            if (!changeConfig(option)) {
                return false;
            }
        }
    }
    // Ensure forced values
    if (!buildForcedValues()) {
        return false;
    }
    if (!buildAutoDetectValues()) {
        return false;
    }
    // Perform validation of values
    if (!passCurrentValues()) {
        return false;
    }
    // Super ensure forced values
    buildForcedValues();
    return true;
}

bool ConfigGenerator::passConfigureFile()
{
    // Generate a new config file by scanning existing build chain files
    outputLine("  Passing configure file...");

    // Setup initial directories
    if (m_rootDirectory.length() == 0) {
        // Search paths starting in current directory then checking parents
        string pathList[] = {"./", "../", "./ffmpeg/", "../ffmpeg/", "../../ffmpeg/", "../../../", "../../", "./libav/",
            "../libav/", "../../libav/"};
        uint pathCount = 0;
        const uint numPaths = sizeof(pathList) / sizeof(string);
        for (; pathCount < numPaths; pathCount++) {
            m_rootDirectory = pathList[pathCount];
            string configFile = m_rootDirectory + "configure";
            if (loadFromFile(configFile, m_configureFile, false, false)) {
                break;
            }
        }
        if (pathCount == numPaths) {
            outputError("Failed to find a 'configure' file");
            return false;
        }
    } else {
        // Open configure file
        const string configFile = m_rootDirectory + "configure";
        if (!loadFromFile(configFile, m_configureFile, false, false)) {
            outputError("Failed to find a 'configure' file in specified root directory");
            return false;
        }
    }

    // Search for start of config.h file parameters
    uint startPos = m_configureFile.find("#define FFMPEG_CONFIG_H");
    if (startPos == string::npos) {
        // Check if this is instead a libav configure
        startPos = m_configureFile.find("#define LIBAV_CONFIG_H");
        if (startPos == string::npos) {
            outputError("Failed finding config.h start parameters");
            return false;
        }
        m_isLibav = true;
        m_projectName = "LIBAV";
    }
    // Move to end of header guard
    startPos += 23 - static_cast<uint>(m_isLibav);

    // Build default value list
    DefaultValuesList defaultValues;
    buildFixedValues(defaultValues);

    for (uint searches = 0; searches < 2; ++searches) {
        // Get each defined option till EOF
        uint configEnd = m_configureFile.find("EOF", startPos + 1);
        startPos = m_configureFile.find("#define", startPos + 1);
        if (configEnd == string::npos) {
            outputError("Failed finding config.h parameters end");
            return false;
        }
        uint endPos = configEnd;
        while ((startPos != string::npos) && (startPos < configEnd)) {
            // Skip white space
            startPos = m_configureFile.find_first_not_of(g_whiteSpace, startPos + 7);
            // Get first string
            endPos = m_configureFile.find_first_of(g_whiteSpace, startPos + 1);
            const string configName = m_configureFile.substr(startPos, endPos - startPos);
            // Get second string
            startPos = m_configureFile.find_first_not_of(g_whiteSpace, endPos + 1);
            endPos = m_configureFile.find_first_of(g_whiteSpace, startPos + 1);
            string configValue = m_configureFile.substr(startPos, endPos - startPos);
            // Check if the value is a variable
            const uint startPos2 = configValue.find('$');
            if (startPos2 != string::npos) {
                // Check if it is a function call
                if (configValue.at(startPos2 + 1) == '(') {
                    endPos = m_configureFile.find(')', startPos);
                    configValue = m_configureFile.substr(startPos, endPos - startPos + 1);
                }
                // Remove any quotes from the tag if there are any
                const uint endPos2 =
                    (configValue.at(configValue.length() - 1) == '"') ? configValue.length() - 1 : configValue.length();
                // Find and replace the value
                auto val = defaultValues.find(configValue.substr(startPos2, endPos2 - startPos2));
                if (val == defaultValues.end()) {
                    outputError("Unknown configuration operation found (" +
                        configValue.substr(startPos2, endPos2 - startPos2) + ")");
                    return false;
                }
                // Check if we need to add the quotes back
                if (configValue.at(0) == '"') {
                    // Replace the value with the default option in quotations
                    configValue = '"' + val->second + '"';
                } else {
                    // Replace the value with the default option
                    configValue = val->second;
                }
            }

            // Add to the list
            m_fixedConfigValues.push_back(ConfigPair(configName, "", configValue));

            // Find next
            startPos = m_configureFile.find("#define", endPos + 1);
        }

        // Find the end of this section
        configEnd = m_configureFile.find("#endif", configEnd + 1);
        if (configEnd == string::npos) {
            outputError("Failed finding config.h header end");
            return false;
        }

        // Get the additional config values
        startPos = m_configureFile.find("print_config", endPos + 3);
        while ((startPos != string::npos) && (startPos < configEnd)) {
            // Add these to the config list
            // Find prefix
            startPos = m_configureFile.find_first_not_of(g_whiteSpace, startPos + 12);
            endPos = m_configureFile.find_first_of(g_whiteSpace, startPos + 1);
            string prefix = m_configureFile.substr(startPos, endPos - startPos);
            // Skip unneeded var
            startPos = m_configureFile.find_first_not_of(g_whiteSpace, endPos + 1);
            endPos = m_configureFile.find_first_of(g_whiteSpace, startPos + 1);

            // Find option list
            startPos = m_configureFile.find_first_not_of(g_whiteSpace, endPos + 1);
            endPos = m_configureFile.find_first_of(g_whiteSpace, startPos + 1);
            string sList = m_configureFile.substr(startPos, endPos - startPos);
            // Strip the variable prefix from start
            sList.erase(0, 1);

            // Create option list
            if (!passConfigList(prefix, "", sList)) {
                return false;
            }

            // Check if multiple lines
            endPos = m_configureFile.find_first_not_of(g_whiteSpace, endPos + 1);
            while (m_configureFile.at(endPos) == '\\') {
                // Skip newline
                ++endPos;
                startPos = m_configureFile.find_first_not_of(" \t", endPos + 1);
                // Check for blank line
                if (m_configureFile.at(startPos) == '\n') {
                    break;
                }
                endPos = m_configureFile.find_first_of(g_whiteSpace, startPos + 1);
                string list = m_configureFile.substr(startPos, endPos - startPos);
                // Strip the variable prefix from start
                list.erase(0, 1);

                // Create option list
                if (!passConfigList(prefix, "", list)) {
                    return false;
                }
                endPos = m_configureFile.find_first_not_of(g_whiteSpace, endPos + 1);
            }

            // Get next
            startPos = m_configureFile.find("print_config", startPos + 1);
        }

        if (searches == 0) {
            // Search for newer config_components.h
            startPos = m_configureFile.find("#define FFMPEG_CONFIG_COMPONENTS_H");
            if (startPos == string::npos) {
                break;
            }
            // Move to end of header guard
            startPos += 34;
            // Set start of component section
            m_configComponentsStart = m_configValues.size();
        }
    }
    // Mark the end of the config list. Any elements added after this are considered temporary and should not be
    // exported
    m_configValuesEnd = m_configValues.size(); // must be uint in case of realloc
    return true;
}

bool ConfigGenerator::passExistingConfig()
{
    outputLine("  Passing in existing config.h file...");
    // load in config.h from root dir
    string configH;
    const string configFile = m_rootDirectory + "config.h";
    if (!loadFromFile(configFile, configH, false, false)) {
        outputError("Failed opening existing config.h file.");
        outputError("Ensure the requested config.h file is found in the source codes root directory.", false);
        outputError("Or omit the --use-existing-config option." + configFile, false);
        return false;
    }

    // Find the first valid configuration option
    uint pos = -1;
    const string configTags[] = {"ARCH_", "HAVE_", "CONFIG_"};
    for (const auto& configTag : configTags) {
        string search = "#define " + configTag;
        uint pos2 = configH.find(search);
        pos = (pos2 < pos) ? pos2 : pos;
    }

    // Loop through each #define tag val and set internal option to val
    while (pos != string::npos) {
        pos = configH.find_first_not_of(g_whiteSpace, pos + 7);
        // Get the tag
        uint pos2 = configH.find_first_of(g_whiteSpace, pos + 1);
        string option = configH.substr(pos, pos2 - pos);

        // Check if the options is valid
        if (!isConfigOptionValidPrefixed(option)) {
            // Check if it is a fixed value and skip
            bool found = false;
            for (const auto& i : m_fixedConfigValues) {
                if (i.m_option == option) {
                    found = true;
                    break;
                }
            }
            if (found) {
                // Get next
                pos = configH.find("#define ", pos2 + 1);
                continue;
            }
            outputInfo("Unknown config option (" + option + ") found in config.h file.");
            return false;
        }

        // Get the value
        pos = configH.find_first_not_of(g_whiteSpace, pos2 + 1);
        pos2 = configH.find_first_of(g_whiteSpace, pos + 1);
        string sValue = configH.substr(pos, pos2 - pos);
        const bool enable = (sValue == "1");
        if (!enable && (sValue != "0")) {
            outputError("Invalid config value (" + sValue + ") for option (" + option + ") found in config.h file.");
            return false;
        }

        // Update intern value
        toggleConfigValue(option, enable);

        // Get next
        pos = configH.find("#define ", pos2 + 1);
    }
    return true;
}

bool ConfigGenerator::changeConfig(const string& option)
{
    if (option == "--help") {
        uint start = m_configureFile.find("show_help(){");
        if (start == string::npos) {
            outputError("Failed finding help list in config file");
            return false;
        }
        // Find first 'EOF'
        start = m_configureFile.find("EOF", start) + 2;
        if (start == string::npos) {
            outputError("Incompatible help list in config file");
            return false;
        }
        uint end = m_configureFile.find("EOF", start);
        string helpOptions = m_configureFile.substr(start, end - start);
        // Search through help options and remove any values not supported
        string removeSections[] = {"Standard options:", "Documentation options:", "Toolchain options:",
            "Advanced options (experts only):", "Developer options (useful when working on FFmpeg itself):", "NOTE:"};
        for (const string& section : removeSections) {
            start = helpOptions.find(section);
            if (start != string::npos) {
                end = helpOptions.find("\n\n", start + section.length() + 1);
                helpOptions = helpOptions.erase(start, end - start + 2);
            }
        }
        outputLine(helpOptions);
        // Add in custom standard string
        outputLine("Standard options:");
        outputLine("  --prefix=PREFIX          install in PREFIX [../../../msvc/]");
        // outputLine("  --bindir=DIR             install binaries in DIR [PREFIX/bin]");
        // outputLine("  --libdir=DIR             install libs in DIR [PREFIX/lib]");
        // outputLine("  --incdir=DIR             install includes in DIR [PREFIX/include]");
        outputLine("  --rootdir=DIR            location of source configure file [auto]");
        outputLine("  --projdir=DIR            location of output project files [ROOT/SMP]");
        outputLine(
            "  --use-existing-config    use an existing config.h file found in rootdir, ignoring any other passed parameters affecting config");
        // Add in custom toolchain string
        outputLine("Toolchain options:");
        outputLine("  --dce-only               do not output a project and only generate missing DCE files");
        outputLine(
            "  --use-yasm               use YASM instead of the default NASM (this is not advised as it does not support newer instructions)");
        // Add in reserved values
        vector<string> reservedItems;
        buildReservedValues(reservedItems);
        outputLine("\nReserved options (auto handled and cannot be set explicitly):");
        for (const string& resVal : reservedItems) {
            outputLine("  " + resVal);
        }
        return false;
    }
    if (option.find("--prefix") == 0) {
        // Check for correct command syntax
        if (option.at(8) != '=') {
            outputError("Incorrect prefix syntax (" + option + ")");
            outputError("Excepted syntax (--prefix=PREFIX)", false);
            return false;
        }
        // A output dir has been specified
        string value = option.substr(9);
        m_outDirectory = value;
        // Convert '\' to '/'
        replace(m_outDirectory.begin(), m_outDirectory.end(), '\\', '/');
        // Check if a directory has been passed
        if (m_outDirectory.length() == 0) {
            m_outDirectory = "./";
        }
        // Check if directory has trailing '/'
        if (m_outDirectory.back() != '/') {
            m_outDirectory += '/';
        }
    } else if (option.find("--rootdir") == 0) {
        // Check for correct command syntax
        if (option.at(9) != '=') {
            outputError("Incorrect rootdir syntax (" + option + ")");
            outputError("Excepted syntax (--rootdir=DIR)", false);
            return false;
        }
        // A source dir has been specified
        string value = option.substr(10);
        m_rootDirectory = value;
        // Convert '\' to '/'
        replace(m_rootDirectory.begin(), m_rootDirectory.end(), '\\', '/');
        // Check if a directory has been passed
        if (m_rootDirectory.length() == 0) {
            m_rootDirectory = "./";
        }
        // Check if directory has trailing '/'
        if (m_rootDirectory.back() != '/') {
            m_rootDirectory += '/';
        }
        // rootdir is passed before all other options are set up so must skip any other remaining steps
        return true;
    } else if (option.find("--projdir") == 0) {
        // Check for correct command syntax
        if (option.at(9) != '=') {
            outputError("Incorrect projdir syntax (" + option + ")");
            outputError("Excepted syntax (--projdir=DIR)", false);
            return false;
        }
        // A project dir has been specified
        string value = option.substr(10);
        m_solutionDirectory = value;
        // Convert '\' to '/'
        replace(m_solutionDirectory.begin(), m_solutionDirectory.end(), '\\', '/');
        // Check if a directory has been passed
        if (m_solutionDirectory.length() == 0) {
            m_solutionDirectory = "./";
        }
        // Check if directory has trailing '/'
        if (m_solutionDirectory.back() != '/') {
            m_solutionDirectory += '/';
        }
    } else if (option == "--dce-only") {
        // This has no parameters and just sets internal value
        m_onlyDCE = true;
    } else if (option == "--use-yasm") {
        // This has no parameters and just sets internal value
        m_useNASM = false;
    } else if (option.find("--use-existing-config") == 0) {
        // A input config file has been specified
        m_usingExistingConfig = true;
    } else if (option.find("--list-") == 0) {
        string option2 = option.substr(7);
        string optionList = option2;
        if (optionList.back() == 's') {
            optionList = optionList.substr(0, optionList.length() - 1); // Remove the trailing s
        }
        transform(optionList.begin(), optionList.end(), optionList.begin(), ::toupper);
        optionList += "_LIST";
        vector<string> list;
        if (!getConfigList(optionList, list)) {
            outputError("Unknown list option (" + option2 + ")");
            outputError("Use --help to get available options", false);
            return false;
        }
        outputLine(option2 + ": ");
        for (auto& i : list) {
            // cut off any trailing type
            uint pos = i.rfind('_');
            if (pos != string::npos) {
                i = i.substr(0, pos);
            }
            transform(i.begin(), i.end(), i.begin(), ::tolower);
            outputLine("  " + i);
        }
        return false;
    } else if (option.find("--quiet") == 0) {
        setOutputVerbosity(VERBOSITY_ERROR);
    } else if (option.find("--loud") == 0) {
        setOutputVerbosity(VERBOSITY_INFO);
    } else {
        bool enable;
        string option2;
        if (option.find("--enable-") == 0) {
            enable = true;
            // Find remainder of option
            option2 = option.substr(9);
        } else if (option.find("--disable-") == 0) {
            enable = false;
            // Find remainder of option
            option2 = option.substr(10);
        } else {
            outputError("Unknown command line option (" + option + ")");
            outputError("Use --help to get available options", false);
            return false;
        }

        // Replace any '-'s with '_'
        replace(option2.begin(), option2.end(), '-', '_');
        // Check and make sure that a reserved item is not being changed
        vector<string> reservedItems;
        buildReservedValues(reservedItems);
        for (auto& i : reservedItems) {
            if (i == option2) {
                outputWarning("Reserved option (" + option2 + ") was passed in command line option (" + option2 + ")");
                outputWarning("This option is reserved and will be ignored", false);
                return true;
            }
        }
        uint startPos = option2.find('=');
        if (startPos != string::npos) {
            // Find before the =
            string list = option2.substr(0, startPos);
            // The actual element name is suffixed by list name (all after the =)
            option2 = option2.substr(startPos + 1) + "_" + list;
            // Get the config element
            if (!isConfigOptionValid(option2)) {
                outputError("Unknown option (" + option2 + ") in command line option (" + option2 + ")");
                outputError("Use --help to get available options", false);
                return false;
            }
            toggleConfigValue(option2, enable, false, true);
            // Enable parent list
            toggleConfigValue(list + 's', true);
        } else {
            // Check for changes to entire list
            if (option2 == "devices") {
                // Change INDEV_LIST
                vector<string> list;
                if (!getConfigList("INDEV_LIST", list)) {
                    return false;
                }
                for (const auto& i : list) {
                    toggleConfigValue(i, enable, false, true);
                }
                // Change OUTDEV_LIST
                list.resize(0);
                if (!getConfigList("OUTDEV_LIST", list)) {
                    return false;
                }
                for (const auto& i : list) {
                    toggleConfigValue(i, enable, false, true);
                }
            } else if (option2 == "programs") {
                // Change PROGRAM_LIST
                vector<string> list;
                if (!getConfigList("PROGRAM_LIST", list)) {
                    return false;
                }
                for (const auto& i : list) {
                    toggleConfigValue(i, enable, false, true);
                }
            } else if (option2 == "everything") {
                // Change ALL_COMPONENTS
                vector<string> list;
                if (!getConfigList("ALL_COMPONENTS", list)) {
                    return false;
                }
                for (const auto& i : list) {
                    toggleConfigValue(i, enable, false, true);
                }
            } else if (option2 == "all") {
                // Change ALL_COMPONENTS
                vector<string> list;
                if (!getConfigList("ALL_COMPONENTS", list)) {
                    return false;
                }
                for (const auto& i : list) {
                    toggleConfigValue(i, enable, false, true);
                }
                // Change LIBRARY_LIST
                list.resize(0);
                if (!getConfigList("LIBRARY_LIST", list)) {
                    return false;
                }
                for (const auto& i : list) {
                    toggleConfigValue(i, enable, false, true);
                }
                // Change PROGRAM_LIST
                list.resize(0);
                if (!getConfigList("PROGRAM_LIST", list)) {
                    return false;
                }
                for (const auto& i : list) {
                    toggleConfigValue(i, enable, false, true);
                }
            } else if (option2 == "autodetect") {
                // Change AUTODETECT_LIBS
                vector<string> list;
                if (!getConfigList("AUTODETECT_LIBS", list)) {
                    return false;
                }
                for (const auto& i : list) {
                    toggleConfigValue(i, enable, true);
                }
                toggleConfigValue(option2, enable);
            } else {
                // Check if the option is a component
                vector<string> list;
                getConfigList("COMPONENT_LIST", list);
                if (find(list.begin(), list.end(), option2) != list.end()) {
                    // This is a component
                    string option3 = option2.substr(0, option2.length() - 1); // Need to remove the s from end
                    // Get the specific list
                    list.resize(0);
                    transform(option3.begin(), option3.end(), option3.begin(), ::toupper);
                    getConfigList(option3 + "_LIST", list);
                    for (const auto& i : list) {
                        toggleConfigValue(i, enable, false, true);
                    }
                } else {
                    // If not one of above components then check if it exists as standalone option
                    if (!isConfigOptionValid(option2)) {
                        outputError("Unknown option (" + option2 + ") in command line option (" + option2 + ")");
                        outputError("Use --help to get available options", false);
                        return false;
                    }
                    // Check if this option has a component list
                    string option3 = option2;
                    transform(option3.begin(), option3.end(), option3.begin(), ::toupper);
                    option3 += "_COMPONENTS";
                    list.resize(0);
                    getConfigList(option3, list, false);
                    for (const auto& i : list) {
                        // This is a component
                        option3 = i.substr(0, i.length() - 1); // Need to remove the s from end
                        // Get the specific list
                        vector<string> list2;
                        transform(option3.begin(), option3.end(), option3.begin(), ::toupper);
                        getConfigList(option3 + "_LIST", list2);
                        for (const auto& j : list) {
                            toggleConfigValue(j, enable, false, true);
                        }
                    }
                }
                toggleConfigValue(option2, enable, false, true);
            }
        }
    }
    // Add to the internal configuration variable
    auto configPair = m_fixedConfigValues.begin();
    for (; configPair < m_fixedConfigValues.end(); ++configPair) {
        if (configPair->m_option == m_projectName + "_CONFIGURATION") {
            break;
        }
    }
    if (configPair != m_fixedConfigValues.end()) {
        // This will happen when passing early --prefix, --rootdir etc.
        configPair->m_value.resize(configPair->m_value.length() - 1); // Remove trailing "
        if (configPair->m_value.length() > 2) {
            configPair->m_value += ' ';
        }
        configPair->m_value += option + "\"";
    }
    return true;
}

bool ConfigGenerator::passCurrentValues()
{
    if (m_usingExistingConfig) {
        // Don't output a new config as just use the original
        return passExistingConfig();
    }

    // Correct license variables
    if (isConfigOptionEnabled("version3")) {
        if (isConfigOptionEnabled("gpl")) {
            fastToggleConfigValue("gplv3", true);
        } else {
            fastToggleConfigValue("lgplv3", true);
        }
    }

    // Check for disabled libraries and disable all components
    // In order to correctly enable _select dependencies we also do a weak enable of any unset library components
    vector<string> libList;
    if (getConfigList("LIBRARY_LIST", libList, false)) {
        vector<string> list2;
        for (const auto& i : libList) {
            const auto opt = getConfigOption(i);
            const bool enable = (opt != m_configValues.end()) && (opt->m_value != "0");
            const bool weak = enable;
            string optionUpper = i; // Ensure it is in upper case
            transform(optionUpper.begin(), optionUpper.end(), optionUpper.begin(), ::toupper);
            list2.resize(0);
            if (getConfigList(optionUpper + "_COMPONENTS", list2, false)) {
                for (const auto& j : list2) {
                    toggleConfigValue(j, enable, weak);
                }
            }
            list2.resize(0);
            if (getConfigList(optionUpper + "_COMPONENTS_LIST", list2, false)) {
                for (const auto& j : list2) {
                    toggleConfigValue(j, enable, weak);
                }
            }
        }
    }

    // Perform full check of all config values
    auto option = m_configValues.begin();
    for (; option < m_configValues.end(); ++option) {
        if (!passDependencyCheck(option)) {
            return false;
        }
    }
    // Check for any still unset values
    option = m_configValues.begin();
    for (; option < m_configValues.end(); ++option) {
        // If still not set then disable
        if (option->m_value.empty()) {
            option->m_value = "0";
        }
    }

#if defined(OPTIMISE_ENCODERS) || defined(OPTIMISE_DECODERS)
    // Optimise the config values. Based on user input different encoders/decoder can be disabled as there are now
    // better inbuilt alternatives
    OptimisedConfigList optimisedDisables;
    buildOptimisedDisables(optimisedDisables);
    // Check everything that is disabled based on current configuration
    bool disabledOpt = false;
    for (const auto& i : optimisedDisables) {
        // Check if optimised value is valid for current configuration
        auto disableOpt = getConfigOption(i.first);
        if (disableOpt != m_configValues.end()) {
            if (disableOpt->m_value == "1") {
                // Disable unneeded items
                for (const auto& j : i.second) {
                    disabledOpt = true;
                    toggleConfigValue(j, false);
                }
            }
        }
    }
    // It may be possible that the above optimisation pass disables some dependencies of other options.
    // If this happens then a full recheck is performed
    if (disabledOpt) {
        option = m_configValues.begin();
        for (; option < m_configValues.end(); ++option) {
            if (!passDependencyCheck(option)) {
                return false;
            }
        }
    }
#endif

    // Check the current options are valid for selected license
    if (!isConfigOptionEnabled("nonfree")) {
        vector<string> licenseList;
        // Check for existence of specific license lists
        if (getConfigList("EXTERNAL_LIBRARY_NONFREE_LIST", licenseList, false)) {
            for (auto i = licenseList.begin(); i < licenseList.end(); ++i) {
                if (isConfigOptionEnabled(*i)) {
                    outputError("Current license does not allow for option (" + getConfigOption(*i)->m_option + ")");
                    return false;
                }
            }
            // Check for gpl3 lists
            if (!isConfigOptionEnabled("gplv3")) {
                licenseList.clear();
                if (getConfigList("EXTERNAL_LIBRARY_GPLV3_LIST", licenseList, false)) {
                    for (auto i = licenseList.begin(); i < licenseList.end(); ++i) {
                        if (isConfigOptionEnabled(*i)) {
                            outputError(
                                "Current license does not allow for option (" + getConfigOption(*i)->m_option + ")");
                            return false;
                        }
                    }
                }
            }
            // Check for version3 lists
            if ((!isConfigOptionEnabled("lgplv3")) && (!isConfigOptionEnabled("gplv3"))) {
                licenseList.clear();
                if (getConfigList("EXTERNAL_LIBRARY_VERSION3_LIST", licenseList, false)) {
                    for (auto itI = licenseList.begin(); itI < licenseList.end(); ++itI) {
                        if (isConfigOptionEnabled(*itI)) {
                            outputError(
                                "Current license does not allow for option (" + getConfigOption(*itI)->m_option + ")");
                            return false;
                        }
                    }
                }
            }
            // Check for gpl lists
            if (!isConfigOptionEnabled("gpl")) {
                licenseList.clear();
                if (getConfigList("EXTERNAL_LIBRARY_GPL_LIST", licenseList, false)) {
                    for (auto itI = licenseList.begin(); itI < licenseList.end(); ++itI) {
                        if (isConfigOptionEnabled(*itI)) {
                            outputError(
                                "Current license does not allow for option (" + getConfigOption(*itI)->m_option + ")");
                            return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool ConfigGenerator::outputConfig()
{
    outputLine("  Outputting config.h...");

    // Create header output
    string fileHeader = getCopywriteHeader("Automatically generated configuration values") + '\n';
    string configureFile = fileHeader;
    configureFile += "\n#ifndef SMP_CONFIG_H\n";
    configureFile += "#define SMP_CONFIG_H\n";

    // Update the license configuration
    auto option = m_fixedConfigValues.begin();
    for (; option < m_fixedConfigValues.end(); ++option) {
        if (option->m_option == (m_projectName + "_LICENSE")) {
            break;
        }
    }
    if (isConfigOptionEnabled("nonfree")) {
        option->m_value = "\"nonfree and unredistributable\"";
    } else if (isConfigOptionEnabled("gplv3")) {
        option->m_value = "\"GPL version 3 or later\"";
    } else if (isConfigOptionEnabled("lgplv3")) {
        option->m_value = "\"LGPL version 3 or later\"";
    } else if (isConfigOptionEnabled("gpl")) {
        option->m_value = "\"GPL version 2 or later\"";
    } else {
        option->m_value = "\"LGPL version 2.1 or later\"";
    }

    // Build inbuilt force replace list
    string header;
    buildReplaceValues(m_replaceList, header, m_replaceListASM);

    // Output header
    configureFile += header + '\n';

    // Output all fixed config options
    for (const auto& i : m_fixedConfigValues) {
        // Check for forced replacement (only if attribute is not disabled)
        if ((i.m_value != "0") && (m_replaceList.find(i.m_option) != m_replaceList.end())) {
            configureFile += m_replaceList[i.m_option] + '\n';
        } else {
            configureFile += "#define " + i.m_option + ' ' + i.m_value + '\n';
        }
    }

    // Create ASM config file
    string header2 = fileHeader;
    header2.replace(header2.find(" */", header2.length() - 4), 3, ";******");
    size_t findPos = header2.find("/*");
    header2.replace(findPos, 2, ";******");
    while ((findPos = header2.find(" *", findPos)) != string::npos) {
        header2.replace(findPos, 2, ";* ");
        findPos += 3;
    }
    string configureFileASM = header2 + '\n';

    // Output all internal options
    auto endConfig = (m_configComponentsStart > 0) ? m_configComponentsStart : m_configValuesEnd;
    for (auto i = m_configValues.begin(); i < m_configValues.begin() + endConfig; ++i) {
        string sTagName = i->m_prefix + i->m_option;
        // Check for forced replacement (only if attribute is not disabled)
        string addConfig;
        if ((i->m_value != "0") && (m_replaceList.find(sTagName) != m_replaceList.end())) {
            addConfig = m_replaceList[sTagName];
        } else {
            addConfig = "#define " + sTagName + ' ' + i->m_value;
        }
        configureFile += addConfig + '\n';
        if ((i->m_value != "0") && (m_replaceListASM.find(sTagName) != m_replaceListASM.end())) {
            configureFileASM += m_replaceListASM[sTagName] + '\n';
        } else {
            configureFileASM += "%define " + sTagName + ' ' + i->m_value + '\n';
        }
    }

    // Output end header guard
    configureFile += "#endif /* SMP_CONFIG_H */\n";
    // Write output files
    string configFile = m_solutionDirectory + "config.h";
    if (!writeToFile(configFile, configureFile)) {
        outputError("Failed opening output configure file (" + configFile + ")");
        return false;
    }
    configFile = m_solutionDirectory + "config.asm";
    if (!writeToFile(configFile, configureFileASM)) {
        outputError("Failed opening output asm configure file (" + configFile + ")");
        return false;
    }

    if (m_configComponentsStart > 0) {
        // Output config_components.h
        outputLine("  Outputting config_components.h...");
        string componentsFile = fileHeader;
        componentsFile += "\n#ifndef FFMPEG_CONFIG_COMPONENTS_H\n";
        componentsFile += "#define FFMPEG_CONFIG_COMPONENTS_H\n";
        for (auto i = m_configValues.begin() + m_configComponentsStart; i < m_configValues.begin() + m_configValuesEnd;
             ++i) {
            string sTagName = i->m_prefix + i->m_option;
            // Check for forced replacement (only if attribute is not disabled)
            string addConfig;
            if ((i->m_value != "0") && (m_replaceList.find(sTagName) != m_replaceList.end())) {
                addConfig = m_replaceList[sTagName];
            } else {
                addConfig = "#define " + sTagName + ' ' + i->m_value;
            }
            componentsFile += addConfig + '\n';
        }
        // Output end header guard
        componentsFile += "#endif /* FFMPEG_CONFIG_COMPONENTS_H */\n";
        // Write output files
        configFile = m_solutionDirectory + "config_components.h";
        if (!writeToFile(configFile, componentsFile)) {
            outputError("Failed opening output configure file (" + configFile + ")");
            return false;
        }
    }

    // Output avconfig.h
    outputLine("  Outputting avconfig.h...");
    if (!makeDirectory(m_solutionDirectory + "libavutil")) {
        outputError("Failed creating local libavutil directory");
        return false;
    }

    // Output header guard
    string configFileAV = fileHeader + '\n';
    configFileAV += "#ifndef SMP_LIBAVUTIL_AVCONFIG_H\n";
    configFileAV += "#define SMP_LIBAVUTIL_AVCONFIG_H\n";

    // avconfig.h currently just uses HAVE_LIST_PUB to define its values
    vector<string> configListAV;
    if (!getConfigList("HAVE_LIST_PUB", configListAV)) {
        outputError("Failed finding HAVE_LIST_PUB needed for avconfig.h generation");
        return false;
    }
    for (const auto& i : configListAV) {
        auto option2 = getConfigOption(i);
        configFileAV += "#define AV_HAVE_" + option2->m_option + ' ' + option2->m_value + '\n';
    }
    configFileAV += "#endif /* SMP_LIBAVUTIL_AVCONFIG_H */\n";
    configFile = m_solutionDirectory + "libavutil/avconfig.h";
    if (!writeToFile(configFile, configFileAV)) {
        outputError("Failed opening output avconfig file (" + configFileAV + ")");
        return false;
    }

    // Output ffversion.h
    outputLine("  Outputting ffversion.h...");
    // Open VERSION file and get version string
    string versionDefFile = m_rootDirectory + "RELEASE";
    ifstream ifVersionDefFile(versionDefFile);
    if (!ifVersionDefFile.is_open()) {
        outputError("Failed opening output version file (" + versionDefFile + ")");
        return false;
    }
    // Load first line into string
    string version;
    getline(ifVersionDefFile, version);
    ifVersionDefFile.close();

    // Output header
    string versionFile = fileHeader + '\n';

    // Output info
    versionFile += "#ifndef SMP_LIBAVUTIL_FFVERSION_H\n#define SMP_LIBAVUTIL_FFVERSION_H\n#define FFMPEG_VERSION \"";
    versionFile += version;
    versionFile += "\"\n#endif /* SMP_LIBAVUTIL_FFVERSION_H */\n";
    // Open output file
    configFile = m_solutionDirectory + "libavutil/ffversion.h";
    if (!writeToFile(configFile, versionFile)) {
        outputError("Failed opening output version file (" + versionFile + ")");
        return false;
    }

    // Output enabled components lists
    uint start = m_configureFile.find("print_enabled_components ");
    while (start != string::npos) {
        // Get file name input parameter
        start = m_configureFile.find_first_not_of(g_whiteSpace, start + 24);
        uint end = m_configureFile.find_first_of(g_whiteSpace, start + 1);
        string file = m_configureFile.substr(start, end - start);
        // Get struct name input parameter
        start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
        end = m_configureFile.find_first_of(g_whiteSpace, start + 1);
        string structName = m_configureFile.substr(start, end - start);
        // Get list name input parameter
        start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
        end = m_configureFile.find_first_of(g_whiteSpace, start + 1);
        string name = m_configureFile.substr(start, end - start);
        // Get config list input parameter
        start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
        end = m_configureFile.find_first_of(g_whiteSpace, ++start); // skip preceding '$'
        string list = m_configureFile.substr(start, end - start);
        if (!passEnabledComponents(file, structName, name, list)) {
            return false;
        }
        start = m_configureFile.find("print_enabled_components ", end + 1);
    }
    return true;
}

void ConfigGenerator::deleteCreatedFiles() const
{
    if (!m_usingExistingConfig) {
        // Delete any previously generated files
        vector<string> existingFiles;
        findFiles(m_solutionDirectory + "config.h", existingFiles, false);
        findFiles(m_solutionDirectory + "config.asm", existingFiles, false);
        findFiles(m_solutionDirectory + "libavutil/avconfig.h", existingFiles, false);
        findFiles(m_solutionDirectory + "libavutil/ffversion.h", existingFiles, false);
        for (const auto& i : existingFiles) {
            deleteFile(i);
        }
    }
}

void ConfigGenerator::makeFileProjectRelative(const string& fileName, string& retFileName) const
{
    string path;
    string file = fileName;
    uint pos = file.rfind('/');
    if (pos != string::npos) {
        ++pos;
        path = fileName.substr(0, pos);
        file = fileName.substr(pos);
    }
    makePathsRelative(path, m_solutionDirectory, retFileName);
    // Check if relative to project dir
    if (retFileName.find("./") == 0) {
        retFileName = retFileName.substr(2);
    }
    retFileName += file;
}

void ConfigGenerator::makeFileGeneratorRelative(const string& fileName, string& retFileName) const
{
    string path;
    string file = fileName;
    uint pos = file.rfind('/');
    if (pos != string::npos) {
        ++pos;
        path = fileName.substr(0, pos);
        file = fileName.substr(pos);
    }
    if (path != m_solutionDirectory) {
        path = m_solutionDirectory + path;
    }
    makePathsRelative(path, "./", retFileName);
    // Check if relative to current dir
    if (retFileName.find("./") == 0) {
        retFileName = retFileName.substr(2);
    }
    retFileName += file;
}

bool ConfigGenerator::getConfigList(const string& list, vector<string>& returnList, bool force, uint currentFilePos)
{
    // Check if list is in existing cache
    auto cachedList = m_cachedConfigLists.find(list);
    if (currentFilePos == string::npos && cachedList != m_cachedConfigLists.end()) {
        returnList.insert(returnList.end(), cachedList->second.begin(), cachedList->second.end());
        return true;
    }
    vector<string> foundList;

    // Find List name in file (searches backwards so that it finds the closest definition to where we currently are)
    //   This is in case a list is redefined
    uint start = m_configureFile.rfind(list + "=", currentFilePos);
    // Need to ensure this is the correct list
    while ((start != string::npos) && (m_configureFile.at(start - 1) != '\n')) {
        start = m_configureFile.rfind(list + "=", start - 1);
    }
    if (start == string::npos) {
        if (force) {
            outputError("Failed finding config list (" + list + ")");
        }
        return false;
    }
    start += list.length() + 1;
    // Check if this is a list or a function
    char endList = '\n';
    if (m_configureFile.at(start) == '"') {
        endList = '"';
        ++start;
    } else if (m_configureFile.at(start) == '\'') {
        endList = '\'';
        ++start;
    }
    // Get start of tag
    start = m_configureFile.find_first_not_of(g_whiteSpace, start);
    while (m_configureFile.at(start) != endList) {
        // Check if this is a function
        uint end;
        if ((m_configureFile.at(start) == '$') && (m_configureFile.at(start + 1) == '(')) {
            // Skip $(
            start += 2;
            // Get function name
            end = m_configureFile.find_first_of(g_whiteSpace, start + 1);
            string function = m_configureFile.substr(start, end - start);
            // Check if this is a known function
            if (function == "find_things") {
                // Get first parameter
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace, start + 1);
                string param1 = m_configureFile.substr(start, end - start);
                // Get second parameter
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace, start + 1);
                string param2 = m_configureFile.substr(start, end - start);
                // Get file name
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace + ")", start + 1);
                string param3 = m_configureFile.substr(start, end - start);
                // Call function find_things
                if (!passFindThings(param1, param2, param3, foundList)) {
                    return false;
                }
                // Make sure the closing ) is not included
                end = (m_configureFile.at(end) == ')') ? end + 1 : end;
            } else if (function == "find_things_extern") {
                // Get first parameter
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace, start + 1);
                string param1 = m_configureFile.substr(start, end - start);
                // Get second parameter
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace, start + 1);
                string param2 = m_configureFile.substr(start, end - start);
                // Get file name
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace + ")", start + 1);
                string param3 = m_configureFile.substr(start, end - start);
                // Check for optional 4th argument
                string param4;
                if ((m_configureFile.at(end) != ')') &&
                    (m_configureFile.at(m_configureFile.find_first_not_of(g_whiteSpace, end)) != ')')) {
                    start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                    end = m_configureFile.find_first_of(g_whiteSpace + ")", start + 1);
                    param4 = m_configureFile.substr(start, end - start);
                }
                // Call function find_things
                if (!passFindThingsExtern(param1, param2, param3, param4, foundList)) {
                    return false;
                }
                // Make sure the closing ) is not included
                end = (m_configureFile.at(end) == ')') ? end + 1 : end;
            } else if (function == "add_suffix") {
                // Get first parameter
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace, start + 1);
                string param1 = m_configureFile.substr(start, end - start);
                // Get second parameter
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace + ")", start + 1);
                string param2 = m_configureFile.substr(start, end - start);
                // Call function add_suffix
                if (!passAddSuffix(param1, param2, foundList)) {
                    return false;
                }
                // Make sure the closing ) is not included
                end = (m_configureFile.at(end) == ')') ? end + 1 : end;
            } else if (function == "filter_out") {
                // This should filter out occurrence of first parameter from the list passed in the second
                uint startSearch = start - list.length() - 5; // ensure search is before current instance of
                // list
                // Get first parameter
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace, start + 1);
                string param1 = m_configureFile.substr(start, end - start);
                // Get second parameter
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace + ")", start + 1);
                string param2 = m_configureFile.substr(start, end - start);
                // Call function add_suffix
                if (!passFilterOut(param1, param2, foundList, startSearch)) {
                    return false;
                }
                // Make sure the closing ) is not included
                end = (m_configureFile.at(end) == ')') ? end + 1 : end;
            } else if (function == "find_filters_extern") {
                // Get file name
                start = m_configureFile.find_first_not_of(g_whiteSpace, end + 1);
                end = m_configureFile.find_first_of(g_whiteSpace + ")", start + 1);
                string param = m_configureFile.substr(start, end - start);
                // Call function find_filters_extern
                if (!passFindFiltersExtern(param, foundList)) {
                    return false;
                }
                // Make sure the closing ) is not included
                end = (m_configureFile.at(end) == ')') ? end + 1 : end;
            } else {
                outputError("Unknown list function (" + function + ") found in list (" + list + ")");
                return false;
            }
        } else {
            end = m_configureFile.find_first_of(g_whiteSpace + endList, start + 1);
            // Get the tag
            string tag = m_configureFile.substr(start, end - start);
            // Check the type of tag
            if (tag.at(0) == '$') {
                // Strip the identifier
                tag.erase(0, 1);
                // Recursively pass
                if (!getConfigList(tag, foundList, force, end)) {
                    return false;
                }
            } else {
                // Directly add the identifier
                foundList.push_back(tag);
            }
        }
        start = m_configureFile.find_first_not_of(g_whiteSpace, end);
        // If this is not specified as a list then only a '\' will allow for more than 1 line
        if ((endList == '\n') && (m_configureFile.at(start) != '\\')) {
            break;
        }
    }
    // Add the new list to the cache
    if (currentFilePos == string::npos) {
        m_cachedConfigLists[list] = foundList;
    }
    returnList.insert(returnList.end(), foundList.begin(), foundList.end());
    return true;
}

bool ConfigGenerator::passFindThings(const string& param1, const string& param2, const string& param3,
    vector<string>& returnList, vector<string>* returnExterns) const
{
    // Need to find and open the specified file
    const string file = m_rootDirectory + param3;
    string findFile;
    if (!loadFromFile(file, findFile)) {
        return false;
    }
    string decl;

    // Find the search pattern in the file
    uint start = findFile.find(param2);
    while (start != string::npos) {
        // Find the start of the tag (also as ENCDEC should be treated as both DEC+ENC we skip that as well)
        start = findFile.find_first_of(g_whiteSpace + "(", start + 1);
        // Skip any filling white space
        start = findFile.find_first_not_of(" \t", start);
        // Check if valid
        if (findFile.at(start) != '(') {
            // Get next
            start = findFile.find(param2, start + 1);
            continue;
        }
        ++start;
        // Find end of tag
        uint end = findFile.find_first_of(g_whiteSpace + ",);", start);
        if (findFile.at(end) != ',') {
            // Get next
            start = findFile.find(param2, end + 1);
            continue;
        }
        // Get the tag string
        string tag = findFile.substr(start, end - start);
        // Check to make sure this is a definition not a macro declaration
        if (tag == "X") {
            if ((returnExterns != nullptr) && (decl.length() == 0)) {
                // Get the first occurance of extern then till ; as that gives naming for export as well as type
                start = findFile.find("extern ", end + 1) + 7;
                end = findFile.find(';', start + 1);
                decl = findFile.substr(start, end - start);
                start = decl.find("##");
                while (start != string::npos) {
                    char cReplace = '@';
                    if (decl.at(start + 2) == 'y') {
                        cReplace = '$';
                    }
                    decl.replace(start, ((decl.length() - start) > 3) ? 5 : 3, 1, cReplace);
                    start = decl.find("##");
                }
            }

            // Get next
            start = findFile.find(param2, end + 1);
            continue;
        }
        // Get second tag
        start = findFile.find_first_not_of(" \t", end + 1);
        end = findFile.find_first_of(g_whiteSpace + ",);", start);
        if ((findFile.at(end) != ')') && (findFile.at(end) != ',')) {
            // Get next
            start = findFile.find(param2, end + 1);
            continue;
        }
        string tag2 = findFile.substr(start, end - start);
        if (returnExterns == nullptr) {
            // Check that both tags match
            transform(tag2.begin(), tag2.end(), tag2.begin(), ::toupper);
            if (tag2 != tag) {
                // This is somewhat incorrect as the official configuration will always take the second tag
                //  and create a config option out of it. This is actually incorrect as the source code itself
                //  only uses the first parameter as the config option.
                swap(tag, tag2);
            }
        }
        transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
        // Add any requested externs
        if (returnExterns != nullptr) {
            // Create new extern by replacing tag with found one
            start = 0;
            string decTag = decl;
            while ((start = decTag.find('@', start)) != std::string::npos) {
                decTag.replace(start, 1, tag2);
                start += tag2.length();
            }
            // Get any remaining tags and add to extern
            if (decTag.find('$') != string::npos) {
                // Get third tag
                start = findFile.find_first_not_of(" \t", end + 1);
                end = findFile.find_first_of(g_whiteSpace + ",);", start);
                if ((findFile.at(end) != ')') && (findFile.at(end) != ',')) {
                    // Get next
                    start = findFile.find(param2, end + 1);
                    continue;
                }
                string tag3 = findFile.substr(start, end - start);
                // Replace second tag
                start = 0;
                while ((start = decTag.find('$', start)) != std::string::npos) {
                    decTag.replace(start, 1, tag3);
                    start += tag3.length();
                }
            }

            // Add to the list
            returnExterns->push_back(decTag);
        }
        tag += "_" + param1;
        // Add the new value to list
        returnList.push_back(tag);
        // Get next
        start = findFile.find(param2, end + 1);
    }
    return true;
}

bool ConfigGenerator::passFindThingsExtern(const string& param1, const string& param2, const string& param3,
    const string& param4, vector<string>& returnList) const
{
    // Need to find and open the specified file
    const string file = m_rootDirectory + param3;
    string findFile;
    if (!loadFromFile(file, findFile)) {
        return false;
    }

    // Find the search pattern in the file
    const string startSearch = "extern ";
    uint start = findFile.find(startSearch);
    while (start != string::npos) {
        start += startSearch.length();
        // Skip any occurrence of 'const'
        if ((findFile.at(start) == 'c') && (findFile.find("const ", start) == start)) {
            start += 6;
        } else if ((findFile.at(start) == 'L') && (findFile.find("LIBX264_CONST ", start) == start)) {
            // Hacky fix for detecting a macro define of const
            start += 14;
        }
        // Check for search tag
        start = findFile.find_first_not_of(g_whiteSpace, start);
        if ((findFile.at(start) != param2.at(0)) || (findFile.find(param2, start) != start)) {
            // Get next
            start = findFile.find(startSearch, start + 1);
            continue;
        }
        start += param2.length() + 1;
        start = findFile.find_first_not_of(g_whiteSpace, start);
        // Check for function start
        if ((findFile.at(start) != 'f') || (findFile.find("ff_", start) != start)) {
            // Get next
            start = findFile.find(startSearch, start + 1);
            continue;
        }
        start += 3;
        // Find end of tag
        uint end = findFile.find_first_of(g_whiteSpace + ",();[]", start);
        uint end2 = findFile.find("_" + param1, start);
        end = (end2 < end) ? end2 : end;
        if ((findFile.at(end) != '_') || (end2 != end)) {
            // Get next
            start = findFile.find(startSearch, end + 1);
            continue;
        }
        // Get the tag string
        end += 1 + param1.length();
        string tag = findFile.substr(start, end - start);
        // Check for any 4th value replacements
        if (param4.length() > 0) {
            const uint rep = tag.find("_" + param1);
            tag.replace(rep, rep + 1 + param1.length(), "_" + param4);
        }
        // Add the new value to list
        transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
        returnList.push_back(tag);
        // Get next
        start = findFile.find(startSearch, end + 1);
    }
    return true;
}

bool ConfigGenerator::passFindFiltersExtern(const string& param1, vector<string>& returnList) const
{
    // s/^extern AVFilter ff_([avfsinkrc]{2,5})_([a-zA-Z0-9_]+);/\2_filter/p
    // s/^extern const AVFilter ff_[avfsinkrc]\{2,5\}_\([[:alnum:]_]\{1,\}\);/\1_filter/p from 4.4 onwards
    // Need to find and open the specified file
    const string file = m_rootDirectory + param1;
    string findFile;
    if (!loadFromFile(file, findFile)) {
        return false;
    }

    // Find the search pattern in the file
    string search = "extern AVFilter ff_";
    uint start = findFile.find(search);
    if (start == string::npos) {
        search = "extern const AVFilter ff_";
        start = findFile.find(search);
    }
    while (start != string::npos) {
        // Find the start and end of the tag
        start += search.length();
        // Find end of tag
        const uint end = findFile.find_first_of(g_whiteSpace + ",();", start);
        // Get the tag string
        string tag = findFile.substr(start, end - start);
        // Get first part
        start = tag.find('_');
        if (start == string::npos) {
            // Get next
            start = findFile.find(search, end + 1);
            continue;
        }
        const string first = tag.substr(0, start);
        if (first.find_first_not_of("avfsinkrc") != string::npos) {
            // Get next
            start = findFile.find(search, end + 1);
            continue;
        }
        // Get second part
        tag = tag.substr(++start);
        transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
        tag += "_filter";
        // Add the new value to list
        returnList.push_back(tag);
        // Get next
        start = findFile.find(search, end + 1);
    }
    return true;
}

bool ConfigGenerator::passAddSuffix(
    const string& param1, const string& param2, vector<string>& returnList, const uint currentFilePos)
{
    // Convert the first parameter to upper case
    string param1Upper = param1;
    transform(param1Upper.begin(), param1Upper.end(), param1Upper.begin(), ::toupper);
    // Erase the $ from variable
    const string param2Cut = param2.substr(1, param2.length() - 1);
    // Just call getConfigList
    vector<string> vTemp;
    if (getConfigList(param2Cut, vTemp, true, currentFilePos)) {
        // Update with the new suffix and add to the list
        for (const auto& i : vTemp) {
            returnList.push_back(i + param1Upper);
        }
        return true;
    }
    return false;
}

bool ConfigGenerator::passFilterOut(
    const string& param1, const string& param2, vector<string>& returnList, const uint currentFilePos)
{
    // Remove the "'" from the front and back of first parameter
    const string param1Cut = param1.substr(1, param1.length() - 2);
    // Erase the $ from variable2
    const string param2Cut = param2.substr(1, param2.length() - 1);
    // Get the list
    if (getConfigList(param2Cut, returnList, true, currentFilePos)) {
        for (auto checkItem = returnList.begin(); checkItem < returnList.end(); ++checkItem) {
            if (*checkItem == param1Cut) {
                returnList.erase(checkItem);
                // assume only appears once in list
                break;
            }
        }
        return true;
    }
    return false;
}

bool ConfigGenerator::passFullFilterName(const string& param1, string& returnString) const
{
    // sed -n "s/^extern AVFilter ff_\([avfsinkrc]\{2,5\}\)_$1;/\1_$1/p"
    // s/^extern const AVFilter ff_[avfsinkrc]\{2,5\}_\([[:alnum:]_]\{1,\}\);/\1_filter/p from 4.4 onwards
    // Need to find and open the specified file
    const string file = m_rootDirectory + "libavfilter/allfilters.c";
    string findFile;
    if (!loadFromFile(file, findFile)) {
        return false;
    }

    // Find the search pattern in the file
    string search = "extern AVFilter ff_";
    uint start = findFile.find(search);
    if (start == string::npos) {
        search = "extern const AVFilter ff_";
        start = findFile.find(search);
    }
    while (start != string::npos) {
        // Find the start and end of the tag
        start += search.length();
        // Find end of tag
        const uint end = findFile.find_first_of(g_whiteSpace + ",();", start);
        // Get the tag string
        string tag = findFile.substr(start, end - start);
        // Get first part
        start = tag.find('_');
        if (start == string::npos) {
            // Get next
            start = findFile.find(search, end + 1);
            continue;
        }
        const string first = tag.substr(0, start);
        if (first.find_first_not_of("avfsinkrc") != string::npos) {
            // Get next
            start = findFile.find(search, end + 1);
            continue;
        }
        // Get second part
        string second = tag.substr(++start);
        transform(second.begin(), second.end(), second.begin(), ::tolower);
        if (second == param1) {
            returnString = tag;
            transform(returnString.begin(), returnString.end(), returnString.begin(), ::tolower);
            return true;
        }
        // Get next
        start = findFile.find(search, end + 1);
    }
    return true;
}

bool ConfigGenerator::passConfigList(const string& prefix, const string& suffix, const string& list)
{
    vector<string> configList;
    if (getConfigList(list, configList)) {
        // Loop through each member of the list and add it to internal list
        for (const auto& i : configList) {
            // Directly add the identifier
            string tag = i;
            transform(tag.begin(), tag.end(), tag.begin(), ::toupper);
            tag += suffix;
            m_configValues.push_back(ConfigPair(tag, prefix, ""));
        }
        return true;
    }
    return false;
}

bool ConfigGenerator::passEnabledComponents(
    const string& file, const string& structName, const string& name, const string& list)
{
    outputLine("  Outputting enabled components file " + file + "...");
    string output;
    // Add copywrite header
    string nameNice = name;
    replace(nameNice.begin(), nameNice.end(), '_', ' ');
    output += getCopywriteHeader("Available items from " + nameNice);
    output += '\n';

    // Output includes
    output += "#include \"config.h\"\n";
    if (m_configComponentsStart > 0) {
        output += "#include \"config_components.h\"\n";
    }

    // Output header
    output += "static const " + structName + " *" + name + "[] = {\n";

    // Output each element of desired list
    vector<string> configList;
    if (!getConfigList(list, configList)) {
        return false;
    }

    // Check if using newer static filter list
    bool staticFilterList = false;
    if ((name == "filter_list") &&
        ((m_configureFile.find("full_filter_name()") != string::npos) ||
            (m_configureFile.find("$full_filter_name_$") != string::npos))) {
        staticFilterList = true;
    }
    for (const auto& i : configList) {
        auto option = getConfigOption(i);
        if (option == m_configValues.end()) {
            outputError("Unknown config option (" + i + ") found in component list (" + list + ")");
            continue;
        }
        if (option->m_value == "1") {
            string optionLower = option->m_option;
            transform(optionLower.begin(), optionLower.end(), optionLower.begin(), ::tolower);
            // Check for device type replacements
            if (name == "indev_list") {
                uint find = optionLower.find("_indev");
                if (find != string::npos) {
                    optionLower.resize(find);
                    optionLower += "_demuxer";
                }
            } else if (name == "outdev_list") {
                uint find = optionLower.find("_outdev");
                if (find != string::npos) {
                    optionLower.resize(find);
                    optionLower += "_muxer";
                }
            } else if (staticFilterList) {
                uint find = optionLower.find("_filter");
                if (find != string::npos) {
                    optionLower.resize(find);
                }
                if (!passFullFilterName(optionLower, optionLower)) {
                    continue;
                }
            }
            // Check if option requires replacement
            if (m_replaceList.find(option->m_prefix + option->m_option) != m_replaceList.end()) {
                // Since this is a replaced option we need to wrap it in its config preprocessor
                output += "#if " + option->m_prefix + option->m_option + '\n';
                output += "    &ff_" + optionLower + ",\n";
                output += "#endif\n";
            } else {
                output += "    &ff_" + optionLower + ",\n";
            }
        }
    }
    if (staticFilterList) {
        output += "    &ff_asrc_abuffer,\n";
        output += "    &ff_vsrc_buffer,\n";
        output += "    &ff_asink_abuffer,\n";
        output += "    &ff_vsink_buffer,\n";
    }
    output += "    NULL };";

    // Open output file
    writeToFile(m_solutionDirectory + file, output);
    return true;
}

bool ConfigGenerator::fastToggleConfigValue(const string& option, const bool enable, const bool weak)
{
    // Simply find the element in the list and change its setting
    string optionUpper = option; // Ensure it is in upper case
    transform(optionUpper.begin(), optionUpper.end(), optionUpper.begin(), ::toupper);
    // Find in internal list
    bool bRet = false;
    // Some options appear more than once with different prefixes
    for (auto& i : m_configValues) {
        if (i.m_option == optionUpper) {
            if (weak && !i.m_value.empty()) {
                continue;
            }
            i.m_value = (enable) ? "1" : "0";
            bRet = true;
        }
    }
    return bRet;
}

bool ConfigGenerator::toggleConfigValue(
    const string& option, const bool enable, const bool weak, const bool deep, const bool recursive)
{
    string optionUpper = option; // Ensure it is in upper case
    transform(optionUpper.begin(), optionUpper.end(), optionUpper.begin(), ::toupper);
    // Find in internal list
    bool ret = false;
    // Some options appear more than once with different prefixes
    for (auto& i : m_configValues) {
        if (i.m_option == optionUpper) {
            ret = true;
            if (!i.m_lock) {
                // Skip weak setting an already configured value
                if (!!(weak && !i.m_value.empty())) {
                    continue;
                }
                // Lock the item to prevent cyclic conditions
                i.m_lock = true;
                // Need to convert the name to lower case
                string optionLower = option;
                transform(optionLower.begin(), optionLower.end(), optionLower.begin(), ::tolower);
                if (enable) {
                    if (deep) {
                        string checkFunc = optionLower + "_select";
                        vector<string> checkList;
                        if (getConfigList(checkFunc, checkList, false)) {
                            for (const auto& j : checkList) {
                                toggleConfigValue(j, true, weak, true);
                            }
                        }

                        // If enabled then all of these should then be enabled if not already disabled
                        checkFunc = optionLower + "_suggest";
                        checkList.resize(0);
                        if (getConfigList(checkFunc, checkList, false)) {
                            for (const auto& j : checkList) {
                                toggleConfigValue(j, true, true, true);
                            }
                        }
                    }

                    // Check for any hard dependencies that must be enabled
                    vector<string> forceEnable;
                    buildForcedEnables(optionLower, forceEnable);
                    for (const auto& j : forceEnable) {
                        toggleConfigValue(j, true, weak, true);
                    }
                } else if (!enable) {
                    // Check for any hard dependencies that must be disabled
                    vector<string> forceDisable;
                    buildForcedDisables(optionLower, forceDisable);
                    for (const auto& j : forceDisable) {
                        toggleConfigValue(j, false, false, true);
                    }
                }
                if (!(weak && !i.m_value.empty())) {
                    // Change the items value
                    i.m_value = (enable) ? "1" : "0";
                }
                // Unlock item
                i.m_lock = false;
            }
        }
    }
    if (!ret) {
        DependencyList additionalDependencies;
        buildAdditionalDependencies(additionalDependencies);
        const auto dep = additionalDependencies.find(option);
        if (recursive) {
            // Ensure this is not already set
            if (dep == additionalDependencies.end()) {
                // Some options are passed in recursively that do not exist in internal list
                // However there dependencies should still be processed
                m_configValues.push_back(ConfigPair(optionUpper, "", ""));
                outputInfo("Unlisted config dependency found (" + option + ")");
            }
        } else {
            if (dep == additionalDependencies.end()) {
                outputError("Unknown config option (" + option + ")");
                return false;
            }
        }
    }
    return true;
}

ConfigGenerator::ValuesList::iterator ConfigGenerator::getConfigOption(const string& option)
{
    // Ensure it is in upper case
    string optionUpper = option;
    transform(optionUpper.begin(), optionUpper.end(), optionUpper.begin(), ::toupper);
    // Find in internal list
    auto values = m_configValues.begin();
    for (; values < m_configValues.end(); ++values) {
        if (values->m_option == optionUpper) {
            return values;
        }
    }
    return values;
}

vector<ConfigGenerator::ConfigPair>::const_iterator ConfigGenerator::getConfigOption(const string& option) const
{
    // Ensure it is in upper case
    string optionUpper = option;
    transform(optionUpper.begin(), optionUpper.end(), optionUpper.begin(), ::toupper);
    // Find in internal list
    auto values = m_configValues.begin();
    for (; values < m_configValues.end(); ++values) {
        if (values->m_option == optionUpper) {
            return values;
        }
    }
    return values;
}

ConfigGenerator::ValuesList::iterator ConfigGenerator::getConfigOptionPrefixed(const string& option)
{
    // Ensure it is in upper case
    string optionUpper = option;
    transform(optionUpper.begin(), optionUpper.end(), optionUpper.begin(), ::toupper);
    // Find in internal list
    auto values = m_configValues.begin();
    for (; values < m_configValues.end(); ++values) {
        if (optionUpper == values->m_prefix + values->m_option) {
            return values;
        }
    }
    return values;
}

ConfigGenerator::ValuesList::const_iterator ConfigGenerator::getConfigOptionPrefixed(const string& option) const
{
    // Ensure it is in upper case
    string optionUpper = option;
    transform(optionUpper.begin(), optionUpper.end(), optionUpper.begin(), ::toupper);
    // Find in internal list
    auto values = m_configValues.begin();
    for (; values < m_configValues.end(); ++values) {
        if (optionUpper == values->m_prefix + values->m_option) {
            return values;
        }
    }
    return values;
}

bool ConfigGenerator::isConfigOptionEnabled(const string& option) const
{
    const auto opt = getConfigOption(option);
    return (opt != m_configValues.end()) && (opt->m_value == "1");
}

bool ConfigGenerator::isConfigOptionValid(const string& option) const
{
    const auto opt = getConfigOption(option);
    return opt != m_configValues.end();
}

bool ConfigGenerator::isConfigOptionValidPrefixed(const string& option) const
{
    const auto opt = getConfigOptionPrefixed(option);
    return opt != m_configValues.end();
}

bool ConfigGenerator::isASMEnabled() const
{
    return isConfigOptionValidPrefixed("HAVE_X86ASM") || isConfigOptionValidPrefixed("HAVE_YASM");
}

bool ConfigGenerator::isCUDAEnabled() const
{
    return isConfigOptionValidPrefixed("CONFIG_CUDA_NVCC") || isConfigOptionValidPrefixed("CONFIG_CUDA_SDK");
}

bool ConfigGenerator::getMinWindowsVersion(uint& major, uint& minor) const
{
    const string search = "cppflags -D_WIN32_WINNT=0x";
    uint pos = m_configureFile.find(search);
    uint majorT = 10; // Initially set minimum version to Win 10
    uint minorT = 0;
    bool found = false;
    while (pos != string::npos) {
        pos += search.length();
        const uint endPos = m_configureFile.find_first_of(g_nonName, pos);
        // Check if valid version tag
        if ((endPos - pos) != 4) {
            outputInfo("Unknown windows version string found (" + search + ")");
        } else {
            const string versionMajor = m_configureFile.substr(pos, 2);
            // Convert to int from hex string
            const uint major2 = stoul(versionMajor, nullptr, 16);
            // Check if new version is less than current
            if (major2 <= majorT) {
                const string versionMinor = m_configureFile.substr(pos + 2, 2);
                const uint minor2 = stoul(versionMinor, nullptr, 16);
                if ((major2 < majorT) || (minor2 < minorT)) {
                    // Update best found version
                    majorT = major2;
                    minorT = minor2;
                    found = true;
                }
            }
        }
        // Get next
        pos = m_configureFile.find(search, endPos + 1);
    }
    if (found) {
        major = majorT;
        minor = minorT;
    }
    return found;
}

bool ConfigGenerator::passDependencyCheck(const ValuesList::iterator& option)
{
    // Need to convert the name to lower case
    string optionLower = option->m_option;
    transform(optionLower.begin(), optionLower.end(), optionLower.begin(), ::tolower);

    // Get list of additional dependencies
    DependencyList additionalDependencies;
    buildAdditionalDependencies(additionalDependencies);

    // Check if not enabled
    if (option->m_value != "1") {
        // Enabled if any of these
        string checkFunc = optionLower + "_if_any";
        vector<string> checkList;
        bool valid = getConfigList(checkFunc, checkList, false);
        // Also check if this has its own component list
        string optionUpper = option->m_option;
        transform(optionUpper.begin(), optionUpper.end(), optionUpper.begin(), ::toupper);
        valid |= getConfigList(optionUpper + "_COMPONENTS", checkList, false);
        if (valid) {
            for (auto& i : checkList) {
                // Check if this is a not !
                bool bToggle = false;
                if (i.at(0) == '!') {
                    i.erase(0);
                    bToggle = true;
                }
                bool enabled;
                auto temp = getConfigOption(i);
                if (temp == m_configValues.end()) {
                    auto dep = additionalDependencies.find(i);
                    if (dep == additionalDependencies.end()) {
                        outputInfo("Unknown option in ifa dependency (" + i + ") for option (" + optionLower + ")");
                        enabled = false;
                    } else {
                        enabled = dep->second ^ bToggle;
                    }
                } else {
                    // Check if this variable has been initialized already
                    if (temp > option) {
                        if (!passDependencyCheck(temp)) {
                            return false;
                        }
                    }
                    enabled = (temp->m_value == "1") ^ bToggle;
                }
                if (enabled) {
                    // If any deps are enabled then enable
                    toggleConfigValue(optionLower, true, true);
                    break;
                }
            }
        }
    }
    // Check if still not enabled
    if (option->m_value != "1") {
        // Should be enabled if all of these
        string checkFunc = optionLower + "_if";
        vector<string> checkList;
        if (getConfigList(checkFunc, checkList, false)) {
            bool allEnabled = true;
            for (auto& i : checkList) {
                // Check if this is a not !
                bool toggle = false;
                if (i.at(0) == '!') {
                    i.erase(0);
                    toggle = true;
                }
                auto temp = getConfigOption(i);
                if (temp == m_configValues.end()) {
                    auto dep = additionalDependencies.find(i);
                    if (dep == additionalDependencies.end()) {
                        outputInfo("Unknown option in if dependency (" + i + ") for option (" + optionLower + ")");
                        allEnabled = false;
                    } else {
                        allEnabled = dep->second ^ toggle;
                    }
                } else {
                    // Check if this variable has been initialized already
                    if (temp > option) {
                        if (!passDependencyCheck(temp)) {
                            return false;
                        }
                    }
                    allEnabled = (temp->m_value == "1") ^ toggle;
                }
                if (!allEnabled) {
                    break;
                }
            }
            if (allEnabled) {
                // If all deps are enabled then enable
                toggleConfigValue(optionLower, true, true);
            }
        }
    }
    // Perform dependency check if not disabled
    if (option->m_value != "0") {
        // The following are the needed dependencies that must be enabled
        string checkFunc = optionLower + "_deps";
        vector<string> checkList;
        if (getConfigList(checkFunc, checkList, false)) {
            for (auto& i : checkList) {
                // Check if this is a not !
                bool toggle = false;
                if (i.at(0) == '!') {
                    i.erase(0, 1);
                    toggle = true;
                }
                bool enabled;
                auto temp = getConfigOption(i);
                if (temp == m_configValues.end()) {
                    auto dep = additionalDependencies.find(i);
                    if (dep == additionalDependencies.end()) {
                        outputInfo("Unknown option in dependency (" + i + ") for option (" + optionLower + ")");
                        enabled = false;
                    } else {
                        enabled = dep->second ^ toggle;
                    }
                } else {
                    // Check if this variable has been initialized already
                    if (temp > option) {
                        if (!passDependencyCheck(temp)) {
                            return false;
                        }
                    }
                    enabled = (temp->m_value == "1") ^ toggle;
                }
                // If not all deps are enabled then disable
                if (!enabled) {
                    toggleConfigValue(optionLower, false);
                    outputInfo("Option (" + optionLower + ") was disabled due to an unmet dependency (" + i + ')');
                    break;
                }
            }
        }
    }
    // Perform dependency check if not disabled
    if (option->m_value != "0") {
        // Any 1 of the following dependencies are needed
        string checkFunc = optionLower + "_deps_any";
        vector<string> checkList;
        if (getConfigList(checkFunc, checkList, false)) {
            bool anyEnabled = false;
            for (auto& i : checkList) {
                // Check if this is a not !
                bool toggle = false;
                if (i.at(0) == '!') {
                    i.erase(0);
                    toggle = true;
                }
                auto temp = getConfigOption(i);
                if (temp == m_configValues.end()) {
                    auto dep = additionalDependencies.find(i);
                    if (dep == additionalDependencies.end()) {
                        outputInfo("Unknown option in any dependency (" + i + ") for option (" + optionLower + ")");
                        anyEnabled = false;
                    } else {
                        anyEnabled = dep->second ^ toggle;
                    }
                } else {
                    // Check if this variable has been initialized already
                    if (temp > option) {
                        if (!passDependencyCheck(temp)) {
                            return false;
                        }
                    }
                    anyEnabled = (temp->m_value == "1") ^ toggle;
                }
                if (anyEnabled) {
                    break;
                }
            }
            if (!anyEnabled) {
                // If not a single dep is enabled then disable
                toggleConfigValue(optionLower, false);
                string deps;
                for (const auto& j : checkList) {
                    if (!deps.empty()) {
                        deps += ',';
                    }
                    deps += j;
                }
                outputInfo("Option (" + optionLower + ") was disabled due to an unmet any dependency (" + deps + ')');
            }
        }
    }
    // Perform dependency check if not disabled
    if (option->m_value != "0") {
        // If conflict items are enabled then this one must be disabled
        string checkFunc = optionLower + "_conflict";
        vector<string> checkList;
        if (getConfigList(checkFunc, checkList, false)) {
            bool anyEnabled = false;
            for (auto& i : checkList) {
                // Check if this is a not !
                bool toggle = false;
                if (i.at(0) == '!') {
                    i.erase(0);
                    toggle = true;
                }
                auto temp = getConfigOption(i);
                if (temp == m_configValues.end()) {
                    auto dep = additionalDependencies.find(i);
                    if (dep == additionalDependencies.end()) {
                        outputInfo(
                            "Unknown option in conflict dependency (" + i + ") for option (" + optionLower + ")");
                        anyEnabled = false;
                    } else {
                        anyEnabled = dep->second ^ toggle;
                    }
                } else {
                    // Check if this variable has been initialized already
                    if (temp > option) {
                        if (!passDependencyCheck(temp)) {
                            return false;
                        }
                    }
                    anyEnabled = (temp->m_value == "1") ^ toggle;
                }
                if (anyEnabled) {
                    // If a single conflict is enabled then disable
                    toggleConfigValue(optionLower, false);
                    outputInfo("Option (" + optionLower + ") was disabled due to an conflict dependency (" + i + ')');
                    break;
                }
            }
        }
    }
    // Perform dependency check if not disabled
    if (option->m_value != "0") {
        // All select items are enabled when this item is enabled. If one of them has since been disabled then so must
        // this one
        string checkFunc = optionLower + "_select";
        vector<string> checkList;
        if (getConfigList(checkFunc, checkList, false)) {
            for (auto& i : checkList) {
                auto temp = getConfigOption(i);
                if (temp == m_configValues.end()) {
                    auto dep = additionalDependencies.find(i);
                    if (dep == additionalDependencies.end()) {
                        outputInfo("Unknown option in select dependency (" + i + ") for option (" + optionLower + ")");
                    } else if (!dep->second) {
                        // If any deps are disabled then disable
                        toggleConfigValue(optionLower, false);
                        outputInfo(
                            "Option (" + optionLower + ") was disabled due to an unmet select dependency (" + i + ')');
                        break;
                    }
                    continue;
                }
                // Check if this variable has been initialized already
                if (temp > option) {
                    if (!passDependencyCheck(temp)) {
                        return false;
                    }
                }
                if (temp->m_value == "0") {
                    // If any deps are disabled then disable
                    toggleConfigValue(optionLower, false);
                    outputInfo(
                        "Option (" + optionLower + ") was disabled due to an unmet select dependency (" + i + ')');
                    break;
                }
            }
        }
    }
    // Enable any required deps if still enabled
    if (option->m_value == "1") {
        // Perform a deep enable
        fastToggleConfigValue(optionLower, false);
        toggleConfigValue(optionLower, true, false, true);
    }
    return true;
}
