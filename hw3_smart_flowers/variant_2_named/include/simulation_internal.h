#ifndef SIMULATION_INTERNAL_H
#define SIMULATION_INTERNAL_H

#include "model.h"
#include <semaphore.h>

/** @brief Model data shared by all flower and gardener processes. */
typedef struct {
    Settings settings;
    Flower flowers[MAX_FLOWERS];
    Gardener gardeners[MAX_GARDENERS];

    /** @brief Named semaphore guarding model data across processes. */
    sem_t *model_mutex;
    /** @brief Named semaphores notifying gardeners about new requests. */
    sem_t *gardener_notifications[MAX_GARDENERS];

    int stop;
    int current_tick;
    int requests_created;
    int claim_conflicts;
    int worker_error;
} SimulationContext;

/** @brief Sleeps for one simulation tick. */
void sleep_one_tick(void);

/** @brief Notifies gardeners about new requests or shutdown. */
void wake_gardeners(SimulationContext *simulation);

/** @brief Runs one flower in a child process. */
int flower_worker(SimulationContext *simulation, int index);

/** @brief Runs one gardener in a child process. */
int gardener_worker(SimulationContext *simulation, int index);

/** @brief Waits until the shared model lock is acquired. */
int lock_model(SimulationContext *simulation);

#endif
