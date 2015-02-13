#ifndef EXTENSION_INCLUDED
#define EXTENSION_INCLUDED

/**
 * Structure with information about the harness.
 *
 * This structure is made available to extensions so that they can get
 * information about the plugin harness.
 *
 * @note We are intentionally using C calls here to avoid issues with
 * symbol conversions and calling conventions.
 *
 */

struct Harness {
  const char *ext_dir;                  /* Directory for extensions */
  const char *log_dir;                  /* Directory for log files */
};

/**
 * Structure containing information about the extension.
 * 
 * The name of the extension is give by the basename of the filename.
 *
 * @todo Use dotted names for extension names and map to directory
 * structure.
 */

struct Extension {
  /**
   * Version of the interface.
   *
   * The least significant byte contain the minor version, the second
   * least significant byte contain the major version.
   */
  int version;

  /**
   * Brief description of extension, to show in listings.
   */
  const char *brief;

  /**
   * Array of names of required extensions.
  */
  int requires_length;
  const char **requires;

  /**
   * Array of names of extensions it conflicts with.
   */
  int conflicts_length;
  const char **conflicts;

  int (*init)(Harness*);
  int (*deinit)(Harness*);
  void* (*start)(Harness*);
};

const unsigned int EXTENSION_VERSION = 0x0100;

#endif /* EXTENSION_INCLUDED */
