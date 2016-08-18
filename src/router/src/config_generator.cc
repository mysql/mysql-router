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

#include <fstream>
#include <iostream>
#include <mysql.h>
#include <sstream>
#include <termios.h>
#include <unistd.h>

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

void ConfigGenerator::fetch_bootstrap_servers(
  const std::string &server_url,
  std::string &bootstrap_servers,
  std::string &username,
  std::string &password,
  std::string &metadata_cluster,
  std::string &metadata_replicaset) {
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
    "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')), "
    "JSON_UNQUOTE(JSON_EXTRACT(F.mysql_user_accounts, '$.clusterReader.username')), "
    "JSON_UNQUOTE(JSON_EXTRACT(F.mysql_user_accounts, '$.clusterReader.password')) "
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
    "Please enter the MASTER password for the MySQL InnoDB cluster");

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
      err << "Query failed: " << query.str() << "With error: " <<
        mysql_error(metadata_connection_);
      throw std::runtime_error(err.str());
    }

    MYSQL_RES *result = mysql_store_result(metadata_connection_);

    if (!result) {
      std::ostringstream err;
      err << "Failed fetching results: " << query.str() << "With error: " <<
        mysql_error(metadata_connection_);
      throw std::runtime_error(err.str());
    }

    unsigned int num_fields = mysql_num_fields(result);

    if (num_fields != 5) {
       std::ostringstream err;
       err << "unexpected number of fields " << num_fields << " in result set";
       throw std::runtime_error(err.str());
    }

    MYSQL_ROW row = nullptr;
    metadata_cluster = "";
    metadata_replicaset = "";
    bootstrap_servers = "";
    username = "";
    password = "";

    while ((row = mysql_fetch_row(result)) != nullptr) {
      if (metadata_cluster == "") {
        metadata_cluster = get_string(row[0]);
      }
      if (metadata_replicaset == "") {
        metadata_replicaset = get_string(row[1]);
      }
      if (bootstrap_servers != "")
        bootstrap_servers += ",";
      bootstrap_servers += "mysql://"+get_string(row[2]);
      if (username == "")
        username = get_string(row[3]);
      if (password == "")
        password = get_string(row[4]);
    }
  } else {
    std::ostringstream err;
    err << "Unable to connect to the metadata server " << u.host
        << ":" <<  std::to_string(u.port) << ": " << mysql_error(metadata_connection_);
    throw std::runtime_error(err.str());
  }
}

void ConfigGenerator::create_config(
  const std::string &bootstrap_server_addresses,
  const std::string &metadata_cluster,
  const std::string &metadata_replicaset,
  const std::string &username,
  const std::string &password) {
  std::ofstream cfp;

  int rw_port = 3360;
  int ro_port = 3361;

  // FIXME
  char *basedir = realpath((origin_+"/..").c_str(), NULL);

  cfp.open("mysqlrouter_autogen.ini");
  cfp << "[DEFAULT]\n"
      << "plugin_folder=" << basedir << "/lib/mysqlrouter" << "\n"
      << "logging_folder=" << basedir << "/log\n"
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
      << "\n"
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
  cfp.close();
  free(basedir);
  std::cout << "Configuration file written to mysqlrouter_autogen.ini"
            << std::endl
            << std::endl;

  std::cout
    << "MySQL Router has now been configured for the InnoDB cluster '" << metadata_cluster << "'.\n"
    << "\n"
    << "The following connection information can be used to connect to the cluster.\n"
    << "\n"
    << "Classic MySQL protocol connections to cluster '" << metadata_cluster << "':\n"
    << "- Read/Write Connections: localhost:" << rw_port << "\n"
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
