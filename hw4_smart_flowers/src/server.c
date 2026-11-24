#include "constants.h"
#include "flower_logic.h"
#include "gardener_logic.h"
#include "input.h"
#include "logger.h"
#include "model.h"
#include "network.h"
#include "statistics.h"
#include "server_internal.h"
#include "worker_protocol.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_TICK_MS 120
static int tick_ms = DEFAULT_TICK_MS;
Garden garden = {.mutex = PTHREAD_MUTEX_INITIALIZER,
                 .changed = PTHREAD_COND_INITIALIZER};
volatile sig_atomic_t interrupted = 0;

/** @brief Records a request to stop the server. */
static void stop_handler(int signal_number) {
    (void)signal_number;
    interrupted = 1;
}

/** @brief Sleeps for one configurable fraction of a simulation tick. */
static void sleep_ms(int millis) {
    struct timespec duration = {.tv_sec = millis / 1000,
                                .tv_nsec = (long)(millis % 1000) * 1000000L};
    while (nanosleep(&duration, &duration) == -1 && errno == EINTR) {
        if (interrupted) break;
    }
}

/** @brief Writes a line and marks a failed connection for closure. */
void send_to(Client *client, const char *text) {
    if (client->fd >= 0 && !send_line(client->fd, text)) {
        client->alive = 0;
        shutdown(client->fd, SHUT_RDWR);
    }
}

/** @brief Returns whether an entity has a connected worker. */
int worker_connected(int role, int id) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        Client *client = &garden.clients[i];
        if (client->alive && client->role == role && client->id == id) return 1;
    }
    return 0;
}

/** @brief Sends the whole garden state to one observer. */
void send_snapshot(Client *client) {
    char line[256];
    int alive = 0, dry = 0, overwatered = 0, waterings = 0;
    for (int i = 0; i < garden.settings.flower_count; ++i) {
        Flower *flower = &garden.flowers[i];
        waterings += flower->watering_count;
        if (flower->state == FLOWER_DRY) dry++;
        else if (flower->state == FLOWER_DEAD_OVERWATERED) overwatered++;
        else alive++;
    }
    snprintf(line, sizeof(line), "SNAPSHOT tick=%d alive=%d dry=%d overwatered=%d waterings=%d\n",
             garden.tick, alive, dry, overwatered, waterings);
    send_to(client, line);
    for (int i = 0; i < garden.settings.flower_count && client->alive; ++i) {
        Flower *flower = &garden.flowers[i];
        snprintf(line, sizeof(line), "FLOWER %d state=%s moisture=%d request=%d gardener=%d waterings=%d\n",
                 flower->id, flower_state_name(flower->state), flower->moisture,
                 flower->request_active, flower->assigned_gardener, flower->watering_count);
        send_to(client, line);
    }
    for (int i = 0; i < garden.settings.gardener_count && client->alive; ++i) {
        Gardener *gardener = &garden.gardeners[i];
        snprintf(line, sizeof(line), "GARDENER %d busy=%d flower=%d work_left=%d\n",
                 gardener->id, gardener->busy, gardener->flower_index == NO_FLOWER_INDEX
                 ? 0 : gardener->flower_index + 1, gardener->work_left);
        send_to(client, line);
    }
    if (client->alive) send_to(client, "END_SNAPSHOT\n");
}

/** @brief Broadcasts role-specific state and full observer snapshots. */
static void publish_state(void) {
    char line[256];
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        Client *client = &garden.clients[i];
        if (!client->alive) continue;
        if (client->role == 3) {
            send_snapshot(client);
        } else if (client->role == 1) {
            Flower *f = &garden.flowers[client->id - 1];
            snprintf(line, sizeof(line), "STATE tick=%d flower=%d state=%s moisture=%d request=%d\n",
                     garden.tick, f->id, flower_state_name(f->state), f->moisture,
                     f->request_active);
            send_to(client, line);
        } else {
            Gardener *g = &garden.gardeners[client->id - 1];
            snprintf(line, sizeof(line), "STATE tick=%d gardener=%d busy=%d flower=%d\n",
                     garden.tick, g->id, g->busy, g->flower_index + 1);
            send_to(client, line);
        }
    }
}

/** @brief Finds a connected worker under the model mutex. */
static Client *find_worker(int role, int id) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        Client *client = &garden.clients[i];
        if (client->alive && client->role == role && client->id == id)
            return client;
    }
    return NULL;
}

/** @brief Waits briefly for a registration or a worker's result. */
static void wait_for_change(int millis) {
    struct timespec limit;
    clock_gettime(CLOCK_REALTIME, &limit);
    limit.tv_nsec += (long)(millis % 1000) * 1000000L;
    limit.tv_sec += millis / 1000 + limit.tv_nsec / 1000000000L;
    limit.tv_nsec %= 1000000000L;
    pthread_cond_timedwait(&garden.changed, &garden.mutex, &limit);
}

/** @brief Waits until all configured workers have joined the session. */
static int wait_for_workers(void) {
    pthread_mutex_lock(&garden.mutex);
    while (!interrupted) {
        int ready = 1;
        for (int i = 1; i <= garden.settings.flower_count; ++i)
            if (!worker_connected(1, i)) ready = 0;
        for (int i = 1; i <= garden.settings.gardener_count; ++i)
            if (!worker_connected(2, i)) ready = 0;
        if (ready) break;
        wait_for_change(200);
    }
    int ok = !interrupted;
    pthread_mutex_unlock(&garden.mutex);
    return ok;
}

/** @brief Sends one task and waits for its result or reconnection. */
static int run_worker(int role, int index) {
    const int id = index + 1;
    const int deadline_ms = 3000;
    int waiting_time = 0;
    char job[WORKER_LINE_SIZE];
    pthread_mutex_lock(&garden.mutex);
    while (!interrupted) {
        int completed = role == 1 ? garden.flower_step[index] : garden.gardener_step[index];
        if (completed == garden.tick) break;
        Client *client = find_worker(role, id);
        if (!client) {
            waiting_time = 0;
            wait_for_change(200);
            continue;
        }
        if (!client->pending_tick) {
            int ok = role == 1
                ? make_flower_job(job, sizeof(job), garden.tick,
                                  &garden.flowers[index], &garden.settings)
                : make_gardener_job(job, sizeof(job), garden.tick,
                                    &garden.gardeners[index], garden.flowers,
                                    &garden.settings);
            if (!ok) {
                interrupted = 1;
                break;
            }
            client->pending_tick = garden.tick;
            send_to(client, job);
            waiting_time = 0;
        }
        if (!client->alive) continue;
        wait_for_change(200);
        waiting_time += 200;
        if (waiting_time >= deadline_ms && client->alive && client->pending_tick) {
            log_event("Работник %d:%d не ответил, ожидание переподключения\n", role, id);
            client->alive = 0;
            shutdown(client->fd, SHUT_RDWR);
        }
    }
    int ok = !interrupted;
    pthread_mutex_unlock(&garden.mutex);
    return ok;
}

/** @brief Advances ticks only through results computed by TCP workers. */
static void *run_ticks(void *unused) {
    (void)unused;
    log_event("Ожидание подключения всех цветов и садовников...\n");
    if (wait_for_workers()) {
        for (int t = 1; t <= OBSERVATION_PERIOD && !interrupted; ++t) {
            pthread_mutex_lock(&garden.mutex);
            garden.tick = t;
            log_event("Такт %d\n", t);
            pthread_mutex_unlock(&garden.mutex);
            int ok = 1;
            for (int i = 0; i < garden.settings.flower_count && ok; ++i)
                ok = run_worker(1, i);
            for (int i = 0; i < garden.settings.gardener_count && ok; ++i)
                ok = run_worker(2, i);
            if (!ok) break;
            pthread_mutex_lock(&garden.mutex);
            publish_state();
            int done = all_flowers_inactive(garden.flowers, garden.settings.flower_count);
            pthread_mutex_unlock(&garden.mutex);
            if (done) break;
            sleep_ms(tick_ms);
        }
    }
    pthread_mutex_lock(&garden.mutex);
    garden.finished = 1;
    char end[96];
    snprintf(end, sizeof(end), "FINISH tick=%d\n", garden.tick);
    for (int i = 0; i < MAX_CLIENTS; ++i)
        if (garden.clients[i].alive) send_to(&garden.clients[i], end);
    pthread_cond_broadcast(&garden.changed);
    pthread_mutex_unlock(&garden.mutex);
    return NULL;
}

/** @brief Reads the settings, opens TCP, and runs the simulation. */
int main(int argc, char **argv) {
    int port;
    const char *delay = getenv("SMART_FLOWERS_TICK_MS");
    if (delay && *delay) {
        char *end;
        long parsed = strtol(delay, &end, 10);
        if (*end || parsed < 10 || parsed > 2000) {
            fprintf(stderr, "SMART_FLOWERS_TICK_MS must be between 10 and 2000\n");
            return 1;
        }
        tick_ms = (int)parsed;
    }
    if ((argc != 3 && argc != 4) || !parse_port(argv[2], &port)) {
        fprintf(stderr, "Usage: %s <bind_ip> <port> [settings.cfg]\n", argv[0]);
        return 1;
    }
    int ok = argc == 4 ? read_settings_from_file(argv[3], &garden.settings)
                       : read_settings(&garden.settings);
    if (!ok || !validate_settings(&garden.settings)) return 1;
    int listener = create_listener(argv[1], port);
    if (listener < 0) {
        perror("Cannot start TCP server");
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    struct sigaction action = {0};
    action.sa_handler = stop_handler;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    for (int i = 0; i < MAX_CLIENTS; ++i) garden.clients[i].fd = -1;
    for (int i = 0; i < garden.settings.flower_count; ++i)
        init_flower(&garden.flowers[i], i + 1, INITIAL_MOISTURE,
                    garden.settings.drying_rates[i]);
    for (int i = 0; i < garden.settings.gardener_count; ++i)
        init_gardener(&garden.gardeners[i], i + 1, garden.settings.gardener_speed);
    logger_init(DEFAULT_LOG_FILE);
    print_settings(&garden.settings);
    log_event("TCP server listening on %s:%d\n", argv[1], port);
    pthread_t ticks;
    if (pthread_create(&ticks, NULL, run_ticks, NULL) != 0) {
        close(listener);
        logger_close();
        return 1;
    }
    while (!interrupted) {
        pthread_mutex_lock(&garden.mutex);
        int done = garden.finished;
        pthread_mutex_unlock(&garden.mutex);
        if (done) break;
        struct pollfd wait_fd = {.fd = listener, .events = POLLIN};
        int ready = poll(&wait_fd, 1, 80);
        if (ready > 0 && (wait_fd.revents & POLLIN)) accept_client(listener);
        if (ready < 0 && errno != EINTR) break;
    }
    interrupted = 1;
    pthread_join(ticks, NULL);
    close(listener);
    pthread_mutex_lock(&garden.mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (garden.clients[i].fd >= 0) shutdown(garden.clients[i].fd, SHUT_RDWR);
    }
    pthread_mutex_unlock(&garden.mutex);
    for (int attempts = 0; attempts < 40; ++attempts) {
        int active = 0;
        pthread_mutex_lock(&garden.mutex);
        for (int i = 0; i < MAX_CLIENTS; ++i) active += garden.clients[i].thread_running;
        pthread_mutex_unlock(&garden.mutex);
        if (!active) break;
        sleep_ms(50);
    }
    print_final_statistics(garden.flowers, garden.settings.flower_count, garden.tick);
    logger_close();
    return 0;
}
