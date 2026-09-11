# Stand

**Dies ist der Einstiegspunkt.** Wer an diesem Projekt weiterarbeitet, liest
zuerst diese Datei und danach gezielt weiter. Alle anderen Notizen in `debug/`
sind Protokolle einzelner Arbeitsschritte und beschreiben den Stand *zu ihrem
Zeitpunkt* — sie werden nicht nachgeführt.

Stand dieser Datei: **2026-09-11** (Abend), Profilpflicht + Ride/Profil-UI auf `.88`.

**Wochenende:** Einstieg hier. Hand-Beweis: `tools/hand-proof.sh`.
Details: [debug/UPDATE_PROFILE_MODE.md](debug/UPDATE_PROFILE_MODE.md),
[debug/UPDATE_PROFILE_PERSIST.md](debug/UPDATE_PROFILE_PERSIST.md),
[debug/UPDATE_CONTROL_UI.md](debug/UPDATE_CONTROL_UI.md).

---

## 1. Was läuft, und wo

| | |
|---|---|
| Gerät | ESP32-S3 auf `192.168.178.88`, MAC `68B6B329339C`, `ergo 0.1.0-dev` |
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

## 2. Der Unterschied, auf den es ankommt

**Belegt** ist, dass Kommandos ankommen und quittiert werden.

**Nicht belegt** ist, dass eine gestellte Widerstandsstufe die Last tatsächlich
ändert.

Das ist keine Formalie. Die erste Hardware-Session hat lauter
Erfolgsquittungen gesehen und daraus geschlossen, der Schreibweg stehe. Er stand
nicht: `needsWideResistance()` leitete das Drahtformat aus dem *Stellbereich* ab,
der Varon-Bereich (10…160) passt in ein `uint8`, also ging die schmale Form
`04 64` hinaus — und genau die quittiert das Gerät laut
[GERAETEPROFIL.md](docs/ergometer/GERAETEPROFIL.md) §5 mit Success, ohne etwas zu
tun. Der Fix steht in [UPDATE_CAPS_FIX.md](debug/UPDATE_CAPS_FIX.md).

> **Eine Erfolgsquittung beweist nichts.** Ein Stellbereich kann keine
> Formatfrage beantworten. Wer von hier aus weiterarbeitet, sollte diesen Satz
> als Arbeitsregel behandeln und nicht als Anekdote.

Damit sich das nicht wiederholt, urteilt die Firmware seit diesem Zyklus selbst:
das **Steuer-Journal** (`src/control/ControlJournal.{h,cpp}`) bewertet jeden
Write kadenznormiert und benennt den Zustand „quittiert, aber wirkungslos" als
`contradictory()` — rot im Debug-Reiter, im Log als `[JRN] WIDERSPRUCH`.

## 3. Der eine offene Beweis — er braucht keinen Fahrer

Die Frage ist qualitativ, also genügt eine Hand an der Kurbel.

1. Flashen, Bike verbinden, `POST /api/control/request`
2. Mitschnitt einschalten (Debug-Reiter) — dann ist der Lauf hinterher
   auswertbar, auch wenn live niemand mitliest
3. Stufe 1 setzen, Kurbel ~20 s gleichmäßig von Hand drehen
4. Stufe 16 setzen, ~20 s gleichmäßig von Hand drehen
5. Ist der Widerstand von Hand deutlich verschieden? Das Journal sagt es
   zusätzlich selbst

Das Bike meldet auch im Handbetrieb Werte — im letzten Bericht 12 W bei 21 rpm.
Die Konsole taugt nicht als Rückmeldung: ihr Display ist aus, solange der
BLE-Link steht, und die Stufe meldet das Bike in `0x2AD2` nicht zurück.

Bleibt es bei „keine Wirkung" **mit** Erfolgsquittung, ist auch sint16 falsch.
Nächste Verdächtige dann: fehlende `Start/Resume`-Freigabe, oder ein Betriebsmodus
an der Konsole.

Alles Weitere hängt daran — Kalibrierung, `MANUAL_ERG`, `HR_HOLD`.

## 4. Was ohne Fahrer geht, und was nicht

| Ohne Fahrer | |
|---|---|
| Der Beweis aus §3 | Hand an der Kurbel |
| Reconnect | Bike aus, `LOST`, Bike an, `READY` ohne Neustart |
| Nachtest 5 Dual-Link | Gurt umlegen und zehn Minuten sitzen, nicht treten |
| Nachtest 6 Crash unter Last | hohe Stufe setzen, dem ESP den Strom ziehen, Kurbel von Hand prüfen. Entscheidet, ob der Hub-Watchdog überhaupt aktiv bleiben darf |

| Braucht einen Fahrer | |
|---|---|
| Nachtest 1 · 60 rpm | die Zahlen der Kennfläche, Reiter Kalibrierung |
| Nachtest 2 · 80 rpm | Kadenzabhängigkeit — Tabelle oder Regler |
| Nachtest 3 | Watt-Nachtest, bestätigt `0x05` als tot |

Nachtest 4 (`0x11` Simulation) braucht keinen Fahrer für die Quittung, aber einen
für die Wirkung — und `guardAllowSim` muss dafür von `false` auf `true`.

## 5. Nächste Schritte

1. ~~Kopieren und bauen.~~ / ~~Caps-Fix OTA.~~
2. **Der Beweis aus §3** (Hand an der Kurbel) — wenn du daheim bist:
   `tools/hand-proof.sh` oder Debug-Reiter + Stufe 1 vs 16.
3. Nachtest 1 und 2 mit Fahrer.
4. ~~**Profile mit Grenzen**~~ — RAM + API + Limiter; **NVS-Persistenz + UI-Reiter**
   (`debug/UPDATE_PROFILE_PERSIST.md`). LittleFS weiterhin optional.
5. ~~**Steuermodi OFF / MANUAL_LEVEL**~~ — gebaut; ERG/HR/WORKOUT warten auf §3.
   Profilpflicht + Ride-UI + Profil-Editor: [debug/UPDATE_CONTROL_UI.md](debug/UPDATE_CONTROL_UI.md).
6. ~~`.github/workflows/build.yml`~~ — native + `ergo`-Build.

Noch offen aus dem Hardware-Bericht: ein UI-Hinweis, dass nach Stop
`Start/Resume` plus erneuter Tritt nötig sein können. Wartet sinnvoll auf §3,
denn der zeigt nebenbei, ob `Start/Resume` am Varon überhaupt gebraucht wird.

## 6. Harte Regeln

1. **`/ota-upload` und die WiFi/Hub-Shell bleiben.** Jede Bin muss remote
   flashbar bleiben; `tools/deploy.sh` prüft die Strings und verweigert sonst.
2. **Kein `[env]`-Block** in `platformio.ini` — er würde von `env:native` geerbt
   und die Hosttests zerlegen. `[common]` plus `${common.x}`. Der
   `build_src_filter` von `env:native` ist eine **Positivliste** und muss es
   bleiben.
3. **Arduino-frei und hosttestbar bleiben:** `FtmsCodec`, `FtmsCapabilities`,
   `Limiter`, `PowerMap`, `SweepRunner`, `ControlJournal`, `DebugRing`,
   `ProfileStore`, `ControlMode`.
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
| 5 | **offen** — genau der Beweis aus §3 |
| 6 | Mechanik gebaut (Sweep, Kennfläche, verworfene Punkte werden als verworfen ausgewiesen), Messung fehlt |
| 6a | gebaut; der Fixture-Rundlauf über `tools/make-fixtures.py` ist noch nicht einmal durchgespielt |
| 6b | gebaut und hosttestbar; auf Hardware noch nicht gesehen |
| 7–12, 14–17, 19–21 | nicht angefangen |
| 13 | erfüllt (gewollter Neustart sendet `08 01`) |
| 18 | Profilpflicht + Wechsel-Lock gebaut (UI/API); Session-Begriff noch ohne Workout |

## 8. Welches Dokument beantwortet was

| Datei | Rolle |
|---|---|
| **diese Datei** | Stand, offene Beweise, nächste Schritte, harte Regeln |
| [PFLICHTENHEFT.md](docs/ergometer/PFLICHTENHEFT.md) | das Konzept, Rev. 4. Versionsplan §11, Abnahmekriterien §12. **Aktuell.** |
| [GERAETEPROFIL.md](docs/ergometer/GERAETEPROFIL.md) | was das Bike wirklich kann, gemessen. **Aktuell** — und war beim Drahtformat von Anfang an richtig. |
| [WEBINTERFACE.md](docs/ergometer/WEBINTERFACE.md) | Designkonzept der UI, Reiterfolge §7, Flash-Budget §8 |
| [NACHTESTS.md](docs/ergometer/NACHTESTS.md) | die sechs Messungen und was jede entscheidet. Die Kommandos darin beschreiben noch die Sonde, siehe Hinweis im Kopf der Datei. |
| [BLE-SCAN.md](docs/ergometer/BLE-SCAN.md) | erster Protokolltest, erledigt. Vorlage für weitere Geräte. |
| [debug/UPDATE_CAPS_FIX.md](debug/UPDATE_CAPS_FIX.md) | **jüngste Übergabe**: der Fix, Debug-Modus, Steuer-Journal, Kopierliste |
| [debug/HW_TEST_REPORT.md](debug/HW_TEST_REPORT.md) | Protokoll der Hardware-Session 2026-09-11 |
| `debug/*` sonst | Protokolle älterer Arbeitsschritte, historisch |

## 9. Offene Entscheidungen

- **Reihenfolge v0.2 gegen v0.3.** Die einzige echte Scope-Frage, siehe
  PFLICHTENHEFT §11 am Ende. Editor und Tests setzen direkt auf v0.1 auf;
  MyWhoosh ist die Motivation, die erhalten bleiben soll.
- **Wandert die UI nach LittleFS?** Zuletzt gemessen 64,9 % Flash bei 33977 Byte
  UI-Seite; die Seite ist inzwischen 40487 Byte. WEBINTERFACE.md §8: diese
  Entscheidung wird gemessen, nicht geraten. Derzeit nicht dringend.
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
