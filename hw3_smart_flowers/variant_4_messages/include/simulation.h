#ifndef SIMULATION_H
#define SIMULATION_H

#include "model.h"

/** @brief Requests a safe termination of the simulation. */
void request_stop(void);

/** @brief Runs an independent-process simulation for a named session. */
int run_simulation(const Settings *settings, const char *session);

#endif
