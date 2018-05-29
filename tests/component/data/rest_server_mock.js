({
  stmts: function (stmt) {
    if (stmt === "select @@port") {
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
    } else {
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
