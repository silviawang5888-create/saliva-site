/* Inside a Drop of Saliva — developer simulator.
   sim/anim.wasm uses the animation headers from the ESP32 project and a browser wrapper for panel mapping
   and tour timing. The full soundtrack comes from audio/journey.mp3 (or the embedded local preview copy).
   The preview is for checking sequence and timing; final LED orientation and brightness need physical testing. */
(function () {
  const SR = 22050, FRAME_MS = 100;
  const CUE_TICK = 0, CUE_CLICK = 9;                         // cueLevel() in the firmware: tick 0.5, click 0.8, rest 1
  const GRID_BYTES = (4 * 2 * 256 + 3 * 256) * 3;
  let ex = null, info = null, ready = null;
  // playback state
  let mode = "tour", box = 0, playing = false, T = 0, speed = 1, last = 0, timer = null, ended = false;
  // sound
  let actx = null, out = null, soundOn = true, volume = 0.7;
  const cueBuffers = {};
  const JOURNEY = 4, JOURNEY_BOXES = 4;              // track 4 = the whole soundtrack Box 1 → 4
  const music = [null, null, null, null, null];    // { buf: AudioBuffer, name, sec, from }
  let journeyLoadError = null;
  let mSrc = null, mGain = null, mBox = -1, mStartCtx = 0, mStartPos = 0;
  let mediaError = "", mediaStarting = false, mediaPlayToken = 0;
  let ui = null;

  // Browsers block fetch(file://...) for a page opened directly from a ZIP extraction.
  // The small local-preview loader is only used in that case; hosted pages fetch the MP3.
  const loadLocalJourney = (base) => new Promise((resolve, reject) => {
    const decode = () => {
      try {
        const raw = atob(window.D2D_JOURNEY_B64);
        const bytes = new Uint8Array(raw.length);
        for (let i = 0; i < raw.length; i++) bytes[i] = raw.charCodeAt(i);
        resolve(bytes.buffer);
      } catch (e) { reject(e); }
    };
    if (window.D2D_JOURNEY_B64) return decode();
    const script = document.createElement("script");
    script.src = base + "audio/journey-inline.js?v=20261028-levelled";
    script.onload = decode;
    script.onerror = () => reject(new Error("找不到本地预览音乐 audio/journey-inline.js"));
    document.head.appendChild(script);
  });

  // ---------- load the compiled firmware animations ----------
  const load = (base) => ready || (ready = (async () => {
    let wasm, meta;
    if (window.D2D_ANIM) {                                    // sim/anim.js: the program packed into a script (works offline)
      const bin = atob(window.D2D_ANIM.wasm); wasm = new Uint8Array(bin.length);
      for (let i = 0; i < bin.length; i++) wasm[i] = bin.charCodeAt(i);
      meta = window.D2D_ANIM.info;
    } else {
      try {
        [wasm, meta] = await Promise.all([
          fetch(base + "sim/anim.wasm").then(r => { if (!r.ok) throw new Error("网站上没有 sim/anim.wasm"); return r.arrayBuffer(); }),
          fetch(base + "sim/anim.json").then(r => r.ok ? r.json() : null).catch(() => null),
        ]);
      } catch (e) {
        throw new Error((e.message.includes("sim/") ? e.message : "下载不到模拟器文件") +
          "。检查：电脑是不是连着 ESP32 的热点（没有外网）？网站的 sim 文件夹里有没有 anim.js？");
      }
    }
    const stubs = new Proxy({}, { get: () => () => 0 });      // no files or system clock are used
    const env = {                                              // math used by the real animation source
      expf: Math.exp, sinf: Math.sin, sqrtf: Math.sqrt, cosf: Math.cos,
      powf: Math.pow, fmodf: (a, b) => a % b, floorf: Math.floor,
      fabsf: Math.abs, fminf: Math.min, hypotf: Math.hypot,
      tanhf: Math.tanh, ceilf: Math.ceil
    };
    const { instance } = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: stubs, env });
    ex = instance.exports;
    if (ex._initialize) ex._initialize();
    info = meta;
    for (let b = 0; b <= JOURNEY; b++) {                      // sound that ships with the website: audio/box1.wav … audio/journey.mp3
      const f = b === JOURNEY ? "journey.mp3" : "box" + (b + 1) + ".wav";
      try {
        if (b === JOURNEY && window.location.protocol === "file:") {
          await setMusic(b, await loadLocalJourney(base), "本地包里的 audio/" + f, "site");
        } else {
          const r = await fetch(base + "audio/" + f + (b === JOURNEY ? "?v=20261028-levelled" : ""));
          if (r.ok) await setMusic(b, await r.arrayBuffer(), "网站上的 audio/" + f, "site");
        }
      } catch (e) { if (b === JOURNEY) journeyLoadError = e.message || String(e); }
    }
    return ex;
  })());

  // ---------- sound ----------
  const ctx = () => {
    if (!actx) {
      actx = new (window.AudioContext || window.webkitAudioContext)();
      out = actx.createGain(); out.gain.value = volume; out.connect(actx.destination);
    }
    if (actx.state === "suspended") actx.resume();
    return actx;
  };
  // the ESP32 synth (Audio::task), note by note
  const cueBuffer = (cue) => {
    if (cueBuffers[cue]) return cueBuffers[cue];
    const nn = ex.noteCount(), f = new Float32Array(ex.memory.buffer, ex.notes(), nn * 8);
    const notes = [];
    for (let i = 0; i < nn; i++) if (f[i * 8] === cue) notes.push(Array.from(f.subarray(i * 8, i * 8 + 8)));
    let len = 0;
    for (const n of notes) len = Math.max(len, n[1] + n[5]);
    const N = Math.ceil(len * SR) + 1, data = new Float32Array(N);
    let noise = 22222;
    // same envelope and per-cue loudness as the firmware (noteEnv, computeCueGains)
    const env = (t, dur, atk) => {
      if (t < 0 || t >= dur) return 0;
      if (atk < 0) return Math.min(1, t / 0.01, (dur - t) / 0.05);
      const u = t / dur; return t < atk ? t / atk : (1 - u) * (1 - u);
    };
    let peak = 0;
    for (let t = 0; t < len; t += 0.002) {
      let a = 0;
      for (const [, delay, wave, , , dur, vol, atk] of notes) a += vol * (wave === 1 ? 0.6 : 1) * env(t - delay, dur, atk);
      peak = Math.max(peak, a);
    }
    const gain = peak > 0 ? (cue === CUE_TICK ? 0.5 : cue === CUE_CLICK ? 0.8 : 1) / peak : 0;
    for (const [, delay, wave, f0, f1, dur, vol, atk] of notes) {
      let phase = 0;
      const s0 = Math.round(delay * SR), sn = Math.floor(dur * SR);
      for (let k = 0; k < sn && s0 + k < N; k++) {
        const t = k / SR, u = t / dur;
        const e = env(t, dur, atk);
        phase += (f0 + (f1 - f0) * u) / SR; if (phase >= 1) phase -= 1;
        let w;
        if (wave === 1) w = phase < 0.5 ? 0.6 : -0.6;
        else if (wave === 2) w = 4 * Math.abs(phase - 0.5) - 1;
        else if (wave === 3) { noise = (Math.imul(noise, 1664525) + 1013904223) >>> 0; w = ((noise >>> 9) / 8388608) * 2 - 1; }
        else w = Math.sin(6.2831853 * phase);
        data[s0 + k] += w * e * vol * gain;
      }
    }
    for (let i = 0; i < N; i++) data[i] = Math.max(-1, Math.min(1, data[i]));
    const b = ctx().createBuffer(1, N, SR); b.copyToChannel(data, 0);
    return (cueBuffers[cue] = b);
  };
  const playCue = (cue) => {
    if (!soundOn) return;
    const s = ctx().createBufferSource(); s.buffer = cueBuffer(cue); s.connect(out); s.start();
  };
  // convert like the ESP32 upload page
  const convert = async (arrayBuffer, maxS) => {
    const dec = await ctx().decodeAudioData(arrayBuffer.slice(0));
    const sec = Math.min(dec.duration, maxS), n = Math.floor(sec * SR);
    const oc = new OfflineAudioContext(1, n, SR);
    const src = oc.createBufferSource(); src.buffer = dec; src.connect(oc.destination); src.start();
    const o = (await oc.startRendering()).getChannelData(0);
    let pk = 0; for (let i = 0; i < n; i++) pk = Math.max(pk, Math.abs(o[i]));
    const k = pk > 0 ? 0.95 / pk : 1, fade = Math.min(n, 11025), d = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      let x = o[i] * k; if (i > n - fade) x *= (n - i) / fade;
      x = Math.max(-1, Math.min(1, x));
      d[i] = Math.round(x * 32767) / 32768;                  // 16-bit, like the file on the ESP32
    }
    const b = ctx().createBuffer(1, n, SR); b.copyToChannel(d, 0);
    return { buf: b, sec, orig: dec.duration };
  };
  const setMusic = async (b, arrayBuffer, name, from) => {
    if (b === JOURNEY) {
      // The browser's normal media player can play this MP3 even on computers
      // where Web Audio decoding/resuming fails. Keep the whole file in memory.
      const url = URL.createObjectURL(new Blob([arrayBuffer], { type: "audio/mpeg" }));
      const media = new Audio();
      media.preload = "auto";
      media.src = url;
      try {
        await new Promise((resolve, reject) => {
          media.onloadedmetadata = resolve;
          media.onerror = () => reject(new Error("浏览器无法读取完整旅程音乐"));
          media.load();
        });
        if (!Number.isFinite(media.duration) || media.duration <= 0) throw new Error("音乐长度无法读取");
      } catch (e) { URL.revokeObjectURL(url); throw e; }
      stopMusic();
      if (music[b] && music[b].url) URL.revokeObjectURL(music[b].url);
      music[b] = { media, url, sec: media.duration, orig: media.duration, name, from };
      journeyLoadError = null; mediaError = "";
      ex.setTrack(b, 1, media.duration);
      refreshMusic();
      return;
    }
    const c = await convert(arrayBuffer, 60);
    music[b] = { buf: c.buf, sec: c.sec, orig: c.orig, name, from };
    ex.setTrack(b, 1, c.sec);
    stopMusic(); refreshMusic();
  };
  const clearMusic = (b) => { stopMusic(); if (music[b] && music[b].url) URL.revokeObjectURL(music[b].url); music[b] = null; if (ex) ex.setTrack(b, 0, 0); refreshMusic(); };
  const stopMusic = () => {
    mediaPlayToken++; mediaStarting = false;
    if (mSrc) { const g = mGain, s = mSrc, t = actx.currentTime; g.gain.setTargetAtTime(0, t, 0.01); s.stop(t + 0.03); }
    if (music[JOURNEY] && music[JOURNEY].media) music[JOURNEY].media.pause();
    mSrc = null; mBox = -1;
  };
  const startMedia = (pos) => {
    const media = music[JOURNEY] && music[JOURNEY].media;
    if (!media || !playing || !soundOn || speed !== 1 || pos >= music[JOURNEY].sec) return;
    media.volume = volume;
    if (Math.abs(media.currentTime - pos) > 0.75) media.currentTime = pos;
    const token = ++mediaPlayToken;
    mediaError = "";
    const promise = media.play();
    if (promise) {
      mediaStarting = true;
      promise.then(() => { if (token === mediaPlayToken) { mediaStarting = false; mediaError = ""; refreshMusic(); } })
        .catch(e => { if (token === mediaPlayToken) { mediaStarting = false; mediaError = "浏览器阻止了播放，请再点一次「播放」：" + e.message; refreshMusic(); } });
    }
  };
  const startMediaFromGesture = () => {
    if (!music[JOURNEY] || !playing || !soundOn || speed !== 1) return;
    if (mode === "tour") startMedia(T);
    else if (mode === "focus" && box < JOURNEY_BOXES && T < ex.boxS(box)) startMedia(ex.boxStart(box) + T);
  };
  // the ESP32 plays box b's music from position p while that box is on (main loop: Audio::want)
  const syncMusic = (wb, wp) => {
    if (!soundOn || !playing || speed !== 1) { stopMusic(); return; }
    if (wb < 0 || !music[wb] || wp >= music[wb].sec) { stopMusic(); return; }
    if (wb === JOURNEY && music[wb].media) {
      const media = music[wb].media;
      media.volume = volume;
      if (Math.abs(media.currentTime - wp) > 0.75) media.currentTime = wp;
      if (media.paused && !mediaStarting && !mediaError) startMedia(wp);
      if (mSrc) { const g = mGain, s = mSrc, t = actx.currentTime; g.gain.setTargetAtTime(0, t, 0.03); s.stop(t + 0.2); mSrc = null; }
      return;
    }
    if (music[JOURNEY] && music[JOURNEY].media) music[JOURNEY].media.pause();
    if (mSrc && mBox === wb) {
      const at = mStartPos + (actx.currentTime - mStartCtx);
      if (Math.abs(at - wp) <= 1) return;
    }
    stopMusic();
    const c = ctx();
    mGain = c.createGain(); mGain.gain.value = 0; mGain.connect(out);
    mGain.gain.setTargetAtTime(1, c.currentTime, 0.03);        // ~0.1 s fade in, like the ESP32
    mSrc = c.createBufferSource(); mSrc.buffer = music[wb].buf; mSrc.connect(mGain);
    mSrc.start(0, wp); mBox = wb; mStartCtx = c.currentTime; mStartPos = wp;
  };

  // ---------- one frame ----------
  const length = () => mode === "tour" ? ex.tourEnd() : mode === "focus" ? ex.focusEnd(box) : mode === "idle" ? 60 : 0;
  const covers = (b) => !!music[JOURNEY] && b < JOURNEY_BOXES;
  const render = (silent) => {
    let alive = true;
    if (mode === "tour") { if (T >= ex.tourEnd()) { alive = false; ex.idle(0); } else ex.tour(T); }
    else if (mode === "focus") alive = !!ex.focus(box, T);
    else if (mode === "idle") ex.idle(T);
    else ex.test();
    const n = ex.cueCount();
    if (!silent && playing && speed === 1) {
      const c = new Uint8Array(ex.memory.buffer, ex.cues(), n);
      for (const cue of c) playCue(cue);
    }
    let wb = -1, wp = 0;
    // same choice as the firmware's main loop
    if (mode === "tour") {
      if (music[JOURNEY] && T < music[JOURNEY].sec) { wb = JOURNEY; wp = T; }
      else for (let i = 0; i < 4; i++) { const lt = T - ex.boxStart(i); if (lt >= 0 && lt < ex.boxS(i) && !covers(i)) { wb = i; wp = lt; } }
    } else if (mode === "focus" && alive) {
      if (covers(box)) { if (T < ex.boxS(box)) { wb = JOURNEY; wp = ex.boxStart(box) + T; } }
      else { wb = box; wp = T; }
    }
    if (!silent) syncMusic(wb, wp);
    draw();
    return alive;
  };
  const tick = () => {
    const now = performance.now();
    if (playing) {
      const media = music[JOURNEY] && music[JOURNEY].media;
      const mediaClock = media && soundOn && speed === 1 &&
        (mode === "tour" || (mode === "focus" && box < JOURNEY_BOXES && T < ex.boxS(box)));
      if (mediaClock) T = Math.max(0, media.currentTime - (mode === "focus" ? ex.boxStart(box) : 0));
      else T += (now - last) / 1000 * speed;
      if (mode === "idle" && T > 60) T -= 60;
    }
    last = now;
    if (!render(false) && playing) { playing = false; ended = true; stopMusic(); }
    updateUi();
  };
  // jump without firing the sounds in between (the ESP32 never jumps, so nothing to copy here)
  const seek = (t) => { T = Math.max(0, t); ex.reset(); render(true); updateUi(); };

  // ---------- drawing ----------
  const draw = () => {
    if (!ui) return;
    const g = new Uint8Array(ex.memory.buffer, ex.grid(), GRID_BYTES);
    ui.panels.forEach(({ c, b, part }) => {
      const o = (b * 2 + part) * 256 * 3;
      D2DLive.paint(c, 16, 16, (x, y) => { const i = o + (y * 16 + x) * 3; return [g[i], g[i + 1], g[i + 2]]; }, part === 1);
    });
    ui.bridges.forEach(({ c, b }) => {
      const o = (8 * 256 + b * 256) * 3;
      D2DLive.paint(c, 32, 8, (x, y) => { const i = o + (y * 32 + x) * 3; return [g[i], g[i + 1], g[i + 2]]; }, false);
    });
    // which box is "on" right now
    let on = -1;
    if (mode === "tour" && T < ex.tourEnd()) for (let i = 0; i < 4; i++) { const lt = T - ex.boxStart(i); if (lt >= 0 && lt < ex.boxS(i)) on = i; }
    if (mode === "focus") on = box;
    ui.boxes.forEach((el, i) => el.classList.toggle("playing", on === i));
  };

  const fmt = s => { s = Math.max(0, s); const m = Math.floor(s / 60); return m + ":" + String(Math.floor(s % 60)).padStart(2, "0") + "." + Math.floor(s * 10 % 10); };
  const where = () => {
    if (mode === "test") return "测试图案（看每块灯板方向对不对）";
    if (mode === "idle") return "待机动画";
    if (mode === "focus") return "只播 Box " + (box + 1) + " · 盒内 " + fmt(T) + " / " + ex.boxS(box) + " s";
    if (T >= ex.tourEnd()) return "导览结束 → 回到待机";
    for (let i = 3; i >= 0; i--) {
      const lt = T - ex.boxStart(i);
      if (lt >= ex.boxS(i) && i < 3 && lt < ex.boxS(i) + ex.bridgeS(i)) return "光桥 " + (i + 1) + " · " + fmt(lt - ex.boxS(i)) + " / " + ex.bridgeS(i) + " s";
      if (lt >= 0 && lt < ex.boxS(i)) return "Box " + (i + 1) + " · 盒内 " + fmt(lt) + " / " + ex.boxS(i) + " s";
    }
    return "结尾保持 / 淡出";
  };
  const updateUi = () => {
    if (!ui) return;
    const L = length();
    ui.seek.disabled = !L; ui.seek.max = String(L || 1);
    if (document.activeElement !== ui.seek) ui.seek.value = String(Math.min(T, L));
    ui.time.textContent = L ? fmt(T) + " / " + fmt(L) : "—";
    ui.where.textContent = where();
    ui.play.textContent = playing ? "暂停" : (ended ? "重播" : "播放");
    ui.modes.forEach(b => b.classList.toggle("on", b.dataset.m === mode && (mode !== "focus" || +b.dataset.b === box)));
    ui.speedNote.hidden = speed === 1;
  };
  const refreshMusic = () => {
    if (!ui) return;
    ui.music.forEach((row, b) => {
      const m = music[b];
      const cap = b === JOURNEY ? 135 : 60;
      row.state.textContent = b === JOURNEY
        ? (m ? `${m.name} · ${m.sec.toFixed(1)} 秒 → 导览从 Box 1 开始一直播到底，代替 Box 1–4 和光桥的提示音` : journeyLoadError ? `完整旅程音乐加载失败：${journeyLoadError}` : "没有 → Box 1–4 用各自的音乐或电子提示音")
        : covers(b) ? "由「完整旅程」负责（单独播这个盒子时播它对应的那一段）"
        : m ? `${m.name} · 转换后 ${m.sec.toFixed(1)} 秒${m.orig > cap ? "（原来 " + m.orig.toFixed(0) + " 秒，只保留前 " + cap + " 秒）" : ""}${m.sec < ex.boxS(b) ? " · 比盒子短，后面没声音" : ""} → 这个盒子的电子提示音不播`
        : "没有音乐 → 播放程序里的电子提示音";
      if (b === JOURNEY && mediaError) row.state.textContent += " · " + mediaError;
      row.clear.hidden = !m;
    });
  };

  // ---------- page ----------
  window.D2DSim = {
    async mount(root, base = "") {
      root.innerHTML = `
        <div class="simbar">
          <div class="row simmodes">
            <button class="g" data-m="tour">完整导览</button>
            <button class="g" data-m="focus" data-b="0">Box 1</button><button class="g" data-m="focus" data-b="1">Box 2</button>
            <button class="g" data-m="focus" data-b="2">Box 3</button><button class="g" data-m="focus" data-b="3">Box 4</button>
            <button class="g" data-m="idle">待机</button><button class="g" data-m="test">测试图案</button>
          </div>
          <div class="row">
            <button id="simPlay" style="min-width:84px">播放</button>
            <input type="range" id="simSeek" min="0" max="1" step="0.1" value="0" style="flex:1;min-width:160px" aria-label="时间">
            <span id="simTime" class="mono">—</span>
          </div>
          <div class="row" style="justify-content:space-between">
            <b id="simWhere"></b>
            <span class="row" style="gap:10px">
              <label class="line" style="gap:6px">速度 <select id="simSpeed" style="width:auto"><option value="0.5">0.5×</option><option value="1" selected>1×</option><option value="2">2×</option><option value="4">4×</option></select></label>
              <label class="line" style="gap:6px"><input type="checkbox" id="simSound" checked> 声音</label>
              <input type="range" id="simVol" min="0" max="1" step="0.05" value="0.7" style="width:90px" aria-label="电脑音量">
            </span>
          </div>
          <div class="muted" id="simSpeedNote" hidden>不是 1× 时不放声音（音乐和提示音只有原速才对得上）。</div>
        </div>
        <div class="mapwrap"><div class="map simmap" id="simMap"></div></div>
        <div class="card" style="padding:12px;gap:8px">
          <h2 style="font-size:14px">每个盒子的音乐 <span class="muted" style="font-weight:400">选文件只是在这台电脑上试听，不会上传到 ESP32</span></h2>
          <div id="simMusic"></div>
        </div>`;
      const LABS = [["#ff5500", "Box 1 图案"], ["#56d1c2", "Box 2 感知"], ["#6d33bd", "Box 3 连接"], ["#ff005f", "Box 4 组装"]];
      let h = "";
      LABS.forEach(([c, n], i) => {
        h += `<div class="mbox" style="--c:${c}"><b><i></i>${n}</b>
          <canvas data-b="${i}" data-part="1"></canvas><small>背板 · 均光膜</small>
          <canvas data-b="${i}" data-part="0"></canvas><small>底部</small></div>`;
        if (i < 3) h += `<div class="mbcol"><div class="mbslot"><canvas class="mbridge" data-br="${i}" title="光桥 ${i + 1}"></canvas></div></div>`;
      });
      root.querySelector("#simMap").innerHTML = h;
      root.querySelector("#simMusic").innerHTML = LABS.concat([["#00004d", "完整旅程 Box 1→4"]]).map(([c, n], i) => `
        <div class="it" style="flex-wrap:wrap"><b style="min-width:90px;color:${c === "#56d1c2" ? "#1a8f82" : c}">${n}</b>
          <span class="muted" style="flex:1;min-width:180px" data-ms="${i}"></span>
          <span class="row" style="gap:6px"><label class="g btnlike">选音乐<input type="file" accept="audio/*,.wav,.mp3,.m4a" data-mf="${i}" hidden></label>
          <button class="g" data-mt="${i}">试听</button><button class="g" data-mc="${i}" hidden>去掉</button></span></div>`).join("");
      const $ = s => root.querySelector(s);
      const showBox = (b) => {                                   // on a phone the map scrolls sideways: bring the box into view
        const w = root.querySelector(".mapwrap"), el = root.querySelectorAll(".mbox")[b];
        if (w && el) w.scrollTo({ left: el.offsetLeft - (w.clientWidth - el.offsetWidth) / 2, behavior: "smooth" });
      };
      ui = {
        panels: [...root.querySelectorAll("canvas[data-part]")].map(c => ({ c, b: +c.dataset.b, part: +c.dataset.part })),
        bridges: [...root.querySelectorAll("canvas[data-br]")].map(c => ({ c, b: +c.dataset.br })),
        boxes: [...root.querySelectorAll(".mbox")],
        modes: [...root.querySelectorAll(".simmodes button")],
        play: $("#simPlay"), seek: $("#simSeek"), time: $("#simTime"), where: $("#simWhere"), speedNote: $("#simSpeedNote"),
        music: [0, 1, 2, 3, 4].map(i => ({ state: root.querySelector(`[data-ms="${i}"]`), clear: root.querySelector(`[data-mc="${i}"]`) })),
      };
      ui.where.textContent = "加载程序里的动画…";
      try { await load(base); }
      catch (e) { ui.where.textContent = "模拟器加载失败：" + e.message; return; }
      const src = root.parentElement.querySelector("#simSrc");
      if (src && info) src.textContent = `来自程序文件 ${info.source} · 保存于 ${info.saved} · 编号 ${info.md5}`;
      ui.modes.forEach(b => b.addEventListener("click", () => {
        mode = b.dataset.m; box = +(b.dataset.b || 0); ended = false; stopMusic();
        seek(0); playing = mode !== "test"; last = performance.now(); if (playing) { startMediaFromGesture(); ctx(); } updateUi();
        showBox(mode === "focus" ? box : 0);
      }));
      ui.play.addEventListener("click", () => {
        if (ended) { ended = false; seek(0); }
        playing = !playing; last = performance.now();
        if (playing) { startMediaFromGesture(); ctx(); } else stopMusic();
        updateUi();
      });
      ui.seek.addEventListener("input", () => { ended = false; stopMusic(); seek(+ui.seek.value); startMediaFromGesture(); });
      $("#simSpeed").addEventListener("change", e => { speed = +e.target.value; stopMusic(); updateUi(); });
      $("#simSound").addEventListener("change", e => { soundOn = e.target.checked; if (!soundOn) stopMusic(); else startMediaFromGesture(); });
      $("#simVol").addEventListener("input", e => { volume = +e.target.value; if (out) out.gain.value = volume; if (music[JOURNEY] && music[JOURNEY].media) music[JOURNEY].media.volume = volume; });
      root.querySelectorAll("[data-mf]").forEach(inp => inp.addEventListener("change", async () => {
        const f = inp.files[0], b = +inp.dataset.mf; if (!f) return;
        ui.music[b].state.textContent = "转换中…";
        try { await setMusic(b, await f.arrayBuffer(), f.name, "file"); }
        catch (e) { ui.music[b].state.textContent = "这个文件读不了：" + e.message; }
        inp.value = "";
      }));
      root.querySelectorAll("[data-mc]").forEach(btn => btn.addEventListener("click", () => clearMusic(+btn.dataset.mc)));
      root.querySelectorAll("[data-mt]").forEach(btn => btn.addEventListener("click", () => {   // hear this box: start its focus
        const b = +btn.dataset.mt; if (b === JOURNEY) { mode = "tour"; box = 0; } else { mode = "focus"; box = b; } ended = false; stopMusic(); seek(0); playing = true; last = performance.now(); startMediaFromGesture(); ctx(); updateUi(); showBox(b === JOURNEY ? 0 : b);
        root.querySelector(".simbar").scrollIntoView({ block: "start", behavior: "smooth" });
      }));
      refreshMusic();
      seek(0);
      clearInterval(timer); timer = setInterval(tick, FRAME_MS);   // 10 frames/s, like the ESP32
      window.addEventListener("resize", draw);
    },
    // stop sound when the tab is hidden
    pause() { playing = false; stopMusic(); updateUi(); },
  };
})();
