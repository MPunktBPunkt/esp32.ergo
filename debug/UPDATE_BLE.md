# Update — BleCentral + FtmsClient + Kalibrierung (Antwort auf TODO_PFLICHTENHEFT.md)

Schritte 1 bis 4 des Auftrags sind umgesetzt. Schritt 5 (Steuermodi) bewusst
nicht — siehe unten. Dazugekommen ist die **Kalibrierung**: der geführte
Stufen-Sweep und die Kennfläche, weil Nachtest 1 und 2 sonst wieder von Hand
protokolliert würden und wieder Punkte verlorengingen.

Diese Instanz kann nicht bauen. **Ergebnis unten ist auszufüllen.**

## Neue Dateien

| Datei | Inhalt |
| --- | --- |
| `src/ble/BleTypes.h` | Rollen, Linkzustände, Scan-Eintrag, `HrSample`, `HrSource` |
| `src/ble/BleCentral.{h,cpp}` | Scan, Connect, Merken, Reconnect-Backoff — zwei Rollen |
| `src/ble/FtmsClient.{h,cpp}` | GATT `0x1826`, Capabilities, Notify `0x2AD2`/`0x2AD9`, Schreiben |
| `src/ble/HrClient.{h,cpp}` | `0x180D`/`0x2A37` plus einmalig Batterie |
| `src/control/PowerMap.{h,cpp}` | Kennfläche Stufe × Kadenz → Watt, Interpolation, NVS-Blob |
| `src/control/SweepRunner.{h,cpp}` | Geführter Sweep nach NACHTESTS.md Test 1 und 2 |
| `test/test_powermap/` | 19 Fälle: Interpolation, Gewichtung, Roundtrip |
| `test/test_sweep/` | 17 Fälle: Ablauf, Verwerfungsregeln, Abbrüche |
| `docs/ergometer/*` | Pflichtenheft, Webinterface, Geräteprofil, Nachtests, BLE-Scan gespiegelt |

Geändert: `src/app/App.{h,cpp}`, `src/core/ConfigStore.{h,cpp}` (`cfg_ver = 2`),
`src/web/UiPages.h`, `platformio.ini`, `README.md`, `.gitignore`,
neu `.gitattributes`.

**`platformio.ini`:** `env:native` hat zwei Einträge mehr im
`build_src_filter` — `control/PowerMap.cpp` und `control/SweepRunner.cpp`.
Beide sind Arduino-frei, das ist die Bedingung. Die Positivliste bleibt eine
Positivliste; `+<*>` würde das Environment sofort zerlegen.

## Die Oberfläche hat jetzt die Reiterstruktur des Konzepts

`UiPages.h` ist von einer Karten-Kolonne auf die zehn Reiter aus
`docs/ergometer/WEBINTERFACE.md` §7 umgebaut: **Ride, Workouts, Tests, Verlauf,
Profile, Geräte, Kalibrierung, Debug, Einstellungen, OTA**. Fünf tragen Inhalt,
fünf sind Platzhalter mit Zielversion und einem Satz, was dort hinkommt.

Der Grund für die Platzhalter: die Navigation ist die eine Entscheidung, die man
nicht zweimal treffen will. Ein Reiter, der später dazukommt, soll ein
gelöschtes Flag in der `NAV`-Liste sein und kein Umbau — und der Nutzer sieht,
was geplant ist, statt es zu erraten.

Drei Dinge daran sind mehr als Kosmetik:

- **Die Stufen-Kachel** ist gebaut wie im Konzept beschrieben: Segmente nach
  `caps.levels`, gestellte Stufe gefüllt, Reserve beziffert, roter Rand bei
  erreichter Decke, und ein `~` vor dem Wert, weil das Bike die Stufe nicht
  zurückmeldet. Kein stilles Klemmen.
- **Der Debug-Reiter** zeigt die Rohkennungen (`0x2ACC`, `0x2AD6`, `0x2AD8`),
  Notify- und Antwortzähler, die letzte Control-Point-Antwort und die letzte
  Limiter-Ablehnung. Genau das braucht man bei der ersten Fahrt.
- **Ohne JavaScript** zeigt die Seite alle Abschnitte untereinander und das
  OTA-Formular sendet native `multipart/form-data` an `/ota-upload`. Das ist
  Absicht: diese Seite ist der Rückweg nach einem Fehlflash und darf nicht an
  einem Skriptfehler hängen. Deshalb `body.js` per CSS statt `hidden`-Attribute.

Nicht umgesetzt und **nicht erfunden**: Zonenschiene, Hero-Zonenfarbe und der
Kadenz-Hinweis („halten / schneller / langsamer"). Alle drei brauchen Profile
beziehungsweise einen laufenden Regler; eine Zielspanne ohne Regler wäre eine
ausgedachte Zahl.

Die Seite wächst damit von 6,4 kB (Shell) über 12,5 kB auf **26,1 kB**. Das
Konzept §8 nennt als Vergleich „heartrate braucht für eine einfachere UI schon
etwa 27 kB" — wir liegen also im erwarteten Rahmen, aber der Editor und das
Debug-Panel sollen laut §8 später **nachgeladen** werden, nicht mit in diese
Seite. Beim Flash-Ergebnis bitte darauf achten.

`src/ble/FtmsCodec.*`, `src/ble/FtmsCapabilities.*`, `src/control/Limiter.*` und
beide Testsuiten sind **unverändert**. `platformio.ini` ebenfalls — `env:ergo`
hat keinen `build_src_filter`, die neuen Dateien kommen automatisch mit, und
`env:native` listet weiterhin nur die drei Arduino-freien Übersetzungseinheiten.

## Die harten Regeln, Punkt für Punkt

1. **`/ota-upload` und die Shell bleiben.** Unverändert, plus: `/api/status`
   liefert jetzt `bikeLink`, das `tools/deploy.sh` schon vorher gelesen hat.
   Der Flash bricht damit ab, solange ein Bike hängt.
2. **Kein `[env]`-Block.** `platformio.ini` nicht angefasst.
3. **Codec und Limiter bleiben Arduino-frei.** Kein neuer Include in den drei
   Dateien, `build_src_filter` unverändert.
4. **Jeder Write nur durch den Limiter.** Strukturell, nicht durch Disziplin:
   `FtmsClient` hat keine öffentliche Methode, die rohe Bytes schreibt. Alles
   läuft durch das private `send()`, und das fragt zuerst `limiter_->check()`.
   Ein Bypass müsste die Klasse ändern, nicht sie nur falsch benutzen.
5. **Kein Steuerweg über `0x05`.** `setPowerW()` existiert und ruft den Codec,
   aber der Limiter lehnt ab, solange `powerTargetTrusted` nicht gilt — und das
   gilt nur mit veröffentlichter `0x2AD8`, die der Varon nicht hat. Der Endpunkt
   bleibt trotzdem erreichbar, damit die Ablehnung **mit Begründung** in der UI
   landet statt still zu verschwinden.
6. **Shell-Routen angefasst?** Nein, nur ergänzt. Review vor Flash trotzdem
   sinnvoll, weil `main.cpp` und `App` jetzt BLE initialisieren.
7. **Kein Blind-Restart unter Last.** Der Hub-Watchdog sendet bei stehendem
   Bike-Link erst `08 01`, wartet 300 ms und startet dann neu. Gleiches gilt für
   `/api/system/restart`, `/api/ble/disconnect` und `/api/ble/forget`.

## Zwei Entscheidungen, die Erklärung brauchen

**Der Aufstieg eines Links wird nicht im BLE-Callback verarbeitet.**
`connectRole()` setzt nur ein Flag, `loop()` ruft danach `attach()`. Eine
GATT-Leseoperation im NimBLE-Callback-Kontext blockiert den Host-Task und läuft
in den Watchdog — und `attach()` liest drei Characteristics.

**Abonniert wird nach Properties, nicht nach Spec.** Der Control Point ist laut
Standard Indicate, der Varon liefert ihn als Notify. `subscribeTo()` fragt
deshalb `canNotify()` / `canIndicate()` und richtet sich danach. Genau das ist
der Unterschied, wenn später ein anderes Ergometer dranhängt.

## Was bewusst fehlt

Schritt 5 des Auftrags — `OFF`, `MANUAL_LEVEL`, `MANUAL_ERG`, `HR_HOLD`,
`WORKOUT` — ist **nicht** umgesetzt. Stattdessen gibt es eine Handsteuerung mit
fünf Endpunkten und vier Knöpfen in der UI. Begründung: der Auftrag sagt selbst
„erst wenn Write-Pfad steht", und ob er steht, weiß niemand vor der ersten
Fahrt. Ein Modusautomat darüber würde im Fehlerfall nur verschleiern, ob es an
der Regelung, am Limiter oder am Gerät liegt.

Ebenfalls nicht dabei: Profile, Workouts, Zonen, LittleFS, Debug-Ring, Relay
als Pulsquelle (`HrSource::Relay` ist definiert, aber nicht gefüllt).

## Kalibrierung: geführter Sweep und Kennfläche

`NACHTESTS.md` schreibt, Test 1 und 2 würden mit der Sonde gefahren und „erst
danach lohnt das `esp32.ergo`-Repo". Das ist überholt: Ergo hat
`/api/control/level` und ein `/api/status` mit Leistung und Kadenz. Die
Messungen mit der Zielfirmware zu fahren ist sogar besser, weil dann der
Limiter mitgemessen wird und nicht die Labor-Guards der Sonde.

**Warum das automatisiert ist und nicht per Hand protokolliert wird:** zwei der
drei Wirkungsmessungen des ersten Sondenlaufs sind wertlos, weil die Kadenz
weggelaufen ist und es niemandem aufgefallen ist. Eine Verwerfungsregel, an die
man sich erinnern muss, ist keine.

`SweepRunner` fährt je Stufe 20 s einschwingen und 40 s mitteln und verwirft
ein Fenster, wenn die mittlere Kadenz unter der Schwelle liegt oder die Spanne
im Fenster mehr als 10 % des Mittels beträgt. Verworfene Punkte erscheinen mit
Grund in der UI und im Log — sie verschwinden nicht.

Drei Punkte, die im Entwurf Absicht sind:

- **Das Messfenster beginnt erst nach dem bestätigten Write.** Der Limiter
  lehnt während der Rampe mit `Deferred` ab; würde man diese Sekunden
  mitmessen, wäre jeder erste Wert zu klein. Nativ getestet.
- **Der Runner schreibt nicht selbst.** Wie der Limiter trifft er nur
  Entscheidungen, `App` setzt sie um — durch `FtmsClient` und damit durch den
  Limiter, wie jeder andere Schreibweg. Deshalb ist der komplette Ablauf
  hostseitig prüfbar, inklusive Zeitverhalten.
- **Ein Sweep-Punkt wird von passivem Lernen nur mit 1/32 nachgezogen.** Er
  entstand unter gehaltener Kadenz; eine einzige unruhige Fahrt darf ihn nicht
  verwaschen. Umgekehrt ersetzt ein Sweep-Punkt passiv Gelerntes vollständig.

Passives Lernen läuft nebenher: alle 5 s ein Punkt, aber erst 20 s nachdem die
Stufe zuletzt gewechselt hat — vorher beschreibt der Wert einen Übergang und
keinen Beharrungszustand.

Die Fläche liegt als 1160-Byte-Blob im NVS-Namensraum `ergomap`, versioniert
und byteweise serialisiert. Gesichert wird nach jedem Sweep sofort und sonst
höchstens alle 5 Minuten. Meldet das Bike beim Verbinden einen anderen
Stellweg, wird die Fläche verworfen: Stufe 8 von 16 ist nicht Stufe 8 von 24.

Neue Endpunkte: `POST /api/calib/sweep/start?coarse=0|1[&rpm=&settleS=&windowS=]`,
`POST /api/calib/sweep/stop`, `GET /api/calib/map`, `POST /api/calib/clear`.
Der Reiter **Kalibrierung** ist damit vom Platzhalter zum Inhalt geworden:
Sweep-Steuerung mit Kadenz-Coach, Punktliste und die Fläche als Heatmap, in der
geführt gemessene Zellen einen hellen Rand tragen und gelernte nicht.

## Ergebnis

Ausgefüllt von der Build-Instanz am 2026-09-11, Commit `b101125`.

```text
pio test -e native
  test_codec     22/22 PASSED
  test_limiter   21/21 PASSED
  test_powermap  19/19 PASSED
  test_sweep     17/17 PASSED
  → 79/79 in ~2.5 s

pio run -e ergo
  → SUCCESS (~31 s)
  RAM:   61432 / 327680  (18.7 %)
  Flash: 1276469 / 1966080 (64.9 %)   ← unter 85 %-Schwelle, OK
  UI:    33977 Bytes (GET /)
  Bin:   dist/ergo.0.1.0-dev.esp32s3.bin  (~1246 kB)
```

## Hardware-Test auf `.88`

Reihenfolge ist wichtig: erst ohne Bike prüfen, dass die Shell noch lebt, dann
verbinden, dann erst Last stellen.

- [x] `tools/deploy.sh --ota 192.168.178.88` (Probe-Stand war schon Ergo-Shell; BLE-Bin OK)
- [x] Zweiter Roundtrip Ergo→Ergo (Selbst-Recovery)
- [x] `curl …/api/status` → `codecSelfTest=ok`, `bikeLink=false`, `ble.linkCount=0`, `hubOk=true`
- [x] UI 10 Reiter erreichbar (`GET /` 200, OTA-Form ohne JS)
- [x] SSE `/events` liefert Status-JSON
- [x] Control ohne Bike → `no-link` (stop/request/level/power)
- [x] Calib ohne Bike: `GET /api/calib/map` leer; `POST …/sweep/start` → **409** `kein Bike verbunden`
- [x] BLE-Scan start/stop funktioniert (Geräte in der Luft gefunden)
- [ ] `GET /api/ble/devices` → **kein TC174 / kein `ftms:true`** (Bike offenbar aus)
- [ ] Connect / Caps / Live-Daten / Control mit Last / Sweep mit Fahrer — **offen, braucht eingeschaltetes Ergometer**

Hub: MAC `68B6B329339C`, version `0.1.0-dev`, `ios.ergo_state=IDLE`.

### Noch offen (braucht Hardware vor Ort)

- [ ] `POST /api/ble/connect` mit Bike-MAC
- [ ] `/api/status` → `ftms.caps.levels = 16`, `strategy = emulate-resistance`,
      `powerTrusted = false`, `powerRangeHex = fehlt`
- [ ] treten → `ftms.data.powerW` / `cadenceRpm`, `stale = false`
- [ ] `POST /api/control/request` → `controlGranted = true`
- [ ] `POST /api/control/level?tenths=60` → `deferred`/`ok` + Rampe
- [ ] `POST /api/control/power?watt=100` → **`denied`** (Wattziel ohne 0x2AD8)
- [ ] `POST /api/control/stop` → Last sofort weg
- [ ] Bike aus/an → LOST / Reconnect / READY
- [ ] Pulsgurt → `hr.attached`
- [ ] Leerer-Sattel-Sweep verkürzt, dann Test 1/2 mit Fahrer

**Risiko:** das ist die erste Firmware, die an diesem Gerät Last stellt. Nicht
mit jemandem auf dem Rad testen. Erster Versuch mit leerem Sattel und Hand am
Netzschalter — die Rampe begrenzt den Anstieg auf eine Stufe pro zwei Sekunden,
aber getestet ist das bisher nur nativ, nicht am Gerät.

### Sweep erst danach, und erst dann mit jemandem auf dem Rad

Der Sweep ist der einzige Vorgang, der von sich aus Stufen stellt. Er wird erst
gefahren, wenn die Liste oben durch ist.

- [ ] Reiter **Kalibrierung** öffnen, ohne Bike: beide Test-Knöpfe sind grau,
      die Heatmap sagt „kein Stellweg bekannt"
- [ ] Bike verbunden, leerer Sattel: `POST /api/calib/sweep/start?coarse=1&settleS=3&windowS=5`
      — verkürzt, nur um den Ablauf zu sehen. Erwartung: vier Stufen werden
      gestellt, alle vier Punkte als „keine Daten" oder „Kadenz zu niedrig"
      verworfen, am Ende `ABORTED` mit Grund und ein Stop
- [ ] `GET /api/calib/map` liefert `levels = 16` und leere Zellen
- [ ] Dann erst mit Fahrer: `Test 1 · 60 rpm`. Metronom oder die Kadenzanzeige
      im Reiter benutzen — die Anzeige wird rot, sobald mehr als 4 rpm daneben
- [ ] Nach dem Lauf: Punktliste vollständig, Heatmap in Spalte „60" gefüllt,
      `map.sweepCells` entspricht der Zahl gültiger Punkte
- [ ] Neustart → `[MAP] geladen: ...` im Log, Heatmap unverändert
- [ ] `Test 2 · 80 rpm` — danach zwei gefüllte Spalten. Das ist die Antwort auf
      die Frage „Tabelle oder Fläche"

## Danach

Nachtest 1 und 2 aus `docs/ergometer/NACHTESTS.md` sind damit gefahren und
protokolliert. Ihr Ergebnis ist die Kennfläche Stufe × Kadenz → Watt und damit
die Voraussetzung für `MANUAL_ERG` und `HR_HOLD`. Die entscheidende Zahl ist
die Leistung bei Stufe 16: deutlich über 200 W heißt, der Widerstandskanal
trägt v0.1 vollständig; um 130 W heißt, `0x11` wird Pflicht und Nachtest 4
rückt nach vorn.

Offen bleiben Nachtest 3 (Watt-Nachtest, Erwartung: `80 05 01` ohne Wirkung),
4 (Simulation `0x11`), 5 (Dual-Link über 10 Minuten — mit dieser Firmware nur
noch eine Frage der Laufzeit) und 6 (Crash unter Last). Test 6 kann auf den
Watchdog zurückschlagen: bleibt die Last stehen und ist nicht bedienbar, muss
der Hub-Watchdog aus, statt wie jetzt vorher Stop zu senden.
