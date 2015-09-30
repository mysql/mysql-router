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

#include "loader.h"

////////////////////////////////////////
// Package include files
#include "designator.h"
#include "exception.h"
#include "filesystem.h"
#include "plugin.h"
#include "utilities.h"

////////////////////////////////////////
// Standard include files
#include <algorithm>
#include <cassert>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <sstream>
#include <thread>

#include <dlfcn.h>
#include <unistd.h>

using std::ostringstream;

void LoaderConfig::fill_and_check()
{
  // Set the default value of library for all sections that do not
  // have the library set.
  for (auto&& elem: sections_)
  {
    if (!elem.second.has("library"))
    {
      const std::string& section_name = elem.first.first;

      // Section name is always a always stored as lowercase legal C
      // identifier, hence it is also legal as a file name, but we
      // assert that to make sure.
      assert(std::all_of(section_name.begin(), section_name.end(),
                         [](const char ch) -> bool {
                           return isalnum(ch) || ch == '_';
                         }));

      elem.second.set("library", section_name);
    }
  }

  // Check all sections to make sure that the values are correct.
  for (auto&& iter = sections_.begin() ; iter != sections_.end() ; ++iter)
  {
    const std::string& section_name = iter->second.name;
    const auto& seclist = find_range_first(sections_, section_name, iter);

    const std::string& library = seclist.first->second.get("library");
    auto library_mismatch = [&library](decltype(*seclist.first)& it) -> bool {
      return it.second.get("library") != library;
    };

    auto mismatch = find_if(seclist.first, seclist.second, library_mismatch);
    if (mismatch != seclist.second)
    {
      const auto& name = seclist.first->first;
      std::ostringstream buffer;
      buffer << "Library for section '"
             << name.first << ":" << name.second
             << "' does not match library in section '"
             << mismatch->first.first << ":" << mismatch->first.second;
      throw bad_section(buffer.str());
    }
  }
}

Loader::~Loader()
{
}

// We use RTLD_LAZY when opening the file. This will make function
// references not be resolve until they are actually used. All
// interfaces between plugins and the harness have to be as functions.
//
// In addition, we use RTLD_GLOBAL to expose the plugin's symbols to
// other plugins.

Plugin* Loader::load_from(const std::string& plugin_name,
                          const std::string& library_name)
{
  setup_info();
  // Create a path to the plugin file.
  Path path = Path::make_path(plugin_folder_, library_name, "so");

  // We always load the library (even if it is already loaded) to
  // honor dlopen()/dlclose() reference counts.
  const unsigned int flags = RTLD_LAZY | RTLD_GLOBAL;
  void *handle = dlopen(path.c_str(), flags);

  // We need to call dlerror() here regardless of whether handle is
  // NULL or not since we need to clear the error flag prior to
  // calling dlsym() below if the handle was not NULL.
  const char *error = dlerror();
  if (handle == NULL)
    throw bad_plugin(error);

  // If it was already loaded previously, we can skip the init part,
  // so let's check that and return early in that case.
  PluginMap::const_iterator it = plugins_.find(plugin_name);
  if (it != plugins_.end())
  {
    // This should not happen, but let's check to make sure
    if (it->second.handle != handle)
      throw std::runtime_error("Reloading returned different handle");
    return it->second.plugin;
  }

  // Try to load the symbol either using the same name as the plugin,
  // or with the suffix "_plugin", or with the prefix
  // "harness_plugin_". The latter case is to provide an alternative
  // when the plugin name need to be used for other purposes.
  std::vector<std::string> alternatives{
    plugin_name,
    plugin_name + "_plugin",
    "harness_plugin_" + plugin_name
  };

  Plugin *plugin = nullptr;
  for (auto&& symbol: alternatives) {
    plugin = static_cast<Plugin*>(dlsym(handle, symbol.c_str()));
    if (plugin)
      break;
  }

  if (plugin == nullptr)
  {
    std::ostringstream buffer;
    buffer << "symbol '" << plugin_name << "' not found in " << path.str();
    throw bad_plugin(buffer.str());
  }

  // Check that ABI version and architecture match
  if ((plugin->abi_version & 0xFF00) != (PLUGIN_ABI_VERSION & 0xFF00) ||
      (plugin->abi_version & 0xFF) > (PLUGIN_ABI_VERSION & 0xFF))
  {
    throw bad_plugin("Bad ABI version");
  }

  // Recursively load dependent modules, we skip NULL entries since
  // the user might have added these by accident (for example, he
  // assumed that the array was NULL-terminated) and they can safely
  // be ignored instead of raising an error.
  for (auto req : make_range(plugin->requires, plugin->requires_length))
  {
    if (req != nullptr)
    {
      // Parse the designator to extract the plugin and constraints.
      Designator designator(req);

      // Load the plugin using the plugin name.
      Plugin* dep_plugin = load(designator.plugin);

      // Check that the version of the plugin match what the
      // designator expected and raise an exception if they don't
      // match.
      if (!designator.version_good(dep_plugin->plugin_version))
      {
        Version version(dep_plugin->plugin_version);
        std::ostringstream buffer;
        buffer << "plugin version was " << version
               << ", expected " << designator.constraint;
        throw bad_plugin(buffer.str());
      }
    }
  }

  // If all went well, we register the plugin and return a
  // pointer to it.
  plugins_.emplace(plugin_name, PluginInfo(handle, plugin));
  return plugin;
}

Plugin* Loader::load(const std::string& plugin_name, const std::string& key)
{
  auto& plugin = config_.get(plugin_name, key);
  const auto& library_name = plugin.get("library");
  return load_from(plugin_name, library_name);
}

Plugin* Loader::load(const std::string& plugin_name)
{
  auto plugins = config_.get(plugin_name);
  if (plugins.size() > 1) {
    std::ostringstream buffer;
    buffer << "Section name '" << plugin_name
           << "' is ambiguous. Alternatives are:";
    for (const ConfigSection* plugin: plugins)
      buffer << " " << plugin->key;
    throw bad_section(buffer.str());
  }
  else if (plugins.size() == 0)
  {
    std::ostringstream buffer;
    buffer << "Section name '" << plugin_name
           << "' does not exist";
    throw bad_section(buffer.str());
  }

  assert(plugins.size() == 1);
  const ConfigSection* section = plugins.front();
  const auto& library_name = section->get("library");
  return load_from(plugin_name, library_name);
}


void Loader::start()
{
  for (auto& name : available())
    load(name.first, name.second);
  init_all();
  start_all();
}

bool Loader::is_loaded(const std::string& name) const
{
  return plugins_.find(name) != plugins_.end();
}

auto Loader::available() const
  -> decltype(config_.section_names())
{
  return config_.section_names();
}

void Loader::read(const Path& path)
{
  config_.read(path);

  // This means it is checked after each file load, which might
  // require changes in the future if checks that cover the entire
  // configuration are added. Right now it just contain safety checks.
  config_.fill_and_check();
}

void Loader::setup_info()
{
  logging_folder_ = config_.get_default("logging_folder");
  plugin_folder_ = config_.get_default("plugin_folder");
  runtime_folder_ = config_.get_default("runtime_folder");
  config_folder_ = config_.get_default("config_folder");

  appinfo_.logging_folder = logging_folder_.c_str();
  appinfo_.plugin_folder = plugin_folder_.c_str();
  appinfo_.runtime_folder = runtime_folder_.c_str();
  appinfo_.config_folder = config_folder_.c_str();
  appinfo_.config = &config_;
  appinfo_.program = program_.c_str();
}

void Loader::init_all()
{
  if (!topsort())
    throw std::logic_error("Circular dependencies in plugins");

  for (auto& plugin_key : reverse(order_))
  {
    PluginInfo &info = plugins_.at(plugin_key);
    if (info.plugin->init && info.plugin->init(&appinfo_))
      throw std::runtime_error("Plugin init failed");
  }
}

void Loader::start_all()
{
  // Start all the threads
  for (auto&& section: config_.sections())
  {
    auto& plugin = plugins_.at(section->name);
    void (*fptr)(const ConfigSection*) = plugin.plugin->start;
    if (fptr)
      sessions_.push_back(std::thread(fptr, section));
  }

  // Reap all the threads
  for (auto&& session : sessions_)
  {
    assert(session.joinable());
    session.join();
  }
}

void Loader::deinit_all() {
  for (auto& name : order_)
  {
    PluginInfo& info = plugins_.at(name);
    if (info.plugin->deinit)
      info.plugin->deinit(&appinfo_);
  }
}

enum {
  UNVISITED,
  ONGOING,
  VISITED
};

bool Loader::topsort()
{
  std::map<std::string, int> status;
  std::list<std::string> order;
  for (auto& plugin : plugins_)
  {
    bool succeeded = visit(plugin.first, status, order);
    if (!succeeded)
      return false;
  }
  order_.swap(order);
  return true;
}

bool Loader::visit(const std::string& designator,
                   std::map<std::string, int>& status,
                   std::list<std::string>& order)
{
  Designator info(designator);
  switch (status[info.plugin])
  {
  case VISITED:
    return true;

  case ONGOING:
    // If we see a node we are processing, it's not a DAG and cannot
    // be topologically sorted.
    return false;

  case UNVISITED:
    {
      status[info.plugin] = ONGOING;
      if (Plugin *plugin = plugins_.at(info.plugin).plugin)
      {
        for (auto required : make_range(plugin->requires, plugin->requires_length))
        {
          assert(required != NULL);
          bool succeeded = visit(required, status, order);
          if (!succeeded)
            return false;
        }
      }
      status[info.plugin] = VISITED;
      order.push_front(info.plugin);
      return true;
    }
  }
  return true;
}

void Loader::add_logger(const std::string& default_level) {
  if (!config_.has("logger")) {
    auto&& section = config_.add("logger");
    section.add("library", "logger");
    section.add("level", default_level);
  }
}
