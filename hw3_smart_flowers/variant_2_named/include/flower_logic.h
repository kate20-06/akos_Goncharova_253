#ifndef FLOWER_LOGIC_H
#define FLOWER_LOGIC_H

#include "model.h"

/**
 * @brief Advances exactly one flower by one local time step.
 *
 * The caller must hold the shared model semaphore. A newly created request
 * returns 1 so that each gardener process can be notified.
 *
 * @return 1 if a new request was created, otherwise 0.
 */
int update_flower(Flower *flower, const Settings *settings, int tick);

/** @brief Checks if every flower is dry or dead; caller holds model mutex. */
int all_flowers_inactive(const Flower flowers[], int flower_count);

#endif
