#pragma once

#include <Arduino.h>

/**
 * Minimale Shell-Oberflaeche: Status und OTA, mehr nicht.
 *
 * Bewusst klein gehalten - die eigentliche Trainingsoberflaeche kommt spaeter
 * und ist in docs/ergometer/WEBINTERFACE.md beschrieben. Das Farbsystem hier
 * ist schon das von dort, damit die Shell nicht wie ein Fremdkoerper wirkt.
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
.wrap{max-width:760px;margin:0 auto}
header{display:flex;align-items:baseline;gap:14px;margin-bottom:4px}
h1{font-family:'Syne',system-ui,sans-serif;font-size:30px;letter-spacing:.14em;
  margin:0;font-weight:800}
.badge{font-size:12px;color:var(--accent);border:1px solid var(--accent);
  border-radius:999px;padding:2px 10px}
.sub{color:var(--dim);font-size:13px;margin-bottom:22px}
.card{background:var(--card);border:1px solid var(--edge);border-radius:12px;
  padding:18px 20px;margin-bottom:16px}
.card h2{font-family:'Syne',system-ui,sans-serif;font-size:13px;letter-spacing:.18em;
  text-transform:uppercase;color:var(--dim);margin:0 0 14px;font-weight:700}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:14px}
.k{color:var(--dim);font-size:11px;letter-spacing:.09em;text-transform:uppercase}
.v{font-size:17px;font-variant-numeric:tabular-nums;word-break:break-all}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:7px;
  vertical-align:middle;background:var(--bad)}
.dot.on{background:var(--ok)}
input[type=file]{position:absolute;width:1px;height:1px;opacity:0;pointer-events:none}
.drop{display:block;padding:22px;margin-bottom:16px;text-align:center;cursor:pointer;
  background:#0E1116;border:1px dashed var(--edge);border-radius:10px;color:var(--dim)}
.drop:hover{border-color:var(--accent);color:var(--fg)}
.drop b{display:block;color:var(--fg);font-weight:400;margin-bottom:4px}
.drop.set{border-style:solid;border-color:var(--accent)}
button{font:inherit;background:var(--accent);color:#160B02;border:0;border-radius:10px;
  padding:13px 26px;font-weight:700;cursor:pointer;min-height:44px}
button:disabled{opacity:.45;cursor:default}
.msg{margin-top:12px;color:var(--dim);font-size:13px;min-height:19px}
.msg.err{color:var(--bad)} .msg.ok{color:var(--ok)}
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

  <div class="card">
    <h2>Status</h2>
    <div class="grid">
      <div><div class="k">Name</div><div class="v" id="name">-</div></div>
      <div><div class="k">IP</div><div class="v" id="ip">-</div></div>
      <div><div class="k">MAC</div><div class="v" id="mac">-</div></div>
      <div><div class="k">Board</div><div class="v" id="board">-</div></div>
      <div><div class="k">WLAN</div><div class="v" id="rssi">-</div></div>
      <div><div class="k">Laufzeit</div><div class="v" id="up">-</div></div>
      <div><div class="k">Heap frei</div><div class="v" id="heap">-</div></div>
      <div><div class="k">Hub</div><div class="v"><span class="dot" id="hubdot"></span><span id="hub">-</span></div></div>
      <div><div class="k">Codec-Selbsttest</div><div class="v"><span class="dot" id="cdot"></span><span id="codec">-</span></div></div>
    </div>
  </div>

  <div class="card">
    <h2>Firmware aktualisieren</h2>
    <label class="drop" for="fw" id="drop">
      <b>Firmware-Datei waehlen</b>
      ergo.&lt;version&gt;.esp32s3.bin
    </label>
    <input type="file" id="fw" accept=".bin">
    <button id="go" disabled>Hochladen und neu starten</button>
    <div class="msg" id="msg"></div>
  </div>

  <footer>esp32.ergo &middot; Trainingsrechner Hammer Varon XTR II</footer>
</div>
<script>
const $=i=>document.getElementById(i);
function dot(el,ok){el.classList.toggle('on',!!ok)}
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
}
function poll(){fetch('/api/status').then(r=>r.json()).then(render).catch(()=>{})}
poll();
try{
  const es=new EventSource('/events');
  es.onmessage=e=>{try{render(JSON.parse(e.data))}catch(_){}};
  es.onerror=()=>{};
}catch(_){setInterval(poll,3000)}

$('fw').onchange=()=>{
  const f=$('fw').files[0];
  $('go').disabled=!f;
  $('drop').classList.toggle('set',!!f);
  $('drop').innerHTML=f?('<b>'+f.name+'</b>'+(f.size/1024).toFixed(0)+' kB')
                       :('<b>Firmware-Datei waehlen</b>ergo.&lt;version&gt;.esp32s3.bin');
};
$('go').onclick=()=>{
  const f=$('fw').files[0]; if(!f) return;
  const m=$('msg'); m.className='msg'; m.textContent='Lade '+f.name+' ('+(f.size/1024).toFixed(0)+' kB)...';
  $('go').disabled=true;
  const fd=new FormData(); fd.append('firmware',f);
  fetch('/ota-upload',{method:'POST',body:fd})
    .then(r=>r.text().then(t=>({ok:r.ok,t})))
    .then(r=>{m.className='msg '+(r.ok?'ok':'err');m.textContent=r.t;
      if(r.ok) setTimeout(()=>location.reload(),9000);})
    .catch(()=>{m.className='msg';
      m.textContent='Verbindung waehrend des Flashens beendet - das ist normal. Neustart laeuft.';
      setTimeout(()=>location.reload(),9000);});
};
</script>
</body>
</html>)HTML";
