#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H

#include "model.h"

#include <pthread.h>
#include <signal.h>

#define MAX_CLIENTS 192

/** @brief One connected flower, gardener, or observer. */
typedef struct {
    int fd;
    int role;
    int id;
    int alive;
    int thread_running;
    int pending_tick;
} Client;

/** @brief Server-owned state shared by network threads. */
typedef struct {
    Settings settings;
    Flower flowers[MAX_FLOWERS];
    Gardener gardeners[MAX_GARDENERS];
    Client clients[MAX_CLIENTS];
    int flower_step[MAX_FLOWERS];
    int gardener_step[MAX_GARDENERS];
    int tick;
    int finished;
    pthread_mutex_t mutex;
    pthread_cond_t changed;
} Garden;

extern Garden garden;
extern volatile sig_atomic_t interrupted;

/** @brief Sends a message to one connected client. */
void send_to(Client *client, const char *text);
/** @brief Checks if a numbered worker is already connected. */
int worker_connected(int role, int id);
/** @brief Advances one flower at most once per tick. */
void step_flower(int index);
/** @brief Advances one gardener at most once per tick. */
void step_gardener(int index);
/** @brief Sends a complete garden snapshot to an observer. */
void send_snapshot(Client *client);
/** @brief Accepts a new TCP connection and starts its handler. */
void accept_client(int listener);

#endif
