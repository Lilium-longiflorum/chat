// run_all.js
const { exec } = require("child_process");
const isWin = process.platform === "win32";

if (isWin) {
  exec('start cmd /k server');
  exec('start cmd /k node ws_tcp_bridge\\bridge.js');
} else {
  exec('gnome-terminal -- bash -c "./server"');
  exec('gnome-terminal -- bash -c "node ws_tcp_bridge/bridge.js; exec bash"');
}
