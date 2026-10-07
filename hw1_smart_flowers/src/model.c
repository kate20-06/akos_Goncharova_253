#include "model.h"
#include "soft_assert.h"

#include <stddef.h>

const char *flower_state_name(FlowerState state) {
    switch (state) {
        case FLOWER_NORMAL: return "NORMAL";
        case FLOWER_WILTING: return "WILTING";
        case FLOWER_WATERING: return "WATERING";
        case FLOWER_RECOVERING: return "RECOVERING";
        case FLOWER_DRY: return "DRY";
        case FLOWER_DEAD_OVERWATERED: return "DEAD_OVERWATERED";
        default: return "UNKNOWN";
    }
}

const char *request_policy_name(RequestPolicy policy) {
    switch (policy) {
        case REQUEST_FIRST: return "first";
        case REQUEST_DRIEST: return "driest";
        default: return "unknown";
    }
}

void init_flower(Flower *flower, int id, int moisture, int drying_rate) {
    if (!SOFT_ASSERT(flower != NULL, "flower must not be NULL")) return;

    flower->id = id;
    flower->moisture = moisture;
    flower->drying_rate = drying_rate;
    flower->state = FLOWER_NORMAL;

    flower->request_active = 0;
    flower->assigned_gardener = NO_GARDENER_ID;

    flower->watering_left = 0;
    flower->recovery_left = 0;
    flower->protection_left = 0;

    flower->watering_count = 0;
}

void init_gardener(Gardener *gardener, int id, int speed) {
    if (!SOFT_ASSERT(gardener != NULL, "gardener must not be NULL")) return;

    gardener->id = id;
    gardener->busy = 0;
    gardener->flower_index = NO_FLOWER_INDEX;
    gardener->work_left = 0;
    gardener->speed = speed;
}

int flower_is_active(const Flower *flower) {
    if (!SOFT_ASSERT(flower != NULL, "flower must not be NULL")) return 0;

    return flower->state != FLOWER_DRY &&
           flower->state != FLOWER_DEAD_OVERWATERED;
}
