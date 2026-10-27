/* Inside a Drop of Saliva — live view of the LED boxes
   The ESP32 sends a small copy of the LEDs (4 frames/s) to <TOPIC>/live while a page asks for it
   on <TOPIC>/watch. Frame: 1 version byte, then RGB for
     Box 1–4: under 8x8, back 8x8   (each 16x16 panel shrunk 2x)
     Bridge 1–3: 4 rows x 16        (x 0 = end next to Box n)
   Used by index.html (visitors) and dev.html (developer). */
(function () {
  const NAVY = "#00004d";
  const views = [];
  let frame = null, frameAt = 0, client = null, topic = "", timer = null;

  // LED values are dim by design; lift them a little so they read on a screen (gamma 0.6)
  const lift = new Uint8Array(256);
  for (let i = 0; i < 256; i++) lift[i] = Math.round(255 * Math.pow(i / 255, 0.6));

  function pixel(i) {
    if (!frame) return null;
    const o = 1 + i * 3;
    return [frame[o], frame[o + 1], frame[o + 2]];
  }

  function size(canvas, cols, rows) {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = canvas.clientWidth || cols * 12, h = canvas.clientHeight || rows * 12;
    if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
      canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr);
    }
    return { w: canvas.width, h: canvas.height };
  }

  // draw a grid of LEDs; get(col,row) returns [r,g,b] or null
  function drawGrid(canvas, cols, rows, get, externalFrame) {
    const { w, h } = size(canvas, cols, rows);
    const ctx = canvas.getContext("2d");
    const cw = w / cols, ch = h / rows, r = Math.min(cw, ch) * 0.36;
    ctx.globalCompositeOperation = "source-over";
    ctx.fillStyle = NAVY; ctx.fillRect(0, 0, w, h);
    // unlit LEDs: faint dots
    ctx.fillStyle = "rgba(255,255,255,0.05)";
    for (let y = 0; y < rows; y++) for (let x = 0; x < cols; x++) {
      ctx.beginPath(); ctx.arc((x + .5) * cw, (y + .5) * ch, r * 0.55, 0, 6.2832); ctx.fill();
    }
    if (!frame && !externalFrame) return;
    ctx.globalCompositeOperation = "lighter";
    for (let y = 0; y < rows; y++) for (let x = 0; x < cols; x++) {
      const c = get(x, y);
      if (!c || (c[0] < 3 && c[1] < 3 && c[2] < 3)) continue;
      const R = lift[c[0]], G = lift[c[1]], B = lift[c[2]];
      const cx = (x + .5) * cw, cy = (y + .5) * ch;
      const g = ctx.createRadialGradient(cx, cy, 0, cx, cy, r * 2.6);
      g.addColorStop(0, `rgba(${R},${G},${B},0.95)`);
      g.addColorStop(0.35, `rgba(${R},${G},${B},0.55)`);
      g.addColorStop(1, `rgba(${R},${G},${B},0)`);
      ctx.fillStyle = g;
      ctx.fillRect(cx - r * 2.6, cy - r * 2.6, r * 5.2, r * 5.2);
    }
  }

  function draw() {
    for (const v of views) {
      if (v.kind === "panel") {
        const base = v.box * 128 + v.part * 64;
        drawGrid(v.canvas, 8, 8, (x, y) => pixel(base + y * 8 + x));
      } else {
        const base = 512 + v.bridge * 64;
        if (v.vertical) drawGrid(v.canvas, 4, 16, (c, r) => pixel(base + c * 16 + r));   // Box n at the top
        else drawGrid(v.canvas, 16, 4, (c, r) => pixel(base + r * 16 + c));            // Box n on the left
      }
      v.canvas.classList.toggle("live-stale", !isLive());
    }
  }

  function isLive() { return !!frame && Date.now() - frameAt < 3000; }

  function askToWatch() {
    if (client && client.connected && document.visibilityState === "visible") client.publish(topic + "/watch", "1");
  }

  window.D2DLive = {
    // Draw an animation frame supplied by the offline simulator.
    paint(canvas, cols, rows, get) { drawGrid(canvas, cols, rows, get, true); },
    // box 0–3, part 0 = under-light, 1 = back
    addPanel(canvas, box, part) { views.push({ kind: "panel", canvas, box, part }); draw(); },
    // bridge 0–2; vertical = Box n on top, Box n+1 below
    addBridge(canvas, bridge, vertical) { views.push({ kind: "bridge", canvas, bridge, vertical: !!vertical }); draw(); },
    isLive,
    clear() { views.length = 0; },
    // call once the MQTT client exists
    start(mqttClient, mqttTopic) {
      client = mqttClient; topic = mqttTopic;
      client.on("connect", () => { client.subscribe(topic + "/live"); askToWatch(); });
      client.on("message", (t, payload) => {
        if (!t.endsWith("/live") || payload.length < 1 + 704 * 3 || payload[0] !== 1) return;
        frame = new Uint8Array(payload); frameAt = Date.now(); draw();
      });
      document.addEventListener("visibilitychange", askToWatch);
      clearInterval(timer);
      timer = setInterval(() => { askToWatch(); if (!isLive()) draw(); }, 15000);
      setInterval(() => { if (!isLive()) draw(); }, 2000);
      window.addEventListener("resize", draw);
    },
  };
})();
