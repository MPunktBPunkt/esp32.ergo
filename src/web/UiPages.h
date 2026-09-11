#pragma once

#include <Arduino.h>

/**
 * Die Reiterstruktur aus docs/ergometer/WEBINTERFACE.md §7, vollstaendig:
 * Ride, Workouts, Tests, Verlauf, Profile, Geraete, Kalibrierung, Debug,
 * Einstellungen, OTA.
 *
 * Sechs davon tragen Inhalt (Ride, Geraete, Kalibrierung, Debug, Einstellungen,
 * OTA), vier sind Platzhalter mit Zielversion. Das ist Absicht: die Navigation ist die
 * eine Entscheidung, die man nicht zweimal treffen will, und ein Reiter, der
 * spaeter dazukommt, soll ein geloeschtes Flag sein und kein Umbau. Der Nutzer
 * sieht ausserdem, was geplant ist, statt es zu erraten.
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
 * Nicht umgesetzt und bewusst nicht erfunden: Zonenschiene, Hero-Zonenfarbe
 * und der Kadenz-Hinweis. Alle drei brauchen Profile beziehungsweise einen
 * laufenden Regler; eine Zielspanne ohne Regler waere eine ausgedachte Zahl.
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
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);
  font-family:'IBM Plex Mono',ui-monospace,SFMono-Regular,Menlo,monospace;
  font-size:15px;line-height:1.5;padding:24px}
.wrap{max-width:820px;margin:0 auto}
header{display:flex;align-items:baseline;gap:14px;margin-bottom:4px}
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
.v.hero{font-size:46px;font-weight:800;line-height:1.1}
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
footer{color:var(--dim);font-size:12px;text-align:center;margin-top:26px}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1>ERGO</h1>
    <span class="badge" id="ver">-</span>
  </header>
  <div class="sub" id="sub">verbinde...</div>
  <nav id="nav"></nav>

  <section id="t-ride">
    <div class="card">
      <h2>Verbindung</h2>
      <div class="grid">
        <div><div class="k">Bike</div>
          <div class="v"><span class="dot j-bdot"></span><span class="j-bike">-</span></div>
          <div class="k j-bsub"></div></div>
        <div><div class="k">Pulsgurt</div>
          <div class="v"><span class="dot j-hdot"></span><span class="j-strap">-</span></div>
          <div class="k j-hsub"></div></div>
      </div>
    </div>

    <div class="card">
      <h2>Messwerte</h2>
      <div class="msg warn" id="startHint" hidden>Nach STOP: ggf. <b>Start</b> drücken und
        erneut treten — die Freigabe am Bike kann sonst fehlen.</div>
      <div class="grid">
        <div id="pwtile"><div class="k">Leistung</div><div class="v" id="pw">-</div>
          <div class="k" id="pwsub"></div></div>
        <div><div class="k">Kadenz</div><div class="v big" id="cad">-</div>
          <div class="k" id="cadsub"></div></div>
        <div id="hrtile"><div class="k">Puls</div><div class="v" id="hrv">-</div>
          <div class="k" id="hrvsub"></div>
          <div class="capbar" id="hrcap" hidden><i id="hrcapi"></i>
            <span class="mark" id="hrsoftm"></span></div>
          <div class="k" id="hrcapsub"></div></div>
        <div class="tile" id="ltile"><div class="k">Stufe</div>
          <div class="v big" id="lvl">-</div>
          <div class="segs" id="segs"></div>
          <div class="k" id="lvlsub"></div></div>
        <div><div class="k">Geschwindigkeit</div><div class="v" id="spd">-</div></div>
        <div><div class="k">Strecke</div><div class="v" id="dst">-</div></div>
        <div><div class="k">Energie</div><div class="v" id="kcal">-</div></div>
        <div><div class="k">Fahrzeit</div><div class="v" id="el">-</div></div>
      </div>
      <div class="reg" id="regstrip">
        <div class="k">Ist / Ziel</div>
        <div class="v" id="regline">-</div>
        <canvas id="regcv" width="640" height="48" aria-label="Ist gegen Ziel"></canvas>
        <div class="bar" id="rehaprogress" hidden><i id="rehaprogi"></i></div>
        <div class="k" id="rehaprogsub"></div>
      </div>
    </div>

    <div class="card">
      <h2>Steuerung</h2>
      <div class="grid">
        <div><div class="k">Profil</div><div class="v" id="rprof">-</div>
          <div class="k" id="rprofsub"></div></div>
        <div><div class="k">Modus</div><div class="v" id="rmode">-</div>
          <div class="k" id="rmodesub"></div></div>
      </div>
      <div class="row flat">
        <button id="moff" class="ghost">OFF</button>
        <button id="mlvl" class="ghost">LEVEL</button>
        <button id="merg" class="ghost">ERG</button>
        <button id="mhr" class="ghost">HR</button>
        <button id="mreha" class="ghost">REHA</button>
        <button id="panic" class="danger">STOP</button>
      </div>
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
        <button id="lvldn" class="ghost">Stufe -1</button>
        <button id="lvlup" class="ghost">Stufe +1</button>
      </div>
      <div class="msg" id="cmsg"></div>
      <div class="hint flat">Ohne aktives Profil keine Last. ERG/HR/REHA brauchen eine
        Kennfläche. REHA: festes Watt mit Pulsdeckel. Nach STOP ggf. Start + Tritt.</div>
    </div>
  </section>

  <section id="t-profile">
    <div class="card">
      <h2>Profile</h2>
      <div class="hint flat">Kein stilles Standardprofil. Vor dem Training eines
        wählen — Wechsel nur im Modus OFF. Grenzen gelten sofort für den Limiter.</div>
      <table id="plist"></table>
      <div class="k" id="plnone">noch keine Profile</div>
      <div class="row tight"><button id="pclr" class="ghost sm">Auswahl aufheben</button>
        <button id="pnew" class="ghost sm">Neu</button>
        <button id="prld" class="ghost sm">Neu laden</button></div>
      <div class="msg" id="pmsg"></div>
    </div>
    <div class="card">
      <h2 id="pfh">Profil bearbeiten</h2>
      <div class="grid">
        <label class="f"><div class="k">ID</div>
          <input type="text" id="pf-id" maxlength="15" autocomplete="off"></label>
        <label class="f"><div class="k">Name</div>
          <input type="text" id="pf-name" maxlength="23" autocomplete="off"></label>
        <label class="f"><div class="k">FTP (W)</div>
          <input type="number" id="pf-ftp" min="0" max="600"></label>
        <label class="f"><div class="k">HRmax</div>
          <input type="number" id="pf-hrmax" min="0" max="190"></label>
        <label class="f"><div class="k">max. Stufe</div>
          <input type="number" id="pf-maxlvl" min="0" max="16" step="0.1"></label>
        <label class="f"><div class="k">max. Watt</div>
          <input type="number" id="pf-maxw" min="0" max="500"></label>
        <label class="f"><div class="k">max. Puls</div>
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
      <div class="row tight">
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
 ['workouts','Workouts','v0.2','Bibliothek, Editor mit Live-Vorschau und Machbarkeitsprüfung, Import und Export.'],
 ['tests','Tests','v0.2','Geführte Tests: Rampe, 20 Minuten, Recovery. Ergebnis wird vorgeschlagen, nie automatisch übernommen.'],
 ['verlauf','Verlauf','v0.2','Sessions je Profil, Zonenverteilung, Physio-Progression, Ghost-Vergleich.'],
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
function drawRegChart(){
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
  stroke(istHist,'#E2802F',[]);
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
  const loadModes={MANUAL_LEVEL:1,MANUAL_ERG:1,HR_HOLD:1,REHA:1};
  if(loadModes[prevMode] && mode==='OFF') afterStop=true;
  if(mode!=='OFF') afterStop=false;
  prevMode=mode;
  const sh=$('startHint');
  if(sh){
    const show=afterStop && !!s.bikeLink && mode==='OFF';
    sh.hidden=!show;
  }

  const ceil=!!(erg.ceiling && (mode==='MANUAL_ERG'||mode==='REHA'||mode==='HR_HOLD'));
  $('pw').className='v'+(leadHr?'':(' hero'+(ceil?' ceil':'')));
  $('hrv').className='v'+(leadHr?' hero':' big');
  $('pw').textContent=live?num(d.powerW,0,' W'):'-';
  let pwsub='';
  if(live){
    if(mode==='MANUAL_ERG' && erg.targetW){
      pwsub='Ist · Ziel '+Math.round(erg.targetW)+' W'+(ceil?' · unerreichbar':'');
    } else if(mode==='REHA' && rh.desiredW!=null){
      pwsub='Soll '+Math.round(rh.desiredW)+' W'
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
  const showCap=mode==='REHA' || (leadHr && pi && pi.maxHr);
  if(hrCapEl){
    hrCapEl.hidden=!showCap;
    if(showCap){
      const hard=mode==='REHA'?(rh.hrMax||120):(pi.maxHr||120);
      const soft=mode==='REHA'?(rh.hrSoft||(hard-5)):Math.max(40,hard-5);
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

  // Ist/Ziel-Kurve
  let ziel=0, ist=live?(d.powerW||0):0;
  if(mode==='MANUAL_ERG') ziel=erg.targetW||0;
  else if(mode==='REHA') ziel=rh.capActive?(rh.effectiveW||0):(rh.desiredW||0);
  else if(mode==='HR_HOLD') ziel=hh.powerTargetW||0;
  if(mode==='MANUAL_ERG'||mode==='REHA'||mode==='HR_HOLD'){
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
  drawRegChart();

  const rp=$('rehaprogress'), rpi=$('rehaprogi'), rps=$('rehaprogsub');
  if(rp){
    const timed=mode==='REHA' && rh.durationS>0;
    rp.hidden=!timed;
    if(timed){
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
    ?('max Stufe '+lvDisp(pi&&pi.maxLevelTenths)+' · max '+(pi&&pi.maxPowerW||'-')+' W')
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
  $('rmodesub').textContent=msub;
  $('moff').classList.toggle('ghost', mode!=='OFF');
  $('mlvl').classList.toggle('ghost', mode!=='MANUAL_LEVEL');
  $('merg').classList.toggle('ghost', mode!=='MANUAL_ERG');
  $('mhr').classList.toggle('ghost', mode!=='HR_HOLD');
  $('mreha').classList.toggle('ghost', mode!=='REHA');
  const ergLike=mode==='MANUAL_ERG'||mode==='HR_HOLD'||mode==='REHA';
  $('lvlup').disabled=!hasP||ergLike;
  $('lvldn').disabled=!hasP||ergLike;
  $('mlvl').disabled=!hasP;
  $('merg').disabled=!hasP||!(erg.mapReady);
  $('mhr').disabled=!hasP||!(erg.mapReady);
  $('mreha').disabled=!hasP||!(erg.mapReady);
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
let _plist=[];
function fillProfileForm(p){
  const e=!p;
  $('pfh').textContent=e?'Neues Profil':('Profil: '+(p.name||p.id));
  $('pf-id').value=p?p.id:'';
  $('pf-id').disabled=!!p;
  $('pf-name').value=p?p.name:'';
  $('pf-ftp').value=p&&p.ftpW?p.ftpW:'';
  $('pf-hrmax').value=p&&p.hrMax?p.hrMax:'';
  $('pf-maxlvl').value=p&&p.maxLevelTenths? (p.maxLevelTenths/10).toFixed(1):'';
  $('pf-maxw').value=p&&p.maxPowerW?p.maxPowerW:'';
  $('pf-maxhr').value=p&&p.maxHr?p.maxHr:'';
  $('pf-cad').value=p&&p.targetCadenceRpm?p.targetCadenceRpm:'';
  $('pf-lead').value=(p&&p.leadingZone)||'power';
  $('pf-loss').value=(p&&p.onHrLoss)||'reduce';
  $('pfdel').disabled=!p;
  $('pfmsg').textContent='';
}
function loadProfiles(){
  fetch('/api/profile/list').then(r=>r.json()).then(j=>{
    const t=$('plist'); t.innerHTML='';
    const act=j.active||null;
    _plist=j.profiles||[];
    $('plnone').style.display=_plist.length?'none':'';
    _plist.forEach(p=>{
      const tr=document.createElement('tr');
      const on=act&&act===p.id;
      tr.innerHTML='<td><span class="dot'+(on?' on':'')+'"></span><b>'+(p.name||p.id)+'</b>'
        +(on?' <span class="tag">aktiv</span>':'')
        +'<br><span class="k">'+p.id
        +' · FTP '+(p.ftpW||'-')+' W · max Stufe '+lvDisp(p.maxLevelTenths)
        +' · max '+(p.maxPowerW||'-')+' W · max HR '+(p.maxHr||'-')+'</span></td>'
        +'<td class="r"></td>';
      const td=tr.querySelector('td.r');
      const be=document.createElement('button');
      be.className='ghost sm'; be.textContent='Bearbeiten';
      be.onclick=ev=>{ev.stopPropagation(); fillProfileForm(p);};
      td.appendChild(be);
      if(!on){
        const b=document.createElement('button');
        b.className='ghost sm'; b.textContent='Wählen';
        b.onclick=ev=>{ev.stopPropagation(); selectProfile(p.id);};
        td.appendChild(b);
      }
      t.appendChild(tr);
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
  const body={id:id,name:name,
    ftpW:+$('pf-ftp').value||0, hrMax:+$('pf-hrmax').value||0,
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
