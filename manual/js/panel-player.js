// Magic Panel sequence player: replays a recorded sequence (tools/gen_docs.py -> sequence.json)
// at its real timing. Markup: <div class="mp-player" data-src="sequence.json"></div>.
// Frame format: [t_ms, "16 hex digits = rows 0..7", intensity_top, intensity_bottom].
(function () {
  "use strict";
  const SPEEDS = [0.25, 0.5, 1, 2];

  function fmt(ms) { return (ms / 1000).toFixed(2) + " s"; }

  function frameAt(frames, t) {             // last frame with frame[0] <= t (binary search)
    let lo = 0, hi = frames.length - 1;
    while (lo < hi) {
      const mid = (lo + hi + 1) >> 1;
      if (frames[mid][0] <= t) lo = mid; else hi = mid - 1;
    }
    return frames[lo];
  }

  function draw(canvas, frame) {
    const ctx = canvas.getContext("2d");
    const css = getComputedStyle(canvas);
    const on = css.getPropertyValue("--mp-led-on").trim() || "#ff281e";
    const off = css.getPropertyValue("--mp-led-off").trim() || "#2e1616";
    const size = canvas.width, cell = size / 8, r = cell * 0.38;
    ctx.clearRect(0, 0, size, size);
    for (let row = 0; row < 8; row++) {
      const bits = parseInt(frame[1].substr(row * 2, 2), 16);
      const level = (row < 4 ? frame[2] : frame[3]) / 15;
      for (let col = 0; col < 8; col++) {
        const lit = (bits >> (7 - col)) & 1;   // bit 7 = leftmost column (assumption A-1)
        ctx.globalAlpha = lit ? 0.45 + 0.55 * level : 1;
        ctx.fillStyle = lit ? on : off;
        ctx.beginPath();
        ctx.arc(col * cell + cell / 2, row * cell + cell / 2, r, 0, 2 * Math.PI);
        ctx.fill();
      }
    }
    ctx.globalAlpha = 1;
  }

  function el(tag, cls, parent, text) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text) e.textContent = text;
    if (parent) parent.appendChild(e);
    return e;
  }

  function build(root, data) {
    const frames = data.frames, total = data.recorded_ms;
    root.textContent = "";
    const canvas = el("canvas", "mp-canvas", root);
    const dpr = window.devicePixelRatio || 1;
    canvas.width = canvas.height = Math.round(256 * dpr);
    const bar = el("div", "mp-controls", root);
    const play = el("button", "mp-play", bar);
    play.type = "button";
    const scrub = el("input", "mp-scrub", bar);
    Object.assign(scrub, { type: "range", min: 0, max: Math.round(total), step: 1, value: 0 });
    scrub.setAttribute("aria-label", "Position");
    const time = el("span", "mp-time", bar);
    const speed = el("select", "mp-speed", bar);
    speed.setAttribute("aria-label", "Speed");
    SPEEDS.forEach(s => { const o = el("option", null, speed, s + "×"); o.value = s; if (s === 1) o.selected = true; });

    let t = 0, playing = false, last = null;
    const reduce = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;

    function show() {
      draw(canvas, frameAt(frames, t));
      scrub.value = Math.round(t);
      time.textContent = fmt(t) + " / " + fmt(total);
      play.textContent = playing ? "❚❚" : "▶";
      play.setAttribute("aria-label", playing ? "Pause" : "Play");
    }
    function tick(now) {
      if (!playing) return;
      if (last !== null) t += (now - last) * parseFloat(speed.value);
      last = now;
      if (t >= total) t = 0;                 // loop
      show();
      requestAnimationFrame(tick);
    }
    function setPlaying(p) {
      playing = p; last = null; show();
      if (p) requestAnimationFrame(tick);
    }
    play.addEventListener("click", () => setPlaying(!playing));
    scrub.addEventListener("input", () => { t = +scrub.value; last = null; show(); });
    canvas.addEventListener("click", () => setPlaying(!playing));
    setPlaying(!reduce);
  }

  function init() {
    document.querySelectorAll(".mp-player:not([data-ready])").forEach(root => {
      root.setAttribute("data-ready", "");
      fetch(root.getAttribute("data-src"))
        .then(r => { if (!r.ok) throw new Error(r.status); return r.json(); })
        .then(data => build(root, data))
        .catch(() => {                      // e.g. opened from disk (file://): show the GIF instead
          const gif = root.getAttribute("data-gif");
          if (!gif) { root.textContent = "The player could not load this recording."; return; }
          const img = el("img", "mp-fallback", root);
          img.src = gif; img.alt = root.getAttribute("data-title") || "";
        });
    });
  }
  // Material/Zensical instant navigation exposes document$; plain page loads use DOMContentLoaded.
  if (window.document$ && window.document$.subscribe) window.document$.subscribe(init);
  else if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
