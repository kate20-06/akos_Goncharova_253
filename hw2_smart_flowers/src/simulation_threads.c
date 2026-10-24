#include "simulation_internal.h"

#include "flower_logic.h"
#include "gardener_logic.h"
#include "request_logic.h"
#include "logger.h"

#include <errno.h>

/** @brief Signals every gardener that request availability may have changed. */
void wake_gardeners(SimulationContext *simulation) {
    for (int i = 0; i < simulation->settings->gardener_count; ++i) {
        sem_post(&simulation->gardener_notifications[i]);
    }
}

/** @brief Runs one flower thread and generates requests after moisture updates. */
void *flower_worker(void *argument) {
    WorkerArgument *worker = argument;
    SimulationContext *simulation = worker->simulation;
    Flower *flower = &simulation->flowers[worker->index];

    for (;;) {
        sleep_one_tick();
        pthread_mutex_lock(&simulation->model_mutex);
        if (simulation->stop) {
            pthread_mutex_unlock(&simulation->model_mutex);
            break;
        }

        int new_request = update_flower(flower, simulation->settings,
                                        simulation->current_tick);
        if (new_request) {
            simulation->requests_created++;
            /* Notify each gardener. The mutex prevents duplicate assignments. */
            wake_gardeners(simulation);
        }
        pthread_mutex_unlock(&simulation->model_mutex);
    }
    return NULL;
}

/** @brief Runs one gardener thread, receiving work through its semaphore. */
void *gardener_worker(void *argument) {
    WorkerArgument *worker = argument;
    SimulationContext *simulation = worker->simulation;
    Gardener *gardener = &simulation->gardeners[worker->index];
    sem_t *notification = &simulation->gardener_notifications[worker->index];

    for (;;) {
        int result;
        do {
            result = sem_wait(notification);
        } while (result == -1 && errno == EINTR);

        pthread_mutex_lock(&simulation->model_mutex);
        if (result == -1) {
            simulation->worker_error = 1;
            simulation->stop = 1;
            wake_gardeners(simulation);
            pthread_mutex_unlock(&simulation->model_mutex);
            break;
        }
        if (simulation->stop) {
            pthread_mutex_unlock(&simulation->model_mutex);
            break;
        }

        /* Choose and claim a request while holding the model mutex. */
        update_gardener(gardener, simulation->flowers,
                        simulation->settings, simulation->current_tick);
        if (!gardener->busy) {
            int claimed = find_claimed_request(simulation->flowers,
                                                simulation->settings->flower_count);
            if (claimed != NO_FLOWER_INDEX) {
                Flower *flower = &simulation->flowers[claimed];
                simulation->claim_conflicts++;
                log_event("[%02d] Садовник %d: конфликт за заявку цветка %d "
                          "(её уже принял садовник %d)\n",
                          simulation->current_tick, gardener->id, flower->id,
                          flower->assigned_gardener);
            }
        }
        pthread_mutex_unlock(&simulation->model_mutex);

        /* Keep working without holding the mutex between steps. */
        for (;;) {
            pthread_mutex_lock(&simulation->model_mutex);
            int busy = gardener->busy;
            int stopping = simulation->stop;
            pthread_mutex_unlock(&simulation->model_mutex);
            if (!busy || stopping) break;

            sleep_one_tick();
            pthread_mutex_lock(&simulation->model_mutex);
            if (simulation->stop) {
                pthread_mutex_unlock(&simulation->model_mutex);
                break;
            }
            update_gardener(gardener, simulation->flowers,
                            simulation->settings, simulation->current_tick);
            pthread_mutex_unlock(&simulation->model_mutex);
        }
    }
    return NULL;
}

