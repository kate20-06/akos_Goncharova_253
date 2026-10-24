#ifndef SIMULATION_H
#define SIMULATION_H

#include "model.h"

/**
 * @brief Requests a safe termination of the simulation.
 *
 * This function is called by the SIGINT handler after Ctrl+C is pressed.
 */
void request_stop(void);

/** @brief Runs asynchronous pthread flower and gardener workers; 1 on success. */
int run_simulation(const Settings *settings);

#endif
