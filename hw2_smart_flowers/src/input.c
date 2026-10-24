#include "input.h"

#include "config_parser.h"
#include "constants.h"
#include "input_validation.h"
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

    if (*end != '\0') {
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

/** @brief Reads one bounded line of text from standard input. */
static int read_text(const char *prompt, char *value, size_t value_size) {
    size_t length;

    if (!SOFT_ASSERT(prompt != NULL, "input prompt must not be NULL") ||
        !SOFT_ASSERT(value != NULL, "text destination must not be NULL") ||
        !SOFT_ASSERT(value_size > 1, "text destination must have space")) {
        return 0;
    }

    printf("%s", prompt);
    if (fgets(value, value_size, stdin) == NULL) {
        fprintf(stderr, "Не удалось прочитать значение.\n");
        return 0;
    }

    length = strlen(value);
    while (length > 0 && (value[length - 1] == '\n' || value[length - 1] == '\r')) {
        value[--length] = '\0';
    }

    return length > 0;
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

/** @brief Repeats a prompt until an integer in the given range is entered. */
static int read_int_in_range(const char *prompt, int *value, int minimum, int maximum) {
    char line[INPUT_LINE_SIZE];

    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) {
            fprintf(stderr, "Не удалось прочитать значение.\n");
            return 0;
        }
        if (!parse_int_value(line, value)) {
            printf("Введите целое число от %d до %d.\n", minimum, maximum);
        } else if (*value < minimum || *value > maximum) {
            printf("Недопустимое значение: ожидается число от %d до %d. Попробуйте ещё раз.\n",
                   minimum, maximum);
        } else {
            return 1;
        }
    }
}

/** @brief Prompts for settings and explains valid values before reading them. */
int read_settings(Settings *settings) {
    char policy_text[INPUT_LINE_SIZE];
    char prompt[INPUT_LINE_SIZE];

    if (!SOFT_ASSERT(settings != NULL, "settings must not be NULL")) return 0;
    memset(settings, 0, sizeof(*settings));

    printf("Влажность каждого цветка начинается со 100 и уменьшается со временем.\n");
    printf("Когда влажность падает до порога заявки, цветок просит полив.\n");
    printf("Если она падает до порога засыхания, цветок погибает.\n");
    printf("Поэтому порог заявки должен быть СТРОГО БОЛЬШЕ порога засыхания.\n");
    printf("Например: порог заявки 30, порог засыхания 10.\n");
    printf("При ошибке ввода можно повторить попытку.\n\n");

    snprintf(prompt, sizeof(prompt), "Количество цветов (1–%d): ", MAX_FLOWERS);
    if (!read_int_in_range(prompt, &settings->flower_count, 1, MAX_FLOWERS)) return 0;

    printf("Скорость высыхания — сколько единиц влажности цветок теряет за такт (целое число >= 1).\n");
    for (int i = 0; i < settings->flower_count; ++i) {
        snprintf(prompt, sizeof(prompt), "  Цветок %d, скорость высыхания (>= 1): ", i + FIRST_ENTITY_ID);
        if (!read_int_in_range(prompt, &settings->drying_rates[i], 1, INT_MAX)) return 0;
    }

    if (!read_int_in_range("Порог заявки (1–100, выше порога засыхания): ",
                           &settings->request_threshold, 1, MAX_MOISTURE)) return 0;

    snprintf(prompt, sizeof(prompt),
             "Порог засыхания (0–%d, строго ниже порога заявки %d): ",
             settings->request_threshold - 1, settings->request_threshold);
    if (!read_int_in_range(prompt, &settings->dry_threshold,
                           MIN_MOISTURE, settings->request_threshold - 1)) return 0;

    if (!read_int_in_range("Длительность полива (в тактах, >= 1): ",
                           &settings->watering_duration, 1, INT_MAX)) return 0;
    if (!read_int_in_range("Длительность восстановления (в тактах, >= 0; 0 — без ожидания): ",
                           &settings->recovery_duration, 0, INT_MAX)) return 0;
    if (!read_int_in_range("Защитный интервал (в тактах, >= 0; повторный полив в этот период опасен): ",
                           &settings->protection_interval, 0, INT_MAX)) return 0;

    snprintf(prompt, sizeof(prompt), "Количество садовников (1–%d): ", MAX_GARDENERS);
    if (!read_int_in_range(prompt, &settings->gardener_count, 1, MAX_GARDENERS)) return 0;
    if (!read_int_in_range("Скорость садовника (единиц пути за такт, >= 1): ",
                           &settings->gardener_speed, 1, INT_MAX)) return 0;

    for (;;) {
        if (!read_text("Правило выбора (first — первая заявка; driest — самый сухой цветок): ",
                       policy_text, sizeof(policy_text))) return 0;
        if (parse_request_policy(policy_text, &settings->request_policy)) break;
        printf("Введите first или driest.\n");
    }

    return 1;
}

/** @brief Loads settings from the specified configuration file. */
int read_settings_from_file(const char *filename, Settings *settings) {
    return parse_config_file(filename, settings);
}

/** @brief Displays the settings selected for this run. */
void print_settings(const Settings *settings) {
    if (!SOFT_ASSERT(settings != NULL, "settings must not be NULL")) return;

    printf("\nПараметры запуска:\n");
    printf("  Цветов: %d\n", settings->flower_count);
    printf("  Скорости высыхания:");
    for (int i = 0; i < settings->flower_count; ++i) {
        printf(" %d", settings->drying_rates[i]);
    }
    printf("\n");
    printf("  Порог заявки: %d\n", settings->request_threshold);
    printf("  Порог засыхания: %d\n", settings->dry_threshold);
    printf("  Длительность полива: %d\n", settings->watering_duration);
    printf("  Длительность восстановления: %d\n", settings->recovery_duration);
    printf("  Защитный интервал: %d\n", settings->protection_interval);
    printf("  Садовников: %d\n", settings->gardener_count);
    printf("  Скорость работы садовника: %d\n", settings->gardener_speed);
    printf("  Правило выбора заявок: %s\n\n", request_policy_name(settings->request_policy));
}
