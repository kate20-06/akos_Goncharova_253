#ifndef SIMULATION_INTERNAL_H
#define SIMULATION_INTERNAL_H

#include "model.h"
#include <semaphore.h>
#include <sys/types.h>

/** @brief Model data stored in POSIX shared memory. */
typedef struct {
    Settings settings;
    Flower flowers[MAX_FLOWERS];
    Gardener gardeners[MAX_GARDENERS];
    pid_t controller_pid;
    pid_t flower_pids[MAX_FLOWERS];
    pid_t gardener_pids[MAX_GARDENERS];
    int ready_count;
    int started;
    int stop;
    int current_tick;
    int requests_created;
    int claim_conflicts;
} SharedModel;

/** @brief Process-local handles for a shared simulation. */
typedef struct {
    SharedModel *shared;
    sem_t *model_mutex;
    sem_t *gardener_notifications[MAX_GARDENERS];
    char session[25];
    int created_model_mutex;
    int created_notifications;
} SimulationContext;

/** @brief Validates an IPC session identifier. */
int valid_session(const char *session);

/** @brief Creates shared resources for the controller process. */
int create_simulation(SimulationContext *simulation, const char *session,
                      const Settings *settings);

/** @brief Connects an independently launched worker to existing resources. */
int connect_simulation(SimulationContext *simulation, const char *session);

/** @brief Closes local handles and optionally unlinks shared resources. */
void close_simulation(SimulationContext *simulation, int unlink_resources);

/** @brief Registers exactly one worker for a given flower or gardener. */
int register_worker(SimulationContext *simulation, int gardener, int index);

/** @brief Checks whether the controller is still alive. */
int controller_alive(SimulationContext *simulation);

/** @brief Sleeps for one simulation tick. */
void sleep_one_tick(void);

/** @brief Waits for the model lock with a bounded timeout. */
int lock_model(SimulationContext *simulation);

/** @brief Notifies gardeners about requests and shutdown. */
void wake_gardeners(SimulationContext *simulation);

/** @brief Runs one flower in its own process. */
int flower_worker(SimulationContext *simulation, int index);

/** @brief Runs one gardener in its own process. */
int gardener_worker(SimulationContext *simulation, int index);

#endif
