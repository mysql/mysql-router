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

#include "arg_handler.h"
#include "mysqlrouter/utils.h"
#include "utils.h"

#include <algorithm>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <string>
#include <regex>
#include <vector>

using std::string;
using std::vector;
using std::unique_ptr;
using mysqlrouter::wrap_string;
using mysqlrouter::string_format;

void CmdArgHandler::add_option(const OptionNames names, const string description, const CmdOptionValueReq value_req,
                               const string metavar, ActionFunc action) noexcept {
  assert(!names.empty());  // need none empty names container
  for (auto name : names) {
    assert(is_valid_option_name(name));  // valid option names
    assert(options_.end() == find_option(name));  // unique name
  }

  options_.emplace_back(names, description, value_req, metavar, action);
}

void CmdArgHandler::add_option(const CmdOption &other) noexcept {
  assert(!other.names.empty());  // need none empty names container
  for (auto name : other.names) {
    assert(is_valid_option_name(name));  // valid option names
    assert(options_.end() == find_option(name));  // unique name
  }

  options_.emplace_back(other.names, other.description, other.value_req, other.metavar, other.action);
}

OptionContainer::iterator CmdArgHandler::find_option(const string name) noexcept {
  for (auto opt = options_.begin(); opt != options_.end(); ++opt) {
    auto res = std::find(opt->names.begin(), opt->names.end(), name);
    if (res != opt->names.end()) {
      return opt;
    }
  }

  return options_.end();
}

/** @fn CmdArgHandler::is_valid_option_name(const string name) noexcept
 *
 * @devnote
 * Some compilers, like gcc 4.8, have no support for C++11 regular expression.
 * @enddevnote
 */
bool CmdArgHandler::is_valid_option_name(const string name) noexcept {
  // Handle tokens like -h or -v
  if (name.size() == 2 && name.at(1) != '-') {
    return name.at(0) == '-';
  }

  // Handle tokens like --help or --with-sauce
  try {
    return std::regex_match(name, std::regex("^--[A-Za-z][A-Za-z_-]*[A-Za-z]$"));
  } catch (std::regex_error) {
    // Fall back to some non-regular expression checks
    if (name.size() < 4) {
      return false;
    } else if (name.find("--") != 0) {
      return false;
    } else if (name.back() == '-' || name.back() == '_') {
      return false;
    }

    // First 2 characters after -- must be alpha
    if (!(isalpha(name.at(2)) && isalpha(name.at(3)))) {
      return false;
    }

    if (name.size() == 4) {
      return true;
    }

    // Rest can be either alpha, dash or underscore
    if (name.size() == 4) {
      return true;
    }
    return std::find_if(name.begin() + 4, name.end(),
                        [](char c) {
                          return isalpha(c) || c == '-' || c == '_';
                        }) != name.end();
  }
}

void CmdArgHandler::process(const vector<string> arguments) {
  size_t pos;
  string argpart;
  string value;
  rest_arguments_.clear();
  auto args_end = arguments.end();
  vector<std::pair<ActionFunc, string> > schedule;

  for (auto part = arguments.begin(); part < args_end; ++part) {

    if ((pos = (*part).find('=')) != string::npos) {
      // Option like --config=/path/to/config.ini
      argpart = (*part).substr(0, pos);
      value = (*part).substr(pos + 1);
    } else {
      argpart = *part;
      value = "";
    }

    // Save none-option arguments
    if (!is_valid_option_name(argpart)) {
      if (!allow_rest_arguments) {
        throw std::invalid_argument("invalid argument '" + argpart +"'.");
      }
      rest_arguments_.push_back(argpart);
      continue;
    }

    auto opt_iter = find_option(argpart);
    if (options_.end() != opt_iter) {
      auto &option = *opt_iter;
      string err_value_req = string_format("option '%s' requires a value.", argpart.c_str());

      if (option.value_req == CmdOptionValueReq::required) {
        if (value.empty()) {
          if (part == (args_end - 1)) {
            // No more parts to get value from
            throw std::invalid_argument(err_value_req);
            exit(1);
          }

          ++part;
          if (part->at(0) == '-') {
            throw std::invalid_argument(err_value_req);
            exit(1);
          }
          value = *part;
        }
      } else if (option.value_req == CmdOptionValueReq::optional) {
        if (value.empty() && part != (args_end - 1)) {
          ++part;
          if (part->at(0) != '-') {
            value = *part;
          }
        }
      }

      // Execute actions alter
      if (option.action != nullptr) {
        schedule.emplace_back(option.action, value);
      }
    } else {
      throw std::invalid_argument(string_format("unknown option '%s'.", argpart.c_str()));
    }
  }

  // Execute actions after processing
  for(auto it: schedule) {
    std::bind(it.first, it.second)();
  }
}

vector<string> CmdArgHandler::usage_lines(const string prefix, const string rest_metavar, size_t width) noexcept {
  std::stringstream ss;
  vector<string> usage;

  for (auto option = options_.begin(); option != options_.end(); ++option) {
    ss.clear();
    ss.str(string());

    ss << "[";
    for (auto name = option->names.begin(); name != option->names.end(); ++name) {
      ss << *name;
      if (name == --option->names.end()) {
        if (option->value_req != CmdOptionValueReq::none) {
          if (option->value_req == CmdOptionValueReq::optional) {
            ss << "=[";
          } else {
            ss << "=";
          }
          ss << "<" << (option->metavar.empty() ? "VALUE" : option->metavar) << ">";
          if (option->value_req == CmdOptionValueReq::optional) {
            ss << "]";
          }
        }
        ss << "]";
      } else {
        ss << "|";
      }

    }
    usage.push_back(ss.str());
  }

  if (allow_rest_arguments && !rest_metavar.empty()) {
    ss.clear();
    ss.str(string());
    ss << "[" << rest_metavar << "]";
    usage.push_back(std::move(ss.str()));
  }

  ss.clear();
  ss.str(string());
  size_t line_size = 0;
  vector<string> result{};

  ss << prefix;
  line_size = ss.str().size();
  auto indent = string(line_size, ' ');

  auto end_usage = usage.end();
  for (auto item = usage.begin(); item != end_usage; ++item) {
    assert(((*item).size() + indent.size()) < width); // option can not be bigger than width
    auto need_newline = (line_size + (*item).size() + indent.size()) > width;

    if (need_newline) {
      result.push_back(ss.str());
      ss.clear();
      ss.str(string());
      ss << indent;
    }

    ss << " " << *item;
    line_size = ss.str().size();
  }

  // Add the last line
  result.push_back(ss.str());

  return result;
}

vector<string> CmdArgHandler::option_descriptions(const size_t width, const size_t indent) noexcept {
  std::stringstream ss;
  vector<string> desc_lines;

  for (auto option = options_.begin(); option != options_.end(); ++option) {
    auto value_req = option->value_req;
    ss.clear();
    ss.str(string());

    ss << "  ";
    for (auto iter_name = option->names.begin(); iter_name != option->names.end(); ++iter_name) {
      auto name = *iter_name;
      ss << name;

      if (value_req != CmdOptionValueReq::none) {
        if (value_req == CmdOptionValueReq::optional) {
          ss << " [";
        }
        ss << " <" << (option->metavar.empty() ? "VALUE" : option->metavar) << ">";
        if (value_req == CmdOptionValueReq::optional) {
          ss << "]";
        }
      }

      if (iter_name != --option->names.end()) {
        ss << ", ";
      }
    }
    desc_lines.push_back(ss.str());

    ss.clear();
    ss.str(string());

    string desc = option->description;
    for (auto line: wrap_string(option->description, width, indent)) {
      desc_lines.push_back(line);
    }
  }

  return desc_lines;
}
