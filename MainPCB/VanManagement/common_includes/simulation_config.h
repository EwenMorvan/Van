/**
 * @file simulation_config.h
 * @brief Global configuration for energy simulation
 * 
 * This file controls whether simulation code is compiled or not.
 * Include this file in any source that needs simulation-aware compilation.
 */

#ifndef SIMULATION_CONFIG_H
#define SIMULATION_CONFIG_H

// Enable energy simulation (set to 0 to disable all simulation code)
// This will exclude all simulation-related code from compilation when disabled
#ifndef ENABLE_ENERGY_SIMULATION
#define ENABLE_ENERGY_SIMULATION 1
#endif

#endif // SIMULATION_CONFIG_H
