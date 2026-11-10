#include "simulation.h"
#include "simulation_internal.h"
#include "flower_logic.h"

#include "constants.h"
#include "logger.h"
#include "statistics.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

/** @brief Requests termination from the parent SIGINT handler. */
void request_stop(void) {
    stop_requested = 1;
}

/** @brief Sleeps for one simulation tick. */
void sleep_one_tick(void) {
    struct timespec delay = {
        .tv_sec = TICK_DURATION_MS / 1000,
        .tv_nsec = (long)(TICK_DURATION_MS % 1000) * 1000000L
    };
    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
        /* Continue sleeping after an interrupted system call. */
    }
}

/** @brief Initializes the shared model before creating children. */
static void initialize_model(SimulationContext *simulation,
                             const Settings *settings) {
    memset(simulation, 0, sizeof(*simulation));
    simulation->settings = *settings;
    for (int i = 0; i < settings->flower_count; ++i) {
        init_flower(&simulation->flowers[i], i + FIRST_ENTITY_ID,
                    INITIAL_MOISTURE, settings->drying_rates[i]);
    }
    for (int i = 0; i < settings->gardener_count; ++i) {
        init_gardener(&simulation->gardeners[i], i + FIRST_ENTITY_ID,
                      settings->gardener_speed);
    }
}

/** @brief Creates a uniquely named POSIX shared memory object. */
static SimulationContext *create_shared_model(char *name, size_t name_size) {
    if (snprintf(name, name_size, "/smart_flowers_%ld", (long)getpid()) >=
        (int)name_size) return MAP_FAILED;

    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) return MAP_FAILED;
    if (ftruncate(fd, (off_t)sizeof(SimulationContext)) == -1) {
        close(fd);
        shm_unlink(name);
        return MAP_FAILED;
    }
    void *memory = mmap(NULL, sizeof(SimulationContext), PROT_READ | PROT_WRITE,
                        MAP_SHARED, fd, 0);
    close(fd);
    if (memory == MAP_FAILED) shm_unlink(name);
    return memory;
}

/** @brief Reaps a child and reports unexpected worker termination. */
static int check_children(pid_t children[], int count, int reaped[]) {
    for (int i = 0; i < count; ++i) {
        if (reaped[i]) continue;
        int status;
        pid_t result = waitpid(children[i], &status, WNOHANG);
        if (result == 0) continue;
        if (result == children[i]) {
            reaped[i] = 1;
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return 0;
            /* Workers should not exit before the parent initiates shutdown. */
            return 0;
        }
        if (result == -1 && errno != EINTR) return 0;
    }
    return 1;
}

/** @brief Creates flower and gardener processes and supervises the simulation. */
int run_simulation(const Settings *settings) {
    if (settings == NULL) return 0;

    char shm_name[64];
    SimulationContext *simulation = create_shared_model(shm_name, sizeof(shm_name));
    if (simulation == MAP_FAILED) {
        perror("Не удалось создать разделяемую память");
        return 0;
    }
    initialize_model(simulation, settings);

    int ready = 0;
    int model_ready = 0;
    int success = 1;
    int finished_tick = 0;
    pid_t children[MAX_FLOWERS + MAX_GARDENERS];
    int reaped[MAX_FLOWERS + MAX_GARDENERS] = {0};
    int started = 0;
    sigset_t blocked, old_mask;
    int mask_changed = 0;

    if (sem_init(&simulation->model_mutex, 1, 1) == -1) {
        perror("Не удалось создать семафор модели");
        success = 0;
        goto finish;
    }
    model_ready = 1;
    for (int i = 0; i < settings->gardener_count; ++i) {
        if (sem_init(&simulation->gardener_notifications[i], 1, 0) == -1) {
            perror("Не удалось создать семафор садовника");
            success = 0;
            goto finish;
        }
        ready++;
    }

    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    if (sigprocmask(SIG_BLOCK, &blocked, &old_mask) == -1) {
        perror("Не удалось заблокировать SIGINT");
        success = 0;
        goto finish;
    }
    mask_changed = 1;

    pid_t parent_pid = getpid();

    /* Fork children while SIGINT is blocked to avoid startup races. */
    for (int i = 0; i < settings->flower_count + settings->gardener_count; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Не удалось создать процесс");
            success = 0;
            goto finish;
        }
        if (pid == 0) {
            /* Stop workers if the parent dies unexpectedly. */
            if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1 ||
                getppid() != parent_pid) _exit(EXIT_FAILURE);
            /* Children never handle Ctrl+C; the parent handles shutdown. */
            struct sigaction ignore = {.sa_handler = SIG_IGN};
            sigemptyset(&ignore.sa_mask);
            sigaction(SIGINT, &ignore, NULL);
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            int result = i < settings->flower_count
                ? flower_worker(simulation, i)
                : gardener_worker(simulation, i - settings->flower_count);
            munmap(simulation, sizeof(*simulation));
            _exit(result ? EXIT_SUCCESS : EXIT_FAILURE);
        }
        children[started++] = pid;
    }
    if (sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1) {
        success = 0;
        goto finish;
    }
    mask_changed = 0;

    log_event("Запущены процессы: цветы=%d, садовники=%d; 1 такт = %d мс\n",
              settings->flower_count, settings->gardener_count, TICK_DURATION_MS);

    for (int tick = 1; tick <= OBSERVATION_PERIOD; ++tick) {
        if (stop_requested) {
            log_event("\nМоделирование прервано пользователем (Ctrl+C).\n");
            break;
        }
        sleep_one_tick();
        if (!lock_model(simulation)) {
            success = 0;
            break;
        }
        simulation->current_tick = tick;
        finished_tick = tick;
        int inactive = all_flowers_inactive(simulation->flowers,
                                            settings->flower_count);
        sem_post(&simulation->model_mutex);

        if (stop_requested) {
            log_event("\nМоделирование прервано пользователем (Ctrl+C).\n");
            break;
        }
        if (!check_children(children, started, reaped)) {
            log_event("\nОдин из дочерних процессов завершился с ошибкой.\n");
            success = 0;
            break;
        }
        if (inactive) {
            log_event("\nВсе цветы прекратили активность.\n");
            break;
        }
    }
    if (finished_tick == OBSERVATION_PERIOD && !stop_requested) {
        log_event("\nПериод наблюдения (%d тактов) завершён.\n", OBSERVATION_PERIOD);
    }

finish:
    if (model_ready) {
        /* Workers observe stop under the shared lock. */
        if (lock_model(simulation)) {
            simulation->stop = 1;
            sem_post(&simulation->model_mutex);
        } else {
            success = 0;
        }
        for (int i = 0; i < ready; ++i) {
            sem_post(&simulation->gardener_notifications[i]);
        }
    }
    /* A worker may have died while holding the lock. Never wait forever. */
    if (!success) {
        for (int i = 0; i < started; ++i)
            if (!reaped[i]) kill(children[i], SIGTERM);
    }
    for (int i = 0; i < started; ++i) {
        if (reaped[i]) continue;
        int status;
        pid_t result;
        do {
            result = waitpid(children[i], &status, 0);
        } while (result == -1 && errno == EINTR);
        if (result != children[i] || !WIFEXITED(status) ||
            WEXITSTATUS(status) != EXIT_SUCCESS) success = 0;
    }
    if (mask_changed) sigprocmask(SIG_SETMASK, &old_mask, NULL);

    if (model_ready) {
        log_event("Заявок создано: %d; конфликтов за заявки: %d\n",
                  simulation->requests_created, simulation->claim_conflicts);
        print_final_statistics(simulation->flowers, settings->flower_count, finished_tick);
        for (int i = 0; i < ready; ++i)
            sem_destroy(&simulation->gardener_notifications[i]);
        sem_destroy(&simulation->model_mutex);
    }
    munmap(simulation, sizeof(*simulation));
    if (shm_unlink(shm_name) == -1) {
        perror("Не удалось удалить разделяемую память");
        success = 0;
    }
    return success;
}
