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
        {
            "stmt.regex": "^DROP USER IF EXISTS mysql_router.*@'%'",
            "exec_time": 15.643484,
            "ok": {}
        },
        {
            "stmt.regex": "^CREATE USER mysql_router.*@'%' IDENTIFIED WITH mysql_native_password AS '.*'",
            "exec_time": 4.102568,
            "ok": {}
        },
        {
            "stmt.regex": "^GRANT SELECT ON mysql_innodb_cluster_metadata.* TO mysql_router.*@'%'",
            "exec_time": 9.717177,
            "ok": {}
        },
        {
            "stmt.regex": "^GRANT SELECT ON performance_schema.replication_group_members TO mysql_router.*@'%'",
            "exec_time": 6.181016,
            "ok": {}
        },
        {
            "stmt.regex": "^GRANT SELECT ON performance_schema.replication_group_member_stats TO mysql_router.*@'%'",
            "exec_time": 14.53201,
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
