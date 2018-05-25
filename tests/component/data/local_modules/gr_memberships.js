var uuid_v4 = function() {
  return "00000000-0000-4000-8000-000000000000".replace(/0/g, function() {
      return (0|Math.random()*16).toString(16)}
  );
}

exports.single_host = function(host, port_and_state) {
  return port_and_state.map(function (current_value) {
    return [ uuid_v4(), host, current_value[0], current_value[1] ];
  });
};
