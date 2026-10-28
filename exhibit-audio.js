/* Exhibition soundtrack for the dedicated computer connected to the JBL.
   The fixed MP3 is downloaded and decoded before playback; LED status only selects its position. */
window.D2DExhibitAudio = (() => {
  const FILE = "audio/journey.mp3?v=20261028-full135";
  let ctx = null, buffer = null, source = null, loading = false, ready = false, error = "";
  let enabled = false, ref = null, scene = "", startPos = 0, startTime = 0;
  let onChange = () => {};
  const notify = () => onChange({ loading, ready, error, enabled, playing: !!source });
  const stop = () => {
    if (!source) return;
    const old = source; source = null; old.onended = null;
    try { old.stop(); } catch (e) {}
    old.disconnect();
    notify();
  };
  const localBase64 = () => new Promise((resolve, reject) => {
    if (window.D2D_JOURNEY_B64) return resolve(window.D2D_JOURNEY_B64);
    const script = document.createElement("script");
    script.src = "audio/journey-inline.js";
    script.onload = () => window.D2D_JOURNEY_B64 ? resolve(window.D2D_JOURNEY_B64) : reject(new Error("Local music missing"));
    script.onerror = () => reject(new Error("Local music missing"));
    document.head.appendChild(script);
  });
  const prepare = async () => {
    if (ready || loading) return;
    loading = true; error = ""; notify();
    try {
      let bytes;
      if (location.protocol === "file:") {
        const binary = atob(await localBase64());
        const data = new Uint8Array(binary.length);
        for (let i = 0; i < binary.length; i++) data[i] = binary.charCodeAt(i);
        bytes = data.buffer;
      } else {
        let cache = null, response = null;
        try { if ("caches" in window) { cache = await caches.open("d2d-fixed-audio-v20261028"); response = await cache.match(FILE); } } catch (e) {}
        if (!response) {
          response = await fetch(FILE, { cache: "force-cache" });
          if (!response.ok) throw new Error("Music download failed");
          if (cache) cache.put(FILE, response.clone()).catch(() => {});
        }
        bytes = await response.arrayBuffer();
      }
      if (bytes.byteLength < 1000000) throw new Error("Music download incomplete");
      const AudioContextClass = window.AudioContext || window.webkitAudioContext;
      if (!AudioContextClass) throw new Error("This browser cannot play the music");
      ctx = new AudioContextClass();
      buffer = await ctx.decodeAudioData(bytes);
      ready = true;
    } catch (e) {
      error = e.message || "Music unavailable";
      if (ctx) { ctx.close().catch(() => {}); ctx = null; }
    } finally { loading = false; notify(); }
  };
  const position = () => {
    if (!ref) return null;
    const p = ref.pos + (performance.now() - ref.t0) / 1000;
    return p < ref.end ? p : null;
  };
  const playAt = pos => {
    if (!enabled || !ready || ctx.state !== "running") return;
    stop();
    const node = ctx.createBufferSource();
    node.buffer = buffer; node.connect(ctx.destination);
    source = node; startPos = pos; startTime = ctx.currentTime;
    node.onended = () => { if (source === node) { source = null; notify(); } };
    node.start(0, Math.min(pos, buffer.duration - 0.01));
    notify();
  };
  const tick = () => {
    if (!enabled || !ready) return;
    const want = position();
    if (want === null || want >= buffer.duration) { stop(); return; }
    if (!source) { playAt(want); return; }
    const actual = startPos + ctx.currentTime - startTime;
    if (Math.abs(actual - want) > 4) playAt(want); // recover after sleep, never for routine status jitter
  };
  const onStatus = (st, retained = false) => {
    if (!st || st.at === undefined) return;
    if (retained && (!st.ms || Math.abs(Date.now() - st.ms) > 10000)) return;
    if (st.at !== 4 || !(st.ae > st.ap)) { ref = null; scene = ""; stop(); return; }
    const nextScene = st.s + ":" + st.b + ":" + st.at;
    const skewOk = st.ms > 0 && Math.abs(Date.now() - st.ms) < 5000;
    const p = Math.max(0, Number(st.ap) + (skewOk ? (Date.now() - st.ms) / 1000 : 0));
    const old = position();
    if (scene !== nextScene || old === null || Math.abs(old - p) > 4) {
      scene = nextScene; ref = { pos: p, end: Number(st.ae), t0: performance.now() };
      if (enabled) playAt(p);
    } else ref.end = Number(st.ae);
  };
  const arm = async () => {
    if (!ready || !ctx) return false;
    try { await ctx.resume(); enabled = true; tick(); notify(); return true; }
    catch (e) { error = "Browser blocked audio"; notify(); return false; }
  };
  const unlock = async () => {
    if (!ready || !ctx) return false;
    try { await ctx.resume(); return true; } catch (e) { error = "Browser blocked audio"; notify(); return false; }
  };
  const off = () => { enabled = false; stop(); notify(); };
  setInterval(tick, 1000);
  return { prepare, unlock, arm, off, onStatus, state: () => ({ loading, ready, error, enabled, playing: !!source }), onChange: fn => { onChange = fn; notify(); } };
})();
