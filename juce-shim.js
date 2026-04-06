// juce-shim.js — Drop-in replacement for window.__JUCE__.backend
// Bridges rev2-panel.html events to the Pi's midi_router.py via WebSocket.
// Audio plays via WebAudio BufferSource scheduling.

(function () {
  'use strict';

  // ── Config ──
  // Set by browser/index.html before this script loads
  const config = window.__ANARACK_CONFIG || {};
  const WS_HOST = config.host || location.hostname + ':8765';
  // Use wss for remote hosts, ws for local (localhost/LAN IPs)
  const hostname = WS_HOST.split(':')[0];
  const isLocal = /^(localhost|127\.\d+\.\d+\.\d+|192\.168\.\d+\.\d+|10\.\d+\.\d+\.\d+|::1)$/.test(hostname);
  const WS_PROTO = isLocal ? 'ws' : 'wss';

  // ── State ──
  let midiWs = null;
  let audioWs = null;
  let audioCtx = null;
  let audioGen = 0; // generation counter to invalidate stale audio callbacks
  let nextPlayTime = 0;
  let audioStarted = false;
  const listeners = {};
  let connected = false;
  let connState = 0; // 0=disconnected, 1=connecting, 2=connected, 3=offline
  let statusInterval = null;
  let connectTimeout = null;

  // ── Latency measurement state ──
  let latencyMeasuring = false;
  let latencySendTime = 0;
  let latencyResults = [];
  let latencyDoneCallback = null;
  const LATENCY_PEAK_THRESHOLD = 800; // int16 amplitude — synth attack
  const LATENCY_TEST_COUNT = 5;
  const LATENCY_TIMEOUT_MS = 1000;
  const LATENCY_GAP_MS = 350;

  // ── Event system (matches JUCE backend API) ──
  function addEventListener(event, callback) {
    if (!listeners[event]) listeners[event] = [];
    listeners[event].push(callback);
  }

  function _dispatch(event, data) {
    const cbs = listeners[event];
    if (cbs) cbs.forEach(cb => { try { cb(data); } catch (e) { console.error(event, e); } });
  }

  // ── MIDI WebSocket ──
  function connectMidi(host) {
    if (midiWs) { midiWs.close(); midiWs = null; }
    connState = 1;
    _broadcastStatus();

    const url = `${WS_PROTO}://${host}`;
    midiWs = new WebSocket(url);

    midiWs.onopen = () => {
      // Mark as connecting — will go "live" once audio arrives
      connState = 1;
      _broadcastStatus();
      // Set timeout: if no audio/data after 10s, show offline
      if (connectTimeout) clearTimeout(connectTimeout);
      connectTimeout = setTimeout(() => {
        if (!connected) {
          connState = 3; // offline
          _broadcastStatus();
        }
      }, 10000);
    };

    midiWs.onmessage = (e) => {
      if (typeof e.data !== 'string') return;
      try {
        const msg = JSON.parse(e.data);
        if (msg.type === 'cc') {
          _dispatch('paramUpdate', { cc: msg.cc, value: msg.value });
          if (!connected) { connected = true; connState = 2; if (connectTimeout) clearTimeout(connectTimeout); _broadcastStatus(); }
        } else if (msg.type === 'patchName') {
          _lastPatchName = msg.name;
          if (!connected) { connected = true; connState = 2; if (connectTimeout) clearTimeout(connectTimeout); _broadcastStatus(); }
        } else if (msg.type === 'programChange') {
          _dispatch('paramUpdate', { cc: 120, value: msg.program });
          if (msg.bank !== undefined)
            _dispatch('paramUpdate', { cc: 121, value: msg.bank });
        }
      } catch (err) { /* ignore non-JSON */ }
    };

    midiWs.onclose = () => {
      connected = false;
      connState = 0;
      midiWs = null;
      _broadcastStatus();
    };

    midiWs.onerror = () => {
      connected = false;
      connState = 3; // offline — couldn't reach relay/Pi
      if (connectTimeout) clearTimeout(connectTimeout);
      _broadcastStatus();
    };
  }

  function disconnectMidi() {
    if (midiWs) { midiWs.close(); midiWs = null; }
    connected = false;
    connState = 0;
    _broadcastStatus();
  }

  function sendMidiMsg(status, d1, d2) {
    if (!midiWs || midiWs.readyState !== WebSocket.OPEN) return;
    const msg = { status, data1: d1 };
    if (d2 !== undefined) msg.data2 = d2;
    midiWs.send(JSON.stringify(msg));
  }

  // ── Audio WebSocket + WebAudio playback ──
  function connectAudio(host) {
    if (audioWs) { audioWs.close(); audioWs = null; }
    const gen = ++audioGen; // invalidate any stale callbacks

    if (!audioCtx) {
      // Use parent's AudioContext if available (created on user gesture for iOS)
      try { audioCtx = window.parent.__ANARACK_AUDIO_CTX; } catch(e) {}
      if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)({
          sampleRate: 48000,
          latencyHint: 'interactive'
        });
      }
    }
    nextPlayTime = 0;
    audioStarted = false;

    const url = `${WS_PROTO}://${host}/audio`;
    audioWs = new WebSocket(url);
    audioWs.binaryType = 'arraybuffer';

    audioWs.onopen = () => {
      // Resume AudioContext (may be suspended in iframe)
      if (audioCtx && audioCtx.state === 'suspended') audioCtx.resume();
    };

    audioWs.onmessage = (e) => {
      if (gen !== audioGen) return; // stale socket, discard
      if (!(e.data instanceof ArrayBuffer) || !audioCtx) return;
      if (e.data.byteLength < 2 || e.data.byteLength % 2 !== 0) return; // invalid PCM
      // First audio packet = Pi is streaming, go live
      if (!connected) { connected = true; connState = 2; if (connectTimeout) clearTimeout(connectTimeout); _broadcastStatus(); }
      const int16 = new Int16Array(e.data);

      // Latency measurement: detect synth attack peak
      if (latencyMeasuring) {
        let peak = 0;
        for (let i = 0; i < int16.length; i++) {
          const a = Math.abs(int16[i]);
          if (a > peak) peak = a;
        }
        if (peak > LATENCY_PEAK_THRESHOLD) {
          const ms = performance.now() - latencySendTime;
          latencyMeasuring = false;
          latencyResults.push(ms);
          if (latencyDoneCallback) latencyDoneCallback({ type: 'progress', n: latencyResults.length, ms });
          setTimeout(() => _runLatencyStep(), LATENCY_GAP_MS);
        }
      }
      const float32 = new Float32Array(int16.length);
      for (let i = 0; i < int16.length; i++) float32[i] = int16[i] / 32768;

      const buf = audioCtx.createBuffer(1, float32.length, 48000);
      buf.getChannelData(0).set(float32);
      const src = audioCtx.createBufferSource();
      src.buffer = buf;
      src.connect(audioCtx.destination);

      const now = audioCtx.currentTime;
      const BUFFER_AHEAD = 0.05; // 50ms scheduling buffer
      if (!audioStarted || nextPlayTime < now) {
        nextPlayTime = now + BUFFER_AHEAD;
        audioStarted = true;
      }
      // Only clamp on extreme runaway (> 1s buffered)
      if (nextPlayTime > now + 1.0) nextPlayTime = now + BUFFER_AHEAD;
      src.start(nextPlayTime);
      nextPlayTime += buf.duration;
    };

    audioWs.onclose = () => { audioWs = null; };
  }

  // ── Latency test ──
  function measureLatency(callback) {
    if (!connected || !midiWs || midiWs.readyState !== WebSocket.OPEN) {
      if (callback) callback({ type: 'error', msg: 'Not connected' });
      return;
    }
    latencyResults = [];
    latencyDoneCallback = callback;
    if (callback) callback({ type: 'start' });
    _runLatencyStep();
  }

  function _runLatencyStep() {
    if (latencyResults.length >= LATENCY_TEST_COUNT) {
      const sum = latencyResults.reduce((a, b) => a + b, 0);
      const avg = sum / latencyResults.length;
      const min = Math.min.apply(null, latencyResults);
      const max = Math.max.apply(null, latencyResults);
      if (latencyDoneCallback) {
        latencyDoneCallback({
          type: 'done',
          avg, min, max,
          results: latencyResults.slice()
        });
      }
      return;
    }
    latencyMeasuring = true;
    latencySendTime = performance.now();
    // Note On C4 at velocity 127
    sendMidiMsg(0x90, 60, 127);
    setTimeout(() => sendMidiMsg(0x80, 60, 0), 100);
    // Timeout: if no peak detected, count as failed and move on
    setTimeout(() => {
      if (latencyMeasuring) {
        latencyMeasuring = false;
        if (latencyDoneCallback) {
          latencyDoneCallback({ type: 'progress', n: latencyResults.length + 1, ms: null, failed: true });
        }
        latencyResults.push(LATENCY_TIMEOUT_MS); // worst case
        setTimeout(() => _runLatencyStep(), LATENCY_GAP_MS);
      }
    }, LATENCY_TIMEOUT_MS);
  }

  function disconnectAudio() {
    if (audioWs) { audioWs.close(); audioWs = null; }
    audioGen++;
    nextPlayTime = 0;
    audioStarted = false;
  }

  // ── Status broadcast (replaces C++ timerCallback at 10Hz) ──
  let _lastPatchName = '';

  function _broadcastStatus() {
    _dispatch('connectionStatus', {
      connected,
      connState,
      mode: connected ? 'WebSocket' : '',
      pktSize: 0,
      rtt: 0,
      bufferMs: 0,
      bufferTarget: -1,
      totalLatency: 0,
      midiIn: 0,
      mappedSends: 0,
      learning: false,
      autoBuffer: 0,
      blockSize: 128,
      asrcDrops: 0,
      asrcDups: 0,
      plcSamples: 0,
      pktLost: 0,
      pktRecv: 0,
      pktDup: 0,
      patchName: _lastPatchName || undefined
    });
  }

  // ── Event handlers (UI → server) ──
  function emitEvent(event, payload) {
    switch (event) {
      case 'ccChange': {
        const cc = payload.cc;
        const val = payload.value;
        if (cc === 120) {
          // Program change
          sendMidiMsg(0xC0, val);
        } else if (cc === 121) {
          // Bank select
          sendMidiMsg(0xB0, 0x00, val);
        } else {
          sendMidiMsg(0xB0, cc, val);
        }
        break;
      }
      case 'nrpnChange': {
        // Send 4 CC messages for NRPN
        const nrpn = payload.nrpn;
        const val = payload.value;
        sendMidiMsg(0xB0, 99, nrpn >> 7);
        sendMidiMsg(0xB0, 98, nrpn & 0x7F);
        sendMidiMsg(0xB0, 6, val);
        sendMidiMsg(0xB0, 38, 0);
        break;
      }
      case 'doConnect': {
        const host = payload.host || WS_HOST;
        // Resume AudioContext on user gesture
        if (audioCtx && audioCtx.state === 'suspended') audioCtx.resume();
        connectMidi(host);
        connectAudio(host);
        break;
      }
      case 'doDisconnect':
        disconnectMidi();
        disconnectAudio();
        break;
      case 'doPing':
        _dispatch('logMessage', { msg: 'Ping not available in browser mode' });
        break;
      case 'setBuffer':
      case 'setSensitivity':
      case 'openMidiInput':
        // These are plugin-only features, no-op in browser
        break;
      case 'startLearn':
      case 'cancelLearn':
        // MIDI learn not supported in browser mode (no DAW MIDI input)
        break;
      default:
        console.log('Unhandled shim event:', event, payload);
    }
  }

  // ── Install the shim as window.__JUCE__.backend ──
  window.__JUCE__ = {
    backend: {
      emitEvent,
      addEventListener,
      // Exposed for keyboard and index.html
      _listeners: listeners,
      _dispatch,
      _sendMidi: sendMidiMsg,
      _measureLatency: measureLatency
    },
    initialisationData: {
      config: {
        host: WS_HOST,
        lan: true, // browser connects like LAN (no WG)
        bufferMs: 0,
        version: 'browser',
        build: 0
      }
    }
  };

  // Auto-fire initConfig after a short delay (gives panel time to register listeners)
  setTimeout(() => {
    _dispatch('initConfig', {
      host: WS_HOST,
      lan: true,
      bufferMs: 0,
      version: 'browser',
      build: 0,
      midiInputs: []
    });

    // Auto-connect after initConfig (user already clicked the audio gate)
    if (config.autoConnect !== false) {
      setTimeout(() => {
        connectMidi(WS_HOST);
        connectAudio(WS_HOST);
      }, 200);
    }
  }, 600);

  // Resume AudioContext on any user interaction (mobile requires gesture)
  function resumeAudio() {
    if (audioCtx && audioCtx.state === 'suspended') {
      audioCtx.resume();
    }
  }
  document.addEventListener('click', resumeAudio);
  document.addEventListener('touchstart', resumeAudio);
  document.addEventListener('keydown', resumeAudio);

  // Start status broadcast at ~10Hz once connected
  statusInterval = setInterval(() => {
    if (connected) _broadcastStatus();
  }, 100);

})();
