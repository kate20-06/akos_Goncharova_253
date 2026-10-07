#include "simulation.h"

#include "constants.h"
#include "flower_logic.h"
#include "gardener_logic.h"
#include "logger.h"
#include "statistics.h"
#include "soft_assert.h"

#include <signal.h>
#include <stddef.h>

static volatile sig_atomic_t stop_requested = 0;

void request_stop(void) {
    stop_requested = 1;
}

static void init_flowers(Flower flowers[], const Settings *settings) {
    for (int i = 0; i < settings->flower_count; ++i) {
        init_flower(&flowers[i],
                    i + FIRST_ENTITY_ID,
                    INITIAL_MOISTURE,
                    settings->drying_rates[i]);
    }
}

static void init_gardeners(Gardener gardeners[], const Settings *settings) {
    for (int i = 0; i < settings->gardener_count; ++i) {
        init_gardener(&gardeners[i],
                      i + FIRST_ENTITY_ID,
                      settings->gardener_speed);
    }
}

void run_simulation(const Settings *settings) {
    Flower flowers[MAX_FLOWERS];
    Gardener gardeners[MAX_GARDENERS];
    int finished_tick = 0;

    if (!SOFT_ASSERT(settings != NULL, "settings must not be NULL")) return;

    stop_requested = 0;
    init_flowers(flowers, settings);
    init_gardeners(gardeners, settings);

    for (int tick = 0; tick < OBSERVATION_PERIOD; ++tick) {
        finished_tick = tick;

        if (stop_requested) {
            log_event("\nМоделирование прервано пользователем.\n");
            break;
        }

        log_event("\n===== ТАКТ %d =====\n", tick);

        update_all_flowers(flowers, settings, tick);
        update_all_gardeners(gardeners, flowers, settings, tick);

        if (all_flowers_inactive(flowers, settings->flower_count)) {
            log_event("\nВсе цветы прекратили активность.\n");
            break;
        }
    }

    print_final_statistics(flowers, settings->flower_count, finished_tick);
}
