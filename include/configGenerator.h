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

#ifndef _CONFIGGENERATOR_H_
#define _CONFIGGENERATOR_H_

#include "helperFunctions.h"
#include <map>
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
        string m_sOption;
        string m_sPrefix;
        string m_sValue;
        bool m_bLock;

        ConfigPair(const string & sOption, const string & sPrefix, const string & sValue) :
            m_sOption(sOption),
            m_sPrefix(sPrefix),
            m_sValue(sValue),
            m_bLock(false)
        {
        }
    };
    typedef vector<ConfigPair> ValuesList;
    typedef map<string, string> DefaultValuesList;
    typedef map<string, bool> DependencyList;
    typedef map<string, vector<string>> OptimisedConfigList;

    ValuesList m_vFixedConfigValues;
    ValuesList m_vConfigValues;
    uint m_uiConfigValuesEnd;
    string m_sConfigureFile;
    string m_sToolchain;
    bool m_bLibav;
    string m_sProjectName;
    string m_sRootDirectory;
    string m_sSolutionDirectory;
    string m_sOutDirectory;
    bool m_bDCEOnly;
    bool m_bUsingExistingConfig;
    DefaultValuesList m_mReplaceList;
    DefaultValuesList m_mASMReplaceList;
    bool m_bUseNASM;

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
    void deleteCreatedFiles();

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
     * @param stOption The option to change.
     * @return True if it succeeds, false if it fails.
     */
    bool changeConfig(const string & stOption);

    /**
     * Checks current config values and performs validation of requirements.
     * @return True if it succeeds, false if it fails.
     */
    bool passCurrentValues();

    /**
     * Makes a files path relative to the project directory.
     * @remark Assumes input file path is relative to the generator.
     * @param       sFileName    Filename of the file.
     * @param [out] sRetFileName Filename with the path modified.
     */
    void makeFileProjectRelative(const string & sFileName, string & sRetFileName);

    /**
     * Makes a files path relative to the generator directory.
     * @remark Assumes input file path is relative to the project.
     * @param       sFileName    Filename of the file.
     * @param [out] sRetFileName Filename with the path modified.
     */
    void makeFileGeneratorRelative(const string & sFileName, string & sRetFileName);

    void buildFixedValues(DefaultValuesList & mFixedValues);

    void buildReplaceValues(DefaultValuesList & mReplaceValues, DefaultValuesList & mASMReplaceValues);

    /**
     * Creates a list of config items that are automatically set and should be be set by the user.
     * @param [out] vReservedItems The reserved items.
     */
    void buildReservedValues(vector<string> & vReservedItems);

    /**
     * Creates a list of additional config option dependencies that are not available as actual config options.
     * @param [out] mAdditionalDependencies The additional dependencies.
     */
    void buildAdditionalDependencies(DependencyList & mAdditionalDependencies);

    /**
     * Creates a list of components that can be disabled based on the current configuration as better alternatives are
     * enabled.
     * @param [in,out] mOptimisedDisables The optimised disables.
     */
    void buildOptimisedDisables(OptimisedConfigList & mOptimisedDisables);

    /**
     * Creates a list of config options that must be forced to be enabled if the specified option is enabled.
     * @param          sOptionLower The enabled option (in lower case).
     * @param [in,out] vForceEnable The forced enable options.
     */
    void buildForcedEnables(string sOptionLower, vector<string> & vForceEnable);

    /**
    * Creates a list of config options that must be forced to be disabled if the specified option is disabled.
    * @param          sOptionLower The disabled option (in lower case).
    * @param [in,out] vForceEnable The forced enable options.
    */
    void buildForcedDisables(string sOptionLower, vector<string> & vForceDisable);

    /**
     * Creates a list of command line arguments that must be handled before all others.
     * @param [out] vEarlyArgs The early arguments.
     */
    void buildEarlyConfigArgs(vector<string> & vEarlyArgs);

    void buildObjects(const string & sTag, vector<string> & vObjects);

    bool getConfigList(const string & sList, vector<string> & vReturn, bool bForce = true, uint uiCurrentFilePos = string::npos);

    /**
     * Perform the equivalent of configures find_things function.
     * @param          sParam1        The first parameter.
     * @param          sParam2        The second parameter.
     * @param          sParam3        The third parameter.
     * @param [in,out] vReturn        Returns any detected configure defines.
     * @param [in,out] vReturnExterns (Optional) If non-null, returns any detected extern variables.
     * @return True if it succeeds, false if it fails.
     */
    bool passFindThings(const string & sParam1, const string & sParam2, const string & sParam3, vector<string> & vReturn, vector<string> * vReturnExterns = NULL);

    /**
     * Perform the equivalent of configures find_things_extern function.
     * @param          sParam1 The first parameter.
     * @param          sParam2 The second parameter.
     * @param          sParam3 The third parameter.
     * @param          sParam4 The fourth parameter.
     * @param [in,out] vReturn Returns any detected configure defines.
     * @return True if it succeeds, false if it fails.
     */
    bool passFindThingsExtern(const string & sParam1, const string & sParam2, const string & sParam3, const string & sParam4, vector<string> & vReturn);

    /**
     * Perform the equivalent of configures find_filters_extern function.
     * @param          sParam1      The first parameter.
     * @param [in,out] vReturn      Returns any detected configure defines.
     * @return True if it succeeds, false if it fails.
     */
    bool passFindFiltersExtern(const string & sParam1, vector<string> & vReturn);

    bool passAddSuffix(const string & sParam1, const string & sParam2, vector<string> & vReturn, uint uiCurrentFilePos = string::npos);

    bool passFilterOut(const string & sParam1, const string & sParam2, vector<string> & vReturn, uint uiCurrentFilePos);

    /**
     * Perform the equivalent of configures full_filter_name function.
     * @param          sParam1 The first parameter.
     * @param [in,out] sReturn The return.
     * @return True if it succeeds, false if it fails.
     */
    bool passFullFilterName(const string & sParam1, string & sReturn);

    bool passConfigList(const string & sPrefix, const string & sSuffix, const string & sList);

    bool passEnabledComponents(const string & sFile, const string & sStruct, const string & sName, const string & sList);

    /**
     * Sets up all default starting config values.
     * @return True if it succeeds, false if it fails.
     */
    bool buildDefaultValues();

    /**
     * Sets up all config values that have a forced value.
     * @return True if it succeeds, false if it fails.
     */
    bool buildForcedValues();

    /**
     * Update configuration option without performing any dependency option checks.
     * @param sOption The option to update.
     * @param bEnable True to enable, false to disable.
     * @return True if it succeeds, false if it fails.
     */
    bool fastToggleConfigValue(const string & sOption, bool bEnable);

    /**
     * Update configuration option and perform any dependency option updates as well.
     * @param sOption The option to update.
     * @param bEnable True to enable, false to disable.
     * @return True if it succeeds, false if it fails.
     */
    bool toggleConfigValue(const string & sOption, bool bEnable, bool bRecursive = false);

    /**
     * Gets configuration option.
     * @param sOption The options name.
     * @return The configuration option, m_vConfigValues.end() if option not found.
     */
    ValuesList::iterator getConfigOption(const string & sOption);

    /**
     * Gets configuration option with prefix (i.e. HAVE_, CONFIG_ etc.) included.
     * @param sOption The options name.
     * @return The configuration option, m_vConfigValues.end() if option not found.
     */
    ValuesList::iterator getConfigOptionPrefixed(const string & sOption);

    /**
     * Queries if a configuration option is enabled.
     * @param sOption The option.
     * @return True if the configuration option is enabled, false if not.
     */
    bool isConfigOptionEnabled(const string & sOption);

    /**
     * Queries if assembly is enabled.
     * @return True if asm is enabled, false if not.
     */
    bool isASMEnabled();

    /**
     * Gets minimum supported windows version from config file.
     * @param [out] uiMajor The version number major.
     * @param [out] uiMinor The version number minor.
     * @return True if it succeeds, false if it fails.
     */
    bool getMinWindowsVersion(uint & uiMajor, uint & uiMinor);

    bool passDependencyCheck(const ValuesList::iterator vitOption);
};

#endif