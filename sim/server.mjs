#!/usr/bin/env node
// Zero-dependency Local Simulator & Companion Web Server for Smart 16x16 Acrylic LED Matrix
// Bridges the real compiled C++17 firmware engine (sim/matrix_sim) <-> data/ PWA!

import http from 'node:http';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { spawn, execSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(__dirname, '..');
const DATA_DIR = path.join(ROOT, 'data');
const SIM_BIN = path.join(__dirname, 'matrix_sim');
const PORT = Number(process.env.PORT || 8080);

// Auto-build native C++17 engine if needed
function ensureSimBinary() {
  if (!fs.existsSync(SIM_BIN)) {
    console.log('[Sim] Building native C++17 display engine...');
    execSync(
      `clang++ -std=c++17 -O2 -DMATRIX_NATIVE=1 -Iinclude -Isrc src/gfx/*.cpp src/core/*.cpp src/modes/*.cpp sim/sim_engine.cpp -o "${SIM_BIN}"`,
      { cwd: ROOT, stdio: 'inherit' }
    );
  }
}
ensureSimBinary();

const proc = spawn(SIM_BIN, [], { stdio: ['pipe', 'pipe', 'inherit'] });
let stdoutBuf = '';
const waiters = [];
const wsClients = new Set();

function sendCmd(line, expectReply = true) {
  return new Promise((resolve) => {
    if (expectReply) waiters.push(resolve);
    proc.stdin.write(line + '\n');
    if (!expectReply) resolve('');
  });
}

proc.stdout.on('data', (chunk) => {
  stdoutBuf += chunk.toString('utf8');
  let nl;
  while ((nl = stdoutBuf.indexOf('\n')) !== -1) {
    const line = stdoutBuf.slice(0, nl).trim();
    stdoutBuf = stdoutBuf.slice(nl + 1);
    if (line.startsWith('FRAME ')) {
      const rgb = Buffer.from(line.slice(6), 'hex');
      if (rgb.length === 768) {
        const pkt = Buffer.allocUnsafe(769);
        pkt[0] = 0x01;
        rgb.copy(pkt, 1);
        broadcastWsBinary(pkt);
      }
    } else if (waiters.length) {
      const fn = waiters.shift();
      fn(line);
    }
  }
});

// Minimal RFC 6455 WebSocket framing (zero dependencies)
function wsFrameBinary(payload) {
  const len = payload.length;
  let header;
  if (len < 126) {
    header = Buffer.from([0x82, len]);
  } else {
    header = Buffer.allocUnsafe(4);
    header[0] = 0x82; header[1] = 126;
    header.writeUInt16BE(len, 2);
  }
  return Buffer.concat([header, payload]);
}

function broadcastWsBinary(payload) {
  if (!wsClients.size) return;
  const frame = wsFrameBinary(payload);
  for (const sock of wsClients) {
    if (!sock.destroyed) sock.write(frame);
  }
}

function parseWsFrames(buf, onMessage) {
  let off = 0;
  while (off + 2 <= buf.length) {
    const b0 = buf[off], b1 = buf[off + 1];
    const opcode = b0 & 0x0F;
    const masked = (b1 & 0x80) !== 0;
    let len = b1 & 0x7F;
    let pos = off + 2;
    if (len === 126) {
      if (pos + 2 > buf.length) break;
      len = buf.readUInt16BE(pos); pos += 2;
    } else if (len === 127) {
      if (pos + 8 > buf.length) break;
      len = Number(buf.readBigUInt64BE(pos)); pos += 8;
    }
    const maskLen = masked ? 4 : 0;
    if (pos + maskLen + len > buf.length) break;
    const payload = Buffer.allocUnsafe(len);
    if (masked) {
      const mask = buf.subarray(pos, pos + 4);
      pos += 4;
      for (let i = 0; i < len; i++) payload[i] = buf[pos + i] ^ mask[i & 3];
    } else {
      buf.copy(payload, 0, pos, pos + len);
    }
    off = pos + len;
    if (opcode === 0x02) onMessage(payload);
  }
  return buf.subarray(off);
}

// Tick engine at 25 FPS whenever clients are connected (or 5 FPS idle)
let lastTick = Date.now();
setInterval(() => {
  const now = Date.now();
  const dt = Math.min(0.1, Math.max(0.01, (now - lastTick) / 1000));
  lastTick = now;
  proc.stdin.write(`TICK ${dt.toFixed(4)}\n`);
}, 40);

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml'
};

function readBody(req) {
  return new Promise((resolve) => {
    const chunks = [];
    req.on('data', (c) => chunks.push(c));
    req.on('end', () => resolve(Buffer.concat(chunks)));
  });
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);

  if (url.pathname === '/api/state' && req.method === 'GET') {
    const line = await sendCmd('STATE');
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end(line.startsWith('STATE ') ? line.slice(6) : '{}');
  }

  if (url.pathname === '/api/control' && req.method === 'POST') {
    const body = JSON.parse((await readBody(req)).toString('utf8') || '{}');
    for (const k of ['powerOn', 'brightness', 'animationSpeed', 'primaryColor', 'accentColor', 'transition', 'activeMode', 'playlistPlaying', 'activePlaylistId']) {
      if (body[k] !== undefined) await sendCmd(`SET ${k} ${body[k]}`);
    }
    if (body.playlistAction === 'next') await sendCmd('SET playlistNext 1');
    if (body.playlistAction === 'prev') await sendCmd('SET playlistPrev 1');
    if (body.modeSettings) {
      for (const [k, v] of Object.entries(body.modeSettings)) {
        await sendCmd(`SET ms.${k} ${v}`);
      }
    }
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end('{"ok":true}');
  }

  if (url.pathname === '/api/sensors/inject' && req.method === 'POST') {
    const body = JSON.parse((await readBody(req)).toString('utf8') || '{}');
    for (const k of ['weatherCondition', 'weatherTempC', 'printerState', 'printerProgress', 'heartRateBpm', 'aqi', 'indoorTempC']) {
      if (body[k] !== undefined) await sendCmd(`SET live.${k} ${body[k]}`);
    }
    if (body.motionDetected) {
      await sendCmd(`SET live.motion ${body.motionX ?? 0.5} ${body.motionY ?? 0.5}`);
    }
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end('{"ok":true}');
  }

  if (url.pathname === '/api/media/upload' && req.method === 'POST') {
    const raw = await readBody(req);
    const id = (url.searchParams.get('id') || ('art_' + Date.now())).replace(/\s+/g, '_');
    const name = (url.searchParams.get('name') || id).replace(/\s+/g, '_');
    const reply = await sendCmd(`UPLOAD_MPX ${id} ${name} ${raw.toString('hex')}`);
    res.writeHead(reply === 'OK' ? 200 : 400, { 'Content-Type': 'application/json' });
    return res.end(JSON.stringify({ ok: reply === 'OK', id }));
  }

  if (url.pathname === '/api/media/delete' && req.method === 'POST') {
    const body = JSON.parse((await readBody(req)).toString('utf8') || '{}');
    await sendCmd(`DEL_MEDIA ${body.id || ''}`);
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end('{"ok":true}');
  }

  if (url.pathname === '/api/album_art' && req.method === 'POST') {
    const raw = await readBody(req);
    await sendCmd(`ALBUM_HEX ${raw.toString('hex')}`);
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end('{"ok":true}');
  }

  if (url.pathname === '/api/playlists' && req.method === 'POST') {
    const body = JSON.parse((await readBody(req)).toString('utf8') || '{}');
    if (Array.isArray(body.playlists) && body.playlists.length) {
      await sendCmd('CLEAR_PLAYLISTS');
      for (const pl of body.playlists) {
        await sendCmd(`ADD_PLAYLIST ${pl.id} ${pl.name}`);
        for (const pg of (pl.pages || [])) {
          await sendCmd(`ADD_PAGE ${pl.id} ${pg.mode} ${pg.durationSec || 15}`);
        }
      }
    }
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end('{"ok":true}');
  }

  if (url.pathname === '/api/schedules' && req.method === 'POST') {
    const body = JSON.parse((await readBody(req)).toString('utf8') || '{}');
    if (body.nightSchedule) {
      for (const [k, v] of Object.entries(body.nightSchedule)) {
        await sendCmd(`SET ns.${k} ${v}`);
      }
    }
    if (Array.isArray(body.schedules)) {
      await sendCmd('CLEAR_SCHEDULES');
      for (const r of body.schedules) {
        await sendCmd(`ADD_SCHEDULE ${r.enabled ? 1 : 0} ${r.hour || 0} ${r.minute || 0} ${r.daysMask ?? 127} ${r.action || 'mode'} ${r.value || 'clock'} ${r.brightness || 180}`);
      }
    }
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end('{"ok":true}');
  }

  if (url.pathname === '/api/config' && req.method === 'POST') {
    const body = JSON.parse((await readBody(req)).toString('utf8') || '{}');
    if (body.network) {
      for (const k of ['wifiSsid', 'hostname', 'haEnabled', 'haUrl', 'mqttHost', 'mqttPort']) {
        if (body.network[k] !== undefined) await sendCmd(`SET net.${k} ${body.network[k]}`);
      }
    }
    if (body.location) {
      for (const k of ['zipCode', 'latitude', 'longitude', 'tzOffsetMinutes', 'useFahrenheit']) {
        if (body.location[k] !== undefined) await sendCmd(`SET loc.${k} ${body.location[k]}`);
      }
    }
    if (body.sensorHw) {
      for (const k of ['audioSource', 'mmwaveModel', 'envSensorModel', 'cameraUrl']) {
        if (body.sensorHw[k] !== undefined) await sendCmd(`SET hw.${k} ${body.sensorHw[k]}`);
      }
    }
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end('{"ok":true}');
  }

  // Static PWA assets from data/
  let rel = url.pathname === '/' ? '/index.html' : url.pathname;
  const fullPath = path.join(DATA_DIR, path.normalize(rel));
  if (fullPath.startsWith(DATA_DIR) && fs.existsSync(fullPath) && fs.statSync(fullPath).isFile()) {
    const ext = path.extname(fullPath);
    res.writeHead(200, { 'Content-Type': MIME[ext] || 'application/octet-stream' });
    return fs.createReadStream(fullPath).pipe(res);
  }
  res.writeHead(404);
  res.end('Not Found');
});

server.on('upgrade', (req, socket) => {
  const key = req.headers['sec-websocket-key'];
  if (!key) return socket.destroy();
  const accept = crypto.createHash('sha1').update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  socket.write(
    'HTTP/1.1 101 Switching Protocols\r\n' +
    'Upgrade: websocket\r\n' +
    'Connection: Upgrade\r\n' +
    `Sec-WebSocket-Accept: ${accept}\r\n\r\n`
  );
  wsClients.add(socket);
  let rxBuf = Buffer.alloc(0);
  socket.on('data', (chunk) => {
    rxBuf = parseWsFrames(Buffer.concat([rxBuf, chunk]), (payload) => {
      if (!payload.length) return;
      const op = payload[0];
      if (op === 0x02 && payload.length >= 769) {
        proc.stdin.write(`MIRROR_HEX ${payload.subarray(1, 769).toString('hex')}\n`);
      } else if (op === 0x03 && payload.length >= 17) {
        proc.stdin.write(`AUDIO_HEX ${payload.subarray(1, 17).toString('hex')}\n`);
      } else if (op === 0x04 && payload.length >= 3) {
        const mx = (payload[1] / 255).toFixed(3);
        const my = (payload[2] / 255).toFixed(3);
        proc.stdin.write(`SET live.motion ${mx} ${my}\n`);
      }
    });
  });
  socket.on('close', () => wsClients.delete(socket));
  socket.on('error', () => wsClients.delete(socket));
});

server.listen(PORT, '127.0.0.1', () => {
  console.log(`\n✨ Smart 16×16 Acrylic LED Matrix Simulator running at http://127.0.0.1:${PORT}\n`);
});

process.on('SIGINT', () => { proc.kill(); process.exit(0); });
process.on('SIGTERM', () => { proc.kill(); process.exit(0); });
