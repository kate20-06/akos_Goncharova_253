#include "input_validation.h"

#include "constants.h"
#include "soft_assert.h"

#include <stdio.h>

/** @brief Checks range constraints and consistency of input values. */
int validate_settings(const Settings *settings) {
    if (!SOFT_ASSERT(settings != NULL, "settings must not be NULL")) return 0;

    if (settings->flower_count <= 0 || settings->flower_count > MAX_FLOWERS) {
        fprintf(stderr, "Количество цветов должно быть от 1 до %d.\n", MAX_FLOWERS);
        return 0;
    }
    if (settings->dry_threshold < MIN_MOISTURE || settings->dry_threshold > MAX_MOISTURE) {
        fprintf(stderr, "Порог засыхания должен быть от %d до %d.\n", MIN_MOISTURE, MAX_MOISTURE);
        return 0;
    }
    if (settings->request_threshold <= settings->dry_threshold ||
        settings->request_threshold > MAX_MOISTURE) {
        fprintf(stderr, "Порог заявки (%d) должен быть строго больше порога засыхания (%d) и не больше %d.\n"
                "Например, можно указать порог заявки 30 и порог засыхания 10.\n",
                settings->request_threshold, settings->dry_threshold, MAX_MOISTURE);
        return 0;
    }
    if (settings->watering_duration <= 0) {
        fprintf(stderr, "Длительность полива должна быть не меньше 1 такта.\n");
        return 0;
    }
    if (settings->recovery_duration < 0) {
        fprintf(stderr, "Длительность восстановления не может быть отрицательной.\n");
        return 0;
    }
    if (settings->protection_interval < 0) {
        fprintf(stderr, "Защитный интервал не может быть отрицательным.\n");
        return 0;
    }
    if (settings->gardener_count <= 0 || settings->gardener_count > MAX_GARDENERS) {
        fprintf(stderr, "Количество садовников должно быть от 1 до %d.\n", MAX_GARDENERS);
        return 0;
    }
    if (settings->gardener_speed <= 0) {
        fprintf(stderr, "Скорость работы садовника должна быть положительной.\n");
        return 0;
    }
    if (settings->request_policy != REQUEST_FIRST && settings->request_policy != REQUEST_DRIEST) {
        fprintf(stderr, "Неизвестное правило выбора заявок.\n");
        return 0;
    }

    for (int i = 0; i < settings->flower_count; ++i) {
        if (settings->drying_rates[i] <= 0) {
            fprintf(stderr, "Скорость высыхания цветка %d должна быть положительной.\n",
                    i + FIRST_ENTITY_ID);
            return 0;
        }
    }

    return 1;
}
