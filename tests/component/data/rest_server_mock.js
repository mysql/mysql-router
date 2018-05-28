function statement_handler() {
  return new Duktape.Thread(function(stmt) {
    var yield = Duktape.Thread.yield;

    while(true) {
      if (stmt === "select @@port") {
        stmt = yield({
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
        });
      } else {
        stmt = yield({
          error: {
            code: 1273,
            sql_state: "HY001",
            message: "Syntax Error at: " + stmt
          }
        });
      }
    }
  });
}

({
  stmts: statement_handler()
})
