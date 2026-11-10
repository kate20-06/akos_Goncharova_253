#include "gardener_logic.h"
#include "request_logic.h"

#include "constants.h"
#include "logger.h"
#include "soft_assert.h"
#include <stddef.h>

/** @brief Clears the gardener assignment and remaining work. */
static void release_gardener(Gardener *gardener) {
    gardener->busy = 0;
    gardener->flower_index = NO_FLOWER_INDEX;
    gardener->work_left = 0;
}

/** @brief Claims a request and starts the approach phase. */
static void assign_request(Gardener *gardener,
                           Flower *flower,
                           int flower_index,
                           int tick) {
    gardener->busy = 1;
    gardener->flower_index = flower_index;
    gardener->work_left = GARDENER_APPROACH_WORK;
    flower->assigned_gardener = gardener->id;

    log_event("[%02d] Садовник %d: принял заявку цветка %d\n",
              tick, gardener->id, flower->id);
}

/** @brief Assigns an available request to an idle gardener, if any. */
static void try_take_request(Gardener *gardener,
                             Flower flowers[],
                             const Settings *settings,
                             int tick) {
    int index = choose_request(flowers,
                               settings->flower_count,
                               settings->request_policy);

    if (index != NO_FLOWER_INDEX) {
        assign_request(gardener, &flowers[index], index, tick);
    }
}

/** @brief Begins watering or handles a prohibited repeat watering. */
static void start_watering(Gardener *gardener,
                           Flower *flower,
                           const Settings *settings,
                           int tick) {
    if (!flower->request_active || flower->assigned_gardener != gardener->id) {
        release_gardener(gardener);
        return;
    }

    if (flower->protection_left > 0) {
        log_event("[%02d] Цветок %d: перелив, защитный интервал ещё %d такт(а)\n",
                  tick, flower->id, flower->protection_left);
        flower->state = FLOWER_DEAD_OVERWATERED;
        flower->request_active = 0;
        flower->assigned_gardener = NO_GARDENER_ID;
        log_event("[%02d] Цветок %d: заявка снята (перелив)\n",
                  tick, flower->id);
        release_gardener(gardener);
        return;
    }

    flower->state = FLOWER_WATERING;
    flower->watering_left = settings->watering_duration;
    gardener->work_left = settings->watering_duration;

    log_event("[%02d] Садовник %d: начал полив цветка %d\n",
              tick, gardener->id, flower->id);
}

/** @brief Advances the approach according to gardener speed. */
static void move_to_flower(Gardener *gardener,
                           Flower *flower,
                           const Settings *settings,
                           int tick) {
    if (gardener->work_left > 0) {
        gardener->work_left -= gardener->speed;
        if (gardener->work_left < 0) gardener->work_left = 0;

        log_event("[%02d] Садовник %d: направляется к цветку %d, осталось %d ед. работы\n",
                  tick, gardener->id, flower->id, gardener->work_left);
    }

    if (gardener->work_left == 0) {
        start_watering(gardener, flower, settings, tick);
    }
}

/** @brief Restores moisture, closes the request, and releases the gardener. */
static void finish_watering(Gardener *gardener,
                            Flower *flower,
                            const Settings *settings,
                            int tick) {
    int old_moisture = flower->moisture;

    flower->moisture = MAX_MOISTURE;
    flower->watering_count++;
    flower->protection_left = settings->protection_interval;
    flower->recovery_left = settings->recovery_duration;
    flower->request_active = 0;
    flower->assigned_gardener = NO_GARDENER_ID;

    log_event("[%02d] Садовник %d: закончил полив цветка %d, влажность %d -> %d\n",
              tick, gardener->id, flower->id, old_moisture, flower->moisture);
    log_event("[%02d] Цветок %d: заявка снята\n", tick, flower->id);

    flower->state = settings->recovery_duration > 0
                        ? FLOWER_RECOVERING
                        : FLOWER_NORMAL;

    release_gardener(gardener);
}

/** @brief Advances watering by one local gardener step. */
static void continue_watering(Gardener *gardener,
                              Flower *flower,
                              const Settings *settings,
                              int tick) {
    if (gardener->work_left > 0) {
        gardener->work_left--;
        flower->watering_left--;
        log_event("[%02d] Садовник %d: поливает цветок %d, осталось %d такт(а)\n",
                  tick, gardener->id, flower->id, gardener->work_left);
    }

    if (gardener->work_left == 0) {
        finish_watering(gardener, flower, settings, tick);
    }
}

/** @brief Advances one gardener; the caller holds the model mutex. */
void update_gardener(Gardener *gardener,
                            Flower flowers[],
                            const Settings *settings,
                            int tick) {
    if (!gardener->busy) {
        try_take_request(gardener, flowers, settings, tick);
        return;
    }

    if (!SOFT_ASSERT(gardener->flower_index >= 0 &&
                     gardener->flower_index < settings->flower_count,
                     "busy gardener must reference a valid flower")) {
        release_gardener(gardener);
        return;
    }

    Flower *flower = &flowers[gardener->flower_index];

    if (!flower_is_active(flower)) {
        log_event("[%02d] Садовник %d: прекратил обслуживание погибшего цветка %d\n",
                  tick, gardener->id, flower->id);
        release_gardener(gardener);
        return;
    }

    if (flower->state == FLOWER_WILTING) {
        move_to_flower(gardener, flower, settings, tick);
        return;
    }

    if (flower->state == FLOWER_WATERING) {
        continue_watering(gardener, flower, settings, tick);
    }
}

