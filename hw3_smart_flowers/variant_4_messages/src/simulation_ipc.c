#include "simulation_internal.h"
#include "constants.h"
#include "model.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/** @brief Checks that the IPC session name is safe and short. */
int valid_session(const char *session) {
    if (!session) return 0;
    size_t len = strlen(session);
    if (len < 1 || len > 24) return 0;
    for (size_t i = 0; i < len; ++i)
        if (!((session[i] >= 'a' && session[i] <= 'z') ||
              (session[i] >= 'A' && session[i] <= 'Z') ||
              (session[i] >= '0' && session[i] <= '9') ||
              session[i] == '_' || session[i] == '-')) return 0;
    return 1;
}

/** @brief Creates a unique POSIX name for a session's resources. */
static void resource_name(char *dest, size_t cap, const char *session,
                          const char *type, int slot) {
    if (slot < 0) snprintf(dest, cap, "/sf4_%s_%s", session, type);
    else snprintf(dest, cap, "/sf4_%s_%s_%d", session, type, slot);
}

/** @brief Returns a deadline for bounded POSIX waits. */
static struct timespec deadline_after(int seconds) {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_sec += seconds;
    return t;
}

/** @brief Initializes invalid queue handles before opening them. */
static void init_context(SimulationContext *sim, const char *session) {
    memset(sim, 0, sizeof(*sim));
    snprintf(sim->session, sizeof(sim->session), "%s", session);
    sim->events = (mqd_t)-1;
    sim->event_notice = SEM_FAILED;
    sim->worker_slot = -1;
    for (int i = 0; i < WORKER_SLOTS; ++i) sim->replies[i] = (mqd_t)-1;
}

/** @brief Creates the event queue, reply queues and named event semaphore. */
int create_simulation(SimulationContext *sim, const char *session, const Settings *settings) {
    init_context(sim, session);
    sim->settings = *settings;
    for (int i = 0; i < settings->flower_count; ++i)
        init_flower(&sim->flowers[i], i + 1, MAX_MOISTURE, settings->drying_rates[i]);
    for (int i = 0; i < settings->gardener_count; ++i)
        init_gardener(&sim->gardeners[i], i + 1, settings->gardener_speed);

    char name[80];
    struct mq_attr events_attr = {.mq_maxmsg = 10, .mq_msgsize = sizeof(WorkerMessage)};
    struct mq_attr work_attr = {.mq_maxmsg = 1, .mq_msgsize = sizeof(WorkMessage)};
    resource_name(name, sizeof(name), session, "events", -1);
    sim->events = mq_open(name, O_CREAT | O_EXCL | O_RDWR, 0600, &events_attr);
    if (sim->events == (mqd_t)-1) return 0;
    sim->created_events = 1;
    resource_name(name, sizeof(name), session, "notice", -1);
    sim->event_notice = sem_open(name, O_CREAT | O_EXCL, 0600, 0);
    if (sim->event_notice == SEM_FAILED) { close_simulation(sim, 1); return 0; }
    sim->created_notice = 1;

    for (int i = 0; i < settings->flower_count; ++i) {
        resource_name(name, sizeof(name), session, "reply", i);
        sim->replies[i] = mq_open(name, O_CREAT | O_EXCL | O_RDWR, 0600, &work_attr);
        if (sim->replies[i] == (mqd_t)-1) { close_simulation(sim, 1); return 0; }
        sim->created_replies[i] = 1;
    }
    for (int i = 0; i < settings->gardener_count; ++i) {
        int slot = MAX_FLOWERS + i;
        resource_name(name, sizeof(name), session, "reply", slot);
        sim->replies[slot] = mq_open(name, O_CREAT | O_EXCL | O_RDWR, 0600, &work_attr);
        if (sim->replies[slot] == (mqd_t)-1) { close_simulation(sim, 1); return 0; }
        sim->created_replies[slot] = 1;
    }
    return 1;
}

/** @brief Opens the event queue and private queue of an existing worker. */
int connect_worker(SimulationContext *sim, const char *session, int gardener, int index) {
    init_context(sim, session);
    int slot = gardener ? MAX_FLOWERS + index : index;
    char name[80];
    resource_name(name, sizeof(name), session, "events", -1);
    sim->events = mq_open(name, O_WRONLY);
    if (sim->events == (mqd_t)-1) return 0;
    sim->created_events = 1;
    resource_name(name, sizeof(name), session, "notice", -1);
    sim->event_notice = sem_open(name, 0);
    if (sim->event_notice == SEM_FAILED) { close_simulation(sim, 0); return 0; }
    resource_name(name, sizeof(name), session, "reply", slot);
    sim->replies[slot] = mq_open(name, O_RDONLY);
    if (sim->replies[slot] == (mqd_t)-1) { close_simulation(sim, 0); return 0; }
    sim->worker_slot = slot;
    return 1;
}

/** @brief Closes handles and unlinks named resources in the controller. */
void close_simulation(SimulationContext *sim, int owner) {
    char name[80];
    if (sim->events != (mqd_t)-1) mq_close(sim->events);
    for (int i = 0; i < WORKER_SLOTS; ++i)
        if (sim->replies[i] != (mqd_t)-1) mq_close(sim->replies[i]);
    if (sim->event_notice != SEM_FAILED) sem_close(sim->event_notice);
    if (!owner) return;
    if (sim->created_events) {
        resource_name(name, sizeof(name), sim->session, "events", -1);
        mq_unlink(name);
    }
    if (sim->created_notice) {
        resource_name(name, sizeof(name), sim->session, "notice", -1);
        sem_unlink(name);
    }
    for (int i = 0; i < WORKER_SLOTS; ++i) {
        if (!sim->created_replies[i]) continue;
        resource_name(name, sizeof(name), sim->session, "reply", i);
        mq_unlink(name);
    }
    /* Remove ownership names even if their workers were killed. */
    for (int i = 0; i < sim->settings.flower_count; ++i) {
        snprintf(name, sizeof(name), "/sf4_%s_owner_0_%d", sim->session, i);
        sem_unlink(name);
    }
    for (int i = 0; i < sim->settings.gardener_count; ++i) {
        snprintf(name, sizeof(name), "/sf4_%s_owner_1_%d", sim->session, i);
        sem_unlink(name);
    }
}

/** @brief Sends a worker its current snapshot or a stop request. */
int send_work(SimulationContext *sim, int slot, MessageType type, int tick) {
    WorkMessage work = {.type = type, .tick = tick,
                        .controller_pid = getpid(), .settings = sim->settings};
    if (type == MSG_WORK) {
        memcpy(work.flowers, sim->flowers, sizeof(work.flowers));
        if (slot >= MAX_FLOWERS) work.worker = sim->gardeners[slot - MAX_FLOWERS];
    }
    struct timespec deadline = deadline_after(2);
    return mq_timedsend(sim->replies[slot], (const char *)&work,
                        sizeof(work), 0, &deadline) == 0;
}

/** @brief Waits for a notification before receiving an event. */
int receive_event(SimulationContext *sim, WorkerMessage *message) {
    struct timespec deadline = deadline_after(1);
    if (sem_timedwait(sim->event_notice, &deadline) != 0) return 0;
    return mq_receive(sim->events, (char *)message, sizeof(*message), NULL) == sizeof(*message);
}

/** @brief Sleeps for one simulated tick. */
void sleep_one_tick(void) {
    struct timespec delay = {.tv_nsec = 25000000L};
    nanosleep(&delay, NULL);
}
