/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef LOADER_INCLUDED
#define LOADER_INCLUDED

#include "config_parser.h"
#include "filesystem.h"
#include "plugin.h"

#include <istream>
#include <list>
#include <map>
#include <thread>
#include <set>
#include <string>

struct Plugin;
class Path;

/**
 * Configuration file handler for the loader.
 *
 * Specialized version of the config file read that do some extra
 * checks after reading the configuration file.
 */

class LoaderConfig : public Config {
public:
  using Config::Config;

  /**
   * Fill and check the configuration.
   *
   * This function will fill in default values for any options that
   * should have default values and check all sections to make sure
   * that they have valid values.
   *
   * @exception bad_section Thrown if the configuration is not correct.
   */
  void fill_and_check();
};

/**
 * Loader class.
 *
 * The loader class is responsible for managing the life-cycle of
 * plugins in the harness. Each plugin goes through five steps in the
 * life-cycle, of which three are optional:
 *
 * 1. Loading
 * 2. Initialization
 * 3. Start
 * 4. Deinitialization
 * 5. Unloading
 *
 * When *loading* the plugin is loaded using the dynamic library
 * support available on the operating system. Symbols are evaluated
 * lazily (for example, the `RTLD_LAZY` flag is used for `dlopen`) to
 * allow plugins to be loaded in any order. The symbols that are
 * exported by the plugin are are made available to all other plugins
 * loaded (flag `RTLD_GLOBAL` to `dlopen`).
 *
 * As part of the loading procedure, the *plugin structure* (see
 * Plugin class) is fetched from the module and used for the three
 * optional steps below.
 *
 * After all the plugins are successfully loaded, each plugin is given
 * a chance to perform initialization. This step is only executed if
 * the plugin structure defines an `init` function. Note that it is
 * guaranteed that the init function of a plugin is called *after* the
 * `init` function of all plugins it requires have been called.
 *
 * After all plugins have been successfully initialized, a thread is
 * created for each plugin that have a non-NULL `start` field in the
 * plugin structure. The threads are started in an arbitrary order,
 * so you have to be careful about not assuming that, for example,
 * other plugins required by the plugin have started their thread. If
 * the plugin do not define a `start` field, no thread is created. If
 * necessary, the plugin can spawn more threads using standard POSIX
 * thread calls. These functions are exported by the harness and is
 * available to all plugins.
 *
 * After all threads have stopped, regardless of they stopped with an
 * error or not, the plugins are deinitialized in reverse order of
 * initialization by calling the function in the `deinit` field of the
 * plugin structure. Regardless of whether the deinit functions
 * return an error or not, all plugins will be deinitialized.
 *
 * After a plugin have deinitialized, it can be unloaded. It is
 * guaranteed that no module is unloaded before it has been
 * deinitialized.
 */

class Loader {
public:
  /**
   * Constructor for Loader.
   *
   * @param defaults Associative array of defaults.
   * @param reserved Sequence container of patterns for reserved words.
   */

  template <class AssocT, class SeqT>
  Loader(const std::string& program,
         const AssocT& defaults = AssocT(),
         const SeqT& reserved = SeqT())
    : config_(defaults, reserved, Config::allow_keys)
    , program_(program)
  {
  }

  /** @overload */
  template <class AssocT>
  Loader(const std::string& program,
         const AssocT& defaults = AssocT())
    : Loader(program, defaults, std::vector<std::string>())
  {
  }

  /**
   * Destructor.
   *
   * The destructor will call dlclose() on all unclosed shared
   * libraries.
   */

  ~Loader();


  /**
   * Read a configuration entry.
   *
   * This will read one configuration entry and incorporate it into
   * the loader configuration. The entry can be either a directory or
   * a file.
   *
   * This function allow reading multiple configuration entries and
   * can be used to load paths of configurations. An example of how it
   * could be used is:
   *
   * @code
   * Loader loader;
   * for (auto&& entry: my_path)
   *    loader.read(entry);
   * @endcode
   *
   * @param path Path to configuration entry to read.
   */
  void read(const Path& path);

  /**
   * Fetch available plugins.
   *
   * @return List of names of available plugins.
   */

  std::list<Config::SectionKey> available() const;

  /**
   * Load the named plugin from a specific library.
   *
   * @param plugin_name Name of the plugin to be loaded.
   *
   * @param library_name Name of the library the plugin should be
   * loaded from.
   */
  Plugin *load_from(const std::string& plugin_name,
                    const std::string& library_name);


  /**
   * Load the named plugin and all dependent plugins.
   *
   * @param plugin_name Name of the plugin to be loaded.
   * @param key Key of the plugin to be loaded.
   *
   * @post After the execution of this procedure, the plugin and all
   * plugins required by that plugin will be loaded.
   */
  Plugin *load(const std::string& plugin_name);

  /** @overload */
  Plugin *load(const std::string& plugin_name, const std::string& key);

  bool is_loaded(const std::string& ext) const;

  /**
   * Initialize and start all loaded plugins.
   *
   * All registered plugins will be initialized in proper order and
   * started (if they have a `start` callback).
   */
  void start();

  /**
   */
  Path get_log_file() const {
    return Path::make_path(config_.get_default("logging_folder"), program_, "log");
  }

  /**
   * Add a configuration section
   *
   * @param section ConfigSection instance to add.
   */
  void add_logger(const std::string& default_level);

private:
  void setup_info();
  void init_all();
  void start_all();
  void deinit_all();

  /**
   * Topological sort of all plugins and their dependencies.
   *
   * Will create a list of plugins in topological order from "top"
   * to "bottom".
   */
  bool topsort();
  bool visit(const std::string& name,
             std::map<std::string, int>& seen,
             std::list<std::string>& order);

private:
  class PluginInfo {
  public:
    explicit PluginInfo(void* h, Plugin* ext)
      : handle(h), plugin(ext)
    {
    }

    void *handle;
    Plugin *plugin;
  };

  typedef std::map<std::string, PluginInfo> PluginMap;
  typedef std::vector<std::thread> SessionList;

  // Init order is important, so keep config_ first.

  /**
   * Configuration sections for all plugins.
   */
  LoaderConfig config_;

  /**
   * Map of all plugins (without key name).
   */
  PluginMap plugins_;

  /**
   * List of all active session.
   */
  SessionList sessions_;

  /**
   * Initialization order.
   */
  std::list<std::string> order_;

  std::string logging_folder_;
  std::string plugin_folder_;
  std::string runtime_folder_;
  std::string config_folder_;
  std::string program_;
  AppInfo appinfo_;
};

#endif /* LOADER_INCLUDED */
