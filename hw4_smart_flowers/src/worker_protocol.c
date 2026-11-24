#include "worker_protocol.h"
#include "flower_logic.h"
#include "gardener_logic.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Appends text while checking the line length. */
static int append(char *out, size_t cap, size_t *length, const char *text) {
    size_t n = strlen(text);
    if (n >= cap - *length) return 0;
    memcpy(out + *length, text, n + 1);
    *length += n;
    return 1;
}

/** @brief Serializes one flower without copying native struct padding. */
static int write_flower(char *out, size_t cap, size_t *length, const Flower *f) {
    char part[200];
    snprintf(part, sizeof(part), " %d %d %d %d %d %d %d %d %d %d",
             f->id, f->moisture, f->drying_rate, (int)f->state,
             f->request_active, f->assigned_gardener, f->watering_left,
             f->recovery_left, f->protection_left, f->watering_count);
    return append(out, cap, length, part);
}

/** @brief Serializes the gardener's local progress. */
static int write_gardener(char *out, size_t cap, size_t *length, const Gardener *g) {
    char part[120];
    snprintf(part, sizeof(part), " %d %d %d %d %d", g->id, g->busy,
             g->flower_index, g->work_left, g->speed);
    return append(out, cap, length, part);
}

/** @brief Parses a bounded signed integer from a protocol line. */
static int read_int(const char **cursor, int *value) {
    const char *p = *cursor;
    while (*p == ' ') ++p;
    if (!*p) return 0;
    errno = 0;
    char *end;
    long v = strtol(p, &end, 10);
    if (errno || end == p || v < INT_MIN || v > INT_MAX ||
        (*end != ' ' && *end != '\0')) return 0;
    *value = (int)v;
    *cursor = end;
    return 1;
}

static int read_flower(const char **cursor, Flower *f) {
    int state;
    if (!read_int(cursor, &f->id) || !read_int(cursor, &f->moisture) ||
        !read_int(cursor, &f->drying_rate) || !read_int(cursor, &state) ||
        !read_int(cursor, &f->request_active) ||
        !read_int(cursor, &f->assigned_gardener) ||
        !read_int(cursor, &f->watering_left) ||
        !read_int(cursor, &f->recovery_left) ||
        !read_int(cursor, &f->protection_left) ||
        !read_int(cursor, &f->watering_count)) return 0;
    if (state < FLOWER_NORMAL || state > FLOWER_DEAD_OVERWATERED) return 0;
    f->state = (FlowerState)state;
    return 1;
}

static int read_gardener(const char **cursor, Gardener *g) {
    return read_int(cursor, &g->id) && read_int(cursor, &g->busy) &&
           read_int(cursor, &g->flower_index) && read_int(cursor, &g->work_left) &&
           read_int(cursor, &g->speed);
}

static int at_end(const char *cursor) {
    while (*cursor == ' ') ++cursor;
    return !*cursor;
}

int make_flower_job(char *out, size_t size, int tick, const Flower *f,
                    const Settings *settings) {
    int n = snprintf(out, size, "FJOB %d %d %d", tick,
                     settings->request_threshold, settings->dry_threshold);
    if (n < 0 || (size_t)n >= size) return 0;
    size_t length = (size_t)n;
    return write_flower(out, size, &length, f) && append(out, size, &length, "\n");
}

int make_gardener_job(char *out, size_t size, int tick, const Gardener *g,
                      const Flower flowers[], const Settings *settings) {
    int n = snprintf(out, size, "GJOB %d %d %d %d %d %d", tick,
                     settings->flower_count, (int)settings->request_policy,
                     settings->watering_duration, settings->recovery_duration,
                     settings->protection_interval);
    if (n < 0 || (size_t)n >= size) return 0;
    size_t length = (size_t)n;
    if (!write_gardener(out, size, &length, g)) return 0;
    for (int i = 0; i < settings->flower_count; ++i) {
        if (!write_flower(out, size, &length, &flowers[i])) return 0;
    }
    return append(out, size, &length, "\n");
}

int execute_flower_job(const char *job, char *reply, size_t size) {
    if (strncmp(job, "FJOB ", 5)) return 0;
    const char *p = job + 5;
    int tick;
    Settings settings = {0};
    Flower f = {0};
    if (!read_int(&p, &tick) || !read_int(&p, &settings.request_threshold) ||
        !read_int(&p, &settings.dry_threshold) || !read_flower(&p, &f) ||
        !at_end(p)) return 0;
    update_flower(&f, &settings, tick);
    int n = snprintf(reply, size, "FRESULT %d", tick);
    if (n < 0 || (size_t)n >= size) return 0;
    size_t length = (size_t)n;
    return write_flower(reply, size, &length, &f) &&
           append(reply, size, &length, "\n");
}

int execute_gardener_job(const char *job, char *reply, size_t size) {
    if (strncmp(job, "GJOB ", 5)) return 0;
    const char *p = job + 5;
    int tick, policy;
    Settings settings = {0};
    Gardener g = {0};
    Flower flowers[MAX_FLOWERS] = {{0}};
    if (!read_int(&p, &tick) || !read_int(&p, &settings.flower_count) ||
        !read_int(&p, &policy) || !read_int(&p, &settings.watering_duration) ||
        !read_int(&p, &settings.recovery_duration) ||
        !read_int(&p, &settings.protection_interval) ||
        !read_gardener(&p, &g)) return 0;
    if (settings.flower_count < 1 || settings.flower_count > MAX_FLOWERS ||
        (policy != REQUEST_FIRST && policy != REQUEST_DRIEST)) return 0;
    settings.request_policy = (RequestPolicy)policy;
    for (int i = 0; i < settings.flower_count; ++i) {
        if (!read_flower(&p, &flowers[i])) return 0;
    }
    if (!at_end(p)) return 0;
    Flower before[MAX_FLOWERS];
    memcpy(before, flowers, (size_t)settings.flower_count * sizeof(Flower));
    update_gardener(&g, flowers, &settings, tick);
    int changed = -1;
    for (int i = 0; i < settings.flower_count; ++i) {
        if (memcmp(&before[i], &flowers[i], sizeof(Flower)) != 0) {
            if (changed != -1) return 0;
            changed = i;
        }
    }
    int n = snprintf(reply, size, "GRESULT %d", tick);
    if (n < 0 || (size_t)n >= size) return 0;
    size_t length = (size_t)n;
    if (!write_gardener(reply, size, &length, &g)) return 0;
    char suffix[40];
    snprintf(suffix, sizeof(suffix), " %d", changed);
    if (!append(reply, size, &length, suffix)) return 0;
    if (changed >= 0 && !write_flower(reply, size, &length, &flowers[changed])) return 0;
    return append(reply, size, &length, "\n");
}

int parse_flower_result(const char *line, int tick, int id, Flower *flower) {
    if (strncmp(line, "FRESULT ", 8)) return 0;
    const char *p = line + 8;
    int got_tick;
    if (!read_int(&p, &got_tick) || got_tick != tick ||
        !read_flower(&p, flower) || !at_end(p)) return 0;
    return flower->id == id && flower->moisture >= 0 && flower->moisture <= 100;
}

int parse_gardener_result(const char *line, int tick, int id, Gardener *g,
                          int *changed_index, Flower *changed_flower) {
    if (strncmp(line, "GRESULT ", 8)) return 0;
    const char *p = line + 8;
    int got_tick;
    if (!read_int(&p, &got_tick) || got_tick != tick ||
        !read_gardener(&p, g) || !read_int(&p, changed_index)) return 0;
    if (*changed_index >= 0 && !read_flower(&p, changed_flower)) return 0;
    return got_tick == tick && g->id == id && at_end(p) &&
           *changed_index >= -1 && *changed_index < MAX_FLOWERS;
}
