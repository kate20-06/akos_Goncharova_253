#ifndef MODEL_H
#define MODEL_H

#include "constants.h"

/** @brief Possible flower states. */
typedef enum {
    FLOWER_NORMAL,
    FLOWER_WILTING,
    FLOWER_WATERING,
    FLOWER_RECOVERING,
    FLOWER_DRY,
    FLOWER_DEAD_OVERWATERED
} FlowerState;

/** @brief Supported rules for selecting watering requests. */
typedef enum {
    REQUEST_FIRST,
    REQUEST_DRIEST
} RequestPolicy;

/** @brief Represents one smart flower in the model. */
typedef struct {
    int id;
    int moisture;
    int drying_rate;
    FlowerState state;

    int request_active;
    int assigned_gardener;

    int watering_left;
    int recovery_left;
    int protection_left;

    int watering_count;
} Flower;

/** @brief Represents a gardener who handles flower requests. */
typedef struct {
    int id;
    int busy;
    int flower_index;
    int work_left;
    int speed;
} Gardener;

/**
 * @brief Input parameters of a simulation run.
 *
 * The fields correspond directly to the input parameters listed in variant 14.
 */
typedef struct {
    int flower_count;
    int drying_rates[MAX_FLOWERS];

    int request_threshold;
    int dry_threshold;

    int watering_duration;
    int recovery_duration;
    int protection_interval;

    int gardener_count;
    int gardener_speed;

    RequestPolicy request_policy;
} Settings;

const char *flower_state_name(FlowerState state);
const char *request_policy_name(RequestPolicy policy);

void init_flower(Flower *flower, int id, int moisture, int drying_rate);
void init_gardener(Gardener *gardener, int id, int speed);
int flower_is_active(const Flower *flower);

#endif
