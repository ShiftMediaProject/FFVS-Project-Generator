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
    string m_sProjectDirectory;
    string m_sOutDirectory;
    bool m_bDCEOnly;
    bool m_bUsingExistingConfig;
    DefaultValuesList m_mReplaceList;
    DefaultValuesList m_mASMReplaceList;

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

    void buildFixedValues(DefaultValuesList & mFixedValues);

    void buildReplaceValues(DefaultValuesList & mReplaceValues, DefaultValuesList & mASMReplaceValues);

    void buildReservedValues(vector<string> & vReservedItems);

    void buildAdditionalDependencies(DependencyList & mAdditionalDependencies);

    void buildOptimisedDisables(OptimisedConfigList & mOptimisedDisables);

    void buildForcedEnables(string sOptionLower, vector<string> & vForceEnable);

    void buildForcedDisables(string sOptionLower, vector<string> & vForceDisable);

    void buildObjects(const string & sTag, vector<string> & vObjects);

    bool getConfigList(const string & sList, vector<string> & vReturn, bool bForce = true, uint uiCurrentFilePos = string::npos);

    /**
     * Perform the equivalent of configures find_things function.
     * @param          sParam1        The first parameter.
     * @param          sParam2        The second parameter.
     * @param          sParam3        The third parameter.
     * @param [in,out] vReturn        Returns and detected configure defines.
     * @param [in,out] vReturnExterns (Optional) If non-null, returns any detected extern variables.
     * @return True if it succeeds, false if it fails.
     */
    bool passFindThings(const string & sParam1, const string & sParam2, const string & sParam3, vector<string> & vReturn, vector<string> * vReturnExterns = NULL);

    bool passFindThingsExtern(const string & sParam1, const string & sParam2, const string & sParam3, vector<string> & vReturn);

    bool passAddSuffix(const string & sParam1, const string & sParam2, vector<string> & vReturn, uint uiCurrentFilePos = string::npos);

    bool passFilterOut(const string & sParam1, const string & sParam2, vector<string> & vReturn, uint uiCurrentFilePos);

    bool passConfigList(const string & sPrefix, const string & sSuffix, const string & sList);

    bool passEnabledComponents(const string & sFile, const string & sStruct, const string & sName, const string & sList);

    /**
     * Setups all default starting config values.
     * @return True if it succeeds, false if it fails.
     */
    bool buildDefaultValues();

    /**
     * Setsup all config values that have a forced value.
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

    bool passDependencyCheck(const ValuesList::iterator vitOption);
};

#endif