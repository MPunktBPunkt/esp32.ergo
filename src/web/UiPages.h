#pragma once

#include <Arduino.h>

/**
 * Die Reiterstruktur aus docs/ergometer/WEBINTERFACE.md §7, vollstaendig:
 * Ride, Workouts, Tests, Verlauf, Profile, Geraete, Kalibrierung, Debug,
 * Einstellungen, OTA.
 *
 * Alle zehn Reiter tragen Inhalt. NAV-Eintraege mit Zielversion oeffnen weiter
 * den Soon-Platzhalter — derzeit ungenutzt.
 *
 * Die Reiterliste `NAV` ist die einzige Quelle dafuer — Leiste und Platzhalter
 * werden daraus erzeugt. Ein Reiter mehr ist eine Zeile, nicht drei Stellen.
 *
 * Drei Punkte aus dem Konzept sind hier schon umgesetzt, weil die Daten dafuer
 * da sind: die Stufen-Kachel mit Segmenten, Reserve und rotem Rand bei
 * erreichter Decke (§3 „Ehrlichkeitsanzeige"), die Kennzeichnung der Stufe
 * als Schattenwert per `~` — das Bike meldet sie nicht zurueck —, und die
 * Kennflaeche als Heatmap mit sichtbarem Unterschied zwischen gefuehrt
 * gemessenen und beim Fahren gelernten Zellen.
 *
 * Nicht umgesetzt und bewusst nicht erfunden: Geisterlinie. Tablet-Ride-Dock
 * (Stufe/STOP sticky), Hero-Zweispalter und Zonen-Hysterese sind umgesetzt.
 *
 * Ohne JavaScript zeigt die Seite alle Abschnitte untereinander und das
 * OTA-Formular sendet native — das ist der Grund fuer `body.js` statt
 * `hidden`-Attribute. Diese Seite ist der Rueckweg nach einem Fehlflash; sie
 * darf nicht an einem Skriptfehler haengen.
 *
 * Die Seite liegt vollstaendig im Flash, deshalb keine Kommentare im CSS und
 * keine Leerzeilen zur Zierde. Merkregeln: `.v.mac` bricht mitten im Wort,
 * `.v` nicht; Werte auf mehreren Reitern laufen ueber `j-`Klassen statt IDs.
 */
static const char PAGE_MAIN[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ERGO</title>
<style>
:root{
  --bg:#0E1116; --card:#161A21; --edge:#232936;
  --fg:#E6EAF2; --dim:#8A94A6; --accent:#E2802F; --ok:#4CAF63; --bad:#C9304A;
  --zone:#8A94A6;
  --z1:#3FB8B0; --z2:#4CAF63; --z3:#D8B23A; --z4:#E2802F;
  --z5:#DE5334; --z6:#C9304A; --z7:#A63FB0;
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);
  font-family:'IBM Plex Mono',ui-monospace,SFMono-Regular,Menlo,monospace;
  font-size:15px;line-height:1.5;padding:24px;
  transition:background .4s ease}
body.z1{background:radial-gradient(ellipse 90% 55% at 30% 12%,rgba(63,184,176,.16),transparent 55%),var(--bg);--zone:var(--z1);--accent:var(--z1)}
body.z2{background:radial-gradient(ellipse 90% 55% at 30% 12%,rgba(76,175,99,.16),transparent 55%),var(--bg);--zone:var(--z2);--accent:var(--z2)}
body.z3{background:radial-gradient(ellipse 90% 55% at 30% 12%,rgba(216,178,58,.16),transparent 55%),var(--bg);--zone:var(--z3);--accent:var(--z3)}
body.z4{background:radial-gradient(ellipse 90% 55% at 30% 12%,rgba(226,128,47,.16),transparent 55%),var(--bg);--zone:var(--z4);--accent:var(--z4)}
body.z5{background:radial-gradient(ellipse 90% 55% at 30% 12%,rgba(222,83,52,.16),transparent 55%),var(--bg);--zone:var(--z5);--accent:var(--z5)}
body.z6{background:radial-gradient(ellipse 90% 55% at 30% 12%,rgba(201,48,74,.16),transparent 55%),var(--bg);--zone:var(--z6);--accent:var(--z6)}
body.z7{background:radial-gradient(ellipse 90% 55% at 30% 12%,rgba(166,63,176,.16),transparent 55%),var(--bg);--zone:var(--z7);--accent:var(--z7)}
.v.hero{font-size:46px;font-weight:800;line-height:1.1;color:var(--zone);
  text-shadow:0 0 28px var(--zone)}
.zonebadge{display:inline-flex;align-items:baseline;gap:8px;margin-top:6px;
  font-family:'Syne',system-ui,sans-serif;letter-spacing:.06em}
.zonebadge .zc{font-size:18px;font-weight:800;color:var(--zone)}
.zonebadge .zn{font-size:13px;color:var(--dim);text-transform:uppercase}
.zrail{margin-top:16px;padding-top:14px;border-top:1px solid var(--edge)}
.zrail .segs{display:flex;gap:4px;margin:8px 0 6px;height:14px}
.zrail .segs i{flex:1;position:relative;border-radius:3px;background:var(--edge);
  overflow:hidden}
.zrail .segs i b{display:block;height:100%;width:0;border-radius:3px;
  transition:width .4s ease}
.zrail .segs i.cur{outline:2px solid var(--fg);outline-offset:1px}
.zrail .segs i:nth-child(1) b{background:var(--z1)}
.zrail .segs i:nth-child(2) b{background:var(--z2)}
.zrail .segs i:nth-child(3) b{background:var(--z3)}
.zrail .segs i:nth-child(4) b{background:var(--z4)}
.zrail .segs i:nth-child(5) b{background:var(--z5)}
.zrail .segs i:nth-child(6) b{background:var(--z6)}
.zrail .segs i:nth-child(7) b{background:var(--z7)}
.zlabels{display:flex;gap:4px;font-size:10px;color:var(--dim);letter-spacing:.04em}
.zlabels span{flex:1;text-align:center;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.zmini{display:flex;gap:2px;height:8px;margin-top:6px}
.zmini i{flex:1;border-radius:2px;background:var(--edge);overflow:hidden}
.zmini i b{display:block;height:100%;width:0}
.zmini i:nth-child(1) b{background:var(--z1)}
.zmini i:nth-child(2) b{background:var(--z2)}
.zmini i:nth-child(3) b{background:var(--z3)}
.zmini i:nth-child(4) b{background:var(--z4)}
.zmini i:nth-child(5) b{background:var(--z5)}
.zmini i:nth-child(6) b{background:var(--z6)}
.zmini i:nth-child(7) b{background:var(--z7)}
.wrap{max-width:980px;margin:0 auto}
header{display:flex;align-items:baseline;gap:14px;margin-bottom:4px;flex-wrap:wrap}
h1{font-family:'Syne',system-ui,sans-serif;font-size:30px;letter-spacing:.14em;
  margin:0;font-weight:800}
.badge{font-size:12px;color:var(--accent);border:1px solid var(--accent);
  border-radius:999px;padding:2px 10px}
.sub{color:var(--dim);font-size:13px;margin-bottom:18px}
nav{display:flex;flex-wrap:wrap;gap:3px;margin-bottom:18px;background:var(--card);
  border:1px solid var(--edge);border-radius:12px;padding:4px}
.tab{background:transparent;color:var(--dim);border:0;border-radius:9px;
  padding:11px 15px;font:inherit;font-size:13px;font-weight:700;letter-spacing:.05em;
  white-space:nowrap;min-height:0;cursor:pointer}
.tab:hover{color:var(--fg)}
.tab.on{background:var(--accent);color:#160B02}
.tab.soon{opacity:.5}
body:not(.js) nav{display:none}
body.js section{display:none}
body.js section.on{display:block}
.ridedock{position:sticky;top:0;z-index:30;display:none;gap:10px;align-items:stretch;
  flex-wrap:wrap;margin:0 0 14px;padding:10px;background:rgba(14,17,22,.92);
  border:1px solid var(--edge);border-radius:12px;backdrop-filter:blur(8px)}
#t-ride.on .ridedock{display:flex}
.ridedock .docklvl{flex:1;min-width:120px;display:flex;flex-direction:column;justify-content:center}
.ridedock .docklvl .v{font-size:22px;font-weight:800;color:var(--zone)}
.ridedock button{flex:1;min-width:96px;min-height:52px;font-size:16px}
.ridedock button.danger{flex:1.1}
.ridehero{display:grid;grid-template-columns:1.2fr .9fr;gap:16px;align-items:start}
@media (max-width:720px){.ridehero{grid-template-columns:1fr}}
.ridehero .heroBlock .v.hero{font-size:64px}
.connline{display:flex;flex-wrap:wrap;gap:14px 22px;align-items:center;font-size:13px}
.connline .v{font-size:14px}
.modes{display:flex;flex-wrap:wrap;gap:8px;margin:0 0 12px}
.modes button{min-height:48px}
.advbox{margin-top:12px;padding-top:12px;border-top:1px solid var(--edge)}
.advbox summary{cursor:pointer;color:var(--dim);font-size:12px;letter-spacing:.08em;
  text-transform:uppercase;list-style:none}
.advbox summary::-webkit-details-marker{display:none}
.woedrow{display:grid;grid-template-columns:1.4fr .7fr .7fr .55fr .55fr auto;gap:6px;
  align-items:end;margin:8px 0;padding:8px;background:#0E1116;border:1px solid var(--edge);
  border-radius:9px}
.woedrow .k{margin-bottom:3px}
.woedrow input{padding:8px 9px;font-size:14px}
.woedrow .ops{display:flex;gap:4px;flex-wrap:wrap}
.woedrow .ops button{min-height:36px;padding:6px 10px;font-size:12px}
@media (max-width:720px){
  .woedrow{grid-template-columns:1fr 1fr}
  .woedrow .ops{grid-column:1/-1}
}
.card{background:var(--card);border:1px solid var(--edge);border-radius:12px;
  padding:18px 20px;margin-bottom:16px}
.card h2{font-family:'Syne',system-ui,sans-serif;font-size:13px;letter-spacing:.18em;
  text-transform:uppercase;color:var(--dim);margin:0 0 14px;font-weight:700}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:14px}
.fgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:0 16px}
.k{color:var(--dim);font-size:11px;letter-spacing:.09em;text-transform:uppercase}
.v{font-size:17px;font-variant-numeric:tabular-nums;overflow-wrap:break-word}
.v.mac{word-break:break-all}
.v.addr{font-size:15px}
.v.big{font-size:26px;font-weight:700}
.v.ceil{color:var(--bad)}
.tile{border:1px solid transparent;border-radius:9px;padding:8px;margin:-8px}
.tile.cap{border-color:var(--bad)}
.segs{display:flex;gap:3px;margin:9px 0 7px}
.segs i{flex:1;height:10px;border-radius:2px;background:var(--edge)}
.segs i.on{background:var(--accent)}
.capbar{height:8px;background:var(--edge);border-radius:4px;margin-top:8px;overflow:hidden;
  position:relative}
.capbar i{display:block;height:100%;background:var(--ok);width:0;transition:width .3s linear}
.capbar.soft i{background:var(--accent)}
.capbar.hard i{background:var(--bad)}
.capbar .mark{position:absolute;top:0;bottom:0;width:2px;background:rgba(230,234,242,.55)}
.reg{margin-top:16px;padding-top:14px;border-top:1px solid var(--edge)}
.reg canvas{display:block;width:100%;height:48px;margin-top:8px;background:#0E1116;
  border-radius:8px;border:1px solid var(--edge)}
.msg.warn{color:var(--accent)}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:7px;
  vertical-align:middle;background:var(--bad)}
.dot.on{background:var(--ok)}
button{font:inherit;background:var(--accent);color:#160B02;border:0;border-radius:10px;
  padding:13px 26px;font-weight:700;cursor:pointer;min-height:44px}
button:disabled{opacity:.45;cursor:default}
button.ghost{background:transparent;color:var(--fg);border:1px solid var(--edge)}
button.ghost:hover:enabled{border-color:var(--accent);color:var(--accent)}
button.danger{background:var(--bad);color:#fff}
button.sm{padding:8px 14px;font-size:12px;min-height:0}
.row{display:flex;flex-wrap:wrap;gap:10px;margin-top:16px}
.row.tight{margin-top:10px}
.row.flat{margin-top:0}
label.f{display:block;margin-bottom:14px}
label.f.wide{grid-column:1/-1}
label.f .k{margin-bottom:5px}
input[type=text],input[type=number],select{width:100%;font:inherit;font-size:15px;
  background:#0E1116;color:var(--fg);border:1px solid var(--edge);border-radius:9px;
  padding:11px 12px}
input[type=text]:focus,input[type=number]:focus,select:focus{outline:0;border-color:var(--accent)}
select{appearance:none;-webkit-appearance:none}
.chk{display:flex;align-items:center;gap:10px;margin-bottom:14px;cursor:pointer;
  font-size:14px}
.chk input{width:18px;height:18px;accent-color:var(--accent);margin:0}
.hint{color:var(--dim);font-size:12px;margin:-6px 0 16px}
.hint.flat{margin:12px 0 0}
input[type=file]{position:absolute;width:1px;height:1px;opacity:0;pointer-events:none}
.drop{display:block;padding:22px;margin-bottom:16px;text-align:center;cursor:pointer;
  background:#0E1116;border:1px dashed var(--edge);border-radius:10px;color:var(--dim)}
.drop:hover{border-color:var(--accent);color:var(--fg)}
.drop b{display:block;color:var(--fg);font-weight:400;margin-bottom:4px}
.drop.set{border-style:solid;border-color:var(--accent)}
table{width:100%;border-collapse:collapse;margin-top:14px;font-size:13px}
table:empty{margin:0}
td{padding:9px 6px;border-top:1px solid var(--edge)}
td.r{text-align:right;color:var(--dim);white-space:nowrap}
tr.pick{cursor:pointer}
tr.pick:hover td{color:var(--accent)}
.tag{font-size:10px;letter-spacing:.08em;text-transform:uppercase;border:1px solid var(--edge);
  border-radius:999px;padding:1px 7px;margin-left:7px;color:var(--dim)}
.tag.ftms{border-color:var(--accent);color:var(--accent)}
.msg{margin-top:12px;color:var(--dim);font-size:13px;min-height:19px}
.msg.err{color:var(--bad)} .msg.ok{color:var(--ok)}
.hm{display:grid;gap:2px;margin-top:14px;font-size:10px;font-variant-numeric:tabular-nums}
.hm div{border-radius:3px;padding:6px 2px;text-align:center;overflow:hidden}
.hm .hd{background:transparent;color:var(--dim);letter-spacing:.05em;padding:3px 0}
.hm .e{background:#1A1F28}
.hm .sw{box-shadow:inset 0 0 0 1px rgba(230,234,242,.55)}
.bar{height:6px;background:var(--edge);border-radius:3px;margin-top:16px;overflow:hidden}
.bar i{display:block;height:100%;background:var(--accent);width:0;
  transition:width .4s linear}
.v.cadok{color:var(--ok)} .v.cadbad{color:var(--bad)}
.v.ok{color:var(--ok)}
/* Der Widerspruch „quittiert, aber wirkungslos" bekommt eigenes Gewicht — er
   ist der Befund, der die letzte Hardware-Session gekostet hat. */
.warn{margin-top:12px;padding:10px 12px;border-radius:6px;font-size:13px;
  color:#F3C6CF;background:rgba(201,48,74,.14);border:1px solid rgba(201,48,74,.45)}
.q{font-style:italic;color:var(--fg)}
.vd{display:inline-block;padding:1px 8px;border-radius:10px;font-size:11px;
  letter-spacing:.04em;background:var(--edge);color:var(--dim);white-space:nowrap}
.vd.w{background:rgba(76,175,99,.18);color:var(--ok)}
.vd.n{background:rgba(201,48,74,.18);color:var(--bad)}
.soonbox{text-align:center;padding:34px 20px;color:var(--dim)}
.soonbox b{display:block;font-family:'Syne',system-ui,sans-serif;font-size:20px;
  letter-spacing:.1em;text-transform:uppercase;color:var(--fg);margin-bottom:8px}
.soonbox .d{display:block;max-width:48ch;margin:0 auto}
.soonbox .ver{display:table;margin:16px auto 0;font-size:12px;color:var(--accent);
  border:1px solid var(--accent);border-radius:999px;padding:2px 12px}
.pcards{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px;
  margin:14px 0}
.pcard{background:#0E1116;border:2px solid var(--edge);border-radius:14px;padding:16px 14px;
  cursor:pointer;text-align:center;transition:border-color .2s,transform .15s}
.pcard:hover{border-color:var(--accent)}
.pcard.on{border-color:var(--pc,var(--accent));box-shadow:0 0 0 1px var(--pc,var(--accent))}
.pcard .av{width:52px;height:52px;border-radius:50%;margin:0 auto 10px;display:flex;
  align-items:center;justify-content:center;font-family:'Syne',system-ui,sans-serif;
  font-size:22px;font-weight:800;color:#0E1116}
.pcard .pn{font-family:'Syne',system-ui,sans-serif;font-size:16px;font-weight:700;
  letter-spacing:.04em}
.pcard .pm{color:var(--dim);font-size:11px;margin-top:6px;line-height:1.4}
.pcard .prow{display:flex;justify-content:center;gap:6px;margin-top:10px;flex-wrap:wrap}
.chip{display:inline-flex;align-items:center;gap:8px;padding:4px 10px 4px 4px;
  border-radius:999px;border:1px solid var(--edge);background:var(--card);font-size:13px}
.chip .av{width:28px;height:28px;border-radius:50%;display:inline-flex;align-items:center;
  justify-content:center;font-family:'Syne',system-ui,sans-serif;font-size:12px;font-weight:800;
  color:#0E1116}
.colorrow{display:flex;gap:8px;flex-wrap:wrap;margin-top:6px}
.colorrow button{width:28px;height:28px;min-height:0;padding:0;border-radius:50%;
  border:2px solid transparent;cursor:pointer}
.colorrow button.on{border-color:#fff}
footer{color:var(--dim);font-size:12px;text-align:center;margin-top:26px}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1>ERGO</h1>
    <span class="badge" id="ver">-</span>
    <span class="chip" id="pchip" hidden>
      <span class="av" id="pchipav">?</span>
      <span id="pchipname">—</span>
    </span>
  </header>
  <div class="sub" id="sub">verbinde...</div>
  <nav id="nav"></nav>

  <section id="t-ride">
    <div class="ridedock" id="ridedock">
      <div class="docklvl"><div class="k">Stufe</div><div class="v" id="docklvl">-</div></div>
      <button id="lvldn" class="ghost">Stufe −</button>
      <button id="lvlup" class="ghost">Stufe +</button>
      <button id="panic" class="danger">STOP</button>
    </div>
    <div class="card">
      <div class="connline">
        <div><span class="dot j-bdot"></span><span class="j-bike">-</span>
          <span class="k j-bsub"></span></div>
        <div><span class="dot j-hdot"></span><span class="j-strap">-</span>
          <span class="k j-hsub"></span></div>
        <div><span class="k">Profil</span> <span class="v" id="rprof">-</span></div>
        <div><span class="k">Modus</span> <span class="v" id="rmode">-</span></div>
      </div>
    </div>

    <div class="card">
      <h2>Fahrt</h2>
      <div class="msg warn" id="startHint" hidden>Nach STOP: ggf. <b>Start</b> drücken und
        erneut treten — die Freigabe am Bike kann sonst fehlen.</div>
      <div class="msg warn" id="pauseHint" hidden>Auto-Pause: keine Trittfrequenz —
        Last gehalten. Weiter treten setzt die Session fort.</div>
      <div class="msg warn" id="freezeHint" hidden>Pulsverlust — Stufe eingefroren.
        Nach Timeout Rückfall auf LEVEL.</div>
      <div class="ridehero">
        <div class="heroBlock">
          <div id="pwtile"><div class="k">Leistung</div><div class="v" id="pw">-</div>
            <div class="k" id="pwsub"></div></div>
          <div id="hrtile" style="margin-top:12px"><div class="k">Puls</div><div class="v" id="hrv">-</div>
            <div class="k" id="hrvsub"></div>
            <div class="capbar" id="hrcap" hidden><i id="hrcapi"></i>
              <span class="mark" id="hrsoftm"></span></div>
            <div class="k" id="hrcapsub"></div></div>
          <div class="zonebadge" id="zbadge">
            <span class="zc" id="zcode">—</span>
            <span class="zn" id="zname">keine Zone</span>
          </div>
        </div>
        <div>
          <div><div class="k">Kadenz</div><div class="v big" id="cad">-</div>
            <div class="k" id="cadsub"></div></div>
          <div class="tile" id="ltile" style="margin-top:12px"><div class="k">Stufe</div>
            <div class="v big" id="lvl">-</div>
            <div class="segs" id="segs"></div>
            <div class="k" id="lvlsub"></div></div>
          <div class="grid" style="margin-top:14px">
            <div><div class="k">Fahrzeit</div><div class="v" id="el">-</div></div>
            <div><div class="k">Energie</div><div class="v" id="kcal">-</div></div>
            <div><div class="k">Strecke</div><div class="v" id="dst">-</div></div>
            <div><div class="k">Geschwindigkeit</div><div class="v" id="spd">-</div></div>
          </div>
        </div>
      </div>
      <div class="reg" id="regstrip">
        <div class="k">Ist / Ziel</div>
        <div class="v" id="regline">-</div>
        <canvas id="regcv" width="640" height="48" aria-label="Ist gegen Ziel"></canvas>
        <div class="bar" id="rehaprogress" hidden><i id="rehaprogi"></i></div>
        <div class="k" id="rehaprogsub"></div>
      </div>
      <div class="zrail" id="zrail">
        <div class="k">Zonen · Zeit in Zone</div>
        <div class="segs" id="zsegs">
          <i data-z="1"><b></b></i><i data-z="2"><b></b></i><i data-z="3"><b></b></i>
          <i data-z="4"><b></b></i><i data-z="5"><b></b></i><i data-z="6"><b></b></i>
          <i data-z="7"><b></b></i>
        </div>
        <div class="zlabels" id="zlabels">
          <span>Z1</span><span>Z2</span><span>Z3</span><span>Z4</span>
          <span>Z5</span><span>Z6</span><span>Z7</span>
        </div>
        <div class="k" id="zrailsub"></div>
      </div>
    </div>

    <div class="card">
      <h2>Steuerung</h2>
      <div class="k" id="rprofsub"></div>
      <div class="k" id="rmodesub"></div>
      <div class="modes">
        <button id="moff" class="ghost">OFF</button>
        <button id="mlvl" class="ghost">LEVEL</button>
        <button id="merg" class="ghost">ERG</button>
        <button id="mhr" class="ghost">HR</button>
        <button id="mreha" class="ghost">REHA</button>
        <button id="mwo" class="ghost">PHYSIO</button>
      </div>
      <div class="row flat" id="worow">
        <button id="wopause" class="ghost">Pause</button>
        <button id="woresume" class="ghost">Weiter</button>
        <button id="woskip" class="ghost">Schritt überspringen</button>
      </div>
      <details class="advbox" id="rideadv">
        <summary>Ziele · Freigabe · Reset</summary>
        <div class="row flat">
          <label class="f" style="flex:1;margin:0"><div class="k">Zielwatt (ERG)</div>
            <input type="number" id="ergw" min="20" max="300" step="5" value="80"></label>
          <button id="erggo" class="ghost">Ziel setzen</button>
        </div>
        <div class="row flat">
          <label class="f" style="flex:1;margin:0"><div class="k">Zielpuls (HR)</div>
            <input type="number" id="hrbpm" min="40" max="220" step="1" value="130"></label>
          <button id="hrgo" class="ghost">Puls setzen</button>
        </div>
        <div class="row flat">
          <label class="f" style="flex:1;margin:0"><div class="k">Reha-Watt</div>
            <input type="number" id="rehaw" min="20" max="150" step="5" value="60"></label>
          <label class="f" style="flex:1;margin:0"><div class="k">Pulsdeckel</div>
            <input type="number" id="rehahr" min="80" max="180" step="1" value="120"></label>
          <label class="f" style="flex:1;margin:0"><div class="k">Dauer (min)</div>
            <input type="number" id="rehamin" min="0" max="60" step="1" value="10"></label>
        </div>
        <div class="k" id="ergsub"></div>
        <div class="k" id="hrsub" style="color:#b45309"></div>
        <div class="k" id="rehasub" style="color:#b45309"></div>
        <div class="row flat">
          <button id="req" class="ghost">Steuerhoheit</button>
          <button id="cstart" class="ghost">Start</button>
          <button id="creset" class="ghost">Reset</button>
        </div>
      </details>
      <div class="msg" id="cmsg"></div>
      <div class="hint flat">Stufe und STOP bleiben oben kleben. Ohne Profil keine Last.
        ERG/HR/REHA brauchen Kennfläche. Nach STOP ggf. Start + Tritt.</div>
    </div>
  </section>

  <section id="t-profile">
    <div class="card">
      <h2>Wer fährt?</h2>
      <div class="hint flat">Jedes Profil hat eigene Grenzen, Zonen und HRmax.
        Ohne Auswahl startet keine Session. Wechsel nur im Modus OFF.
        Zonen brauchen FTP (Leistung) bzw. HRmax (Puls) im Profil.</div>
      <div class="pcards" id="pcards"></div>
      <div class="k" id="plnone">noch keine Profile</div>
      <div class="row tight"><button id="pclr" class="ghost sm">Auswahl aufheben</button>
        <button id="pnew" class="ghost sm">Neu</button>
        <button id="prld" class="ghost sm">Neu laden</button></div>
      <div class="msg" id="pmsg"></div>
    </div>
    <div class="card">
      <h2 id="pfh">Profil bearbeiten</h2>
      <div class="fgrid">
        <label class="f"><div class="k">ID</div>
          <input type="text" id="pf-id" maxlength="15" autocomplete="off"></label>
        <label class="f"><div class="k">Name</div>
          <input type="text" id="pf-name" maxlength="23" autocomplete="off"></label>
        <label class="f"><div class="k">Initiale</div>
          <input type="text" id="pf-initial" maxlength="2" autocomplete="off"></label>
        <label class="f"><div class="k">Farbe</div>
          <div class="colorrow" id="pf-colors"></div>
          <input type="hidden" id="pf-color" value="14881855"></label>
        <label class="f"><div class="k">Geburtsjahr</div>
          <input type="number" id="pf-birth" min="1920" max="2015" placeholder="1981"></label>
        <label class="f"><div class="k">Gewicht (kg)</div>
          <input type="number" id="pf-weight" min="0" max="200"></label>
        <label class="f"><div class="k">Ziel</div>
          <select id="pf-goal">
            <option value="none">—</option>
            <option value="fitness">Training / Fitness</option>
            <option value="fatloss">Fettabbau</option>
            <option value="performance">Leistung</option>
            <option value="reha">Reha</option>
          </select></label>
        <label class="f"><div class="k">FTP (W)</div>
          <input type="number" id="pf-ftp" min="0" max="600"></label>
        <label class="f"><div class="k">FTP-Herkunft</div>
          <select id="pf-ftporig">
            <option value="manual">manuell</option>
            <option value="estimate">Schätzung</option>
            <option value="test">Test</option>
          </select></label>
        <label class="f"><div class="k">HRmax</div>
          <input type="number" id="pf-hrmax" min="0" max="190">
          <div class="hint flat" id="pf-hrehint"></div></label>
        <label class="f"><div class="k">Ruhepuls</div>
          <input type="number" id="pf-rest" min="0" max="120"></label>
        <label class="f"><div class="k">LTHR</div>
          <input type="number" id="pf-lthr" min="0" max="190"></label>
        <label class="f"><div class="k">Zonenbasis Puls</div>
          <select id="pf-zbasis">
            <option value="hrmax">% HRmax</option>
            <option value="lthr">% LTHR</option>
          </select></label>
        <label class="f"><div class="k">max. Stufe</div>
          <input type="number" id="pf-maxlvl" min="0" max="16" step="0.1"></label>
        <label class="f"><div class="k">max. Watt</div>
          <input type="number" id="pf-maxw" min="0" max="500"></label>
        <label class="f"><div class="k">max. Puls (Hartlimit)</div>
          <input type="number" id="pf-maxhr" min="0" max="190"></label>
        <label class="f"><div class="k">Ziel-Kadenz</div>
          <input type="number" id="pf-cad" min="0" max="120"></label>
        <label class="f"><div class="k">Führende Zone</div>
          <select id="pf-lead"><option value="power">Leistung</option>
            <option value="hr">Puls</option></select></label>
        <label class="f"><div class="k">Bei Pulsverlust</div>
          <select id="pf-loss"><option value="reduce">Absenken</option>
            <option value="freeze">Einfrieren</option>
            <option value="stop">Stop</option></select></label>
      </div>
      <div class="k" id="pf-stats"></div>
      <div class="row tight">
        <button id="pfest" class="ghost">HRmax aus Alter</button>
        <button id="pfsave">Speichern</button>
        <button id="pfdel" class="danger ghost">Löschen</button>
      </div>
      <div class="msg" id="pfmsg"></div>
    </div>
  </section>

  <section id="t-dev">
    <div class="card">
      <h2>Gemerkte Geräte</h2>
      <div class="grid">
        <div><div class="k">Bike</div>
          <div class="v"><span class="dot j-bdot"></span><span class="j-bike">-</span></div>
          <div class="k j-bsub"></div>
          <div class="row tight">
            <button class="ghost sm" id="brec">Verbinden</button>
            <button class="ghost sm" id="bdis">Trennen</button>
            <button class="ghost sm" id="bfor">Vergessen</button>
          </div></div>
        <div><div class="k">Pulsgurt</div>
          <div class="v"><span class="dot j-hdot"></span><span class="j-strap">-</span></div>
          <div class="k j-hsub"></div>
          <div class="row tight">
            <button class="ghost sm" id="hrec">Verbinden</button>
            <button class="ghost sm" id="hdis">Trennen</button>
            <button class="ghost sm" id="hfor">Vergessen</button>
          </div></div>
      </div>
      <div class="msg" id="rmsg"></div>
    </div>

    <div class="card">
      <h2>Suchen</h2>
      <div class="row flat"><button id="scan" class="ghost">Umgebung suchen</button></div>
      <table id="devs"></table>
      <div class="msg" id="dmsg"></div>
    </div>
  </section>

  <section id="t-debug">
    <div class="card">
      <h2>Status</h2>
      <div class="grid">
        <div><div class="k">Name</div><div class="v" id="name">-</div></div>
        <div><div class="k">IP</div><div class="v" id="ip">-</div></div>
        <div><div class="k">MAC</div><div class="v mac" id="mac">-</div></div>
        <div><div class="k">Board</div><div class="v" id="board">-</div></div>
        <div><div class="k">WLAN</div><div class="v" id="rssi">-</div></div>
        <div><div class="k">Laufzeit</div><div class="v" id="up">-</div></div>
        <div><div class="k">Heap frei</div><div class="v" id="heap">-</div></div>
        <div><div class="k">Hub</div>
          <div class="v addr"><span class="dot" id="hubdot"></span><span id="hub">-</span></div></div>
        <div><div class="k">Codec-Selbsttest</div>
          <div class="v"><span class="dot" id="cdot"></span><span id="codec">-</span></div></div>
      </div>
    </div>

    <div class="card">
      <h2>Gerätekennungen</h2>
      <div class="grid">
        <div><div class="k">0x2ACC Feature</div><div class="v mac" id="dfeat">-</div></div>
        <div><div class="k">0x2AD6 Stellweg</div><div class="v mac" id="dres">-</div></div>
        <div><div class="k">0x2AD8 Wattbereich</div><div class="v mac" id="dpow">-</div></div>
        <div><div class="k">Strategie</div><div class="v" id="strat">-</div></div>
        <div><div class="k">Wattziel vertrauenswürdig</div><div class="v" id="dtrust">-</div></div>
        <div><div class="k">Control Point</div><div class="v" id="dcp">-</div></div>
      </div>
      <div class="hint flat">Der Varon meldet keinen Wattbereich und quittiert
        <code>0x05</code> trotzdem mit Success. Deshalb wird einem Wattziel nur
        geglaubt, wenn 0x2AD8 vorhanden ist.</div>
    </div>

    <div class="card">
      <h2>Datenstrom und Limiter</h2>
      <div class="grid">
        <div><div class="k">0x2AD2 Notifies</div><div class="v" id="dnot">-</div></div>
        <div><div class="k">0x2AD9 Antworten</div><div class="v" id="dresp">-</div></div>
        <div><div class="k">Letzte Antwort</div><div class="v" id="dlast">-</div></div>
        <div><div class="k">Limiter</div><div class="v" id="limit">-</div></div>
        <div><div class="k">Letzte Ablehnung</div><div class="v" id="ddeny">-</div></div>
        <div><div class="k">Datenstrom</div><div class="v" id="dstale">-</div></div>
      </div>
    </div>

    <div class="card">
      <h2>Steuer-Journal</h2>
      <div class="grid">
        <div><div class="k">Beurteilt</div><div class="v" id="jrn">-</div></div>
        <div><div class="k">Wirkt</div><div class="v ok" id="jrw">-</div></div>
        <div><div class="k">Ohne Wirkung</div><div class="v" id="jr0">-</div></div>
        <div><div class="k">Kein Urteil</div><div class="v" id="jru">-</div></div>
      </div>
      <div class="warn" id="jrwarn" hidden></div>
      <table id="jrt"></table>
      <div class="hint flat" id="jrnone">Noch kein Schreibvorgang beurteilt.</div>
      <div class="hint flat">Beurteilt wird <b>Watt pro Kadenz</b> und nicht Watt:
        schneller treten erhöht die Leistung, nicht die Stufe. Bei zu niedriger
        oder zwischen den Fenstern weggelaufener Kadenz fällt bewusst kein
        Urteil — <span class="q">weiß nicht</span> ist ein Ergebnis.</div>
    </div>

    <div class="card">
      <h2>Mitschnitt</h2>
      <div class="row flat">
        <button id="rgon" class="ghost">Mitschnitt starten</button>
        <button id="rgdl" class="ghost">Als JSONL laden</button>
        <button id="rgx" class="ghost">Leeren</button>
      </div>
      <div class="grid">
        <div><div class="k">Zustand</div>
          <div class="v"><span class="dot" id="rgdot"></span><span id="rgst">aus</span></div></div>
        <div><div class="k">Datensätze</div><div class="v" id="rgn">-</div></div>
        <div><div class="k">Angeboten</div><div class="v" id="rgs">-</div></div>
        <div><div class="k">Ausgedünnt</div><div class="v" id="rgt">-</div></div>
      </div>
      <div class="bar"><i id="rgbar"></i></div>
      <div class="msg" id="rgmsg"></div>
      <div class="hint flat">Rohbytes im JSONL-Format der Sonde. Der Export lässt
        sich unverändert von <code>tools/make-fixtures.py</code> lesen — Fixtures
        aus einer echten Fahrt statt aus einem Laborlauf. Ausgedünnt wird nur der
        Messstrom; jedes neue Flagwort und der gesamte Steuerverkehr bleiben
        vollständig. Der Mitschnitt startet nicht von allein und überlebt keinen
        Neustart.</div>
    </div>
  </section>

  <section id="t-cfg">
    <div class="card">
      <h2>Einstellungen</h2>
      <div class="fgrid">
        <label class="f"><div class="k">Gerätename</div>
          <input type="text" id="c-deviceName"></label>
        <label class="f"><div class="k">Hub-Adresse</div>
          <input type="text" id="c-hubHost"></label>
        <label class="f"><div class="k">Hub-Port</div>
          <input type="number" id="c-hubPort" min="1" max="65535"></label>
        <label class="f"><div class="k">Heartbeat (s)</div>
          <input type="number" id="c-heartbeatIntervalS" min="5" max="600"></label>
        <label class="f"><div class="k">Hub-Watchdog (s, 0 = aus)</div>
          <input type="number" id="c-watchdogS" min="0" max="65535"></label>
        <label class="f"><div class="k">NTP-Server</div>
          <input type="text" id="c-ntpServer"></label>
        <label class="f wide"><div class="k">Zeitzone</div>
          <input type="text" id="c-tz"></label>
      </div>
      <label class="chk"><input type="checkbox" id="c-enableHub"> Hub-Heartbeat senden</label>
      <label class="chk"><input type="checkbox" id="c-enableMdns"> mDNS bekanntmachen</label>
      <label class="chk"><input type="checkbox" id="c-enableNtp"> Zeit über NTP holen</label>
      <label class="chk"><input type="checkbox" id="c-autoConnect"> Bike nach dem Booten selbst verbinden</label>
      <div class="hint">Selbst verbinden ist bewusst aus: ein Ergometer, das sich nach
        einem Stromausfall unaufgefordert ankoppelt, während niemand daneben steht,
        ist kein Komfortgewinn.</div>
      <div class="row flat"><button id="save">Speichern</button></div>
      <div class="msg" id="gmsg"></div>
    </div>
  </section>

  <section id="t-ota">
    <div class="card">
      <h2>Firmware aktualisieren</h2>
      <form id="otaf" method="post" action="/ota-upload" enctype="multipart/form-data">
        <label class="drop" for="fw" id="drop">
          <b>Firmware-Datei wählen</b>
          ergo.&lt;version&gt;.esp32s3.bin
        </label>
        <input type="file" id="fw" name="firmware" accept=".bin">
        <button id="go" type="submit">Hochladen und neu starten</button>
      </form>
      <div class="msg" id="msg"></div>
    </div>

    <div class="card">
      <h2>Neustart</h2>
      <div class="row flat"><button id="reboot" class="ghost">Neu starten</button></div>
      <div class="hint flat">Hängt ein Bike, sendet das Gerät vorher Stop — sonst
        bliebe das Ergometer gebremst stehen.</div>
      <div class="msg" id="smsg"></div>
    </div>
  </section>

  <section id="t-calib">
    <div class="card">
      <h2>Stufen-Sweep</h2>
      <div class="grid">
        <div><div class="k">Zustand</div>
          <div class="v big" id="swst">-</div>
          <div class="k" id="swsub"></div></div>
        <div><div class="k">Stufe</div>
          <div class="v big" id="swlvl">-</div>
          <div class="k" id="swprog"></div></div>
        <div><div class="k">Kadenz halten</div>
          <div class="v big" id="swcad">-</div>
          <div class="k" id="swcadsub"></div></div>
      </div>
      <div class="bar"><i id="swbar"></i></div>
      <div class="row">
        <button id="sw1">Test 1 &middot; 60 rpm</button>
        <button id="sw2" class="ghost">Test 2 &middot; 80 rpm</button>
        <button id="sw2l" class="ghost">Test 2 leicht &middot; Stufe 4+8</button>
        <button id="swx" class="danger">Abbrechen</button>
      </div>
      <div class="hint flat">Test 1: neun Stufen, knapp neun Minuten. Test 2: vier Stufen
        bei 80 rpm (~4 min) — deutlich härter. <b>Test 2 leicht</b>: nur Stufe 4 und 8
        bei 80 rpm (~2 min), reicht für die Kadenzfrage. Je Stufe 20 s einschwingen,
        40 s messen. Wer aufhört zu treten, bricht den Sweep ab.</div>
      <div class="msg" id="swmsg"></div>
    </div>

    <div class="card">
      <h2>Punkte des letzten Sweeps</h2>
      <table id="swpts"></table>
      <div class="k" id="swnone">noch kein Sweep gefahren</div>
    </div>

    <div class="card">
      <h2>Kennfläche Stufe &times; Kadenz &rarr; Watt</h2>
      <div class="grid">
        <div><div class="k">Stützstellen</div><div class="v" id="mpn">-</div>
          <div class="k" id="mpnsub"></div></div>
        <div><div class="k">Stufen belegt</div><div class="v" id="mpl">-</div></div>
        <div><div class="k">Kadenzbänder</div><div class="v" id="mpb">-</div></div>
      </div>
      <div class="hm" id="hm"></div>
      <div class="hint flat">Zeilen sind Stufen, Spalten die untere Grenze des
        Kadenzbandes. Helle Zellen bedeuten mehr Leistung, ein heller Rand markiert eine
        Stützstelle aus einem geführten Sweep — alles andere ist beim Fahren gelernt.
        Leere Zellen werden aus den Nachbarn interpoliert, nicht erfunden.</div>
      <div class="row tight"><button id="mpr" class="ghost sm">Neu laden</button>
        <button id="mpc" class="danger sm">Fläche verwerfen</button></div>
      <div class="msg" id="mpmsg"></div>
    </div>
  </section>

  <section id="t-workouts">
    <div class="card">
      <h2>Programme</h2>
      <div class="hint flat">Eingebaute Programme und Dateien auf LittleFS.
        Vorschau, Editor und Machbarkeit gegen das aktive Profil.</div>
      <div class="pcards" id="wocards"></div>
      <table id="wolist" hidden></table>
      <div class="k" id="wonone">lade…</div>
      <div class="row flat">
        <label class="f" style="flex:1;margin:0"><div class="k">Zeitfaktor</div>
          <input type="number" id="woscale" min="0.05" max="2" step="0.05" value="1"></label>
        <button id="woreload" class="ghost">Neu laden</button>
      </div>
      <div class="msg" id="womsg"></div>
    </div>
    <div class="card">
      <h2>Vorschau</h2>
      <div class="k" id="woprevmeta">Programm wählen oder JSON prüfen</div>
      <canvas id="woprev" width="640" height="96" aria-label="Workout-Vorschau"
        style="display:block;width:100%;height:96px;margin:10px 0;background:#0E1116;
        border-radius:8px;border:1px solid var(--edge)"></canvas>
      <table id="wosteps"></table>
      <div class="msg" id="wowarn"></div>
      <div class="row flat">
        <button id="wostartsel" class="ghost" disabled>Auswahl starten</button>
      </div>
    </div>
    <div class="card">
      <h2>Schritt-Editor</h2>
      <div class="hint flat">Nur Steady-Schritte (max. 8). Intervalblöcke/Rampen später.
        Änderungen schreiben sofort das JSON und prüfen Machbarkeit.</div>
      <div class="row flat">
        <label class="f" style="flex:1;margin:0"><div class="k">Name</div>
          <input type="text" id="woedname" maxlength="39" autocomplete="off"></label>
        <label class="f" style="flex:1;margin:0"><div class="k">ID</div>
          <input type="text" id="woedid" maxlength="23" autocomplete="off"></label>
      </div>
      <div class="row flat">
        <button id="woedunit" class="ghost">Ziel: Watt</button>
        <button id="woedadd" class="ghost">+ Schritt</button>
        <button id="woedblank" class="ghost">Leer</button>
      </div>
      <div id="woedsteps"></div>
      <div class="row flat">
        <button id="woedsync" class="ghost">Aus JSON lesen</button>
        <button id="woeddl" class="ghost">JSON-Datei</button>
        <button id="woval" class="ghost">Prüfen</button>
        <button id="woput" class="ghost">Auf Gerät speichern</button>
      </div>
      <div class="msg" id="wojmsg"></div>
      <details class="advbox" id="wojsonbox">
        <summary>JSON (Datei / Drop)</summary>
        <label class="drop" id="wodrop"><b>JSON-Datei ablegen oder tippen</b>
          <span>oder unten einfügen</span>
          <input type="file" id="wofile" accept="application/json,.json"></label>
        <textarea id="wojson" rows="6" style="width:100%;font:inherit;background:#0E1116;color:var(--fg);
          border:1px solid var(--edge);border-radius:9px;padding:10px"></textarea>
      </details>
    </div>
    <div class="card">
      <h2>Letzte Session</h2>
      <div class="v" id="wosess">-</div>
    </div>
  </section>

  <section id="t-tests">
    <div class="card">
      <h2>Geführte Tests</h2>
      <div class="hint flat">Rampe, 20 Minuten und Recovery laufen als Programme.
        Ergebnis wird vorgeschlagen, nie automatisch übernommen. Bei Reha-Profil ausgeblendet.</div>
      <div class="pcards" id="tstcards"></div>
      <div class="k" id="tstnone">lade…</div>
      <div class="row flat">
        <label class="f" style="flex:1;margin:0"><div class="k">Zeitfaktor</div>
          <input type="number" id="tstscale" min="0.05" max="2" step="0.05" value="1"></label>
        <button id="tstreload" class="ghost">Neu laden</button>
      </div>
      <div class="msg" id="tstmsg"></div>
    </div>
    <div class="card">
      <h2>Vorschau / Machbarkeit</h2>
      <div class="k" id="tstprevmeta">Test wählen</div>
      <canvas id="tstprev" width="640" height="72" aria-label="Test-Vorschau"
        style="display:block;width:100%;height:72px;margin:10px 0;background:#0E1116;
        border-radius:8px;border:1px solid var(--edge)"></canvas>
      <div class="msg" id="tstwarn"></div>
      <div class="row flat">
        <button id="tststart" class="ghost" disabled>Test starten</button>
      </div>
    </div>
    <div class="card">
      <h2>Ergebnis</h2>
      <div class="k" id="tstresmeta">nach Stop oder Abschluss</div>
      <div class="v" id="tstres">-</div>
      <div class="row flat">
        <button id="tstaccept" class="ghost" disabled>FTP übernehmen</button>
        <button id="tstresload" class="ghost">Letzte Session</button>
      </div>
      <div class="msg" id="tstresmsg"></div>
    </div>
    <div class="card">
      <h2>Testhistorie</h2>
      <table id="tsthist"></table>
      <div class="k" id="tsthistnone" hidden>noch keine Test-Sessions</div>
    </div>
  </section>

  <section id="t-verlauf">
    <div class="card" id="progcard">
      <h2>Physio-Progression</h2>
      <div class="hint flat">Nach einer sauberen Physio-Einheit kann der Hauptteil
        um eine Minute wachsen — nie automatisch.</div>
      <div class="k" id="progmeta">lade…</div>
      <canvas id="progcv" width="640" height="72" aria-label="Progression"
        style="display:block;width:100%;height:72px;margin:10px 0;background:#0E1116;
        border-radius:8px;border:1px solid var(--edge)"></canvas>
      <div class="v" id="progoffer">-</div>
      <div class="row flat">
        <button id="progyes" class="ghost" disabled>Hauptteil +1 min</button>
        <button id="progno" class="ghost" disabled>Nicht steigern</button>
        <button id="progreload" class="ghost">Aktualisieren</button>
      </div>
      <div class="msg" id="progmsg"></div>
      <table id="proghist"></table>
    </div>
    <div class="card">
      <h2>Session-Verlauf</h2>
      <p class="k">Die letzten Fahrten auf dem Gerät (LittleFS). Neueste zuerst.</p>
      <div class="row flat">
        <button id="sessreload" class="ghost">Aktualisieren</button>
      </div>
      <div class="msg" id="sessmsg"></div>
      <table id="sesslist"></table>
      <div class="k" id="sessnone" hidden>noch keine Sessions</div>
    </div>
    <div class="card">
      <h2>Letzte Session</h2>
      <div class="v" id="sesslast">-</div>
    </div>
  </section>

  <section id="t-soon">
    <div class="card"><div class="soonbox">
      <b id="soont">-</b>
      <span class="d" id="soond"></span>
      <span class="ver" id="soonv"></span>
    </div></div>
  </section>

  <footer>esp32.ergo &middot; Trainingsrechner Hammer Varon XTR II</footer>
</div>
<script>
const $=i=>document.getElementById(i);
// Reiterfolge und -inhalt laut WEBINTERFACE.md §7. Leere Version = fertig.
const NAV=[
 ['ride','Ride','',''],
 ['workouts','Workouts','',''],
 ['tests','Tests','',''],
 ['verlauf','Verlauf','',''],
 ['profile','Profile','',''],
 ['dev','Geräte','',''],
 ['calib','Kalibrierung','',''],
 ['debug','Debug','',''],
 ['cfg','Einstellungen','',''],
 ['ota','OTA','','']];
function setAll(sel,txt){document.querySelectorAll(sel).forEach(e=>e.textContent=txt)}
function dotAll(sel,ok){document.querySelectorAll(sel).forEach(e=>e.classList.toggle('on',!!ok))}
function dot(el,ok){el.classList.toggle('on',!!ok)}
function yn(v){return v?'ja':'nein'}

document.body.className='js';
$('nav').innerHTML=NAV.map(x=>
  '<button class="tab'+(x[2]?' soon':'')+'" data-t="'+x[0]+'">'+x[1]+'</button>').join('');
document.querySelectorAll('.tab').forEach(b=>{b.onclick=()=>tab(b.dataset.t)});

function zoneMiniHtml(times, count){
  const n=count||7;
  const arr=times||[];
  let sum=0; for(let i=0;i<n;i++) sum+=(arr[i]|0);
  let h='';
  for(let i=0;i<n;i++){
    const t=arr[i]|0;
    const pct=sum?Math.min(100,100*t/sum):0;
    h+='<i><b style="width:'+pct+'%"></b></i>';
  }
  return '<div class="zmini">'+h+'</div>';
}

function tab(n){
  const e=NAV.find(x=>x[0]===n)||NAV[0];
  n=e[0];
  document.querySelectorAll('section').forEach(s=>s.classList.remove('on'));
  if(e[2]){
    $('soont').textContent=e[1];
    $('soond').textContent=e[3];
    $('soonv').textContent='geplant für '+e[2];
    $('t-soon').classList.add('on');
  }else{
    $('t-'+n).classList.add('on');
  }
  document.querySelectorAll('.tab').forEach(b=>b.classList.toggle('on',b.dataset.t===n));
  if(location.hash!=='#'+n) history.replaceState(null,'','#'+n);
  if(n==='dev') devs();
  if(n==='cfg') loadCfg();
  if(n==='calib') loadMap();
  if(n==='profile') loadProfiles();
  if(n==='workouts') loadWorkouts();
  if(n==='tests') loadTests();
  if(n==='verlauf'){ loadSessions(); loadProgression(); }
}

function num(v,d,u){return (v==null)?'-':(d?v.toFixed(d):Math.round(v))+(u||'');}
function hms(s){if(s==null)return '-';const p=n=>(n<10?'0':'')+n;
  return p(Math.floor(s/3600))+':'+p(Math.floor(s/60)%60)+':'+p(s%60);}

function render(s){
  $('ver').textContent=s.version||'-';
  $('name').textContent=s.name||'-';
  $('ip').textContent=s.ip||'-';
  $('mac').textContent=s.mac||'-';
  $('board').textContent=s.boardLabel||s.board||'-';
  $('rssi').textContent=(s.rssi!=null?s.rssi+' dBm':'-');
  $('up').textContent=s.uptime||'-';
  $('heap').textContent=(s.heap!=null?(s.heap/1024).toFixed(0)+' kB':'-');
  $('hub').textContent=s.hub||'-';
  dot($('hubdot'),s.hubOk);
  $('codec').textContent=s.codecSelfTest||'-';
  dot($('cdot'),s.codecSelfTest==='ok');
  $('sub').textContent=[s.fwType||'ergo',s.chip||'',s.time||'ohne Zeit'].join(' / ');
  const chip=$('pchip'), cav=$('pchipav'), cn=$('pchipname');
  if(chip){
    const pi=s.profileInfo;
    if(pi&&pi.name){
      chip.hidden=false;
      const col='#'+(('000000'+((pi.color||0xe2802f)>>>0).toString(16)).slice(-6));
      cav.style.background=col;
      cav.textContent=(pi.initial||pi.name.charAt(0)||'?').toUpperCase();
      cn.textContent=pi.name+(pi.goal&&pi.goal!=='none'?(' · '+({fatloss:'Fettabbau',fitness:'Training',reha:'Reha',performance:'Leistung'}[pi.goal]||'')):'');
    } else {
      chip.hidden=true;
    }
  }
  renderBle(s);
  renderCalib(s);
  renderDebug(s);
}

const VD={WORKS:['wirkt','w'],NO_EFFECT:['keine Wirkung','n'],
          UNJUDGED:['kein Urteil',''],PENDING:['läuft','']};

/**
 * Das Steuer-Journal und der Mitschnitt.
 *
 * Die auffaelligste Zeile ist bewusst der Widerspruch: Geraet meldet Erfolg,
 * Messung sieht nichts. Genau diesen Zustand hat die erste Hardware-Session
 * gehabt, ohne ihn benennen zu koennen.
 */
function renderDebug(s){
  const d=s.debug||{}, r=d.ring||{}, j=d.journal||{};

  $('jrn').textContent=(j.judged!=null?j.judged:'-')+(j.pending?' (+1 läuft)':'');
  $('jrw').textContent=j.worked!=null?j.worked:'-';
  $('jr0').textContent=j.noEffect!=null?j.noEffect:'-';
  $('jru').textContent=j.unjudged!=null?j.unjudged:'-';

  const w=$('jrwarn');
  if(j.contradictions>0){
    w.hidden=false;
    w.innerHTML='<b>'+j.contradictions+'&times; quittiert, aber wirkungslos.</b> '+
      'Das Gerät hat den Schreibvorgang mit Success beantwortet, die Messung '+
      'sieht bei gehaltener Kadenz keine Änderung. Eine Erfolgsquittung beweist '+
      'nichts — das Kommando kommt an und tut trotzdem nichts.';
  } else { w.hidden=true; }

  const E=j.entries||[];
  $('jrnone').hidden=E.length>0;
  $('jrt').innerHTML=E.length?
    '<tr><th>Kommando</th><th>Stufe</th><th>W/rpm</th><th>Δ</th><th>Kadenz</th><th>Urteil</th></tr>'+
    E.map(e=>{
      const v=VD[e.effect]||[e.effect,''];
      const lvl=(e.from>=0&&e.to>=0)?(e.from/10).toFixed(0)+' → '+(e.to/10).toFixed(0):'–';
      const note=e.contradictory?' <span class="vd n">Success</span>':'';
      return '<tr><td class="mac">'+(e.cmd||'')+'</td><td>'+lvl+'</td>'+
        '<td>'+e.prePerRpm.toFixed(2)+' → '+e.postPerRpm.toFixed(2)+'</td>'+
        '<td>'+(e.effect==='UNJUDGED'?'–':(e.changePct>0?'+':'')+e.changePct+' %')+'</td>'+
        '<td>'+Math.round(e.preRpm)+' → '+Math.round(e.postRpm)+'</td>'+
        '<td><span class="vd '+v[1]+'">'+v[0]+'</span>'+note+
        (e.reason?'<div class="k">'+e.reason+'</div>':'')+'</td></tr>';
    }).join(''):'';

  const on=!!r.on;
  dot($('rgdot'),on);
  $('rgst').textContent=on?'schreibt mit':'aus';
  $('rgon').textContent=on?'Mitschnitt anhalten':'Mitschnitt starten';
  $('rgn').textContent=(r.count!=null?r.count:'-')+(r.slots?' / '+r.slots:'');
  $('rgs').textContent=r.seen!=null?r.seen:'-';
  $('rgt').textContent=(r.thinned!=null?r.thinned:'-')+
    (r.overwritten>0?' · '+r.overwritten+' überschrieben':'');
  $('rgbar').style.width=(r.slots?Math.min(100,100*(r.count||0)/r.slots):0)+'%';
  $('rgdl').disabled=!(r.count>0);
}

let scanPrev=false;
let prevMode='OFF';
let afterStop=false;
const istHist=[], zielHist=[];
const HIST_N=48;

function pushHist(ist,ziel){
  istHist.push(ist); zielHist.push(ziel);
  while(istHist.length>HIST_N){istHist.shift();zielHist.shift();}
}
function drawRegChart(istColor){
  const cv=$('regcv'); if(!cv) return;
  const ctx=cv.getContext('2d');
  const W=cv.width, H=cv.height;
  ctx.clearRect(0,0,W,H);
  if(istHist.length<2) return;
  let mx=1;
  for(let i=0;i<istHist.length;i++){
    mx=Math.max(mx, istHist[i]||0, zielHist[i]||0);
  }
  mx*=1.15;
  const step=W/Math.max(1,HIST_N-1);
  function stroke(arr,color,dash){
    ctx.beginPath();
    ctx.strokeStyle=color; ctx.lineWidth=2;
    ctx.setLineDash(dash||[]);
    for(let i=0;i<arr.length;i++){
      const x=i*step, y=H-4-((arr[i]||0)/mx)*(H-8);
      if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
    }
    ctx.stroke();
  }
  stroke(zielHist,'#8A94A6',[4,4]);
  stroke(istHist,istColor||'#E2802F',[]);
  ctx.setLineDash([]);
}

function renderBle(s){
  const L=(s.ble&&s.ble.links)||{}, b=L.bike||{}, h=L.hr||{};
  const f=s.ftms||{}, c=f.caps||{}, d=f.data||{}, li=s.limiter||{};
  setAll('.j-bike',b.state||'-');
  dotAll('.j-bdot', b.state==='READY');
  setAll('.j-bsub',[b.name||b.rememberedName||b.mac||'nichts gemerkt',
    b.rssi?b.rssi+' dBm':'', f.controlGranted?'Steuerhoheit':'',
    b.losses?('Verluste '+b.losses):''].filter(Boolean).join(' / '));
  setAll('.j-strap',h.state||'-');
  dotAll('.j-hdot', h.state==='READY');
  setAll('.j-hsub',[h.name||h.rememberedName||h.mac||'nichts gemerkt',
    (s.hr&&s.hr.battery)?s.hr.battery+'%':''].filter(Boolean).join(' / '));

  const live=f.attached&&!f.stale;
  const pi=s.profileInfo||null;
  const leadHr=pi&&pi.leadingZone==='hr';
  const mode=(s.mode||'OFF');
  const erg=s.erg||{};
  const hh=s.hrHold||{};
  const rh=s.reha||{};
  const wo=s.workout||{};
  const loadModes={MANUAL_LEVEL:1,MANUAL_ERG:1,HR_HOLD:1,REHA:1,WORKOUT:1};
  if(loadModes[prevMode] && mode==='OFF') afterStop=true;
  if(mode!=='OFF') afterStop=false;
  prevMode=mode;
  const sh=$('startHint');
  if(sh){
    const show=afterStop && !!s.bikeLink && mode==='OFF';
    sh.hidden=!show;
  }
  const sess=s.session||{};
  const ph=$('pauseHint');
  if(ph) ph.hidden=!(sess.active && sess.paused && mode!=='OFF');
  const fh=$('freezeHint');
  if(fh){
    const freeze=(mode==='HR_HOLD'&&hh.lost)||((mode==='REHA'||mode==='WORKOUT')&&rh.lost);
    fh.hidden=!freeze;
  }

  // Zone-Optik (führende Zone aus Profil)
  const zi=s.zone||{};
  const zIdx=zi.index|0;
  const zClasses=['js'];
  if(zIdx>=1&&zIdx<=7) zClasses.push('z'+zIdx);
  document.body.className=zClasses.join(' ');
  const zc=$('zcode'), zn=$('zname'), zsub=$('zrailsub');
  if(zc) zc.textContent=zi.code||'—';
  if(zn) zn.textContent=zi.name||(zi.lead==='hr'?'Pulsbasis':'Leistungsbasis');
  const times=(sess.zoneTimeS)||[];
  const zCount=(sess.zoneCount||(zi.lead==='hr'?5:7))|0;
  let sumT=0;
  for(let i=0;i<times.length;i++) sumT+=(times[i]|0);
  const segs=$('zsegs');
  if(segs){
    const kids=segs.children;
    for(let i=0;i<kids.length;i++){
      const on=i<zCount;
      kids[i].style.display=on?'':'none';
      kids[i].classList.toggle('cur', on && (i+1)===zIdx);
      const fill=kids[i].querySelector('b');
      if(fill){
        const t=times[i]|0;
        fill.style.width=sumT?Math.max(0,Math.min(100,100*t/sumT))+'%':'0%';
      }
    }
  }
  const zl=$('zlabels');
  if(zl){
    for(let i=0;i<zl.children.length;i++){
      zl.children[i].style.display=i<zCount?'':'none';
      const t=times[i]|0;
      zl.children[i].textContent=t?('Z'+(i+1)+' '+hms(t)):('Z'+(i+1));
    }
  }
  if(zsub){
    zsub.textContent=(!zi.ftpW&&zi.lead!=='hr')
      ?'FTP im Profil setzen für Leistungszonen'
      :((zi.lead==='hr'&&!zi.hrMax)?'HRmax im Profil setzen für Pulszonen'
        :(sumT?('Summe '+hms(sumT)+(zIdx?(' · aktuell Z'+zIdx):'')):'Zeit sammelt sich in der Session'));
  }

  const ceil=!!(erg.ceiling && (mode==='MANUAL_ERG'||mode==='REHA'||mode==='HR_HOLD'||mode==='WORKOUT'));
  $('pw').className='v'+(leadHr?'':(' hero'+(ceil?' ceil':'')));
  $('hrv').className='v'+(leadHr?' hero':' big');
  $('pw').textContent=live?num(d.powerW,0,' W'):'-';
  let pwsub='';
  if(live){
    if(mode==='MANUAL_ERG' && erg.targetW){
      pwsub='Ist · Ziel '+Math.round(erg.targetW)+' W'+(ceil?' · unerreichbar':'');
    } else if((mode==='REHA'||mode==='WORKOUT') && (rh.desiredW!=null||wo.desiredW!=null)){
      const des=mode==='WORKOUT'?(wo.desiredW!=null?wo.desiredW:rh.desiredW):rh.desiredW;
      pwsub='Soll '+Math.round(des)+' W'
        +(rh.capActive?(' · wirkt '+Math.round(rh.effectiveW)+' W'):'')
        +(ceil?' · Decke':'');
    } else if(mode==='HR_HOLD' && hh.powerTargetW!=null){
      pwsub='aus Puls · ~'+Math.round(hh.powerTargetW)+' W';
    } else if(!leadHr){
      pwsub='führend';
    }
  } else pwsub='keine Daten';
  $('pwsub').textContent=pwsub;

  const rpm=live?(d.cadenceRpm||0):0;
  $('cad').textContent=live?num(rpm,0,' rpm'):'-';
  const tc=pi&&pi.targetCadenceRpm;
  let cadsub='';
  if(live && tc){
    if(rpm<tc-5) cadsub='schneller · Ziel '+tc+' rpm';
    else if(rpm>tc+5) cadsub='langsamer · Ziel '+tc+' rpm';
    else cadsub='halten · '+tc+' rpm';
  }
  $('cadsub').textContent=cadsub;

  $('hrv').textContent=s.heartRate?(s.heartRate+' bpm'):'-';
  $('hrvsub').textContent=({strap:'Gurt',machine:'über das Bike',relay:'Relay'}[s.hrSource]||'')
    +(leadHr?(s.hrSource?' · ':'')+'führend':'');

  // Deckel-Näherung (REHA oder Profil maxHr bei leadHr)
  const hrCapEl=$('hrcap'), hrCapI=$('hrcapi'), hrSoftM=$('hrsoftm');
  const showCap=mode==='REHA'||mode==='WORKOUT' || (leadHr && pi && pi.maxHr);
  if(hrCapEl){
    hrCapEl.hidden=!showCap;
    if(showCap){
      const hard=mode==='WORKOUT'?(wo.hrMax||rh.hrMax||120)
        :(mode==='REHA'?(rh.hrMax||120):(pi.maxHr||120));
      const soft=mode==='WORKOUT'?(wo.hrSoft||rh.hrSoft||(hard-5))
        :(mode==='REHA'?(rh.hrSoft||(hard-5)):Math.max(40,hard-5));
      const hr=s.heartRate||0;
      const pct=hard?Math.min(100,Math.max(0,100*hr/hard)):0;
      hrCapI.style.width=pct+'%';
      hrCapEl.className='capbar'+(hr>=hard?' hard':(hr>=soft?' soft':''));
      if(hrSoftM) hrSoftM.style.left=(hard?Math.min(100,100*soft/hard):0)+'%';
      $('hrcapsub').textContent=hr
        ?(hr>=hard?('über Deckel '+hard)
          :(hr>=soft?('Anfahrband · Soft '+soft+' / Hard '+hard)
            :('Deckel '+hard+(soft?' · Soft '+soft:''))))
        :('Deckel '+hard);
    } else {
      $('hrcapsub').textContent='';
    }
  }

  $('spd').textContent=live?num(d.speedKmh,1,' km/h'):'-';
  $('dst').textContent=live?num(d.distanceM,0,' m'):'-';
  $('kcal').textContent=live?num(d.energyKcal,0,' kcal'):'-';
  $('el').textContent=live?hms(d.elapsedS):'-';
  levelTile(li,c);

  // Ist/Ziel-Kurve — Farbe folgt Zone
  let ziel=0, ist=live?(d.powerW||0):0;
  if(mode==='MANUAL_ERG') ziel=erg.targetW||0;
  else if(mode==='REHA') ziel=rh.capActive?(rh.effectiveW||0):(rh.desiredW||0);
  else if(mode==='WORKOUT') ziel=rh.capActive?(rh.effectiveW||0):(wo.desiredW||rh.desiredW||0);
  else if(mode==='HR_HOLD') ziel=hh.powerTargetW||0;
  const zColor=zi.color?('#'+zi.color):'#E2802F';
  if(mode==='MANUAL_ERG'||mode==='REHA'||mode==='HR_HOLD'||mode==='WORKOUT'){
    if(live) pushHist(ist, ziel);
    let line='';
    if(ziel>0){
      const delta=ist-ziel;
      line='Ist '+Math.round(ist)+' W · Ziel '+Math.round(ziel)+' W · Δ '
        +(delta>=0?'+':'')+Math.round(delta)+' W';
      if(ceil) line+=' · Ziel über Kennfläche';
    } else line='kein Ziel';
    $('regline').textContent=line;
  } else {
    $('regline').textContent=mode==='OFF'?'keine Regelung':'Handstufe';
  }
  drawRegChart(zColor);

  const rp=$('rehaprogress'), rpi=$('rehaprogi'), rps=$('rehaprogsub');
  if(rp){
    const timedReha=mode==='REHA' && rh.durationS>0;
    const timedWo=mode==='WORKOUT' && wo.state && wo.state!=='IDLE';
    rp.hidden=!(timedReha||timedWo);
    if(timedWo){
      const tot=wo.totalRemainingS||0, el=wo.elapsedS||0;
      const span=el+tot;
      rpi.style.width=span?Math.min(100,100*el/span)+'%':'0%';
      rps.textContent=(wo.name||'Workout')+' · Schritt '+(1+(wo.stepIndex||0))+'/'+(wo.stepCount||'?')
        +' · '+(wo.label||'')
        +' · noch '+(wo.stepRemainingS!=null?wo.stepRemainingS:'-')+' s'
        +(rh.interventions?(' · Deckel '+rh.interventions+'×'):'');
    } else if(timedReha){
      const el=rh.elapsedS||0, dur=rh.durationS||1;
      rpi.style.width=Math.min(100,100*el/dur)+'%';
      rps.textContent='Physio '+Math.round(dur/60)+' min · '
        +hms(el)+' von '+hms(dur)
        +(rh.interventions!=null?(' · Deckel griff '+rh.interventions+'×'):'');
    } else { rps.textContent=''; }
  }

  const hasP=!!s.profile;
  $('rprof').textContent=pi?(pi.name||pi.id):(s.profile||'(keins)');
  $('rprofsub').textContent=hasP
    ?((pi.goal&&pi.goal!=='none'?(({fatloss:'Fettabbau',fitness:'Training',reha:'Reha',performance:'Leistung'}[pi.goal]||'')+' · '):'')
      +'max Stufe '+lvDisp(pi&&pi.maxLevelTenths)+' · max '+(pi&&pi.maxPowerW||'-')+' W'
      +(pi.weightKg?(' · '+pi.weightKg+' kg'):'')
      +(pi.hrMax?(' · HRmax '+pi.hrMax):''))
    :'vor LEVEL Profil wählen';
  $('rmode').textContent=mode;
  let msub=mode==='MANUAL_LEVEL'?'Handstufe':(mode==='OFF'?'keine Last':'');
  if(mode==='MANUAL_ERG'){
    msub=(erg.targetW?('Ziel '+Math.round(erg.targetW)+' W'):'kein Ziel')
      +(erg.ceiling?' · Decke':'')
      +(erg.smoothedW!=null?(' · Ist~'+Math.round(erg.smoothedW)+' W'):'');
  }
  if(mode==='HR_HOLD'){
    msub='Ziel '+(hh.targetBpm||s.hrTargetBpm||'-')+' bpm'
      +(hh.powerTargetW!=null?(' · ~'+Math.round(hh.powerTargetW)+' W'):'')
      +(hh.smoothedHr?(' · Ist '+hh.smoothedHr):'')
      +(hh.lost?' · PULSVERLUST':'');
  }
  if(mode==='REHA'){
    msub='PHYSIO '+(rh.desiredW!=null?Math.round(rh.desiredW):'-')+' W · Puls ≤ '+(rh.hrMax||'-')
      +(rh.effectiveW!=null&&rh.capActive?(' · wirkt '+Math.round(rh.effectiveW)+' W'):'')
      +(rh.durationS?(' · noch '+(rh.remainingS!=null?rh.remainingS:rh.durationS)+' s'):'')
      +(rh.lost?' · PULSVERLUST':'');
  }
  if(mode==='WORKOUT'){
    msub=(wo.name||'Workout')+' · '+(wo.label||'')
      +' ('+(1+(wo.stepIndex||0))+'/'+(wo.stepCount||'?')+')'
      +(wo.desiredW!=null?(' · '+Math.round(wo.desiredW)+' W'):'')
      +(wo.state==='PAUSED'?' · PAUSE':'')
      +(rh.lost?' · PULSVERLUST':'');
  }
  $('rmodesub').textContent=msub;
  if(sess.active){
    const ds=sess.durationS!=null?sess.durationS:0;
    const extra=(sess.paused?' · PAUSE':'')
      +(sess.workKj!=null?(' · '+sess.workKj.toFixed(1)+' kJ'):'')
      +(sess.autoPauses?(' · AutoPause '+sess.autoPauses+'×'):'');
    $('rmodesub').textContent=(msub?msub+' · ':'')+'Session '+hms(ds)+extra;
  }
  $('moff').classList.toggle('ghost', mode!=='OFF');
  $('mlvl').classList.toggle('ghost', mode!=='MANUAL_LEVEL');
  $('merg').classList.toggle('ghost', mode!=='MANUAL_ERG');
  $('mhr').classList.toggle('ghost', mode!=='HR_HOLD');
  $('mreha').classList.toggle('ghost', mode!=='REHA');
  $('mwo').classList.toggle('ghost', mode!=='WORKOUT');
  const ergLike=mode==='MANUAL_ERG'||mode==='HR_HOLD'||mode==='REHA'||mode==='WORKOUT';
  $('lvlup').disabled=!hasP||ergLike;
  $('lvldn').disabled=!hasP||ergLike;
  const adv=$('rideadv');
  if(adv && (mode==='MANUAL_ERG'||mode==='HR_HOLD'||mode==='REHA')) adv.open=true;
  $('mlvl').disabled=!hasP;
  $('merg').disabled=!hasP||!(erg.mapReady);
  $('mhr').disabled=!hasP||!(erg.mapReady);
  $('mreha').disabled=!hasP||!(erg.mapReady);
  $('mwo').disabled=!hasP||!(erg.mapReady);
  const woOn=mode==='WORKOUT';
  $('wopause').disabled=!woOn||wo.state==='PAUSED';
  $('woresume').disabled=!woOn||wo.state!=='PAUSED';
  $('woskip').disabled=!woOn;
  $('erggo').disabled=!hasP||!(erg.mapReady)||mode==='HR_HOLD';
  $('hrgo').disabled=!hasP||!(erg.mapReady)||mode!=='HR_HOLD';
  if(hh.targetBpm&&document.activeElement!==$('hrbpm')) $('hrbpm').value=hh.targetBpm;
  if(mode==='REHA'){
    if(rh.desiredW!=null&&document.activeElement!==$('rehaw')) $('rehaw').value=Math.round(rh.desiredW);
    if(rh.hrMax&&document.activeElement!==$('rehahr')) $('rehahr').value=rh.hrMax;
  }
  $('ergsub').textContent=erg.mapReady
    ?(mode==='MANUAL_ERG'&&erg.ceiling?'Ziel oberhalb der Kennfläche — höchste Stufe':'')
    :'ERG/HR/REHA: zuerst Kalibrierung (Kennfläche)';
  let hrHint='';
  if(mode==='HR_HOLD'&&hh.lost){
    const pol=hh.onHrLoss||'reduce';
    hrHint=pol==='stop'?'Pulsverlust — STOP':
           pol==='freeze'?'Pulsverlust — Stufe eingefroren':
           'Pulsverlust — Watt wird abgesenkt';
  }
  $('hrsub').textContent=hrHint;
  let rehaHint='';
  if(mode==='REHA'){
    if(rh.lost){
      const pol=rh.onHrLoss||'reduce';
      rehaHint=pol==='stop'?'Pulsverlust — STOP':
               pol==='freeze'?'Pulsverlust — Stufe eingefroren':
               'Pulsverlust — Watt wird abgesenkt';
    } else if(rh.capActive){
      rehaHint='Deckel greift'+(rh.interventions?(' · '+rh.interventions+'×'):'')
        +(rh.effectiveW!=null?(' · '+Math.round(rh.effectiveW)+' W'):'');
    } else if(rh.interventions){
      rehaHint='Deckel griff '+rh.interventions+'×';
    }
  }
  $('rehasub').textContent=rehaHint;
  $('rprof').style.color=(pi&&pi.color)?('#'+('000000'+Number(pi.color).toString(16)).slice(-6)):'';

  const strat={'emulate-resistance':'Emulation über Widerstand','direct-target':'Wattziel direkt',
               'none':'nur Anzeige'}[c.strategy]||c.strategy;
  $('strat').textContent=c.strategy?(strat+(c.levels?(', '+c.levels+' Stufen'):'')):'-';
  $('dfeat').textContent=c.featureHex||'-';
  $('dres').textContent=c.resistanceHex||'-';
  $('dpow').textContent=c.powerRangeHex||'-';
  $('dtrust').textContent=c.strategy?yn(c.powerTrusted):'-';
  $('dcp').textContent=f.controlPoint?(f.controlGranted?'Hoheit erteilt':'vorhanden'):'-';
  $('dnot').textContent=(f.notifies!=null)?f.notifies:'-';
  $('dresp').textContent=(f.responses!=null)?f.responses:'-';
  const lr=f.lastResponse;
  $('dlast').textContent=lr?(lr.opcode+' -> '+lr.result):'-';
  $('limit').textContent=(li.writes!=null)?(li.writes+' gestellt, '+li.denies+' abgelehnt'):'-';
  $('ddeny').textContent=f.lastDeny||'-';
  $('dstale').textContent=f.attached?(f.stale?'still':'läuft'):'-';

  const sc=!!(s.ble&&s.ble.scanning);
  if($('t-dev').classList.contains('on') && (sc || (scanPrev&&!sc))) devs();
  scanPrev=sc;
}

/**
 * Stufen-Kachel nach Konzept: Segmente, gefuellte Stufe, Reserve, roter Rand
 * bei erreichter Decke. Das `~` sagt, dass der Wert ein Schattenwert ist —
 * das Bike meldet die Stufe nicht zurueck.
 */
function levelTile(li,c){
  const n=c.levels||0, step=c.levelStepTenths||10, min=c.levelMinTenths||0;
  const lt=li.levelTenths;
  const set=(lt==null||lt<0||!n)?0:Math.min(n,Math.max(0,Math.round((lt-min)/step)+1));
  const box=$('segs');
  if(box.childElementCount!==n){box.innerHTML='';
    for(let i=0;i<n;i++) box.appendChild(document.createElement('i'));}
  Array.prototype.forEach.call(box.children,(d,i)=>{d.className=(i<set)?'on':''});
  $('lvl').textContent=n?(set?('~'+set+'/'+n):('- / '+n)):'-';
  if($('docklvl')) $('docklvl').textContent=n?(set?('~'+set+'/'+n):('- / '+n)):'-';
  $('lvlsub').textContent=n?(set?('Reserve '+(n-set)):'nichts gestellt')
    :'kein Stellweg gemeldet';
  $('ltile').classList.toggle('cap', n>0 && set>=n);
}

const SWST={IDLE:'bereit',SETTLE:'einschwingen',MEASURE:'messen',DONE:'fertig',
            ABORTED:'abgebrochen'};
let swPts=-1, mpPts=-1;

/**
 * Der Sweep ist der einzige Vorgang, bei dem das Geraet von sich aus Last
 * stellt — und der einzige, bei dem der Mensch etwas leisten muss, damit die
 * Messung etwas wert ist. Deshalb steht die Kadenzabweichung so gross wie der
 * Zustand: sie ist die einzige Zahl, die der Fahrer waehrenddessen beeinflusst.
 */
function renderCalib(s){
  const c=s.calib||{}, sw=c.sweep||{}, m=c.map||{};
  const run=!!sw.running, step=m.stepTenths||10, min=(m.minTenths!=null)?m.minTenths:10;
  const lvOf=t=>Math.round((t-min)/step)+1;

  $('swst').textContent=SWST[sw.state]||'-';
  $('swsub').textContent=run?(Math.round((sw.remainingMs||0)/1000)+' s in dieser Phase')
    :(sw.abortReason?('Grund: '+sw.abortReason)
     :(sw.total?(sw.valid+' von '+sw.total+' Punkten gültig'):''));

  const lt=sw.levelTenths;
  $('swlvl').textContent=(run&&lt!=null&&lt>=0)?(lvOf(lt)+'/'+(m.levels||'?')):'-';
  $('swprog').textContent=(sw.total&&sw.state!=='IDLE')
    ?((sw.progress||0)+' % · Stufe '+((sw.index||0)+1)+' von '+sw.total):'';

  const cad=(s.ftms&&s.ftms.data)?s.ftms.data.cadenceRpm:null;
  const tgt=sw.targetRpm||0;
  if(run&&cad!=null&&tgt){
    const d=Math.round(cad-tgt);
    $('swcad').textContent=Math.round(cad)+' rpm';
    $('swcad').className='v big '+(Math.abs(d)<=4?'cadok':'cadbad');
    $('swcadsub').textContent=(d>0?'+':'')+d+' gegen Ziel '+Math.round(tgt)+' rpm';
  }else{
    $('swcad').textContent=tgt?(Math.round(tgt)+' rpm'):'-';
    $('swcad').className='v big';
    $('swcadsub').textContent=tgt?'Zielkadenz':'';
  }

  // Balken zeigt die laufende Phase, nicht den ganzen Sweep: „wann wechselt
  // die Stufe" ist waehrend des Tretens die nuetzlichere Frage.
  const ph=(sw.state==='MEASURE')?(sw.windowS||0):(sw.settleS||0);
  const gone=ph?Math.max(0,ph*1000-(sw.remainingMs||0)):0;
  $('swbar').style.width=(run&&ph)?((gone/(ph*1000)*100).toFixed(1)+'%'):'0';

  const bike=!!(s.ble&&s.ble.links&&s.ble.links.bike&&s.ble.links.bike.state==='READY');
  $('sw1').disabled=!bike||run;
  $('sw2').disabled=!bike||run;
  if($('sw2l')) $('sw2l').disabled=!bike||run;
  $('swx').disabled=!run;

  const pts=sw.points||[];
  if(pts.length!==swPts){
    swPts=pts.length;
    $('swnone').style.display=pts.length?'none':'';
    $('swpts').innerHTML=pts.map(p=>
      '<td>Stufe '+lvOf(p.levelTenths)
      +(p.valid?'':'<span class="tag">verworfen</span>')
      +'<br><span class="k">'+(p.valid
        ?(Math.round(p.rpmMin)+'-'+Math.round(p.rpmMax)+' rpm')
        :p.reason)+'</span></td>'
      +'<td class="r">'+(p.valid?(Math.round(p.watt)+' W'):'-')
      +'<br><span class="k">'+Math.round(p.rpm)+' rpm</span></td>')
      .map(x=>'<tr>'+x+'</tr>').join('');
  }

  $('mpn').textContent=(m.points!=null)?m.points:'-';
  $('mpnsub').textContent=(m.sweepCells!=null)?(m.sweepCells+' aus Sweeps'):'';
  $('mpl').textContent=m.levels?((m.levelsCovered||0)+' von '+m.levels):'-';
  $('mpb').textContent=(m.bandsCovered!=null)?((m.bandsCovered||0)+' von 8'):'-';

  if(m.points!=null&&m.points!==mpPts){
    if(mpPts>=0&&$('t-calib').classList.contains('on')) loadMap();
    mpPts=m.points;
  }
}

function loadMap(){
  fetch('/api/calib/map').then(r=>r.json()).then(j=>{
    const L=j.levels||0, B=j.bands||8, box=$('hm');
    if(!L){
      box.style.gridTemplateColumns='1fr';
      box.innerHTML='<div class="hd">kein Stellweg bekannt — erst ein Bike verbinden</div>';
      return;
    }
    box.style.gridTemplateColumns='30px repeat('+B+',1fr)';
    const W=j.w||[], N=j.n||[], S=j.s||[];
    let mx=0; W.forEach((v,i)=>{if(N[i]&&v>mx)mx=v});
    let h='<div class="hd"></div>';
    for(let b=0;b<B;b++) h+='<div class="hd">'+(j.cadMin+j.cadStep*b)+'</div>';
    // Hohe Stufen oben: die Flaeche soll sich wie ein Diagramm lesen.
    for(let l=L-1;l>=0;l--){
      h+='<div class="hd">'+(l+1)+'</div>';
      for(let b=0;b<B;b++){
        const i=l*B+b;
        if(!N[i]){h+='<div class="e"></div>';continue}
        const a=mx?(0.18+0.82*W[i]/mx):0.6;
        h+='<div class="'+(S[i]?'sw':'')+'" style="background:rgba(226,128,47,'
          +a.toFixed(2)+');color:'+(a>0.55?'#160B02':'#E6EAF2')+'">'+W[i]+'</div>';
      }
    }
    box.innerHTML=h;
  }).catch(e=>{$('mpmsg').className='msg err';$('mpmsg').textContent=''+e});
}

function devs(){
  fetch('/api/ble/devices').then(r=>r.json()).then(j=>{
    const t=$('devs'); t.innerHTML='';
    (j.devices||[]).sort((a,b)=>b.rssi-a.rssi).forEach(x=>{
      const tr=document.createElement('tr');
      tr.className='pick';
      // FTMS und HR sagen, was das Geraet bewirbt; die Rollenmarke sagt, als
      // was es gemerkt ist. Deshalb zwei Sprachen statt zweimal "HR".
      const tags=(x.ftms?'<span class="tag ftms">FTMS</span>':'')
               + (x.hr?'<span class="tag">HR</span>':'')
               + (x.role?'<span class="tag">gemerkt: '+(x.role==='hr'?'Gurt':'Bike')+'</span>':'');
      tr.innerHTML='<td>'+(x.name||'(ohne Namen)')+tags+'<br><span class="k">'+x.mac+'</span></td>'
                 + '<td class="r">'+x.rssi+' dBm</td>';
      tr.onclick=()=>join(x);
      t.appendChild(tr);
    });
    const n=(j.devices||[]).length;
    $('dmsg').textContent=j.scanning?('suche... '+n+' gefunden')
      :(n?'Zeile antippen zum Verbinden.':'nichts gefunden');
  }).catch(()=>{});
}

function join(x){
  const role=x.hr&&!x.ftms?'hr':'bike';
  $('dmsg').textContent='verbinde '+(x.name||x.mac)+' als '+role+' ...';
  fetch('/api/ble/connect',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({mac:x.mac,role:role,addrType:x.addrType})})
    .then(r=>r.json()).then(j=>{$('dmsg').textContent=j.ok?'verbunden':('Fehler: '+(j.error||'?'));})
    .catch(e=>{$('dmsg').textContent='Fehler: '+e;});
}

function post(url,msg){
  const m=$(msg||'cmsg');
  return fetch(url,{method:'POST'}).then(r=>r.json().then(j=>({s:r.status,j})))
    .then(o=>{m.className='msg '+(o.j.ok?'ok':'err');
      m.textContent=(o.j.result||o.j.error||'ok')+(o.j.reason?(' - '+o.j.reason):'');return o.j;})
    .catch(e=>{m.className='msg err';m.textContent=''+e;});
}

function lvDisp(t){return t==null||t<=0?'-':(t/10).toFixed(1);}
const PCOLORS=[0xE2802F,0x4EC9A5,0x3FB8B0,0xD8B23A,0xDE5334,0xC9304A,0xA63FB0,0x4CAF63];
function goalLabel(g){
  return ({fitness:'Training',fatloss:'Fettabbau',reha:'Reha',performance:'Leistung',none:''}[g]||'');
}
function hexColor(n){
  const v=(n>>>0)&0xffffff;
  return '#'+('000000'+v.toString(16)).slice(-6);
}
function paintColors(sel){
  const row=$('pf-colors'); if(!row) return;
  row.innerHTML='';
  PCOLORS.forEach(c=>{
    const b=document.createElement('button');
    b.type='button'; b.style.background=hexColor(c);
    b.className=(c===(sel>>>0))?'on':'';
    b.onclick=()=>{
      $('pf-color').value=String(c);
      paintColors(c);
    };
    row.appendChild(b);
  });
}
function updateHrHint(){
  const y=+$('pf-birth').value||0;
  const el=$('pf-hrehint');
  if(!el) return;
  if(!y){el.textContent=''; return;}
  const age=2026-y;
  const est=Math.round(208-0.7*age);
  el.textContent='Tanaka-Schätzung ~'+est+' bpm (Alter '+age+')';
}
let _plist=[];
function fillProfileForm(p){
  const e=!p;
  $('pfh').textContent=e?'Neues Profil':('Profil: '+(p.name||p.id));
  $('pf-id').value=p?p.id:'';
  $('pf-id').disabled=!!p;
  $('pf-name').value=p?p.name:'';
  $('pf-initial').value=p&&p.initial?p.initial:(p&&p.name?p.name.charAt(0):'');
  const col=p&&p.color?p.color:0xE2802F;
  $('pf-color').value=String(col);
  paintColors(col);
  $('pf-birth').value=p&&p.birthYear?p.birthYear:'';
  $('pf-weight').value=p&&p.weightKg?p.weightKg:'';
  $('pf-goal').value=(p&&p.goal)||'none';
  $('pf-ftp').value=p&&p.ftpW?p.ftpW:'';
  $('pf-ftporig').value=(p&&p.ftpOrigin)||'manual';
  $('pf-hrmax').value=p&&p.hrMax?p.hrMax:'';
  $('pf-rest').value=p&&p.restingHr?p.restingHr:'';
  $('pf-lthr').value=p&&p.lthr?p.lthr:'';
  $('pf-zbasis').value=(p&&p.zoneBasis)||'hrmax';
  $('pf-maxlvl').value=p&&p.maxLevelTenths? (p.maxLevelTenths/10).toFixed(1):'';
  $('pf-maxw').value=p&&p.maxPowerW?p.maxPowerW:'';
  $('pf-maxhr').value=p&&p.maxHr?p.maxHr:'';
  $('pf-cad').value=p&&p.targetCadenceRpm?p.targetCadenceRpm:'';
  $('pf-lead').value=(p&&p.leadingZone)||'power';
  $('pf-loss').value=(p&&p.onHrLoss)||'reduce';
  $('pfdel').disabled=!p;
  $('pfmsg').textContent='';
  updateHrHint();
  const st=$('pf-stats');
  if(st){
    const ftp=p&&p.ftpW, w=p&&p.weightKg;
    st.textContent=(ftp&&w)?('≈ '+(ftp/w).toFixed(2)+' W/kg'):'';
  }
}
function loadProfiles(){
  fetch('/api/profile/list').then(r=>r.json()).then(j=>{
    const box=$('pcards'); box.innerHTML='';
    const act=j.active||null;
    _plist=j.profiles||[];
    $('plnone').style.display=_plist.length?'none':'';
    _plist.forEach(p=>{
      const on=act&&act===p.id;
      const card=document.createElement('div');
      card.className='pcard'+(on?' on':'');
      card.style.setProperty('--pc', hexColor(p.color||0x4EC9A5));
      const ini=(p.initial||(p.name||'?').charAt(0)||'?').toUpperCase();
      const g=goalLabel(p.goal);
      card.innerHTML='<div class="av" style="background:'+hexColor(p.color||0x4EC9A5)+'">'+ini+'</div>'
        +'<div class="pn">'+(p.name||p.id)+'</div>'
        +'<div class="pm">'+(g?g+' · ':'')+'FTP '+(p.ftpW||'—')+' W'
        +(p.weightKg?(' · '+p.weightKg+' kg'):'')
        +'<br>HRmax '+(p.hrMax||'—')+(p.birthYear?(' · *'+p.birthYear):'')+'</div>'
        +'<div class="prow"></div>';
      const prow=card.querySelector('.prow');
      const bed=document.createElement('button');
      bed.className='ghost sm'; bed.textContent='Bearbeiten';
      bed.onclick=ev=>{ev.stopPropagation(); fillProfileForm(p);};
      prow.appendChild(bed);
      if(!on){
        const b=document.createElement('button');
        b.className='ghost sm'; b.textContent='Wählen';
        b.onclick=ev=>{ev.stopPropagation(); selectProfile(p.id);};
        prow.appendChild(b);
      } else {
        const t=document.createElement('span');
        t.className='tag'; t.textContent='aktiv';
        prow.appendChild(t);
      }
      card.onclick=()=>{ if(!on) selectProfile(p.id); else fillProfileForm(p); };
      box.appendChild(card);
    });
    const ap=_plist.find(x=>x.id===act);
    if(ap) fillProfileForm(ap);
    else if(!$('pf-id').value) fillProfileForm(null);
  }).catch(e=>{$('pmsg').className='msg err';$('pmsg').textContent=''+e;});
}
function selectProfile(id){
  const q=id?('?id='+encodeURIComponent(id)):'';
  fetch('/api/profile/select'+q,{method:'POST'}).then(r=>r.json().then(j=>({s:r.status,j})))
    .then(o=>{
      $('pmsg').className='msg '+(o.j.ok?'ok':'err');
      $('pmsg').textContent=o.j.ok?(o.j.active?('aktiv: '+o.j.active):'Auswahl aufgehoben')
        :(o.j.error||'Fehler');
      loadProfiles();
    }).catch(e=>{$('pmsg').className='msg err';$('pmsg').textContent=''+e;});
}
function saveProfile(){
  const id=$('pf-id').value.trim();
  const name=$('pf-name').value.trim();
  if(!id||!name){$('pfmsg').className='msg err';$('pfmsg').textContent='ID und Name nötig';return;}
  let initial=$('pf-initial').value.trim();
  if(!initial) initial=name.charAt(0);
  const body={id:id,name:name,initial:initial,
    color:+$('pf-color').value||0xE2802F,
    birthYear:+$('pf-birth').value||0,
    weightKg:+$('pf-weight').value||0,
    goal:$('pf-goal').value,
    ftpW:+$('pf-ftp').value||0, ftpOrigin:$('pf-ftporig').value,
    hrMax:+$('pf-hrmax').value||0, restingHr:+$('pf-rest').value||0,
    lthr:+$('pf-lthr').value||0, zoneBasis:$('pf-zbasis').value,
    maxPowerW:+$('pf-maxw').value||0, maxHr:+$('pf-maxhr').value||0,
    maxLevelTenths:Math.round((+$('pf-maxlvl').value||0)*10),
    targetCadenceRpm:+$('pf-cad').value||0,
    leadingZone:$('pf-lead').value, onHrLoss:$('pf-loss').value};
  fetch('/api/profile/put',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(body)}).then(r=>r.json().then(j=>({s:r.status,j})))
    .then(o=>{
      $('pfmsg').className='msg '+(o.j.ok?'ok':'err');
      $('pfmsg').textContent=o.j.ok?'gespeichert':(o.j.error||'Fehler');
      if(o.j.ok) loadProfiles();
    }).catch(e=>{$('pfmsg').className='msg err';$('pfmsg').textContent=''+e;});
}
function deleteProfile(){
  const id=$('pf-id').value.trim();
  if(!id||!confirm('Profil „'+id+'“ löschen?')) return;
  fetch('/api/profile/delete?id='+encodeURIComponent(id),{method:'POST'})
    .then(r=>r.json().then(j=>({s:r.status,j})))
    .then(o=>{
      $('pfmsg').className='msg '+(o.j.ok?'ok':'err');
      $('pfmsg').textContent=o.j.ok?'gelöscht':(o.j.error||'Fehler');
      if(o.j.ok){fillProfileForm(null); loadProfiles();}
    }).catch(e=>{$('pfmsg').className='msg err';$('pfmsg').textContent=''+e;});
}
$('prld').onclick=()=>loadProfiles();
$('pclr').onclick=()=>selectProfile('');
$('pnew').onclick=()=>fillProfileForm(null);
$('pfsave').onclick=()=>saveProfile();
$('pfdel').onclick=()=>deleteProfile();
$('pf-birth').oninput=()=>updateHrHint();
$('pfest').onclick=()=>{
  const y=+$('pf-birth').value||0;
  if(!y){$('pfmsg').className='msg err';$('pfmsg').textContent='Geburtsjahr setzen';return;}
  const est=Math.round(208-0.7*(2026-y));
  $('pf-hrmax').value=est;
  if(!+$('pf-maxhr').value) $('pf-maxhr').value=est;
  $('pfmsg').className='msg ok';
  $('pfmsg').textContent='HRmax ≈ '+est+' (Tanaka) — bitte speichern';
};

$('scan').onclick=()=>{$('dmsg').textContent='suche 8 s ...';
  fetch('/api/ble/scan/start',{method:'POST'}).catch(()=>{});};
$('brec').onclick=()=>post('/api/ble/reconnect?role=bike','rmsg');
$('bdis').onclick=()=>post('/api/ble/disconnect?role=bike','rmsg');
$('bfor').onclick=()=>post('/api/ble/forget?role=bike','rmsg');
$('hrec').onclick=()=>post('/api/ble/reconnect?role=hr','rmsg');
$('hdis').onclick=()=>post('/api/ble/disconnect?role=hr','rmsg');
$('hfor').onclick=()=>post('/api/ble/forget?role=hr','rmsg');
$('req').onclick=()=>post('/api/control/request');
$('panic').onclick=()=>post('/api/control/stop');
$('cstart').onclick=()=>post('/api/control/start');
$('creset').onclick=()=>post('/api/control/reset');
$('moff').onclick=()=>post('/api/control/mode?mode=off');
$('mlvl').onclick=()=>post('/api/control/mode?mode=level');
$('merg').onclick=()=>{
  const w=+$('ergw').value||80;
  post('/api/control/mode?mode=erg&watt='+w);
};
$('mhr').onclick=()=>{
  const bpm=+$('hrbpm').value||130;
  post('/api/control/mode?mode=hr&hr='+bpm);
};
$('mreha').onclick=()=>{
  const w=+$('rehaw').value||60;
  const hr=+$('rehahr').value||120;
  const min=+$('rehamin').value;
  const dur=(min>0)?(Math.round(min*60)):0;
  post('/api/control/mode?mode=reha&watt='+w+'&hrMax='+hr+'&durationS='+dur);
};
$('mwo').onclick=()=>post('/api/workout/start?id=physio');
$('wopause').onclick=()=>post('/api/workout/pause');
$('woresume').onclick=()=>post('/api/workout/resume');
$('woskip').onclick=()=>post('/api/workout/skip');

let _woSelId='';
let _woPreview=null;
function loadWorkouts(){
  const m=$('womsg'); m.className='msg'; m.textContent='';
  fetch('/api/workout/list').then(r=>r.json()).then(d=>{
    const rows=[];
    (d.builtins||[]).forEach(x=>rows.push(Object.assign({source:'builtin'},x)));
    (d.files||[]).forEach(x=>rows.push(Object.assign({source:x.source||'file'},x)));
    $('wonone').hidden=rows.length>0;
    $('wonone').textContent=rows.length?'':'keine Programme';
    const box=$('wocards'); box.innerHTML='';
    rows.forEach(x=>{
      const id=x.id||'';
      const card=document.createElement('div');
      card.className='pcard'+(id===_woSelId?' on':'');
      card.dataset.id=id;
      card.innerHTML='<div class="pn">'+(x.name||id)+'</div>'
        +'<div class="pm">'+(x.source||'')+(x.bytes?(' · '+x.bytes+' B'):'')+'</div>'
        +'<div class="prow"></div>';
      const prow=card.querySelector('.prow');
      const bPrev=document.createElement('button');
      bPrev.className='ghost sm'; bPrev.textContent='Vorschau';
      bPrev.onclick=ev=>{ev.stopPropagation(); previewWorkoutId(id);};
      prow.appendChild(bPrev);
      const bGo=document.createElement('button');
      bGo.className='ghost sm'; bGo.textContent='Start';
      bGo.onclick=ev=>{
        ev.stopPropagation();
        const sc=+$('woscale').value||1;
        post('/api/workout/start?id='+encodeURIComponent(id)+'&scale='+sc,'womsg');
      };
      prow.appendChild(bGo);
      card.onclick=()=>previewWorkoutId(id);
      box.appendChild(card);
    });
  }).catch(e=>{m.className='msg err'; m.textContent=String(e);});
  fetch('/api/session/last').then(r=>r.json()).then(d=>{
    if(!d.ok){$('wosess').textContent='noch keine'; return;}
    $('wosess').textContent=fmtSess(d);
  }).catch(()=>{});
}
$('woreload').onclick=()=>loadWorkouts();

const TST_CAT=[
  {id:'test_ramp',name:'Rampe',blurb:'60 W, +20 W / min · Stop = Abbruch · FTP ≈ 0,75 × Spitze'},
  {id:'test_20min',name:'20 Minuten',blurb:'Warmup + 20 min bei 100 % FTP · FTP ≈ 0,95 × Ø'},
  {id:'test_recovery',name:'Recovery',blurb:'Belastung + 60 s leicht · Note aus Puls (Session)'}
];
let _tstId='';
let _tstPreview=null;
let _tstPropose=0;
let _tstProfileId='';

function isTestSession(x){
  const n=(x&& (x.workoutName||''))||'';
  return n==='Rampe'||n==='20 Minuten'||n==='Recovery'
    ||n.indexOf('test_')===0;
}
function loadTests(){
  const m=$('tstmsg'); m.className='msg'; m.textContent='';
  fetch('/api/status').then(r=>r.json()).then(s=>{
    const pi=s.profileInfo||{};
    _tstProfileId=pi.id||'';
    const reha=pi.goal==='reha';
    $('tstnone').hidden=false;
    if(reha){
      $('tstcards').innerHTML='';
      $('tstnone').textContent='Bei Reha-Profil ausgeblendet (Maximaltests).';
      $('tststart').disabled=true;
      return;
    }
    if(!pi.id){
      $('tstcards').innerHTML='';
      $('tstnone').textContent='Zuerst Profil wählen (Reiter Profile).';
      $('tststart').disabled=true;
      return;
    }
    $('tstnone').hidden=true;
    const box=$('tstcards'); box.innerHTML='';
    TST_CAT.forEach(t=>{
      const card=document.createElement('div');
      card.className='pcard'+(t.id===_tstId?' on':'');
      card.dataset.id=t.id;
      card.innerHTML='<div class="pn">'+t.name+'</div><div class="pm">'+t.blurb+'</div><div class="prow"></div>';
      const prow=card.querySelector('.prow');
      const b=document.createElement('button');
      b.className='ghost sm'; b.textContent='Prüfen';
      b.onclick=ev=>{ev.stopPropagation(); previewTest(t.id);};
      prow.appendChild(b);
      card.onclick=()=>previewTest(t.id);
      box.appendChild(card);
    });
  }).catch(e=>{m.className='msg err'; m.textContent=String(e);});
  loadTestResult();
  loadTestHist();
}
$('tstreload').onclick=()=>loadTests();

function previewTest(id){
  _tstId=id;
  document.querySelectorAll('#tstcards .pcard').forEach(c=>c.classList.toggle('on',c.dataset.id===id));
  fetch('/api/workout/download?id='+encodeURIComponent(id)).then(r=>r.text()).then(txt=>
    fetch('/api/workout/validate',{method:'POST',headers:{'Content-Type':'application/json'},body:txt})
      .then(r=>r.json())).then(d=>{
    _tstPreview=d;
    const meta=$('tstprevmeta');
    if(!d||!d.ok){
      meta.textContent='ungueltig';
      const cv=$('tstprev'); if(cv){const c=cv.getContext('2d');c.clearRect(0,0,cv.width,cv.height);}
      $('tststart').disabled=true;
      return;
    }
    meta.textContent=(d.name||id)+' · '+hmsShort(d.durationS||0)
      +' · Spitze '+(d.peakW!=null?Math.round(d.peakW)+' W':'—')
      +(d.mapLevelTenths!=null?(' · ~Stufe '+(d.mapLevelTenths/10).toFixed(1)):'')
      +(d.feasible===false?' · nicht machbar':(d.feasible?' · machbar':''));
    const cv=$('tstprev');
    if(cv){
      const tl=d.timeline||[];
      const ctx=cv.getContext('2d');
      const W=cv.width, H=cv.height;
      ctx.clearRect(0,0,W,H);
      let tot=0, peak=1;
      tl.forEach(s=>{tot+=(s.durationS|0); peak=Math.max(peak,s.resolvedW||s.powerW||0);});
      if(tot>0){
        peak*=1.12; let x=0;
        const cols=['#3FB8B0','#4CAF63','#D8B23A','#E2802F','#DE5334','#C9304A','#A63FB0'];
        tl.forEach((s,i)=>{
          const dur=s.durationS|0;
          const w=s.resolvedW||s.powerW||0;
          const bw=W*(dur/tot);
          const bh=Math.max(2,(w/peak)*(H-12));
          ctx.fillStyle=cols[i%cols.length];
          ctx.fillRect(x, H-6-bh, Math.max(1,bw-1), bh);
          x+=bw;
        });
      }
    }
    const w=$('tstwarn');
    const warns=d.warnings||[];
    if(warns.length){
      w.className='msg err';
      w.textContent=warns.join(' · ')+' — Start gesperrt, bis Kennfläche/Profil passen.';
    } else {
      w.className='msg ok';
      w.textContent='gegen Profil/Kennfläche ok';
    }
    $('tststart').disabled=!(d.feasible!==false && d.ok);
  }).catch(e=>{
    const w=$('tstwarn'); w.className='msg err'; w.textContent=String(e);
    $('tststart').disabled=true;
  });
}
$('tststart').onclick=()=>{
  if(!_tstId) return;
  const sc=+$('tstscale').value||1;
  post('/api/workout/start?id='+encodeURIComponent(_tstId)+'&scale='+sc,'tstmsg');
};

function proposeFromSession(d){
  _tstPropose=0;
  if(!d||!d.ok) return {text:'noch keine', ftp:0};
  const name=d.workoutName||'';
  let ftp=0, text=fmtSess(d);
  if(name==='Rampe'){
    let peak=Math.max(d.avgDesiredW||0, d.avgPowerW||0);
    if(d.endReason==='done'&&_tstPreview&&_tstPreview.peakW)
      peak=Math.max(peak,_tstPreview.peakW);
    ftp=peak>0?Math.round(0.75*peak):0;
    text+=' · Vorschlag FTP '+(ftp||'—')+' W (0,75 × Spitze ~'
      +Math.round(peak)+' W — kein echtes 1-Min-MAP)';
  } else if(name==='20 Minuten'){
    const avg=d.avgPowerW||0;
    ftp=Math.round(0.95*avg);
    text+=' · Vorschlag FTP '+ftp+' W (0,95 × Ø '+Math.round(avg)+' W)';
  } else if(name==='Recovery'){
    const drop=(d.hrMax||0)-(d.hrAvg||0);
    text+=' · Puls max '+(d.hrMax||0)+' / Ø '+(d.hrAvg||0)
      +(drop>0?(' · grobe Drop-Note '+drop+' bpm'):'')
      +' — volle Erholungsnote folgt mit TestRunner';
  }
  return {text, ftp};
}
function loadTestResult(){
  fetch('/api/session/last').then(r=>r.json()).then(d=>{
    if(!d.ok||!isTestSession(d)){
      $('tstresmeta').textContent='nach Stop oder Abschluss';
      $('tstres').textContent='-';
      $('tstaccept').disabled=true;
      _tstPropose=0;
      return;
    }
    $('tstresmeta').textContent=(d.workoutName||'')+' · '+(d.endReason||'');
    const p=proposeFromSession(d);
    $('tstres').textContent=p.text;
    _tstPropose=p.ftp|0;
    $('tstaccept').disabled=!(_tstPropose>0 && _tstProfileId);
  }).catch(()=>{});
}
$('tstresload').onclick=()=>loadTestResult();
$('tstaccept').onclick=()=>{
  const m=$('tstresmsg');
  if(!_tstPropose||!_tstProfileId){
    m.className='msg err'; m.textContent='kein Vorschlag'; return;
  }
  if(!confirm('FTP '+_tstPropose+' W ins Profil '+_tstProfileId+' übernehmen?')) return;
  m.className='msg'; m.textContent='schreibe…';
  fetch('/api/profile/get?id='+encodeURIComponent(_tstProfileId)).then(r=>r.json()).then(p=>{
    if(!p||!p.id) throw new Error('Profil fehlt');
    p.ftpW=_tstPropose;
    p.ftpOrigin='test';
    p.ftpDateUnix=Math.floor(Date.now()/1000);
    return fetch('/api/profile/put',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify(p)}).then(r=>r.json());
  }).then(d=>{
    m.className='msg '+(d.ok?'ok':'err');
    m.textContent=d.ok?('FTP '+_tstPropose+' W übernommen'):(d.error||'Fehler');
  }).catch(e=>{m.className='msg err'; m.textContent=String(e);});
};
function loadTestHist(){
  fetch('/api/session/list').then(r=>r.json()).then(d=>{
    const rows=(d.sessions||[]).filter(isTestSession);
    $('tsthistnone').hidden=rows.length>0;
    $('tsthist').innerHTML=rows.length
      ?('<tr><th>Test</th><th>Dauer</th><th>Leistung</th><th>Ende</th></tr>'+
        rows.map(x=>'<tr><td>'+(x.workoutName||'')+'</td><td>'+(x.durationS||0)+' s</td><td>'
          +(x.avgPowerW!=null?Math.round(x.avgPowerW)+' W':'—')
          +(x.hrMax?(' · HR≤'+x.hrMax):'')+'</td><td>'+(x.endReason||'')+'</td></tr>').join(''))
      :'';
  }).catch(()=>{});
}

function hmsShort(s){
  s=s|0; const m=Math.floor(s/60), r=s%60;
  return m+':'+(r<10?'0':'')+r;
}

let _woEd={name:'',id:'',unit:'w',steps:[]};
let _woEdQuiet=false;
let _woDeb=0;
function woSlug(s){
  return String(s||'').toLowerCase().replace(/[^a-z0-9]+/g,'_').replace(/^_|_$/g,'').slice(0,23)||'workout';
}
function editorDefaultStep(){
  return {label:'Schritt',durationS:120,powerW:60,ftpPct:0,hrMax:0,hrSoft:0};
}
function editorFromJson(txt){
  let d=null;
  try{d=JSON.parse(txt);}catch(e){return false;}
  if(!d||!Array.isArray(d.steps)) return false;
  const steps=[];
  let unit=_woEd.unit;
  d.steps.slice(0,8).forEach((s,i)=>{
    const t=(s&&s.target)||{};
    const lim=(s&&s.limit)||{};
    const st={label:(s&&s.label)||('Schritt '+(i+1)),
      durationS:(s&&(s.duration_s||s.durationS))|0,
      powerW:0,ftpPct:0,
      hrMax:(lim.hr_max||lim.hrMax||0)|0,
      hrSoft:(lim.hr_soft||lim.hrSoft||0)|0};
    if(t.ftp_pct!=null||t.ftpPct!=null){
      st.ftpPct=+(t.ftp_pct!=null?t.ftp_pct:t.ftpPct)||0; unit='ftp';
    } else {
      st.powerW=+(t.power!=null?t.power:(s.powerW||0))||0; if(st.powerW) unit='w';
    }
    if(st.durationS<1) st.durationS=60;
    steps.push(st);
  });
  _woEd={name:d.name||'',id:d.id||woSlug(d.name),unit:unit,steps:steps};
  renderEditor();
  return true;
}
function editorToJson(){
  const name=($('woedname').value||_woEd.name||'Workout').trim();
  let id=($('woedid').value||_woEd.id||'').trim()||woSlug(name);
  _woEd.name=name; _woEd.id=id;
  const steps=_woEd.steps.map(st=>{
    const o={type:'steady',duration_s:st.durationS|0,label:st.label||'',
      target:{},limit:{}};
    if(_woEd.unit==='ftp') o.target.ftp_pct=+st.ftpPct||0;
    else o.target.power=+st.powerW||0;
    if(st.hrMax) o.limit.hr_max=st.hrMax|0;
    if(st.hrSoft) o.limit.hr_soft=st.hrSoft|0;
    if(!o.limit.hr_max&&!o.limit.hr_soft) delete o.limit;
    return o;
  });
  return JSON.stringify({name:name,id:id,steps:steps},null,2);
}
function pushEditor(validate){
  if(_woEdQuiet) return;
  const txt=editorToJson();
  $('wojson').value=txt;
  if(validate!==false){
    clearTimeout(_woDeb);
    _woDeb=setTimeout(()=>validateWorkoutUi(false),350);
  }
}
function renderEditor(){
  _woEdQuiet=true;
  if($('woedname')&&document.activeElement!==$('woedname')) $('woedname').value=_woEd.name||'';
  if($('woedid')&&document.activeElement!==$('woedid')) $('woedid').value=_woEd.id||'';
  $('woedunit').textContent=_woEd.unit==='ftp'?'Ziel: % FTP':'Ziel: Watt';
  const box=$('woedsteps'); box.innerHTML='';
  _woEd.steps.forEach((st,i)=>{
    const row=document.createElement('div');
    row.className='woedrow';
    const tgtLabel=_woEd.unit==='ftp'?'% FTP':'Watt';
    const tgtVal=_woEd.unit==='ftp'?st.ftpPct:st.powerW;
    row.innerHTML=
      '<label class="f" style="margin:0"><div class="k">Label</div><input data-f="label" type="text" maxlength="23"></label>'+
      '<label class="f" style="margin:0"><div class="k">Dauer s</div><input data-f="durationS" type="number" min="5" max="7200" step="5"></label>'+
      '<label class="f" style="margin:0"><div class="k">'+tgtLabel+'</div><input data-f="tgt" type="number" min="0" max="600" step="1"></label>'+
      '<label class="f" style="margin:0"><div class="k">HR max</div><input data-f="hrMax" type="number" min="0" max="220"></label>'+
      '<label class="f" style="margin:0"><div class="k">HR soft</div><input data-f="hrSoft" type="number" min="0" max="220"></label>'+
      '<div class="ops">'+
        '<button type="button" class="ghost sm" data-a="up">↑</button>'+
        '<button type="button" class="ghost sm" data-a="dn">↓</button>'+
        '<button type="button" class="ghost sm" data-a="rm">×</button></div>';
    row.querySelector('[data-f=label]').value=st.label||'';
    row.querySelector('[data-f=durationS]').value=st.durationS||60;
    row.querySelector('[data-f=tgt]').value=tgtVal||0;
    row.querySelector('[data-f=hrMax]').value=st.hrMax||'';
    row.querySelector('[data-f=hrSoft]').value=st.hrSoft||'';
    row.querySelectorAll('input').forEach(inp=>{
      inp.oninput=()=>{
        const f=inp.dataset.f;
        if(f==='label') st.label=inp.value;
        else if(f==='durationS') st.durationS=+inp.value||60;
        else if(f==='tgt'){
          if(_woEd.unit==='ftp'){st.ftpPct=+inp.value||0; st.powerW=0;}
          else {st.powerW=+inp.value||0; st.ftpPct=0;}
        }
        else if(f==='hrMax') st.hrMax=+inp.value||0;
        else if(f==='hrSoft') st.hrSoft=+inp.value||0;
        pushEditor(true);
      };
    });
    row.querySelector('[data-a=up]').onclick=()=>{
      if(i<1) return;
      const t=_woEd.steps[i-1]; _woEd.steps[i-1]=_woEd.steps[i]; _woEd.steps[i]=t;
      renderEditor(); pushEditor(true);
    };
    row.querySelector('[data-a=dn]').onclick=()=>{
      if(i>=_woEd.steps.length-1) return;
      const t=_woEd.steps[i+1]; _woEd.steps[i+1]=_woEd.steps[i]; _woEd.steps[i]=t;
      renderEditor(); pushEditor(true);
    };
    row.querySelector('[data-a=rm]').onclick=()=>{
      _woEd.steps.splice(i,1); renderEditor(); pushEditor(true);
    };
    box.appendChild(row);
  });
  $('woedadd').disabled=_woEd.steps.length>=8;
  _woEdQuiet=false;
}
$('woedunit').onclick=()=>{
  _woEd.unit=_woEd.unit==='ftp'?'w':'ftp';
  _woEd.steps.forEach(st=>{
    if(_woEd.unit==='ftp'){ if(!st.ftpPct&&st.powerW) st.ftpPct=50; st.powerW=0; }
    else { if(!st.powerW&&st.ftpPct) st.powerW=60; st.ftpPct=0; }
  });
  renderEditor(); pushEditor(true);
};
$('woedadd').onclick=()=>{
  if(_woEd.steps.length>=8) return;
  const s=editorDefaultStep();
  if(_woEd.unit==='ftp'){s.ftpPct=60;s.powerW=0;}
  _woEd.steps.push(s); renderEditor(); pushEditor(true);
};
$('woedblank').onclick=()=>{
  _woEd={name:'Neu',id:'neu',unit:'w',steps:[editorDefaultStep()]};
  renderEditor(); pushEditor(true);
};
$('woedsync').onclick=()=>{
  if(!editorFromJson($('wojson').value)){
    const m=$('wojmsg'); m.className='msg err'; m.textContent='JSON nicht lesbar';
  } else pushEditor(true);
};
$('woeddl').onclick=()=>{
  const txt=editorToJson();
  const a=document.createElement('a');
  a.href=URL.createObjectURL(new Blob([txt],{type:'application/json'}));
  a.download=(_woEd.id||'workout')+'.json';
  a.click(); URL.revokeObjectURL(a.href);
};
$('woedname').oninput=()=>{
  _woEd.name=$('woedname').value;
  if(!$('woedid').dataset.touch) $('woedid').value=woSlug(_woEd.name);
  _woEd.id=$('woedid').value; pushEditor(true);
};
$('woedid').oninput=()=>{ $('woedid').dataset.touch='1'; _woEd.id=$('woedid').value; pushEditor(true); };
if(!_woEd.steps.length){
  _woEd={name:'Physio Grundlage',id:'physio',unit:'w',steps:[
    {label:'Einfahren',durationS:120,powerW:40,ftpPct:0,hrMax:120,hrSoft:115},
    {label:'Hauptteil',durationS:600,powerW:60,ftpPct:0,hrMax:120,hrSoft:115},
    {label:'Ausfahren',durationS:120,powerW:35,ftpPct:0,hrMax:120,hrSoft:0}
  ]};
  renderEditor(); pushEditor(false);
}

function drawWorkoutPreview(tl){
  const cv=$('woprev'); if(!cv) return;
  const ctx=cv.getContext('2d');
  const W=cv.width, H=cv.height;
  ctx.clearRect(0,0,W,H);
  if(!tl||!tl.length) return;
  let tot=0, peak=1;
  tl.forEach(s=>{tot+=(s.durationS|0); peak=Math.max(peak,s.resolvedW||s.powerW||0);});
  if(tot<1) return;
  peak*=1.12;
  let x=0;
  const cols=['#3FB8B0','#4CAF63','#D8B23A','#E2802F','#DE5334','#C9304A','#A63FB0'];
  tl.forEach((s,i)=>{
    const dur=s.durationS|0;
    const w=s.resolvedW||s.powerW||0;
    const bw=W*(dur/tot);
    const bh=Math.max(2,(w/peak)*(H-16));
    ctx.fillStyle=cols[i%cols.length];
    ctx.fillRect(x, H-8-bh, Math.max(1,bw-1), bh);
    x+=bw;
  });
}
function showWorkoutPreview(d, id){
  _woPreview=d; _woSelId=id||d.id||'';
  const meta=$('woprevmeta');
  if(!d||!d.ok){
    meta.textContent='ungültig';
    drawWorkoutPreview([]);
    $('wosteps').innerHTML='';
    $('wostartsel').disabled=true;
    return;
  }
  meta.textContent=(d.name||d.id||'')+' · '+hmsShort(d.durationS||0)
    +' · Spitze '+(d.peakW!=null?Math.round(d.peakW)+' W':'—')
    +(d.avgW!=null?(' · Ø '+Math.round(d.avgW)+' W'):'')
    +(d.ftpW?(' · FTP '+d.ftpW):'')
    +(d.mapLevelTenths!=null?(' · ~Stufe '+(d.mapLevelTenths/10).toFixed(1)):'')
    +(d.feasible===false?' · Warnungen':(d.feasible?' · machbar':''));
  drawWorkoutPreview(d.timeline||[]);
  const steps=d.timeline||[];
  $('wosteps').innerHTML=steps.length
    ?('<tr><th>#</th><th>Label</th><th>Dauer</th><th>Ziel</th><th>Puls</th></tr>'+
      steps.map(s=>{
        const tgt=s.resolvedW!=null?(Math.round(s.resolvedW)+' W')
          :(s.ftpPct!=null?(s.ftpPct+' % FTP'):(s.powerW!=null?(Math.round(s.powerW)+' W'):'—'));
        return '<tr><td>'+(1+(s.i|0))+'</td><td>'+(s.label||'')+'</td><td>'+hmsShort(s.durationS)
          +'</td><td>'+tgt+'</td><td>'+(s.hrMax?('≤ '+s.hrMax):'—')+'</td></tr>';
      }).join(''))
    :'';
  const w=$('wowarn');
  const warns=d.warnings||[];
  if(warns.length){
    w.className='msg err';
    w.textContent=warns.join(' · ');
  } else {
    w.className='msg ok';
    w.textContent=d.feasible?'gegen Profil/Kennfläche ok':'';
  }
  $('wostartsel').disabled=!_woSelId;
  document.querySelectorAll('#wocards .pcard').forEach(c=>{
    c.classList.toggle('on', c.dataset.id===_woSelId);
  });
}
function previewWorkoutId(id){
  _woSelId=id;
  fetch('/api/workout/download?id='+encodeURIComponent(id)).then(r=>r.text()).then(txt=>{
    $('wojson').value=txt;
    editorFromJson(txt);
    return fetch('/api/workout/validate',{method:'POST',headers:{'Content-Type':'application/json'},body:txt});
  }).then(r=>r.json()).then(d=>{
    showWorkoutPreview(d, id);
  }).catch(e=>{
    const m=$('wowarn'); m.className='msg err'; m.textContent=String(e);
  });
}
$('wostartsel').onclick=()=>{
  if(!_woSelId) return;
  const sc=+$('woscale').value||1;
  post('/api/workout/start?id='+encodeURIComponent(_woSelId)+'&scale='+sc,'womsg');
};

function fmtSess(d){
  return (d.mode||'')+' · '+(d.workoutName||d.profileId||'')+' · '+
    (d.durationS||0)+' s'+(d.pausedS?(' / Pause '+d.pausedS+' s'):'')+
    (d.avgPowerW!=null?(' · Ø '+Math.round(d.avgPowerW)+' W'):'')+
    (d.workKj!=null?(' · '+Number(d.workKj).toFixed(1)+' kJ'):'')+
    (d.hrAvg?(' · HR '+d.hrAvg+(d.hrMax?('/'+d.hrMax):'')+' bpm'):'')+
    (d.interventions?(' · Deckel '+d.interventions+'×'):'')+
    (d.autoPauses?(' · AutoPause '+d.autoPauses+'×'):'')+
    (d.endReason?(' · '+d.endReason):'');
}
function loadSessions(){
  const m=$('sessmsg'); if(m){m.className='msg'; m.textContent='';}
  fetch('/api/session/list').then(r=>r.json()).then(d=>{
    const rows=d.sessions||[];
    $('sessnone').hidden=rows.length>0;
    $('sesslist').innerHTML=rows.length
      ?('<tr><th>Modus</th><th>Dauer</th><th>Leistung</th><th>Zonen</th><th>Ende</th></tr>'+
        rows.map(x=>'<tr><td>'+(x.mode||'')+(x.workoutName?(' · '+x.workoutName):'')+
          '</td><td>'+(x.durationS||0)+' s'+(x.pausedS?(' (+'+x.pausedS+' Pause)'):'')+
          '</td><td>'+(x.avgPowerW!=null?Math.round(x.avgPowerW)+' W':'—')+
          (x.workKj!=null?(' / '+Number(x.workKj).toFixed(1)+' kJ'):'')+
          '</td><td>'+zoneMiniHtml(x.zoneTimeS,x.zoneCount||(x.leadHr?5:7))+
          '</td><td>'+(x.endReason||'')+
          (x.interventions?(' · Deckel '+x.interventions):'')+
          (x.autoPauses?(' · AP '+x.autoPauses):'')+'</td></tr>').join(''))
      :'';
  }).catch(e=>{if(m){m.className='msg err'; m.textContent=String(e);}});
  fetch('/api/session/last').then(r=>r.json()).then(d=>{
    $('sesslast').textContent=d.ok?fmtSess(d):'noch keine';
  }).catch(()=>{});
}
$('sessreload').onclick=()=>{loadSessions(); loadProgression();};

function drawProgChart(durs){
  const cv=$('progcv'); if(!cv) return;
  const ctx=cv.getContext('2d');
  const W=cv.width, H=cv.height;
  ctx.clearRect(0,0,W,H);
  if(!durs||!durs.length) return;
  const max=Math.max.apply(null,durs.concat([600]))*1.1;
  const bw=Math.max(4,(W-8)/durs.length-2);
  durs.forEach((d,i)=>{
    const h=Math.max(2,(d/max)*(H-14));
    ctx.fillStyle=i===durs.length-1?'var(--accent)':'#3FB8B0';
    ctx.fillStyle=i===durs.length-1?'#E2802F':'#3FB8B0';
    ctx.fillRect(6+i*(bw+2), H-8-h, bw, h);
  });
}
function loadProgression(){
  const m=$('progmsg'); if(m){m.className='msg'; m.textContent='';}
  Promise.all([
    fetch('/api/progression/get?id=physio').then(r=>r.json()),
    fetch('/api/session/list').then(r=>r.json()),
    fetch('/api/session/last').then(r=>r.json())
  ]).then(([p,list,last])=>{
    const cur=p.currentMainS||600, next=p.nextMainS||(cur+60);
    $('progmeta').textContent='Hauptteil jetzt '+hmsShort(cur)
      +(p.maxS?(' · max '+hmsShort(p.maxS)):'')
      +(p.stepS?(' · Schritt +'+p.stepS+' s'):'');
    const rows=(list.sessions||[]).filter(x=>
      (x.workoutName||'')==='Physio Grundlage' || (x.workoutName||'').indexOf('Physio')===0);
    const durs=rows.slice().reverse().map(x=>x.durationS||0).filter(x=>x>0);
    if(p.currentMainS) durs.push(p.currentMainS);
    drawProgChart(durs.slice(-12));
    $('proghist').innerHTML=rows.length
      ?('<tr><th>Dauer</th><th>Ø W</th><th>Puls</th><th>Deckel</th><th>Ende</th></tr>'+
        rows.map(x=>'<tr><td>'+hmsShort(x.durationS)+'</td><td>'
          +(x.avgPowerW!=null?Math.round(x.avgPowerW):'—')+'</td><td>'
          +(x.hrAvg||'—')+(x.hrMax?('/'+x.hrMax):'')+'</td><td>'
          +(x.interventions||0)+'</td><td>'+(x.endReason||'')+'</td></tr>').join(''))
      :'';
    const offer=(last&&last.progression)||p.offer||{};
    const show=!!(offer.pending||offer.workoutId);
    if(!show){
      $('progoffer').textContent='Noch kein Angebot — Physio bis zum Ende fahren.';
      $('progyes').disabled=true; $('progno').disabled=true;
      return;
    }
    if(offer.clean && offer.nextMainS>offer.currentMainS){
      $('progoffer').textContent='Sauber · Hauptteil '
        +hmsShort(offer.currentMainS)+' → '+hmsShort(offer.nextMainS)+'?';
      $('progyes').disabled=false; $('progno').disabled=false;
    } else {
      $('progoffer').textContent='Keine Steigerung: '+(offer.reason||'—')
        +(offer.currentMainS?(' · aktuell '+hmsShort(offer.currentMainS)):'');
      $('progyes').disabled=true; $('progno').disabled=!offer.pending;
    }
  }).catch(e=>{if(m){m.className='msg err'; m.textContent=String(e);}});
}
$('progreload').onclick=()=>loadProgression();
$('progyes').onclick=()=>{
  const m=$('progmsg'); m.className='msg'; m.textContent='speichere…';
  fetch('/api/progression/accept?id=physio',{method:'POST'}).then(r=>r.json()).then(d=>{
    m.className='msg '+(d.ok?'ok':'err');
    m.textContent=d.ok?('Hauptteil jetzt '+hmsShort(d.mainS)):(d.error||'abgelehnt');
    loadProgression();
  }).catch(e=>{m.className='msg err'; m.textContent=String(e);});
};
$('progno').onclick=()=>{
  fetch('/api/progression/decline',{method:'POST'}).then(()=>loadProgression());
};

function validateWorkoutUi(showMsg){
  const body=$('wojson').value;
  const m=$('wojmsg');
  if(showMsg){ m.className='msg'; m.textContent='prüfe…'; }
  return fetch('/api/workout/validate',{method:'POST',headers:{'Content-Type':'application/json'},
    body:body}).then(r=>r.json()).then(d=>{
    if(showMsg){
      m.className='msg '+(d.ok?'ok':'err');
      m.textContent=d.ok
        ?('ok · '+d.name+' · '+d.steps+' Schritte · '+hmsShort(d.durationS)
          +(d.peakW!=null?(' · Spitze '+Math.round(d.peakW)+' W'):'')
          +(d.feasible===false?' · Warnungen':''))
        :(d.error||'ungueltig');
    }
    if(d.ok) showWorkoutPreview(d, d.id||_woSelId);
    return d;
  }).catch(e=>{
    if(showMsg){ m.className='msg err'; m.textContent=String(e); }
  });
}
$('woval').onclick=()=>validateWorkoutUi(true);
$('wojson').addEventListener('input',()=>{
  clearTimeout(_woDeb);
  _woDeb=setTimeout(()=>{ if($('wojson').value.trim().length>8) validateWorkoutUi(false); },450);
});
function readWoFile(f){
  if(!f) return;
  const r=new FileReader();
  r.onload=()=>{ $('wojson').value=String(r.result||''); editorFromJson($('wojson').value); validateWorkoutUi(true); };
  r.readAsText(f);
}
$('wofile').onchange=e=>readWoFile(e.target.files&&e.target.files[0]);
const drop=$('wodrop');
if(drop){
  drop.addEventListener('dragover',e=>{e.preventDefault(); drop.classList.add('set');});
  drop.addEventListener('dragleave',()=>drop.classList.remove('set'));
  drop.addEventListener('drop',e=>{
    e.preventDefault(); drop.classList.remove('set');
    const f=e.dataTransfer&&e.dataTransfer.files&&e.dataTransfer.files[0];
    readWoFile(f);
  });
}
$('woput').onclick=()=>{
  const m=$('wojmsg'); m.className='msg'; m.textContent='speichere…';
  fetch('/api/workout/put',{method:'POST',headers:{'Content-Type':'application/json'},
    body:$('wojson').value}).then(r=>r.json()).then(d=>{
    m.className='msg '+(d.ok?'ok':'err');
    m.textContent=d.ok?('gespeichert · '+d.id):(d.error||'Fehler');
    if(d.ok){ _woSelId=d.id; loadWorkouts(); validateWorkoutUi(false); }
  }).catch(e=>{m.className='msg err'; m.textContent=String(e);});
};

$('erggo').onclick=()=>{
  const w=+$('ergw').value||80;
  post('/api/control/power?watt='+w);
};
$('hrgo').onclick=()=>{
  const bpm=+$('hrbpm').value||130;
  post('/api/control/hr?bpm='+bpm);
};
$('lvlup').onclick=()=>step(+10);
$('lvldn').onclick=()=>step(-10);
function step(delta){
  fetch('/api/status').then(r=>r.json()).then(s=>{
    if(!s.profile){
      const m=$('cmsg'); m.className='msg err';
      m.textContent='Profil wählen (Reiter Profile)'; return;
    }
    const li=s.limiter||{};
    let cur=li.levelTenths;
    if(cur==null||cur<0) cur=(li.minLevelTenths||10)-delta;
    post('/api/control/level?tenths='+(cur+delta));
  });
}
function sweepStart(kind){
  const m=$('swmsg'); m.className='msg'; m.textContent='starte...';
  let q='';
  if(kind==='light') q='light=1';
  else if(kind==='coarse'||kind===true) q='coarse=1';
  fetch('/api/calib/sweep/start?'+q,{method:'POST'})
    .then(r=>r.json().then(j=>({ok:r.ok,j})))
    .then(o=>{m.className='msg '+(o.ok?'ok':'err');
      m.textContent=o.ok?(o.j.levels+' Stufen, etwa '+Math.round(o.j.estimateS/60)
        +' min — jetzt gleichmäßig '+Math.round(o.j.targetRpm)+' rpm treten'
        +(o.j.clipped?' (an Profilgrenze gekürzt)':''))
        :('Fehler: '+(o.j.error||'?'));})
    .catch(e=>{m.className='msg err';m.textContent=''+e});
}
$('sw1').onclick=()=>sweepStart('full');
$('sw2').onclick=()=>sweepStart('coarse');
$('sw2l').onclick=()=>sweepStart('light');
$('swx').onclick=()=>post('/api/calib/sweep/stop','swmsg');
$('rgon').onclick=()=>{
  const on=$('rgst').textContent!=='aus';
  post('/api/debug/ring?on='+(on?0:1),'rgmsg');
};
$('rgdl').onclick=()=>{location.href='/api/debug/export'};
$('rgx').onclick=()=>{
  if(!confirm('Mitschnitt und Steuer-Journal leeren?')) return;
  post('/api/debug/clear','rgmsg');
};
$('mpr').onclick=loadMap;
$('mpc').onclick=()=>{
  if(!confirm('Kennfläche verwerfen? Alle gemessenen Stützstellen sind dann weg.')) return;
  post('/api/calib/clear','mpmsg').then(()=>{mpPts=-1;loadMap()});
};
$('reboot').onclick=()=>post('/api/system/restart','smsg').then(()=>{
  $('smsg').textContent='Neustart läuft...'; setTimeout(()=>location.reload(),6000);});

const CFGF=[['deviceName','t'],['hubHost','t'],['hubPort','n'],['heartbeatIntervalS','n'],
  ['watchdogS','n'],['ntpServer','t'],['tz','t'],['enableHub','b'],['enableMdns','b'],
  ['enableNtp','b'],['autoConnect','b']];
function loadCfg(){
  fetch('/api/config/get').then(r=>r.json()).then(c=>{
    CFGF.forEach(f=>{const e=$('c-'+f[0]); if(!e) return;
      if(f[1]==='b') e.checked=!!c[f[0]]; else e.value=(c[f[0]]!=null?c[f[0]]:'');});
    $('gmsg').className='msg'; $('gmsg').textContent='';
  }).catch(()=>{});
}
$('save').onclick=()=>{
  const body={};
  CFGF.forEach(f=>{const e=$('c-'+f[0]); if(!e) return;
    body[f[0]]=(f[1]==='b')?e.checked:(f[1]==='n')?parseInt(e.value,10):e.value;});
  const m=$('gmsg'); m.className='msg'; m.textContent='speichere...';
  fetch('/api/config/save',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(body)}).then(r=>r.json().then(j=>({ok:r.ok,j})))
    .then(o=>{m.className='msg '+(o.ok?'ok':'err');
      m.textContent=o.ok?'gespeichert':('Fehler: '+(o.j.error||'?'));})
    .catch(e=>{m.className='msg err';m.textContent=''+e;});
};

$('go').disabled=true;
$('fw').onchange=()=>{
  const f=$('fw').files[0];
  $('go').disabled=!f;
  $('drop').classList.toggle('set',!!f);
  $('drop').innerHTML=f?('<b>'+f.name+'</b>'+(f.size/1024).toFixed(0)+' kB')
                       :('<b>Firmware-Datei wählen</b>ergo.&lt;version&gt;.esp32s3.bin');
};
$('otaf').onsubmit=(ev)=>{
  ev.preventDefault();
  const f=$('fw').files[0]; if(!f) return;
  const m=$('msg'); m.className='msg'; m.textContent='Lade '+f.name+' ('+(f.size/1024).toFixed(0)+' kB)...';
  $('go').disabled=true;
  const fd=new FormData(); fd.append('firmware',f);
  fetch('/ota-upload',{method:'POST',body:fd})
    .then(r=>r.text().then(t=>({ok:r.ok,t})))
    .then(r=>{m.className='msg '+(r.ok?'ok':'err');m.textContent=r.t;
      if(r.ok) setTimeout(()=>location.reload(),9000);})
    .catch(()=>{m.className='msg';
      m.textContent='Verbindung während des Flashens beendet - das ist normal. Neustart läuft.';
      setTimeout(()=>location.reload(),9000);});
};

function poll(){fetch('/api/status').then(r=>r.json()).then(render).catch(()=>{})}
tab((location.hash||'').replace('#','') || (location.pathname==='/ota'?'ota':'ride'));
window.onhashchange=()=>tab((location.hash||'').replace('#',''));
poll();
try{
  const es=new EventSource('/events');
  es.onmessage=e=>{try{render(JSON.parse(e.data))}catch(_){}};
  es.onerror=()=>{};
}catch(_){setInterval(poll,3000)}
</script>
</body>
</html>)HTML";
