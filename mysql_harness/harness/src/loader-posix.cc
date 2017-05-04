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

#include "mysql/harness/filesystem.h"

#include "exception.h"

#include <dlfcn.h>
#include <unistd.h>

#include <cassert>
#include <sstream>

namespace mysql_harness {

////////////////////////////////////////////////////////////////
// class Loader

void Loader::start() {
  for (auto& name : available())
    load(name.first, name.second);
  init_all();
  start_all();
}

////////////////////////////////////////////////////////////////
// class Loader::PluginInfo::Impl

class Loader::PluginInfo::Impl {
 public:
  Impl(const std::string& plugin_folder,
       const std::string& library_name);
  ~Impl();

  Path path;
  void* handle;
};

Loader::PluginInfo::Impl::Impl(const std::string& plugin_folder,
                               const std::string& library_name)
  : path(Path::make_path(plugin_folder, library_name, "so")),
    handle(dlopen(path.c_str(), RTLD_LOCAL | RTLD_NOW)) {
  if (handle == nullptr)
    throw bad_plugin(dlerror());
}

Loader::PluginInfo::Impl::~Impl() {
  dlclose(handle);
}

////////////////////////////////////////////////////////////////
// class Loader::PluginInfo

Loader::PluginInfo::~PluginInfo() {
  delete impl_;
}

Loader::PluginInfo::PluginInfo(PluginInfo&& p) {
  if (&p != this) {
    this->impl_ = p.impl_;
    p.impl_ = NULL;
    this->plugin = p.plugin;
    p.plugin = NULL;
    this->handle = p.handle;
    p.handle = NULL;
  }
}

Loader::PluginInfo::PluginInfo(const std::string& plugin_folder,
                               const std::string& library_name)
  : impl_(new Impl(plugin_folder, library_name)) {}

void Loader::PluginInfo::load_plugin(const std::string& name) {
  assert(impl_->handle);

  dlerror();  // clear any previous errors

  std::string symbol = "harness_plugin_" + name;
  Plugin* plugin = reinterpret_cast<Plugin*>(dlsym(impl_->handle, symbol.c_str()));

  const char* error = dlerror();
  if (error) {
    std::ostringstream buffer;
    buffer << "Loading plugin '" << name << "' failed: " << error;
    throw bad_plugin(buffer.str());
  }

  this->plugin = plugin;
}

} // namespace mysql_harness
