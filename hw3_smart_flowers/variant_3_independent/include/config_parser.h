#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "model.h"

/**
 * @brief Reads simulation settings from a key=value configuration file.
 * @param filename Path to the configuration file.
 * @param settings Structure where parsed settings are stored.
 * @return 1 on success, 0 on error.
 */
int parse_config_file(const char *filename, Settings *settings);

#endif
