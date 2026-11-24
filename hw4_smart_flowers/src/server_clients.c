#include "server_internal.h"
#include "network.h"
#include "worker_protocol.h"
#include "logger.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/** @brief Releases a client registration and wakes the scheduler. */
static void unregister_client(Client *client) {
    pthread_mutex_lock(&garden.mutex);
    client->alive = 0;
    client->pending_tick = 0;
    if (client->fd >= 0) {
        shutdown(client->fd, SHUT_RDWR);
        close(client->fd);
    }
    client->fd = -1;
    client->role = 0;
    client->thread_running = 0;
    pthread_cond_broadcast(&garden.changed);
    pthread_mutex_unlock(&garden.mutex);
}

/** @brief Validates and registers one TCP client. */
static int register_client(Client *client, const char *line) {
    int role = 0, id = 0, consumed = 0;
    if (strcmp(line, "HELLO OBSERVER") == 0) {
        role = 3;
    } else if (sscanf(line, "HELLO FLOWER %d %n", &id, &consumed) == 1 &&
               consumed > 0 && line[consumed] == '\0') {
        role = 1;
    } else if (sscanf(line, "HELLO GARDENER %d %n", &id, &consumed) == 1 &&
               consumed > 0 && line[consumed] == '\0') {
        role = 2;
    }
    if (!role || (role == 1 && (id < 1 || id > garden.settings.flower_count)) ||
        (role == 2 && (id < 1 || id > garden.settings.gardener_count))) return 0;
    pthread_mutex_lock(&garden.mutex);
    int accepted = !garden.finished &&
                   (role == 3 || !worker_connected(role, id));
    if (accepted) {
        client->role = role;
        client->id = id;
        client->alive = 1;
        client->pending_tick = 0;
        send_to(client, "WELCOME\n");
        if (role == 3) send_snapshot(client);
        log_event("Клиент подключён: роль=%d номер=%d\n", role, id);
        pthread_cond_broadcast(&garden.changed);
    }
    pthread_mutex_unlock(&garden.mutex);
    return accepted;
}

/** @brief Applies a completed worker step to the server-owned model. */
static int receive_step(Client *client, const char *line) {
    if (!client->pending_tick || garden.finished) return 0;
    int tick = client->pending_tick;
    if (client->role == 1) {
        Flower updated;
        if (!parse_flower_result(line, tick, client->id, &updated)) return 0;
        Flower *old = &garden.flowers[client->id - 1];
        if (updated.drying_rate != old->drying_rate) return 0;
        log_event("[%02d] Цветок %d: влажность %d -> %d, состояние %s\n",
                  tick, client->id, old->moisture, updated.moisture,
                  flower_state_name(updated.state));
        *old = updated;
        garden.flower_step[client->id - 1] = tick;
    } else if (client->role == 2) {
        Gardener updated;
        Flower changed;
        int index;
        if (!parse_gardener_result(line, tick, client->id, &updated,
                                   &index, &changed)) return 0;
        if (updated.speed != garden.gardeners[client->id - 1].speed ||
            (index >= 0 && (index >= garden.settings.flower_count ||
                            changed.id != index + 1))) return 0;
        garden.gardeners[client->id - 1] = updated;
        if (index >= 0) {
            Flower *old = &garden.flowers[index];
            log_event("[%02d] Садовник %d: цветок %d, влажность %d -> %d, состояние %s\n",
                      tick, client->id, changed.id, old->moisture,
                      changed.moisture, flower_state_name(changed.state));
            *old = changed;
        }
        garden.gardener_step[client->id - 1] = tick;
    } else {
        return 0;
    }
    client->pending_tick = 0;
    pthread_cond_broadcast(&garden.changed);
    return 1;
}

/** @brief Reads messages from one independently connected client. */
static void *serve_client(void *argument) {
    Client *client = argument;
    char line[WORKER_LINE_SIZE];
    int result;
    do {
        result = read_line(client->fd, line, sizeof(line));
        if (result == 0 || interrupted) goto leave;
    } while (result == -1);
    if (!register_client(client, line)) {
        send_to(client, "ERROR invalid role, number or duplicate worker\n");
        goto leave;
    }
    while (!interrupted) {
        pthread_mutex_lock(&garden.mutex);
        int done = garden.finished || !client->alive;
        pthread_mutex_unlock(&garden.mutex);
        if (done) break;
        result = read_line(client->fd, line, sizeof(line));
        if (result == 0) break;
        if (result == -1) continue;
        if (strcmp(line, "QUIT") == 0) break;
        pthread_mutex_lock(&garden.mutex);
        int ok = receive_step(client, line);
        if (!ok) send_to(client, "ERROR invalid or unexpected worker result\n");
        pthread_mutex_unlock(&garden.mutex);
        if (!ok) break;
    }
leave:
    unregister_client(client);
    return NULL;
}

/** @brief Accepts a connection and assigns a separate handler thread. */
void accept_client(int listener) {
    int fd = accept(listener, NULL, NULL);
    if (fd < 0) return;
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    pthread_mutex_lock(&garden.mutex);
    Client *slot = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!garden.clients[i].thread_running) {
            slot = &garden.clients[i];
            slot->fd = fd;
            slot->role = 0;
            slot->alive = 0;
            slot->pending_tick = 0;
            slot->thread_running = 1;
            break;
        }
    }
    pthread_mutex_unlock(&garden.mutex);
    if (!slot) {
        close(fd);
        return;
    }
    pthread_t thread;
    if (pthread_create(&thread, NULL, serve_client, slot) == 0) {
        pthread_detach(thread);
    } else {
        unregister_client(slot);
    }
}
