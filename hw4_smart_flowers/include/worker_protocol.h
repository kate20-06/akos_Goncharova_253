#ifndef WORKER_PROTOCOL_H
#define WORKER_PROTOCOL_H

#include "model.h"
#include <stddef.h>

#define WORKER_LINE_SIZE 16384

/** @brief Encodes a flower task for a connected worker. */
int make_flower_job(char *out, size_t size, int tick, const Flower *flower,
                    const Settings *settings);
/** @brief Encodes a gardener task with the current garden snapshot. */
int make_gardener_job(char *out, size_t size, int tick, const Gardener *gardener,
                      const Flower flowers[], const Settings *settings);
/** @brief Executes a flower task using the existing flower model. */
int execute_flower_job(const char *job, char *reply, size_t size);
/** @brief Executes a gardener task using the existing gardener model. */
int execute_gardener_job(const char *job, char *reply, size_t size);
/** @brief Reads one flower result and checks its identity. */
int parse_flower_result(const char *line, int tick, int id, Flower *flower);
/** @brief Reads a gardener result and its optional flower change. */
int parse_gardener_result(const char *line, int tick, int id, Gardener *gardener,
                          int *changed_index, Flower *changed_flower);
#endif
