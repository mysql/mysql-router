MySQL Harness
=============

MySQL Harness is an extensible framework that handles loading and
unloading of *extensions* (also known as *plugins*). The built-in
features is dependency tracking between extensions, configuration file
handling, and support for extension life-cycles.

Building
--------

To build the MySQL Harness you use the standard steps to build from
CMake:

    cmake .
    make

If you want to do an out-of-source build, the procedure is:

    mkdir build
    cd build
    cmake <path-to-source>
    make


Installing
----------

To install the files, use `make install`. This will install the
harness, the harness library, the header files for writing extensions,
and the available extensions that were not marked with `NO_INSTALL` (see
below).


Running
-------

To start the harness, just run it using `harness` and use the `-r`
option to give the root directory:

    harness -r /

Note that the harness uses the base name of the file (whatever is in
argv[0]) together with the prefix to create directories for logging
files and extensions. This means that if you want to create a harness
that automatically load all the extensions in the `/var/lib/router`
directory and start them, you need to create a file `router`
containing the line above and install it under `/usr/bin` or `/bin`.

The harness will then load extensions from the directory
`/var/lib/router` and write log files to `/var/log/router`.


Writing Extensions
---------------

All available extensions are in the `plugins/` directory. There is one
directory for each extension and it is assumed that it contain a
`CMakeLists.txt` file.

The main `CMakeLists.txt` file provide an `add_plugin` macro that can
be used add new extensions.

    add_plugin(<name> [ NO_INSTALL ] <source> ...)

This macro adds a extension named `<name>`. If `NO_INSTALL` is provided,
it will not be installed with the harness (useful if you have extensions
used for testing, see the `tests/` directory). Otherwise, the extension
will be installed in the *root*`/var/lib/`*harness-name* directory.


### Harness Structure ###

The harness structure contain some basic fields providing information
to the extension. Currently only two fields are provided:

    struct Harness {
      const char *ext_dir;                  /* Location of extensions */
      const char *log_dir;                  /* Log file directory */
    };


### Extension Structure ###

To define a new extension, you have to create an instance of the
`Extension` structure in your extension similar to this:

    #include "extension.h"
    
    static const char* requires[] = {
      "magic.so"
    };

    Extension ext_info = {
      EXTENSION_VERSION,

      // Brief description of extension
      "An example plugin",

      // Array of required extensions
      sizeof(requires)/sizeof(*requires), requires,

      // Array of extensions that conflict with this one
      0, NULL,

      init,
      deinit,
      start,
    };


### Initialization and Cleanup ###

After the extension is loaded, the `init()` function is called for all
extensions with a pointer to the harness (as defined above) as the
only argument.

The `init()` functions are called in dependency order so that all
`init()` functions in required extensions are called before the `init()`
function in the extension itself.

Before the harness exits, it will call the `deinit()` function with a
pointer to the harness as the only argument.


### Starting the Extension ###

After all the extensions have been successfully initialized, a thread
will be created for each extensions that have a `start()` function
defined.

The start function will be called with a pointer to the harness as the
only parameter. When all the extensions return from their `start()`
functions, the harness will perform cleanup and exit.


### Logging ###

Logging is handled by re-directing standard output to
*root*`/var/log/general.log` and standard error to
*root*`/var/log/error.log`. This is deployed as a extension itself and
is automatically loaded when starting the harness.
