#include "simulation.h"
#include "simulation_internal.h"
#include "flower_logic.h"

#include "constants.h"
#include "logger.h"
#include "statistics.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Only the main thread handles SIGINT. Workers inherit SIGINT blocked. */
static volatile sig_atomic_t stop_requested = 0;

/** @brief Sets the signal-safe flag requesting graceful shutdown. */
void request_stop(void) {
    stop_requested = 1;
}

/** @brief Sleeps for one configured time interval, retrying after EINTR. */
void sleep_one_tick(void) {
    struct timespec delay = {
        .tv_sec = TICK_DURATION_MS / 1000,
        .tv_nsec = (long)(TICK_DURATION_MS % 1000) * 1000000L
    };
    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
        /* Resume the remaining portion after a signal interruption. */
    }
}

/** @brief Creates initial flower and gardener states. */
static void initialize_model(SimulationContext *simulation,
                             const Settings *settings) {
    memset(simulation, 0, sizeof(*simulation));
    simulation->settings = settings;

    for (int i = 0; i < settings->flower_count; ++i) {
        init_flower(&simulation->flowers[i], i + FIRST_ENTITY_ID,
                    INITIAL_MOISTURE, settings->drying_rates[i]);
    }
    for (int i = 0; i < settings->gardener_count; ++i) {
        init_gardener(&simulation->gardeners[i], i + FIRST_ENTITY_ID,
                      settings->gardener_speed);
    }
}

/** @brief Creates workers, supervises execution, then joins and cleans up. */
int run_simulation(const Settings *settings) {
    if (settings == NULL) return 0;

    SimulationContext simulation;
    pthread_t flower_threads[MAX_FLOWERS];
    pthread_t gardener_threads[MAX_GARDENERS];
    WorkerArgument flower_args[MAX_FLOWERS];
    WorkerArgument gardener_args[MAX_GARDENERS];
    int semaphores_ready = 0;
    int flowers_started = 0;
    int gardeners_started = 0;
    int success = 1;
    int finished_tick = 0;
    sigset_t sigint_set;
    sigset_t original_mask;
    int mask_set = 0;

    initialize_model(&simulation, settings);
    if (pthread_mutex_init(&simulation.model_mutex, NULL) != 0) {
        fprintf(stderr, "Не удалось инициализировать мьютекс модели.\n");
        return 0;
    }
    for (int i = 0; i < settings->gardener_count; ++i) {
        if (sem_init(&simulation.gardener_notifications[i], 0, 0) != 0) {
            perror("Не удалось инициализировать семафор");
            success = 0;
            goto finish;
        }
        semaphores_ready++;
    }

    sigemptyset(&sigint_set);
    sigaddset(&sigint_set, SIGINT);
    int error = pthread_sigmask(SIG_BLOCK, &sigint_set, &original_mask);
    if (error != 0) {
        fprintf(stderr, "Ошибка настройки SIGINT: %s\n", strerror(error));
        success = 0;
        goto finish;
    }
    mask_set = 1;

    /* Create one thread for each flower and gardener. */
    for (int i = 0; i < settings->flower_count; ++i) {
        flower_args[i] = (WorkerArgument){&simulation, i};
        error = pthread_create(&flower_threads[i], NULL,
                               flower_worker, &flower_args[i]);
        if (error != 0) {
            fprintf(stderr, "Ошибка создания потока цветка %d: %s\n",
                    i + FIRST_ENTITY_ID, strerror(error));
            success = 0;
            goto finish;
        }
        flowers_started++;
    }
    for (int i = 0; i < settings->gardener_count; ++i) {
        gardener_args[i] = (WorkerArgument){&simulation, i};
        error = pthread_create(&gardener_threads[i], NULL,
                               gardener_worker, &gardener_args[i]);
        if (error != 0) {
            fprintf(stderr, "Ошибка создания потока садовника %d: %s\n",
                    i + FIRST_ENTITY_ID, strerror(error));
            success = 0;
            goto finish;
        }
        gardeners_started++;
    }

    /* Signals are now handled only by the main thread; workers still block
       SIGINT because they inherited its blocked mask at creation. */
    error = pthread_sigmask(SIG_SETMASK, &original_mask, NULL);
    if (error != 0) {
        fprintf(stderr, "Ошибка восстановления маски сигналов: %s\n", strerror(error));
        success = 0;
        goto finish;
    }
    mask_set = 0;

    log_event("Запущены потоки: цветы=%d, садовники=%d; 1 такт = %d мс\n",
              settings->flower_count, settings->gardener_count, TICK_DURATION_MS);

    /* The main thread supervises the time limit and shutdown; it does
       not sequentially iterate through flowers or gardeners. */
    for (int tick = 1; tick <= OBSERVATION_PERIOD; ++tick) {
        if (stop_requested) {
            log_event("\nМоделирование прервано пользователем (Ctrl+C).\n");
            break;
        }
        sleep_one_tick();
        pthread_mutex_lock(&simulation.model_mutex);
        simulation.current_tick = tick;
        finished_tick = tick;
        int inactive = all_flowers_inactive(simulation.flowers, settings->flower_count);
        int failure = simulation.worker_error;
        pthread_mutex_unlock(&simulation.model_mutex);

        if (stop_requested) {
            log_event("\nМоделирование прервано пользователем (Ctrl+C).\n");
            break;
        }
        if (failure) {
            log_event("\nОшибка рабочего потока; выполнение прекращено.\n");
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
    pthread_mutex_lock(&simulation.model_mutex);
    simulation.stop = 1;
    pthread_mutex_unlock(&simulation.model_mutex);

    /* Release workers blocked on sem_wait; sleep-based workers wake within
       one tick. All shared resources live until after pthread_join. */
    for (int i = 0; i < semaphores_ready; ++i) {
        sem_post(&simulation.gardener_notifications[i]);
    }
    for (int i = 0; i < flowers_started; ++i) pthread_join(flower_threads[i], NULL);
    for (int i = 0; i < gardeners_started; ++i) pthread_join(gardener_threads[i], NULL);
    if (mask_set) pthread_sigmask(SIG_SETMASK, &original_mask, NULL);

    log_event("Заявок создано: %d; конфликтов за заявки: %d\n",
              simulation.requests_created, simulation.claim_conflicts);
    print_final_statistics(simulation.flowers, settings->flower_count, finished_tick);

    for (int i = 0; i < semaphores_ready; ++i) {
        sem_destroy(&simulation.gardener_notifications[i]);
    }
    pthread_mutex_destroy(&simulation.model_mutex);
    return success;
}
