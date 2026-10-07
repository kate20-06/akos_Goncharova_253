#ifndef GARDENER_LOGIC_H
#define GARDENER_LOGIC_H

#include "model.h"

/**
 * @brief Updates all gardeners for one simulation tick.
 * @param gardeners Array of gardeners.
 * @param flowers Array of flowers.
 * @param settings Simulation parameters.
 * @param tick Current tick number.
 */
void update_all_gardeners(Gardener gardeners[],
                          Flower flowers[],
                          const Settings *settings,
                          int tick);

#endif
