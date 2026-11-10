#include "simulation_internal.h"
#include "constants.h"
#include "logger.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/** @brief Builds a POSIX IPC name using the session identifier. */
static void ipc_name(char *output, size_t size, const char *session,
                     const char *kind, int index) {
    if (index < 0) snprintf(output, size, "/sf3_%s_%s", session, kind);
    else snprintf(output, size, "/sf3_%s_%s_%d", session, kind, index);
}

/** @brief Rejects empty or unsafe session identifiers. */
int valid_session(const char *session) {
    if (!session || !*session || strlen(session) > 24) return 0;
    for (const unsigned char *p = (const unsigned char *)session; *p; ++p) {
        if (!isalnum(*p) && *p != '_' && *p != '-') return 0;
    }
    return 1;
}

/** @brief Initializes flowers and gardeners in shared memory. */
static void initialize_model(SharedModel *model, const Settings *settings) {
    memset(model, 0, sizeof(*model));
    model->settings = *settings;
    model->controller_pid = getpid();
    for (int i = 0; i < settings->flower_count; ++i)
        init_flower(&model->flowers[i], i + FIRST_ENTITY_ID,
                    INITIAL_MOISTURE, settings->drying_rates[i]);
    for (int i = 0; i < settings->gardener_count; ++i)
        init_gardener(&model->gardeners[i], i + FIRST_ENTITY_ID,
                      settings->gardener_speed);
}

/** @brief Creates a shared-memory object and maps it into this process. */
static int map_model(SimulationContext *sim, int create) {
    char name[64];
    ipc_name(name, sizeof(name), sim->session, "shared", -1);
    int fd = shm_open(name, (create ? O_CREAT | O_EXCL : 0) | O_RDWR, 0600);
    if (fd == -1) return 0;
    if (create && ftruncate(fd, sizeof(SharedModel)) == -1) {
        close(fd);
        shm_unlink(name);
        return 0;
    }
    sim->shared = mmap(NULL, sizeof(SharedModel), PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, 0);
    close(fd);
    if (sim->shared == MAP_FAILED) {
        sim->shared = NULL;
        if (create) shm_unlink(name);
        return 0;
    }
    return 1;
}

/** @brief Opens the named semaphores required by this process. */
static int open_semaphores(SimulationContext *sim, int create) {
    char name[64];
    ipc_name(name, sizeof(name), sim->session, "lock", -1);
    sim->model_mutex = create
        ? sem_open(name, O_CREAT | O_EXCL, 0600, 1)
        : sem_open(name, 0);
    if (sim->model_mutex == SEM_FAILED) {
        sim->model_mutex = NULL;
        return 0;
    }
    if (create) sim->created_model_mutex = 1;
    for (int i = 0; i < sim->shared->settings.gardener_count; ++i) {
        ipc_name(name, sizeof(name), sim->session, "gardener", i);
        sim->gardener_notifications[i] = create
            ? sem_open(name, O_CREAT | O_EXCL, 0600, 0)
            : sem_open(name, 0);
        if (sim->gardener_notifications[i] == SEM_FAILED) {
            sim->gardener_notifications[i] = NULL;
            return 0;
        }
        if (create) sim->created_notifications++;
    }
    return 1;
}

/** @brief Prepares shared memory and semaphores for a new simulation. */
int create_simulation(SimulationContext *sim, const char *session,
                      const Settings *settings) {
    memset(sim, 0, sizeof(*sim));
    snprintf(sim->session, sizeof(sim->session), "%s", session);
    if (!map_model(sim, 1)) return 0;
    initialize_model(sim->shared, settings);
    if (!open_semaphores(sim, 1)) {
        close_simulation(sim, 1);
        return 0;
    }
    return 1;
}

/** @brief Attaches a worker to an existing simulation. */
int connect_simulation(SimulationContext *sim, const char *session) {
    memset(sim, 0, sizeof(*sim));
    snprintf(sim->session, sizeof(sim->session), "%s", session);
    if (!map_model(sim, 0)) return 0;
    if (!open_semaphores(sim, 0)) {
        close_simulation(sim, 0);
        return 0;
    }
    return 1;
}

/** @brief Closes handles and removes names when called by the controller. */
void close_simulation(SimulationContext *sim, int unlink_resources) {
    char name[64];
    for (int i = 0; i < MAX_GARDENERS; ++i) {
        if (sim->gardener_notifications[i]) {
            sem_close(sim->gardener_notifications[i]);
            sim->gardener_notifications[i] = NULL;
        }
        if (unlink_resources && i < sim->created_notifications) {
            ipc_name(name, sizeof(name), sim->session, "gardener", i);
            sem_unlink(name);
        }
    }
    if (sim->model_mutex) {
        sem_close(sim->model_mutex);
        sim->model_mutex = NULL;
    }
    if (unlink_resources && sim->created_model_mutex) {
        ipc_name(name, sizeof(name), sim->session, "lock", -1);
        sem_unlink(name);
    }
    if (sim->shared) {
        munmap(sim->shared, sizeof(SharedModel));
        sim->shared = NULL;
    }
    if (unlink_resources) {
        ipc_name(name, sizeof(name), sim->session, "shared", -1);
        shm_unlink(name);
    }
}

/** @brief Tests controller liveness, including an unreaped zombie. */
int controller_alive(SimulationContext *sim) {
    pid_t pid = sim->shared->controller_pid;
    if (pid <= 1 || (kill(pid, 0) == -1 && errno == ESRCH)) return 0;
#ifdef __linux__
    char path[64], line[256];
    snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);
    FILE *file = fopen(path, "r");
    if (file) {
        if (fgets(line, sizeof(line), file)) {
            char *after_name = strrchr(line, ')');
            if (after_name && after_name[2] == 'Z') {
                fclose(file);
                return 0;
            }
        }
        fclose(file);
    }
#endif
    return 1;
}

/** @brief Acquires the shared lock without waiting forever. */
int lock_model(SimulationContext *sim) {
    int attempts = 0;
    for (;;) {
        struct timespec deadline;
        if (clock_gettime(CLOCK_REALTIME, &deadline) == -1) return 0;
        deadline.tv_nsec += 100000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }
        if (sem_timedwait(sim->model_mutex, &deadline) == 0) return 1;
        if (errno == EINTR) continue;
        if (errno != ETIMEDOUT || !controller_alive(sim) || ++attempts >= 30) return 0;
    }
}

/** @brief Registers a worker under the shared semaphore. */
int register_worker(SimulationContext *sim, int gardener, int index) {
    if (!lock_model(sim)) return 0;
    SharedModel *model = sim->shared;
    int count = gardener ? model->settings.gardener_count
                         : model->settings.flower_count;
    pid_t *slots = gardener ? model->gardener_pids : model->flower_pids;
    int valid = index >= 0 && index < count && !model->stop &&
                !model->started && slots[index] == 0 && controller_alive(sim);
    if (valid) {
        slots[index] = getpid();
        model->ready_count++;
    }
    sem_post(sim->model_mutex);
    return valid;
}

/** @brief Sleeps for one simulation tick. */
void sleep_one_tick(void) {
    struct timespec delay = {
        .tv_sec = TICK_DURATION_MS / 1000,
        .tv_nsec = (long)(TICK_DURATION_MS % 1000) * 1000000L
    };
    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) { }
}

/** @brief Notifies all gardeners about requests or shutdown. */
void wake_gardeners(SimulationContext *sim) {
    for (int i = 0; i < sim->shared->settings.gardener_count; ++i)
        sem_post(sim->gardener_notifications[i]);
}
