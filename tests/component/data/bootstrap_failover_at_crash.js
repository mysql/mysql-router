{
    "stmts": [
        {
            "stmt": "START TRANSACTION",
            "exec_time": 0.082893,
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
            "stmt.regex": "^DROP USER IF EXISTS mysql_router.*",
            "ok": {}
        },
        {
            "stmt.regex": "^CREATE USER mysql_router.*",
            "error": {
                "code": 2013,
                "message": "Lost connection to MySQL server during query",
                "sql_state": "HY000"
            }
        }
    ]
}
