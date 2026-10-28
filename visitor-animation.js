/* Render the same animation headers as the ESP32 on the visitor's device.
   MQTT status sets the clock; LED frames no longer have to cross the network. */
(function () {
  const GRID_BYTES = 2816 * 3;
  let ex = null, status = null, received = 0, anchor = 0, scene = "", lastDraw = 0;
  const imports = {
    wasi_snapshot_preview1: new Proxy({}, { get: () => () => 0 }),
    env: { expf: Math.exp, sinf: Math.sin, sqrtf: Math.sqrt, cosf: Math.cos,
      powf: Math.pow, fmodf: (a,b) => a % b, floorf: Math.floor,
      fabsf: Math.abs, fminf: Math.min, hypotf: Math.hypot,
      tanhf: Math.tanh, ceilf: Math.ceil }
  };
  const max2 = (g, offset, width, x, y) => {
    const p = offset + (y * width + x) * 3;
    return [Math.max(g[p], g[p+3], g[p+width*3], g[p+width*3+3]),
      Math.max(g[p+1], g[p+4], g[p+width*3+1], g[p+width*3+4]),
      Math.max(g[p+2], g[p+5], g[p+width*3+2], g[p+width*3+5])];
  };
  const draw = now => {
    requestAnimationFrame(draw);
    if (!ex || !status || now - lastDraw < 100) return;
    lastDraw = now;
    const elapsed = Math.max(0, anchor + (now - received) / 1000);
    if (status.s === "tour" && elapsed < ex.tourEnd()) ex.tour(elapsed);
    else if (status.s === "focus" && status.b >= 1 && status.b <= 4 && elapsed < ex.focusEnd(status.b - 1)) ex.focus(status.b - 1, elapsed);
    else ex.idle(elapsed % 60);
    const g = new Uint8Array(ex.memory.buffer, ex.grid(), GRID_BYTES);
    document.querySelectorAll("canvas[data-part]").forEach(c => {
      const r = c.getBoundingClientRect();
      if (!r.height || r.bottom < 0 || r.top > window.innerHeight) return;
      const o = (+c.dataset.box * 2 + +c.dataset.part) * 256 * 3;
      D2DLive.paint(c, 8, 8, (x,y) => max2(g, o, 16, x*2, y*2));
    });
    document.querySelectorAll("canvas[data-bridge]").forEach(c => {
      const r = c.getBoundingClientRect();
      if (!r.height || r.bottom < 0 || r.top > window.innerHeight) return;
      const o = (8 + +c.dataset.bridge) * 256 * 3;
      D2DLive.paint(c, 4, 16, (x,y) => max2(g, o, 32, y*2, x*2));
    });
  };
  window.D2DVisitor = {
    async init() {
      try {
        if (!window.D2D_ANIM) return false;
        const raw = atob(window.D2D_ANIM.wasm), bytes = new Uint8Array(raw.length);
        for (let i = 0; i < raw.length; i++) bytes[i] = raw.charCodeAt(i);
        const { instance } = await WebAssembly.instantiate(bytes, imports);
        ex = instance.exports;
        if (ex._initialize) ex._initialize();
        D2DLive.useLocal();
        status = { s: "idle", b: 0 };
        received = performance.now();
        requestAnimationFrame(draw);
        return true;
      } catch (e) { console.warn("Local animation unavailable; using live frames", e); return false; }
    },
    update(st) {
      if (!ex || !st || !Number.isFinite(+st.p)) return;
      if (st.retained && (!st.ms || Math.abs(Date.now() - st.ms) > 10000)) return;
      const key = st.s + ":" + st.b;
      const offset = st.ms > 0 && Math.abs(Date.now() - st.ms) < 5000 ? (Date.now() - st.ms) / 1000 : 0;
      const next = Math.max(0, +st.p + offset);
      const predicted = anchor + (performance.now() - received) / 1000;
      if (key !== scene || Math.abs(next - predicted) > 0.75) { anchor = next; received = performance.now(); }
      status = st; scene = key;
      D2DLive.localStatus();
    }
  };
})();
