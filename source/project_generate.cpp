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
#include "projectGenerator.h"

int main(const int argc, char** argv)
{
    outputLine("Project generator (this may take several minutes, please wait)...");
    // Pass the input configuration
    ProjectGenerator projectGenerator;
    if (!projectGenerator.m_configHelper.passConfig(argc, argv)) {
        projectGenerator.errorFunc(false);
    }

    // Delete any previously generated files
    projectGenerator.m_configHelper.deleteCreatedFiles();
    projectGenerator.deleteCreatedFiles();

    // Output config.h and avutil.h
    if (!projectGenerator.m_configHelper.outputConfig()) {
        projectGenerator.errorFunc();
    }

    // Generate desired configuration files
    if (!projectGenerator.passAllMake()) {
        projectGenerator.errorFunc();
    }
    outputLine("Completed Successfully");
#if _DEBUG
    pressKeyToContinue();
#endif
    exit(0);
}