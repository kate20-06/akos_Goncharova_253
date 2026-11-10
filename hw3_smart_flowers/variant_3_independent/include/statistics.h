#ifndef STATISTICS_H
#define STATISTICS_H

#include "model.h"

/**
 * @brief Prints the final simulation statistics.
 * @param flowers Array of flowers.
 * @param flower_count Number of flowers.
 * @param finished_tick Number of the last completed tick.
 */
void print_final_statistics(const Flower flowers[],
                            int flower_count,
                            int finished_tick);

#endif
