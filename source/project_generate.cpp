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

#include <iostream>

int main(int argc, char** argv)
{
    cout << "Project generator..." << endl;
    //Pass the input configuration
    ProjectGenerator ProjectHelper;
    if (!ProjectHelper.m_ConfigHelper.passConfig(argc, argv)) {
        ProjectHelper.errorFunc();
    }

    //Delete any previously generated files
    ProjectHelper.m_ConfigHelper.deleteCreatedFiles();
    ProjectHelper.deleteCreatedFiles();

    //Output config.h and avutil.h
    if (!ProjectHelper.m_ConfigHelper.outputConfig()) {
        ProjectHelper.errorFunc();
    }

    //Generate desired configuration files
    if (!ProjectHelper.passAllMake()) {
        ProjectHelper.errorFunc();
    }
    cout << "Completed Successfully" << endl;
#if _DEBUG
    pressKeyToContinue();
#endif
    exit(0);
}