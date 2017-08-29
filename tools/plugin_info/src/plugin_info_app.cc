/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin_info_app.h"
#include "library_file.h"

#include "router_config.h"

Plugin_info_app::Plugin_info_app(int argc,
                                 const char** argv,
                                 std::ostream& out_stream,
                                 std::ostream& out_stream_err):
                    argc_(argc), argv_(argv),
                    out_stream_(out_stream),
                    out_stream_err_(out_stream_err)
{}

int Plugin_info_app::run() {
  if (argc_ == 2) {
    std::string command = argv_[1];
    if (command == "--help") {
      print_usage(argv_[0]);
      return 0;
    }

    if (command == "--version") {
      print_version();
      return 0;
    }
  }

  if (argc_ != 3) {
    print_usage(argv_[0]);
    return -1;
  }

  const std::string file_name = argv_[1];
  const std::string plugin_name = argv_[2];

  try {
    Library_file plugin_file(file_name, plugin_name);
    uint32_t abi_version = plugin_file.get_abi_version();

    if (abi_version > mysql_harness::PLUGIN_ABI_VERSION) {
      throw std::runtime_error("Unsupported plugin ABI version: "
                               + Plugin_info::get_abi_version_str(abi_version));
    }

    // all the other versions so far have the same structure from our perspective
    Plugin_v1* plugin = plugin_file.get_plugin_struct<Plugin_v1>(plugin_name);
    Plugin_info plugin_info(*plugin);
    out_stream_ << plugin_info << std::endl;
  }
  catch (std::runtime_error& err) {
    out_stream_err_ << err.what() << std::endl;
    return -1;
  }

  return 0;
}

void Plugin_info_app::print_usage(const char* exec_name) {
  out_stream_err_ << "Usage:" << std::endl;
  out_stream_err_ << "\t" << exec_name << " <mysqlrouter_plugin_file> <mysql_plugin_name>" << std::endl;
  out_stream_err_ << "Example:" << std::endl;
#ifndef _WIN32
  out_stream_err_ << "\t" << exec_name << " /usr/lib/mysqlrouter/routing.so routing" << std::endl;
#else
  out_stream_err_ << "\t" << exec_name << " \"c:\Program Files (x86)\\MySQL\\MySQL Router 2.1\\lib\\routing.dll\" routing" << std::endl;
#endif
  out_stream_err_ << "To print help information:" << std::endl;
  out_stream_err_ << "\t" << exec_name << " --help" << std::endl;
  out_stream_err_ << "To print application version:" << std::endl;
  out_stream_err_ << "\t" << exec_name << " --version" << std::endl;
}

void Plugin_info_app::print_version() {
  out_stream_err_ << kPluginInfoAppName << " "
                  << MYSQL_ROUTER_VERSION << std::endl; // we use the same version as MySQLRouter
}
