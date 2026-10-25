/* Inside a Drop of Saliva — live view of the LED boxes
   The ESP32 sends a small copy of the LEDs (4 frames/s) to <TOPIC>/live while a page asks for it
   on <TOPIC>/watch. Frame: 1 version byte, then RGB for
     Box 1–4: under 8x8, back 8x8   (each 16x16 panel shrunk 2x)
     Bridge 1–3: 4 rows x 16        (x 0 = end next to Box n)
   Looks like the real thing:
     backlight  → has a diffuser film: one soft, even glow, no single LEDs visible
     under-light and bridges → no diffuser: separate, sharp LED dots
   Used by index.html (visitors) and dev.html (developer). */
(function () {
  const NAVY = "#00004d";
  const views = [];
  let frame = null, frameAt = 0, client = null, topic = "", timer = null;

  // LED values are dim by design; lift them a little so they read on a screen (gamma 0.6)
  const lift = new Uint8Array(256);
  for (let i = 0; i < 256; i++) lift[i] = Math.round(255 * Math.pow(i / 255, 0.6));

  const pixel = i => {
    if (!frame) return null;
    const o = 1 + i * 3;
    return [frame[o], frame[o + 1], frame[o + 2]];
  };

  const size = (canvas, cols, rows) => {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = canvas.clientWidth || cols * 12, h = canvas.clientHeight || rows * 12;
    if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
      canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr);
    }
    return { w: canvas.width, h: canvas.height };
  };

  // ---- no diffuser: every LED is its own small sharp dot ----
  const drawDots = (canvas, cols, rows, get, on = !!frame) => {
    const { w, h } = size(canvas, cols, rows);
    const ctx = canvas.getContext("2d");
    const cw = w / cols, ch = h / rows, r = Math.min(cw, ch);
    ctx.globalCompositeOperation = "source-over";
    ctx.fillStyle = NAVY; ctx.fillRect(0, 0, w, h);
    ctx.fillStyle = "rgba(255,255,255,0.07)";                       // unlit LEDs
    for (let y = 0; y < rows; y++) for (let x = 0; x < cols; x++) {
      ctx.beginPath(); ctx.arc((x + .5) * cw, (y + .5) * ch, r * 0.2, 0, 6.2832); ctx.fill();
    }
    if (!on) return;
    ctx.globalCompositeOperation = "lighter";
    for (let y = 0; y < rows; y++) for (let x = 0; x < cols; x++) {
      const c = get(x, y);
      if (!c || (c[0] < 3 && c[1] < 3 && c[2] < 3)) continue;
      const R = lift[c[0]], G = lift[c[1]], B = lift[c[2]];
      const cx = (x + .5) * cw, cy = (y + .5) * ch;
      const g = ctx.createRadialGradient(cx, cy, 0, cx, cy, r * 0.75);   // small halo only
      g.addColorStop(0, `rgba(${R},${G},${B},0.55)`);
      g.addColorStop(1, `rgba(${R},${G},${B},0)`);
      ctx.fillStyle = g; ctx.fillRect(cx - r, cy - r, r * 2, r * 2);
      ctx.fillStyle = `rgb(${R},${G},${B})`;                       // the LED itself
      ctx.beginPath(); ctx.arc(cx, cy, r * 0.27, 0, 6.2832); ctx.fill();
    }
  };

  // ---- diffuser film: the LEDs melt into one smooth glow ----
  const small = document.createElement("canvas");                  // the frame at LED size
  const mid = document.createElement("canvas");                    // 4x bigger, smoothed once more
  const drawDiffused = (canvas, cols, rows, get, on = !!frame) => {
    const { w, h } = size(canvas, cols, rows);
    const ctx = canvas.getContext("2d");
    ctx.globalCompositeOperation = "source-over";
    ctx.fillStyle = NAVY; ctx.fillRect(0, 0, w, h);
    if (!on) return;
    small.width = cols + 2; small.height = rows + 2;               // 1-LED dark border so the edge fades like the film
    const sctx = small.getContext("2d");
    const img = sctx.createImageData(cols + 2, rows + 2);
    for (let y = 0; y < rows; y++) for (let x = 0; x < cols; x++) {
      const c = get(x, y) || [0, 0, 0], o = ((y + 1) * (cols + 2) + x + 1) * 4;
      img.data[o] = lift[c[0]]; img.data[o + 1] = lift[c[1]]; img.data[o + 2] = lift[c[2]]; img.data[o + 3] = 255;
    }
    for (let i = 3; i < img.data.length; i += 4) img.data[i] = 255;
    sctx.putImageData(img, 0, 0);
    mid.width = (cols + 2) * 4; mid.height = (rows + 2) * 4;
    const mctx = mid.getContext("2d");
    mctx.imageSmoothingEnabled = true; mctx.imageSmoothingQuality = "high";
    mctx.drawImage(small, 0, 0, mid.width, mid.height);
    ctx.imageSmoothingEnabled = true; ctx.imageSmoothingQuality = "high";
    const cw = w / cols, ch = h / rows;
    ctx.globalCompositeOperation = "lighter";
    // the border LED is outside the panel: place the picture so LED centres line up with the canvas grid
    const W = w + 2 * cw, H = h + 2 * ch;
    if ("filter" in ctx) ctx.filter = `blur(${Math.round(cw * 0.3)}px)`;   // smooths the last bit of LED grid (ignored by older browsers)
    ctx.drawImage(mid, -cw, -ch, W, H);
    ctx.filter = "none";
    ctx.globalAlpha = 0.4;                                         // a softer, wider copy = light spreading in the film
    ctx.drawImage(mid, (w - W * 1.12) / 2, (h - H * 1.12) / 2, W * 1.12, H * 1.12);
    ctx.globalAlpha = 1;
  };

  const draw = () => {
    for (const v of views) {
      if (v.kind === "panel") {
        const base = v.box * 128 + v.part * 64;
        (v.diffuse ? drawDiffused : drawDots)(v.canvas, 8, 8, (x, y) => pixel(base + y * 8 + x));
      } else {
        const base = 512 + v.bridge * 64;
        if (v.vertical) drawDots(v.canvas, 4, 16, (c, r) => pixel(base + c * 16 + r));   // Box n at the top
        else drawDots(v.canvas, 16, 4, (c, r) => pixel(base + r * 16 + c));            // Box n on the left
      }
      v.canvas.classList.toggle("live-stale", !isLive());
    }
  };

  const isLive = () => !!frame && Date.now() - frameAt < 3000;

  const askToWatch = () => {
    if (client && client.connected && document.visibilityState === "visible") client.publish(topic + "/watch", "1");
  };

  window.D2DLive = {
    // box 0–3, part 0 = under-light (no diffuser), 1 = backlight (diffuser film)
    addPanel(canvas, box, part) { views.push({ kind: "panel", canvas, box, part, diffuse: part === 1 }); draw(); },
    // bridge 0–2; vertical = Box n on top, Box n+1 below
    addBridge(canvas, bridge, vertical) { views.push({ kind: "bridge", canvas, bridge, vertical: !!vertical }); draw(); },
    isLive,
    // draw any LED grid the same way (used by the simulator): get(x, y) → [r, g, b]
    paint(canvas, cols, rows, get, diffuse) { (diffuse ? drawDiffused : drawDots)(canvas, cols, rows, get, true); },
    clear() { views.length = 0; },
    // for previews/tests: show a frame without the ESP32
    show(bytes) { frame = bytes; frameAt = Date.now(); draw(); },
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
