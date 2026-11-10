#include "simulation.h"
#include "simulation_internal.h"
#include "flower_logic.h"
#include "constants.h"
#include "logger.h"
#include "statistics.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

/** @brief Requests safe termination after Ctrl+C. */
void request_stop(void) {
    stop_requested = 1;
}

/** @brief Returns milliseconds elapsed on a monotonic clock. */
static long long clock_millis(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/** @brief Checks a worker PID, treating zombies as stopped processes. */
static int process_alive(pid_t pid) {
    if (pid <= 0 || (kill(pid, 0) == -1 && errno == ESRCH)) return 0;
#ifdef __linux__
    char path[64], line[256];
    snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);
    FILE *file = fopen(path, "r");
    if (file) {
        int alive = 1;
        if (fgets(line, sizeof(line), file)) {
            char *name_end = strrchr(line, ')');
            if (name_end && name_end[2] == 'Z') alive = 0;
        }
        fclose(file);
        return alive;
    }
#endif
    return 1;
}

/** @brief Reports an absent or dead worker process. */
static int workers_alive(const SharedModel *model) {
    for (int i = 0; i < model->settings.flower_count; ++i)
        if (!process_alive(model->flower_pids[i])) return 0;
    for (int i = 0; i < model->settings.gardener_count; ++i)
        if (!process_alive(model->gardener_pids[i])) return 0;
    return 1;
}

/** @brief Waits for independently launched workers to register. */
static int wait_for_workers(SimulationContext *simulation) {
    int expected = simulation->shared->settings.flower_count +
                   simulation->shared->settings.gardener_count;
    long long deadline = clock_millis() + 15000;
    while (!stop_requested && clock_millis() < deadline) {
        if (!lock_model(simulation)) return 0;
        int ready = simulation->shared->ready_count;
        if (ready == expected) {
            simulation->shared->started = 1;
            sem_post(simulation->model_mutex);
            return 1;
        }
        sem_post(simulation->model_mutex);
        sleep_one_tick();
    }
    if (!stop_requested) fprintf(stderr, "Не все процессы подключились за 15 секунд.\n");
    return 0;
}

/** @brief Runs the controller without creating any child processes. */
int run_simulation(const Settings *settings, const char *session) {
    SimulationContext simulation;
    if (!create_simulation(&simulation, session, settings)) {
        perror("Не удалось создать IPC-ресурсы (возможно, имя занято)");
        return 0;
    }
    int success = 1;
    int finished_tick = 0;
    log_event("Сессия %s: ожидание %d процессов цветов и %d садовников\n",
              session, settings->flower_count, settings->gardener_count);
    if (!wait_for_workers(&simulation)) {
        success = 0;
        goto finish;
    }
    log_event("Все процессы подключены. Моделирование началось.\n");
    for (int tick = 1; tick <= OBSERVATION_PERIOD && !stop_requested; ++tick) {
        sleep_one_tick();
        if (!lock_model(&simulation)) {
            success = 0;
            break;
        }
        simulation.shared->current_tick = tick;
        finished_tick = tick;
        int inactive = all_flowers_inactive(simulation.shared->flowers,
                                            settings->flower_count);
        int alive = workers_alive(simulation.shared);
        sem_post(simulation.model_mutex);
        if (!alive) {
            fprintf(stderr, "Один из рабочих процессов завершился неожиданно.\n");
            success = 0;
            break;
        }
        if (inactive) {
            log_event("\nВсе цветы прекратили активность.\n");
            break;
        }
    }
    if (stop_requested) log_event("\nМоделирование прервано (Ctrl+C).\n");
    else if (finished_tick == OBSERVATION_PERIOD)
        log_event("\nПериод наблюдения (%d тактов) завершён.\n", OBSERVATION_PERIOD);
finish:
    if (lock_model(&simulation)) {
        simulation.shared->stop = 1;
        sem_post(simulation.model_mutex);
    } else success = 0;
    wake_gardeners(&simulation);
    /* Workers have a short interval to notice shutdown before unlinking. */
    for (int i = 0; i < 8; ++i) sleep_one_tick();
    log_event("Заявок создано: %d; конфликтов за заявки: %d\n",
              simulation.shared->requests_created,
              simulation.shared->claim_conflicts);
    print_final_statistics(simulation.shared->flowers, settings->flower_count,
                           finished_tick);
    close_simulation(&simulation, 1);
    return success;
}
