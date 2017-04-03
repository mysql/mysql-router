/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_HARNESS_LOADER_INCLUDED
#define MYSQL_HARNESS_LOADER_INCLUDED

#include "config_parser.h"
#include "filesystem.h"
#include "mysql/harness/plugin.h"

#include "harness_export.h"

#include <exception>
#include <future>
#include <istream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <thread>

namespace mysql_harness {

struct Plugin;
class Path;

/**
 * Configuration file handler for the loader.
 *
 * @ingroup Loader
 *
 * Specialized version of the config file read that do some extra
 * checks after reading the configuration file.
 */

class LoaderConfig : public Config {
 public:
  template <class AssocT, class SeqT>
  explicit LoaderConfig(const AssocT& parameters,
                        const SeqT& reserved,
                        bool allow_keys = false)
          : Config(parameters, allow_keys) {
    for (const SeqT& word : reserved)
      reserved_.push_back(word);
  }

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
 * @ingroup Loader
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

class HARNESS_EXPORT Loader {
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
      : config_(defaults, reserved, Config::allow_keys),
        program_(program) {}

  /** @overload */
  template <class AssocT>
  explicit Loader(const std::string& program,
                  const AssocT& defaults = AssocT())
      : Loader(program, defaults, std::vector<std::string>()) {}

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
   * Return true if we are logging to a file, false if we are logging
   * to console instead.
   */
  bool logging_to_file() const {
    return !config_.get_default("logging_folder").empty();
  }

  /**
   * Return log filename.
   *
   * @throws std::invalid_argument if not logging to file
   */
  Path get_log_file() const {
    return Path::make_path(config_.get_default("logging_folder"),
                           program_, "log");
  }

  /**
   * Add a configuration section
   *
   * @param section ConfigSection instance to add.
   */
  void add_logger(const std::string& default_level);

  /**
   * Get reference to configuration object.
   */
  LoaderConfig &get_config() { return config_; }

 private:
  enum class Status {
    UNVISITED,
    ONGOING,
    VISITED
  };
  void setup_info();
  void init_all();
  void start_all();
  void stop_all();
  void deinit_all();

  /**
   * Topological sort of all plugins and their dependencies.
   *
   * Will create a list of plugins in topological order from "top"
   * to "bottom".
   */
  bool topsort();
  bool visit(const std::string& name, std::map<std::string, Status>* seen,
             std::list<std::string>* order);

// FIXME mod Mats' code to make it unittestable
  /**
   * Setup and teardown of logging facility.
   */
  void setup_logging();
  void teardown_logging();

  /**
   * Plugin information for managing a plugin.
   *
   * This class represents the harness part of managing a plugin and
   * also works as an interface to the platform-specific parts of
   * loading pluging using dynamic loading features of the platform.
   */
  class HARNESS_EXPORT PluginInfo {
   public:
    PluginInfo(const std::string& folder, const std::string& library);
    PluginInfo(const PluginInfo&) = delete;
    PluginInfo(PluginInfo&&);
    PluginInfo(void* h, Plugin* ext) : handle(h), plugin(ext) {}
    ~PluginInfo();

    void load_plugin(const std::string& name);

    /**
     * Pointer to plugin structure.
     *
     * @note This pointer can be null, so remember to check it before
     * using it.
     *
     * @todo Make this member private to avoid exposing the internal
     * state.
     */
    void *handle;
    Plugin *plugin;


   private:
    class Impl;
    Impl* impl_;
  };

  using PluginMap = std::map<std::string, PluginInfo>;
  using SessionList = std::vector<std::future<std::exception_ptr>>;

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

  std::queue<size_t> done_sessions_;
  std::mutex done_mutex_;
  std::condition_variable done_cond_;

  /**
   * Initialization order.
   */
  std::list<std::string> order_;

  std::string logging_folder_;
  std::string plugin_folder_;
  std::string runtime_folder_;
  std::string config_folder_;
  std::string data_folder_;
  std::string program_;
  AppInfo appinfo_;
};

//FIXME we need to move this to a better place, or think of another way of making this accessible to tests
HARNESS_EXPORT
void setup_logging(const std::string& program,
                   const std::string& logging_folder,
                   const Config& config,
                   const std::list<std::string>& modules);

} // namespace mysql_harness

#endif /* MYSQL_HARNESS_LOADER_INCLUDED */
