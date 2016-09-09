/*
  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "config_generator.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/my_aes.h"

#if defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#elif defined _MSC_VER
#pragma warning (push)
#pragma warning (disable : ) //TODO: add MSVC code for pedantic and shadow
#endif
#include "rapidjson/document.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning (pop)
#endif

#include <fstream>
#include <iostream>
#include <mysql.h>
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <cstring>

/**
 * Return a string representation of the input character string.
 *
 * @param input_str A character string.
 *
 * @return A string object encapsulation of the input character string. An empty
 *         string if input string is nullptr.
 */
static std::string get_string(const char *input_str) {
  if (input_str == nullptr) {
    return "";
  }
  return std::string(input_str);
}

static void extract_account_info(const std::string &data, const std::string &key,
    std::string &ret_username, std::string &ret_password) {
  std::string decrypted_data;
  decrypted_data.resize(data.length());
  int len;
  if ((len = myaes::my_aes_decrypt(reinterpret_cast<const unsigned char*>(data.data()),
                     static_cast<uint32_t>(data.length()),
                     reinterpret_cast<unsigned char*>(&decrypted_data[0]),
                     reinterpret_cast<const unsigned char*>(key.data()),
                     static_cast<uint32_t>(key.length()),
                     myaes::my_aes_128_ecb, NULL, false)) < 0)
    throw std::runtime_error("Error decrypting account information");
  decrypted_data.resize((size_t)len);

  rapidjson::Document document;
  if (document.Parse(decrypted_data.c_str()).HasParseError())
    throw std::runtime_error("Could not decode account information from metadata");
  if (!document.IsObject())
    throw std::runtime_error("Invalid account information in metadata");
  auto member_iter = document.FindMember("clusterReader");
  if (member_iter == document.MemberEnd())
    throw std::runtime_error("Invalid account information in metadata");
  const rapidjson::Value &member = document["clusterReader"];
  if (member.FindMember("username") == member.MemberEnd() ||
    member.FindMember("password") == member.MemberEnd())
    throw std::runtime_error("Invalid account information in metadata");
  ret_username = member["username"].GetString();
  ret_password = member["password"].GetString();
}

void ConfigGenerator::fetch_bootstrap_servers(
  const std::string &server_url,
  std::string &bootstrap_servers,
  std::string &username,
  std::string &password,
  std::string &metadata_cluster,
  std::string &metadata_replicaset,
  bool &multi_master) {
  // MySQL client connection to the bootstrap server.
  MYSQL *metadata_connection_;
  // Setup reconnection flag
  bool reconnect_ = false;
  // Setup connection timeout
  int connection_timeout_ = 1;
  unsigned int protocol = MYSQL_PROTOCOL_TCP;

  std::ostringstream query;

  // Extract connection information from the bootstrap server URL.
  std::string normalized_url(server_url);
  if (normalized_url.find("//") == std::string::npos) {
    normalized_url = "mysql://" + normalized_url;
  }
  mysqlrouter::URI u(normalized_url);

  // If the user name is mentioned as part of the URL throw an error.
  if (!u.username.empty()) {
    throw std::runtime_error("Username must be ommited in the URL for metadata server");
  }

  // setup localhost address.
  u.host = (u.host == "localhost" ? "127.0.0.1" : u.host);

  // Query the name of the replicaset, the servers in the replicaset and the
  // router credentials using the URL of a server in the replicaset.
  query << "SELECT "
    "F.cluster_name, "
    "R.replicaset_name, "
    "R.topology_type, "
    "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')), "
    "F.mysql_user_accounts "
    "FROM "
    "mysql_innodb_cluster_metadata.clusters AS F, "
    "mysql_innodb_cluster_metadata.instances AS I, "
    "mysql_innodb_cluster_metadata.replicasets AS R "
    "WHERE "
    "R.replicaset_id = "
    "(SELECT replicaset_id FROM mysql_innodb_cluster_metadata.instances WHERE "
      "mysql_server_uuid = @@server_uuid)"
    "AND "
    "I.replicaset_id = R.replicaset_id "
    "AND "
    "R.cluster_id = F.cluster_id";

  // we need to prompt for the password
  std::string metadata_cache_password = prompt_password(
    "Please enter the administrative MASTER key for the MySQL InnoDB cluster");

  //Initialize the connection to the bootstrap server
  metadata_connection_ = mysql_init(nullptr);

  mysql_options(metadata_connection_, MYSQL_OPT_CONNECT_TIMEOUT,
                &connection_timeout_);
  mysql_options(metadata_connection_, MYSQL_OPT_PROTOCOL,
                reinterpret_cast<char *> (&protocol));
  mysql_options(metadata_connection_, MYSQL_OPT_RECONNECT, &reconnect_);

  const unsigned long client_flags = (
    CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 |
    CLIENT_MULTI_RESULTS
    );

  if (mysql_real_connect(metadata_connection_,
                         u.host.c_str(),
                         "mysql_innodb_cluster_admin",
                         metadata_cache_password.c_str(),
                         nullptr,
                         u.port,
                         nullptr,
                         client_flags)) {
    int status = mysql_query(metadata_connection_, query.str().c_str());

    if (status) {
      std::ostringstream err;
      err << "Query failed: " << query.str() << "\n\tWith error: " <<
        mysql_error(metadata_connection_);
      //log_warning("%s", err.str().c_str());
      throw std::runtime_error(std::string("Error querying metadata: ") + mysql_error(metadata_connection_));
    }

    MYSQL_RES *result = mysql_store_result(metadata_connection_);

    if (!result) {
      std::ostringstream err;
      err << "Failed fetching results: " << query.str() << "\n\tWith error: " <<
        mysql_error(metadata_connection_);
      throw std::runtime_error(err.str());
    }

    unsigned int num_fields = mysql_num_fields(result);

    if (num_fields != 5) {
       std::ostringstream err;
       err << "unexpected number of fields " << num_fields << " in result set";
       throw std::runtime_error(err.str());
    }

    std::string encrypted_account_data;
    MYSQL_ROW row = nullptr;
    metadata_cluster = "";
    metadata_replicaset = "";
    bootstrap_servers = "";
    username = "";
    password = "";

    while ((row = mysql_fetch_row(result)) != nullptr) {
      unsigned long *lengths = mysql_fetch_lengths(result);
      if (metadata_cluster == "") {
        metadata_cluster = get_string(row[0]);
      }
      if (metadata_replicaset == "") {
        metadata_replicaset = get_string(row[1]);
      }
      if (bootstrap_servers != "")
        bootstrap_servers += ",";
      if (row[2] && strcmp(row[2], "mm") == 0)
        multi_master = true;
      bootstrap_servers += "mysql://"+get_string(row[3]);
      if (row[4])
        encrypted_account_data = std::string(row[4], lengths[4]);
    }

    if (encrypted_account_data.empty()) {
      throw std::runtime_error("Invalid metadata: no account data found");
    }

    extract_account_info(encrypted_account_data, metadata_cache_password,
        username, password);
  } else {
    std::ostringstream err;
    err << "Unable to connect to the metadata server " << u.host
        << ":" <<  std::to_string(u.port) << ": " << mysql_error(metadata_connection_);
    throw std::runtime_error(err.str());
  }
}

std::string g_program_name;

static std::string find_my_base_dir() {
  if (g_program_name.find('/') != std::string::npos) {
    char *tmp = realpath(g_program_name.substr(0, g_program_name.rfind('/')).c_str(), NULL);
    std::string path(tmp);
    free(tmp);
    if (path.find('/') != std::string::npos)
      return path.substr(0, path.rfind('/'));
    return path;
  } else {
    std::string path(std::getenv("PATH"));
    char *last = NULL;
    char *p = strtok_r(&path[0], ":", &last);
    while (p) {
      if (*p && p[strlen(p)-1] == '/')
        p[strlen(p)-1] = 0;
      std::string tmp(std::string(p)+"/"+g_program_name);
      if (access(tmp.c_str(), R_OK|X_OK) == 0) {
        path = p;
        if (path.find('/') != std::string::npos)
          return path.substr(0, path.rfind('/'));
        return path;
      }
      p = strtok_r(NULL, ":", &last);
    }
    throw std::logic_error("Could not find own installation directory");
  }
}


void ConfigGenerator::create_config(
  const std::string &config_file_path,
  const std::string &default_log_path,
  const std::string &bootstrap_server_addresses,
  const std::string &metadata_cluster,
  const std::string &metadata_replicaset,
  const std::string &username,
  const std::string &password,
  bool multi_master) {
  std::ofstream cfp;
  std::string plugindir;
  int rw_port = 6446;
  int ro_port = 6447;

  std::string basedir(find_my_base_dir());

  if (access((basedir+"/lib64/mysqlrouter").c_str(), X_OK) == 0) {
    plugindir = basedir+"/lib64/mysqlrouter";
  } else {
    plugindir = basedir+"/lib/mysqlrouter";
  }
  cfp.open(config_file_path);
  if (cfp.fail()) {
    throw std::runtime_error("Could not open "+config_file_path+" for writing: "+strerror(errno));
  }
  cfp << "[DEFAULT]\n"
      << "plugin_folder=" << plugindir << "\n"
      << "# logging_folder=" << default_log_path << "\n"
      << "\n"
      << "[logger]\n"
      << "level = INFO\n"
      << "\n"
      << "[metadata_cache]\n"
      << "bootstrap_server_addresses=" << bootstrap_server_addresses << "\n"
      << "user=" << username << "\n"
      << "password=" << password << "\n"
      << "metadata_cluster=" << metadata_cluster << "\n"
      << "ttl=300" << "\n"
      << "metadata_replicaset=" << metadata_replicaset << "\n"
      << "\n";
      if (!multi_master) {
        cfp
          << "[routing:" << metadata_replicaset << "_rw]\n"
          << "bind_port=" << rw_port << "\n"
          << "destinations=metadata-cache:///"
              << metadata_replicaset << "?role=PRIMARY\n"
          << "mode=read-write\n"
          << "\n"
          << "[routing:" << metadata_replicaset << "_ro]\n"
          << "bind_port=" << ro_port << "\n"
          << "destinations=metadata-cache:///"
              << metadata_replicaset << "?role=SECONDARY\n"
          << "mode=read-only\n";
      } else {
        cfp
          << "[routing:" << metadata_replicaset << "_rw]\n"
          << "bind_port=" << rw_port << "\n"
          << "destinations=metadata-cache:///"
              << metadata_replicaset << "?role=PRIMARY\n"
          << "mode=read-write\n"
          << "\n";
      }
  cfp.close();

  std::cout
    << "MySQL Router has now been configured for the InnoDB cluster '" << metadata_cluster << "'" << (multi_master ? " (multi-master)" : "") << ".\n"
    << "\n"
    << "The following connection information can be used to connect to the cluster.\n"
    << "\n"
    << "Classic MySQL protocol connections to cluster '" << metadata_cluster << "':\n"
    << "- Read/Write Connections: localhost:" << rw_port << "\n";
  if (!multi_master)
    std::cout
      << "- Read/Only Connections: localhost:" << ro_port << "\n";
}

/**
 * Prompt for the password that will be used for accessing the metadata
 * in the metadata servers.
 *
 * @param prompt The password prompt.
 *
 * @return a string representing the password.
 */
#ifndef _WIN32
const std::string ConfigGenerator::prompt_password(const std::string &prompt) {
  struct termios console;
  tcgetattr(STDIN_FILENO, &console);

  std::cout << prompt << ": ";

  // prevent showing input
  console.c_lflag &= ~(uint)ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &console);

  std::string result;
  std::getline(std::cin, result);

  // reset
  console.c_lflag |= ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &console);

  std::cout << std::endl;
  return result;
}
#else
const std::string ConfigGenerator::prompt_password(const std::string &prompt) {

  std::cout << prompt << ": ";

  // prevent showing input
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  GetConsoleMode(hStdin, &mode);
  mode &= ~ENABLE_ECHO_INPUT;
  SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));

  std::string result;
  std::getline(std::cin, result);

  // reset
  SetConsoleMode(hStdin, mode);

  std::cout << std::endl;
  return result;
}
#endif
