#ifndef GARDENER_LOGIC_H
#define GARDENER_LOGIC_H

#include "model.h"

/**
 * @brief Advances one gardener by one independent work step.
 *
 * When idle, attempts an atomic claim of one available request. While busy,
 * progresses toward the claimed flower or performs watering. Must be called
 * with the model mutex held.
 */
void update_gardener(Gardener *gardener, Flower flowers[],
                     const Settings *settings, int tick);

#endif
