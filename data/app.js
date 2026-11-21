(() => {
  'use strict';

  const $ = (id) => document.getElementById(id);
  let state = null;
  let ws = null;
  let wsConnected = false;

  // Live streaming handles (phone camera for Mirror, phone mic for Music)
  let camStream = null;
  let camTimer = null;
  let micCtx = null;
  let micTimer = null;

  // Uploader state
  let uploadFrames = []; // [{canvas, delayMs}]
  let uploadAnimIdx = 0;
  let uploadAnimTimer = null;
  let cropPanX = 0, cropPanY = 0, cropZoom = 1.0;
  let draggingCrop = false, dragStartX = 0, dragStartY = 0;

  const MODES_FALLBACK = [
    { key: 'clock', label: 'Clock', category: 'Utility' },
    { key: 'weather', label: 'Weather', category: 'Info' },
    { key: 'moon_phase', label: 'Moon Phase', category: 'Astronomy' },
    { key: 'music_visualizer', label: 'Music Visualizer', category: 'Reactive' },
    { key: 'seasonal', label: 'Seasonal Themes', category: 'Ambient' },
    { key: 'sunrise_sunset', label: 'Sunrise & Sunset', category: 'Astronomy' },
    { key: 'album_art', label: 'Album Art', category: 'Media' },
    { key: 'printer_3d', label: '3D Printer', category: 'Smart Home' },
    { key: 'pixel_art', label: 'Pixel Art', category: 'Art' },
    { key: 'fireplace', label: 'Fireplace', category: 'Ambient' },
    { key: 'waves', label: 'Ocean Waves', category: 'Ambient' },
    { key: 'aquarium', label: 'Aquarium', category: 'Ambient' },
    { key: 'motion_reactive', label: 'Motion Reactive', category: 'Reactive' },
    { key: 'mirror', label: 'Mirror Mode', category: 'Reactive' },
    { key: 'heart_rate', label: 'Heart Rate', category: 'Sensors' },
    { key: 'air_quality', label: 'Air Quality', category: 'Sensors' },
    { key: 'solar_system', label: 'Solar System', category: 'Space' },
    { key: 'spinning_globe', label: 'Spinning Globe', category: 'Space' },
  ];

  function toast(msg) {
    const el = $('toast');
    el.textContent = msg;
    el.classList.add('show');
    clearTimeout(el._t);
    el._t = setTimeout(() => el.classList.remove('show'), 2200);
  }

  function hexFromInt(n) {
    return '#' + (n >>> 0).toString(16).padStart(6, '0').slice(-6);
  }
  function intFromHex(h) {
    return parseInt(h.replace('#', ''), 16) || 0;
  }

  async function apiGet(url) {
    const r = await fetch(url);
    return r.json();
  }
  async function apiPost(url, body, isBinary = false) {
    const r = await fetch(url, {
      method: 'POST',
      headers: isBinary ? { 'Content-Type': 'application/octet-stream' } : { 'Content-Type': 'application/json' },
      body: isBinary ? body : JSON.stringify(body)
    });
    return r.json();
  }

  // ---- Realistic 8-inch Frosted Acrylic 16x16 Diffuser Renderer ----
  const previewCanvas = $('matrixPreview');
  const previewCtx = previewCanvas.getContext('2d');

  function renderAcrylicFrame(rgbBytes, targetCtx = previewCtx, size = 320) {
    const cell = size / 16;
    targetCtx.fillStyle = '#06080f';
    targetCtx.fillRect(0, 0, size, size);

    // 1. Soft diffused subsurface glow under frosted acrylic
    targetCtx.save();
    targetCtx.filter = `blur(${Math.max(2, cell * 0.42)}px)`;
    for (let y = 0; y < 16; y++) {
      for (let x = 0; x < 16; x++) {
        const i = (y * 16 + x) * 3;
        const r = rgbBytes[i], g = rgbBytes[i + 1], b = rgbBytes[i + 2];
        if ((r | g | b) === 0) continue;
        targetCtx.fillStyle = `rgb(${r},${g},${b})`;
        targetCtx.fillRect(x * cell + 1, y * cell + 1, cell - 2, cell - 2);
      }
    }
    targetCtx.restore();

    // 2. Crisp rounded LED cells with subtle frosted lattice dividers
    const pad = Math.max(1, cell * 0.11);
    const rad = Math.max(2, cell * 0.24);
    for (let y = 0; y < 16; y++) {
      for (let x = 0; x < 16; x++) {
        const i = (y * 16 + x) * 3;
        const r = rgbBytes[i], g = rgbBytes[i + 1], b = rgbBytes[i + 2];
        const px = x * cell + pad;
        const py = y * cell + pad;
        const pw = cell - pad * 2;
        targetCtx.fillStyle = `rgba(${r},${g},${b},0.88)`;
        targetCtx.beginPath();
        targetCtx.roundRect(px, py, pw, pw, rad);
        targetCtx.fill();
      }
    }
  }

  // ---- WebSocket Live Preview + Mirror/Audio/Motion Streaming ----
  function connectWs() {
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    // Simulator serves WS on same port at /ws; ESP32 serves WS on port 81
    const isStandardPort = location.port === '' || location.port === '80' || location.port === '443';
    const wsUrl = isStandardPort
      ? `${proto}//${location.hostname}:81/`
      : `${proto}//${location.host}/ws`;

    try {
      ws = new WebSocket(wsUrl);
      ws.binaryType = 'arraybuffer';
      ws.onopen = () => { wsConnected = true; };
      ws.onclose = () => {
        wsConnected = false;
        setTimeout(connectWs, 2500);
      };
      ws.onmessage = (ev) => {
        if (ev.data instanceof ArrayBuffer) {
          const u8 = new Uint8Array(ev.data);
          if (u8.length >= 769 && u8[0] === 0x01) {
            renderAcrylicFrame(u8.subarray(1, 769));
          }
        }
      };
    } catch (_) {
      setTimeout(connectWs, 3000);
    }
  }

  // Interactive touch/mouse on acrylic preview -> Motion Reactive coordinates!
  function sendMotionFromPointer(ev) {
    if (!wsConnected || ws.readyState !== WebSocket.OPEN) return;
    const rect = previewCanvas.getBoundingClientRect();
    const x = Math.max(0, Math.min(255, Math.round(((ev.clientX - rect.left) / rect.width) * 255)));
    const y = Math.max(0, Math.min(255, Math.round(((ev.clientY - rect.top) / rect.height) * 255)));
    ws.send(new Uint8Array([0x04, x, y]));
  }
  let pointerDown = false;
  previewCanvas.addEventListener('pointerdown', (e) => { pointerDown = true; sendMotionFromPointer(e); });
  previewCanvas.addEventListener('pointermove', (e) => { if (pointerDown) sendMotionFromPointer(e); });
  window.addEventListener('pointerup', () => { pointerDown = false; });

  // ---- Phone Camera Stream for Mirror Mode ----
  async function toggleCameraStream() {
    if (camStream) {
      camStream.getTracks().forEach((t) => t.stop());
      camStream = null;
      clearInterval(camTimer);
      renderModeSettings();
      toast('Camera mirror stopped');
      return;
    }
    try {
      camStream = await navigator.mediaDevices.getUserMedia({
        video: { facingMode: 'user', width: 160, height: 160 },
        audio: false
      });
      const video = document.createElement('video');
      video.srcObject = camStream;
      video.muted = true;
      await video.play();

      const tmp = document.createElement('canvas');
      tmp.width = 16; tmp.height = 16;
      const tctx = tmp.getContext('2d', { willReadFrequently: true });
      const packet = new Uint8Array(1 + 768);
      packet[0] = 0x02;

      camTimer = setInterval(() => {
        if (!wsConnected || ws.readyState !== WebSocket.OPEN) return;
        const vw = video.videoWidth || 160;
        const vh = video.videoHeight || 160;
        const side = Math.min(vw, vh);
        const sx = (vw - side) / 2;
        const sy = (vh - side) / 2;
        tctx.drawImage(video, sx, sy, side, side, 0, 0, 16, 16);
        const img = tctx.getImageData(0, 0, 16, 16).data;
        for (let i = 0, j = 1; i < 256; i++, j += 3) {
          packet[j] = img[i * 4];
          packet[j + 1] = img[i * 4 + 1];
          packet[j + 2] = img[i * 4 + 2];
        }
        ws.send(packet);
      }, 66);
      renderModeSettings();
      toast('Live Mirror camera streaming!');
    } catch (err) {
      toast('Camera permission unavailable');
    }
  }

  // ---- Phone Mic Stream for Music Visualizer ----
  async function toggleMicStream() {
    if (micCtx) {
      clearInterval(micTimer);
      await micCtx.close();
      micCtx = null;
      renderModeSettings();
      toast('Microphone stream stopped');
      return;
    }
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      micCtx = new (window.AudioContext || window.webkitAudioContext)();
      const src = micCtx.createMediaStreamSource(stream);
      const analyser = micCtx.createAnalyser();
      analyser.fftSize = 256;
      src.connect(analyser);
      const freq = new Uint8Array(analyser.frequencyBinCount);
      const packet = new Uint8Array(17);
      packet[0] = 0x03;

      micTimer = setInterval(() => {
        if (!wsConnected || ws.readyState !== WebSocket.OPEN) return;
        analyser.getByteFrequencyData(freq);
        for (let b = 0; b < 16; b++) {
          const start = Math.floor((b / 16) * (freq.length * 0.65));
          const end = Math.max(start + 1, Math.floor(((b + 1) / 16) * (freq.length * 0.65)));
          let sum = 0;
          for (let k = start; k < end; k++) sum += freq[k];
          packet[1 + b] = Math.min(255, Math.round((sum / (end - start)) * 1.35));
        }
        ws.send(packet);
      }, 45);
      renderModeSettings();
      toast('Phone microphone streaming to visualizer!');
    } catch (_) {
      toast('Microphone permission unavailable');
    }
  }

  // ---- State Sync & UI Rendering ----
  async function refreshState() {
    try {
      state = await apiGet('/api/state');
      renderAll();
    } catch (e) {
      console.error(e);
    }
  }

  async function sendControl(patch) {
    try {
      await apiPost('/api/control', patch);
      await refreshState();
    } catch (_) {}
  }

  function renderAll() {
    if (!state) return;

    // Header status pills
    const netPill = $('pillNet');
    netPill.textContent = state.live?.wifiConnected ? 'Wi-Fi' : 'Offline OK';
    netPill.className = 'pill ' + (state.live?.wifiConnected ? 'ok' : 'warn');

    const haPill = $('pillHa');
    haPill.textContent = (state.live?.haConnected || state.live?.mqttConnected) ? 'HA Live' : 'HA Idle';
    haPill.className = 'pill ' + ((state.live?.haConnected || state.live?.mqttConnected) ? 'ok' : '');

    const pwr = $('btnPower');
    pwr.textContent = state.powerOn ? 'ON' : 'OFF';
    pwr.className = 'btn small ' + (state.powerOn ? 'power-on' : 'danger');

    // Global sliders
    $('rngBright').value = state.brightness;
    $('valBright').textContent = Math.round((state.brightness / 255) * 100) + '%';
    $('rngSpeed').value = state.animationSpeed;
    $('valSpeed').textContent = Number(state.animationSpeed).toFixed(2) + '×';
    $('colPrimary').value = hexFromInt(state.primaryColor);
    $('colAccent').value = hexFromInt(state.accentColor);
    $('selTransition').value = state.transition || 'crossfade';

    const modes = state.modes || MODES_FALLBACK;
    const curInfo = modes.find((m) => m.key === state.activeMode) || { label: state.activeMode };
    $('lblActiveMode').textContent = curInfo.label;
    $('lblStatusText').textContent = state.autoOffActive
      ? 'Auto Display-Off Active'
      : state.nightDimActive
        ? 'Nighttime Dimming Active'
        : state.playlistPlaying
          ? `Playlist: ${state.activePlaylistId}`
          : 'Live 16×16 Acrylic Diffuser';

    // Mode Grid
    const grid = $('modeGrid');
    grid.innerHTML = '';
    for (const m of modes) {
      const b = document.createElement('button');
      b.className = 'mode-btn' + (m.key === state.activeMode ? ' active' : '');
      b.innerHTML = `<span class="cat">${m.category}</span><span>${m.label}</span>`;
      b.onclick = () => sendControl({ activeMode: m.key });
      grid.appendChild(b);
    }

    renderModeSettings();
    renderMediaGallery();
    renderPlaylists();
    renderSchedules();
    populateSetupFields();
  }

  function renderModeSettings() {
    if (!state) return;
    const ms = state.modeSettings || {};
    const m = state.activeMode;
    const body = $('modeSettingsBody');
    const title = $('modeSettingsTitle');
    body.innerHTML = '';

    const addSelect = (label, key, opts, val) => {
      const wrap = document.createElement('div');
      wrap.className = 'row';
      wrap.innerHTML = `<span>${label}</span><select style="width:165px">${opts.map(([v, t]) =>
        `<option value="${v}" ${v === val ? 'selected' : ''}>${t}</option>`).join('')}</select>`;
      wrap.querySelector('select').onchange = (e) => sendControl({ modeSettings: { [key]: e.target.value } });
      body.appendChild(wrap);
    };

    const addToggle = (label, key, val) => {
      const wrap = document.createElement('div');
      wrap.className = 'row';
      wrap.innerHTML = `<span>${label}</span><label class="switch"><input type="checkbox" ${val ? 'checked' : ''}><span class="slider"></span></label>`;
      wrap.querySelector('input').onchange = (e) => sendControl({ modeSettings: { [key]: e.target.checked } });
      body.appendChild(wrap);
    };

    if (m === 'clock') {
      title.textContent = 'Clock Settings';
      addToggle('24-Hour Format', 'clock24h', ms.clock24h);
      addToggle('Seconds Progress Bar', 'clockShowSecondsBar', ms.clockShowSecondsBar);
      addSelect('Color Style', 'clockColorStyle', [
        ['gradient', 'Gradient'], ['solid', 'Solid'], ['rainbow', 'Rainbow'], ['time_of_day', 'Time of Day']
      ], ms.clockColorStyle);
    } else if (m === 'weather') {
      title.textContent = 'Weather Settings';
      addToggle('Show Temperature', 'weatherShowTemp', ms.weatherShowTemp);
    } else if (m === 'moon_phase') {
      title.textContent = 'Moon Phase Settings';
      addToggle('Show Illumination %', 'moonShowPercent', ms.moonShowPercent);
    } else if (m === 'music_visualizer') {
      title.textContent = 'Music Visualizer Settings';
      addSelect('Visualizer Style', 'musicStyle', [
        ['spectrum', '16-Band Spectrum'], ['mirror', 'Center Mirror'], ['radial', 'Radial Pulse'], ['vu', 'Stereo VU Meter']
      ], ms.musicStyle);
      const btn = document.createElement('button');
      btn.className = 'btn ' + (micCtx ? 'danger' : 'primary');
      btn.textContent = micCtx ? 'Stop Phone Mic Stream' : 'Stream Phone Microphone Live';
      btn.onclick = toggleMicStream;
      body.appendChild(btn);
    } else if (m === 'seasonal') {
      title.textContent = 'Seasonal Theme';
      addSelect('Theme', 'seasonalTheme', [
        ['auto', 'Auto (By Date)'], ['spring', 'Spring Blossoms'], ['summer', 'Summer Beach'],
        ['autumn', 'Autumn Leaves'], ['winter', 'Winter Snow'], ['halloween', 'Halloween'],
        ['christmas', 'Christmas Tree'], ['newyear', 'New Year Fireworks'], ['valentine', 'Valentine Heart']
      ], ms.seasonalTheme);
    } else if (m === 'sunrise_sunset') {
      title.textContent = 'Sunrise & Sunset Settings';
      addToggle('Gradual Dawn/Dusk Glow', 'sunGradualEffect', ms.sunGradualEffect);
    } else if (m === 'pixel_art') {
      title.textContent = 'Pixel Art Scene';
      const opts = [
        ['cabin', 'Cozy Cabin'], ['invader', 'Retro Invader'], ['coffee', 'Steaming Coffee'],
        ['cat', 'Blinking Cat'], ['synth', 'Synthwave Sunset'], ['bonsai', 'Bonsai Tree']
      ];
      for (const a of (state.media || [])) {
        opts.push([a.id, `★ ${a.name}${a.animated ? ' (GIF)' : ''}`]);
      }
      addSelect('Active Scene / Upload', 'pixelArtScene', opts, ms.pixelArtScene);
    } else if (m === 'motion_reactive') {
      title.textContent = 'Motion Reactive Settings';
      addSelect('Animation Style', 'motionStyle', [
        ['ripples', 'Expanding Ripples'], ['swarm', 'Magnetic Swarm'], ['aura', 'Presence Aura']
      ], ms.motionStyle);
      const hint = document.createElement('div');
      hint.style.cssText = 'font-size:12px;color:var(--muted)';
      hint.textContent = 'Tip: Touch or drag directly on the 16×16 acrylic preview above to test motion ripples!';
      body.appendChild(hint);
    } else if (m === 'mirror') {
      title.textContent = 'Mirror Mode Settings';
      addToggle('Mirror Flip Horizontal', 'mirrorFlipX', ms.mirrorFlipX);
      addSelect('Color Palette', 'mirrorPalette', [
        ['truecolor', 'True Color'], ['thermal', 'Thermal Heatmap'], ['retro', 'Retro Handheld'], ['neon', 'Neon Cyber']
      ], ms.mirrorPalette);
      const btn = document.createElement('button');
      btn.className = 'btn ' + (camStream ? 'danger' : 'primary');
      btn.textContent = camStream ? 'Stop Camera Mirror' : 'Start Phone Camera Mirror';
      btn.onclick = toggleCameraStream;
      body.appendChild(btn);
    } else if (m === 'heart_rate') {
      title.textContent = 'Heart Rate (mmWave) Settings';
      addToggle('Show ECG Waveform', 'heartRateShowEcg', ms.heartRateShowEcg);
    } else if (m === 'air_quality') {
      title.textContent = 'Air Quality Settings';
      addToggle('Include CO2 Cycle', 'airQualityShowCo2', ms.airQualityShowCo2);
    } else if (m === 'printer_3d') {
      title.textContent = '3D Printer Status';
      addToggle('Flash Completion Alert', 'printerAlertFlash', ms.printerAlertFlash);
    } else {
      title.textContent = 'Mode Info';
      body.innerHTML = `<div style="font-size:12px;color:var(--muted)">Running smoothly offline &amp; synced with global speed, colors, and brightness.</div>`;
    }
  }

  // ---- Media Gallery & Image/GIF Uploader ----
  function renderMediaGallery() {
    const el = $('mediaGallery');
    const list = state.media || [];
    if (!list.length) {
      el.innerHTML = `<div style="font-size:12px;color:var(--muted)">No custom artwork uploaded yet. Pick a PNG, JPG, or GIF above!</div>`;
      return;
    }
    el.innerHTML = '';
    for (const item of list) {
      const row = document.createElement('div');
      row.className = 'list-item';
      row.innerHTML = `
        <div>
          <strong>${item.name}</strong>
          <div style="font-size:11px;color:var(--muted)">${item.animated ? `${item.frames} GIF frames` : 'Static 16×16'}</div>
        </div>
        <div style="display:flex;gap:6px">
          <button class="btn small primary">Display</button>
          <button class="btn small danger">✕</button>
        </div>`;
      row.querySelector('.primary').onclick = () =>
        sendControl({ activeMode: 'pixel_art', modeSettings: { pixelArtScene: item.id } });
      row.querySelector('.danger').onclick = async () => {
        await apiPost('/api/media/delete', { id: item.id });
        toast('Deleted artwork');
        refreshState();
      };
      el.appendChild(row);
    }
  }

  function extractCropped16(srcCanvas) {
    const out = document.createElement('canvas');
    out.width = 16; out.height = 16;
    const octx = out.getContext('2d', { willReadFrequently: true });
    octx.imageSmoothingEnabled = $('selResize').value !== 'nearest';
    octx.imageSmoothingQuality = 'high';

    const side = Math.min(srcCanvas.width, srcCanvas.height) / cropZoom;
    const cx = srcCanvas.width / 2 - cropPanX * (srcCanvas.width / 210);
    const cy = srcCanvas.height / 2 - cropPanY * (srcCanvas.height / 210);
    const sx = Math.max(0, Math.min(srcCanvas.width - side, cx - side / 2));
    const sy = Math.max(0, Math.min(srcCanvas.height - side, cy - side / 2));

    octx.fillStyle = '#000';
    octx.fillRect(0, 0, 16, 16);
    octx.drawImage(srcCanvas, sx, sy, side, side, 0, 0, 16, 16);

    const img = octx.getImageData(0, 0, 16, 16).data;
    const contrast = parseFloat($('rngContrast').value);
    const sat = parseFloat($('rngSat').value);
    const rgb = new Uint8Array(768);

    for (let i = 0; i < 256; i++) {
      let r = img[i * 4] / 255, g = img[i * 4 + 1] / 255, b = img[i * 4 + 2] / 255;
      // Contrast around mid-gray
      r = (r - 0.5) * contrast + 0.5;
      g = (g - 0.5) * contrast + 0.5;
      b = (b - 0.5) * contrast + 0.5;
      // Saturation boost for acrylic clarity
      const luma = 0.299 * r + 0.587 * g + 0.114 * b;
      r = luma + (r - luma) * sat;
      g = luma + (g - luma) * sat;
      b = luma + (b - luma) * sat;

      rgb[i * 3]     = Math.max(0, Math.min(255, Math.round(r * 255)));
      rgb[i * 3 + 1] = Math.max(0, Math.min(255, Math.round(g * 255)));
      rgb[i * 3 + 2] = Math.max(0, Math.min(255, Math.round(b * 255)));
    }
    return rgb;
  }

  function redrawCropper() {
    if (!uploadFrames.length) return;
    const frame = uploadFrames[uploadAnimIdx % uploadFrames.length];
    const cc = $('cropCanvas');
    const cctx = cc.getContext('2d');
    cctx.fillStyle = '#050811';
    cctx.fillRect(0, 0, 210, 210);

    const side = Math.min(frame.canvas.width, frame.canvas.height) / cropZoom;
    const cx = frame.canvas.width / 2 - cropPanX * (frame.canvas.width / 210);
    const cy = frame.canvas.height / 2 - cropPanY * (frame.canvas.height / 210);
    const sx = Math.max(0, Math.min(frame.canvas.width - side, cx - side / 2));
    const sy = Math.max(0, Math.min(frame.canvas.height - side, cy - side / 2));
    cctx.drawImage(frame.canvas, sx, sy, side, side, 0, 0, 210, 210);

    // Draw subtle 16x16 crop grid overlay
    cctx.strokeStyle = 'rgba(255,255,255,0.08)';
    cctx.lineWidth = 1;
    for (let i = 1; i < 16; i++) {
      const p = (i * 210) / 16;
      cctx.beginPath(); cctx.moveTo(p, 0); cctx.lineTo(p, 210); cctx.stroke();
      cctx.beginPath(); cctx.moveTo(0, p); cctx.lineTo(210, p); cctx.stroke();
    }

    const rgb = extractCropped16(frame.canvas);
    renderAcrylicFrame(rgb, $('uploadPreview16').getContext('2d'), 128);
  }

  function startUploadPreviewLoop() {
    clearTimeout(uploadAnimTimer);
    if (!uploadFrames.length) return;
    const step = () => {
      redrawCropper();
      if (uploadFrames.length > 1) {
        const delay = uploadFrames[uploadAnimIdx % uploadFrames.length].delayMs || 100;
        uploadAnimIdx = (uploadAnimIdx + 1) % uploadFrames.length;
        uploadAnimTimer = setTimeout(step, Math.max(40, delay));
      }
    };
    step();
  }

  $('fileUpload').onchange = async (ev) => {
    const file = ev.target.files[0];
    if (!file) return;
    uploadFrames = [];
    uploadAnimIdx = 0;
    cropPanX = 0; cropPanY = 0; cropZoom = 1.0;
    $('rngZoom').value = '1';
    $('valZoom').textContent = '1.0×';
    $('inpArtName').value = file.name.replace(/\.[^.]+$/, '').slice(0, 24);

    if (file.type === 'image/gif' || file.name.toLowerCase().endsWith('.gif')) {
      try {
        const buf = await file.arrayBuffer();
        const decoded = window.GifDecoder.decodeGif(buf);
        const step = Math.max(1, Math.ceil(decoded.frames.length / 80)); // cap to 80 frames for ESP32 flash
        for (let i = 0; i < decoded.frames.length; i += step) {
          const f = decoded.frames[i];
          const fc = document.createElement('canvas');
          fc.width = f.width; fc.height = f.height;
          const fctx = fc.getContext('2d');
          fctx.putImageData(new ImageData(f.rgba, f.width, f.height), 0, 0);
          uploadFrames.push({ canvas: fc, delayMs: f.delayMs * step });
        }
        $('lblUploadMeta').textContent = `GIF • ${uploadFrames.length} frames`;
      } catch (e) {
        toast('Could not decode GIF');
        return;
      }
    } else {
      const img = new Image();
      img.src = URL.createObjectURL(file);
      await new Promise((res) => { img.onload = res; });
      const fc = document.createElement('canvas');
      fc.width = img.width; fc.height = img.height;
      fc.getContext('2d').drawImage(img, 0, 0);
      uploadFrames.push({ canvas: fc, delayMs: 100 });
      $('lblUploadMeta').textContent = `Image • ${img.width}×${img.height}`;
    }
    startUploadPreviewLoop();
  };

  // Drag-to-pan on crop canvas
  const cropEl = $('cropCanvas');
  cropEl.addEventListener('pointerdown', (e) => {
    draggingCrop = true; dragStartX = e.clientX; dragStartY = e.clientY;
  });
  window.addEventListener('pointermove', (e) => {
    if (!draggingCrop) return;
    cropPanX += (e.clientX - dragStartX) / cropZoom;
    cropPanY += (e.clientY - dragStartY) / cropZoom;
    dragStartX = e.clientX; dragStartY = e.clientY;
    redrawCropper();
  });
  window.addEventListener('pointerup', () => { draggingCrop = false; });

  $('rngZoom').oninput = (e) => {
    cropZoom = parseFloat(e.target.value);
    $('valZoom').textContent = cropZoom.toFixed(2) + '×';
    redrawCropper();
  };
  $('selResize').onchange = redrawCropper;
  $('rngContrast').oninput = redrawCropper;
  $('rngSat').oninput = redrawCropper;

  function encodeMpxBinary(frames) {
    const count = Math.min(frames.length, 100);
    const buf = new Uint8Array(10 + count * 770);
    buf[0] = 77; buf[1] = 80; buf[2] = 88; buf[3] = 49; // "MPX1"
    buf[4] = 16; buf[5] = 16;
    buf[6] = count & 0xFF; buf[7] = (count >> 8) & 0xFF;
    const defDelay = frames[0].delayMs || 100;
    buf[8] = defDelay & 0xFF; buf[9] = (defDelay >> 8) & 0xFF;
    let off = 10;
    for (let i = 0; i < count; i++) {
      const d = Math.max(25, Math.min(5000, Math.round(frames[i].delayMs || 100)));
      buf[off++] = d & 0xFF;
      buf[off++] = (d >> 8) & 0xFF;
      const rgb = extractCropped16(frames[i].canvas);
      buf.set(rgb, off);
      off += 768;
    }
    return buf;
  }

  $('btnUploadArt').onclick = async () => {
    if (!uploadFrames.length) return toast('Choose an image or GIF first');
    const name = ($('inpArtName').value || 'Artwork').trim();
    const id = name.toLowerCase().replace(/[^a-z0-9]+/g, '_').slice(0, 18) + '_' + (Date.now() % 10000);
    const mpx = encodeMpxBinary(uploadFrames);
    await apiPost(`/api/media/upload?id=${encodeURIComponent(id)}&name=${encodeURIComponent(name)}`, mpx, true);
    toast(`Uploaded "${name}" (${uploadFrames.length}f)!`);
    refreshState();
  };

  $('btnSendAlbum').onclick = async () => {
    if (!uploadFrames.length) return toast('Choose an image first');
    const rgb = extractCropped16(uploadFrames[0].canvas);
    await apiPost('/api/album_art', rgb, true);
    await sendControl({ activeMode: 'album_art' });
    toast('Sent to Album Art mode!');
  };

  // ---- Playlists Tab ----
  let editPlaylists = [];
  let selectedPlId = 'default';

  function renderPlaylists() {
    if (!state.playlists) return;
    editPlaylists = JSON.parse(JSON.stringify(state.playlists));
    if (!editPlaylists.find((p) => p.id === selectedPlId) && editPlaylists.length) {
      selectedPlId = state.activePlaylistId || editPlaylists[0].id;
    }
    $('chkPlaylistPlay').checked = !!state.playlistPlaying;

    const sel = $('selPlaylist');
    sel.innerHTML = editPlaylists.map((p) =>
      `<option value="${p.id}" ${p.id === selectedPlId ? 'selected' : ''}>${p.name}</option>`).join('');
    renderPlaylistPages();
  }

  function renderPlaylistPages() {
    const pl = editPlaylists.find((p) => p.id === selectedPlId);
    const container = $('playlistPages');
    container.innerHTML = '';
    if (!pl) return;
    const modes = state.modes || MODES_FALLBACK;

    pl.pages.forEach((pg, idx) => {
      const row = document.createElement('div');
      row.className = 'list-item';
      row.innerHTML = `
        <select class="pg-mode" style="flex:1">
          ${modes.map((m) => `<option value="${m.key}" ${m.key === pg.mode ? 'selected' : ''}>${m.label}</option>`).join('')}
        </select>
        <input class="pg-dur" type="number" min="3" max="3600" value="${pg.durationSec}" style="width:68px" title="Seconds" />
        <span style="font-size:11px;color:var(--muted)">s</span>
        <button class="btn small pg-up">↑</button>
        <button class="btn small danger pg-del">✕</button>`;
      row.querySelector('.pg-mode').onchange = (e) => { pg.mode = e.target.value; };
      row.querySelector('.pg-dur').onchange = (e) => { pg.durationSec = Math.max(3, parseInt(e.target.value, 10) || 15); };
      row.querySelector('.pg-up').onclick = () => {
        if (idx === 0) return;
        [pl.pages[idx - 1], pl.pages[idx]] = [pl.pages[idx], pl.pages[idx - 1]];
        renderPlaylistPages();
      };
      row.querySelector('.pg-del').onclick = () => {
        if (pl.pages.length <= 1) return toast('Keep at least 1 page');
        pl.pages.splice(idx, 1);
        renderPlaylistPages();
      };
      container.appendChild(row);
    });
  }

  $('selPlaylist').onchange = (e) => {
    selectedPlId = e.target.value;
    sendControl({ activePlaylistId: selectedPlId });
    renderPlaylistPages();
  };
  $('chkPlaylistPlay').onchange = (e) => sendControl({ playlistPlaying: e.target.checked, activePlaylistId: selectedPlId });
  $('btnPlNext').onclick = () => sendControl({ playlistAction: 'next' });
  $('btnPlPrev').onclick = () => sendControl({ playlistAction: 'prev' });
  $('btnAddPage').onclick = () => {
    const pl = editPlaylists.find((p) => p.id === selectedPlId);
    if (!pl) return;
    pl.pages.push({ mode: 'clock', durationSec: 15, mediaId: '', sceneVariant: '' });
    renderPlaylistPages();
  };
  $('btnNewPlaylist').onclick = () => {
    const name = prompt('New playlist name:', 'Evening Vibe');
    if (!name) return;
    const id = name.toLowerCase().replace(/[^a-z0-9]+/g, '_') + '_' + (Date.now() % 1000);
    editPlaylists.push({ id, name, pages: [{ mode: 'fireplace', durationSec: 20, mediaId: '', sceneVariant: '' }] });
    selectedPlId = id;
    $('selPlaylist').innerHTML = editPlaylists.map((p) =>
      `<option value="${p.id}" ${p.id === selectedPlId ? 'selected' : ''}>${p.name}</option>`).join('');
    renderPlaylistPages();
  };
  $('btnDelPlaylist').onclick = () => {
    if (editPlaylists.length <= 1) return toast('Cannot delete last playlist');
    editPlaylists = editPlaylists.filter((p) => p.id !== selectedPlId);
    selectedPlId = editPlaylists[0].id;
    apiPost('/api/playlists', { playlists: editPlaylists }).then(() => { toast('Playlist deleted'); refreshState(); });
  };
  $('btnSavePlaylists').onclick = async () => {
    await apiPost('/api/playlists', { playlists: editPlaylists });
    toast('Playlists saved!');
    refreshState();
  };

  // ---- Schedules & Nighttime Dimming Tab ----
  let editSchedules = [];
  const pad2 = (n) => String(n).padStart(2, '0');

  function renderSchedules() {
    if (!state.nightSchedule) return;
    const ns = state.nightSchedule;
    $('chkNightDim').checked = !!ns.dimEnabled;
    $('inpDimStart').value = `${pad2(ns.dimStartHour)}:${pad2(ns.dimStartMinute)}`;
    $('inpDimEnd').value = `${pad2(ns.dimEndHour)}:${pad2(ns.dimEndMinute)}`;
    $('rngDimBright').value = ns.dimBrightness;
    $('valDimBright').textContent = Math.round((ns.dimBrightness / 255) * 100) + '%';

    $('chkAutoOff').checked = !!ns.autoOffEnabled;
    $('inpOffStart').value = `${pad2(ns.offStartHour)}:${pad2(ns.offStartMinute)}`;
    $('inpOffEnd').value = `${pad2(ns.offEndHour)}:${pad2(ns.offEndMinute)}`;
    $('chkWakeMotion').checked = !!ns.wakeOnMotion;

    editSchedules = JSON.parse(JSON.stringify(state.schedules || []));
    drawScheduleList();
  }

  function drawScheduleList() {
    const el = $('scheduleList');
    el.innerHTML = '';
    const days = ['S', 'M', 'T', 'W', 'T', 'F', 'S'];
    editSchedules.forEach((r, idx) => {
      const box = document.createElement('div');
      box.className = 'list-item';
      box.style.flexDirection = 'column';
      box.style.alignItems = 'stretch';
      box.innerHTML = `
        <div class="row" style="margin-bottom:6px">
          <input type="time" class="sc-time" value="${pad2(r.hour)}:${pad2(r.minute)}" style="width:110px" />
          <select class="sc-act" style="flex:1">
            <option value="mode" ${r.action === 'mode' ? 'selected' : ''}>Set Mode</option>
            <option value="playlist" ${r.action === 'playlist' ? 'selected' : ''}>Start Playlist</option>
            <option value="power_on" ${r.action === 'power_on' ? 'selected' : ''}>Power On</option>
            <option value="power_off" ${r.action === 'power_off' ? 'selected' : ''}>Power Off</option>
          </select>
          <input type="text" class="sc-val" value="${r.value || ''}" style="width:105px" placeholder="mode/id" />
          <button class="btn small danger sc-del">✕</button>
        </div>
        <div class="day-pills">
          ${days.map((d, di) => `<button class="day-pill ${(r.daysMask & (1 << di)) ? 'on' : ''}" data-bit="${di}">${d}</button>`).join('')}
        </div>`;
      box.querySelector('.sc-time').onchange = (e) => {
        const [h, m] = e.target.value.split(':').map(Number);
        r.hour = h || 0; r.minute = m || 0;
      };
      box.querySelector('.sc-act').onchange = (e) => { r.action = e.target.value; };
      box.querySelector('.sc-val').onchange = (e) => { r.value = e.target.value; };
      box.querySelector('.sc-del').onclick = () => { editSchedules.splice(idx, 1); drawScheduleList(); };
      box.querySelectorAll('.day-pill').forEach((btn) => {
        btn.onclick = () => {
          const bit = 1 << Number(btn.dataset.bit);
          r.daysMask ^= bit;
          btn.classList.toggle('on', !!(r.daysMask & bit));
        };
      });
      el.appendChild(box);
    });
  }

  $('rngDimBright').oninput = (e) => {
    $('valDimBright').textContent = Math.round((e.target.value / 255) * 100) + '%';
  };
  $('btnAddSchedule').onclick = () => {
    editSchedules.push({ enabled: true, hour: 8, minute: 0, daysMask: 0x7F, action: 'mode', value: 'clock', brightness: 180 });
    drawScheduleList();
  };
  $('btnSaveSchedules').onclick = async () => {
    const [dsh, dsm] = $('inpDimStart').value.split(':').map(Number);
    const [deh, dem] = $('inpDimEnd').value.split(':').map(Number);
    const [osh, osm] = $('inpOffStart').value.split(':').map(Number);
    const [oeh, oem] = $('inpOffEnd').value.split(':').map(Number);
    await apiPost('/api/schedules', {
      nightSchedule: {
        dimEnabled: $('chkNightDim').checked,
        dimStartHour: dsh, dimStartMinute: dsm,
        dimEndHour: deh, dimEndMinute: dem,
        dimBrightness: Number($('rngDimBright').value),
        autoOffEnabled: $('chkAutoOff').checked,
        offStartHour: osh, offStartMinute: osm,
        offEndHour: oeh, offEndMinute: oem,
        wakeOnMotion: $('chkWakeMotion').checked,
        wakeDurationSec: 60
      },
      schedules: editSchedules
    });
    toast('Schedules & night dimming saved!');
    refreshState();
  };

  // ---- Setup & Config Tab ----
  let setupPopulated = false;
  function populateSetupFields() {
    if (!state.network) return;
    if (state.location?.locationLabel) {
      $('lblResolvedLoc').textContent = 'Resolved: ' + state.location.locationLabel;
    }
    if (setupPopulated) return;
    setupPopulated = true;
    const n = state.network, l = state.location || {}, s = state.sensorHw || {}, mh = state.matrixHw || {};
    $('selMatrixRot').value = String(mh.rotation ?? 0);
    $('chkSerpentine').checked = mh.serpentine !== false;
    $('chkFlipX').checked = mh.flipX !== false;
    $('chkFlipY').checked = !!mh.flipY;
    $('inpWifiSsid').value = n.wifiSsid || '';
    $('inpWifiPass').placeholder = n.wifiHasPassword ? '•••••••• (saved — leave blank to keep)' : 'Wi-Fi password';
    $('inpHostname').value = n.hostname || 'matrix-acrylic';
    $('chkHaEnabled').checked = !!n.haEnabled;
    $('inpHaUrl').value = n.haUrl || 'http://homeassistant.local:8123';
    $('inpMqttHost').value = n.mqttHost || 'homeassistant.local';
    $('inpMqttPort').value = n.mqttPort || 1883;
    $('inpHaWeather').value = n.haWeatherEntity || 'weather.home';
    $('inpHaMedia').value = n.haMediaEntity || 'media_player.living_room';
    $('inpHaPrinterProg').value = n.haPrinterProgressEntity || 'sensor.3d_printer_progress';
    $('inpHaPrinterState').value = n.haPrinterStateEntity || 'sensor.3d_printer_state';
    $('inpHaTemp').value = n.haTempEntity || 'sensor.living_room_temperature';
    $('inpHaAqi').value = n.haAqiEntity || 'sensor.living_room_aqi';
    $('inpZip').value = l.zipCode || '94103';
    const tzVal = String(l.tzOffsetMinutes ?? -480);
    const tzSel = $('selTzOffset');
    if ([...tzSel.options].some((o) => o.value === tzVal)) {
      tzSel.value = tzVal;
    }
    $('selUnits').value = l.useFahrenheit === false ? 'C' : 'F';
    $('selAudioSrc').value = s.audioSource || 'i2s';
    $('selMmwave').value = s.mmwaveModel || 'mr60bha1';
    $('selEnvSensor').value = s.envSensorModel || 'bme680';
    $('inpCamUrl').value = s.cameraUrl || '';
  }

  $('btnAutoTz').onclick = () => {
    const off = -new Date().getTimezoneOffset();
    const tzSel = $('selTzOffset');
    let bestOpt = tzSel.options[0];
    let bestDiff = 1e9;
    for (const o of tzSel.options) {
      const d = Math.abs(Number(o.value) - off);
      if (d < bestDiff) { bestDiff = d; bestOpt = o; }
    }
    tzSel.value = bestOpt.value;
    toast('Timezone matched to phone (' + bestOpt.textContent + ')');
  };

  async function saveAllConfig(wifiOnly = false) {
    const netPayload = {
      wifiSsid: $('inpWifiSsid').value.trim(),
      hostname: $('inpHostname').value.trim() || 'matrix-acrylic'
    };
    const typedPass = $('inpWifiPass').value;
    if (typedPass.length > 0) {
      netPayload.wifiPassword = typedPass;
    }

    const payload = { network: netPayload };
    if (!wifiOnly) {
      Object.assign(netPayload, {
        haEnabled: $('chkHaEnabled').checked,
        haUrl: $('inpHaUrl').value.trim(),
        haToken: $('inpHaToken').value.trim(),
        mqttEnabled: $('chkHaEnabled').checked,
        mqttHost: $('inpMqttHost').value.trim(),
        mqttPort: Number($('inpMqttPort').value) || 1883,
        haWeatherEntity: $('inpHaWeather').value.trim(),
        haMediaEntity: $('inpHaMedia').value.trim(),
        haPrinterProgressEntity: $('inpHaPrinterProg').value.trim(),
        haPrinterStateEntity: $('inpHaPrinterState').value.trim(),
        haTempEntity: $('inpHaTemp').value.trim(),
        haAqiEntity: $('inpHaAqi').value.trim()
      });
      payload.location = {
        zipCode: $('inpZip').value.trim(),
        tzOffsetMinutes: parseInt($('selTzOffset').value, 10),
        useFahrenheit: $('selUnits').value === 'F'
      };
      payload.sensorHw = {
        audioSource: $('selAudioSrc').value,
        mmwaveModel: $('selMmwave').value,
        envSensorModel: $('selEnvSensor').value,
        cameraUrl: $('inpCamUrl').value.trim()
      };
      payload.matrixHw = {
        rotation: Number($('selMatrixRot').value) || 0,
        serpentine: $('chkSerpentine').checked,
        flipX: $('chkFlipX').checked,
        flipY: $('chkFlipY').checked
      };
    }

    const res = await apiPost('/api/config', payload);
    $('inpWifiPass').value = '';
    if (res && res.reconnecting) {
      toast(`Saved! Connecting to "${netPayload.wifiSsid}"...`);
    } else {
      toast('Configuration saved!');
    }
    setTimeout(refreshState, 800);
  }

  $('btnSaveWifi').onclick = () => saveAllConfig(true);
  $('btnSaveConfig').onclick = () => saveAllConfig(false);

  // ---- Live Sensor / HA Simulator Controls ----
  $('simPrinterProg').oninput = (e) => { $('valSimProg').textContent = e.target.value + '%'; };
  $('simBpm').oninput = (e) => { $('valSimBpm').textContent = e.target.value; };
  $('simAqi').oninput = (e) => { $('valSimAqi').textContent = e.target.value; };

  $('btnSimApply').onclick = async () => {
    await apiPost('/api/sensors/inject', {
      weatherCondition: Number($('simWeatherCond').value),
      weatherTempC: parseFloat($('simWeatherTemp').value) || 21,
      printerState: Number($('simPrinterState').value),
      printerProgress: Number($('simPrinterProg').value),
      heartRateBpm: Number($('simBpm').value),
      aqi: Number($('simAqi').value)
    });
    toast('Live sensor / HA data updated!');
    refreshState();
  };
  $('btnSimMotion').onclick = async () => {
    await apiPost('/api/sensors/inject', { motionDetected: true, motionX: Math.random(), motionY: Math.random() });
    toast('Motion triggered!');
  };

  // ---- Global Control Event Listeners ----
  $('btnPower').onclick = () => sendControl({ powerOn: !state.powerOn });
  $('rngBright').oninput = (e) => { $('valBright').textContent = Math.round((e.target.value / 255) * 100) + '%'; };
  $('rngBright').onchange = (e) => sendControl({ brightness: Number(e.target.value) });
  $('rngSpeed').oninput = (e) => { $('valSpeed').textContent = Number(e.target.value).toFixed(2) + '×'; };
  $('rngSpeed').onchange = (e) => sendControl({ animationSpeed: parseFloat(e.target.value) });
  $('colPrimary').onchange = (e) => sendControl({ primaryColor: intFromHex(e.target.value) });
  $('colAccent').onchange = (e) => sendControl({ accentColor: intFromHex(e.target.value) });
  $('selTransition').onchange = (e) => sendControl({ transition: e.target.value });

  // Bottom Navigation Tabs
  document.querySelectorAll('.nav-btn').forEach((btn) => {
    btn.onclick = () => {
      document.querySelectorAll('.nav-btn').forEach((b) => b.classList.remove('active'));
      document.querySelectorAll('.tab-panel').forEach((p) => p.classList.remove('active'));
      btn.classList.add('active');
      $('tab-' + btn.dataset.tab).classList.add('active');
    };
  });

  // Register offline PWA service worker
  if ('serviceWorker' in navigator) {
    navigator.serviceWorker.register('/sw.js').catch(() => {});
  }

  refreshState();
  connectWs();
  setInterval(refreshState, 5000);
})();
