/* Exhibition soundtrack for the dedicated computer connected to the JBL.
   Download the whole MP3 first. The operator's click starts its normal browser
   player silently; live ESP32 status selects the audible position. */
window.D2DExhibitAudio = (() => {
  const FILE = "audio/journey.mp3?v=20261028-levelled";
  let media = null, mediaUrl = "", loading = false, ready = false, error = "";
  let enabled = false, primed = false, playing = false, seeking = false;
  let ref = null, scene = "", seekToken = 0;
  let onChange = () => {};
  const notify = () => onChange({ loading, ready, error, enabled, playing });
  const stop = () => {
    ++seekToken; seeking = false;
    if (media) media.volume = 0;
    if (playing) { playing = false; notify(); }
  };
  const localBase64 = () => new Promise((resolve, reject) => {
    if (window.D2D_JOURNEY_B64) return resolve(window.D2D_JOURNEY_B64);
    const script = document.createElement("script");
    script.src = "audio/journey-inline.js?v=20261028-levelled";
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
        try { if ("caches" in window) { cache = await caches.open("d2d-fixed-audio-v20261028-levelled"); response = await cache.match(FILE); } } catch (e) {}
        if (!response) {
          response = await fetch(FILE, { cache: "force-cache" });
          if (!response.ok) throw new Error("Music download failed");
          if (cache) cache.put(FILE, response.clone()).catch(() => {});
        }
        bytes = await response.arrayBuffer();
      }
      if (bytes.byteLength < 1000000) throw new Error("Music download incomplete");
      mediaUrl = URL.createObjectURL(new Blob([bytes], { type: "audio/mpeg" }));
      media = new Audio(); media.preload = "auto"; media.loop = true; media.volume = 0; media.src = mediaUrl;
      await new Promise((resolve, reject) => {
        media.onloadedmetadata = resolve;
        media.onerror = () => reject(new Error("Browser cannot read music"));
        media.load();
      });
      if (!Number.isFinite(media.duration) || media.duration < 130) throw new Error("Music duration is incomplete");
      ready = true;
    } catch (e) {
      error = e.message || "Music unavailable";
      if (media) { media.pause(); media = null; }
      if (mediaUrl) { URL.revokeObjectURL(mediaUrl); mediaUrl = ""; }
    } finally { loading = false; notify(); }
  };
  const position = () => {
    if (!ref) return null;
    const p = ref.pos + (performance.now() - ref.t0) / 1000;
    return p < ref.end ? p : null;
  };
  const playAt = pos => {
    if (!enabled || !ready || !primed || !media || media.paused || pos >= media.duration) return;
    const token = ++seekToken;
    seeking = true; playing = false; media.volume = 0; notify();
    media.currentTime = Math.min(pos, media.duration - 0.01);
    const audible = () => {
      if (token !== seekToken || !enabled || !primed) return;
      seeking = false; media.volume = 1; playing = true; notify();
    };
    if (media.seeking) media.addEventListener("seeked", audible, { once: true });
    else audible();
  };
  const tick = () => {
    if (!enabled || !ready || !primed) return;
    const want = position();
    if (want === null || want >= media.duration) { stop(); return; }
    if (media.paused) { error = "Browser paused the soundtrack; click 展示有音乐 again"; stop(); notify(); return; }
    if (seeking) return;
    if (!playing || Math.abs(media.currentTime - want) > 4) playAt(want);
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
  const unlock = async () => {
    if (!ready || !media) return false;
    if (primed && !media.paused) return true;
    try {
      media.volume = 0;
      await media.play();
      primed = true; error = ""; notify(); return true;
    } catch (e) { error = "Browser blocked audio: " + e.message; notify(); return false; }
  };
  const arm = async () => {
    if (!ready || !primed || !media || media.paused) return false;
    enabled = true; tick(); notify(); return true;
  };
  const off = () => {
    enabled = false; stop(); primed = false;
    if (media) media.pause();
    notify();
  };
  setInterval(tick, 1000);
  return { prepare, unlock, arm, off, onStatus, state: () => ({ loading, ready, error, enabled, playing }), onChange: fn => { onChange = fn; notify(); } };
})();
