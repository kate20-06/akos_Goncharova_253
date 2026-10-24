#ifndef CONSTANTS_H
#define CONSTANTS_H

/** @brief Integer constants used by the model and input subsystem. */
enum {
    MAX_FLOWERS = 100,
    MAX_GARDENERS = 10,

    MIN_MOISTURE = 0,
    MAX_MOISTURE = 100,
    INITIAL_MOISTURE = MAX_MOISTURE,

    OBSERVATION_PERIOD = 50,
    TICK_DURATION_MS = 25,
    GARDENER_APPROACH_WORK = 6,

    NO_GARDENER_ID = -1,
    NO_FLOWER_INDEX = -1,
    FIRST_ENTITY_ID = 1,

    CONFIG_FIELD_COUNT = 10,

    INPUT_LINE_SIZE = 128,
    CONFIG_LINE_SIZE = 256,
    CONFIG_KEY_SIZE = 64,
    CONFIG_VALUE_SIZE = 192,
    LOG_BUFFER_SIZE = 1024
};

extern const char DEFAULT_LOG_FILE[];

#endif
