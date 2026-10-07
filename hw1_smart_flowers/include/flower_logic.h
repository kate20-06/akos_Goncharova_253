#ifndef FLOWER_LOGIC_H
#define FLOWER_LOGIC_H

#include "model.h"

/**
 * @brief Updates all flowers for one simulation tick.
 * @param flowers Array of flowers.
 * @param settings Simulation parameters.
 * @param tick Current tick number.
 */
void update_all_flowers(Flower flowers[], const Settings *settings, int tick);

/**
 * @brief Checks whether all flowers have become inactive.
 * @param flowers Array of flowers.
 * @param flower_count Number of flowers.
 * @return 1 if no active flowers remain, otherwise 0.
 */
int all_flowers_inactive(const Flower flowers[], int flower_count);

#endif
