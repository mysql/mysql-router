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

static void inplace_lower(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

static std::string lower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);
  return str;
}

ConfigSection::ConfigSection(const std::string& name_arg,
                             const std::string& key_arg,
                             const ConfigSection* defaults)
  : name(name_arg), key(key_arg)
  , m_defaults(defaults)
{
}

ConfigSection::ConfigSection(const ConfigSection& other,
                             const ConfigSection* defaults)
  : name(other.name), key(other.key)
  , m_defaults(defaults)
  , m_options(other.m_options)
{
}

void
ConfigSection::clear()
{
  m_options.clear();
}

void
ConfigSection::update(const ConfigSection& other)
{
#ifndef NDEBUG
  auto old_defaults = m_defaults;
#endif

  if (other.name != name || other.key != key)
  {
    ostringstream buffer;
    buffer << "Trying to update section " << name << ":" << key
           << " using section " << other.name << ":" << other.key;
    throw bad_section(buffer.str());
  }

  for (auto& option: other.m_options)
    m_options[option.first] = option.second;

  assert(old_defaults == m_defaults);
}

std::string
ConfigSection::do_replace(const std::string& value) const
{
  std::string result = value;
  std::string::size_type pos = 0;
  while ((pos = result.find("%(", pos)) != std::string::npos)
  {
    std::string::size_type endpos = result.find(")s", pos + 2);

    // Raise an error if the parameter substitution was bad. Consider
    // if this can be relaxed later.
    if (endpos == std::string::npos)
    {
      std::string message("Malformed parameter substitution at '" +
                          result.substr(pos) + "'");
      throw parser_error(message);
    }
    std::string param = get(result.substr(pos + 2, endpos - pos - 2));
    result.replace(pos, endpos + 2 - pos, param);
    pos += param.length();
  }

  return result;
}

std::string
ConfigSection::get(const std::string& option) const
{
  OptionMap::const_iterator it = m_options.find(lower(option));
  if (it != m_options.end())
    return do_replace(it->second);
  if (m_defaults)
    return m_defaults->get(option);
  throw bad_option("Value for '" + option + "' not found");
}

bool
ConfigSection::has(const std::string& option) const
{
  if (m_options.find(lower(option)) != m_options.end())
    return true;
  if (m_defaults)
    return m_defaults->has(option);
  return false;
}

void
ConfigSection::set(const std::string& option, const std::string& value)
{
  m_options[lower(option)] = value;
}

void
ConfigSection::add(const std::string& option, const std::string& value)
{
  auto ret = m_options.emplace(OptionMap::value_type(lower(option), value));
  if (!ret.second)
    throw bad_option("Option '" + option + "' already defined");
}

Config::Config(unsigned int flags)
  : m_defaults("default", "", NULL)
  , m_flags(flags)
{
}

void
Config::copy_guts(const Config& source)
{
  m_reserved = source.m_reserved;
  m_flags = source.m_flags;
}

bool
Config::has(const std::string& section, const std::string& key) const
{
  auto it = m_sections.find(make_pair(section, key));
  return (it != m_sections.end());
}

Config::ConstSectionList
Config::get(const std::string& section) const
{
  auto rng = find_range_first(m_sections, section);
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
  auto rng = find_range_first(m_sections, section);
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
  if (!(m_flags & allow_keys))
    throw bad_section("Key '" + key + "' used but keys are not allowed");

  SectionMap::iterator sec = m_sections.find(make_pair(section, key));
  if (sec == m_sections.end())
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
  return m_defaults.get(option);
}

bool
Config::has_default(const std::string& option) const
{
  return m_defaults.has(option);
}

void
Config::set_default(const std::string& option, const std::string& value)
{
  m_defaults.set(option, value);
}

bool
Config::is_reserved(const std::string& word) const
{
  auto match = [this, &word](const std::string& pattern) {
    return (fnmatch(pattern.c_str(), word.c_str(), 0) == 0);
  };

  auto it = find_if(m_reserved.begin(), m_reserved.end(), match);
  return (it != m_reserved.end());
}

ConfigSection&
Config::add(const std::string& section, const std::string& key)
{
  if (is_reserved(section))
    throw parser_error("Section name '" + section + "' is reserved");

  ConfigSection cnfsec(section, key, &m_defaults);
  auto result = m_sections.emplace(make_pair(section, key), std::move(cnfsec));
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

static bool isident(const char ch)
{
  return isalnum(ch) || ch == '_';
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
  char buf[256];
  while (!input.getline(buf, sizeof(buf)).eof())
  {
    std::string line(buf);
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
        throw parser_error(message);
      }

      // Remove leading and trailing brackets
      line.erase(0, 1);
      line.erase(line.size() - 1);

      // Extract the key, if configured to allow keys. Otherwise, the
      // key will be the empty string and the section name is all
      // within the brackets.
      std::string section_name(line);
      std::string section_key;
      if (m_flags & allow_keys) {
        // Split line at first colon
        auto pos = line.find_last_of(':');
        if (pos != std::string::npos) {
          section_key = std::string(line, pos + 1);

          // Check that the section key is correct
          if (section_key.size() == 0 ||
              !std::all_of(section_key.begin(), section_key.end(), isident))
          {
            std::string message("Invalid section key '" + section_key + "'");
            throw parser_error(message);
          }

          section_name.erase(pos);
        }
      }

      // Check that the section name consists of allowable characters only
      if (!std::all_of(section_name.begin(), section_name.end(), isident))
      {
        std::string message("Invalid section name '" + section_name + "'");
        if (!(m_flags & allow_keys) &&
            line.find_last_of(':') != std::string::npos)
        {
          message += " (keys not configured)";
        }
        throw parser_error(message);
      }

      // Section names are always stored in lowercase and we do not
      // distinguish between sections in lower and upper case.
      inplace_lower(section_name);
      if (section_name == "default")
        current = &m_defaults;
      else
        current = &add(section_name, section_key);
    }
    else
    {
      if (current == NULL)
        throw parser_error("Option line before start of section");
      // Got option line
      std::string::size_type pos = line.find_first_of(":=");
      if (pos == std::string::npos)
        throw parser_error("Malformed option line: '" + line + "'");
      std::string option(line, 0, pos);
      strip(option);
      std::string value(line, pos + 1);
      strip(value);
      current->add(option, value);
    }
  }

  if (input.gcount() > 0)
    throw parser_error("Unterminated last line");

}


bool
Config::empty() const
{
  return m_sections.empty();
}

void
Config::clear()
{
  m_defaults.clear();
  m_sections.clear();
}

void
Config::update(const Config& other)
{
  // Pre-condition is that the default section pointers before the
  // update all refer to the default section for this configuration
  // instance.
  assert(std::all_of(m_sections.cbegin(), m_sections.cend(),
                     [this](const SectionMap::value_type& val) -> bool {
                       return val.second.assert_default(&m_defaults);
                     }));

  for (const auto& section: other.m_sections)
  {
    const SectionKey& key = section.first;
    SectionMap::iterator iter = m_sections.find(key);
    if (iter == m_sections.end())
      m_sections.emplace(key, ConfigSection(section.second, &m_defaults));
    else
      iter->second.update(section.second);
  }

  m_defaults.update(other.m_defaults);

  // Post-condition is that the default section pointers after the
  // update all refer to the default section for this configuration
  // instance.
  assert(std::all_of(m_sections.cbegin(), m_sections.cend(),
                     [this](const SectionMap::value_type& val) -> bool {
                       return val.second.assert_default(&m_defaults);
                     }));
}

Config::ConstSectionList
Config::sections() const
{
  decltype(sections()) result;
  for (auto& section: m_sections)
    result.push_back(&section.second);
  return result;
}
