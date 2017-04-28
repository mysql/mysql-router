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

#include "mysql/harness/loader.h"

////////////////////////////////////////
// Package include files
#include "mysql/harness/filesystem.h"
#include "mysql/harness/plugin.h"
#include "designator.h"
#include "exception.h"
#include "utilities.h"

////////////////////////////////////////
// Standard include files
#include <algorithm>
#include <cassert>
#include <cctype>
#include <exception>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifndef _WINDOWS
#  include <dlfcn.h>
#  include <unistd.h>
#endif

using mysql_harness::utility::find_range_first;
using mysql_harness::utility::make_range;
using mysql_harness::utility::reverse;

using mysql_harness::Path;
using mysql_harness::Config;

using std::ostringstream;

/**
 * @defgroup Loader Plugin loader
 *
 * Plugin loader for loading and working with plugins.
 */

namespace mysql_harness {

void LoaderConfig::fill_and_check() {
  // Set the default value of library for all sections that do not
  // have the library set.
  for (auto&& elem : sections_) {
    if (!elem.second.has("library")) {
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
  for (auto&& iter = sections_.begin() ; iter != sections_.end() ; ++iter) {
    const std::string& section_name = iter->second.name;
    const auto& seclist = find_range_first(sections_, section_name, iter);

    const std::string& library = seclist.first->second.get("library");
    auto library_mismatch = [&library](decltype(*seclist.first)& it) -> bool {
      return it.second.get("library") != library;
    };

    auto mismatch = find_if(seclist.first, seclist.second, library_mismatch);
    if (mismatch != seclist.second) {
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

Loader::~Loader() {
  // TODO PM (after Harness merge):
  // These were brought in during Harness merge. They should be removed in
  // upcoming Lifecycle patch, which already calls these elsewhere.
  stop_all();
  deinit_all();
}

Plugin* Loader::load_from(const std::string& plugin_name,
                          const std::string& library_name) {
  std::string error;
  setup_info();

  // We always load the library (even if it is already loaded) to
  // honor potential dynamic library open/close reference counts. It
  // is up to the platform implementation to ensure that multiple
  // instances of a library can be handled.

  PluginInfo info(plugin_folder_, library_name);

  info.load_plugin(plugin_name);

  // Check that ABI version and architecture match
  auto plugin = info.plugin;
  if ((plugin->abi_version & 0xFF00) != (PLUGIN_ABI_VERSION & 0xFF00) ||
      (plugin->abi_version & 0xFF) > (PLUGIN_ABI_VERSION & 0xFF)) {
    ostringstream buffer;
    buffer.setf(std::ios::hex, std::ios::basefield);
    buffer.setf(std::ios::showbase);
    buffer << "Bad ABI version - plugin version: " << plugin->abi_version
           << ", loader version: " << PLUGIN_ABI_VERSION;
    throw bad_plugin(buffer.str());
  }

  // Recursively load dependent modules, we skip NULL entries since
  // the user might have added these by accident (for example, he
  // assumed that the array was NULL-terminated) and they can safely
  // be ignored instead of raising an error.
  for (auto req : make_range(plugin->requires, plugin->requires_length)) {
    if (req != nullptr) {
      // Parse the designator to extract the plugin and constraints.
      Designator designator(req);

      // Load the plugin using the plugin name.
      Plugin* dep_plugin = load(designator.plugin);

      // Check that the version of the plugin match what the
      // designator expected and raise an exception if they don't
      // match.
      if (!designator.version_good(Version(dep_plugin->plugin_version))) {
        Version version(dep_plugin->plugin_version);
        std::ostringstream buffer;
        buffer << designator.plugin << ": plugin version was " << version
               << ", expected " << designator.constraint;
        throw bad_plugin(buffer.str());
      }
    }
  }

  // If all went well, we register the plugin and return a
  // pointer to it.
  plugins_.emplace(plugin_name, std::move(info));
  return plugin;
}

Plugin *Loader::load(const std::string &plugin_name, const std::string &key) {
  ConfigSection& plugin = config_.get(plugin_name, key);
  const auto& library_name = plugin.get("library");
  return load_from(plugin_name, library_name);
}

Plugin* Loader::load(const std::string& plugin_name) {
  Config::SectionList plugins = config_.get(plugin_name);
  if (plugins.size() > 1) {
    std::ostringstream buffer;
    buffer << "Section name '" << plugin_name
           << "' is ambiguous. Alternatives are:";
    for (const ConfigSection* plugin : plugins)
      buffer << " " << plugin->key;
    throw bad_section(buffer.str());
  } else if (plugins.size() == 0) {
    std::ostringstream buffer;
    buffer << "Section name '" << plugin_name
           << "' does not exist";
    throw bad_section(buffer.str());
  }

  assert(plugins.size() == 1);
  const ConfigSection* section = plugins.front();
  const std::string& library_name = section->get("library");
  return load_from(plugin_name, library_name);
}

bool Loader::is_loaded(const std::string &name) const {
  return plugins_.find(name) != plugins_.end();
}

std::list<Config::SectionKey> Loader::available() const {
  return config_.section_names();
}

void Loader::read(const Path& path) {
  config_.read(path);

  // This means it is checked after each file load, which might
  // require changes in the future if checks that cover the entire
  // configuration are added. Right now it just contain safety checks.
  config_.fill_and_check();
}

void Loader::setup_info() {
  logging_folder_ = config_.get_default("logging_folder");
  plugin_folder_ = config_.get_default("plugin_folder");
  runtime_folder_ = config_.get_default("runtime_folder");
  config_folder_ = config_.get_default("config_folder");
  data_folder_ = config_.get_default("data_folder");

  appinfo_.logging_folder = logging_folder_.c_str();
  appinfo_.plugin_folder = plugin_folder_.c_str();
  appinfo_.runtime_folder = runtime_folder_.c_str();
  appinfo_.config_folder = config_folder_.c_str();
  appinfo_.data_folder = data_folder_.c_str();
  appinfo_.config = &config_;
  appinfo_.program = program_.c_str();
}

void Loader::init_all() {
  if (!topsort())
    throw std::logic_error("Circular dependencies in plugins");

  for (const std::string& plugin_key : reverse(order_)) {
    PluginInfo &info = plugins_.at(plugin_key);
    if (info.plugin->init && info.plugin->init(&appinfo_))
      throw std::runtime_error("Plugin init failed");
  }
}

void Loader::start_all() {
  // Start all the threads
  int stoppable_jobs = 0;
  for (const ConfigSection* section : config_.sections()) {
    PluginInfo& plugin = plugins_.at(section->name);
    void (*fptr)(const ConfigSection*) = plugin.plugin->start;
    if (fptr) {
      auto dispatch = [section, fptr, this](size_t position)
          -> std::exception_ptr {
        std::exception_ptr eptr;
        try {
          fptr(section);
        } catch (...) {
          eptr = std::current_exception();
        }

        {
          std::lock_guard<std::mutex> lock(done_mutex_);
          done_sessions_.push(position);
        }

        done_cond_.notify_all();
        return eptr;
      };
      std::future<std::exception_ptr> fut =
          std::async(std::launch::async, dispatch, sessions_.size());
      sessions_.push_back(std::move(fut));
      if (plugin.plugin->stop == nullptr)
        ++stoppable_jobs;
    }
  }

  std::exception_ptr except;
  while (stoppable_jobs-- > 0) {
    std::unique_lock<std::mutex> lock(done_mutex_);
    done_cond_.wait(lock, [this]{ return done_sessions_.size() > 0; });
    auto idx = done_sessions_.front();
    done_sessions_.pop();
    std::exception_ptr eptr = sessions_[idx].get();
    if (eptr && !except) {
      stop_all();
      except = eptr;
    }
  }

  // We just throw the first exception that was raised. If there are
  // other exceptions, they are ignored.
  if (except)
    std::rethrow_exception(except);
}

void Loader::stop_all() {
  for (auto&& section : config_.sections()) {
    try {
      PluginInfo& plugin = plugins_.at(section->name);
      void (*fptr)(const ConfigSection*) = plugin.plugin->stop;
      if (fptr) {
        fptr(section);
      }
    } catch (std::out_of_range& exc) {
      // We do nothing here, because throwing would only make things worse.
      // This would be analogous to throwing in a destructor. Keep in mind
      // that this code runs during Loader teardown, which means:
      // - Loader will not exist shortly after, so its errorous state doesn't
      //   matter much
      // - it already might be executing due to an error (such as missing
      //   plugin for some config section - the exact case this try/catch traps)
    }
  }
}

void Loader::deinit_all() {
  for (auto& name : order_) {
    PluginInfo& info = plugins_.at(name);
    if (info.plugin->deinit)
      info.plugin->deinit(&appinfo_);
  }
}

bool Loader::topsort() {
  std::map<std::string, Loader::Status> status;
  std::list<std::string> order;
  for (std::pair<const std::string, PluginInfo>& plugin : plugins_) {
    bool succeeded = visit(plugin.first, &status, &order);
    if (!succeeded)
      return false;
  }
  order_.swap(order);
  return true;
}

bool Loader::visit(const std::string& designator,
                   std::map<std::string, Loader::Status>* status,
                   std::list<std::string>* order) {
  Designator info(designator);
  switch ((*status)[info.plugin]) {
  case Status::VISITED:
    return true;

  case Status::ONGOING:
    // If we see a node we are processing, it's not a DAG and cannot
    // be topologically sorted.
    return false;

  case Status::UNVISITED:
    {
      (*status)[info.plugin] = Status::ONGOING;
      if (Plugin *plugin = plugins_.at(info.plugin).plugin) {
        for (auto required : make_range(plugin->requires,
                                        plugin->requires_length)) {
          assert(required != NULL);
          bool succeeded = visit(required, status, order);
          if (!succeeded)
            return false;
        }
      }
      (*status)[info.plugin] = Status::VISITED;
      order->push_front(info.plugin);
      return true;
    }
  }
  return true;
}

} // namespace mysql_harness
