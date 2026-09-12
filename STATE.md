# Stand

**Dies ist der Einstiegspunkt.** Wer an diesem Projekt weiterarbeitet, liest
zuerst diese Datei und danach gezielt weiter. Alle anderen Notizen in `debug/`
sind Protokolle einzelner Arbeitsschritte und beschreiben den Stand *zu ihrem
Zeitpunkt* — sie werden nicht nachgeführt.

Stand dieser Datei: **2026-09-12** Release **v0.1.0**
([debug/RELEASE_v0.1.0.md](debug/RELEASE_v0.1.0.md)).

---

## 1. Was läuft, und wo

| | |
|---|---|
| Gerät | ESP32-S3 auf `192.168.178.88`, MAC `68B6B329339C`, `ergo 0.1.0` |
| Bike | Hammer Varon XTR II, BLE-Name `TC174`, MAC `c2:32:a5:1e:bf:b5` |
| Hub | `192.168.178.113:8093`, `fwType: ergo` |
| Rollback-Bin | `nodes/esp32.ftmsprobe/dist/ftmsprobe.0.1.4.esp32s3.bin` |
| Build-Host | Debian, `/home/martin/projects/esphub/esp32.ergo`, `pio` unter `/home/martin/.venvs/pio/bin/pio` |

Die Entwurfs-Instanz auf Windows hat **nur git** — kein PlatformIO, keinen
Compiler, kein Python. Sie kann nicht bauen und nicht testen. Bauen und Flashen
macht die Build-Instanz, Messen macht ein Mensch am Rad.

Auf Hardware bewährt: WLAN, Web-UI, OTA in beide Richtungen, Hub-Heartbeat,
BLE-Central mit zwei Rollen, FTMS-Client mit Capability-Ableitung, Handsteuerung
über den Limiter, Sweep-Abbruch bei fehlender Kadenz.

## 2. Stufenwirkung — erledigt (2026-09-12)

**Belegt:** `0x04` sint16 wirkt messbar. Journal: WORKS, **0 Widersprüche**.
W/rpm ~1,0@6 → ~3,1@16 → ~0,84@4. Details:
[debug/RELEASE_v0.1.0.md](debug/RELEASE_v0.1.0.md).

Historisch (Caps-Fix): die schmale Form `04 64` wurde quittiert ohne Wirkung —
siehe [UPDATE_CAPS_FIX.md](debug/UPDATE_CAPS_FIX.md). Arbeitsregel bleibt:
**Eine Erfolgsquittung beweist nichts.**

## 3. Hardware 2026-09-12 (Kurz)

- LEVEL-Session und Builtin-Rampe (`done`, 480 s, Puls max 149) gefahren
- Profile Martin + Manu; Kopfzeile/Form vereinfacht
- Flash ~75 %; UI-Seite groß — LittleFS vor weiterem Wachstum empfohlen

## 4. Was ohne Fahrer noch lohnt

| Ohne Fahrer | |
|---|---|
| Reconnect | Bike aus → `LOST` → an → `READY` |
| Nachtest 5 Dual-Link | Gurt umlegen, sitzen |
| Nachtest 6 Crash unter Last | entscheidet Hub-Watchdog |

| Braucht Fahrer | |
|---|---|
| Dichtere Kennfläche / voller Test 2 | optional |
| Nachtest 3 | `0x05` tot |
| ERG/HR/Reha-Abnahme | Mechanik da |

Nachtest 4 (`0x11`): Quittung ohne Fahrer, Wirkung mit Fahrer; `guardAllowSim`.

## 5. Nächste Schritte (nach v0.1.0)

1. Flash-/UI-Budget (LittleFS oder schlanke Seite)
2. TestRunner (echtes MAP / 20 min / Recovery)
3. Optional: Nachtest 4 / dichtere Map / Interval-Editor
4. Entscheidung v0.2 vs v0.3 (Bridge)

Erledigt bis v0.1.0: Caps-Fix, Kalibrierung leicht, ERG/HR/Reha, Session,
Zonen, Profile, Workout-UI/Editor, Tests-UI, Tablet-Ride, Progression, CI.

## 6. Harte Regeln

1. **`/ota-upload` und die WiFi/Hub-Shell bleiben.** Jede Bin muss remote
   flashbar bleiben; `tools/deploy.sh` prüft die Strings und verweigert sonst.
2. **Kein `[env]`-Block** in `platformio.ini` — er würde von `env:native` geerbt
   und die Hosttests zerlegen. `[common]` plus `${common.x}`. Der
   `build_src_filter` von `env:native` ist eine **Positivliste** und muss es
   bleiben.
3. **Arduino-frei und hosttestbar bleiben:** `FtmsCodec`, `FtmsCapabilities`,
   `Limiter`, `PowerMap`, `SweepRunner`, `ControlJournal`, `DebugRing`,
   `ProfileStore`, `ControlMode`, `PowerController`, `HrController`,
   `RehaController`, `WorkoutEngine`, `WorkoutJson`, `SessionTracker`,
   `SessionStore`, `Zone`, `Progression`.
4. **Jeder FTMS-Write nur durch den Limiter.** Es gibt keine öffentliche Methode,
   die rohe Bytes an den Control Point schreibt; alles läuft durch
   `FtmsClient::send()`. Ein Bypass müsste die Klasse ändern, nicht sie nur
   falsch benutzen.
5. **Kein Steuerweg über `0x05`** Set Target Power am Varon. Das Feature-Bit
   fehlt, die Quittung lügt.
6. **Hub-Watchdog:** steht ein Bike-Link, kein Blind-Restart unter Last — erst
   `08 01`. Ob das reicht, entscheidet Nachtest 6.
7. Im NimBLE-Callback wird **nicht** gelesen, gerechnet oder geurteilt. Ein
   GATT-Read dort blockiert den Host-Task und holt den Watchdog. Deshalb
   `pendingUp_`-Flags und Verarbeitung in `loop()`; deshalb sitzt im Callback nur
   das `memcpy` des Rohbyte-Rings.

## 7. Abnahmekriterien v0.1 — Stand

Die Liste selbst steht in [PFLICHTENHEFT.md](docs/ergometer/PFLICHTENHEFT.md) §12.

| Nr. | Stand |
|---|---|
| 1–4 | erfüllt und auf Hardware gesehen |
| 5 | **erfüllt** (Test 1: linear bis 170 W @ Stufe 16; Journal WORKS) |
| 6 | **erfüllt für Architektur** (60 rpm voll + 80 rpm leicht); dichtere Map optional |
| 6a | **erfüllt** (Host + Live-Export + CI `--verify-curated`) |
| 6b | gebaut; Rampen-Stub auf Hardware gefahren (kein volles MAP) |
| 7 | **Mechanik da** (MANUAL_ERG); Abnahme mit Fahrer offen |
| 8 | Ceiling-Flag + UI (Hero rot, Ist/Ziel-Hinweis) |
| 13 | erfüllt (gewollter Neustart sendet `08 01`) |
| 18 | Profile + Sessions + Workouts + Progression auf Hardware genutzt |

## 8. Welches Dokument beantwortet was

| Datei | Rolle |
|---|---|
| **diese Datei** | Stand, offene Beweise, nächste Schritte, harte Regeln |
| [PFLICHTENHEFT.md](docs/ergometer/PFLICHTENHEFT.md) | das Konzept, Rev. 4. Versionsplan §11, Abnahmekriterien §12. **Aktuell.** |
| [GERAETEPROFIL.md](docs/ergometer/GERAETEPROFIL.md) | was das Bike wirklich kann, gemessen. **Aktuell** — und war beim Drahtformat von Anfang an richtig. |
| [WEBINTERFACE.md](docs/ergometer/WEBINTERFACE.md) | Designkonzept der UI, Reiterfolge §7, Flash-Budget §8 |
| [NACHTESTS.md](docs/ergometer/NACHTESTS.md) | die sechs Messungen und was jede entscheidet. Die Kommandos darin beschreiben noch die Sonde, siehe Hinweis im Kopf der Datei. |
| [BLE-SCAN.md](docs/ergometer/BLE-SCAN.md) | erster Protokolltest, erledigt. Vorlage für weitere Geräte. |
| [debug/RELEASE_v0.1.0.md](debug/RELEASE_v0.1.0.md) | **Release-Stand + Testergebnisse v0.1.0** |
| [debug/UPDATE_CAPS_FIX.md](debug/UPDATE_CAPS_FIX.md) | Caps-Fix, Debug-Modus, Steuer-Journal |
| [debug/HW_TEST_REPORT.md](debug/HW_TEST_REPORT.md) | Hardware-Session 2026-09-11 |
| `debug/*` sonst | Protokolle älterer Arbeitsschritte, historisch |

## 9. Offene Entscheidungen

- **Reihenfolge v0.2 gegen v0.3.** Die einzige echte Scope-Frage, siehe
  PFLICHTENHEFT §11 am Ende. Editor und Tests setzen direkt auf v0.1 auf;
  MyWhoosh ist die Motivation, die erhalten bleiben soll.
- **Wandert die UI nach LittleFS?** Flash ~75 %, UI-Seite ~110 kB Quelle —
  vor TestRunner/v0.2-Wachstum dringend messen und entscheiden
  (WEBINTERFACE.md §8).
- **Gerätename.** Das gemeinsame NVS `esphub` hält noch `FtmsProbe-S3`.
  Kosmetisch.
- **mDNS** ist vom Build-Host nicht auflösbar. Bisher nur lästig.

## 10. Git

Von der Entwurfs-Instanz aus wird **nicht** committet und **nicht** gepusht.
Änderungen werden gelesen, verglichen und als Kopierliste übergeben; das
Einpflegen macht der Mensch.

Wer von einer anderen Maschine committet: `git config --global user.email` steht
auf einer Arbeitgeberadresse. Für dieses Projekt repo-lokal auf
`martin@bchmnn.de` setzen, sonst landet die Firmenadresse dauerhaft in der
öffentlichen Historie eines GPL-Projekts.
