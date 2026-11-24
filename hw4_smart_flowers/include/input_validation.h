#ifndef INPUT_VALIDATION_H
#define INPUT_VALIDATION_H

#include "model.h"

/**
 * @brief Validates simulation parameters.
 * @param settings Parameters to validate.
 * @return 1 if all parameters are valid, otherwise 0.
 */
int validate_settings(const Settings *settings);

#endif
