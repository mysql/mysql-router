#ifndef CONFIG_INCLUDED
#define CONFIG_INCLUDED

/**
 * @file
 * Configuration file handling.
 *
 * Configuration files are written using the standard INI file
 * convention with the addition of variable interpolation. The
 * variable interpolation is done when the configuration file
 * structure is read from a file and is therefore not visible.
 */

struct Config;
typedef struct Config Config;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the value of a configuration option.
 */

const char *config_get(Config *config, const char *section, const char *option);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_INCLUDED */
