if (mysqld.global.cluster_partition == undefined) {
  mysqld.global.cluster_partition = false;
}

({
  stmts: function (stmt) {
    if (stmt === "SELECT R.replicaset_name, I.mysql_server_uuid, I.role, I.weight, I.version_token, H.location, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = R.cluster_id JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id = I.replicaset_id JOIN mysql_innodb_cluster_metadata.hosts AS H ON I.host_id = H.host_id WHERE F.cluster_name = 'test';") {
      return {
        result : {
                columns : [
                    {
                        "name": "replicaset_name",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "mysql_server_uuid",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "role",
                        "type": "STRING"
                    },
                    {
                        "name": "weight",
                        "type": "FLOAT"
                    },
                    {
                        "name": "version_token",
                        "type": "LONG"
                    },
                    {
                        "name": "location",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "I.addresses->>'$.mysqlClassic'",
                        "type": "LONGBLOB"
                    },
                    {
                        "name": "I.addresses->>'$.mysqlX'",
                        "type": "LONGBLOB"
                    }
                ],
                rows : [
                    [
                        "default",
                        "199b2df7-4aaf-11e6-bb16-28b2bd168d07",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.PRIMARY_HOST,
                        "localhost:50000"
                    ],
                    [
                        "default",
                        "199bb88e-4aaf-11e6-babe-28b2bd168d07",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.SECONDARY_1_HOST,
                        "127.0.0.1:50010"
                    ],
                    [
                        "default",
                        "1999b9fb-4aaf-11e6-bb54-28b2bd168d07",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.SECONDARY_2_HOST,
                        "127.0.0.1:50020"
                    ],
                    [
                        "default",
                        "19ab72fc-4aaf-11e6-bb51-28b2bd168d07",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.SECONDARY_3_HOST,
                        "127.0.0.1:50030"
                    ],
                    [
                        "default",
                        "19b33846-4aaf-11e6-ba81-28b2bd168d07",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.SECONDARY_4_HOST,
                        "127.0.0.1:50040"
                    ]
                ]
            }
      };
    }
    else if (stmt === "show status like 'group_replication_primary_member'") {
      return {
          result : {
                columns : [
                    {
                        "name": "Variable_name",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "Value",
                        "type": "VAR_STRING"
                    }
                ],
                rows : [
                    [
                        "group_replication_primary_member",
                        "199b2df7-4aaf-11e6-bb16-28b2bd168d07"
                    ]
                ]
            }
      };
    }
    else if (stmt === "SELECT member_id, member_host, member_port, member_state, @@group_replication_single_primary_mode FROM performance_schema.replication_group_members WHERE channel_name = 'group_replication_applier'") {
      if (!mysqld.global.cluster_partition) {
        return {
            result : {
                columns : [
                    {
                        "name": "member_id",
                        "type": "STRING"
                    },
                    {
                        "name": "member_host",
                        "type": "STRING"
                    },
                    {
                        "name": "member_port",
                        "type": "LONG"
                    },
                    {
                        "name": "member_state",
                        "type": "STRING"
                    },
                    {
                        "name": "@@group_replication_single_primary_mode",
                        "type": "LONGLONG"
                    }
                ],
                rows : [
                    [
                        "199b2df7-4aaf-11e6-bb16-28b2bd168d07",
                        "areliga-Ubuntu16",
                        process.env.PRIMARY_PORT,
                        "ONLINE",
                        "1"
                    ],
                    [
                        "199bb88e-4aaf-11e6-babe-28b2bd168d07",
                        "areliga-Ubuntu16",
                        process.env.SECONDARY_1_PORT,
                        "ONLINE",
                        "1"
                    ],
                    [
                        "1999b9fb-4aaf-11e6-bb54-28b2bd168d07",
                        "areliga-Ubuntu16",
                        process.env.SECONDARY_2_PORT,
                        "ONLINE",
                        "1"
                    ],
                    [
                        "19ab72fc-4aaf-11e6-bb51-28b2bd168d07",
                        "areliga-Ubuntu16",
                        process.env.SECONDARY_3_PORT,
                        "ONLINE",
                        "1"
                    ],
                    [
                        "19b33846-4aaf-11e6-ba81-28b2bd168d07",
                        "areliga-Ubuntu16",
                        process.env.SECONDARY_4_PORT,
                        "ONLINE",
                        "1"
                    ]
                ]
            }
        };
      }
      else {
        return {
            result : {
                columns : [
                    {
                        "name": "member_id",
                        "type": "STRING"
                    },
                    {
                        "name": "member_host",
                        "type": "STRING"
                    },
                    {
                        "name": "member_port",
                        "type": "LONG"
                    },
                    {
                        "name": "member_state",
                        "type": "STRING"
                    },
                    {
                        "name": "@@group_replication_single_primary_mode",
                        "type": "LONGLONG"
                    }
                ],
                rows : [
                    [
                        "199b2df7-4aaf-11e6-bb16-28b2bd168d07",
                        "areliga-Ubuntu16",
                        process.env.PRIMARY_PORT,
                        "ONLINE",
                        "1"
                    ],
                    [
                        "199bb88e-4aaf-11e6-babe-28b2bd168d07",
                        "areliga-Ubuntu16",
                        process.env.SECONDARY_1_PORT,
                        "ONLINE",
                        "1"
                    ],
                    [
                        "1999b9fb-4aaf-11e6-bb54-28b2bd168d07",
                        "areliga-Ubuntu16",
                        process.env.SECONDARY_2_PORT,
                        "UNREACHABLE",
                        "1"
                    ],
                    [
                        "19ab72fc-4aaf-11e6-bb51-28b2bd168d07",
                        "areliga-Ubuntu16",
                        process.env.SECONDARY_3_PORT,
                        "UNREACHABLE",
                        "1"
                    ],
                    [
                        "19b33846-4aaf-11e6-ba81-28b2bd168d07",
                        "areliga-Ubuntu16",
                        process.env.SECONDARY_4_PORT,
                        "UNREACHABLE",
                        "1"
                    ]
                ]
            }
        };
      }
    }
    else if (stmt === "select @@port") {
      return {
        result: {
          columns: [
            {
              name: "@@port",
              type: "LONG"
            }
          ],
          rows: [
            [ mysqld.session.port ]
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
