var defaults = {
  version_comment: "community",
  account_user: "root",
  metadata_version: [1, 0, 1],
  exec_time: 0.0,
  group_replication_single_primary_mode: 1,
  // array-of-array
  // - server-uuid
  // - hostname
  // - port
  // - state
  group_replication_membership: [],
  port: mysqld.session.port
};

function ensure_type(options, field, expected_type) {
  var current_type = typeof(options[field]);

  if (expected_type === "array") {
    tested_type = "object";
  } else {
    tested_type = expected_type;
  }

  if (current_type !== tested_type) throw "expected " + field + " to be a " + expected_type + ", got " + current_type;
  if (expected_type === "array") {
    if (!Array.isArray(options[field])) throw "expected " + field + " to be a " + expected_type + ", got " + current_type;
  }
}

exports.get = function get(k) {
  // let's hope the 2nd arg is an object
  var options = Object.assign(defaults, arguments.length === 1 ? {} : arguments[1]);

  ensure_type(options, "version_comment", "string");
  ensure_type(options, "account_user", "string");
  ensure_type(options, "metadata_version", "array");
  ensure_type(options, "exec_time", "number");
  ensure_type(options, "port", "number");

  var statements = {
    mysql_client_select_version_comment: {
      stmt: "select @@version_comment limit 1",
      exec_time: options["exec_time"],
      result: {
        columns: [
          {
            name: "@@version_comment",
            type: "STRING"
          }
        ],
        rows: [
          [ options["version_comment"] ]
        ]
      }
    },
    mysql_client_select_user: {
      stmt: "select USER()",
      result: {
        columns: [
          {
            name: "USER()",
            type: "STRING"
          }
        ],
        rows: [
          [ options["account_user"] ]
        ]
      }
    },
    select_port: {
      stmt: "select @@port",
      result: {
        columns: [
          {
            name: "@@port",
            type: "LONG"
          }
        ],
        rows: [
          [ options["port"] ]
        ]
      }
    },
    router_select_schema_version: {
      stmt: "SELECT * FROM mysql_innodb_cluster_metadata.schema_version",
      exec_time: options["exec_time"],
      result: {
        columns: [
          {
            type: "LONGLONG",
            name: "major"
          },
          {
            type: "LONGLONG",
            name: "minor"
          },
          {
            type: "LONGLONG",
            name: "patch"
          }
        ],
        rows: [
          options["metadata_version"]
        ]
      }
    },
    router_select_group_membership_with_primary_mode: {
      stmt: "SELECT member_id, member_host, member_port, member_state, @@group_replication_single_primary_mode FROM performance_schema.replication_group_members WHERE channel_name = 'group_replication_applier'",
      exec_time: options["exec_time"],
      result: {
        columns: [
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
        rows: options["group_replication_membership"].map(function(currentValue) {
          return currentValue.concat([ options["group_replication_single_primary_mode"] ]);
        }),
      }
    },
    router_select_group_replication_primary_member: {
      "stmt": "show status like 'group_replication_primary_member'",
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
              options["group_replication_primary_member"]
          ]
        ]
      }
    },
  };

  return statements[k];
}
