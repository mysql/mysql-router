/*
  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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


#include "cluster_metadata.h"
#include "common.h"
#include "mysqlrouter/utils.h"
#include "mysqlrouter/utils_sqlstring.h"
#include <memory>
#include <cassert>
#include <string.h>
#ifdef _WIN32
#include <string.h>
#include <io.h>
#define strtok_r strtok_s
#define strcasecmp _stricmp
#else
#include <termios.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#ifndef __APPLE__
#include <ifaddrs.h>
#include <net/if.h>
#endif
#endif

// Semantic version number that this Router version supports
static const int kClusterRequiredMetadataMajorVersion = 1;
static const int kClusterRequiredMetadataMinorVersion = 0;
static const int kClusterRequiredMetadataPatchVersion = 0;

using mysqlrouter::strtoi_checked;
using mysqlrouter::sqlstring;
using mysqlrouter::MySQLSession;
using mysqlrouter::MySQLInnoDBClusterMetadata;
using mysqlrouter::HostnameOperations;

std::string HostnameOperations::get_my_hostname() {
  char buf[1024] = {0};
#if defined(_WIN32) || defined(__APPLE__) || defined(__FreeBSD__)
  if (gethostname(buf, sizeof(buf)) < 0) {
    // log_error("Could not get hostname: %s", mysql_harness::get_message_error(msg);
    throw std::runtime_error("Could not get local hostname");
  }
#else
  struct ifaddrs *ifa = nullptr, *ifap;
  int ret = -1, family;
  socklen_t addrlen;

  std::shared_ptr<ifaddrs> ifa_deleter(nullptr, [&](void*){if (ifa) freeifaddrs(ifa);});
  if ((ret = getifaddrs(&ifa)) != 0 || !ifa) {
    throw std::runtime_error("Could not get local host address: " + mysql_harness::get_strerror(errno)
                             + " (ret: " + std::to_string(ret)
                             + ", errno: " + std::to_string(errno) + ")");
  }
  for (ifap = ifa; ifap != NULL; ifap = ifap->ifa_next) {
    if ((ifap->ifa_addr == NULL) || (ifap->ifa_flags & IFF_LOOPBACK) || (!(ifap->ifa_flags & IFF_UP)))
      continue;
    family = ifap->ifa_addr->sa_family;
    if (family != AF_INET && family != AF_INET6)
      continue;
    if (family == AF_INET6) {
      struct sockaddr_in6 *sin6;

      sin6 = (struct sockaddr_in6 *)ifap->ifa_addr;
      if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) || IN6_IS_ADDR_MC_LINKLOCAL(&sin6->sin6_addr))
        continue;
    }
    addrlen = static_cast<socklen_t>((family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
    ret = getnameinfo(ifap->ifa_addr, addrlen, buf,
        static_cast<socklen_t>(sizeof(buf)), NULL, 0, NI_NAMEREQD);
  }
  if (ret != EAI_NONAME && ret != 0) {
    throw std::runtime_error("Could not get local host address: " + std::string(gai_strerror(ret))
                             + " (ret: " + std::to_string(ret)
                             + ", errno: " + std::to_string(errno) + ")");
  }
#endif
  return buf;
}

HostnameOperations *HostnameOperations::instance() {
  static HostnameOperations instance_;
  return &instance_;
}


static bool version_matches(const std::tuple<int,int,int> &required,
                            const std::tuple<int,int,int> &available) {
  // incompatible metadata
  if (std::get<0>(available) != std::get<0>(required) ||
    // metadata missing stuff we need
    (std::get<1>(available) < std::get<1>(required)) ||
    // metadata missing bugfixes we're expecting
    (std::get<1>(available) == std::get<1>(required) &&
      std::get<2>(available) < std::get<2>(required))) {
    return false;
  }
  return true;
}


static bool check_version(MySQLSession *mysql, std::tuple<int,int,int> &version) {
  std::unique_ptr<MySQLSession::ResultRow> result(mysql->query_one("SELECT * FROM mysql_innodb_cluster_metadata.schema_version"));
  if (!result) {
    throw std::runtime_error("Invalid MySQL InnoDB cluster metadata");
  }

  int major, minor, patch;

  if (result->size() == 2) {
    // Initially shell used to create version number with 2 digits only (1.0)
    // It has since moved to 3 digit numbers. We normalize it to 1.0.0 here for
    // simplicity and backwards compatibility.
    major = 1;
    minor = 0;
    patch = 0;
  } else {
    major = strtoi_checked((*result)[0]);
    minor = strtoi_checked((*result)[1]);
    patch = strtoi_checked((*result)[2]);
  }
  version = std::make_tuple(major, minor, patch);

  if (!version_matches(std::make_tuple(kClusterRequiredMetadataMajorVersion,
                                      kClusterRequiredMetadataMinorVersion,
                                      kClusterRequiredMetadataPatchVersion),
                      version)) {
    // log_error("Metadata schema version is %d.%d.%d, while we require at least %d.%d.%d",
    //           major, minor, patch,
    //           kClusterRequiredMetadataMajorVersion,
    //           kClusterRequiredMetadataMinorVersion,
    //           kClusterRequiredMetadataPatchVersion);
    return false;
  }
  //log_debug("Metadata schema version is %d.%d.%d, required is at least %d.%d.%d",
  //           major, minor, patch,
  //           kClusterRequiredMetadataMajorVersion,
  //           kClusterRequiredMetadataMinorVersion,
  //           kClusterRequiredMetadataPatchVersion);
  return true;
}

static bool check_group_replication_online(MySQLSession *mysql) {
  std::string q = "SELECT member_state"
                  " FROM performance_schema.replication_group_members"
                  " WHERE member_id = @@server_uuid";
  try {
    std::unique_ptr<MySQLSession::ResultRow> result(mysql->query_one(q));
    if (result && (*result)[0]) {
      if (strcmp((*result)[0], "ONLINE") == 0)
        return true;
      // log_warning("Member state for current server is %s", (*result)[0]);
      return false;
    }
  } catch (std::exception &e) {
    //log_error("Error querying for group_replication state: %s", e.what());
    return false;
  }
  throw std::logic_error("No result returned for metadata query");
}

static bool check_group_has_quorum(MySQLSession *mysql) {
  std::string q = "SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, COUNT(*) as num_total"
                  " FROM performance_schema.replication_group_members";
  try {
    std::unique_ptr<MySQLSession::ResultRow> result(mysql->query_one(q));
    if (result) {
      assert(result->size() == 2);
      int online = strtoi_checked((*result)[0]);
      int all = strtoi_checked((*result)[1]);
      //log_info("%d members online out of %d", online, all);
      if (online >= all/2+1)
        return true;
      return false;
    }
  } catch (std::exception &e) {
    // log_error("Error querying for group_replication state: %s", e.what());
    return false;
  }
  throw std::logic_error("No result returned for metadata query");
}

static bool check_group_member_is_primary(MySQLSession *mysql, std::string &ret_primary) {
  std::string q = "SELECT @@group_replication_single_primary_mode=1 as single_primary_mode, "
                  "       (SELECT variable_value FROM performance_schema.global_status WHERE variable_name='group_replication_primary_member') as primary_member, "
                  "        @@server_uuid as my_uuid";
  try {
    std::unique_ptr<MySQLSession::ResultRow> result(mysql->query_one(q));
    if (result) {
      assert(result->size() == 3);
      int single_primary_mode = strtoi_checked((*result)[0]);
      if (!single_primary_mode || strcmp((*result)[1], (*result)[2]) == 0)
        return true;
      // log_info("Single Primary Mode = %s, Current member is %sprimary",
      //           single_primary_mode ? "ON" : "OFF",
      //           is_primary ? "" : "NOT ");
      ret_primary = (*result)[1];
      return false;
    }
  } catch (std::exception &e) {
    // log_error("Error querying for group_replication state: %s", e.what());
    return false;
  }
  throw std::logic_error("No result returned for metadata query");
}

static bool check_metadata_is_supported(MySQLSession *mysql,
      const std::tuple<int,int,int> &version) {
  // check if there's only 1 cluster and 1 replicaset and that this member
  // is in that replicaset
  std::string q = "SELECT "
                  " ((SELECT count(*) FROM mysql_innodb_cluster_metadata.clusters) <= 1"
                  "  AND (SELECT count(*) FROM mysql_innodb_cluster_metadata.replicasets) <= 1) as has_one_replicaset,"
                  " (SELECT attributes->>'$.group_replication_group_name' FROM mysql_innodb_cluster_metadata.replicasets)"
                  "  = @@group_replication_group_name as replicaset_is_ours";
  try {
    std::unique_ptr<MySQLSession::ResultRow> result(mysql->query_one(q));
    if (result) {
      assert(result->size() == 2);
      bool has_only_one_replicaset = strtoi_checked((*result)[0]) == 1;
      bool replicaset_is_ours = true;
      if (version_matches(std::make_tuple(1, 0, 1), version))
        replicaset_is_ours = strtoi_checked((*result)[1]) == 1;

      // log_info("Replicaset/Cluster is unique = %i, Replicaset is our own = %i",
      //           has_only_one_replicaset, replicaset_is_ours);
      return has_only_one_replicaset && replicaset_is_ours;
    }
  } catch (std::exception &e) {
    // log_error("Error querying for group_replication state: %s", e.what());
    return false;
  }
  throw std::logic_error("No result returned for metadata query");
}

void mysqlrouter::check_innodb_metadata_cluster_session(MySQLSession *mysql,
                                                        bool read_only_ok) {
  // check that the server has the metadata in the correct version
  // check that the server we're querying contains metadata for the group it's in
  //   (metadata server group must be same as managed group currently)
  // check that the server we're bootstrapping from has GR enabled
  // check that the server we're bootstrapping from has quorum
  // check that the server we're bootstrapping from is not super_read_only
  try {
    std::tuple<int,int,int> mdversion;

    if (!check_version(mysql, mdversion)) {
      throw std::runtime_error("This version of MySQL Router is not compatible with the provided MySQL InnoDB cluster metadata.");
    }

    if (!check_metadata_is_supported(mysql, mdversion)) {
      throw std::runtime_error("The provided server contains an unsupported InnoDB cluster metadata.");
    }

    if (!check_group_replication_online(mysql)) {
      throw std::runtime_error("The provided server is currently not an ONLINE member of a InnoDB cluster.");
    }

    if (!check_group_has_quorum(mysql)) {
      throw std::runtime_error("The provided server is currently not in a InnoDB cluster group with quorum and thus may contain inaccurate or outdated data.");
    }

    std::string primary;
    if (!read_only_ok && !check_group_member_is_primary(mysql, primary)) {
      throw std::runtime_error("The provided server is not an updatable member of the cluster. Please try again with the Primary member of the replicaset" + (primary.empty() ? std::string(".") : " ("+primary+")."));
    }
  } catch (MySQLSession::Error &e) {
    if (e.code() == 1146) { // Table doesn't exist
      throw std::runtime_error("The provided server does not seem to contain metadata for a MySQL InnoDB cluster");
    }
    throw;
  }
}


void MySQLInnoDBClusterMetadata::check_router_id(uint32_t router_id) {
  // query metadata for this router_id
  sqlstring query("SELECT h.host_id, h.host_name"
                  " FROM mysql_innodb_cluster_metadata.routers r"
                  " JOIN mysql_innodb_cluster_metadata.hosts h"
                  "    ON r.host_id = h.host_id"
                  " WHERE r.router_id = ?");
  query << router_id << sqlstring::end;
  std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
  if (!row) {
    //log_warning("router_id %u not in metadata", router_id);
    throw std::runtime_error("router_id "+std::to_string(router_id)+" not found in metadata");
  }

  std::string hostname;
  try {
    hostname = hostname_operations_->get_my_hostname();
  }
  catch (const std::runtime_error& exc) {
    // If we fail to get the hostname we continue with an empty value.
    // Otherwise it causes the bootstrap fail on the machines with no DNS enabled.
    // Currently the hostname in the metadata is not being used anyway.
    std::cout << "WARNING: Failed calling get_my_hostname() with error: " << exc.what() << std::endl
              << "Continuing with an empty hostname" << std::endl;
    // TODO: also log when the logger is available here
  }

  if ((*row)[1] && strcasecmp((*row)[1], hostname.c_str()) == 0) {
    return;
  }
  //log_warning("router_id %u maps to an instance at hostname %s, while this hostname is %s",
  //                router_id, row[1], hostname.c_str());

  // if the host doesn't match, we force a new router_id to be generated
  throw std::runtime_error("router_id " + std::to_string(router_id)
      + " is associated with a different host ('"+(*row)[1]+"' vs '"+hostname+"')");
}

inline std::string str(const mysqlrouter::ConfigGenerator::Options::Endpoint &ep) {
  if (ep.port > 0)
    return std::to_string(ep.port);
  else if (!ep.socket.empty())
    return ep.socket;
  else
    return "null";
}

void MySQLInnoDBClusterMetadata::update_router_info(uint32_t router_id,
    const mysqlrouter::ConfigGenerator::Options &options) {
  sqlstring query("UPDATE mysql_innodb_cluster_metadata.routers"
                  " SET attributes = "
                  "   JSON_SET(JSON_SET(JSON_SET(JSON_SET(attributes,"
                  "    'RWEndpoint', ?),"
                  "    'ROEndpoint', ?),"
                  "    'RWXEndpoint', ?),"
                  "    'ROXEndpoint', ?)"
                  " WHERE router_id = ?");
  query << str(options.rw_endpoint);
  query << str(options.ro_endpoint);
  query << str(options.rw_x_endpoint);
  query << str(options.ro_x_endpoint);
  query << router_id;
  query << sqlstring::end;

  mysql_->execute(query);
}

uint32_t MySQLInnoDBClusterMetadata::register_router(
    const std::string &router_name, bool overwrite) {
  uint32_t host_id;
  std::string hostname;
  try {
    hostname = hostname_operations_->get_my_hostname();
  }
  catch (const std::runtime_error& exc) {
    // If we fail to get the hostname we continue with an empty value.
    // Otherwise it causes the bootstrap fail on the machines with no DNS enabled.
    // Currently the hostname in the metadata is not being used anyway.

    std::cout << "WARNING: Failed calling get_my_hostname() with error: " << exc.what() << std::endl
              << "Continuing with an empty hostname" << std::endl;
    // TODO: also log when the logger is available here
  }

  // check if the host already exists in the metadata schema and if so, get
  // our host_id.. if it doesn't, insert it and get the host_id
  sqlstring query("SELECT host_id, host_name, ip_address"
                   " FROM mysql_innodb_cluster_metadata.hosts"
                   " WHERE host_name = ?"
                   " LIMIT 1");
  query << hostname << sqlstring::end;
  {
    std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
    if (!row) {
      // host is not known to the metadata, register it
      query = sqlstring("INSERT INTO mysql_innodb_cluster_metadata.hosts"
                        "        (host_name, location, attributes)"
                        " VALUES (?, '', "
                        "         JSON_OBJECT('registeredFrom', 'mysql-router'))");
      query << hostname << sqlstring::end;
      mysql_->execute(query);
      host_id = static_cast<uint32_t>(mysql_->last_insert_id());
      // log_info("host_id for local host '%s' newly registered as '%u'",
      //        hostname.c_str(), host_id);
    } else {
      host_id = static_cast<uint32_t>(std::strtoul((*row)[0], NULL, 10));
      // log_info("host_id for local host '%s' already registered as '%u'",
      //        hostname.c_str(), host_id);
    }
  }
  // now insert the router and get the router id
  query = sqlstring("INSERT INTO mysql_innodb_cluster_metadata.routers"
                    "        (host_id, router_name)"
                    " VALUES (?, ?)");
  // log_info("Router instance '%s' registered with id %u", router_name.c_str(), router_id);
  query << host_id << router_name << sqlstring::end;
  try {
    mysql_->execute(query);
  } catch (MySQLSession::Error &e) {
    if (e.code() == 1062 && overwrite)  {
      //log_warning("Replacing instance %s (host_id %i) of router",
      //            router_name.c_str(), host_id);
      query = sqlstring("SELECT router_id FROM mysql_innodb_cluster_metadata.routers"
                        " WHERE host_id = ? AND router_name = ?");
      query << host_id << router_name << sqlstring::end;
      std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
      if (row) {
        return static_cast<uint32_t>(std::stoul((*row)[0]));
      }
    }
    throw;
  }
  return static_cast<uint32_t>(mysql_->last_insert_id());
}

