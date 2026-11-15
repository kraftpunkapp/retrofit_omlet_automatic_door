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
body{background:var(--bg);color:var(--text);font:400 15px/1.4 var(--font);min-height:100vh;padding-bottom:40px;font-variant-numeric:tabular-nums}
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

  <!-- ── Journal ── -->
  <section class="card" aria-label="Journal des événements">
    <h2>Journal</h2>
    <ul class="log" id="logList"></ul>
  </section>

  <footer>ESP32 · mise à jour en direct</footer>
</main>

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
function afficherMode(mode) {
  modeSelectionne = mode;
  ['soleil', 'luminosite', 'heure_fixe', 'manuel'].forEach(m => {
    $('tab-' + m).setAttribute('aria-pressed', m === mode ? 'true' : 'false');
    $('panel-' + m).hidden = (m !== mode);
  });
  $('plan').hidden = !(mode === 'soleil' || mode === 'heure_fixe');
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
  if (d.rssi !== undefined) majRssi(d.rssi);
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
  majProgramme();
}
function majFdc(id, actif) {
  $(id).textContent = actif ? 'Atteint' : 'Libre';
  $(id + 'Box').className = actif ? 'on' : '';
}
function majRssi(r) {
  const bars = $('rssiEsp').children;
  let n, c;
  if (r === 0)       { n = 0; c = 'var(--muted)'; }
  else if (r >= -60) { n = 4; c = 'var(--open)'; }
  else if (r >= -70) { n = 3; c = 'var(--open)'; }
  else if (r >= -80) { n = 2; c = 'var(--yolk)'; }
  else               { n = 1; c = 'var(--err)'; }
  for (let i = 0; i < 4; i++) bars[i].style.background = i < n ? c : 'var(--line)';
  $('rssiVal').textContent = r ? r : '';
  $('rssiEsp').title = r ? r + ' dBm' : 'Pas de Wi-Fi';
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

window.addEventListener('load', () => {
  afficherOffset('valOffsetOuv', offsetOuv);
  afficherOffset('valOffsetFer', offsetFer);
  afficherOffset('valOffsetHfxOuv', 0);
  afficherOffset('valOffsetHfxFer', 0);
  afficherMode('soleil');
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
      LATITUDE, LONGITUDE
    );
  }

  // Heures effectives selon mode soleil
  int ouvMin = heureEnMinutes(s.leverH,   s.leverM,   offsetOuv);
  int ferMin = heureEnMinutes(s.coucherH, s.coucherM, offsetFer);

  float lux = bh1750.readLightLevel();
  float amp = lireCourant();

  char heureStr[6];
  if (heureOk) snprintf(heureStr, sizeof(heureStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  else         snprintf(heureStr, sizeof(heureStr), "--:--");

  char buf[800];
  snprintf(buf, sizeof(buf),
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
    "\"force_manuel\":%s,"
    "\"rssi\":%d,"
    "\"fdc_ouv\":%s,"
    "\"fdc_fer\":%s"
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
    forceManuel ? "true" : "false",
    (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0,
    digitalRead(PIN_FDC_OUV) == LOW  ? "true" : "false",
    digitalRead(PIN_FDC_FER) == LOW  ? "true" : "false"
  );
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

// ── Initialisation — à appeler dans setup() après WiFi ───────
void setupWebServer() {
  fileWeb = xQueueCreate(WS_FILE_LEN, sizeof(MessageWeb));
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", HTML_PAGE);
  });

  server.begin();
  Serial.printf("[WEB] http://%s\n", WiFi.localIP().toString().c_str());
}
