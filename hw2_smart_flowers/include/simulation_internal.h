#ifndef SIMULATION_INTERNAL_H
#define SIMULATION_INTERNAL_H

#include "model.h"
#include "constants.h"
#include <pthread.h>
#include <semaphore.h>

/** @brief Shared simulation state protected by model_mutex. */
typedef struct {
    const Settings *settings;
    Flower flowers[MAX_FLOWERS];
    Gardener gardeners[MAX_GARDENERS];

    /* All model fields, including stop/current_tick and statistics, are guarded. */
    pthread_mutex_t model_mutex;
    /* One independent notification semaphore per gardener. New requests
       wake every gardener, making competing claims observable. */
    sem_t gardener_notifications[MAX_GARDENERS];

    int stop;
    int current_tick;
    int requests_created;
    int claim_conflicts;
    int worker_error;
} SimulationContext;

/** @brief Worker-specific context passed to pthread_create. */
typedef struct {
    SimulationContext *simulation;
    int index;
} WorkerArgument;

/** @brief Sleeps for one simulation tick. */
void sleep_one_tick(void);

/** @brief Notifies every gardener of a new request or shutdown. */
void wake_gardeners(SimulationContext *simulation);

/** @brief Entry point for a flower thread. */
void *flower_worker(void *argument);

/** @brief Entry point for a gardener thread. */
void *gardener_worker(void *argument);

#endif
