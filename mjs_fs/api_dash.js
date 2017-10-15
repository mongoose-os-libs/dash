let Dash = {
  _send: ffi('void mgos_dash_send_data(char *, double)'),

  // **`Dash.send(name, value)`**
  // Send a numeric data `value` to the Mongoose OS dashboard.
  // See https://mongoose-os.com/docs/reference/dashboard.html for
  // more information.
  // Example:
  // ```javascript
  // Dash.send('temperature', 22.45);
  // ```
  send: function(name, value) {
    this._send('{' + name + ': %lf}', value);
  },
};
