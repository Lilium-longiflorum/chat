const http = require('http');
const fs = require('fs');
const path = require('path');
const WebSocket = require('ws');
const net = require('net');

const loadConfig = () => {
  const configPath = path.resolve(__dirname, '..', 'config.txt');
  let config = {
    tcp_host: '127.0.0.1',
    tcp_port: 8080,
    web_port: 9000,
  };

  try {
    const lines = fs.readFileSync(configPath, 'utf-8')
      .split('\n')
      .map(line => line.trim())
      .filter(line => line.length > 0 && !line.startsWith('#'));

    lines.forEach((line, index) => {
      console.log(`[${index}] Line: "${line}"`);
      const match = line.match(/^([^:]+):\s*(.*?)\s*$/);
      if (match) {
        const key = match[1].trim();
        const value = match[2].trim();
        if (key === 'tcp_host') config.tcp_host = value;
        else if (key === 'port') config.tcp_port = parseInt(value, 10);
        else if (key === 'web_port') config.web_port = parseInt(value, 10);
        else console.log(`Unknown config key: ${key} (ignored)`);
      } else {
        console.log(`No match for line: "${line}"`);
      }
    });
  } catch (err) {
    console.log('No config.txt found, using defaults.');
  }

  console.log('Loaded config:', config);
  return config;
};


// load config
const config = loadConfig();
const { tcp_host: TCP_HOST, tcp_port: TCP_PORT, web_port: PORT } = config;

console.log(`Using config:
  TCP_HOST: ${TCP_HOST}
  TCP_PORT: ${TCP_PORT}
  WEB_PORT: ${PORT}`);

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
