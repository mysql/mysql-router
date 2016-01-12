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

#include "cmd_exec.h"
#include "router_test_helpers.h"

#include <stdexcept>
#include <cerrno>
#include <iostream>
#include <stdio.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

CmdExecResult cmd_exec(const std::string &cmd, bool include_stderr, std::string working_dir) {
  std::string app_cmd(cmd);
  std::string orig_cwd{};

  if (include_stderr) {
    app_cmd += " 2>&1";
  }

  if (!working_dir.empty()) {
    orig_cwd = change_cwd(working_dir);
  }

  auto ld_lib_path = std::getenv("LD_LIBRARY_PATH");
  if (ld_lib_path != nullptr) {
    // Linux/Solaris
    app_cmd = "LD_LIBRARY_PATH=\"" + std::string(ld_lib_path) + "\" " + app_cmd;
  }

  auto dyld_lib_path = std::getenv("DYLD_LIBRARY_PATH");
  if (dyld_lib_path != nullptr) {
    // OS X
    app_cmd = "DYLD_LIBRARY_PATH=\"" + std::string(dyld_lib_path) + "\" " + app_cmd;
  }

  FILE *fp = popen(app_cmd.c_str(), "r");

  if (!fp) {
    throw std::runtime_error("Failed opening pipe to command '" + app_cmd + "'");
  }

  char cmd_output[256];

  std::string output{};

  while (!feof(fp)) {
    if (fgets(cmd_output, 256, fp) == nullptr) {
      break;
    }
    output += cmd_output;
  }

  if (!orig_cwd.empty()) {
    change_cwd(orig_cwd);
  }

  int code = pclose(fp);
  return CmdExecResult{output, WEXITSTATUS(code), WTERMSIG(code)};
}
