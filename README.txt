MySQL Harness
=============

MySQL Harness is an extensible framework that handles loading and
unloading of *plugins*. The built-in features are dependency tracking
between plugins, configuration file handling, and support for plugin
life-cycles.

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


Documentation
-------------

Documentation can be built using Doxygen and the supplied `Doxyfile`
as follows:

    doxygen Doxyfile

The documentation will be placed in the `doc/` directory. For more
detailed information about the code, please read the documentation
rather than rely on this `README`.


Installing
----------

To install the files, use `make install`. This will install the
harness, the harness library, the header files for writing plugins,
and the available plugins that were not marked with `NO_INSTALL` (see
below).


Running
-------

To start the harness, you need a configuration file. You can find an
example in `data/router.cfg`:

    # Example configuration file for router

    [DEFAULT]
    logdir = /var/log/router
    etcdir = /etc/mysql/router
    libdir = /var/lib/router
    rundir = /var/run/router

    [example]
    library = example

    [logger]
    library = logger

The configuration file contain information about all the plugins that
should be loaded when starting and configuration options for each
plugin.  The default section contain configuration options available
in to all plugins.

To run the harness, just provide the configuration file as the only
argument:

    harness /etc/mysql/router/main.cfg

Note that the harness uses read directories for logging,
configuration, etc. from the configuration file so you have to make
sure these are present and that the section name is used to find the
plugin structure in the shared library (see below).

Typically, the harness will then load plugins from the directory
`/var/lib/router` and write log files to `/var/log/router`.


Writing Plugins
---------------

All available plugins are in the `plugins/` directory. There is one
directory for each plugin and it is assumed that it contain a
`CMakeLists.txt` file.

The main `CMakeLists.txt` file provide an `add_plugin` macro that can
be used add new plugins.

    add_plugin(<name> [ NO_INSTALL ]
               SOURCES <source> ...
               INTERFACE <header> ...)

This macro adds a plugin named `<name>` built from the given
sources. If `NO_INSTALL` is provided, it will not be installed with
the harness (useful if you have plugins used for testing, see the
`tests/` directory). Otherwise, the plugin will be installed in the
*root*`/var/lib/harness` directory.

The `INTERFACE` header files are the interface files to the plugin and
shall be used by other plugins requiring features from this
plugin. These header files will be installed alongside the harness
include files and will also be made available to other plugins while
building from source.

### Plugin Directory Structure ###

Similar to the harness, each plugin have two types of files:

* Plugin-internal files used to build the plugin. These include the
  source files and but also header files associated with each source
  file and are stored in the `src` directory of the plugin directory.
* Interface files used by other plugins. These are header files that
  are made available to other plugins and are installed alongside the
  harness installed files, usually under the directory
  `/usr/include/mysql/harness`.

It is assumed that the plugin directory contain the following two
directories:



### Application Information Structure ###

The application information structure contain some basic fields
providing information to the plugin. Currently these fields are
provided:

    struct AppInfo {
      const char *program;                 /* Name of the application */
      const char *libdir;                  /* Location of plugins */
      const char *logdir;                  /* Log file directory */
      const char *etcdir;                  /* Config file directory */
      const char *rundir;                  /* Run file directory */
      const Config* config;                /* Configuration information */
    };


### Plugin Structure ###

To define a new plugin, you have to create an instance of the
`Plugin` structure in your plugin similar to this:

    #include <mysql/harness/plugin.h>
    
    static const char* requires[] = {
      "magic (>>1.0)",
    };

    Plugin example = {
      PLUGIN_ABI_VERSION,
      "An example plugin",       // Brief description of plugin
      VERSION_NUMBER(1,0,0),     // Version number of the plugin

      // Array of required plugins
      sizeof(requires)/sizeof(*requires),
      requires,

      // Array of plugins that conflict with this one
      0,
      NULL,

      init,
      deinit,
      start,
    };


### Initialization and Cleanup ###

After the plugin is loaded, the `init()` function is called for all
plugins with a pointer to the harness (as defined above) as the only
argument.

The `init()` functions are called in dependency order so that all
`init()` functions in required plugins are called before the `init()`
function in the plugin itself.

Before the harness exits, it will call the `deinit()` function with a
pointer to the harness as the only argument.


### Starting the Plugin ###

After all the plugins have been successfully initialized, a thread
will be created for each plugins that have a `start()` function
defined.

The start function will be called with a pointer to the harness as the
only parameter. When all the plugins return from their `start()`
functions, the harness will perform cleanup and exit.


