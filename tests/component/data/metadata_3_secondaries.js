var stmts = {
  "SELECT R.replicaset_name, I.mysql_server_uuid, I.role, I.weight, I.version_token, H.location, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = R.cluster_id JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id = I.replicaset_id JOIN mysql_innodb_cluster_metadata.hosts AS H ON I.host_id = H.host_id WHERE F.cluster_name = 'test';": {
    "exec_time": 3.28679,
    "result": {
      "columns": [
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
      "rows": [
        [
          "default",
          "37dbb0e3-cfc0-11e7-8039-080027d01fcd",
          "HA",
          null,
          null,
          "",
          process.env.PRIMARY_HOST,
          "localhost:50000"
        ],
        [
          "default",
          "49cff431-cfc0-11e7-bb87-080027d01fcd",
          "HA",
          null,
          null,
          "",
          process.env.SECONDARY_1_HOST,
          "127.0.0.1:50010"
        ],
        [
          "default",
          "56d0f99d-cfc0-11e7-bb0a-080027d01fcd",
          "HA",
          null,
          null,
          "",
          process.env.SECONDARY_2_HOST,
          "127.0.0.1:50020"
        ],
        [
          "default",
          "6689460c-cfc0-11e7-907b-080027d01fcd",
          "HA",
          null,
          null,
          "",
          process.env.SECONDARY_3_HOST,
          "127.0.0.1:50030"
        ]
      ]
    }
  },
  "show status like 'group_replication_primary_member'": {
    "exec_time": 46.132637,
    "result": {
      "columns": [
        {
          "name": "Variable_name",
          "type": "VAR_STRING"
        },
        {
          "name": "Value",
          "type": "VAR_STRING"
        }
      ],
      "rows": [
        [
          "group_replication_primary_member",
          "37dbb0e3-cfc0-11e7-8039-080027d01fcd"
        ]
      ]
    }
  },
  "SELECT member_id, member_host, member_port, member_state, @@group_replication_single_primary_mode FROM performance_schema.replication_group_members WHERE channel_name = 'group_replication_applier'": {
    "exec_time": 1.493937,
    "result": {
      "columns": [
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
      "rows": [
        [
          "37dbb0e3-cfc0-11e7-8039-080027d01fcd",
          "areliga-Ubuntu16",
          process.env.PRIMARY_PORT,
          "ONLINE",
          "1"
        ],
        [
          "49cff431-cfc0-11e7-bb87-080027d01fcd",
          "areliga-Ubuntu16",
          process.env.SECONDARY_1_PORT,
          "ONLINE",
          "1"
        ],
        [
          "56d0f99d-cfc0-11e7-bb0a-080027d01fcd",
          "areliga-Ubuntu16",
          process.env.SECONDARY_2_PORT,
          "ONLINE",
          "1"
        ],
        [
          "6689460c-cfc0-11e7-907b-080027d01fcd",
          "areliga-Ubuntu16",
          process.env.SECONDARY_3_PORT,
          "ONLINE",
          "1"
        ]
      ]
    }
  },
  "select @@port": {
    "result": {
      "columns": [
        {
          "name": "@@port",
          "type": "STRING"
        }
      ],
      "rows": [
        [
          process.env.MY_PORT
        ]

      ]
    }
  }
};

function stmt_handler() {
  return new Duktape.Thread(function(stmt) {
    var yield = Duktape.Thread.yield;

    while (true) {
      if (stmts.hasOwnProperty(stmt)) {
        stmt = yield(stmts[stmt]);
      } else {
        stmt = yield({error: {code: 1234, message: "unexpected stmt: " + stmt}});
      }
    }
  });
}

({
    "stmts": stmt_handler()
})
