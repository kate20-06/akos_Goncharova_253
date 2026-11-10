#include "simulation.h"
#include "simulation_internal.h"
#include "constants.h"
#include "flower_logic.h"
#include "logger.h"
#include "statistics.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t stop_requested = 0;

/** @brief Requests safe shutdown after Ctrl+C. */
void request_stop(void) { stop_requested = 1; }

/** @brief Returns the current time in milliseconds. */
static long long clock_millis(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/** @brief Maps a worker role and index to its reply queue. */
static int worker_slot(int gardener, int index) {
    return gardener ? MAX_FLOWERS + index : index;
}

/** @brief Validates worker registration and rejects duplicate IDs. */
static int register_worker(SimulationContext *sim, WorkerMessage *event) {
    int count = event->gardener ? sim->settings.gardener_count
                               : sim->settings.flower_count;
    if (event->index < 0 || event->index >= count) return 0;
    int slot = worker_slot(event->gardener, event->index);
    if (sim->worker_pids[slot] != 0) return 0;
    sim->worker_pids[slot] = event->pid;
    sim->ready_count++;
    return send_work(sim, slot, MSG_READY, 0);
}

/** @brief Waits until the configured number of workers is connected. */
static int wait_for_workers(SimulationContext *sim) {
    int expected = sim->settings.flower_count + sim->settings.gardener_count;
    long long deadline = clock_millis() + 15000;
    while (!stop_requested && sim->ready_count < expected &&
           clock_millis() < deadline) {
        WorkerMessage event;
        if (!receive_event(sim, &event)) continue;
        if (event.type != MSG_REGISTER) continue;
        if (!register_worker(sim, &event))
            fprintf(stderr, "Duplicate or invalid worker: %d (%s)\n",
                    event.index + 1, event.gardener ? "gardener" : "flower");
    }
    if (sim->ready_count < expected && !stop_requested)
        fprintf(stderr, "Not all workers joined within 15 seconds.\n");
    return !stop_requested && sim->ready_count == expected;
}

/** @brief Checks the worker reply before applying any model changes. */
static int apply_result(SimulationContext *sim, const WorkerMessage *event,
                        int gardener, int index, int tick) {
    if (event->type != MSG_RESULT || event->gardener != gardener ||
        event->index != index || event->tick != tick ||
        event->pid != sim->worker_pids[worker_slot(gardener, index)]) return 0;
    if (gardener) {
        sim->gardeners[index] = event->worker;
        if (event->flower_index >= 0 &&
            event->flower_index < sim->settings.flower_count)
            sim->flowers[event->flower_index] = event->flower;
    } else {
        if (event->flower_index != index) return 0;
        sim->flowers[index] = event->flower;
        sim->requests_created += event->new_request;
    }
    return 1;
}

/** @brief Sends one task and receives its result from a worker. */
static int update_worker(SimulationContext *sim, int gardener,
                         int index, int tick) {
    int slot = worker_slot(gardener, index);
    if (!send_work(sim, slot, MSG_WORK, tick)) return 0;
    long long deadline = clock_millis() + 3000;
    while (!stop_requested && clock_millis() < deadline) {
        WorkerMessage event;
        if (!receive_event(sim, &event)) continue;
        if (event.type == MSG_REGISTER) continue;
        return apply_result(sim, &event, gardener, index, tick);
    }
    return 0;
}

/** @brief Stops registered workers and removes IPC objects. */
static void shutdown_workers(SimulationContext *sim) {
    for (int slot = 0; slot < MAX_FLOWERS + MAX_GARDENERS; ++slot)
        if (sim->worker_pids[slot] != 0)
            send_work(sim, slot, MSG_STOP, 0);
}

/** @brief Runs the independent-process message queue simulation. */
int run_simulation(const Settings *settings, const char *session) {
    SimulationContext sim;
    if (!create_simulation(&sim, session, settings)) {
        perror("Cannot create IPC session (name may be in use)");
        return 0;
    }
    int success = 1;
    int finished_tick = 0;
    log_event("Session %s: waiting for %d flowers and %d gardeners\n",
              session, settings->flower_count, settings->gardener_count);
    if (!wait_for_workers(&sim)) { success = 0; goto finish; }
    log_event("All workers connected. Simulation started.\n");
    for (int tick = 1; tick <= OBSERVATION_PERIOD && !stop_requested; ++tick) {
        sleep_one_tick();
        for (int i = 0; i < settings->flower_count && !stop_requested; ++i)
            if (!update_worker(&sim, 0, i, tick)) { success = 0; goto finish; }
        for (int i = 0; i < settings->gardener_count && !stop_requested; ++i)
            if (!update_worker(&sim, 1, i, tick)) { success = 0; goto finish; }
        finished_tick = tick;
        if (all_flowers_inactive(sim.flowers, settings->flower_count)) {
            log_event("\nAll flowers became inactive.\n");
            break;
        }
    }
    if (stop_requested) log_event("\nSimulation interrupted (Ctrl+C).\n");
    else if (finished_tick == OBSERVATION_PERIOD)
        log_event("\nObservation period (%d ticks) completed.\n", OBSERVATION_PERIOD);
finish:
    shutdown_workers(&sim);
    /* Give workers time to receive the stop message. */
    for (int i = 0; i < 8; ++i) sleep_one_tick();
    log_event("Requests created: %d; claim conflicts: %d\n",
              sim.requests_created, sim.claim_conflicts);
    print_final_statistics(sim.flowers, settings->flower_count, finished_tick);
    close_simulation(&sim, 1);
    return success;
}
