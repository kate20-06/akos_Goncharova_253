#ifndef SIMULATION_INTERNAL_H
#define SIMULATION_INTERNAL_H

#include "model.h"
#include <mqueue.h>
#include <semaphore.h>
#include <sys/types.h>

#define WORKER_SLOTS (MAX_FLOWERS + MAX_GARDENERS)

typedef enum { MSG_REGISTER = 1, MSG_READY, MSG_WORK, MSG_RESULT, MSG_STOP } MessageType;

/** @brief Message returned by one worker to the garden controller. */
typedef struct {
    MessageType type;
    int gardener;
    int index;
    int tick;
    pid_t pid;
    int flower_index;
    int new_request;
    Flower flower;
    Gardener worker;
} WorkerMessage;

/** @brief Request sent to a worker through its private message queue. */
typedef struct {
    MessageType type;
    int tick;
    pid_t controller_pid;
    Settings settings;
    Flower flowers[MAX_FLOWERS];
    Gardener worker;
} WorkMessage;

/** @brief IPC state used by either controller or one independent worker. */
typedef struct {
    char session[32];
    Settings settings;
    Flower flowers[MAX_FLOWERS];
    Gardener gardeners[MAX_GARDENERS];
    pid_t worker_pids[WORKER_SLOTS];
    int ready_count;
    int requests_created;
    int claim_conflicts;
    mqd_t events;
    mqd_t replies[WORKER_SLOTS];
    sem_t *event_notice;
    int worker_slot;
    int created_events;
    int created_notice;
    unsigned char created_replies[WORKER_SLOTS];
} SimulationContext;

int valid_session(const char *session);
int create_simulation(SimulationContext *sim, const char *session, const Settings *settings);
int connect_worker(SimulationContext *sim, const char *session, int gardener, int index);
void close_simulation(SimulationContext *sim, int owner);
int send_work(SimulationContext *sim, int slot, MessageType type, int tick);
int receive_event(SimulationContext *sim, WorkerMessage *message);
int worker_loop(SimulationContext *sim, int gardener, int index);
void sleep_one_tick(void);

#endif
