#include "config_parser.h"

#include "constants.h"
#include "soft_assert.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Parses a decimal integer and rejects invalid input. */
static int parse_int_value(const char *text, int *value) {
    char *end = NULL;
    long parsed;

    if (!SOFT_ASSERT(text != NULL, "integer text must not be NULL") ||
        !SOFT_ASSERT(value != NULL, "integer destination must not be NULL")) {
        return 0;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);

    if (text == end || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }

    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
        ++end;
    }

    return *end == '\0' ? (*value = (int)parsed, 1) : 0;
}

/** @brief Removes trailing newline characters from an input line. */
static void trim_newline(char *text) {
    size_t length = strlen(text);

    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        text[--length] = '\0';
    }
}

/** @brief Converts a text policy name to its enum value. */
static int parse_request_policy(const char *value, RequestPolicy *policy) {
    if (!SOFT_ASSERT(value != NULL, "request policy text must not be NULL") ||
        !SOFT_ASSERT(policy != NULL, "request policy destination must not be NULL")) {
        return 0;
    }

    if (strcmp(value, "first") == 0) {
        *policy = REQUEST_FIRST;
        return 1;
    }
    if (strcmp(value, "driest") == 0) {
        *policy = REQUEST_DRIEST;
        return 1;
    }

    return 0;
}

/** @brief Parses comma-separated individual drying rates. */
static int parse_drying_rates(char *value, Settings *settings, int *rate_count) {
    int count = 0;
    char *token;

    if (!SOFT_ASSERT(value != NULL, "drying rates text must not be NULL") ||
        !SOFT_ASSERT(settings != NULL, "settings must not be NULL") ||
        !SOFT_ASSERT(rate_count != NULL, "drying rate count must not be NULL")) {
        return 0;
    }

    token = strtok(value, ",");
    while (token != NULL) {
        if (count >= MAX_FLOWERS ||
            !parse_int_value(token, &settings->drying_rates[count])) {
            return 0;
        }
        ++count;
        token = strtok(NULL, ",");
    }

    *rate_count = count;
    return count > 0;
}

/** @brief Finds the index of a supported configuration key. */
static int setting_index(const char *key) {
    static const char *keys[CONFIG_FIELD_COUNT] = {
        "flower_count", "drying_rates", "request_threshold", "dry_threshold",
        "watering_duration", "recovery_duration", "protection_interval",
        "gardener_count", "gardener_speed", "request_policy"
    };

    for (int i = 0; i < CONFIG_FIELD_COUNT; ++i) {
        if (strcmp(key, keys[i]) == 0) return i;
    }

    return -1;
}

/** @brief Assigns a parsed key-value pair to the settings structure. */
static int set_setting(Settings *settings,
                       const char *key,
                       char *value,
                       int *drying_rate_count) {
    if (strcmp(key, "flower_count") == 0) {
        return parse_int_value(value, &settings->flower_count);
    }
    if (strcmp(key, "drying_rates") == 0) {
        return parse_drying_rates(value, settings, drying_rate_count);
    }
    if (strcmp(key, "request_threshold") == 0) {
        return parse_int_value(value, &settings->request_threshold);
    }
    if (strcmp(key, "dry_threshold") == 0) {
        return parse_int_value(value, &settings->dry_threshold);
    }
    if (strcmp(key, "watering_duration") == 0) {
        return parse_int_value(value, &settings->watering_duration);
    }
    if (strcmp(key, "recovery_duration") == 0) {
        return parse_int_value(value, &settings->recovery_duration);
    }
    if (strcmp(key, "protection_interval") == 0) {
        return parse_int_value(value, &settings->protection_interval);
    }
    if (strcmp(key, "gardener_count") == 0) {
        return parse_int_value(value, &settings->gardener_count);
    }
    if (strcmp(key, "gardener_speed") == 0) {
        return parse_int_value(value, &settings->gardener_speed);
    }
    if (strcmp(key, "request_policy") == 0) {
        return parse_request_policy(value, &settings->request_policy);
    }

    return 0;
}

/** @brief Reads and validates the configuration file structure. */
int parse_config_file(const char *filename, Settings *settings) {
    FILE *file;
    char line[CONFIG_LINE_SIZE];
    unsigned int seen = 0;
    int drying_rate_count = 0;

    if (!SOFT_ASSERT(filename != NULL, "configuration filename must not be NULL") ||
        !SOFT_ASSERT(settings != NULL, "settings must not be NULL")) {
        return 0;
    }

    memset(settings, 0, sizeof(*settings));
    file = fopen(filename, "r");
    if (file == NULL) {
        perror("Не удалось открыть файл параметров");
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char key[CONFIG_KEY_SIZE];
        char value[CONFIG_VALUE_SIZE];
        int index;

        trim_newline(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        if (sscanf(line, " %63[^=]=%191[^\n]", key, value) != 2) {
            fprintf(stderr, "Некорректная строка конфигурации: %s\n", line);
            fclose(file);
            return 0;
        }

        index = setting_index(key);
        if (index < 0) {
            fprintf(stderr, "Неизвестный параметр: %s\n", key);
            fclose(file);
            return 0;
        }

        if (seen & (1u << index)) {
            fprintf(stderr, "Параметр %s указан несколько раз.\n", key);
            fclose(file);
            return 0;
        }

        if (!set_setting(settings, key, value, &drying_rate_count)) {
            fprintf(stderr, "Некорректное значение параметра: %s=%s\n", key, value);
            fclose(file);
            return 0;
        }

        seen |= 1u << index;
    }

    if (ferror(file)) {
        perror("Ошибка чтения файла параметров");
        fclose(file);
        return 0;
    }

    fclose(file);

    if (seen != ((1u << CONFIG_FIELD_COUNT) - 1u)) {
        fprintf(stderr,
                "В конфигурации должны быть заданы все %d параметров ровно по одному разу.\n",
                CONFIG_FIELD_COUNT);
        return 0;
    }

    if (drying_rate_count != settings->flower_count) {
        fprintf(stderr,
                "Количество скоростей высыхания (%d) должно совпадать с количеством цветов (%d).\n",
                drying_rate_count,
                settings->flower_count);
        return 0;
    }

    return 1;
}
