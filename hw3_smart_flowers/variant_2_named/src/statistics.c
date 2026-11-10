#include "statistics.h"

#include "logger.h"

/** @brief Summarizes flower survival, completed watering, and final states. */
void print_final_statistics(const Flower flowers[],
                            int flower_count,
                            int finished_tick) {
    int alive = 0;
    int dry = 0;
    int overwatered = 0;
    int total_waterings = 0;

    for (int i = 0; i < flower_count; ++i) {
        total_waterings += flowers[i].watering_count;

        if (flowers[i].state == FLOWER_DRY) {
            dry++;
        } else if (flowers[i].state == FLOWER_DEAD_OVERWATERED) {
            overwatered++;
        } else {
            alive++;
        }
    }

    log_event("\n===== ИТОГОВАЯ СТАТИСТИКА =====\n");
    log_event("Последний такт: %d\n", finished_tick);
    log_event("Живых цветов: %d\n", alive);
    log_event("Засохших цветов: %d\n", dry);
    log_event("Погибших от перелива: %d\n", overwatered);
    log_event("Всего поливов: %d\n", total_waterings);

    for (int i = 0; i < flower_count; ++i) {
        log_event("Цветок %d: состояние=%s, влажность=%d, поливов=%d\n",
                  flowers[i].id,
                  flower_state_name(flowers[i].state),
                  flowers[i].moisture,
                  flowers[i].watering_count);
    }
}
