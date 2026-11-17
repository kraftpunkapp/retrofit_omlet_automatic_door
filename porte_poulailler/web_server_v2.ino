/*
 * ============================================================
 *  PORTE POULAILLER — Serveur Web + WebSocket  v2
 *  À placer dans le même dossier que porte_poulailler.ino
 *
 *  Dépendances supplémentaires :
 *    - ESPAsyncWebServer  (ESP32Async) https://github.com/ESP32Async/ESPAsyncWebServer
 *    - AsyncTCP           (ESP32Async) https://github.com/ESP32Async/AsyncTCP
 * ============================================================
 */

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include "icones_web.h"  // icônes PNG (raccourci écran d'accueil)

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ── HTML embarqué en flash (PROGMEM) ────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#0e1521">
<title>Poulailler</title>
<link rel="manifest" href="/manifest.json">
<link rel="icon" type="image/png" sizes="192x192" href="/icon-192.png">
<link rel="apple-touch-icon" href="/icon-192.png">
<style>
:root{
  --bg:#0e1521;--card:#162032;--card2:#1d2a40;--line:#2b3a55;
  --text:#edf1f7;--muted:#9eabc2;
  --yolk:#f4b942;--open:#62d2a2;--closed:#8fa8ff;--err:#ff7a6e;
  --font:"Avenir Next","SF Pro Rounded",ui-rounded,"Segoe UI Variable Display","Segoe UI",system-ui,-apple-system,sans-serif;
}
*{box-sizing:border-box;margin:0;padding:0}
[hidden]{display:none!important}
html{-webkit-text-size-adjust:100%}
body{background:var(--bg);color:var(--text);font:400 15px/1.4 var(--font);min-height:100vh;padding-bottom:calc(88px + env(safe-area-inset-bottom));font-variant-numeric:tabular-nums}
button,input{font:inherit;color:inherit}
button{cursor:pointer;-webkit-tap-highlight-color:transparent;user-select:none;touch-action:manipulation}
:focus-visible{outline:2px solid var(--yolk);outline-offset:2px}
svg{display:block}
.ico{width:20px;height:20px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round;flex-shrink:0}

/* ── En-tête ── */
.top{position:sticky;top:0;z-index:10;display:flex;align-items:center;gap:12px;padding:12px 16px;background:rgba(14,21,33,.92);-webkit-backdrop-filter:blur(10px);backdrop-filter:blur(10px);border-bottom:1px solid var(--line)}
.brand{display:flex;align-items:center;gap:10px;margin-right:auto}
.brand svg{width:34px;height:34px}
h1{font-size:20px;font-weight:800;letter-spacing:-.02em}
.clock{font-size:15px;font-weight:700}
.rssi{display:flex;align-items:flex-end;gap:2px;height:16px}
.rssi i{display:block;width:4px;border-radius:1px;background:var(--line)}
.rssi i:nth-child(1){height:5px}.rssi i:nth-child(2){height:8px}.rssi i:nth-child(3){height:12px}.rssi i:nth-child(4){height:16px}
.rssi-val{display:none}
.pill{white-space:nowrap;display:flex;align-items:center;gap:8px;padding:6px 12px;border-radius:99px;background:var(--card);border:1px solid var(--line);font-size:13px;font-weight:600}
.dot{width:8px;height:8px;border-radius:50%;background:var(--open)}
.dot.offline{background:var(--err)}
.dot.moving{background:var(--yolk);animation:pulse 1.2s infinite}
@keyframes pulse{50%{opacity:.35}}

/* ── Structure ── */
main{max-width:560px;margin:0 auto;padding:14px;display:flex;flex-direction:column;gap:14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:22px;padding:18px}
.card>h2{font-size:15px;font-weight:700;margin-bottom:14px}

/* ── Porte ── */
.hero{padding:0;overflow:hidden;border-radius:28px}
.scene{background:linear-gradient(180deg,#22314d,#162032);padding:14px 14px 0}
.scene svg{width:100%;height:auto;max-height:190px}
#shutter{transition:transform .6s ease}
.scene[data-s="open"] #shutter,.scene[data-s="opening"] #shutter{transform:translateX(-28px)}
.scene[data-s="opening"] #shutter,.scene[data-s="closing"] #shutter{transition:transform 20s linear}
.scene[data-s="error"] #shutter{transform:translateX(-14px)}
.scene.init #shutter{transition:none}
#glow{opacity:0;transition:opacity .6s}
.scene[data-s="open"] #glow,.scene[data-s="opening"] #glow{opacity:1}
.hero-body{padding:8px 20px 20px}
.state{font-size:44px;line-height:1.05;font-weight:800;letter-spacing:-.03em}
.state.open{color:var(--open)}.state.closed{color:var(--closed)}.state.moving{color:var(--yolk)}.state.error{color:var(--err)}
.sub{margin-top:6px;color:var(--muted);font-size:15px}
.progress{height:4px;border-radius:9px;background:var(--bg);margin-top:14px;overflow:hidden;opacity:0;transition:opacity .3s}
.progress.visible{opacity:1}
.progress i{display:block;height:100%;width:0;background:var(--yolk);border-radius:9px;animation:prog 20s linear forwards}
@keyframes prog{to{width:100%}}
.badge{margin-top:14px;padding:10px 12px;border-radius:12px;background:rgba(244,185,66,.1);border:1px solid rgba(244,185,66,.4);color:var(--yolk);font-size:13px}

/* ── Boutons ── */
.actions{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
.btn{min-height:64px;border-radius:18px;border:1.5px solid;display:flex;align-items:center;justify-content:center;gap:10px;font-weight:700;font-size:17px;transition:transform .12s,background .15s}
.btn:active{transform:scale(.97)}
.b-open{color:var(--open);border-color:rgba(98,210,162,.55);background:rgba(98,210,162,.12)}
.b-close{color:var(--closed);border-color:rgba(143,168,255,.55);background:rgba(143,168,255,.12)}
.b-stop{grid-column:1/-1;min-height:48px;font-size:14px;color:var(--muted);border-color:var(--line);background:transparent}
.b-stop.armed{color:var(--err);border-color:var(--err);background:rgba(255,122,110,.1)}
.b-reset{grid-column:1/-1;min-height:48px;font-size:14px;display:none;color:var(--err);border-color:var(--err);background:rgba(255,122,110,.12)}
.b-reset.show{display:flex}

/* ── Modes ── */
.seg{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:4px;padding:4px;background:var(--bg);border-radius:16px}
.seg button{border:0;background:transparent;color:var(--muted);border-radius:12px;padding:10px 2px 9px;min-height:62px;display:flex;flex-direction:column;align-items:center;gap:5px;font-size:12px;font-weight:600}
.seg button[aria-pressed="true"]{background:var(--card2);color:var(--text);box-shadow:inset 0 0 0 1.5px var(--yolk)}
.seg button[aria-pressed="true"] svg{color:var(--yolk)}
.pending{margin-top:10px;padding:9px 12px;border-radius:12px;background:rgba(244,185,66,.1);color:var(--yolk);font-size:13px}
.panel{margin-top:16px}

/* ── Programme du jour ── */
.band{position:relative;height:14px;border-radius:99px;background:var(--bg);margin:26px 10px 8px}
.band-day{position:absolute;top:0;bottom:0;border-radius:99px;background:linear-gradient(90deg,#b9801a,#f4b942 50%,#b9801a)}
.mk{position:absolute;top:50%;width:22px;height:22px;margin:-11px 0 0 -11px;border-radius:50%;border:3px solid var(--card)}
.mk.o{background:var(--open)}.mk.c{background:var(--closed)}
.now{position:absolute;top:-9px;bottom:-9px;width:2px;margin-left:-1px;border-radius:2px;background:var(--text)}
.ticks{display:flex;justify-content:space-between;margin:0 10px 16px;font-size:11px;color:var(--muted)}
.times{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
.t{background:var(--card2);border-radius:16px;padding:14px}
.t small{display:flex;align-items:center;gap:7px;font-size:13px;color:var(--muted)}
.t small i{width:10px;height:10px;border-radius:50%}
.t b{display:block;margin-top:6px;font-size:32px;line-height:1;font-weight:800;letter-spacing:-.02em}
.t em{display:block;margin-top:4px;font-style:normal;font-size:12px;color:var(--muted)}
.sun{display:flex;justify-content:space-between;gap:10px;margin-top:14px;font-size:13px;color:var(--muted)}
.sun span{display:flex;align-items:center;gap:7px}
.sun b{color:var(--text);font-weight:700}
.sub-h{font-size:13px;font-weight:700;margin:18px 0 2px;color:var(--muted)}

/* ── Décalages / pas à pas ── */
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 0}
.row+.row{border-top:1px solid var(--line)}
.lbl b{display:block;font-size:15px;font-weight:600}
.lbl span{font-size:13px;color:var(--muted)}
.step{display:flex;align-items:center;gap:4px}
.step button{width:44px;height:44px;border-radius:14px;border:1.5px solid var(--line);background:var(--card2);display:grid;place-items:center}
.step button:active{background:var(--line)}
.step output{min-width:56px;text-align:center;font-size:18px;font-weight:700}
.step output.zero{color:var(--muted)}

/* ── Luminosité ── */
.thr{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
.thr .t b{font-size:26px;white-space:nowrap}
.thr .t small{white-space:normal}
input[type=range]{width:100%;margin-top:14px;height:28px;accent-color:var(--yolk)}
.det{display:flex;align-items:center;gap:10px;padding-top:6px;font-size:13px;color:var(--muted)}
.det .bar{flex:1;height:6px;border-radius:9px;background:var(--bg);overflow:hidden}
.det .bar i{display:block;height:100%;width:0;background:var(--open);transition:width .5s linear}
.det output{min-width:34px;text-align:right;color:var(--text);font-weight:700}
.live{margin-top:12px;font-size:13px;color:var(--muted)}
.live b{color:var(--yolk);font-size:15px}

/* ── Heure fixe ── */
.grp{background:var(--card2);border-radius:16px;padding:14px}
.grp+.grp{margin-top:10px}
.grp-h{display:flex;align-items:center;justify-content:space-between;gap:12px}
.grp-h b{display:flex;align-items:center;gap:8px;font-size:15px}
.grp-h b i{width:10px;height:10px;border-radius:50%}
.time-input{width:116px;height:44px;border-radius:12px;border:1.5px solid var(--line);background:var(--bg);text-align:center;font-size:20px;font-weight:700;outline:none}
.time-input:focus{border-color:var(--yolk)}
.time-input::-webkit-calendar-picker-indicator{filter:invert(1);opacity:.6}
.eff{margin-top:6px;font-size:13px;color:var(--muted)}
.eff b{color:var(--text)}
.manuel{display:flex;align-items:center;gap:14px;padding:14px;border-radius:16px;background:var(--card2);color:var(--muted);font-size:14px}
.manuel svg{width:28px;height:28px;color:var(--yolk)}

/* ── Capteurs ── */
.lim{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
.lim>div{display:flex;align-items:center;gap:12px;background:var(--card2);border-radius:14px;padding:12px}
.lim i{width:14px;height:14px;border-radius:50%;border:2px solid var(--muted);flex-shrink:0}
.lim .on i{background:var(--open);border-color:var(--open);box-shadow:0 0 0 4px rgba(98,210,162,.2)}
.lim b{display:block;font-size:14px;line-height:1.2}
.lim span{font-size:12px;color:var(--muted)}
.metrics{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px;margin-top:10px}
.m{background:var(--card2);border-radius:14px;padding:12px 14px}
.m small{display:flex;align-items:center;gap:7px;font-size:13px;color:var(--muted)}
.m small svg{width:16px;height:16px}
.m b{display:block;margin-top:6px;font-size:30px;line-height:1;font-weight:800;letter-spacing:-.02em}
.m em{font-style:normal;font-size:12px;color:var(--muted)}
.chart{height:120px}
canvas{width:100%;height:100%;display:block}

/* ── Journal ── */
.log{list-style:none;max-height:260px;overflow-y:auto}
.log li{display:flex;align-items:center;gap:10px;padding:10px 0;font-size:14px}
.log li+li{border-top:1px solid var(--line)}
.log li::before{content:"";width:8px;height:8px;border-radius:50%;background:var(--c,var(--closed));flex-shrink:0}
.log li.ok{--c:var(--open)}.log li.warn{--c:var(--yolk)}.log li.error{--c:var(--err)}
.log time{font-size:12px;color:var(--muted);flex-shrink:0}
.empty{color:var(--muted);font-size:14px}
footer{text-align:center;font-size:12px;color:var(--muted);padding-top:4px}

/* ── Onglets ── */
.page,.page>form{display:flex;flex-direction:column;gap:14px}
.tabs{position:fixed;left:0;right:0;bottom:0;z-index:10;background:rgba(14,21,33,.94);-webkit-backdrop-filter:blur(10px);backdrop-filter:blur(10px);border-top:1px solid var(--line);padding-bottom:env(safe-area-inset-bottom)}
.tabs-in{max-width:560px;margin:0 auto;display:grid;grid-template-columns:repeat(4,minmax(0,1fr))}
.tabs a{position:relative;display:flex;flex-direction:column;align-items:center;gap:4px;min-height:58px;padding:9px 2px 8px;color:var(--muted);text-decoration:none;font-size:11px;font-weight:600}
.tabs a[aria-current="page"]{color:var(--yolk)}
.tabs a[aria-current="page"]::before{content:"";position:absolute;top:0;left:28%;right:28%;height:2px;border-radius:0 0 3px 3px;background:var(--yolk)}
.tabs .ico{width:22px;height:22px}
.modeline{display:flex;align-items:center;gap:8px;margin-top:14px;padding:10px 12px;border-radius:12px;background:var(--card2);color:var(--text);text-decoration:none;font-size:14px;font-weight:600}
.modeline .ico{color:var(--yolk)}
.modeline .chev{margin-left:auto;width:16px;height:16px;color:var(--muted)}

/* ── Système ── */
.kv>div{display:flex;justify-content:space-between;align-items:baseline;gap:16px;padding:10px 0;font-size:14px}
.kv>div+div{border-top:1px solid var(--line)}
.kv dt{color:var(--muted);flex-shrink:0}
.kv dd{font-weight:600;text-align:right;overflow-wrap:anywhere}

/* ── Réglages ── */
.field{display:flex;flex-direction:column;gap:6px;padding:6px 0}
.field label{font-size:13px;font-weight:600;color:var(--muted)}
.field input[type=text],.field input[type=password],.field input[type=number]{width:100%;height:46px;padding:0 12px;border-radius:12px;border:1.5px solid var(--line);background:var(--bg);font-size:16px;outline:none}
.field input:focus{border-color:var(--yolk)}
.field small,.hint{font-size:12px;color:var(--muted)}
.hint{margin-top:6px}.hint.center{text-align:center}
.two{display:grid;grid-template-columns:minmax(0,2fr) minmax(0,1fr);gap:10px}
#reglForm .two:has(#fLat){grid-template-columns:repeat(2,minmax(0,1fr))}
.chk{display:flex;align-items:center;gap:12px;padding:6px 0 10px;font-weight:600}
.chk input{width:22px;height:22px;accent-color:var(--yolk)}
.alert{padding:10px 12px;border-radius:12px;background:rgba(244,185,66,.1);border:1px solid rgba(244,185,66,.4);color:var(--yolk);font-size:13px}
.msg{min-height:18px;font-size:13px}.msg.err{color:var(--err)}.msg.ok{color:var(--open)}
.b-save{width:100%;min-height:52px;font-size:16px;color:#0e1521;background:var(--yolk);border-color:var(--yolk)}
.b-ghost{width:100%;min-height:48px;font-size:14px;color:var(--muted);border-color:var(--line);background:transparent}
.overlay{position:fixed;inset:0;z-index:20;display:grid;place-items:center;padding:24px;background:rgba(14,21,33,.92)}
.overlay .card{max-width:360px}

@media (max-width:340px){.seg{grid-template-columns:repeat(2,minmax(0,1fr))}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style>
</head>
<body>
<svg width="0" height="0" style="position:absolute" aria-hidden="true">
  <symbol id="i-left" viewBox="0 0 24 24"><path d="M19 12H5M11 6l-6 6 6 6"/></symbol>
  <symbol id="i-right" viewBox="0 0 24 24"><path d="M5 12h14M13 6l6 6-6 6"/></symbol>
  <symbol id="i-stop" viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="2"/></symbol>
  <symbol id="i-reset" viewBox="0 0 24 24"><path d="M3 12a9 9 0 1 0 3-6.7M3 4v5h5"/></symbol>
  <symbol id="i-sunrise" viewBox="0 0 24 24"><path d="M12 2v6M8 5l4-3 4 3M4.9 10.9l1.4 1.4M19.1 10.9l-1.4 1.4M2 18h2M20 18h2M7 18a5 5 0 0 1 10 0M2 22h20"/></symbol>
  <symbol id="i-sunset" viewBox="0 0 24 24"><path d="M12 8V2M8 5l4 3 4-3M4.9 10.9l1.4 1.4M19.1 10.9l-1.4 1.4M2 18h2M20 18h2M7 18a5 5 0 0 1 10 0M2 22h20"/></symbol>
  <symbol id="i-bulb" viewBox="0 0 24 24"><path d="M9 18h6M10 21h4M12 3a6 6 0 0 0-3.5 10.9c.6.5 1 1.2 1 2.1h5c0-.9.4-1.6 1-2.1A6 6 0 0 0 12 3z"/></symbol>
  <symbol id="i-clock" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></symbol>
  <symbol id="i-hand" viewBox="0 0 24 24"><path d="M18 11V6a2 2 0 0 0-4 0M14 10V4a2 2 0 0 0-4 0v2M10 10.5V6a2 2 0 0 0-4 0v8M18 8a2 2 0 1 1 4 0v6a8 8 0 0 1-8 8h-2c-2.8 0-4.5-.9-5.9-2.4L3.4 16a2 2 0 0 1 3.2-2.4L8 15"/></symbol>
  <symbol id="i-bolt" viewBox="0 0 24 24"><path d="M13 2L4 14h7l-1 8 9-12h-7l1-8z"/></symbol>
  <symbol id="i-minus" viewBox="0 0 24 24"><path d="M5 12h14"/></symbol>
  <symbol id="i-door" viewBox="0 0 24 24"><path d="M3 11l9-8 9 8M5 10v10h14V10M10 20v-6h4v6"/></symbol>
  <symbol id="i-cycle" viewBox="0 0 24 24"><path d="M21 12a9 9 0 0 1-15.5 6.2M3 12A9 9 0 0 1 18.5 5.8M18 2v4h-4M6 22v-4h4"/></symbol>
  <symbol id="i-pulse" viewBox="0 0 24 24"><path d="M3 12h4l3-8 4 16 3-8h4"/></symbol>
  <symbol id="i-sliders" viewBox="0 0 24 24"><path d="M4 7h9M17 7h3M15 5v4M4 17h3M11 17h9M9 15v4"/></symbol>
  <symbol id="i-plus" viewBox="0 0 24 24"><path d="M5 12h14M12 5v14"/></symbol>
</svg>

<header class="top">
  <div class="brand">
    <svg viewBox="0 0 34 34" aria-hidden="true"><rect width="34" height="34" rx="10" fill="#f4b942"/><path d="M6 17L17 7l11 10z" fill="#0e1521"/><rect x="9" y="17" width="16" height="10" fill="#0e1521"/><rect x="14" y="20" width="6" height="7" fill="#f4b942"/></svg>
    <h1>Poulailler</h1>
  </div>
  <span class="clock" id="heureEsp">--:--</span>
  <div class="rssi" id="rssiEsp" role="img" aria-label="Signal Wi-Fi"><i></i><i></i><i></i><i></i></div>
  <span class="rssi-val" id="rssiVal"></span>
  <div class="pill"><span class="dot" id="dotStatus"></span><span id="statusLabel">Connexion…</span></div>
</header>

<main>

<div class="page" id="page-porte">
  <!-- ── Porte ── -->
  <section class="card hero" aria-label="État de la porte">
    <div class="scene init" id="scene" data-s="closed">
      <svg viewBox="0 0 320 150" aria-hidden="true">
        <defs><clipPath id="clipDoor"><rect x="118" y="60" width="56" height="44"/></clipPath></defs>
        <rect x="0" y="128" width="320" height="22" fill="#101a2a"/>
        <g stroke="#22304a" stroke-width="3" stroke-linecap="round"><path d="M262 130v-26M290 130v-26M318 130v-26"/><path d="M256 110h70M256 120h70" stroke-width="1.5"/></g>
        <path d="M118 100v30M202 100v30" stroke="#3a4f78" stroke-width="7"/>
        <rect x="100" y="52" width="120" height="48" fill="#26354f"/>
        <path d="M120 52v48M140 52v48M160 52v48M180 52v48M200 52v48" stroke="#1f2c44" stroke-width="1.5"/>
        <polygon points="88,54 160,24 232,54" fill="#3a4f78"/>
        <rect x="220" y="64" width="24" height="30" fill="#33466a"/>
        <polygon points="217,64 232,54 247,64" fill="#3a4f78"/>
        <ellipse cx="232" cy="80" rx="6" ry="7" fill="#0a101a"/>
        <rect x="146" y="66" width="28" height="34" fill="#0a101a"/>
        <rect id="glow" x="146" y="66" width="28" height="34" fill="#f4b942" fill-opacity=".22"/>
        <g clip-path="url(#clipDoor)">
          <g id="shutter">
            <rect x="146" y="66" width="28" height="34" fill="#4a5f8c"/>
            <path d="M146 78h28M146 89h28" stroke="#3a4c74" stroke-width="2"/>
          </g>
          <rect x="118" y="62" width="28" height="40" fill="#26354f"/>
          <path d="M120 62v40M140 62v40" stroke="#1f2c44" stroke-width="1.5"/>
        </g>
        <rect x="146" y="66" width="28" height="34" fill="none" stroke="#1a2438" stroke-width="3"/>
        <polygon points="146,100 174,100 198,130 170,130" fill="#2f4162"/>
        <path d="M152 107.5h28M158 115h28M164 122.5h28" stroke="#22304a" stroke-width="2"/>
        <g fill="#4a5f8c"><ellipse cx="72" cy="119" rx="15" ry="10"/><circle cx="57" cy="107" r="6"/><path d="M84 112l12-10-3 16z"/></g>
        <circle cx="57" cy="101.8" r="3" fill="#ff7a6e"/><path d="M51 107l-6 2 6 3z" fill="#f4b942"/><circle cx="55" cy="106" r="1.2" fill="#0e1521"/>
        <path d="M68 129l-1 3M76 129l1 3" stroke="#f4b942" stroke-width="2" stroke-linecap="round"/>
      </svg>
    </div>
    <div class="hero-body">
      <div aria-live="polite">
        <h2 class="state closed" id="porteEtat">--</h2>
        <p class="sub" id="porteSub">En attente…</p>
      </div>
      <div class="progress" id="progressBar"><i id="progressFill"></i></div>
      <a class="modeline" href="#auto" id="modeLine"><svg class="ico"><use id="modeIcon" href="#i-sunrise"/></svg><span id="modeTxt">Mode Soleil</span><svg class="ico chev"><use href="#i-right"/></svg></a>
      <div class="badge" id="overrideBadge" hidden>Mode automatique suspendu : une commande manuelle est active.</div>
    </div>
  </section>

  <!-- ── Commande ── -->
  <section class="card" aria-label="Commande manuelle">
    <h2>Commande manuelle</h2>
    <div class="actions">
      <button class="btn b-open" onclick="envoyerCmd('OPEN')"><svg class="ico"><use href="#i-left"/></svg>Ouvrir</button>
      <button class="btn b-close" onclick="envoyerCmd('CLOSE')">Fermer<svg class="ico"><use href="#i-right"/></svg></button>
      <button class="btn b-stop" id="btnStop" onclick="stopUrgence()"><svg class="ico"><use href="#i-stop"/></svg><span id="stopTxt">Arrêt d'urgence</span></button>
      <button class="btn b-reset" id="btnReset" onclick="resetErreur()"><svg class="ico"><use href="#i-reset"/></svg>Réinitialiser l'erreur</button>
    </div>
  </section>

  <footer><span id="wifiInfo">Wi-Fi —</span><br>ESP32 · mise à jour en direct</footer>
</div>

<div class="page" id="page-auto" hidden>
  <!-- ── Mode ── -->
  <section class="card" aria-label="Mode de gestion">
    <h2>Mode de gestion</h2>
    <div class="seg" role="group" aria-label="Mode">
      <button id="tab-soleil" aria-pressed="true" onclick="selectionnerMode('soleil')"><svg class="ico"><use href="#i-sunrise"/></svg>Soleil</button>
      <button id="tab-luminosite" aria-pressed="false" onclick="selectionnerMode('luminosite')"><svg class="ico"><use href="#i-bulb"/></svg>Luminosité</button>
      <button id="tab-heure_fixe" aria-pressed="false" onclick="selectionnerMode('heure_fixe')"><svg class="ico"><use href="#i-clock"/></svg>Heure fixe</button>
      <button id="tab-manuel" aria-pressed="false" onclick="selectionnerMode('manuel')"><svg class="ico"><use href="#i-hand"/></svg>Manuel</button>
    </div>
    <div class="pending" id="modePendingMsg" hidden>Le changement s'appliquera à la fin du mouvement en cours.</div>

    <!-- Programme du jour (soleil + heure fixe) -->
    <div class="panel" id="plan">
      <div class="band" id="band" role="img" aria-label="Programme sur 24 heures">
        <div class="band-day" id="bandDay"></div>
        <div class="mk o" id="mkOpen"></div>
        <div class="mk c" id="mkClose"></div>
        <div class="now" id="bandNow" title="Maintenant"></div>
      </div>
      <div class="ticks" aria-hidden="true"><span>0 h</span><span>6 h</span><span>12 h</span><span>18 h</span><span>24 h</span></div>
      <div class="times">
        <div class="t"><small><i style="background:var(--open)"></i>Ouvre à</small><b id="hOuverture">--:--</b><em id="hOuvNote"></em></div>
        <div class="t"><small><i style="background:var(--closed)"></i>Ferme à</small><b id="hFermeture">--:--</b><em id="hFerNote"></em></div>
      </div>
    </div>

    <!-- SOLEIL -->
    <div class="panel" id="panel-soleil">
      <div class="sun" style="margin-top:0">
        <span><svg class="ico"><use href="#i-sunrise"/></svg>Lever <b id="hLever">--:--</b></span>
        <span><svg class="ico"><use href="#i-sunset"/></svg>Coucher <b id="hCoucher">--:--</b></span>
      </div>
      <p class="sub-h">Décalages</p>
      <div class="row">
        <div class="lbl"><b>Ouverture</b><span>après le lever, en minutes</span></div>
        <div class="step"><button aria-label="Ouverture : 5 minutes plus tôt" onclick="modOffset('ouv',-5)"><svg class="ico"><use href="#i-minus"/></svg></button><output id="valOffsetOuv">+15</output><button aria-label="Ouverture : 5 minutes plus tard" onclick="modOffset('ouv',5)"><svg class="ico"><use href="#i-plus"/></svg></button></div>
      </div>
      <div class="row">
        <div class="lbl"><b>Fermeture</b><span>après le coucher, en minutes</span></div>
        <div class="step"><button aria-label="Fermeture : 5 minutes plus tôt" onclick="modOffset('fer',-5)"><svg class="ico"><use href="#i-minus"/></svg></button><output id="valOffsetFer">+15</output><button aria-label="Fermeture : 5 minutes plus tard" onclick="modOffset('fer',5)"><svg class="ico"><use href="#i-plus"/></svg></button></div>
      </div>
    </div>

    <!-- LUMINOSITÉ -->
    <div class="panel" id="panel-luminosite" hidden>
      <div class="thr">
        <div class="t"><small><i style="background:var(--open)"></i>Ouvre au-dessus</small><b><span id="valLuxOuv">800</span> lx</b>
          <input type="range" id="sliderLuxOuv" min="100" max="2000" step="50" value="800" aria-label="Seuil d'ouverture en lux" oninput="changeLuxSeuil('ouv',this.value)"></div>
        <div class="t"><small><i style="background:var(--closed)"></i>Ferme sous</small><b><span id="valLuxFer">100</span> lx</b>
          <input type="range" id="sliderLuxFer" min="10" max="500" step="10" value="100" aria-label="Seuil de fermeture en lux" oninput="changeLuxSeuil('fer',this.value)"></div>
      </div>
      <div class="row" style="margin-top:8px">
        <div class="lbl"><b>Confirmation</b><span>durée avant l'action, en secondes</span></div>
        <div class="step"><button aria-label="Confirmation : 10 secondes de moins" onclick="modLuxConf(-10)"><svg class="ico"><use href="#i-minus"/></svg></button><output class="zero" id="valLuxConf">60</output><button aria-label="Confirmation : 10 secondes de plus" onclick="modLuxConf(10)"><svg class="ico"><use href="#i-plus"/></svg></button></div>
      </div>
      <div class="det"><span>Détection</span><div class="bar"><i id="confFill"></i></div><output id="confLabel">--</output></div>
      <p class="live">Luminosité actuelle <b id="luxActuel">--</b> lx</p>
    </div>

    <!-- HEURE FIXE -->
    <div class="panel" id="panel-heure_fixe" hidden>
      <div class="grp">
        <div class="grp-h"><b><i style="background:var(--open)"></i>Ouverture</b><input class="time-input" type="time" id="hfxOuvBase" value="07:30" aria-label="Heure d'ouverture" onchange="sendHfx()"></div>
        <div class="row">
          <div class="lbl"><span>Décalage, en minutes</span></div>
          <div class="step"><button aria-label="Ouverture : 5 minutes plus tôt" onclick="modOffset('hfxOuv',-5)"><svg class="ico"><use href="#i-minus"/></svg></button><output class="zero" id="valOffsetHfxOuv">0</output><button aria-label="Ouverture : 5 minutes plus tard" onclick="modOffset('hfxOuv',5)"><svg class="ico"><use href="#i-plus"/></svg></button></div>
        </div>
      </div>
      <div class="grp">
        <div class="grp-h"><b><i style="background:var(--closed)"></i>Fermeture</b><input class="time-input" type="time" id="hfxFerBase" value="20:00" aria-label="Heure de fermeture" onchange="sendHfx()"></div>
        <div class="row">
          <div class="lbl"><span>Décalage, en minutes</span></div>
          <div class="step"><button aria-label="Fermeture : 5 minutes plus tôt" onclick="modOffset('hfxFer',-5)"><svg class="ico"><use href="#i-minus"/></svg></button><output class="zero" id="valOffsetHfxFer">0</output><button aria-label="Fermeture : 5 minutes plus tard" onclick="modOffset('hfxFer',5)"><svg class="ico"><use href="#i-plus"/></svg></button></div>
        </div>
      </div>
    </div>

    <!-- MANUEL -->
    <div class="panel" id="panel-manuel" hidden>
      <div class="manuel"><svg class="ico"><use href="#i-hand"/></svg><span>La porte ne bouge que sur tes commandes. Utilise les boutons Ouvrir et Fermer.</span></div>
    </div>
  </section>

</div>

<div class="page" id="page-diag" hidden>
  <!-- ── Capteurs ── -->
  <section class="card" aria-label="Capteurs">
    <h2>Capteurs</h2>
    <div class="lim">
      <div id="fdcOuvBox"><i></i><p><b>Fin de course ouvert</b><span id="fdcOuv">--</span></p></div>
      <div id="fdcFerBox"><i></i><p><b>Fin de course fermé</b><span id="fdcFer">--</span></p></div>
    </div>
    <div class="metrics">
      <div class="m"><small><svg class="ico"><use href="#i-bulb"/></svg>Luminosité</small><b id="valLux">--</b><em>lux</em></div>
      <div class="m"><small><svg class="ico"><use href="#i-bolt"/></svg>Courant moteur</small><b id="valCourant">--</b><em>ampères</em></div>
    </div>
  </section>

  <!-- ── Courant ── -->
  <section class="card" aria-label="Courant du moteur">
    <h2>Courant du moteur</h2>
    <div class="chart"><canvas id="chartCourant" role="img" aria-label="Historique du courant du moteur"></canvas></div>
  </section>

  <!-- ── Système ── -->
  <section class="card" aria-label="Système">
    <h2>Système</h2>
    <dl class="kv">
      <div><dt>Fonctionne depuis</dt><dd id="sysUptime">--</dd></div>
      <div><dt>Dernier démarrage</dt><dd id="sysReset">--</dd></div>
      <div><dt>Mémoire libre</dt><dd id="sysHeap">--</dd></div>
      <div><dt>Wi-Fi</dt><dd id="sysWifi">--</dd></div>
      <div><dt>Adresse IP</dt><dd id="sysIp">--</dd></div>
      <div><dt>MQTT</dt><dd id="sysMqtt">--</dd></div>
      <div><dt>Firmware</dt><dd id="sysVer">--</dd></div>
    </dl>
  </section>

  <!-- ── Journal ── -->
  <section class="card" aria-label="Journal des événements">
    <h2>Journal</h2>
    <ul class="log" id="logList"></ul>
  </section>

</div>

<div class="page" id="page-reglages" hidden>
  <!-- ── Verrou des réglages ── -->
  <form class="card" id="reglLock" hidden onsubmit="deverrouiller();return false">
    <h2>Réglages protégés</h2>
    <p class="hint">Saisis le mot de passe pour modifier le Wi-Fi, MQTT ou la position.</p>
    <div class="field"><label for="mdpAcces">Mot de passe</label><input type="password" id="mdpAcces" autocomplete="current-password" maxlength="32"></div>
    <p class="msg err" id="lockMsg" role="alert"></p>
    <button class="btn b-save" type="submit">Déverrouiller</button>
  </form>

  <!-- ── Formulaire des réglages ── -->
  <form id="reglForm" hidden onsubmit="enregistrerReglages(event)" autocomplete="off">
    <div class="alert" id="apBanner" hidden>Tu es connecté au point d'accès de secours : le Wi-Fi habituel est injoignable. Corrige-le ci-dessous puis enregistre.</div>
    <div class="alert" id="noPassBanner" hidden>Aucun mot de passe : toute personne du réseau peut modifier ces réglages. Définis-en un tout en bas.</div>

    <section class="card" aria-label="Wi-Fi">
      <h2>Wi-Fi</h2>
      <div class="field"><label for="fSsid">Nom du réseau (SSID)</label><input type="text" id="fSsid" maxlength="32" autocapitalize="none" autocorrect="off" spellcheck="false"></div>
      <div class="field"><label for="fWifiPass">Mot de passe</label><input type="password" id="fWifiPass" maxlength="63" autocomplete="new-password"><small id="wifiPassHint"></small></div>
    </section>

    <section class="card" aria-label="MQTT">
      <h2>MQTT / Home Assistant</h2>
      <label class="chk"><input type="checkbox" id="fMqttOn" onchange="majMqttChamps()"><span>MQTT activé</span></label>
      <div id="mqttChamps">
        <div class="two">
          <div class="field"><label for="fMqttHost">Serveur</label><input type="text" id="fMqttHost" maxlength="63" placeholder="192.168.1.10" autocapitalize="none" autocorrect="off" spellcheck="false"></div>
          <div class="field"><label for="fMqttPort">Port</label><input type="number" id="fMqttPort" min="1" max="65535" inputmode="numeric"></div>
        </div>
        <div class="field"><label for="fMqttUser">Utilisateur</label><input type="text" id="fMqttUser" maxlength="32" autocapitalize="none" autocorrect="off" spellcheck="false"><small>Vide = broker sans identifiants.</small></div>
        <div class="field"><label for="fMqttPass">Mot de passe</label><input type="password" id="fMqttPass" maxlength="63" autocomplete="new-password"><small id="mqttPassHint"></small></div>
      </div>
    </section>

    <section class="card" aria-label="Position">
      <h2>Position géographique</h2>
      <div class="two">
        <div class="field"><label for="fLat">Latitude</label><input type="number" id="fLat" step="any" min="-90" max="90" inputmode="decimal"></div>
        <div class="field"><label for="fLon">Longitude</label><input type="number" id="fLon" step="any" min="-180" max="180" inputmode="decimal"></div>
      </div>
      <p class="hint">Sert au calcul du lever et du coucher du soleil. Astuce : clic droit sur un point de Google Maps pour copier ses coordonnées. Appliqué immédiatement, sans redémarrage.</p>
    </section>

    <section class="card" aria-label="Sécurité">
      <h2>Mot de passe de ces réglages</h2>
      <div class="field"><label for="fWebPass">Nouveau mot de passe</label><input type="password" id="fWebPass" maxlength="32" autocomplete="new-password"><small>6 à 32 caractères, sans accents. Vide = inchangé.</small></div>
      <div class="field"><label for="fWebPass2">Confirmation</label><input type="password" id="fWebPass2" maxlength="32" autocomplete="new-password"></div>
      <p class="hint">La page n'est pas chiffrée (HTTP) : ce mot de passe protège contre les erreurs et les curieux, pas contre une écoute du réseau local.</p>
    </section>

    <p class="msg" id="reglMsg" role="status"></p>
    <button class="btn b-save" type="submit">Enregistrer</button>
    <p class="hint center">Un changement de Wi-Fi ou de MQTT redémarre la carte (porte immobile uniquement).</p>

    <section class="card" aria-label="Maintenance">
      <h2>Maintenance</h2>
      <button class="btn b-ghost" type="button" onclick="redemarrer()"><svg class="ico"><use href="#i-reset"/></svg>Redémarrer la carte</button>
    </section>
  </form>

  <div class="overlay" id="restartOv" hidden>
    <div class="card">
      <h2>Redémarrage…</h2>
      <p class="hint" id="restartTxt">La carte redémarre. Cette page se rechargera toute seule.</p>
    </div>
  </div>
</div>

</main>

<nav class="tabs" aria-label="Onglets">
  <div class="tabs-in">
    <a href="#porte" id="nav-porte" aria-current="page"><svg class="ico"><use href="#i-door"/></svg>Porte</a>
    <a href="#auto" id="nav-auto"><svg class="ico"><use href="#i-cycle"/></svg>Automatisme</a>
    <a href="#diag" id="nav-diag"><svg class="ico"><use href="#i-pulse"/></svg>Diagnostic</a>
    <a href="#reglages" id="nav-reglages"><svg class="ico"><use href="#i-sliders"/></svg>Réglages</a>
  </div>
</nav>

<script>
// ── État local ───────────────────────────────────────────────
const $ = id => document.getElementById(id);
let ws;
let courantHist = Array(40).fill(0);
let modeSelectionne = 'soleil';
let luxConf = 60;
let offsetOuv = 15, offsetFer = 15;
let offsetHfxOuv = 0, offsetHfxFer = 0;
let luxSeuilOuv = 800, luxSeuilFer = 100;
let stopArmed = false, stopArmedTimer = null;
let etat = '', nowMin = null, leverMin = null, coucherMin = null;
let ouvEspMin = null, ferEspMin = null;

function toMin(s) {
  const p = String(s).split(':'), h = parseInt(p[0]), m = parseInt(p[1]);
  return (isNaN(h) || isNaN(m)) ? null : h * 60 + m;
}
function norm(min) { return ((min % 1440) + 1440) % 1440; }
function hhmm(min) {
  if (min === null) return '--:--';
  const m = norm(min);
  return String(Math.floor(m / 60)).padStart(2, '0') + ':' + String(m % 60).padStart(2, '0');
}

// ── WebSocket ────────────────────────────────────────────────
function connectWS() {
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen = () => {
    setStatus('connected');
    ws.send(JSON.stringify({ cmd: 'GET_STATE' }));
    ajouterLog('Connexion établie avec l\'ESP32', 'ok');
  };
  ws.onclose = () => { setStatus('offline'); setTimeout(connectWS, 3000); };
  ws.onmessage = e => { try { handleMsg(JSON.parse(e.data)); } catch (err) {} };
}
function send(obj) { if (ws && ws.readyState === 1) ws.send(JSON.stringify(obj)); }

// ── Commandes ────────────────────────────────────────────────
function resetErreur() {
  send({ cmd: 'RESET' });
  ajouterLog('Réinitialisation de l\'erreur demandée', 'info');
}
function envoyerCmd(cmd) {
  send({ cmd });
  ajouterLog(cmd === 'OPEN' ? 'Ouverture manuelle' : 'Fermeture manuelle', 'info');
}
function stopUrgence() {
  const btn = $('btnStop'), txt = $('stopTxt');
  if (!stopArmed) {
    stopArmed = true;
    btn.classList.add('armed');
    txt.textContent = 'Confirmer l\'arrêt ?';
    stopArmedTimer = setTimeout(desarmerStop, 3000);
  } else {
    desarmerStop();
    send({ cmd: 'STOP' });
    if (etat === 'opening' || etat === 'closing') {
      // Fige la porte à sa position; l'ESP32 renverra l'état réel
      const sh = $('shutter');
      const tr = getComputedStyle(sh).transform;
      sh.style.transition = 'none';
      sh.style.transform = tr;
      $('scene').dataset.s = 'stopped';
      $('porteEtat').textContent = 'Arrêtée';
      $('porteEtat').className = 'state error';
      $('porteSub').textContent = 'Arrêt d\'urgence. En attente de l\'état réel de la porte.';
      etat = 'stopped';
    }
    $('progressBar').classList.remove('visible');
    setStatus('connected');
    ajouterLog('Arrêt d\'urgence : moteur arrêté', 'warn');
  }
}
function desarmerStop() {
  clearTimeout(stopArmedTimer);
  stopArmed = false;
  $('btnStop').classList.remove('armed');
  $('stopTxt').textContent = 'Arrêt d\'urgence';
}

// ── Mode ─────────────────────────────────────────────────────
const MODES_META = {
  soleil:     { nom: 'Soleil',     icone: '#i-sunrise' },
  luminosite: { nom: 'Luminosité', icone: '#i-bulb' },
  heure_fixe: { nom: 'Heure fixe', icone: '#i-clock' },
  manuel:     { nom: 'Manuel',     icone: '#i-hand' }
};
function afficherMode(mode) {
  modeSelectionne = mode;
  ['soleil', 'luminosite', 'heure_fixe', 'manuel'].forEach(m => {
    $('tab-' + m).setAttribute('aria-pressed', m === mode ? 'true' : 'false');
    $('panel-' + m).hidden = (m !== mode);
  });
  $('plan').hidden = !(mode === 'soleil' || mode === 'heure_fixe');
  const meta = MODES_META[mode] || MODES_META.soleil;
  $('modeIcon').setAttribute('href', meta.icone);
  $('modeTxt').textContent = 'Mode ' + meta.nom;
  majProgramme();
}
function selectionnerMode(mode) {
  if (mode === modeSelectionne) return;
  afficherMode(mode);
  send({ mode });
}
function mettreAJourMode(mode, pending) {
  afficherMode(mode);
  $('modePendingMsg').hidden = !pending;
}

// ── Décalages ────────────────────────────────────────────────
function afficherOffset(id, val) {
  const el = $(id);
  el.textContent = (val > 0 ? '+' : '') + val;
  el.className = val === 0 ? 'zero' : '';
}
function modOffset(type, delta) {
  if (type === 'ouv') {
    offsetOuv = Math.max(-60, Math.min(120, offsetOuv + delta));
    afficherOffset('valOffsetOuv', offsetOuv);
    send({ offset_ouv: offsetOuv });
  } else if (type === 'fer') {
    offsetFer = Math.max(-60, Math.min(120, offsetFer + delta));
    afficherOffset('valOffsetFer', offsetFer);
    send({ offset_fer: offsetFer });
  } else if (type === 'hfxOuv') {
    offsetHfxOuv = Math.max(-120, Math.min(120, offsetHfxOuv + delta));
    afficherOffset('valOffsetHfxOuv', offsetHfxOuv);
    sendHfx();
  } else if (type === 'hfxFer') {
    offsetHfxFer = Math.max(-120, Math.min(120, offsetHfxFer + delta));
    afficherOffset('valOffsetHfxFer', offsetHfxFer);
    sendHfx();
  }
  majProgramme();
}

// ── Luminosité ───────────────────────────────────────────────
function changeLuxSeuil(type, val) {
  if (type === 'ouv') { luxSeuilOuv = parseInt(val); $('valLuxOuv').textContent = luxSeuilOuv; }
  else                { luxSeuilFer = parseInt(val); $('valLuxFer').textContent = luxSeuilFer; }
  send({ lux_ouv: luxSeuilOuv, lux_fer: luxSeuilFer });
  majSousTitre();
}
function modLuxConf(delta) {
  luxConf = Math.max(10, Math.min(300, luxConf + delta));
  $('valLuxConf').textContent = luxConf;
  send({ lux_conf: luxConf });
}

// ── Heure fixe ───────────────────────────────────────────────
function sendHfx() {
  const ouv = $('hfxOuvBase').value.split(':');
  const fer = $('hfxFerBase').value.split(':');
  send({
    hfx_ouv_h: parseInt(ouv[0]), hfx_ouv_m: parseInt(ouv[1]), hfx_ouv_off: offsetHfxOuv,
    hfx_fer_h: parseInt(fer[0]), hfx_fer_m: parseInt(fer[1]), hfx_fer_off: offsetHfxFer
  });
  majProgramme();
}

// ── Programme du jour ────────────────────────────────────────
function horairesEffectifs() {
  let ouv = null, fer = null;
  if (modeSelectionne === 'soleil') {
    ouv = leverMin !== null ? norm(leverMin + offsetOuv) : ouvEspMin;
    fer = coucherMin !== null ? norm(coucherMin + offsetFer) : ferEspMin;
  } else if (modeSelectionne === 'heure_fixe') {
    const o = toMin($('hfxOuvBase').value), f = toMin($('hfxFerBase').value);
    ouv = o !== null ? norm(o + offsetHfxOuv) : null;
    fer = f !== null ? norm(f + offsetHfxFer) : null;
  }
  return { ouv, fer };
}
function majProgramme() {
  const h = horairesEffectifs();
  $('hOuverture').textContent = hhmm(h.ouv);
  $('hFermeture').textContent = hhmm(h.fer);
  const pct = m => (m === null ? 0 : m / 14.4) + '%';
  $('mkOpen').style.left = pct(h.ouv);
  $('mkClose').style.left = pct(h.fer);
  $('mkOpen').hidden = h.ouv === null;
  $('mkClose').hidden = h.fer === null;
  $('bandNow').hidden = nowMin === null;
  $('bandNow').style.left = pct(nowMin);
  const day = $('bandDay');
  day.hidden = leverMin === null || coucherMin === null;
  if (!day.hidden) {
    day.style.left = pct(leverMin);
    day.style.width = (Math.max(0, coucherMin - leverMin) / 14.4) + '%';
  }
  majSousTitre(h);
}
function majSousTitre(h) {
  h = h || horairesEffectifs();
  const el = $('porteSub');
  if (etat === 'opening') { el.textContent = 'Ouverture en cours'; return; }
  if (etat === 'closing') { el.textContent = 'Fermeture en cours'; return; }
  if (etat === 'error')   { el.textContent = 'La porte s\'est arrêtée sur une erreur.'; return; }
  if (etat === 'stopped') return;
  if (etat !== 'open' && etat !== 'closed') { el.textContent = 'En attente…'; return; }
  let t = '';
  if (modeSelectionne === 'manuel') t = 'Mode manuel : la porte attend tes commandes';
  else if (modeSelectionne === 'luminosite') {
    t = etat === 'closed' ? 'Ouvre au-dessus de ' + luxSeuilOuv + ' lx' : 'Ferme sous ' + luxSeuilFer + ' lx';
  } else if (etat === 'closed' && h.ouv !== null) {
    t = (nowMin !== null && nowMin >= h.ouv ? 'Ouvre demain à ' : 'Ouvre à ') + hhmm(h.ouv);
  } else if (etat === 'open' && h.fer !== null) {
    t = (nowMin !== null && nowMin >= h.fer ? 'Fermeture en attente' : 'Ferme à ' + hhmm(h.fer));
  }
  el.textContent = t;
}

// ── Messages ESP32 ───────────────────────────────────────────
function handleMsg(d) {
  if (d.heure !== undefined) { $('heureEsp').textContent = d.heure; nowMin = toMin(d.heure); }
  if (d.rssi !== undefined) majRssi(d.rssi, d.ssid);
  if (d.porte !== undefined) mettreAJourPorte(d.porte);
  if (d.mode !== undefined) mettreAJourMode(d.mode, d.mode_pending);
  if (d.lever !== undefined)   { $('hLever').textContent = d.lever;     leverMin = toMin(d.lever); }
  if (d.coucher !== undefined) { $('hCoucher').textContent = d.coucher; coucherMin = toMin(d.coucher); }
  if (d.ouverture !== undefined) ouvEspMin = toMin(d.ouverture);
  if (d.fermeture !== undefined) ferEspMin = toMin(d.fermeture);
  if (d.offset_ouv !== undefined) { offsetOuv = d.offset_ouv; afficherOffset('valOffsetOuv', offsetOuv); }
  if (d.offset_fer !== undefined) { offsetFer = d.offset_fer; afficherOffset('valOffsetFer', offsetFer); }
  if (d.lux_ouv !== undefined) { luxSeuilOuv = d.lux_ouv; $('valLuxOuv').textContent = luxSeuilOuv; $('sliderLuxOuv').value = luxSeuilOuv; }
  if (d.lux_fer !== undefined) { luxSeuilFer = d.lux_fer; $('valLuxFer').textContent = luxSeuilFer; $('sliderLuxFer').value = luxSeuilFer; }
  if (d.lux_conf !== undefined) { luxConf = d.lux_conf; $('valLuxConf').textContent = luxConf; }
  if (d.hfx_ouv_h !== undefined) {
    $('hfxOuvBase').value = String(d.hfx_ouv_h).padStart(2, '0') + ':' + String(d.hfx_ouv_m || 0).padStart(2, '0');
    offsetHfxOuv = d.hfx_ouv_off || 0;
    afficherOffset('valOffsetHfxOuv', offsetHfxOuv);
  }
  if (d.hfx_fer_h !== undefined) {
    $('hfxFerBase').value = String(d.hfx_fer_h).padStart(2, '0') + ':' + String(d.hfx_fer_m || 0).padStart(2, '0');
    offsetHfxFer = d.hfx_fer_off || 0;
    afficherOffset('valOffsetHfxFer', offsetHfxFer);
  }
  if (d.lux !== undefined) {
    const lux = Math.max(0, Math.round(parseFloat(d.lux)));
    $('valLux').textContent = lux;
    $('luxActuel').textContent = lux;
  }
  if (d.courant !== undefined) {
    const a = parseFloat(d.courant);
    const el = $('valCourant');
    el.textContent = a.toFixed(2);
    el.style.color = a > 2 ? 'var(--err)' : a > 0.8 ? 'var(--yolk)' : 'var(--open)';
    courantHist.push(a);
    if (courantHist.length > 40) courantHist.shift();
    dessinerGraphique();
  }
  if (d.force_manuel !== undefined) $('overrideBadge').hidden = !d.force_manuel;
  if (d.fdc_ouv !== undefined) majFdc('fdcOuv', d.fdc_ouv);
  if (d.fdc_fer !== undefined) majFdc('fdcFer', d.fdc_fer);
  if (d.conf_pct !== undefined) {
    $('confFill').style.width = d.conf_pct + '%';
    $('confLabel').textContent = d.conf_pct < 100 ? d.conf_pct + '%' : 'OK';
  }
  if (d.log) ajouterLog(d.log.msg, d.log.type || 'info');
  if (d.uptime !== undefined) majSysteme(d);
  majProgramme();
}
function majFdc(id, actif) {
  $(id).textContent = actif ? 'Atteint' : 'Libre';
  $(id + 'Box').className = actif ? 'on' : '';
}
function majRssi(r, ssid) {
  const bars = $('rssiEsp').children;
  let n, c, q;
  if (r === 0)       { n = 0; c = 'var(--muted)'; q = ''; }
  else if (r >= -60) { n = 4; c = 'var(--open)'; q = 'excellent'; }
  else if (r >= -70) { n = 3; c = 'var(--open)'; q = 'bon'; }
  else if (r >= -80) { n = 2; c = 'var(--yolk)'; q = 'moyen'; }
  else               { n = 1; c = 'var(--err)'; q = 'faible'; }
  for (let i = 0; i < 4; i++) bars[i].style.background = i < n ? c : 'var(--line)';
  $('rssiVal').textContent = r ? r : '';
  $('rssiEsp').title = r ? r + ' dBm' : 'Pas de Wi-Fi';
  // Pied de page : réseau + force du signal (textContent : pas d'injection HTML via le SSID)
  $('wifiInfo').textContent = r
    ? 'Wi-Fi « ' + (ssid || '?') + ' » · ' + r + ' dBm (' + q + ')'
    : 'Wi-Fi non connecté';
}

// ── État porte ───────────────────────────────────────────────
function mettreAJourPorte(p) {
  const moving = p === 'opening' || p === 'closing';
  const labels = { open: 'Ouverte', closed: 'Fermée', opening: 'Ouverture…', closing: 'Fermeture…', error: 'Erreur' };
  const cls = { open: 'open', closed: 'closed', opening: 'moving', closing: 'moving', error: 'error' };
  etat = p;
  const sc = $('scene'), sh = $('shutter');
  const premier = sc.classList.contains('init');
  sh.style.transition = '';
  sh.style.transform = '';
  sc.dataset.s = p;
  if (premier) requestAnimationFrame(() => requestAnimationFrame(() => sc.classList.remove('init')));

  $('porteEtat').textContent = labels[p] || p;
  $('porteEtat').className = 'state ' + (cls[p] || 'moving');
  $('progressBar').classList.toggle('visible', moving);
  $('btnReset').classList.toggle('show', p === 'error');
  if (moving) {
    const fill = $('progressFill');
    fill.style.animation = 'none';
    void fill.offsetWidth;
    fill.style.animation = '';
  }
  setStatus(moving ? 'moving' : 'connected');
}
function setStatus(s) {
  const dot = $('dotStatus'), lbl = $('statusLabel');
  if (s === 'connected') { dot.className = 'dot'; lbl.textContent = 'Connecté'; }
  if (s === 'moving')    { dot.className = 'dot moving'; lbl.textContent = 'Mouvement'; }
  if (s === 'offline')   { dot.className = 'dot offline'; lbl.textContent = 'Hors ligne'; }
}

// ── Journal ──────────────────────────────────────────────────
function ajouterLog(msg, type) {
  const now = new Date();
  const t = String(now.getHours()).padStart(2, '0') + ':' + String(now.getMinutes()).padStart(2, '0');
  const li = document.createElement('li');
  li.className = type || 'info';
  const tm = document.createElement('time'); tm.textContent = t;
  const sp = document.createElement('span'); sp.textContent = msg;
  li.append(tm, sp);
  const list = $('logList');
  list.prepend(li);
  while (list.children.length > 20) list.removeChild(list.lastChild);
}

// ── Graphique du courant ─────────────────────────────────────
function dessinerGraphique() {
  const c = $('chartCourant');
  const W = c.clientWidth, H = c.clientHeight;
  if (!W || !H) return;
  const dpr = window.devicePixelRatio || 1;
  c.width = W * dpr; c.height = H * dpr;
  const ctx = c.getContext('2d');
  ctx.scale(dpr, dpr);
  const max = 3, pad = 8, pts = courantHist;
  const y = v => H - pad - (Math.min(v, max) / max) * (H - pad * 2);
  const x = i => (i / (pts.length - 1)) * (W - 8);
  ctx.clearRect(0, 0, W, H);
  // Seuil de 2 A
  ctx.setLineDash([5, 5]); ctx.strokeStyle = 'rgba(255,122,110,.7)'; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(0, y(2)); ctx.lineTo(W, y(2)); ctx.stroke();
  ctx.setLineDash([]);
  ctx.fillStyle = '#ff7a6e'; ctx.font = '11px system-ui,sans-serif'; ctx.textAlign = 'right';
  ctx.fillText('seuil 2 A', W - 2, y(2) - 6);
  // Aire
  const g = ctx.createLinearGradient(0, 0, 0, H);
  g.addColorStop(0, 'rgba(244,185,66,.28)'); g.addColorStop(1, 'rgba(244,185,66,0)');
  ctx.beginPath();
  pts.forEach((v, i) => i === 0 ? ctx.moveTo(x(i), y(v)) : ctx.lineTo(x(i), y(v)));
  ctx.lineTo(x(pts.length - 1), H); ctx.lineTo(0, H); ctx.closePath();
  ctx.fillStyle = g; ctx.fill();
  // Courbe
  ctx.beginPath(); ctx.strokeStyle = '#f4b942'; ctx.lineWidth = 2.5; ctx.lineJoin = 'round';
  pts.forEach((v, i) => i === 0 ? ctx.moveTo(x(i), y(v)) : ctx.lineTo(x(i), y(v)));
  ctx.stroke();
  ctx.beginPath(); ctx.arc(x(pts.length - 1), y(pts[pts.length - 1]), 4, 0, Math.PI * 2);
  ctx.fillStyle = '#f4b942'; ctx.fill();
}

// ── Diagnostic : informations système ────────────────────────
function dureeTxt(s) {
  const j = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
  return (j ? j + ' j ' : '') + (j || h ? h + ' h ' : '') + m + ' min';
}
function majSysteme(d) {
  $('sysUptime').textContent = dureeTxt(d.uptime);
  $('sysReset').textContent = d.reset || '--';
  $('sysHeap').textContent = Math.round(d.heap / 1024) + ' Ko';
  $('sysVer').textContent = d.ver || '--';
  $('sysIp').textContent = d.ip || '--';
  $('sysMqtt').textContent = ['Désactivé', 'Déconnecté', 'Connecté'][d.mqtt] || '--';
  let w = d.rssi ? '« ' + (d.ssid || '?') + ' » · ' + d.rssi + ' dBm' : 'Non connecté';
  if (d.ap) w += ' · point d\'accès de secours actif';
  $('sysWifi').textContent = w;
}

// ── Onglets ──────────────────────────────────────────────────
const PAGES = ['porte', 'auto', 'diag', 'reglages'];
function afficherPage(p) {
  PAGES.forEach(n => {
    $('page-' + n).hidden = (n !== p);
    if (n === p) $('nav-' + n).setAttribute('aria-current', 'page');
    else $('nav-' + n).removeAttribute('aria-current');
  });
  window.scrollTo(0, 0);
  try { localStorage.setItem('onglet', p); } catch (e) {}
  if (p === 'diag') dessinerGraphique();      // le canvas n'a pas de taille tant qu'il est masqué
  if (p === 'reglages') chargerReglages();
}
function pageInitiale() {
  const h = location.hash.slice(1);
  if (PAGES.includes(h)) return h;
  let m = null;
  try { m = localStorage.getItem('onglet'); } catch (e) {}
  return PAGES.includes(m) ? m : 'porte';
}
window.addEventListener('hashchange', () => afficherPage(PAGES.includes(location.hash.slice(1)) ? location.hash.slice(1) : 'porte'));

// ── Réglages : Wi-Fi, MQTT, position, mot de passe ───────────
// Le mot de passe saisi reste en mémoire (jamais stocké) et accompagne chaque appel.
let mdpReglages = '';
async function api(url, champs) {
  const r = await fetch(url, { method: 'POST', body: new URLSearchParams(Object.assign({ mdp: mdpReglages }, champs)) });
  let j = {};
  try { j = await r.json(); } catch (e) {}
  return { status: r.status, ok: r.ok, j };
}
function montrerVerrou(st, msg) {
  $('reglLock').hidden = false;
  $('reglForm').hidden = true;
  $('lockMsg').textContent = msg || (st === 429 ? 'Trop d\'essais : patiente une minute.' : (mdpReglages ? 'Mot de passe incorrect.' : ''));
}
function deverrouiller() { mdpReglages = $('mdpAcces').value; $('mdpAcces').value = ''; chargerReglages(); }
function majMqttChamps() {
  const on = $('fMqttOn').checked;
  $('mqttChamps').style.opacity = on ? '1' : '.45';
  $('mqttChamps').querySelectorAll('input').forEach(i => i.disabled = !on);
}
async function chargerReglages() {
  try {
    const r = await api('/api/reseau/lire', {});
    if (r.status === 401 || r.status === 429) return montrerVerrou(r.status);
    if (!r.ok) throw new Error(r.status);
    const c = r.j;
    $('fSsid').value = c.wifi_ssid || '';
    $('fWifiPass').value = '';
    $('wifiPassHint').textContent = c.wifi_pass_set ? 'Laisser vide = inchangé.' : 'Réseau ouvert (aucun mot de passe).';
    $('fMqttOn').checked = !!c.mqtt_on;
    $('fMqttHost').value = c.mqtt_host || '';
    $('fMqttPort').value = c.mqtt_port || 1883;
    $('fMqttUser').value = c.mqtt_user || '';
    $('fMqttPass').value = '';
    $('mqttPassHint').textContent = c.mqtt_pass_set ? 'Laisser vide = inchangé.' : 'Aucun mot de passe enregistré.';
    $('fLat').value = c.lat; $('fLon').value = c.lon;
    $('fWebPass').value = ''; $('fWebPass2').value = '';
    $('apBanner').hidden = !c.ap;
    $('noPassBanner').hidden = !!c.web_pass_set;
    majMqttChamps();
    $('reglLock').hidden = true;
    $('reglForm').hidden = false;
  } catch (e) { montrerVerrou(0, 'Carte injoignable. Réessaie.'); }
}
function msgReglages(txt, erreur) { const m = $('reglMsg'); m.textContent = txt; m.className = 'msg ' + (erreur ? 'err' : 'ok'); }
async function enregistrerReglages(ev) {
  ev.preventDefault();
  const nv = $('fWebPass').value;
  if (nv !== $('fWebPass2').value) return msgReglages('Les deux mots de passe ne correspondent pas.', true);
  const f = {
    wifi_ssid: $('fSsid').value, wifi_pass: $('fWifiPass').value,
    mqtt_on: $('fMqttOn').checked ? '1' : '0', mqtt_host: $('fMqttHost').value.trim(), mqtt_port: $('fMqttPort').value,
    mqtt_user: $('fMqttUser').value, mqtt_pass: $('fMqttPass').value,
    lat: $('fLat').value, lon: $('fLon').value, web_pass_new: nv
  };
  msgReglages('Enregistrement…', false);
  try {
    const r = await api('/api/reseau/ecrire', f);
    if (r.status === 401 || r.status === 429) return montrerVerrou(r.status);
    if (!r.ok || !r.j.ok) return msgReglages(r.j.err || 'Échec de l\'enregistrement.', true);
    if (nv) mdpReglages = nv;
    if (r.j.restart) montrerRedemarrage();
    else { await chargerReglages(); msgReglages('Enregistré.', false); }
  } catch (e) { msgReglages('Carte injoignable.', true); }
}
async function redemarrer() {
  if (!confirm('Redémarrer la carte maintenant ?')) return;
  try {
    const r = await api('/api/redemarrer', {});
    if (r.status === 401 || r.status === 429) return montrerVerrou(r.status);
    if (r.ok) montrerRedemarrage(); else msgReglages(r.j.err || 'Échec.', true);
  } catch (e) { msgReglages('Carte injoignable.', true); }
}
function montrerRedemarrage() {
  $('restartOv').hidden = false;
  let n = 0;
  const t = setInterval(async () => {
    n++;
    try {
      const r = await fetch('/manifest.json', { cache: 'no-store' });
      if (r.ok && n > 3) { clearInterval(t); location.reload(); }
    } catch (e) {}
    if (n === 25) $('restartTxt').textContent = 'La carte met du temps à revenir. Si le Wi-Fi a changé, elle a peut-être une nouvelle adresse IP (à chercher dans Home Assistant ou sur ta box). Sinon, connecte-toi au Wi-Fi « Poulailler-Config » puis ouvre http://192.168.4.1';
  }, 2000);
}

window.addEventListener('load', () => {
  afficherOffset('valOffsetOuv', offsetOuv);
  afficherOffset('valOffsetFer', offsetFer);
  afficherOffset('valOffsetHfxOuv', 0);
  afficherOffset('valOffsetHfxFer', 0);
  afficherMode('soleil');
  afficherPage(pageInitiale());
  dessinerGraphique();
  connectWS();
});
window.addEventListener('resize', dessinerGraphique);
</script>
</body>
</html>
)rawhtml";

// ── Envoi état complet JSON à tous les clients ───────────────
// Appelé uniquement depuis loop() (lecture I2C et calculs hors tâche réseau)
void wsBroadcastState() {
  if (ws.count() == 0) return;

  struct tm timeinfo;
  bool heureOk = heureLocale(&timeinfo);

  HeureSoleil s = {0, 0, 0, 0};
  if (heureOk) {
    s = calculSoleil(
      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
      reseau.lat, reseau.lon
    );
  }

  // Heures effectives selon mode soleil
  int ouvMin = heureEnMinutes(s.leverH,   s.leverM,   offsetOuv);
  int ferMin = heureEnMinutes(s.coucherH, s.coucherM, offsetFer);

  float lux = bh1750.readLightLevel();
  float amp = lireCourant();

  // Avancement de la confirmation luminosité (barre « Détection ») : 0 à 100 %
  int confPct = 0;
  if (modeActuel == MODE_LUMINOSITE && conditionLuxActive && luxConfirmationSec > 0) {
    unsigned long ecoule = millis() - debutConditionLux;
    confPct = (int)min(100UL, ecoule * 100UL / ((unsigned long)luxConfirmationSec * 1000UL));
  }

  char heureStr[6];
  if (heureOk) snprintf(heureStr, sizeof(heureStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  else         snprintf(heureStr, sizeof(heureStr), "--:--");

  // SSID échappé pour le JSON (32 caractères max, " et \ doublés, contrôles ignorés)
  char ssidJson[65] = "";
  if (WiFi.status() == WL_CONNECTED) {
    String ssid = WiFi.SSID();
    size_t j = 0;
    for (size_t i = 0; i < ssid.length() && j < sizeof(ssidJson) - 2; i++) {
      char ch = ssid[i];
      if ((unsigned char)ch < 0x20) continue;
      if (ch == '"' || ch == '\\') ssidJson[j++] = '\\';
      ssidJson[j++] = ch;
    }
    ssidJson[j] = '\0';
  }

  char ipStr[16] = "";
  if (WiFi.status() == WL_CONNECTED) strlcpy(ipStr, WiFi.localIP().toString().c_str(), sizeof(ipStr));
  int etatMqtt = !reseau.mqttOn ? 0 : (mqtt.connected() ? 2 : 1);

  char buf[1100];
  int n = snprintf(buf, sizeof(buf),
    "{\"heure\":\"%s\","
    "\"porte\":\"%s\","
    "\"mode\":\"%s\","
    "\"mode_pending\":%s,"
    "\"lever\":\"%02d:%02d\","
    "\"coucher\":\"%02d:%02d\","
    "\"ouverture\":\"%02d:%02d\","
    "\"fermeture\":\"%02d:%02d\","
    "\"offset_ouv\":%d,"
    "\"offset_fer\":%d,"
    "\"lux_ouv\":%.0f,"
    "\"lux_fer\":%.0f,"
    "\"lux_conf\":%d,"
    "\"hfx_ouv_h\":%d,\"hfx_ouv_m\":%d,\"hfx_ouv_off\":%d,"
    "\"hfx_fer_h\":%d,\"hfx_fer_m\":%d,\"hfx_fer_off\":%d,"
    "\"lux\":%.0f,"
    "\"courant\":%.2f,"
    "\"conf_pct\":%d,"
    "\"force_manuel\":%s,"
    "\"rssi\":%d,"
    "\"ssid\":\"%s\","
    "\"fdc_ouv\":%s,"
    "\"fdc_fer\":%s,"
    "\"uptime\":%lu,"
    "\"heap\":%u,"
    "\"reset\":\"%s\","
    "\"ver\":\"%s\","
    "\"mqtt\":%d,"
    "\"ap\":%s,"
    "\"ip\":\"%s\""
    "}",
    heureStr,
    etatVersStr(etatPorte),
    MODES_STR[modeActuel],
    modePendingActif ? "true" : "false",
    s.leverH, s.leverM,
    s.coucherH, s.coucherM,
    ouvMin / 60, ouvMin % 60,
    ferMin / 60, ferMin % 60,
    offsetOuv,
    offsetFer,
    luxSeuilOuverture,
    luxSeuilFermeture,
    luxConfirmationSec,
    heureFixeOuvH, heureFixeOuvM, offsetFixeOuv,
    heureFixeFerH, heureFixeFerM, offsetFixeFer,
    lux,
    amp,
    confPct,
    forceManuel ? "true" : "false",
    (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0,
    ssidJson,
    digitalRead(PIN_FDC_OUV) == LOW  ? "true" : "false",
    digitalRead(PIN_FDC_FER) == LOW  ? "true" : "false",
    millis() / 1000UL,
    (unsigned)ESP.getFreeHeap(),
    causeRedemarrage(),
    FIRMWARE_VERSION,
    etatMqtt,
    apActif ? "true" : "false",
    ipStr
  );
  if (n < 0 || n >= (int)sizeof(buf)) { Serial.printf("[WS] État trop long (%d) — non envoyé\n", n); return; }
  ws.textAll(buf);
}

// ── Envoi d'un log à tous les clients ───────────────────────
void wsLog(const char* msg, const char* type) {
  if (ws.count() == 0) return;
  char buf[256];
  snprintf(buf, sizeof(buf), "{\"log\":{\"msg\":\"%s\",\"type\":\"%s\"}}", msg, type);
  ws.textAll(buf);
}

// ── File des messages reçus ─────────────────────────────────
// Les callbacks WebSocket tournent dans la tâche réseau (AsyncTCP) : on n'y fait
// que copier le message. Il est exécuté dans loop() (moteur, MQTT, I2C, NVS).
#define WS_MSG_MAX   384
#define WS_FILE_LEN  8
struct MessageWeb { char json[WS_MSG_MAX]; };
static QueueHandle_t fileWeb = nullptr;

void wsRecevoirMessage(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;
  if (len >= WS_MSG_MAX || fileWeb == nullptr) return;
  MessageWeb m;
  memcpy(m.json, data, len);   // copie bornée : pas d'écriture dans le tampon de la bibliothèque
  m.json[len] = 0;
  if (xQueueSend(fileWeb, &m, 0) != pdTRUE) Serial.println("[WS] File pleine — message ignoré");
}

// Traite un message de la page web (appelé depuis loop())
void traiterMessageWeb(const char* json) {
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return;

  // Commande porte
  const char* cmd = doc["cmd"];
  if (cmd) {
    if      (strcmp(cmd, "OPEN")      == 0) commandeOuvrir();
    else if (strcmp(cmd, "CLOSE")     == 0) commandeFermer();
    else if (strcmp(cmd, "STOP")      == 0) stopUrgence();
    else if (strcmp(cmd, "RESET")     == 0) resetErreur();
    else if (strcmp(cmd, "GET_STATE") == 0) wsBroadcastState();
  }

  // Changement de mode
  const char* m = doc["mode"];
  if (m) {
    ModeGestion nouveau;
    if (strVersMode(m, nouveau)) changerMode(nouveau);
  }

  // Configuration — sauvegarde et republication MQTT seulement si une valeur change
  if (appliquerConfigJson(doc)) paramModifie();
}

void traiterFileWeb() {
  if (fileWeb == nullptr) return;
  MessageWeb m;
  while (xQueueReceive(fileWeb, &m, 0) == pdTRUE) traiterMessageWeb(m.json);
}

// ── Callback événements WebSocket (tâche réseau) ─────────────
void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("[WS] Client #%u connecté\n", client->id());
      wsBroadcastDemande = true;   // diffusé par loop()
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Client #%u déconnecté\n", client->id());
      break;
    case WS_EVT_DATA:
      wsRecevoirMessage(arg, data, len);
      break;
    default: break;
  }
}

// ── API des réglages réseau ─────────────────────────────────
// Appels POST (formulaire) protégés par le mot de passe des réglages, s'il est défini.
// Ces handlers tournent dans la tâche réseau : ils valident et copient, puis loop() applique
// (NVS, redémarrage) via fileReseau, porte immobile.
static unsigned long dernierEchecAcces = 0;
static uint8_t       echecsAcces       = 0;

static bool egalConstant(const char* a, const char* b) {   // comparaison à durée constante
  size_t la = strlen(a), lb = strlen(b);
  unsigned char d = (la != lb);
  for (size_t i = 0; i < (la < lb ? la : lb); i++) d |= (unsigned char)(a[i] ^ b[i]);
  return d == 0;
}

// 0 = accès accordé, 1 = refusé, 2 = trop d'essais (blocage 60 s après 5 échecs)
static int verifierAcces(AsyncWebServerRequest* req) {
  if (reseau.webPass[0] == '\0') return 0;
  if (echecsAcces >= 5) {
    if (millis() - dernierEchecAcces < 60000UL) return 2;
    echecsAcces = 0;
  }
  const AsyncWebParameter* p = req->getParam("mdp", true);
  if (p && egalConstant(p->value().c_str(), reseau.webPass)) { echecsAcces = 0; return 0; }
  if (p && p->value().length() > 0) { echecsAcces++; dernierEchecAcces = millis(); }   // une saisie vide n'est pas un essai
  return 1;
}

static void repondreErreur(AsyncWebServerRequest* req, int code, const char* msg) {
  String r = "{\"ok\":false,\"err\":\""; r += msg; r += "\"}";
  req->send(code, "application/json", r);
}

static bool refuserSiBloque(AsyncWebServerRequest* req) {
  int acces = verifierAcces(req);
  if (acces == 0) return false;
  repondreErreur(req, acces == 2 ? 429 : 401, acces == 2 ? "Trop d'essais" : "Mot de passe requis");
  return true;
}

static bool texteSain(const String& v, size_t maxLen) {   // pas de caractère de contrôle
  if (v.length() > maxLen) return false;
  for (size_t i = 0; i < v.length(); i++) { unsigned char c = v[i]; if (c < 0x20 || c == 0x7F) return false; }
  return true;
}
static bool hoteValide(const String& v) {
  if (v.length() < 1 || v.length() > 63) return false;
  for (size_t i = 0; i < v.length(); i++) {
    char c = v[i];
    if (!(isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_')) return false;
  }
  return true;
}
static bool lireNombre(const String& v, float mn, float mx, float& out) {
  String t = v; t.replace(',', '.');
  char* fin; float f = strtof(t.c_str(), &fin);
  if (t.length() == 0 || fin == t.c_str() || *fin != '\0' || isnan(f) || f < mn || f > mx) return false;
  out = f; return true;
}

// Remplit « n » (copie de la config courante) avec les champs reçus ; retourne un message d'erreur ou nullptr
static const char* lireFormulaire(AsyncWebServerRequest* req, ReseauCfg& n) {
  auto champ = [&](const char* k, String& out) -> bool {
    const AsyncWebParameter* p = req->getParam(k, true);
    if (!p) return false;
    out = p->value(); return true;
  };
  String v, mdp;

  if (champ("wifi_ssid", v)) {
    if (v.length() < 1 || v.length() > 32 || !texteSain(v, 32)) return "Nom du Wi-Fi : 1 à 32 caractères.";
    bool mdpDonne = champ("wifi_pass", mdp) && mdp.length() > 0;
    if (mdpDonne) {
      if (mdp.length() < 8 || mdp.length() > 63 || !texteSain(mdp, 63)) return "Mot de passe Wi-Fi : 8 à 63 caractères.";
      strlcpy(n.wifiPass, mdp.c_str(), sizeof(n.wifiPass));
    } else if (v != reseau.wifiSsid) {
      n.wifiPass[0] = '\0';          // nouveau réseau sans mot de passe saisi : réseau ouvert
    }
    strlcpy(n.wifiSsid, v.c_str(), sizeof(n.wifiSsid));
  }

  if (champ("mqtt_on", v)) n.mqttOn = (v == "1");
  if (champ("mqtt_host", v)) {
    if (v.length() > 0) {
      if (!hoteValide(v)) return "Serveur MQTT : adresse invalide.";
      strlcpy(n.mqttHost, v.c_str(), sizeof(n.mqttHost));
    } else if (n.mqttOn) return "Serveur MQTT requis.";
  }
  if (champ("mqtt_port", v)) {
    float f;
    if (!lireNombre(v, 1, 65535, f) || f != (int)f) return "Port MQTT : entre 1 et 65535.";
    n.mqttPort = (int)f;
  }
  if (champ("mqtt_user", v)) {
    if (!texteSain(v, 32)) return "Utilisateur MQTT : 32 caractères maximum.";
    bool mdpDonne = champ("mqtt_pass", mdp) && mdp.length() > 0;
    if (mdpDonne && (!texteSain(mdp, 63))) return "Mot de passe MQTT : 63 caractères maximum.";
    if (v.length() == 0) {             // pas d'utilisateur : broker anonyme
      n.mqttUser[0] = '\0'; n.mqttPass[0] = '\0';
    } else {
      if (mdpDonne) strlcpy(n.mqttPass, mdp.c_str(), sizeof(n.mqttPass));
      else if (v != reseau.mqttUser) n.mqttPass[0] = '\0';
      strlcpy(n.mqttUser, v.c_str(), sizeof(n.mqttUser));
    }
  }

  float f;
  if (champ("lat", v)) { if (!lireNombre(v, -90, 90, f))   return "Latitude : entre -90 et 90.";   n.lat = f; }
  if (champ("lon", v)) { if (!lireNombre(v, -180, 180, f)) return "Longitude : entre -180 et 180."; n.lon = f; }

  if (champ("web_pass_new", v) && v.length() > 0) {
    bool ascii = true;
    for (size_t i = 0; i < v.length(); i++) { unsigned char c = v[i]; if (c < 0x20 || c > 0x7E) ascii = false; }
    if (v.length() < MDP_WEB_MIN || v.length() > 32 || !ascii) return "Mot de passe : 6 à 32 caractères, sans accents.";
    strlcpy(n.webPass, v.c_str(), sizeof(n.webPass));
  }
  return nullptr;
}

static void routesReglages() {
  // Lecture : jamais de mot de passe renvoyé, seulement s'il est défini
  server.on("/api/reseau/lire", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (refuserSiBloque(req)) return;
    JsonDocument doc;
    doc["wifi_ssid"]      = reseau.wifiSsid;
    doc["wifi_pass_set"]  = reseau.wifiPass[0] != '\0';
    doc["mqtt_on"]        = reseau.mqttOn;
    doc["mqtt_host"]      = reseau.mqttHost;
    doc["mqtt_port"]      = reseau.mqttPort;
    doc["mqtt_user"]      = reseau.mqttUser;
    doc["mqtt_pass_set"]  = reseau.mqttPass[0] != '\0';
    doc["lat"]            = reseau.lat;
    doc["lon"]            = reseau.lon;
    doc["web_pass_set"]   = reseau.webPass[0] != '\0';
    doc["ap"]             = apActif;
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/reseau/ecrire", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (refuserSiBloque(req)) return;
    ReseauCfg n = reseau;
    const char* err = lireFormulaire(req, n);
    if (err) { repondreErreur(req, 400, err); return; }
    bool wifiChg = strcmp(n.wifiSsid, reseau.wifiSsid) != 0 || strcmp(n.wifiPass, reseau.wifiPass) != 0;
    bool mqttChg = n.mqttOn != reseau.mqttOn || n.mqttPort != reseau.mqttPort
                || strcmp(n.mqttHost, reseau.mqttHost) != 0 || strcmp(n.mqttUser, reseau.mqttUser) != 0
                || strcmp(n.mqttPass, reseau.mqttPass) != 0;
    if (fileReseau == nullptr || xQueueSend(fileReseau, &n, 0) != pdTRUE) {
      repondreErreur(req, 409, "Une modification est déjà en cours, réessaie.");
      return;
    }
    req->send(200, "application/json", (wifiChg || mqttChg) ? "{\"ok\":true,\"restart\":true}" : "{\"ok\":true,\"restart\":false}");
  });

  server.on("/api/redemarrer", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (refuserSiBloque(req)) return;
    demanderRedemarrage();    // exécuté par loop() quand la porte est immobile
    req->send(200, "application/json", "{\"ok\":true}");
  });
}

// ── Initialisation — à appeler dans setup() après WiFi ───────
void setupWebServer() {
  fileWeb = xQueueCreate(WS_FILE_LEN, sizeof(MessageWeb));
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", HTML_PAGE);
  });

  // Icônes et manifeste : permettent un raccourci « écran d'accueil » avec icône
  server.on("/icon-192.png", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(200, "image/png", ICONE_192, ICONE_192_LEN);
    r->addHeader("Cache-Control", "max-age=604800");
    req->send(r);
  });
  server.on("/icon-512.png", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(200, "image/png", ICONE_512, ICONE_512_LEN);
    r->addHeader("Cache-Control", "max-age=604800");
    req->send(r);
  });
  server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/manifest+json",
      "{\"name\":\"Poulailler\",\"short_name\":\"Poulailler\",\"start_url\":\"/\","
      "\"display\":\"standalone\",\"background_color\":\"#0e1521\",\"theme_color\":\"#0e1521\","
      "\"icons\":[{\"src\":\"/icon-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any maskable\"},"
      "{\"src\":\"/icon-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]}");
  });

  routesReglages();

  server.begin();
  Serial.printf("[WEB] http://%s\n", (apActif ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str());
}
