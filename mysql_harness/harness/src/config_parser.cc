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


/**
 * @defgroup ConfigParser Configuration file parser.
 *
 * @section Configuration file format
 *
 * The configuration parser parses traditional `.INI` files consisting
 * of sections and options with values but contain some additional
 * features to provide more flexible configuration of the harness.
 */

#include "config_parser.h"

#include "utilities.h"
#include "filesystem.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>

#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using std::ostringstream;

static bool isident(const char ch)
{
  return isalnum(ch) || ch == '_';
}

static void inplace_lower(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

static std::string lower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);
  return str;
}

static void check_option(const std::string& str) {
  if (!all_of(str.begin(), str.end(), isident))
    throw bad_option("Not a legal option name: '" + str + "'");
}

ConfigSection::ConfigSection(const std::string& name_arg,
                             const std::string& key_arg,
                             const ConfigSection* defaults)
  : name(name_arg), key(key_arg)
  , defaults_(defaults)
{
}

ConfigSection::ConfigSection(const ConfigSection& other,
                             const ConfigSection* defaults)
  : name(other.name), key(other.key)
  , defaults_(defaults)
  , options_(other.options_)
{
}

void
ConfigSection::clear()
{
  options_.clear();
}

void
ConfigSection::update(const ConfigSection& other)
{
#ifndef NDEBUG
  auto old_defaults = defaults_;
#endif

  if (other.name != name || other.key != key)
  {
    ostringstream buffer;
    buffer << "Trying to update section " << name << ":" << key
           << " using section " << other.name << ":" << other.key;
    throw bad_section(buffer.str());
  }

  for (auto& option: other.options_)
    options_[option.first] = option.second;

  assert(old_defaults == defaults_);
}

/** @internal
 *
 * The parser is a simple hand-written scanner with three states:
 * `NORMAL`, `EAT_ONE`, and `IDENT`.
 *
 * <table>
 *  <tr><th></th><th>Input</th><th>Next State</th><th>Action</th></tr>
 *  <tr><th>NORMAL</th><td>'\'</td><td>EAT_ONE</td><td></td></tr>
 *  <tr><th>NORMAL</th><td>'{'</td><td>IDENT</td><td>CLEAR ident</td></tr>
 *  <tr><th>NORMAL</th><td>*</td><td>NORMAL</td><td></td>EMIT INPUT</tr>
 *  <tr><th>EAT_ONE</th><td>*</td><td>NORMAL</td><td>EMIT INOUT</td></tr>
 *  <tr><th>IDENT</th><td>'}'</td><td>NORMAL</td><td>EMIT LOOKUP(ident)</td></tr>
 *  <tr><th>IDENT</th><td>[A-Za-z0-9]</td><td>IDENT</td><td>APPEND TO ident</td></tr>
 *  <tr><th>IDENT</th><td>*</td><td>IDENT</td><td>ERROR</td></tr>
 * </table>
 */
std::string
ConfigSection::do_replace(const std::string& value) const
{
  std::string result;
  enum { NORMAL, EAT_ONE, IDENT } state = NORMAL;
  std::string ident;

  // State machine implementation that will scan the string one
  // character at a time and store the result of substituting variable
  // interpolations in the result variable.
  auto process = [&result, &state, &ident, this](char ch){
    switch (state) {
      case EAT_ONE:  // Unconditionally append the character
        result.push_back(ch);
        break;

      case IDENT:  // Reading an variable interpolation
        if (ch == '}') {
          // Found the end. The identifier is in the 'ident' variable
          result.append(get(ident));
          state = NORMAL;
        } else if (isident(ch)) {
          // One more character of the variable name
          ident.push_back(ch);
        } else {
          // Something that cannot be part of an identifier. Save it
          // away to give a good error message.
          ident.push_back(ch);
          ostringstream buffer;
          buffer << "Only alphanumeric characters in variable names allowed. "
                 << "Saw '" << ident << "'";
          throw syntax_error(buffer.str());
        }
        break;

      default:   // Normal state, just reading characters
        switch (ch) {
          case '\\':  // Next character is escaped
            state = EAT_ONE;
            break;

          case '{':  // Start a variable interpolation
            ident.clear();
            state = IDENT;
            break;

          default:    // Just copy the character to the output string
            result.push_back(ch);
            break;
        }
        break;
    }
  };

  for_each(value.begin(), value.end(), process);
  if (state == EAT_ONE)
    throw syntax_error("String ending with a backslash");
  if (state == IDENT)
    throw syntax_error("Unterminated variable interpolation");
  return result;
}

std::string
ConfigSection::get(const std::string& option) const
{
  check_option(option);
  OptionMap::const_iterator it = options_.find(lower(option));
  if (it != options_.end())
    return do_replace(it->second);
  if (defaults_)
    return defaults_->get(option);
  throw bad_option("Value for '" + option + "' not found");
}

bool
ConfigSection::has(const std::string& option) const
{
  check_option(option);
  if (options_.find(lower(option)) != options_.end())
    return true;
  if (defaults_)
    return defaults_->has(option);
  return false;
}

void
ConfigSection::set(const std::string& option, const std::string& value)
{
  check_option(option);
  options_[lower(option)] = value;
}

void
ConfigSection::add(const std::string& option, const std::string& value)
{
  auto ret = options_.emplace(OptionMap::value_type(lower(option), value));
  if (!ret.second)
    throw bad_option("Option '" + option + "' already defined");
}

Config::Config(unsigned int flags)
  : defaults_("default", "", NULL)
  , flags_(flags)
{
}

void
Config::copy_guts(const Config& source)
{
  reserved_ = source.reserved_;
  flags_ = source.flags_;
}

bool
Config::has(const std::string& section, const std::string& key) const
{
  auto it = sections_.find(make_pair(section, key));
  return (it != sections_.end());
}

Config::ConstSectionList
Config::get(const std::string& section) const
{
  auto rng = find_range_first(sections_, section);
  if (distance(rng.first, rng.second) == 0)
    throw bad_section("Section name '" + section + "' does not exist");
  ConstSectionList result;
  for (auto&& iter = rng.first ; iter != rng.second ; ++iter)
    result.push_back(&iter->second);
  return result;
}

Config::SectionList
Config::get(const std::string& section)
{
  auto rng = find_range_first(sections_, section);
  if (distance(rng.first, rng.second) == 0)
    throw bad_section("Section name '" + section + "' does not exist");
  SectionList result;
  for (auto&& iter = rng.first ; iter != rng.second ; ++iter)
    result.push_back(&iter->second);
  return result;
}

ConfigSection&
Config::get(const std::string& section, const std::string& key)
{
  // Check if we allow keys and throw an error if keys are not
  // allowed.
  if (!(flags_ & allow_keys))
    throw bad_section("Key '" + key + "' used but keys are not allowed");

  SectionMap::iterator sec = sections_.find(make_pair(section, key));
  if (sec == sections_.end())
    throw bad_section("Section '" + section + "' with key '" + key
                      + "' does not exist");
  return sec->second;
}

const ConfigSection&
Config::get(const std::string& section, const std::string& key) const
{
  return const_cast<Config*>(this)->get(section, key);
}

std::string
Config::get_default(const std::string& option) const
{
  return defaults_.get(option);
}

bool
Config::has_default(const std::string& option) const
{
  return defaults_.has(option);
}

void
Config::set_default(const std::string& option, const std::string& value)
{
  defaults_.set(option, value);
}

bool
Config::is_reserved(const std::string& word) const
{
  auto match = [this, &word](const std::string& pattern) {
    return (fnmatch(pattern.c_str(), word.c_str(), 0) == 0);
  };

  auto it = find_if(reserved_.begin(), reserved_.end(), match);
  return (it != reserved_.end());
}

ConfigSection&
Config::add(const std::string& section, const std::string& key)
{
  if (is_reserved(section))
    throw syntax_error("Section name '" + section + "' is reserved");

  ConfigSection cnfsec(section, key, &defaults_);
  auto result = sections_.emplace(make_pair(section, key), std::move(cnfsec));
  if (!result.second)
  {
    ostringstream buffer;
    if (key.empty())
        buffer << "Section '" << section << "' given more than once. "
               << "Please use keys to give multiple sections. "
               << "For example '" << section << ":one' and '"
               << section << ":two' to give two sections for plugin '"
               << section << "'";
    else
      buffer << "Section '" << section << ":" << key << "' already exists";
    throw bad_section(buffer.str());
  }

  // Return reference to the newly inserted section.
  return result.first->second;
}

void
Config::read(const Path& path)
{
  if (path.is_directory())
    read(path, Config::DEFAULT_PATTERN);
  else if (path.is_regular())
  {
    Config new_config;
    new_config.copy_guts(*this);
    new_config.do_read_file(path);
    update(new_config);
  }
  else
  {
    ostringstream buffer;
    buffer << "Path '" << path << "' ";
    if (path.type() == Path::FileType::FILE_NOT_FOUND)
      buffer << "does not exist";
    else
      buffer << "is not a directory or a file";
    throw std::runtime_error(buffer.str());
  }
}

void
Config::read(const Path& path, const std::string& pattern)
{
  Directory dir(path);
  Config new_config;
  new_config.copy_guts(*this);
  for (auto&& iter = dir.glob(pattern) ; iter != dir.end() ; ++iter)
  {
    Path entry(*iter);
    if (entry.is_regular())
      new_config.do_read_file(entry);
  }
  update(new_config);
}

void Config::read(std::istream& input)
{
  do_read_stream(input);
}


void Config::do_read_file(const Path& path)
{
  std::ifstream ifs(path.c_str(), std::ifstream::in);
  if (ifs.fail())
  {
    ostringstream buffer;
    buffer << "Unable to file " << path << " for reading";
    throw std::runtime_error(buffer.str());
  }
  do_read_stream(ifs);
}

void Config::do_read_stream(std::istream& input)
{
  ConfigSection *current = NULL;
  std::string line;
  while (getline(input, line))
  {
    strip(line);

    // Skip empty lines and comment lines.
    if (line.size() == 0 || line[0] == '#' || line[0] == ';')
      continue;

    // Check for section start and parse it if so.
    if (line[0] == '[')
    {
      // Check that it is only allowed characters
      if (line.back() != ']')
      {
        std::string message("Malformed section header: '" + line + "'");
        throw syntax_error(message);
      }

      // Remove leading and trailing brackets
      line.erase(0, 1);
      line.erase(line.size() - 1);

      // Extract the key, if configured to allow keys. Otherwise, the
      // key will be the empty string and the section name is all
      // within the brackets.
      std::string section_name(line);
      std::string section_key;
      if (flags_ & allow_keys) {
        // Split line at first colon
        auto pos = line.find_last_of(':');
        if (pos != std::string::npos) {
          section_key = std::string(line, pos + 1);

          // Check that the section key is correct
          if (section_key.size() == 0 ||
              !std::all_of(section_key.begin(), section_key.end(), isident))
          {
            std::string message("Invalid section key '" + section_key + "'");
            throw syntax_error(message);
          }

          section_name.erase(pos);
        }
      }

      // Check that the section name consists of allowable characters only
      if (!std::all_of(section_name.begin(), section_name.end(), isident))
      {
        std::string message("Invalid section name '" + section_name + "'");
        if (!(flags_ & allow_keys) &&
            line.find_last_of(':') != std::string::npos)
        {
          message += " (keys not configured)";
        }
        throw syntax_error(message);
      }

      // Section names are always stored in lowercase and we do not
      // distinguish between sections in lower and upper case.
      inplace_lower(section_name);
      if (section_name == "default")
        current = &defaults_;
      else
        current = &add(section_name, section_key);
    }
    else
    {
      if (current == NULL)
        throw syntax_error("Option line before start of section");
      // Got option line
      std::string::size_type pos = line.find_first_of(":=");
      if (pos == std::string::npos)
        throw syntax_error("Malformed option line: '" + line + "'");
      std::string option(line, 0, pos);
      strip(option);
      std::string value(line, pos + 1);
      strip(value);
      current->add(option, value);
    }
  }

  if (line.size() > 0)
    throw syntax_error("Unterminated last line");
}


bool
Config::empty() const
{
  return sections_.empty();
}

void
Config::clear()
{
  defaults_.clear();
  sections_.clear();
}

void
Config::update(const Config& other)
{
  // Pre-condition is that the default section pointers before the
  // update all refer to the default section for this configuration
  // instance.
  assert(std::all_of(sections_.cbegin(), sections_.cend(),
                     [this](const SectionMap::value_type& val) -> bool {
                       return val.second.assert_default(&defaults_);
                     }));

  for (const auto& section: other.sections_)
  {
    const SectionKey& key = section.first;
    SectionMap::iterator iter = sections_.find(key);
    if (iter == sections_.end())
      sections_.emplace(key, ConfigSection(section.second, &defaults_));
    else
      iter->second.update(section.second);
  }

  defaults_.update(other.defaults_);

  // Post-condition is that the default section pointers after the
  // update all refer to the default section for this configuration
  // instance.
  assert(std::all_of(sections_.cbegin(), sections_.cend(),
                     [this](const SectionMap::value_type& val) -> bool {
                       return val.second.assert_default(&defaults_);
                     }));
}

Config::ConstSectionList
Config::sections() const
{
  decltype(sections()) result;
  for (auto& section: sections_)
    result.push_back(&section.second);
  return result;
}
