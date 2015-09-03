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

#include "plugin.h"
#include "utilities.h"
#include "designator.h"
#include "exception.h"
#include "filesystem.h"

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
  for (auto&& elem: m_sections)
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
  for (auto&& iter = m_sections.begin() ; iter != m_sections.end() ; ++iter)
  {
    const std::string& section_name = iter->second.name;
    const auto& seclist = find_range_first(m_sections, section_name, iter);

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
  Path path = Path::make_path(m_plugin_folder, library_name, "so");

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
  PluginMap::const_iterator it = m_plugins.find(plugin_name);
  if (it != m_plugins.end())
  {
    // This should not happen, but let's check to make sure
    if (it->second.handle != handle)
      throw std::runtime_error("Reloading returned different handle");
    return it->second.plugin;
  }

  Plugin *plugin = static_cast<Plugin*>(dlsym(handle, plugin_name.c_str()));

  if (plugin == NULL)
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
  m_plugins.emplace(plugin_name, PluginInfo(handle, plugin));
  return plugin;
}

Plugin* Loader::load(const std::string& plugin_name, const std::string& key)
{
  auto& plugin = m_config.get(plugin_name, key);
  const auto& library_name = plugin.get("library");
  return load_from(plugin_name, library_name);
}

Plugin* Loader::load(const std::string& plugin_name)
{
  auto plugins = m_config.get(plugin_name);
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
  return m_plugins.find(name) != m_plugins.end();
}

auto Loader::available() const
  -> decltype(m_config.section_names())
{
  return m_config.section_names();
}

void Loader::read(const Path& path)
{
  m_config.read(path);

  // This means it is checked after each file load, which might
  // require changes in the future if checks that cover the entire
  // configuration are added. Right now it just contain safety checks.
  m_config.fill_and_check();
}

void Loader::setup_info()
{
  m_logging_folder = m_config.get_default("logging_folder");
  m_plugin_folder = m_config.get_default("plugin_folder");
  m_runtime_folder = m_config.get_default("runtime_folder");
  m_config_folder = m_config.get_default("config_folder");

  m_appinfo.logging_folder = m_logging_folder.c_str();
  m_appinfo.plugin_folder = m_plugin_folder.c_str();
  m_appinfo.runtime_folder = m_runtime_folder.c_str();
  m_appinfo.config_folder = m_config_folder.c_str();
  m_appinfo.config = &m_config;
  m_appinfo.program = m_program.c_str();
}

void Loader::init_all()
{
  if (!topsort())
    throw std::logic_error("Circular dependencies in plugins");

  for (auto& plugin_key : reverse(m_order))
  {
    PluginInfo &info = m_plugins.at(plugin_key);
    if (info.plugin->init && info.plugin->init(&m_appinfo))
      throw std::runtime_error("Plugin init failed");
  }
}

void Loader::start_all()
{
  // Start all the threads
  for (auto&& section: m_config.sections())
  {
    auto& plugin = m_plugins.at(section->name);
    void (*fptr)(const ConfigSection*) = plugin.plugin->start;
    if (fptr)
      m_sessions.push_back(std::thread(fptr, section));
  }

  // Reap all the threads
  for (auto&& session : m_sessions)
  {
    assert(session.joinable());
    session.join();
  }
}

void Loader::deinit_all() {
  for (auto& name : m_order)
  {
    PluginInfo& info = m_plugins.at(name);
    if (info.plugin->deinit)
      info.plugin->deinit(&m_appinfo);
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
  for (auto& plugin : m_plugins)
  {
    bool succeeded = visit(plugin.first, status, order);
    if (!succeeded)
      return false;
  }
  m_order.swap(order);
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
      if (Plugin *plugin = m_plugins.at(info.plugin).plugin)
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
