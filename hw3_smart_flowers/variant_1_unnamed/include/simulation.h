#ifndef SIMULATION_H
#define SIMULATION_H

#include "model.h"

/** @brief Requests a safe termination of the simulation. */
void request_stop(void);

/** @brief Runs flower and gardener processes using shared memory. */
int run_simulation(const Settings *settings);

#endif
