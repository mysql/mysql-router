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

#ifndef CONFIG_PARSER_INCLUDED
#define CONFIG_PARSER_INCLUDED

#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>

class ConfigSection;
class Path;

/**
 * Exception thrown for syntax errors.
 */

class syntax_error : public std::logic_error {
public:
  explicit syntax_error(const std::string& msg)
    : std::logic_error(msg)
  {
  }
};

/**
 * Exception thrown for section problems.
 */

class bad_section : public std::runtime_error
{
public:
  explicit bad_section(const std::string& msg)
    : std::runtime_error(msg)
  {
  }
};


/**
 * Exception thrown for option problems.
 */

class bad_option : public std::runtime_error
{
public:
  explicit bad_option(const std::string& msg)
    : std::runtime_error(msg)
  {
  }
};


/**
 * Configuration section.
 *
 * A named configuration file section with a zero or more
 * configuration file options.
 */

class ConfigSection {
public:
  typedef std::map<std::string, std::string> OptionMap;

  ConfigSection(const std::string& name,
                const std::string& key,
                const ConfigSection *defaults);

  ConfigSection(const ConfigSection&, const ConfigSection* defaults);
  ConfigSection& operator=(const ConfigSection&) = delete;

  /**
   * Clear the options in the section.
   *
   * This will remove options from the configuration section.
   */
  void clear();

  /**
   * Update section with contents of another section.
   *
   * The configuration section will be updated with the contents of
   * the other section. For any options that exist in the section, the
   * value will be overwritten by the values in the `other`
   * section. If the option do not exist, a new option will be created
   * and the value set to the value of the option in the `other`
   * section.
   *
   * @note The section name and key have to match for the update to be
   * done.
   *
   * @exception bad_section Thrown if the section name or section key
   * do not match.
   *
   * @param other Section to copy options and values from.
   */
  void update(const ConfigSection& other);

  std::string get(const std::string& option) const;
  void set(const std::string& option, const std::string& value);
  void add(const std::string& option, const std::string& value);
  bool has(const std::string& option) const;

#ifndef NDEBUG
  bool assert_default(const ConfigSection* def) const {
    return def == defaults_;
  }
#endif

public:
  const std::string name;
  const std::string key;

private:
  std::string do_replace(const std::string& value) const;

  const ConfigSection* defaults_;
  OptionMap options_;
};


/**
 * Configuration.
 *
 * A configuration consisting of named configuration sections.
 *
 * There are three different constructors that are available with
 * different kind of parameters.
 */

class Config {
public:
  typedef std::pair<std::string, std::string> SectionKey;
  typedef ConfigSection::OptionMap OptionMap;
  typedef std::list<ConfigSection*> SectionList;
  typedef std::list<const ConfigSection*> ConstSectionList;

  /**@{*/
  /** Flags for construction of configurations. */

  static constexpr unsigned int allow_keys = 1U;

  /**@}*/

  /**
   * Default pattern to used to identify configuration files.
   */
  static constexpr const char* DEFAULT_PATTERN = "*.cfg";

  /**
   * Construct a configuration.
   *
   * Construct a configuration instace by reading a configuration file
   * and overriding the values read from a list of supplied
   * parameters.
   *
   * @param parameters Associative container with parameters.
   * @param reserved Sequence container of reserved words.
   */

  explicit Config(unsigned int flags = 0U);

  /** @overload */
  template <class AssocT>
  explicit Config(const AssocT& parameters, unsigned int flags = 0U)
    : Config(flags)
  {
    for (auto item: parameters)
      defaults_.set(item.first, item.second);
  }

  /** @overload */
  template <class AssocT, class SeqT>
  explicit Config(const AssocT& parameters,
                  const SeqT& reserved,
                  unsigned int flags = 0U)
    : Config(parameters, flags)
  {
    for (auto word: reserved)
      reserved_.push_back(word);
  }

  virtual ~Config() = default;

  template <class SeqT>
  void set_reserved(const SeqT& reserved)
  {
    reserved_.assign(reserved.begin(), reserved.end());
  }

  /**
   * Read configuration file from file, directory, or input stream.
   *
   * If there are conflicting sections (several instance sections with
   * identical name and key) or conflicting options (several instances
   * of an option in the same section) in the input, an exception will
   * be thrown.
   *
   * If the input is a stream, the contents of the stream will be
   * added to the configuration, but if the input is either a file or
   * a directory, the exisisting contents of the configuration will be
   * removed.
   *
   * If a `pattern` is given, the path is assumed to be a directory
   * and all files matching the pattern will be read into the the
   * configuration object. The files together will be treated as if it
   * is a single file, that is, no conflicting option values or
   * sections are allowed and will raise an exception.
   *
   * @param input Input stream to read from.
   * @param path Path to directory or file to read from.
   * @param pattern Glob pattern for configuration files in the directory.
   *
   * @exception syntax_error Raised if there is a syntax error in the
   * configuration file and the configuration file have to be corrected.
   *
   * @exception bad_section Raised if there is a duplicate section
   * (section with the same name and key) in the input stream.
   *
   * @exception bad_option Raised if there is a duplicate definition
   * of an option (same option is given twice in a section).
   */
  void read(std::istream& input);

  /** @overload */
  void read(const Path& path);

  /** @overload */
  void read(const Path& path, const std::string& pattern);

  /**
   * Check if the configuration is empty.
   *
   * @return `true` if there are any sections in the configuration
   * (not counting the default section), `false` otherwise.
   */
  bool empty() const;

  /**
   * Clear the configuration.
   *
   * This will remove all configuration information from the
   * configuration, including the default section, but not the
   * reserved words nor the flags set.
   */
  void clear();

  /**
   * Update configuration using another configuration.
   *
   * This will incorporate all the sections and options from the
   * `other` configuration by adding sections that are missing and
   * overwriting option values for sections that exist in the current
   * configuration.
   *
   * @param other Configuration to read section, options, and values
   * from.
   */
  void update(const Config& other);

  /**
   * Get a list of sections having a name.
   *
   * There can be several sections under the same name, but they will
   * have different keys.
   *
   * @note The empty string is used to denote the keyless section.
   *
   * @param section Section name of sections to fetch.
   */
  ConstSectionList get(const std::string& section) const;

  /** @overload */
  SectionList get(const std::string& section);

  /**
   * Get a section by name and key.
   *
   * Get a section given a name and a key. Since there can be several
   * sections with the same name (but different keys) this will always
   * return a unique section.
   *
   * @note The empty string is used to denote the keyless section.
   *
   * @param section Name of section to fetch.
   * @param key Key for section to fetch.
   *
   * @return Reference to section instance.
   *
   * @exception bad_section Thrown if the section do not exist or if a
   * key were used but is not allowed.
   */
  ConfigSection& get(const std::string& section, const std::string& key);

  /** @overload */
  const ConfigSection& get(const std::string& section, const std::string& key) const;

  /**
   * Add a new section to the configuration.
   *
   * @param section Name of section to add.
   * @param key Optional key of section to add.
   *
   * @return Reference to newly constructed configuration section
   * instance.
   */
  ConfigSection& add(const std::string& section,
                     const std::string& key = std::string());

  bool has(const std::string& section,
           const std::string& key = std::string()) const;

  std::string get_default(const std::string& option) const;
  bool has_default(const std::string& option) const;
  void set_default(const std::string& option, const std::string& value);

  bool is_reserved(const std::string& word) const;

  std::list<Config::SectionKey> section_names() const
  {
    decltype(section_names()) result;
    for (auto& section: sections_)
      result.push_back(section.first);
    return result;
  }

  /**
   * Get a list of all sections in the configuration.
   */
  ConstSectionList sections() const;

protected:
  typedef std::map<SectionKey, ConfigSection> SectionMap;
  typedef std::vector<std::string> ReservedList;

  /**
   * Copy the guts of another configuration.
   *
   * This member functio is used to copy configuration state (the
   * "guts") but not the sections and options, including not copying
   * the default section.
   */
  void copy_guts(const Config& source);

  std::string replace_variables(const std::string& value) const;

  /**
   * Function to read single file.
   */
  virtual void do_read_file(const Path& path);

  /**
   * Function to read the configuration from a stream.
   *
   * @note This function is guaranteeed to be called for reading all
   * configurations so it can be overridden to handle post- or
   * pre-parsing actions.
   */
  virtual void do_read_stream(std::istream& input);

  SectionMap sections_;
  ReservedList reserved_;
  ConfigSection defaults_;
  unsigned int flags_;
};

#endif /* CONFIG_PARSER_INCLUDED */
