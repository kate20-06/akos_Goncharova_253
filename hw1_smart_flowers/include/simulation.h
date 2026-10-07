#ifndef SIMULATION_H
#define SIMULATION_H

#include "model.h"

/**
 * @brief Requests a safe termination of the simulation.
 *
 * This function is called by the SIGINT handler after Ctrl+C is pressed.
 */
void request_stop(void);

/**
 * @brief Starts the sequential simulation of flowers and gardeners.
 * @param settings Simulation parameters.
 */
void run_simulation(const Settings *settings);

#endif
