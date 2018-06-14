if(mysqld.global.md_query_count == undefined){
    mysqld.global.md_query_count = 0;
}
({
    stmts: function (stmt) {
      if (stmt === "SELECT R.replicaset_name, I.mysql_server_uuid, I.role, I.weight, I.version_token, H.location, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = R.cluster_id JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id = I.replicaset_id JOIN mysql_innodb_cluster_metadata.hosts AS H ON I.host_id = H.host_id WHERE F.cluster_name = 'test';") {
        mysqld.global.md_query_count++;
        return {
          result: {
            columns: [
                {
                    name: "replicaset_name",
                    type: "VAR_STRING"
                },
                {
                    name: "mysql_server_uuid",
                    type: "VAR_STRING"
                },
                {
                    name: "role",
                    type: "STRING"
                },
                {
                    name: "weight",
                    type: "FLOAT"
                },
                {
                    name: "version_token",
                    type: "LONG"
                },
                {
                    name: "location",
                    type: "VAR_STRING"
                },
                {
                    name: "I.addresses->>'$.mysqlClassic'",
                    type: "LONGBLOB"
                },
                {
                    name: "I.addresses->>'$.mysqlX'",
                    type: "LONGBLOB"
                }

            ],
            rows: [
                    [
                        "default",
                        "8502d47b-e0f2-11e7-8530-080027c84a43",
                        "HA",
                        null,
                        null,
                        "",
                        "localhost:" + mysqld.session.port,
                        "localhost:33100"
                    ]
            ]
          },
        };
      }
      else if (stmt === "show status like 'group_replication_primary_member'") {
        return {
          result: {
            columns: [
                {
                    name: "Variable_name",
                    type: "VAR_STRING"
                },
                {
                    name: "Value",
                    type: "VAR_STRING"
                }

            ],
            rows: [
                [
                    "group_replication_primary_member",
                    "8502d47b-e0f2-11e7-8530-080027c84a43"
                ]
            ]
          },
        };
      }
      else if (stmt === "SELECT member_id, member_host, member_port, member_state, @@group_replication_single_primary_mode FROM performance_schema.replication_group_members WHERE channel_name = 'group_replication_applier'") {
        return {
          result: {
            columns: [
                {
                    name: "member_id",
                    type: "STRING"
                },
                {
                    name: "member_host",
                    type: "STRING"
                },
                {
                    name: "member_port",
                    type: "LONG"
                },
                {
                    name: "member_state",
                    type: "STRING"
                },
                {
                    name: "@@group_replication_single_primary_mode",
                    type: "LONGLONG"
                }
            ],
            rows: [
                [
                    "8502d47b-e0f2-11e7-8530-080027c84a43",
                    "areliga-VirtualBox",
                    "localhost:" + mysqld.session.port,
                    "ONLINE",
                    "1"
                ]
            ]
          },
        };
      }
      else if (stmt === "SELECT R.replicaset_name, I.mysql_server_uuid, I.role, I.weight, I.version_token, H.location, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = R.cluster_id JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id = I.replicaset_id JOIN mysql_innodb_cluster_metadata.hosts AS H ON I.host_id = H.host_id WHERE F.cluster_name = 'test';") {
        return {
          result: {
            columns: [
                {
                    name: "replicaset_name",
                    type: "VAR_STRING"
                },
                {
                    name: "mysql_server_uuid",
                    type: "VAR_STRING"
                },
                {
                    name: "role",
                    type: "STRING"
                },
                {
                    name: "weight",
                    type: "FLOAT"
                },
                {
                    name: "version_token",
                    type: "LONG"
                },
                {
                    name: "location",
                    type: "VAR_STRING"
                },
                {
                    name: "I.addresses->>'$.mysqlClassic'",
                    type: "LONGBLOB"
                },
                {
                    name: "I.addresses->>'$.mysqlX'",
                    type: "LONGBLOB"
                }
            ],
            "rows": [
                [
                    "default",
                    "8502d47b-e0f2-11e7-8530-080027c84a43",
                    "HA",
                    null,
                    null,
                    "",
                    "localhost:" + mysqld.session.port,
                    "localhost:33100"
                ]
            ]
          },
        };
      }
      else if (stmt === "show status like 'group_replication_primary_member'") {
        return {
          result: {
            columns: [
                {
                    name: "Variable_name",
                    type: "VAR_STRING"
                },
                {
                    name: "Value",
                    type: "VAR_STRING"
                }
            ],
            rows: [
                [
                    "group_replication_primary_member",
                    "8502d47b-e0f2-11e7-8530-080027c84a43"
                ]
            ]
          },
        };
      }
      else if (stmt === "SELECT member_id, member_host, member_port, member_state, @@group_replication_single_primary_mode FROM performance_schema.replication_group_members WHERE channel_name = 'group_replication_applier'") {
        return {
          result: {
            columns: [
                {
                    name: "member_id",
                    type: "STRING"
                },
                {
                    name: "member_host",
                    type: "STRING"
                },
                {
                    name: "member_port",
                    type: "LONG"
                },
                {
                    name: "member_state",
                    type: "STRING"
                },
                {
                    name: "@@group_replication_single_primary_mode",
                    type: "LONGLONG"
                }
            ],
            rows: [
                [
                    "8502d47b-e0f2-11e7-8530-080027c84a43",
                    "areliga-VirtualBox",
                    "localhost:" + mysqld.session.port,
                    "ONLINE",
                    "1"
                ]
            ]
          },
        };
      }
      else {
        return {
          error: {
            code: 1273,
            sql_state: "HY001",
            message: "Syntax Error at: " + stmt
          }
        };
      }
    }
})
