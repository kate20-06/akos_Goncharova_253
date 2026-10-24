#ifndef REQUEST_LOGIC_H
#define REQUEST_LOGIC_H

#include "model.h"

/** @brief Selects an unclaimed watering request according to the policy. */
int choose_request(const Flower flowers[], int flower_count, RequestPolicy policy);

/** @brief Finds a claimed wilting request, or NO_FLOWER_INDEX. */
int find_claimed_request(const Flower flowers[], int flower_count);

#endif
