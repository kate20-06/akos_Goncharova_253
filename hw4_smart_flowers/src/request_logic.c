#include "request_logic.h"

#include "constants.h"

/** @brief Checks whether a request can be claimed by a gardener. */
static int request_is_available(const Flower *flower) {
    return flower->request_active &&
           flower->assigned_gardener == NO_GARDENER_ID &&
           flower->state == FLOWER_WILTING;
}

/** @brief Returns the first unclaimed watering request. */
static int choose_first_request(const Flower flowers[], int flower_count) {
    for (int i = 0; i < flower_count; ++i) {
        if (request_is_available(&flowers[i])) return i;
    }

    return NO_FLOWER_INDEX;
}

/** @brief Returns the unclaimed request with the lowest moisture. */
static int choose_driest_request(const Flower flowers[], int flower_count) {
    int selected = NO_FLOWER_INDEX;

    for (int i = 0; i < flower_count; ++i) {
        if (!request_is_available(&flowers[i])) continue;

        if (selected == NO_FLOWER_INDEX ||
            flowers[i].moisture < flowers[selected].moisture) {
            selected = i;
        }
    }

    return selected;
}

/** @brief Chooses a request according to the configured selection policy. */
int choose_request(const Flower flowers[],
                          int flower_count,
                          RequestPolicy policy) {
    if (policy == REQUEST_DRIEST) {
        return choose_driest_request(flowers, flower_count);
    }

    return choose_first_request(flowers, flower_count);
}

/** @brief Finds a wilting flower whose request is already assigned. */
int find_claimed_request(const Flower flowers[], int flower_count) {
    for (int i = 0; i < flower_count; ++i) {
        if (flowers[i].request_active &&
            flowers[i].assigned_gardener != NO_GARDENER_ID &&
            flowers[i].state == FLOWER_WILTING) {
            return i;
        }
    }
    return NO_FLOWER_INDEX;
}
