#include "simulation_internal.h"

#include "flower_logic.h"
#include "gardener_logic.h"
#include "request_logic.h"
#include "logger.h"

#include <errno.h>
#include <time.h>

/** @brief Acquires the shared model lock with a timeout. */
int lock_model(SimulationContext *simulation) {
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) return 0;
    deadline.tv_sec += 2;
    int result;
    do {
        result = sem_timedwait(&simulation->model_mutex, &deadline);
    } while (result == -1 && errno == EINTR);
    return result == 0;
}

/** @brief Wakes all gardeners when requests or shutdown occur. */
void wake_gardeners(SimulationContext *simulation) {
    for (int i = 0; i < simulation->settings.gardener_count; ++i) {
        sem_post(&simulation->gardener_notifications[i]);
    }
}

/** @brief Updates one flower until the parent requests shutdown. */
int flower_worker(SimulationContext *simulation, int index) {
    for (;;) {
        sleep_one_tick();
        if (!lock_model(simulation)) return 0;
        if (simulation->stop) {
            sem_post(&simulation->model_mutex);
            break;
        }

        int new_request = update_flower(&simulation->flowers[index],
                                        &simulation->settings,
                                        simulation->current_tick);
        if (new_request) {
            simulation->requests_created++;
            wake_gardeners(simulation);
        }
        sem_post(&simulation->model_mutex);
    }
    return 1;
}

/** @brief Accepts watering requests and services flowers in a child process. */
int gardener_worker(SimulationContext *simulation, int index) {
    Gardener *gardener = &simulation->gardeners[index];
    sem_t *notification = &simulation->gardener_notifications[index];

    for (;;) {
        int result;
        do {
            result = sem_wait(notification);
        } while (result == -1 && errno == EINTR);
        if (result == -1) return 0;
        if (!lock_model(simulation)) return 0;
        if (simulation->stop) {
            sem_post(&simulation->model_mutex);
            break;
        }

        update_gardener(gardener, simulation->flowers,
                        &simulation->settings, simulation->current_tick);
        if (!gardener->busy) {
            int claimed = find_claimed_request(simulation->flowers,
                                                simulation->settings.flower_count);
            if (claimed != NO_FLOWER_INDEX) {
                Flower *flower = &simulation->flowers[claimed];
                simulation->claim_conflicts++;
                log_event("[%02d] Садовник %d: конфликт за заявку цветка %d "
                          "(её уже принял садовник %d)\n",
                          simulation->current_tick, gardener->id, flower->id,
                          flower->assigned_gardener);
            }
        }
        sem_post(&simulation->model_mutex);

        for (;;) {
            if (!lock_model(simulation)) return 0;
            int busy = gardener->busy;
            int stopping = simulation->stop;
            sem_post(&simulation->model_mutex);
            if (!busy || stopping) break;

            sleep_one_tick();
            if (!lock_model(simulation)) return 0;
            if (simulation->stop) {
                sem_post(&simulation->model_mutex);
                break;
            }
            update_gardener(gardener, simulation->flowers,
                            &simulation->settings, simulation->current_tick);
            sem_post(&simulation->model_mutex);
        }
    }
    return 1;
}
