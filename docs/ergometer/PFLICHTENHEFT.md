# Pflichtenheft `esp32.ergo`

Eigenständiger ESP32-Knoten: **Ergometer-Steuerung und Trainingscomputer** für das
HAMMER Varon XTR II (SKU 10006, BLE-Name `TC174`) über BLE, mit lokaler WebUI und
ESP-Hub-Anbindung.

Baut auf dem Muster von `esp32.heartrate` auf (PlatformIO, `HubClient`, WiFiManager,
ConfigStore, SSE-WebUI, Web-OTA, Hub-OTA). NimBLE als GATT Central — in v0.2
zusätzlich als Peripheral.

Arbeitsname: `esp32.ergo`

**Stand 2026-09-11, Revision 4.** Neu in dieser Revision: Profile für mehrere
Nutzer, Leistungsziel mit Pulsdeckel als Reha-Programm, geführte Tests,
Workout-Editor und ein eigenes Designkonzept für die WebUI unter
[WEBINTERFACE.md](WEBINTERFACE.md).

Drei Erkenntnisse aus Revision 3 tragen die Architektur:

1. Der Laborlauf mit [`esp32.ftmsprobe`](https://github.com/MPunktBPunkt/esp32.ftmsprobe)
   hat FTMS bestätigt, aber das Bike kennt **kein Set Target Power**. Alles
   Watt-basierte ist Emulation.
2. Die Bedienungsanleitung erklärt, warum: der Watt-Modus der Konsole ist selbst
   eine Emulation über die Kadenz. Damit ist Stufe → Watt eine **Fläche**, keine
   Tabelle.
3. `esp32.heartrate` v0.3 hat ein **HR-Relay** bekommen — BLE-Peripheral, das
   den Gurt weitergibt. Das löst das Verbindungsbudget und liefert die Vorlage
   für die FTMS-Bridge.

- Gemessenes Geräteprofil und Auswertung der Anleitung: [GERAETEPROFIL.md](GERAETEPROFIL.md)
- Offene Messungen vor dem ersten Commit: [NACHTESTS.md](NACHTESTS.md)
- Erster Protokolltest (erledigt): [BLE-SCAN.md](BLE-SCAN.md)

---

## 1. Ziel

Der ESP32 wird der Trainingscomputer, den das Varon XTR II nicht hat: er liest die
Ergometerdaten, verbindet parallel den BLE-Pulsgurt, regelt den Widerstand nach
Programm oder Puls und zeigt alles in einer WebUI, die es wert ist, während des
Trainings angeschaut zu werden.

**Zwei Lücken, die das Projekt füllt.**

Erstens: das Bike bietet über BLE **keine Watt-Steuerung**. Es nimmt nur
Widerstandsstufen. Jede App, die ERG-Intervalle fahren will, scheitert daran.
Der ESP32 baut diese Fähigkeit nach — und gibt sie in v0.2 als Bridge an
MyWhoosh weiter.

Zweitens das pulsgesteuerte Training. Die Konsole hat dafür die Programme HR1,
HR2 und IND, und sie funktionieren auch — nur eben **einseitig**: die Anleitung
beschreibt eine Pulsobergrenze mit fünf Schlägen Hysterese, die den Widerstand
*senkt*, wenn man darüber liegt, ihn aber nicht anhebt, wenn man darunter liegt.
Einen Zielpuls von unten anfahren geht damit nicht. Dazu kommt: die Programme
laufen nur an der Konsole und sind über BLE nicht erreichbar — das
Target-Setting-Wort hat kein Heart-Rate-Bit.

`HR_HOLD` im ESP32 ist deshalb kein Nachbau, sondern die bessere Version: ein
beidseitiger Regler, der den Zielpuls von beiden Seiten hält, mit RR-Intervallen
und HRV im Rücken.

**Motivation bleibt erhalten.** MyWhoosh soll nicht ersetzt werden. Der
Zielzustand v0.2 ist eine Bridge, nicht ein Ersatz.

### Kernprinzip Steuerung

> Lesen ist harmlos, Schreiben nicht. Und `Success` ist kein Wirkungsbeleg.

- Die Verbindung zum Bike wird **nur auf Benutzerbefehl** aufgebaut.
- Das Bike akzeptiert **genau ein Central**. Solange der ESP verbunden ist,
  schaltet das Konsolendisplay ab und kein anderes Gerät kommt heran. Die WebUI
  muss deshalb **alle** Werte zeigen, die sonst die Konsole zeigt.
- Jede Widerstandsänderung geht durch den Limiter (§6). Kein anderer Codepfad
  schreibt an den Control Point.
- Das Gerät quittiert auch Kommandos, die es nicht ausführt — es hat im Labor
  `80 05 01` auf ein Watt-Ziel geantwortet, das es laut Feature-Bits nicht kann.
  Wirkung wird an `2AD2` gemessen, nicht aus der Antwort geschlossen.
- **Stop ist immer erreichbar** — fixer Button in der UI, nicht in einem Tab.

---

## 2. Muss (v0.1 — Coach)

Der ESP32 ist BLE-Central zu **zwei** Geräten: Bike und Pulsgurt. MyWhoosh ist
während der Fahrt nicht verbunden.

### Plattform

- **ESP32-S3 (YD-ESP32-S3)** als primäres Ziel, PSRAM aktiv
- WiFiManager (Captive Portal), keine Secrets im Source, NVS-Namespace `esphub`
- ConfigStore (NVS), LittleFS für Workouts, Kalibrierung und Session-Archiv
- WebServer :80, SSE, Browser-OTA
- Hub-Heartbeat `POST /api/register`, Session-Export via `postJson()`
- NimBLE 1.4.x wie heartrate und Sonde, `MAX_CONNECTIONS=3`

### Bluetooth — Bike (FTMS Central)

Gemessenes Profil in [GERAETEPROFIL.md](GERAETEPROFIL.md), hier nur die Pflichten:

- Connect **per gemerkter MAC samt AddrType**. Das Bike advertised während eines
  Links oft nicht; Discovery darf nicht darauf bauen. Name kommt nach dem Connect
  aus `2A00`.
- `2ACC` lesen und die Target-Setting-Bits auswerten. Die UI schaltet nur frei,
  was das Gerät meldet — bei diesem Bike also Resistance und Simulation, **nicht**
  Power.
- `2AD6` lesen → Stufenbereich als harte Klemme (hier 1,0…16,0, Schritt 1,0).
- `2AD8` darf **fehlen**. Kein Fehlerfall, nur ein Feature weniger.
- `2AD2` abonnieren und vollständig parsen. Flags sind bei diesem Gerät konstant
  `0x0B54`, der Parser muss aber generisch bleiben.
- `2AD9` mit **Notify** abonnieren (nicht Indicate — Spec-Abweichung des Geräts),
  dann `00` Request Control. Antworten dem Opcode zuordnen, mit Timeout.
- Subscribe mit Fallback auf `response=false`, wenn der CCCD-Write scheitert.
- `2ACE` ignorieren.
- Kein Scan, während ein Link steht.
- Verbindungsverlust → Session pausieren, nicht stillschweigend weiterrechnen.

### Puls — drei Quellen, eine Auswahl

Übernahme aus `esp32.heartrate`: Heart Rate Service `0x180D` / `0x2A37` inkl.
RR-Intervalle, Battery `0x180F`, RSSI, gleiche Sample-Struktur und RR-Pipeline.

Neu ist, dass es **drei** Wege zum Puls gibt, und sie unterscheiden sich in
Qualität und Kosten:

| Quelle | Kosten | RR / HRV | Anmerkung |
|--------|--------|----------|-----------|
| **HR-Relay** (`esp32.heartrate` v0.3) | ein BLE-Link | ja, unverändert weitergeleitet | empfohlen — der Relay hält den Gurt, MyWhoosh kann parallel daran hängen |
| **Gurt direkt** über BLE | ein BLE-Link | ja, falls der Gurt sie sendet | belegt den Gurt exklusiv, wenn er nur einen Client zulässt |
| **HR-Feld in `2AD2`** | **keine** | nein | setzt einen Gurt mit 5-kHz-Analogsender voraus |

Die dritte Quelle ist der Grund, warum im Laborlauf durchweg plausible 78–84 bpm
in `2AD2` standen, obwohl kein HR-Link bestand. Sie kostet nichts und eignet
sich als Rückfallebene und als Gegenprobe — aber ohne RR-Intervalle ist damit
weder HRV noch eine saubere Qualitätsbewertung möglich.

**Der Gurt ist austauschbar, und das muss er bleiben.** Die ersten beiden
Quellen sind reines `0x180D` und damit herstellerunabhängig — jeder
BLE-Brustgurt und jedes optische Armband funktioniert. Zwei heute gültige
Annahmen hängen dagegen am Polar H9 und dürfen nicht in den Code wandern:

| Annahme | gilt für | was bei einem anderen Gurt passiert |
|---------|----------|--------------------------------------|
| nur **ein** BLE-Client gleichzeitig | Polar H9, Garmin HRM-Dual | Gurte mit zwei Links machen das Relay optional statt notwendig |
| **5-kHz-GymLink** parallel zu BLE | Polar | ohne Analogsender bleibt das `2AD2`-HR-Feld leer, Quelle 3 entfällt |
| **RR-Intervalle** vorhanden | die meisten Brustgurte | optische Armbänder liefern oft keine; HRV muss sich dann abschalten |
| **Sensor Contact** wird nicht gemeldet | Polar H9 | andere Gurte melden es; die UI darf `n/a` nicht als „kein Kontakt" lesen |

Pflicht daraus: die Quellenliste wird zur Laufzeit aus dem gebildet, was
tatsächlich vorhanden ist. Quelle 3 erscheint nur, wenn im `2AD2`-Strom
überhaupt ein Pulswert ungleich null auftaucht; HRV-Auswertungen erscheinen
nur, wenn RR-Intervalle ankommen. Keine Quelle wird angeboten, weil ein
bestimmter Gurt sie könnte.

Pflicht: alle vorhandenen Quellen führen, die aktive über `hr_source` benennen,
und bei Abweichung zwischen BLE- und Bike-Wert nicht stillschweigend mischen.
Die Regelung in `HR_HOLD` nutzt die BLE-Quelle; fällt sie aus, ist der
Bike-Wert Rückfall statt Stillstand.

### Steuermodi

Die einzige echte Stellgröße ist die **Widerstandsstufe**, 16 diskrete Schritte.
Alles Watt-basierte ist Emulation durch den ESP32.

| Modus | Stellgröße | Umsetzung |
|-------|-----------|-----------|
| `OFF` | — | reines Dashboard, der ESP schreibt nichts |
| `MANUAL_LEVEL` | Stufe | direkt, `04 <sint16 LE>` in Zehntelstufen |
| `MANUAL_ERG` | Zielwatt | **emuliert:** `PowerController` wählt die Stufe |
| `WORKOUT` | Programm | Zielwatt oder Zielstufe je Schritt |
| `HR_HOLD` | Zielpuls | `HrController` → Stufe |
| `SIM` | Steigung | `0x11`, erst nach Test 4 aus [NACHTESTS.md](NACHTESTS.md) |

`MANUAL_ERG` und `HR_HOLD` sind damit keine Durchreiche mehr, sondern Regelung.
Das ist der inhaltliche Kern von v0.1 — siehe §6.

### Kalibrierung

Voraussetzung für `MANUAL_ERG`: der ESP32 braucht den Zusammenhang zwischen
Stufe, Kadenz und Leistung. Nach der Bedienungsanleitung ist das eine **Fläche**,
keine Kurve — bei fester Stufe steigt die Leistung mit der Kadenz.

Zwei Wege, die sich ergänzen:

**Geführter Sweep.** In der UI startbar: Stufen 1…16 durchfahren, je 60 s bei
gehaltener Kadenz, Mittelwerte aufnehmen. Das gibt das Gerüst der Fläche in
einem definierten Kadenzband. Der Lauf gibt die Ziel-Kadenz vor und verwirft
Punkte, deren Kadenz zu weit weggelaufen ist — genau der Fehler, der zwei der
drei Wirkungsmessungen im Laborlauf unbrauchbar gemacht hat.

**Passives Lernen.** Bei jeder normalen Fahrt fallen Tripel aus Stufe, Kadenz
und Leistung an. Stabile Abschnitte — Stufe konstant, Kadenz ruhig, Leistung
eingeschwungen — werden in die Fläche eingetragen und verdichten sie mit jeder
Einheit. Damit wird die Kalibrierung besser, ohne dass man dafür trainiert.

Weiteres:

- Persistenz auf LittleFS, versioniert, mit Datum und Stützstellenzahl
- Zwischen den Stützstellen interpoliert, außerhalb **nicht** extrapoliert —
  dort gilt das Ziel als unerreichbar
- Überschreibbar und exportierbar. Die Fläche ist ein Messergebnis, kein
  Konstantenblock im Code.
- Vorbelegung aus den Laborwerten, damit das Gerät ohne Kalibrierung nutzbar
  ist: bei ~59 rpm Stufe 1 ≈ 25 W und Stufe 10 ≈ 89 W. Diese Vorbelegung wird
  in der UI als „ungemessen" gekennzeichnet.

### Debug-Modus

Erste Versionen sollen vor allem Daten sammeln — das Gerät weiß mehr über das
Bike als jede Dokumentation. Der Debug-Modus ist deshalb kein Anhängsel, sondern
ein Muss in v0.1, und er erbt die Bauform von der Sonde.

- **Rohbyte-Ring** für `2AD2` und `2AD9`, eine Zeile je Paket im Format
  `{seq, ts, dir, uuid, hex, phase}` — genau das Format der Sonden-JSONL, damit
  die Aufzeichnungen als `FtmsCodec`-Fixtures direkt weiterverwendbar sind
- **Phasenmarken** setzbar, damit man im Log wiederfindet, was man gerade getan hat
- **Export** als NDJSON über HTTP, plus optionaler Push an den Hub nach jeder
  Session, damit die Aufzeichnungen nicht nur im RAM leben
- **Steuer-Journal:** jeder Write mit Zielwert, Limiter-Entscheidung, Antwort
  des Geräts und der gemessenen Wirkung 10 s danach. Die Frage „hat das Kommando
  etwas bewirkt" soll das Gerät selbst beantworten können.
- **Wirkungsurteil mit Kadenznormierung.** Kein Urteil aus dem Leistungsdelta
  allein: Fenster verwerfen, wenn die Kadenz unter einer Schwelle liegt oder sich
  zwischen Vorher und Nachher um mehr als etwa 10 % ändert. Im Laborlauf hat
  genau diese fehlende Normierung zweimal einen Effekt bescheinigt, wo keiner war.
- **Rohwert-Panel** in der UI hinter einem Schalter: Flags, Hex, Paketalter,
  Schattenstufe, Regler-Innenleben
- Default aus, aber ohne Neustart einschaltbar. Der Ring kostet RAM, nicht mehr.

### Profile — mehrere Nutzer am selben Bike

Das Bike wird von zwei Personen mit sehr unterschiedlichen Zielen benutzt.
Profile sind deshalb keine Komfortfunktion: eine Pulsobergrenze, die am falschen
Profil hängt, ist ein Sicherheitsproblem.

Je Profil persistent auf LittleFS: Name und Farbe, FTP mit Datum und Herkunft,
HRmax und Ruhepuls samt Zonenmodell, Gewicht für W/kg, die **führende Zone**
(Leistung oder Puls) für die Darstellung, harte Grenzen für Leistung, Puls und
Stufe, die Vorgabe-Kadenz, das Verhalten bei Pulsverlust sowie der eigene
Verlauf mit Sessions, Bestleistungen und Testhistorie.

Drei Regeln:

- **Kein stilles Standardprofil.** Ohne gewähltes Profil startet keine Session,
  sonst gelten die Grenzen des letzten Nutzers weiter.
- **Kein Profilwechsel in einer laufenden Session.** Erst beenden.
- **Die Kennfläche gehört nicht ins Profil.** Stufe × Kadenz → Watt ist eine
  Eigenschaft des Bikes und wird geteilt — so lernt sie aus beiden Nutzern mit.

Das Verhalten bei Pulsverlust ist je Profil wählbar: `freeze` hält die Stufe und
warnt (Leistungstraining), `reduce` senkt auf ein sicheres Niveau (**Vorgabe für
Reha-Profile**), `stop` pausiert.

### Trainingsprogramme

- Workouts als JSON auf LittleFS, Upload und Download über die WebUI
- **Editor in der WebUI** mit Live-Vorschau und Machbarkeitsprüfung, siehe
  [WEBINTERFACE.md §5](WEBINTERFACE.md)
- Schritt-Typen: `steady`, `ramp`, `interval` (mit `repeat`)
- Laufender Fortschritt: aktueller Schritt, Restzeit im Schritt, Restzeit gesamt
- Freies Fahren ohne Programm ist der Default

Ziele je Schritt:

| Ziel | Feld | Zweck |
|------|------|-------|
| Absolute Leistung | `power` | feste Vorgaben, Reha |
| Relative Leistung | `ftp_pct` | macht ein Workout zwischen Profilen übertragbar |
| Widerstandsstufe | `level` | direkt, ohne Regelung |
| Zielpuls | `hr` | geregelt über die Kaskade |
| **Pulsdeckel** | `limit.hr_max`, `limit.hr_soft` | **zusätzlich** zu einem Leistungsziel |

Der Pulsdeckel als eigenständige Begrenzung neben einem Leistungsziel ist der
Kern des Reha-Programms: „zehn Minuten bei 60 W, aber der Puls nicht über 120".
Er greift nicht erst am Grenzwert, sondern fängt im Anfahrband darunter an
gegenzuhalten — der Puls reagiert träge, ein harter Schwellwert würde
regelmäßig darüber schießen.

```json
{
  "name": "Physio Grundlage",
  "profile_hint": "anna",
  "progression": { "field": "duration_s", "step": 60, "max": 1800 },
  "steps": [
    { "type": "steady", "duration_s": 120, "target": { "power": 40 },
      "limit": { "hr_max": 120, "hr_soft": 115 }, "label": "Einfahren" },
    { "type": "steady", "duration_s": 600, "target": { "power": 60 },
      "limit": { "hr_max": 120, "hr_soft": 115 }, "label": "Hauptteil" },
    { "type": "steady", "duration_s": 120, "target": { "power": 35 },
      "limit": { "hr_max": 120 }, "label": "Ausfahren" }
  ]
}
```

`progression` macht daraus ein Programm statt einer Einheit: nach einer
**sauber** gefahrenen Session — Deckel hat nicht gegriffen, Zielleistung
gehalten, Puls im Band — schlägt die UI vor, den Hauptteil um eine Minute zu
verlängern. War die Einheit nicht sauber, wird nicht gesteigert, und die UI sagt
warum. Dazu eine eigene Verlaufsansicht mit Dauer, Leistung, Puls und Anzahl
der Deckel-Eingriffe je Einheit.

Das Reha-Programm ist übrigens von Test 1 der Nachtests **unabhängig**: 60 W
liegen nach den Laborwerten bei etwa Stufe 7 und damit klar im gemessenen
Bereich. Selbst wenn die Stufendecke bei 130 W liegt, ist dieser Anwendungsfall
vollständig bedient.

Für Zielwatt im oberen Bereich gilt weiter: die UI muss ein unerreichbares Ziel
**als solches anzeigen** statt es stumm zu klemmen.

### Geführte Tests

| Test | Ablauf | Ergebnis |
|------|--------|----------|
| Rampe | ab 60 W alle 60 s um 20 W, bis Abbruch | MAP aus der besten Minute, FTP ≈ 0,75 × MAP |
| 20 Minuten | nach Einfahren 20 min gleichmäßig maximal | FTP = 0,95 × Ø Leistung |
| Recovery | 60 s Pulsabfall nach Belastung | Erholungsnote wie die Konsolenfunktion |

Ablauf wie ein Workout, aber mit eigener Ergebnisseite und Vergleich zum letzten
Wert. Der FTP wird **nie automatisch übernommen** — die Übernahme ins Profil ist
eine bewusste Bestätigung.

Zwei Einschränkungen: die UI prüft vor dem Start gegen die Kennfläche, ob die
nötige Leistung überhaupt erreichbar ist, und sagt sonst ab. Und für Reha-Profile
sind Maximaltests **ausgeblendet** — dort sind sie kein Feature, sondern ein
Risiko.

### Session und Auswertung

- Ringbuffer im RAM für die Live-Charts, Fenster ≥ 15 min bei 1 Hz
- Am Ende: Zusammenfassung auf LittleFS und Export an den Hub
- Berechnet: Durchschnitts- und Normalized Power, Arbeit in kJ, IF, TSS,
  Zeit je Pulszone, HRV-Kennzahlen aus der heartrate-Pipeline
- FTP als Config-Wert, dazu eine Schätzung aus der besten 20-Minuten-Leistung
- Distanz, Energie und Zeit kommen vom Bike und laufen über Reboots des ESP
  nicht mit — die Session-Zeit führt der ESP selbst

### Hub-IOs

| Key | Typ | Unit |
|-----|-----|------|
| `ergo_state` | sensor | string |
| `profile` | sensor | string |
| `bike_connected` / `hr_connected` | sensor | 0/1 |
| `power` / `power_target` | sensor | W |
| `level` / `level_target` | sensor | Stufe |
| `cadence` | sensor | rpm |
| `speed` | sensor | km/h |
| `distance` | sensor | m |
| `heart_rate` / `hr_source` | sensor | BPM, string |
| `hr_zone` / `hr_zone_label` | sensor | 1–5 / Zx |
| `control_mode` / `control_granted` | sensor | string, 0/1 |
| `session_active` / `session_duration` | sensor | 0/1, s |
| `work_kj` / `calories` | sensor | kJ, kcal |
| `np` / `if` / `tss` | sensor | W, –, – |
| `workout_name` / `workout_step` / `workout_remaining` | sensor | string, string, s |
| `target_reachable` | sensor | 0/1 |

`level` ist der **Schattenwert** des ESP, nicht eine Rückmeldung des Bikes —
`2AD2` liefert kein Resistance-Level-Feld.

---

## 3. Muss (v0.2 — Bridge)

Der ESP32 ist gleichzeitig Central zum Bike **und** FTMS-Peripheral für MyWhoosh.

MyWhoosh holt den Puls **nicht** vom Ergometer, sondern koppelt den Gurt
getrennt. Zusammen mit dem HR-Relay aus `esp32.heartrate` v0.3 ergibt das eine
Topologie mit zwei Knoten, in der kein Gerät am Limit arbeitet:

```
Polar H9 ──BLE──▶ ESP #1  heartrate (Relay)  ──BLE──▶ MyWhoosh   (Puls)
                     │                       └──BLE──▶ ESP #2     (Puls)
                     └── 5-kHz-GymLink ──▶ Bike-Konsole (HR-Feld in 2AD2)

Varon XTR II ──BLE──▶ ESP #2  ergo  ──BLE Peripheral──▶ MyWhoosh (FTMS)
                         │
                         └── WiFi ──▶ WebUI + ioBroker-Hub
```

Der Relay löst dabei ein Problem, das sonst die Bridge gekippt hätte: der
**Polar H9 erlaubt nur einen BLE-Client**. Ohne Relay müssten sich `esp32.ergo`
und MyWhoosh um den Gurt streiten. Mit Relay hält ihn genau ein Knoten, und
beide Verbraucher hängen daran — `relayMaxClients` ist auf 2 voreingestellt und
passt damit exakt.

Nach dem Scan ist die Bridge nicht mehr nur ein Datenabgriff, sondern eine
**Protokoll-Aufwertung**. Das Bike kann kein Set Target Power. MyWhoosh und
Zwift senden im ERG-Modus genau das. Der ESP32 schließt die Lücke:

- Er meldet in seinem **eigenen** `0x2ACC` `supports_power_target = true` und
  liefert ein `0x2AD8`, das das Bike nicht hat.
- Ein Watt-Ziel der App geht in den `PowerController` und kommt als
  Widerstandsstufe am Bike heraus.
- Damit wird aus einem Bike, das nur Stufen kennt, ein ERG-fähiger Trainer.

Weiteres:

- Peripheral exponiert `0x1826` mit `0x2ACC`, `0x2AD2`, `0x2AD6`, `0x2AD8`,
  `0x2AD9`, `0x2ADA`; zusätzlich `0x1818` (CPS) und `0x1816` (CSC), weil manche
  Apps darauf bestehen. Optional `0x180D` mit den H9-Daten.
- Unser `0x2AD9` bietet **Indicate** wie die Spec es will — nicht die
  Notify-Eigenheit des Bikes nachbauen.
- Advertising mit Service-UUID `0x1826` **und** FTMS Service Data mit gesetztem
  Indoor-Bike-Bit.
- `0x11` Simulation: wenn Test 4 zeigt, dass das Bike es nativ versteht, wird
  SIM durchgereicht statt emuliert. Das ist der bessere Weg, weil die Auflösung
  von 0,01 % Steigung das 16-Stufen-Raster umgeht.
- Eingriffe über Passthrough hinaus: **Difficulty-Faktor** und **HR-Deckel**.
- Loggen und Charts laufen im Bridge-Betrieb voll weiter.

**Verbindungsbudget.** Für den Ergo-Knoten sind es Bike (Central) + Puls
(Central) + App (Peripheral) = drei Links, also `MAX_CONNECTIONS=3`. Das ist
dasselbe Budget, mit dem `heartrate-s3` heute schon läuft: ein Gurt-Link plus
zwei Verbraucher, und das ohne PSRAM. Die Rollenkombination ist damit in der
Familie erprobt — anders als in Revision 1 ist sie kein Risiko mehr, sondern
eine Referenz.

Fällt die Pulsquelle auf das HR-Feld in `2AD2` zurück, sinkt der Ergo-Knoten auf
**zwei** Links. Das ist die Reserve, wenn es eng wird: HRV entfällt, die
Regelung bleibt.

Der Dual-Link am Bike ist trotzdem noch nicht gemessen — Test 5 der Nachtests
holt das nach. Und anders als im `heartrate`-Env darf die Peripheral-Rolle
nicht wegkonfiguriert werden.

---

## 4. Nicht-Ziele

- Keine Cloud, kein Strava-Upload direkt vom ESP
- Keine medizinischen Aussagen oder Alarme
- Kein Nachbau von MyWhoosh — keine Videos, keine Karten, keine Avatare
- Kein automatischer Connect nach Boot in v0.1
- Kein ANT+
- Kein FitShow — das Bike braucht es nicht, es spricht Standard-FTMS
- Kein Anfassen des Vendor-Service `0x1850`, solange FTMS trägt
- Keine Steuerung ohne aktive Session

---

## 5. Protokoll — was dieses Gerät wirklich tut

Vollständig in [GERAETEPROFIL.md](GERAETEPROFIL.md). Für den Parser zählt:

### `2AD2` Indoor Bike Data

Flags konstant `0x0B54`, Paket immer 19 Byte:

```
Flags(2) Speed(2) Cadence(2) Distance(3) Power(2)
EnergyTotal(2) EnergyPerHour(2) EnergyPerMin(1) HR(1) Elapsed(2)
```

Einheiten: Speed 0,01 km/h · Cadence 0,5 rpm (Wert / 2) · Distance uint24 m ·
Power sint16 W · HR uint8 · Elapsed s.

Zwei Fallen bleiben im generischen Parser: Flag-Bit 0 ist **invertiert**
(0 = Speed vorhanden), und Total Distance ist **3 Byte**. Nicht gesendet werden
Average-Felder, MET, Remaining Time und — trotz gemeldeter Fähigkeit — das
**Resistance Level**.

### `2AD9` Control Point

| Kommando | Bytes | Status |
|----------|-------|--------|
| Request Control | `00` | belegt |
| Start/Resume | `07` | belegt |
| Set Target Resistance | `04 <lo> <hi>` sint16 LE, 0,1 Stufe | **belegt, wirkt** |
| Stop | `08 01` | belegt |
| Set Target Power | `05 <lo> <hi>` | Feature-Bit **nein**, Wirkung unbelegt |
| Simulation | `0x11` | Feature-Bit ja, **ungetestet** |

Antwort `80 <opcode> <result>`, `01` = Success, per **Notify**.

`04 0A` als uint8 wirkt nicht — die sint16-Form ist die richtige.

### Und wenn ein anderes Ergometer angeschlossen wird

Alles auf dieser Seite ist **Messung an einem Exemplar**, nicht Gesetz. Ein
zweites Bike im Haushalt, ein Ersatzgerät nach einem Defekt oder ein echter
Smarttrainer müssen ohne Reflash laufen. Deshalb die Regel: **nichts
Gerätespezifisches als Konstante.** Jede Eigenschaft ist entweder ein
gemeldetes Feature, ein gelesener Bereich oder eine gemessene Eigenart.

Was zur Verbindungszeit ermittelt wird (`ftms::Capabilities`):

| Eigenschaft | Quelle |
|-------------|--------|
| Wattziel möglich | `0x2ACC` Target-Bit 3 **und** vorhandene `0x2AD8` |
| Stufenziel möglich, Bereich, Schrittweite, Stufenzahl | `0x2ACC` Target-Bit 2, `0x2AD6` |
| Simulation möglich | `0x2ACC` Target-Bit 13 |
| Welche Felder der Live-Stream führt | beobachtete `2AD2`-Flags |
| Meldet das Gerät die Stufe zurück | `2AD2` Bit 5 — sonst Schattenwert |
| Control Point per Notify oder Indicate | Deskriptor beim Verbinden |

Daraus fällt die **Steuerstrategie** von selbst heraus, statt im Code zu
stehen: `DirectTarget`, wenn das Gerät ein Wattziel meldet *und* seinen
Wattbereich veröffentlicht; `EmulateResistance`, wenn es nur Stufen kann;
`None`, wenn es gar keinen Stellweg hat — dann bleibt ein Dashboard.

Die Kopplung an `0x2AD8` ist dabei Absicht und kein Formalismus. Der Varon
quittiert `05 64 00` mit Success, obwohl er das Feature nicht meldet. Wer ein
Wattziel behauptet, aber seinen Wattbereich nicht veröffentlicht, bekommt
deshalb keinen Vertrauensvorschuss — er landet in der Emulation, was schlimmstenfalls
unnötig umständlich, aber nie gefährlich ist. Umgekehrt lässt sich das
Vertrauen im Geräteprofil nach einer Messung ausdrücklich setzen.

Was sich **nicht** aus den Metadaten ergibt, gehört in ein **Geräteprofil je
MAC** auf LittleFS:

- welche Lesart von `0x04` wirkt (`uint8` oder `sint16`) — rechnerisch
  entschieden, wenn der Bereich über 25,5 hinausgeht, sonst gemessen
- die Kennfläche Stufe × Kadenz → Watt, samt gemessener Leistungsdecke
- ob dem Wattziel trotz fehlender `0x2AD8` zu trauen ist
- Eigenarten beim Verbindungsaufbau, etwa ob `Request Control` nach jedem
  Reconnect neu nötig ist

Damit ist ein Gerätewechsel ein Scan, ein Connect und ein Kalibrierlauf —
kein Firmwarethema. Die Kennflächen bleiben getrennt, sodass zwei Geräte
nebeneinander bestehen können, ohne ihre Messwerte zu vermischen.

Zwei Dinge bleiben bewusst offen, bis es einen Anlass gibt: Rudergerät und
Laufband (`0x2AD1` bzw. `0x2ACD` statt `0x2AD2`) werden nicht unterstützt, und
Hersteller-Protokolle wie FitShow oder Zycle ebenfalls nicht. Beides wäre eine
eigene Codec-Familie, kein Parameter.

---

## 6. Limiter und Regelung

Alles, was ans Bike geschrieben wird, geht durch **einen** Punkt im Code.
Vorlage ist `ProbeGuard` aus der Sonde, der sich im Labor bewährt hat.

### Limiter

| Regel | Default | Zweck |
|-------|---------|-------|
| Opcode-Whitelist | `00 01 04 07 08`, `11` nur nach Freigabe | keine Experimente im Produktivpfad |
| `05` Wattziel | nur bei `powerTargetTrusted` | Success ohne `0x2AD8` ist kein Beleg |
| `06` Pulsziel | immer abgelehnt | wird als Kaskade über den Widerstand gefahren |
| Klemmen aus `2AD6` | Gerätebereich ∩ Profil ∩ absolute Schranke | keine ungültigen Stufen |
| Rasterung | auf die Schrittweite des Geräts | keine Zwischenwerte, die niemand annimmt |
| Stufenrampe **nach oben** | max 1 Stufe pro 2 s | kein Widerstandsschlag ins Knie |
| Lastabbau | **sofort, ohne Rampe** | ein gebremster Pulsdeckel wäre keiner |
| `08` Stop | immer erlaubt, ohne jede Prüfung | auch wenn über das Gerät nichts bekannt ist |
| HR-Obergrenze | aus, sonst Config | Ziel wird gekappt |
| Deadman Trittfrequenz | 10 s ohne Kadenz → `08 01` | Absteigen ohne Restlast |
| Not-Stop | UI-Button, fix sichtbar, nicht sperrbar | `08 01` |
| Reboot-Pfad | `08 01` **vor** jedem Neustart | siehe Test 6 |
| Klemmen der Klemmen | nicht per API aushebelbar | aus der Sonde übernommen |

Die Rampenbegrenzung ist gegenüber Revision 1 anders gebaut: nicht mehr 25 W/s,
denn Watt sind keine Stellgröße. Bei 16 Stufen und grob 7 W pro Stufe entspricht
eine Stufe pro 2 s etwa 3,5 W/s — konservativ, notfalls per Config schneller.

Der Reboot-Pfad ist neu und hängt an Test 6: solange unklar ist, was das Bike mit
einer gesetzten Stufe macht, wenn der Client wegbricht, bleibt der Hub-Watchdog
aus und jeder gewollte Neustart stoppt vorher.

Die **Asymmetrie der Rampe** ist die wichtigste Einzelentscheidung in diesem
Modul. Last aufzubauen ist gefährlich und wird gebremst; Last wegzunehmen ist
die Rettung und passiert sofort. Ein Pulsdeckel, der erst in zwei Sekunden
greifen darf, wäre kein Pulsdeckel.

Nach `08` Stop und `01` Reset fällt der Schattenwert der Stufe auf das
Minimum zurück. Das Bike meldet die Stufe nicht, also ist nach einem Stop
unbekannt, wo es steht — die konservative Annahme lässt den Wiederaufbau von
unten rampen, statt aus einem veralteten Wert heraus zu springen.

Der Limiter ist Arduino-frei gebaut: keine `String`, kein `millis()`, die Zeit
kommt als Parameter herein. Dadurch läuft die gesamte Sicherheitslogik im
nativen Test (`test/test_limiter`, 21 Fälle), und zwar genau der Code, der
später auch auf dem Gerät fährt.

### `PowerController` — ERG-Emulation

Die Konsole macht im Watt-Modus genau das, was hier gebaut wird: „Im Watt
Programm wird der Widerstand basierend auf der RPM angepasst, dass die Leistung
konstant bleibt." Das Verfahren ist also vom Hersteller vorgemacht — nur rechnet
die Konsole blind, während der ESP32 die tatsächliche Leistung zurückmisst.

Damit ist die Bauform vorgegeben: **Vorsteuerung aus der Kennfläche plus
langsame Rückführung.** Aus Zielwatt und aktueller Kadenz liefert `CalibTable`
die passende Stufe, und ein langsamer Integralterm korrigiert die Abweichung
zwischen erwarteter und gemessener Leistung. Die Kadenz ist dabei keine
Störgröße, die man wegregelt, sondern ein **gemessener Eingang** der
Vorsteuerung — steigt die Kadenz, muss die Stufe fallen.

Sollte Test 2 überraschend zeigen, dass die Leistung über die Kadenz konstant
bleibt, entfällt die Kadenzachse und die Kennfläche wird eine Kurve. Der Regler
bleibt derselbe.

Weiter gilt:

- Stellgröße ist eine von 16 Stufen. Das Watt-Ziel ist **quantisiert** und wird
  im Regelfall nicht exakt getroffen.
- Die UI zeigt Ziel, erreichbaren Wert und Ist. Ein Ziel oberhalb des
  Stufenmaximums wird als unerreichbar markiert, nicht stumm geklemmt.
- Messgröße ist die Leistung aus `2AD2`, geglättet über etwa 5 s — die
  Einzelwerte streuen im Labor um ±15 W.
- Regelzyklus im Bereich 5–10 s. Schneller bringt nichts, weil jede Stufe
  einschwingen muss und die Leistung ohnehin geglättet wird.

### `HrController` — Pulsregelung

Der Aktor ist auch hier die Stufe, aber die Bauform ist eine **Kaskade**:
Puls → Zielwatt → `PowerController` → Stufe.

Ein direkter Regler Puls → Stufe wäre einfacher, scheitert aber an der
Kadenzabhängigkeit: schaltet der Fahrer bei gleicher Stufe von 60 auf 90 rpm,
steigt die Leistung und mit ihr der Puls, ohne dass sich am Stellwert etwas
geändert hat. Der innere Watt-Regler fängt das ab, lange bevor der träge Puls
davon etwas mitbekommt. Der äußere Pulsregler arbeitet dadurch auf einer
stabilen Größe statt auf einer, die der Fahrer jederzeit mit dem Trittverhalten
verschiebt.

Das ist zugleich der Punkt, an dem `HR_HOLD` die Konsole schlägt: die regelt
nach der Anleitung nur einseitig — Widerstand runter bei fünf Schlägen über der
Grenze, und sonst nichts.

Unabhängig von der Bauform:

- Totband um den Zielpuls, Default ±3 BPM
- Herzfrequenz reagiert träge — Regelzyklus 10–15 s, I-Anteil mit Anti-Windup
- Ausgang durch dieselbe Stufenrampe wie alles andere
- Harte Klemmen `hr_hold_min_level` / `hr_hold_max_level` aus der Config
- Fällt der Gurt aus: Stufe einfrieren, UI warnt sichtbar, nach Timeout Rückfall
  auf `MANUAL_LEVEL`
- Regelparameter gehören in die Config und in die UI

---

## 7. WebUI

**Vollständiges Designkonzept: [WEBINTERFACE.md](WEBINTERFACE.md).** Dort stehen
Farbsystem, Layouts, Profile-, Workout- und Testansichten sowie die
Abnahmekriterien der UI. Hier nur die tragenden Entscheidungen.

Die UI ist nicht die Beigabe zur Firmware, sie ist das Produkt — und während des
Trainings die **einzige** Anzeige, weil das Konsolendisplay bei bestehendem
BLE-Link abschaltet.

**Die Zone führt die Optik.** Hintergrund, Akzent und das Glühen der Hero-Zahl
folgen der aktiven Trainingszone. Aus zwei Metern und außer Atem liest man
Farbe, nicht Ziffern. Es gibt immer nur **eine** führende Zone — Leistung oder
Puls, je Profil festgelegt. Zwei konkurrierende Farbsysteme auf einem Schirm
sind unlesbar. Farbe ist dabei nie der einzige Träger: jede Zone trägt zusätzlich
Kürzel und Namen.

**Tablet-First, nicht Desktop-verkleinert.** Gelesen wird aus zwei Metern, mit
Puls 150 und verschwitzten Fingern. Keine Hover-Zustände, keine Tooltips, nichts
kleiner als 44 × 44 px.

**Eine Hero-Zahl, und die hängt am Profil.** Watt beim Leistungstraining, Puls
beim Reha-Training. Wer auf eine Pulsgrenze trainiert, muss den Puls groß sehen
und nicht suchen.

**Ehrlichkeit über den Stellweg.** Was der ESP32 gestellt hat, was davon ankommt
und wo die Decke ist, gehört sichtbar hinein. Die Stufenkachel zeigt `12/16`
samt Reserve; steht sie auf `16/16`, ist das Wattziel nicht fahrbar und wird
durchgestrichen. Weil das Bike die Stufe nicht zurückmeldet, ist der Wert als
Schattenwert gekennzeichnet.

Drei Elemente tragen die Ride-Ansicht: die **Zonenschiene** mit Zeit je Zone und
aktueller Position — das ist das MyWhoosh-Gefühl und am Ende von selbst die
Auswertung. Der **Ist-vs-Ziel-Chart** mit Zielleistung als Stufenlinie und
gefahrener Leistung als Fläche darunter, dazu die Geisterlinie der besten
früheren Session desselben Workouts. Und die **Kadenz-Kachel mit Halte-Hinweis**,
die kein kommerzielles Produkt braucht: auf diesem Bike hängt die Leistung bei
fester Stufe an der Kadenz, und wer sie ruhig hält, bekommt eine deutlich
genauere Regelung.

Umsetzung wie in der Familie: PROGMEM im `UiPages.h`-Muster, SSE für Live-Daten,
Canvas für Charts, keine Fremdbibliotheken, keine Build-Kette. Das Flash-Budget
ist dabei der kritische Punkt — heartrate braucht für eine einfachere UI schon
etwa 27 kB. Editor und Debug-Panel werden nachgeladen statt mitgeliefert; reicht
es trotzdem nicht, wandert die UI nach LittleFS. Diese Entscheidung wird
gemessen, nicht geraten.

---

## 8. API und SSE

| Endpoint | Methode | Zweck |
|----------|---------|-------|
| `/` | GET | WebUI |
| `/events` | GET | SSE (sample, state, workout, scan) |
| `/api/status` | GET | Snapshot |
| `/api/history` | GET | Ringbuffer-JSON |
| `/api/ble/scan/start` \| `/stop` | POST | Scan |
| `/api/ble/devices` | GET | Scan-Cache + remembered, je Rolle |
| `/api/ble/connect` \| `/disconnect` | POST | `{ role: "bike"\|"hr", mac, addrType }` |
| `/api/ble/remember` \| `/forget` | POST | `{ role, mac, addrType, name? }` |
| `/api/control/mode` | POST | `{ mode, value? }` |
| `/api/control/stop` | POST | Not-Stop |
| `/api/calib/start` \| `/abort` \| `/get` \| `/put` | POST/GET | Kennfläche |
| `/api/debug` | GET/POST | Debug-Modus an/aus, Ringgröße |
| `/api/debug/log` | GET | NDJSON, `?since=&max=&phase=` |
| `/api/debug/phase` | POST | Phasenmarke setzen |
| `/api/debug/export` | GET | Status + kompletter Ring |
| `/api/profile/list` \| `/get` \| `/put` \| `/delete` | GET/POST | Profile |
| `/api/profile/select` | POST | `{ id }` — Pflicht vor Sessionstart |
| `/api/workout/list` \| `/load` \| `/start` \| `/pause` \| `/skip` \| `/upload` | GET/POST | Programme |
| `/api/workout/put` \| `/download` \| `/validate` | POST/GET | Editor, Machbarkeitsprüfung |
| `/api/test/list` \| `/start` \| `/abort` \| `/result` | GET/POST | geführte Tests |
| `/api/test/accept-ftp` | POST | FTP bewusst ins Profil übernehmen |
| `/api/session/list` \| `/get` \| `/delete` \| `/export` | GET/POST | Archiv |
| `/api/bridge` | GET/POST | Bridge an/aus, Difficulty, HR-Deckel (v0.2) |
| `/api/config` | GET/POST | Einstellungen |
| OTA-Routen | | wie Schwesterprojekte |

```
event: sample
data: {"t":...,"pw":214,"pw_t":220,"pw_reach":218,"lvl":12,"cad":87,
       "spd":3120,"dist":8412,"hr":148,"hr_src":"h9","zone":3,"kj":312}

event: state
data: {"state":"READY","mode":"HR_HOLD","bike":true,"hr":true,
       "control":true,"bridge":false,"calib":"ok"}

event: workout
data: {"name":"4x4 Norweger","step":"work","block":2,"of":4,
       "step_remaining_s":72,"total_remaining_s":1120,"reachable":true}
```

---

## 9. Software-Struktur

Die Sonde ist ausdrücklich kein Wegwurf-Code. Vier Module wandern mit:

| Vorlage | Wird in `esp32.ergo` |
|---------|----------------------|
| `BleProbe` (Sonde) | `BleCentral` + `FtmsClient` |
| `tools/ftms.py` (Sonde) | `FtmsCodec` (C++), gleiche Fixtures |
| `ProbeGuard` (Sonde) | `Limiter` |
| `ProbeLog` (Sonde) | `DebugLog` |
| `ConfigStore`, `HubClient`, `NetUtil`, `UiPages` | unverändert übernehmen |
| `scan-20260910/*.jsonl` (Sonde) | Testfixtures für `FtmsCodec` |
| **`HrServer`** (heartrate v0.3) | **`FtmsServer`** — direkte Vorlage |
| `HrParser` / `buildHeartRateMeasurement()` | Muster „Parser und Encoder als Paar, Roundtrip-testbar" |

Die letzte Zeile ist die wichtigste Neuigkeit. `HrServer` löst für den
HR-Sensor genau die Aufgaben, die `FtmsServer` für den Trainer lösen muss:
Peripheral aufsetzen, Advertising mit Service-UUID und passender Appearance,
Client-Buchhaltung mit Limit, Notify-Einspeisung **aus dem Loop statt aus dem
NimBLE-Callback**, MTU-Budget von 20 nutzbaren Byte, Verhalten bei stehendem
Verbraucher und weggebrochener Quelle. Das ist alles bereits durchdacht und
läuft — `FtmsServer` ist damit kein Neuland, sondern eine Übertragung.

Drei Regeln aus dem Relay-Pflichtenheft gelten unverändert weiter: kein Notify
aus dem Callback, kein zweiter Konsument des Dirty-Flags, und Rohdaten treu
weiterreichen statt unterwegs zu interpretieren.

```
esp32.ergo/
├── platformio.ini
├── docs/PFLICHTENHEFT.md
└── src/
    ├── main.cpp
    ├── app/App.*
    ├── core/ConfigStore.*  HubClient.*  NetUtil.*  HistoryStore.*  SessionStore.*
    │         ProfileStore.*
    ├── ble/BleCentral.*    FtmsClient.*  FtmsCodec.*  HrProfile.*  DeviceStore.*
    │        FtmsServer.*   (v0.2)
    ├── control/Limiter.*   PowerController.*  HrController.*  WorkoutEngine.*
    │           CalibTable.*  Metrics.*  DebugLog.*  TestRunner.*
    └── web/UiPages.*
```

| Modul | Verantwortung |
|-------|----------------|
| `BleCentral` | Scan, Multi-Connect, NimBLE-Lifecycle, RSSI, CCCD-Fallback |
| `FtmsCodec` | reines Parsen und Bauen der FTMS-Bytes, **ohne BLE-Abhängigkeit** |
| `FtmsClient` | Discovery, Subscriptions, Control-Point-Sequenzen, Freigabe halten |
| `FtmsServer` | Peripheral-Seite der Bridge, inkl. aufgewertetem `2ACC`/`2AD8` (v0.2) |
| `HrProfile` | HRM-Parsing, RR-Pipeline (aus heartrate) |
| `Limiter` | **einziger** Schreibpfad zum Bike, alle Regeln aus §6 |
| `CalibTable` | Kennlinie Stufe → Leistung, Persistenz, Interpolation |
| `PowerController` | ERG-Emulation, Zielwatt → Stufe |
| `HrController` | Pulsregelung → Stufe |
| `WorkoutEngine` | Programmablauf, Schritt-Zustand, Zielwertquelle, Pulsdeckel je Schritt |
| `FtmsCapabilities` | leitet aus Features, Bereichen und Datenstrom ab, was das Gerät kann; bestimmt die Steuerstrategie |
| `DeviceStore` | Geräteprofil je MAC: Stufenformat, Kennfläche, Leistungsdecke, Eigenarten |
| `ProfileStore` | Nutzerprofile, Grenzen, Zonenmodelle, aktives Profil |
| `TestRunner` | Rampe, 20 Minuten, Recovery; Auswertung ohne automatische Übernahme |
| `Metrics` | NP, IF, TSS, kJ, Zonen, FTP-Schätzung |
| `DebugLog` | Rohbyte-Ring, Phasenmarken, Steuer-Journal, NDJSON-Export |
| `SessionStore` | Session-Persistenz, Bestleistungen fürs Ghost |
| `App` | Scheduler WiFi ↔ BLE, Zustandsmaschine, Watchdog |

`FtmsCodec` bleibt BLE-frei. Damit sind die Parser und die Grenzfälle gegen die
274 aufgezeichneten Pakete aus dem Laborlauf testbar, ohne Hardware — genau wie
`tools/ftms.py` es heute schon vormacht.

### WiFi / BLE Coexistence

Drei BLE-Links plus WiFi plus SSE ist mehr Funklast als bei heartrate. Die
Learnings gelten verstärkt: WiFi-Modem-Sleep zugunsten der BLE-Airtime,
GATT-Arbeit auf einem eigenen FreeRTOS-Task, Scan nur auf Befehl und **nie**
während eines Links.

Anders als die Sonde darf `esp32.ergo` GATT-Aufrufe nicht synchron im
Webserver-Handler abarbeiten. Für ein Laborwerkzeug ist das die richtige
Semantik, für ein Gerät mit laufender SSE-UI nicht.

---

## 10. Hardware

| Item | Hinweis |
|------|---------|
| **YD-ESP32-S3 (N16R8)** | Ergo-Knoten, primäres Ziel, `esptool flash_id` vorher |
| **Zweiter ESP32-S3** | Relay-Knoten mit `esp32.heartrate` v0.3, hält den Gurt |
| HAMMER Varon XTR II | `TC174`, MAC `c2:32:a5:1e:bf:b5`, Profil vermessen |
| Polar H9 | BLE (1 Client) + ANT+ + 5-kHz-GymLink gleichzeitig |
| USB-Netzteil | am Bike, dauerhaft, für beide Knoten |
| ESP32 D1 Mini | nur optional für v0.1, **nicht** für die Bridge |

Portal-SSID: `ESP-Ergo-Setup` · `fwType`: `ergo` ·
Firmware-Bin: `ergo.<semver>.esp32s3.bin`

### PlatformIO

Vorlage ist das Env `heartrate-s3`, das denselben Rollenmix schon fährt:

```ini
[env:ergo-s3]
board = esp32-s3-devkitc-1
board_build.partitions = min_spiffs.csv
build_flags =
  ${env.build_flags}
  -DARDUINO_USB_CDC_ON_BOOT=0
  -DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=3
  -DCONFIG_NIMBLE_CPP_DEBUG_ASSERT_ENABLED=0
  ; ROLE_PERIPHERAL / ROLE_BROADCASTER NICHT deaktivieren (Bridge v0.2)
lib_deps = h2zero/NimBLE-Arduino@^1.4.3
```

Bemerkenswert daran: `heartrate-s3` läuft mit drei Links, WiFi und offener
SSE-UI **ohne PSRAM** — keine `memory_type`- oder `psram_type`-Zeile, kein
`BOARD_HAS_PSRAM`. Revision 1 hatte PSRAM als notwendig angenommen; belegt ist
das nicht. PSRAM wird erst eingeschaltet, wenn der Heap es verlangt, und dann
mit Messung statt auf Verdacht.

NimBLE bleibt auf 1.4.x gepinnt wie in heartrate und Sonde, damit der Code
unverändert wandert. In 1.4 sind `getStartHandle`/`getEndHandle` privat —
nicht verwenden.

---

## 11. Versionen

### v0.0 — Messen (läuft, `esp32.ftmsprobe`)

Protokolltest erledigt. Offen sind die sechs Punkte aus
[NACHTESTS.md](NACHTESTS.md), davon zwei blockierend: Stufen-Sweep und
Kadenzabhängigkeit.

### v0.1 — Coach

Das Fundament, und schon vollständig nutzbar für beide Personen.

Bike und Puls als Central, **Profile mit Grenzen**, `MANUAL_LEVEL`,
`MANUAL_ERG` (emuliert), `HR_HOLD`, **Leistungsziel mit Pulsdeckel** (das
Reha-Programm), Kalibrierung aus Sweep und passivem Lernen, Workouts als
JSON-Datei mit Upload, Session-Archiv, Ride-UI mit Zonenschiene und Charts,
Limiter, **Debug-Modus**, Hub-IOs. Kein Peripheral-Modus, kein Auto-Connect,
kein Editor, keine Tests.

Der Debug-Modus ist dabei kein Nebenprodukt, sondern der halbe Zweck der
Version: solange die Kennfläche dünn und das Geräteverhalten teils unbelegt ist,
sammelt das Gerät die Daten, aus denen die nächsten Versionen gebaut werden.

Das Reha-Programm ist bewusst in v0.1 und nicht später: es braucht nur 60 W, ist
damit von der offenen Frage der Stufendecke unabhängig, und es ist der eine
Anwendungsfall, der ab dem ersten Tag echten Nutzen hat.

### v0.2 — Trainingslehre

Workout-Editor in der WebUI mit Live-Vorschau und Machbarkeitsprüfung, geführte
Tests (Rampe, 20 Minuten, Recovery), Physio-Progression als Verlaufsansicht,
Ghost-Vergleich gegen die eigene Bestleistung, Zonenauswertung je Session.

### v0.3 — Bridge

FTMS-Peripheral mit aufgewertetem Feature-Satz, MyWhoosh verbindet sich mit dem
ESP32, ERG-Emulation für die App, Difficulty-Faktor, HR-Deckel, CPS und CSC.
SIM-Passthrough, falls Test 4 es hergibt.

### v0.4 — Kür

HRV-Readiness als Intensitätsvorschlag, Langzeitstatistik im ioBroker,
Workout-Import aus `.zwo`, Export als TCX oder FIT.

Die Reihenfolge von v0.2 und v0.3 ist die eine offene Scope-Entscheidung. Für
v0.2 zuerst spricht, dass Editor und Tests direkt auf v0.1 aufsetzen und beiden
Nutzern sofort etwas bringen. Für v0.3 zuerst spricht, dass MyWhoosh die
Motivation ist, die erhalten bleiben soll.

---

## 12. Abnahmekriterien v0.1

1. Nach Boot verbindet sich der ESP mit **nichts**. Konsole und Handy
   funktionieren unverändert.
2. Connect per gemerkter MAC gelingt, **auch wenn das Bike nicht advertised**.
3. Watt, Kadenz, Geschwindigkeit, Distanz, Energie und Zeit erscheinen in der UI,
   plausibel gegen die Konsolenwerte vor dem Connect.
4. `FtmsCodec` dekodiert alle 274 Fixture-Pakete aus dem Laborlauf fehlerfrei und
   überlebt zusätzlich ein konstruiertes Paket mit gesetztem Flag-Bit 0 und
   variablem Layout ohne Fehlausrichtung.
5. `MANUAL_LEVEL` Stufe 10 → messbarer Leistungsanstieg gegenüber Stufe 1 bei
   gehaltener Kadenz, Rampe bleibt bei einer Stufe pro 2 s.
6. Der Kalibrierlauf erzeugt eine vollständige Kennfläche über Stufe 1…16 und
   mindestens ein Kadenzband und legt sie persistent ab. Punkte mit weggelaufener
   Kadenz werden verworfen und als verworfen ausgewiesen.
6a. Der Debug-Modus schreibt Rohbytes im Sonden-Format, und ein Export daraus
   lässt sich unverändert als `FtmsCodec`-Fixture einlesen.
6b. Das Steuer-Journal beurteilt die Wirkung eines Writes **kadenznormiert** und
   verweigert ein Urteil bei Kadenz unter der Schwelle — nachgewiesen an einem
   Write mit anschließendem Absteigen.
7. `MANUAL_ERG` auf ein erreichbares Ziel → Ist-Leistung liegt nach zwei Minuten
   im Band um das Ziel, das die Stufenquantisierung zulässt.
8. `MANUAL_ERG` auf ein **unerreichbares** Ziel → UI markiert es als unerreichbar
   und zeigt den erreichbaren Wert. Kein stummes Klemmen.
9. `HR_HOLD` auf 140 BPM → Puls stabilisiert innerhalb von fünf Minuten im
   Totband, ohne dass die Stufe pendelt.
10. Gurt mitten in `HR_HOLD` ausschalten → Stufe friert ein, UI warnt sichtbar,
    Rückfall auf `MANUAL_LEVEL` nach Timeout.
11. Not-Stop → Bike lastfrei in unter einer Sekunde.
12. Trittfrequenz 10 s auf null → automatische Pause.
13. Gewollter Neustart sendet vorher `08 01`.
14. Disconnect → Bike innerhalb weniger Sekunden wieder mit MyWhoosh koppelbar,
    Konsolendisplay wieder aktiv.
15. Charts laufen ohne Reload über SSE, 45 Minuten Session ohne Reboot oder Leck.
16. Session-Zusammenfassung liegt danach auf LittleFS und im Hub.
17. Die Ride-Seite ist auf einem Tablet in zwei Metern Abstand lesbar.

18. Ohne gewähltes Profil startet keine Session; ein Profilwechsel während einer
    Session wird abgelehnt.
19. Das Reha-Programm fährt 10 Minuten bei 60 W durch, ohne dass der Puls 120
    übersteigt. Der Deckel fängt im Anfahrband darunter an gegenzuhalten, und
    jeder Eingriff wird in der UI benannt und in der Session gezählt.
20. Die harten Profilgrenzen für Leistung, Puls und Stufe lassen sich von keinem
    Workout und keinem Modus überschreiten.
21. Bei Pulsverlust verhält sich das Gerät so, wie es im aktiven Profil steht —
    `reduce` senkt die Stufe, `freeze` hält sie.
22. Die Zonenschiene stimmt am Sessionende mit der Auswertung überein.

## 13. Abnahmekriterien Bridge

1. MyWhoosh findet den ESP32 als Trainer und verbindet.
2. Ein **ERG-Workout** in MyWhoosh steuert das Bike über den ESP32 durch —
   obwohl das Bike selbst kein Set Target Power kennt.
3. Die in MyWhoosh angezeigten Fahrwerte stimmen mit denen der ESP-UI überein.
4. Der Difficulty-Faktor wirkt nachweisbar auf die Stufe.
5. Der HR-Deckel greift, ohne die App-Verbindung zu stören.
6. Alle drei BLE-Links bleiben eine 45-Minuten-Session stabil.
7. Web-UI und SSE bleiben im Bridge-Betrieb bedienbar.

---

## 14. Risiken

| Risiko | Bewertung |
|--------|-----------|
| **Stufe 16 erreicht nur ~130 W** | **Projektkritisch.** Lineare Extrapolation aus den Laborpunkten legt das nahe. Dann trägt der Widerstandskanal nur Grundlagentraining und `0x11` wird Pflicht. Test 1 entscheidet. |
| Nur 16 Stufen als Stellgröße | ERG ist grundsätzlich quantisiert. Ehrlich in der UI zeigen statt exakte Zielwerte vorzutäuschen. |
| Kein Set Target Power | Erledigt durch Emulation, verlagert aber Aufwand in `PowerController` und Kalibrierung. |
| Keine Rückmeldung der Stufe | Schattenwert kann auseinanderlaufen. Nach jedem Connect Stufe neu setzen statt annehmen. |
| Drei BLE-Links am Ergo-Knoten | **Entschärft.** `heartrate-s3` fährt denselben Rollenmix mit `MAX_CONNECTIONS=3` produktiv, ohne PSRAM. Am Bike ist der Dual-Link trotzdem noch zu messen (Test 5); Rückfallebene ist der Puls aus `2AD2` und damit nur zwei Links. |
| Peripheral-Rolle für die Bridge | **Entschärft.** `HrServer` in heartrate v0.3 ist die laufende Vorlage samt Advertising, Client-Limit und Notify-Einspeisung aus dem Loop. |
| Verhalten des Bikes bei Client-Abbruch | Test 6 lief nie. Bis dahin Hub-Watchdog aus und `08 01` vor jedem Neustart. |
| Encoding auf einem Datenpunkt | `04 <sint16>` ruht auf einer belastbaren Messung. Der Sweep bestätigt es nebenbei. |
| `HR_HOLD` pendelt | Träger Puls plus grobes Stufenraster. Großes Totband, langsamer Zyklus. |
| Funklast WiFi + 3× BLE | Bekanntes Terrain aus heartrate, hier enger. |

## 15. Offene Punkte

Blockierend, siehe [NACHTESTS.md](NACHTESTS.md):

- **Test 1 Stufen-Sweep** — Leistungsbereich und Kennlinie
- **Test 2 Kadenzabhängigkeit** — Architektur von `PowerController`

Nicht blockierend, aber vor v0.2 zu klären:

- Test 3 Watt-Nachtest, Test 4 Simulation, Test 5 Dual-Link, Test 6 Crash
- **Reihenfolge v0.2 (Editor und Tests) gegen v0.3 (Bridge)**
- Baut v0.1 auch für den D1 Mini oder von Anfang an nur S3
- Session-Format: eigenes JSON oder gleich TCX/FIT
- Flash-Budget der UI: reicht PROGMEM, oder muss sie nach LittleFS? Wird am
  ersten Build gemessen.
- Ob die Rampe mit 20 W/min über 16 Stufen brauchbar fein ist oder auf `0x11`
  warten muss
- Ob die Kalibrierung pro Fahrer geführt wird (Masse beeinflusst nichts am
  Widerstand, aber die HR-Regelparameter schon)
- Ob der Relay-Knoten und der Ergo-Knoten getrennt bleiben oder ob `esp32.ergo`
  das Relay später selbst mitbringt. Getrennt ist sauberer — zwei Produkte, zwei
  Verantwortungen — kostet aber ein zweites Board.
- Wie träge der Puls über die 5-kHz-Kette ins `2AD2`-Feld kommt. Entscheidet, ob
  die budgetfreie Quelle für `HR_HOLD` taugt oder nur fürs Dashboard.

Erledigt seit Revision 2:

- Der H9 lässt nur einen BLE-Client zu — gelöst durch das HR-Relay in
  `esp32.heartrate` v0.3, das den Gurt hält und an MyWhoosh **und** den
  Ergo-Knoten weitergibt.
- MyWhoosh holt den Puls nicht vom Ergometer, sondern koppelt getrennt. Die
  Bridge muss `0x180D` also nicht zwingend mit anbieten.
