#include "flower_logic.h"

#include "constants.h"
#include "logger.h"

/** @brief Changes a flower state and records the event in the log. */
static void set_state(Flower *flower, FlowerState new_state, int tick) {
    if (flower->state == new_state) {
        return;
    }

    log_event("[%02d] Цветок %d: %s -> %s\n",
              tick,
              flower->id,
              flower_state_name(flower->state),
              flower_state_name(new_state));

    flower->state = new_state;
}

/** @brief Cancels the flower's active request. */
static void cancel_request(Flower *flower, int tick) {
    if (flower->request_active) {
        log_event("[%02d] Цветок %d: заявка снята\n", tick, flower->id);
    }

    flower->request_active = 0;
    flower->assigned_gardener = NO_GARDENER_ID;
}

/** @brief Creates a request if the flower is wilting and has no active request. */
static void create_request_if_needed(Flower *flower, int tick) {
    if (flower->state != FLOWER_WILTING || flower->request_active) {
        return;
    }

    flower->request_active = 1;
    flower->assigned_gardener = NO_GARDENER_ID;
    log_event("[%02d] Цветок %d: отправлена заявка на полив\n", tick, flower->id);
}

/** @brief Decreases the remaining repeat-watering protection time. */
static void decrease_protection(Flower *flower) {
    if (flower->protection_left > 0) {
        flower->protection_left--;
    }
}

/** @brief Decreases flower moisture according to its drying rate. */
static void dry_flower(Flower *flower, int tick) {
    int old_moisture = flower->moisture;

    flower->moisture -= flower->drying_rate;
    if (flower->moisture < MIN_MOISTURE) {
        flower->moisture = MIN_MOISTURE;
    }

    log_event("[%02d] Цветок %d: влажность %d -> %d\n",
              tick, flower->id, old_moisture, flower->moisture);
}

/** @brief Processes one recovery tick after watering. */
static void update_recovery(Flower *flower, int tick) {
    if (flower->recovery_left > 0) {
        flower->recovery_left--;
    }

    if (flower->recovery_left == 0) {
        set_state(flower, FLOWER_NORMAL, tick);
    }
}

/** @brief Determines the flower state after its moisture changes. */
static void update_flower_state(Flower *flower,
                                const Settings *settings,
                                int tick) {
    if (flower->moisture <= settings->dry_threshold) {
        set_state(flower, FLOWER_DRY, tick);
        cancel_request(flower, tick);
        return;
    }

    if (flower->moisture <= settings->request_threshold) {
        set_state(flower, FLOWER_WILTING, tick);
        create_request_if_needed(flower, tick);
        return;
    }

    set_state(flower, FLOWER_NORMAL, tick);
}

/** @brief Processes one simulation tick for a single flower. */
int update_flower(Flower *flower,
                  const Settings *settings,
                  int tick) {
    int had_request = flower->request_active;
    if (!flower_is_active(flower)) {
        return 0;
    }

    decrease_protection(flower);

    if (flower->state == FLOWER_WATERING) {
        return 0;
    }

    if (flower->state == FLOWER_RECOVERING) {
        update_recovery(flower, tick);
        return 0;
    }

    dry_flower(flower, tick);
    update_flower_state(flower, settings, tick);
    return !had_request && flower->request_active;
}

/** @brief Returns true if no flower can produce further events. */
int all_flowers_inactive(const Flower flowers[], int flower_count) {
    for (int i = 0; i < flower_count; ++i) {
        if (flower_is_active(&flowers[i])) {
            return 0;
        }
    }

    return 1;
}
