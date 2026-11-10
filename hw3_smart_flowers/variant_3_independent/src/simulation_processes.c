#include "simulation_internal.h"
#include "flower_logic.h"
#include "gardener_logic.h"
#include "request_logic.h"
#include "logger.h"

#include <errno.h>
#include <time.h>

/** @brief Waits for the controller to start the complete worker set. */
static int wait_for_start(SimulationContext *simulation) {
    for (;;) {
        if (!lock_model(simulation)) return 0;
        int started = simulation->shared->started;
        int stop = simulation->shared->stop;
        sem_post(simulation->model_mutex);
        if (stop || !controller_alive(simulation)) return 0;
        if (started) return 1;
        sleep_one_tick();
    }
}

/** @brief Waits for notification while checking controller liveness. */
static int wait_for_notification(SimulationContext *simulation, sem_t *notification) {
    for (;;) {
        struct timespec deadline;
        if (clock_gettime(CLOCK_REALTIME, &deadline) == -1) return 0;
        deadline.tv_nsec += 100000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }
        if (sem_timedwait(notification, &deadline) == 0) return 1;
        if (errno == EINTR) continue;
        if (errno != ETIMEDOUT || !controller_alive(simulation)) return 0;
    }
}

/** @brief Updates an individual flower until the simulation stops. */
int flower_worker(SimulationContext *simulation, int index) {
    if (!wait_for_start(simulation)) return 0;
    for (;;) {
        sleep_one_tick();
        if (!lock_model(simulation)) return 0;
        SharedModel *model = simulation->shared;
        if (model->stop) {
            sem_post(simulation->model_mutex);
            break;
        }
        int new_request = update_flower(&model->flowers[index],
                                        &model->settings, model->current_tick);
        if (new_request) {
            model->requests_created++;
            wake_gardeners(simulation);
        }
        sem_post(simulation->model_mutex);
    }
    return 1;
}

/** @brief Receives requests and waters flowers in a separate process. */
int gardener_worker(SimulationContext *simulation, int index) {
    if (!wait_for_start(simulation)) return 0;
    SharedModel *model = simulation->shared;
    Gardener *gardener = &model->gardeners[index];
    sem_t *notification = simulation->gardener_notifications[index];
    for (;;) {
        if (!wait_for_notification(simulation, notification)) return 0;
        if (!lock_model(simulation)) return 0;
        if (model->stop) {
            sem_post(simulation->model_mutex);
            break;
        }
        update_gardener(gardener, model->flowers, &model->settings, model->current_tick);
        if (!gardener->busy) {
            int claimed = find_claimed_request(model->flowers,
                                                model->settings.flower_count);
            if (claimed != NO_FLOWER_INDEX) {
                Flower *flower = &model->flowers[claimed];
                model->claim_conflicts++;
                log_event("[%02d] Садовник %d: конфликт за заявку цветка %d "
                          "(её уже принял садовник %d)\n",
                          model->current_tick, gardener->id, flower->id,
                          flower->assigned_gardener);
            }
        }
        sem_post(simulation->model_mutex);
        for (;;) {
            if (!lock_model(simulation)) return 0;
            int busy = gardener->busy;
            int stopping = model->stop;
            sem_post(simulation->model_mutex);
            if (!busy || stopping) break;
            sleep_one_tick();
            if (!lock_model(simulation)) return 0;
            if (model->stop) {
                sem_post(simulation->model_mutex);
                break;
            }
            update_gardener(gardener, model->flowers,
                            &model->settings, model->current_tick);
            sem_post(simulation->model_mutex);
        }
    }
    return 1;
}
