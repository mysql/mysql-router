#ifndef PLUGIN_INCLUDED
#define PLUGIN_INCLUDED

#include <stdlib.h>

/* Forward declarations */
struct Config;
typedef struct Config Config;


/**
 * Structure with information about the harness.
 *
 * This structure is made available to plugins so that they can get
 * information about the plugin harness.
 *
 * @note We are intentionally using C calls here to avoid issues with
 * symbol conversions and calling conventions. The file can be
 * included both as a C and C++ file.
 *
 */

typedef struct AppInfo {
  /**
   * Program name.
   *
   * Name of the application.
   */

  const char *program;

  /**
   * Directory name for plugins.
   *
   * Name of the directory where extensions can be found and it
   * depends on how the harness was installed. In a typical
   * installation with installation prefix `/` it will be
   * `/var/lib/mysql/<name>`.
   */

  const char *libdir;


  /**
   * Directory name for log files.
   *
   * Name of the directory where log files should be placed. In a
   * typical installation with installation prefix `/` this will be
   * `/var/log/<name>`.
   */

  const char *logdir;

  /**
   * Directory name for run files.
   *
   * Name of the directory where run files should be placed. In a
   * typical installation with installation prefix `/` this will be
   * `/var/run/<name>`.
   */

  const char *rundir;


  /**
   * Directory name for configuration files.
   *
   * Name of the directory where run files should be placed. In a
   * typical installation with installation prefix `/` this will be
   * `/etc/<name>`.
   */

  const char *etcdir;


  /**
   * Configuration information.
   */

  const Config* config;

} AppInfo;


/**
 * Structure containing information about the plugin.
 *
 * The name of the plugin is give by the filename.
 */

struct Plugin {
  /**
   * Version of the plugin interface the plugin was built for.
   *
   * This field contain the ABI version the plugin was built for and
   * is checked when loading the plugin to determine if the structure
   * can be safely read. It shall normally be set to
   * `PLUGIN_ABI_VERSION`, which is the version of the ABI that is
   * being used.
   *
   * The least significant byte contain the minor version, the second
   * least significant byte contain the major version.
   */

  int abi_version;


  /**
   * Brief description of plugin, to show in listings.
   */

  const char *brief;


  /**
   * Plugin version.
   *
   * Version of the plugin, given as a version number.
   *
   * @see VERSION_NUMBER
   */
  unsigned long plugin_version;

  /**
   * Array of names of required plugins.
   *
   * Length is given as the number of elements in the array and the
   * array contain the names of the required plugins as C strings.
   *
   * A typical use is:
   * @code
   * const char *requires[] = {
   *   "first",
   *   "second",
   * };
   *
   * Plugin my_plugin = {
   *   ...
   *   sizeof(requires)/sizeof(*requires),
   *   requires,
   *   ...
   * };
   * @endcode
   */

  size_t requires_length;
  const char **requires;


  /**
   * Array of names of plugins it conflicts with.
   *
   * The array is defined in a similar way to how the @c requires
   * array is defined.
   */

  size_t conflicts_length;
  const char **conflicts;


  /**
   * Module initialization function.
   *
   * This function is called after the module is loaded. The pointer
   * can be NULL, in which case no initialization takes place.
   *
   * @pre All modules that is in the list of required modules have
   * their @c init() function called before this modules init
   * function.
   *
   * @param info Pointer to information about harness this module was
   * loaded into.
   */

  int (*init)(AppInfo* info);


  /**
   * Module deinitialization function.
   *
   * This function is called after module threads have exited but
   * before the module is unloaded.
   *
   * @pre All @c deinit() functions in modules will required by this
   * module are called after the @c deinit() function of this module
   * have exited.
   *
   * @param info Pointer to information about the harness this module
   * was loaded into.
   */
  int (*deinit)(AppInfo* info);

  /**
   * Module thread start function.
   *
   * If this field is non-NULL, the plugin will be assigned a new
   * thread and the start function will be called. The start functions
   * of different plugins are called in an arbitrary order, so no
   * expectations on the start order should be made.
   *
   * @param info Pointer to information about the harness this module
   * was loaded into.
   */

  void* (*start)(AppInfo* info);
};


/**
 * Current version of the library.
 *
 * This constant is the version of the plugin interface in use. This
 * should be used when initializing the module structure.
 *
 * @see Plugin
 */

const unsigned int PLUGIN_ABI_VERSION = 0x0100;

#define VERSION_NUMBER(MAJ, MIN, PAT) \
  ((((MAJ) & 0xFF) << 24) | (((MIN) & 0xFF) << 16) | ((PAT) & 0xFFFF))

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the value of a configuration option.
 */

const char *config_get(const Config *config, const char *section, const char *option);

#ifdef __cplusplus
}
#endif

#endif /* PLUGIN_INCLUDED */
