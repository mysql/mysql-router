{
    "stmts": [
        {
            "stmt": "START TRANSACTION",
            "exec_time": 0.057513,
            "ok": {}
        },
        {
            "stmt": "SELECT h.host_id, h.host_name FROM mysql_innodb_cluster_metadata.routers r JOIN mysql_innodb_cluster_metadata.hosts h    ON r.host_id = h.host_id WHERE r.router_id = 8",
            "exec_time": 0.175663,
            "result": {
                "columns": [
                    {
                        "name": "host_id",
                        "type": "LONG"
                    },
                    {
                        "name": "host_name",
                        "type": "VAR_STRING"
                    }
                ],
                "rows": [
                    [
                        "8",
                        process.env.MYSQL_SERVER_MOCK_HOST_NAME
                    ]
                ]
            }
        },



        // delete all old accounts if necessarry (ConfigGenerator::delete_account_for_all_hosts())
        {
            "stmt.regex": "^SELECT COUNT... FROM mysql.user WHERE user = '.*'",
            "result": {
                "columns": [
                    {
                        "type": "LONGLONG",
                        "name": "COUNT..."
                    }
                ],
                "rows": [
                    [
                        "0" // to keep it simple, just tell Router there's no old accounts to erase
                    ]
                ]
            }
        },

        // create temp account to figure out the secure password (ConfigGenerator::generate_compliant_password())
        {
            "stmt.regex": "^CREATE USER mysql_router.*",
            "ok": {}
        },

        // now erase that temp account (ConfigGenerator::delete_account_for_all_hosts())
        {
            "stmt.regex": "^SELECT COUNT... FROM mysql.user WHERE user = '.*'",
            "result": {
                "columns": [
                    {
                        "type": "LONGLONG",
                        "name": "COUNT..."
                    }
                ],
                "rows": [
                    [
                        "1" // 1 = the temp account we just created
                    ]
                ]
            }
        },
        {
            "stmt.regex": "^SELECT CONCAT\\('DROP USER ', GROUP_CONCAT\\(QUOTE\\(user\\), '@', QUOTE\\(host\\)\\)\\) INTO @drop_user_sql FROM mysql.user WHERE user LIKE 'mysql_router.*'",
            "ok": {}
        },
        {
            "stmt.regex": "^PREPARE drop_user_stmt FROM @drop_user_sql",
            "ok": {}
        },
        {
            "stmt.regex": "^EXECUTE drop_user_stmt",
            "ok": {}
        },
        {
            "stmt.regex": "^DEALLOCATE PREPARE drop_user_stmt",
            "ok": {}
        },

        // finally, create the "real" account
        {   "COMMENT": "ConfigGenerator::create_account()",
            "stmt.regex": "^CREATE USER mysql_router.*",
            "ok": {}
        },
        {
            "stmt.regex": "^GRANT SELECT ON mysql_innodb_cluster_metadata.* TO mysql_router8_.*@'%'",
            "exec_time": 8.536869,
            "ok": {}
        },
        {
            "stmt.regex": "^GRANT SELECT ON performance_schema.replication_group_members TO mysql_router8_.*@'%'",
            "exec_time": 8.584342,
            "ok": {}
        },
        {
            "stmt.regex": "^GRANT SELECT ON performance_schema.replication_group_member_stats TO mysql_router8_.*@'%'",
            "exec_time": 6.240789,
            "ok": {}
        },



        {
            "stmt.regex": "^UPDATE mysql_innodb_cluster_metadata.routers SET attributes =    JSON_SET\\(JSON_SET\\(JSON_SET\\(JSON_SET\\(attributes,    'RWEndpoint', '6446'\\),    'ROEndpoint', '6447'\\),    'RWXEndpoint', '64460'\\),    'ROXEndpoint', '64470'\\) WHERE router_id = .*",
            "exec_time": 0.319936,
            "ok": {}
        },
        {
            "stmt": "COMMIT",
            "exec_time": 0.106985,
            "ok": {}
        }
    ]
}
