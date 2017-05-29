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

#ifndef ROUTER_CLUSTER_METADATA_INCLUDED
#define ROUTER_CLUSTER_METADATA_INCLUDED

#include "mysqlrouter/mysql_session.h"
#include "config_generator.h"

namespace mysqlrouter {

class HostnameOperationsBase {
public:
  virtual std::string get_my_hostname() = 0;
};

class HostnameOperations: public HostnameOperationsBase {
public:
  virtual std::string get_my_hostname() override;
  static HostnameOperations* instance();
protected:
  HostnameOperations() {}
};

class MySQLInnoDBClusterMetadata {
public:
  MySQLInnoDBClusterMetadata(MySQLSession *mysql,
                             HostnameOperationsBase *hostname_operations = HostnameOperations::instance())
  : mysql_(mysql), hostname_operations_(hostname_operations) {}

  void check_router_id(uint32_t router_id);
  uint32_t register_router(const std::string &router_name, bool overwrite);
  void update_router_info(uint32_t router_id,
                          const ConfigGenerator::Options &options);
private:
  MySQLSession *mysql_;
  HostnameOperationsBase *hostname_operations_;
};


void check_innodb_metadata_cluster_session(MySQLSession *mysql, bool read_only_ok);

}

#endif //ROUTER_CLUSTER_METADATA_INCLUDED
