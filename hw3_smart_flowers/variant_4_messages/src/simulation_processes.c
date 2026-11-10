#include "simulation_internal.h"
#include "flower_logic.h"
#include "gardener_logic.h"
#include "constants.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/** @brief Sends a worker event and notifies the controller. */
static int post_event(SimulationContext *sim, const WorkerMessage *event) {
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 2;
    if (mq_timedsend(sim->events, (const char *)event, sizeof(*event), 0, &deadline)) return 0;
    return sem_post(sim->event_notice) == 0;
}

/** @brief Checks whether the controller is still running. */
static int controller_alive(pid_t pid) {
    if (pid <= 1 || (kill(pid, 0) == -1 && errno == ESRCH)) return 0;
#ifdef __linux__
    char path[64], line[256];
    snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);
    FILE *file = fopen(path, "r");
    if (file) {
        int alive = 1;
        if (fgets(line, sizeof(line), file)) {
            char *end = strrchr(line, ')');
            if (end && end[2] == 'Z') alive = 0;
        }
        fclose(file);
        return alive;
    }
#endif
    return 1;
}

/** @brief Waits for work while detecting controller failure. */
static int receive_work(SimulationContext *sim, WorkMessage *work, pid_t *controller) {
    for (int tries = 0; tries < 20; ++tries) {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec++;
        ssize_t length = mq_timedreceive(sim->replies[sim->worker_slot],
                                         (char *)work, sizeof(*work), NULL,
                                         &deadline);
        if (length == sizeof(*work)) {
            if (*controller && work->controller_pid != *controller) return 0;
            *controller = work->controller_pid;
            return 1;
        }
        if (errno == EINTR) continue;
        if (errno != ETIMEDOUT || (*controller && !controller_alive(*controller)))
            return 0;
    }
    return 0;
}

/** @brief Runs one independent flower or gardener process. */
int worker_loop(SimulationContext *sim, int gardener, int index) {
    WorkerMessage event = {.type = MSG_REGISTER, .gardener = gardener,
                           .index = index, .pid = getpid()};
    if (!post_event(sim, &event)) return 0;
    pid_t controller = 0;
    for (;;) {
        WorkMessage work;
        if (!receive_work(sim, &work, &controller)) return 0;
        if (work.type == MSG_STOP) return 1;
        if (work.type == MSG_READY) continue;
        if (work.type != MSG_WORK) return 0;

        memset(&event, 0, sizeof(event));
        event.type = MSG_RESULT;
        event.gardener = gardener;
        event.index = index;
        event.pid = getpid();
        event.tick = work.tick;
        event.flower_index = -1;
        if (!gardener) {
            event.flower_index = index;
            event.flower = work.flowers[index];
            event.new_request = update_flower(&event.flower, &work.settings, work.tick);
        } else {
            event.worker = work.worker;
            update_gardener(&event.worker, work.flowers, &work.settings, work.tick);
            if (event.worker.flower_index >= 0 &&
                event.worker.flower_index < work.settings.flower_count)
                event.flower_index = event.worker.flower_index;
            else if (work.worker.flower_index >= 0 &&
                     work.worker.flower_index < work.settings.flower_count)
                event.flower_index = work.worker.flower_index;
            if (event.flower_index >= 0)
                event.flower = work.flowers[event.flower_index];
        }
        if (!post_event(sim, &event)) return 0;
    }
}
