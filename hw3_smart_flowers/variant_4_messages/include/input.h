#ifndef INPUT_H
#define INPUT_H

#include "model.h"

/**
 * @brief Reads simulation parameters interactively from standard input.
 * @param settings Structure where the parameters are stored.
 * @return 1 on successful input, 0 on error.
 */
int read_settings(Settings *settings);

/**
 * @brief Reads simulation parameters from a configuration file.
 *
 * The file uses simple key=value pairs. Drying rates are written as a
 * comma-separated list in the drying_rates field.
 *
 * @param filename Path to the configuration file.
 * @param settings Structure where the parameters are stored.
 * @return 1 on successful input, 0 on error.
 */
int read_settings_from_file(const char *filename, Settings *settings);

/**
 * @brief Validates the entered parameters.
 * @param settings Parameters to validate.
 * @return 1 if the parameters are valid, otherwise 0.
 */
int validate_settings(const Settings *settings);

/**
 * @brief Prints the parameters of the current simulation run.
 * @param settings Simulation parameters.
 */
void print_settings(const Settings *settings);

#endif
