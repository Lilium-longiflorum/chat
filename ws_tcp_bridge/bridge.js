const http = require('http');
const fs = require('fs');
const path = require('path');
const WebSocket = require('ws');
const net = require('net');

const TCP_HOST = '127.0.0.1';
const TCP_PORT = 8080;
const PORT = 9000;

const mimeTypes = {
  '.html': 'text/html; charset=UTF-8',
  '.js': 'text/javascript; charset=UTF-8',
  '.css': 'text/css',
  '.json': 'application/json',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.gif': 'image/gif',
};

const server = http.createServer((req, res) => {
  let filePath = req.url === '/' ? '/index.html' : req.url;
  filePath = path.join(__dirname, 'public', filePath);

  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404);
      res.end("Not found");
    } else {
      const ext = path.extname(filePath).toLowerCase();
      const contentType = mimeTypes[ext] || 'application/octet-stream';
      res.writeHead(200, { 'Content-Type': contentType });
      res.end(data);
    }
  });
});

const wss = new WebSocket.Server({ server });

wss.on('connection', (ws) => {
  console.log('Web client connected');

  const tcpClient = new net.Socket();
  tcpClient.connect(TCP_PORT, TCP_HOST, () => {
    console.log('Connected to C chat server');
  });

  ws.on('message', (msg) => tcpClient.write(msg));
  tcpClient.on('data', (data) => {
    const msg = data.toString('utf8');
    ws.send(msg);
  });

  tcpClient.on('close', () => ws.close());
  ws.on('close', () => tcpClient.destroy());

  tcpClient.on('error', (err) => {
    console.error('TCP error:', err.message);
    ws.close();
  });
});

server.listen(PORT, () => {
  console.log(`Server running at http://localhost:${PORT}/`);
});
