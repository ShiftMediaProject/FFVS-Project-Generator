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

#ifndef CONFIGGENERATOR_H
#define CONFIGGENERATOR_H

#include "helperFunctions.h"

#include <map>
#include <utility>
#include <vector>

class ConfigGenerator
{
    friend class ProjectGenerator;

private:
    class ConfigPair
    {
        friend class ConfigGenerator;
        friend class ProjectGenerator;

    private:
        string m_option;
        string m_prefix;
        string m_value;
        bool m_lock;

        ConfigPair(string option, string prefix, string value)
            : m_option(std::move(option))
            , m_prefix(std::move(prefix))
            , m_value(std::move(value))
            , m_lock(false)
        {}
    };

    using ValuesList = vector<ConfigPair>;
    using DefaultValuesList = map<string, string>;
    using DependencyList = map<string, bool>;
    using ConfigList = map<string, vector<string>>;
    using InterDependencies = map<string, vector<pair<vector<string>, vector<string>>>>;

    ValuesList m_fixedConfigValues;
    ValuesList m_configValues;
    uint m_configValuesEnd{};
    string m_configureFile;
    bool m_isLibav{false};
    string m_projectName;
    string m_rootDirectory;
    string m_solutionDirectory;
    string m_outDirectory;
    bool m_onlyDCE{false};
    bool m_usingExistingConfig{false};
    DefaultValuesList m_replaceList;
    DefaultValuesList m_replaceListASM;
    bool m_useNASM{true};
    ConfigList m_cachedConfigLists;

public:
    /** Default constructor. */
    ConfigGenerator();

    /**
     * Pass configuration options based on input values.
     * @param      argc The number of input options.
     * @param [in] argv If non-null, the the list of input options.
     * @return True if it succeeds, false if it fails.
     */
    bool passConfig(int argc, char** argv);

    /**
     * Outputs a new configurations files based on current internal settings.
     * @return True if it succeeds, false if it fails.
     */
    bool outputConfig();

    /** Deletes any files that may have been previously created by outputConfig. */
    void deleteCreatedFiles() const;

private:
    /**
     * Passes the configure file and loads all available options.
     * @return True if it succeeds, false if it fails.
     */
    bool passConfigureFile();

    /**
     * Passes an existing config.h file.
     * @return True if it succeeds, false if it fails.
     */
    bool passExistingConfig();

    /**
     * Change configuration options.
     * @param option The option to change.
     * @return True if it succeeds, false if it fails.
     */
    bool changeConfig(const string& option);

    /**
     * Checks current config values and performs validation of requirements.
     * @return True if it succeeds, false if it fails.
     */
    bool passCurrentValues();

    /**
     * Makes a files path relative to the project directory.
     * @remark Assumes input file path is relative to the generator.
     * @param       fileName    Filename of the file.
     * @param [out] retFileName Filename with the path modified.
     */
    void makeFileProjectRelative(const string& fileName, string& retFileName) const;

    /**
     * Makes a files path relative to the generator directory.
     * @remark Assumes input file path is relative to the project.
     * @param       fileName    Filename of the file.
     * @param [out] retFileName Filename with the path modified.
     */
    void makeFileGeneratorRelative(const string& fileName, string& retFileName) const;

    static void buildFixedValues(DefaultValuesList& fixedValues);

    /**
     * Builds a list of configuration options that need to be replaced with the returned values.
     * @param [in,out] replaceValues    The replace values for config.h.
     * @param [in,out] header           The header that must be output at top of config file.
     * @param [in,out] replaceValuesASM The replace values for config.asm.
     */
    void buildReplaceValues(DefaultValuesList& replaceValues, string& header, DefaultValuesList& replaceValuesASM);

    /**
     * Creates a list of config items that are automatically set and should be be set by the user.
     * @param [out] reservedItems The reserved items.
     */
    static void buildReservedValues(vector<string>& reservedItems);

    /**
     * Creates a list of additional config option dependencies that are not available as actual config options.
     * @param [out] additionalDependencies The additional dependencies.
     */
    void buildAdditionalDependencies(DependencyList& additionalDependencies);

    /**
     * Creates a list of additional dependencies between config options (in addition to _deps lists).
     * @param [out] interDependencies The additional dependencies. Format is <libName, <pair<{required conditions},
     * {dependencies}>>>
     */
    void buildInterDependencies(InterDependencies& interDependencies);

    /**
     * Creates a list of components that can be disabled based on the current configuration as better alternatives are
     * enabled.
     * @param [in,out] optimisedDisables The optimised disables.
     */
    static void buildOptimisedDisables(ConfigList& optimisedDisables);

    /**
     * Creates a list of config options that must be forced to be enabled if the specified option is enabled.
     * @param          optionLower The enabled option (in lower case).
     * @param [in,out] forceEnable The forced enable options.
     */
    void buildForcedEnables(const string& optionLower, vector<string>& forceEnable);

    /**
     * Creates a list of config options that must be forced to be disabled if the specified option is disabled.
     * @param          optionLower  The disabled option (in lower case).
     * @param [in,out] forceDisable The forced disable options.
     */
    void buildForcedDisables(const string& optionLower, vector<string>& forceDisable);

    /**
     * Creates a list of command line arguments that must be handled before all others.
     * @param [out] earlyArgs The early arguments.
     */
    static void buildEarlyConfigArgs(vector<string>& earlyArgs);

    void buildObjects(const string& tag, vector<string>& objects);

    bool getConfigList(
        const string& list, vector<string>& returnList, bool force = true, uint currentFilePos = string::npos);

    /**
     * Perform the equivalent of configures find_things function.
     * @param          param1        The first parameter.
     * @param          param2        The second parameter.
     * @param          param3        The third parameter.
     * @param [in,out] returnList    Returns any detected configure defines.
     * @param [in,out] returnExterns (Optional) If non-null, returns any detected extern variables.
     * @return True if it succeeds, false if it fails.
     */
    bool passFindThings(const string& param1, const string& param2, const string& param3, vector<string>& returnList,
        vector<string>* returnExterns = nullptr) const;

    /**
     * Perform the equivalent of configures find_things_extern function.
     * @param          param1     The first parameter.
     * @param          param2     The second parameter.
     * @param          param3     The third parameter.
     * @param          param4     The fourth parameter.
     * @param [in,out] returnList Returns any detected configure defines.
     * @return True if it succeeds, false if it fails.
     */
    bool passFindThingsExtern(const string& param1, const string& param2, const string& param3, const string& param4,
        vector<string>& returnList) const;

    /**
     * Perform the equivalent of configures find_filters_extern function.
     * @param          param1     The first parameter.
     * @param [in,out] returnList Returns any detected configure defines.
     * @return True if it succeeds, false if it fails.
     */
    bool passFindFiltersExtern(const string& param1, vector<string>& returnList) const;

    bool passAddSuffix(
        const string& param1, const string& param2, vector<string>& returnList, uint currentFilePos = string::npos);

    bool passFilterOut(const string& param1, const string& param2, vector<string>& returnList, uint currentFilePos);

    /**
     * Perform the equivalent of configures full_filter_name function.
     * @param          param1       The first parameter.
     * @param [in,out] returnString The return.
     * @return True if it succeeds, false if it fails.
     */
    bool passFullFilterName(const string& param1, string& returnString) const;

    bool passConfigList(const string& prefix, const string& suffix, const string& list);

    bool passEnabledComponents(const string& file, const string& structName, const string& name, const string& list);

    /**
     * Sets up all default starting config values.
     * @return True if it succeeds, false if it fails.
     */
    bool buildDefaultValues();

    bool buildAutoDetectValues();

    /**
     * Sets up all config values that have a forced value.
     * @return True if it succeeds, false if it fails.
     */
    bool buildForcedValues();

    /**
     * Update configuration option without performing any dependency option checks.
     * @param option The option to update.
     * @param enable True to enable, false to disable.
     * @param weak   (Optional) True to only change a value if it is not already set.
     * @returns True if it succeeds, false if it fails.
     */
    bool fastToggleConfigValue(const string& option, bool enable, bool weak = false);

    /**
     * Update configuration option and perform any dependency option updates as well.
     * @param option    The option to update.
     * @param enable    True to enable, false to disable.
     * @param weak      (Optional) True to only change a value if it is not already set.
     * @param deep      (Optional) True to also enable _select and _suggest dependencies.
     * @param recursive (Optional) True if the function has been called from within itself.
     * @returns True if it succeeds, false if it fails.
     */
    bool toggleConfigValue(
        const string& option, bool enable, bool weak = false, bool deep = false, bool recursive = false);

    /**
     * Gets configuration option.
     * @param option The options name.
     * @return The configuration option, m_configValues.end() if option not found.
     */
    ValuesList::iterator getConfigOption(const string& option);

    ValuesList::const_iterator getConfigOption(const string& option) const;

    /**
     * Gets configuration option with prefix (i.e. HAVE_, CONFIG_ etc.) included.
     * @param option The options name.
     * @return The configuration option, m_configValues.end() if option not found.
     */
    ValuesList::iterator getConfigOptionPrefixed(const string& option);

    ValuesList::const_iterator getConfigOptionPrefixed(const string& option) const;

    /**
     * Queries if a configuration option is enabled.
     * @param option The option.
     * @return True if the configuration option is enabled, false if not.
     */
    bool isConfigOptionEnabled(const string& option) const;

    /**
     * Queries if a configuration option exists.
     * @param option The option.
     * @return True if the configuration option is valid, false if not.
     */
    bool isConfigOptionValid(const string& option) const;

    /**
     * Queries if a configuration option with prefix (i.e. HAVE_, CONFIG_ etc.) exists.
     * @param option The option.
     * @return True if the configuration option is valid, false if not.
     */
    bool isConfigOptionValidPrefixed(const string& option) const;

    /**
     * Queries if assembly is enabled.
     * @return True if asm is enabled, false if not.
     */
    bool isASMEnabled() const;

    /**
     * Queries if cuda is enabled.
     * @returns True if cuda is enabled, false if not.
     */
    bool isCUDAEnabled() const;

    /**
     * Gets minimum supported windows version from config file.
     * @param [out] major The version number major.
     * @param [out] minor The version number minor.
     * @return True if it succeeds, false if it fails.
     */
    bool getMinWindowsVersion(uint& major, uint& minor) const;

    bool passDependencyCheck(const ValuesList::iterator& option);
};

#endif
