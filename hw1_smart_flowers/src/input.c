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

static int read_int(const char *prompt, int *value) {
    char line[INPUT_LINE_SIZE];

    if (!SOFT_ASSERT(prompt != NULL, "input prompt must not be NULL") ||
        !SOFT_ASSERT(value != NULL, "input destination must not be NULL")) {
        return 0;
    }

    printf("%s", prompt);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        fprintf(stderr, "Не удалось прочитать значение.\n");
        return 0;
    }

    if (!parse_int_value(line, value)) {
        fprintf(stderr, "Ожидалось целое число.\n");
        return 0;
    }

    return 1;
}

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

int read_settings(Settings *settings) {
    char policy_text[INPUT_LINE_SIZE];

    if (!SOFT_ASSERT(settings != NULL, "settings must not be NULL")) return 0;
    memset(settings, 0, sizeof(*settings));

    if (!read_int("Количество цветов: ", &settings->flower_count)) return 0;
    if (settings->flower_count <= 0 || settings->flower_count > MAX_FLOWERS) {
        fprintf(stderr, "Количество цветов должно быть от 1 до %d.\n", MAX_FLOWERS);
        return 0;
    }

    printf("Введите скорости высыхания для %d цветов:\n", settings->flower_count);
    for (int i = 0; i < settings->flower_count; ++i) {
        printf("  Цветок %d: ", i + FIRST_ENTITY_ID);
        if (!read_int("", &settings->drying_rates[i])) return 0;
    }

    if (!read_int("Порог формирования заявки: ", &settings->request_threshold)) return 0;
    if (!read_int("Порог засыхания: ", &settings->dry_threshold)) return 0;
    if (!read_int("Длительность полива: ", &settings->watering_duration)) return 0;
    if (!read_int("Длительность восстановления: ", &settings->recovery_duration)) return 0;
    if (!read_int("Защитный интервал: ", &settings->protection_interval)) return 0;
    if (!read_int("Количество садовников: ", &settings->gardener_count)) return 0;
    if (!read_int("Скорость работы садовника: ", &settings->gardener_speed)) return 0;

    if (!read_text("Правило выбора заявок (first/driest): ",
                   policy_text, sizeof(policy_text))) {
        return 0;
    }
    if (!parse_request_policy(policy_text, &settings->request_policy)) {
        fprintf(stderr, "Правило выбора заявок должно быть first или driest.\n");
        return 0;
    }

    return 1;
}

int read_settings_from_file(const char *filename, Settings *settings) {
    return parse_config_file(filename, settings);
}

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
