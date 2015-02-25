#ifndef EXTENSION_INCLUDED
#define EXTENSION_INCLUDED

/**
 * Structure with information about the harness.
 *
 * This structure is made available to extensions so that they can get
 * information about the plugin harness.
 *
 * @note We are intentionally using C calls here to avoid issues with
 * symbol conversions and calling conventions. The file can be
 * included both as a C and C++ file.
 *
 */

typedef struct Harness {
  /**
   * Directory name for extensions.
   *
   * Name of the directory where extensions can be found and it
   * depends on how the harness was installed. In a typical
   * installation with installation prefix `/` it will be
   * `/var/lib/mysql/<name>`.
   */

  const char *extdir;


  /**
   * Directory name for log files.
   *
   * Name of the directory where log files should be placed. In a
   * typical installation with installation prefix `/` this will be
   * `/var/log/<name>`.
   */

  const char *logdir;
} Harness;


/**
 * Structure containing information about the extension.
 *
 * The name of the extension is give by the filename.
 */

struct Extension {
  /**
   * Version of the interface.
   *
   * The least significant byte contain the minor version, the second
   * least significant byte contain the major version.
   *
   * When definiting an extension, this should typically be set to @c
   * EXTENSION_VERSION since that is the current version of the
   * extension interface.
   */

  int version;


  /**
   * Brief description of extension, to show in listings.
   */

  const char *brief;


  /**
   * Array of names of required extensions.
   *
   * Length is given as the number of elements in the array and the
   * array contain the names of the required extensions as C strings.
   *
   * A typical use is:
   * @code
   * const char *requires[] = {
   *   "first",
   *   "second",
   * };
   *
   * Extension ext_info = {
   *   ...
   *   sizeof(requires)/sizeof(*requires),
   *   requires,
   *   ...
   * };
   * @endcode
   */

  int requires_length;
  const char **requires;


  /**
   * Array of names of extensions it conflicts with.
   *
   * The array is defined in a similar way to how the @c requires
   * array is defined.
   */

  int conflicts_length;
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
   * @param harness Pointer to harness this module was loaded into.
   */

  int (*init)(Harness* harness);


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
   * @param harness Pointer to harness this module was loaded into.
   */
  int (*deinit)(Harness* harness);

  /**
   * Module thread start function.
   *
   * If this field is non-NULL, the extension will be assigned a new
   * thread and the start function will be called. The start functions
   * of different extensions are called in an arbitrary order, so no
   * expectations on the start order should be made.
   *
   * @param harness Pointer to harness this module was loaded into.
   */

  void* (*start)(Harness* harness);
};


/**
 * Current version of the library.
 *
 * This constant is the version of the plugin interface in use. This
 * should be used when initializing the module structure.
 *
 * @see Extension
 */

const unsigned int EXTENSION_VERSION = 0x0100;

#endif /* EXTENSION_INCLUDED */
