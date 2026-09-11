# Update — Connectivity-Shell (WiFi + Web + OTA + Hub)

Antwort auf `TODO_WIFI_OTA.md`. Der Einwand dort war richtig und der Grund, warum
diese Schicht vor `BleCentral` kommt: die Bin, die zuletzt im Repo lag, baute
sauber, hatte aber keinen HTTP-Server. Ein OTA-Flash auf `.88` wäre ein
Remote-Brick gewesen.

Diese Instanz kann nicht bauen (nur `git`, kein PlatformIO, keine Toolchain).
Der Abschnitt **Ergebnis** unten ist deshalb leer und von der Build-Instanz
auszufüllen.

## Was implementiert wurde

| Datei | Inhalt |
| --- | --- |
| `src/main.cpp` | nur noch `App::instance().begin()` / `.loop()` |
| `src/app/App.{h,cpp}` | Orchestrierung, Web-Routen, OTA-Handler, Reset-Taste, Watchdog |
| `src/core/ConfigStore.{h,cpp}` | NVS `esphub` (geteilt) + `ergo` (versioniert, `cfg_ver = 1`) |
| `src/core/HubClient.{h,cpp}` | unverändert aus heartrate, inkl. `otaUrl`-Pull |
| `src/core/NetUtil.{h,cpp}` | wie heartrate, plus `toHex()` für den späteren Debug-Modus |
| `src/web/UiPages.h` | eine Seite: Status + OTA, Farbsystem aus `WEBINTERFACE.md`, ~5 kB |
| `tools/deploy.sh` | Build, Familien-Bin-Name, OTA direkt oder über den Hub |

`src/ble/*` und `src/control/*` sind **unangetastet**. Der Limiter ist weiterhin
nicht verdrahtet — bewusst, es gibt noch keinen Schreibpfad, den er schützen
könnte.

### Definition of Done, Punkt für Punkt

1. WiFiManager-Portal `ESP-Ergo-Setup`, Parameter `name` / `hub_host` / `hub_port`, danach STA + NTP, `WiFi.setSleep(true)` — **ja**
2. mDNS `ergo-XXXXXX.local` (letzte 6 Hex der MAC) — **ja**
3. `GET /`, `GET /ota` (dieselbe Seite), `GET /api/status`, `POST /ota-upload`, dazu `/events`, `/api/config/get`, `/api/config/save`, `/api/system/restart` — **ja**
4. Heartbeat `POST /api/register` mit `fwType` aus `FW_TYPE`, `ios.ergo_state = "SHELL"` — **ja**
5. BOOT/GPIO0 3 s → `wm.resetSettings()` + `config.factoryReset()` + Restart — **ja**
6. Codec-Selbsttest bleibt, jetzt als Feld `codecSelfTest` in `/api/status` statt nur auf Serial — **ja**
7. `pio run -e ergo` und `pio test -e native` — **von der Build-Instanz zu bestätigen**
8. Bin-Name `ergo.<version>.esp32s3.bin` — über `tools/deploy.sh`, nicht über `extra_scripts` (ein Post-Hook, der bei jedem Build nach `dist/` kopiert, macht aus einem Zwischenbuild ein Artefakt, das nach Release aussieht)

### Abweichungen von der Anweisung

- **`/api/status` enthält mehr als das Minimum**: zusätzlich `chip`, `freeSketch`, `uptime` als lesbarer String und `time`. Das Feld `uptimeS` bleibt daneben stehen.
- **Kein `HistoryStore`, kein `SessionArchive`, kein LittleFS.** Ohne Messwerte gibt es nichts zu speichern.
- **`ConfigStore` ist absichtlich schmal** — zehn Felder, `cfg_ver = 1`. Neue Felder kommen mit dem Baustein, der sie braucht; fehlende Schlüssel behalten ihren Default, also kostet das keinen Factory-Reset.
- **Der Selbsttest prüft scharf statt zu protokollieren.** Er dekodiert das Level-10-Paket und vergleicht sechs Werte gegen die Aufzeichnung. Auf dem Host ist das redundant, auf dem S3 nicht: Ausrichtung, Endianness und `int`-Breite sind dort andere.

## Der Punkt, auf den es beim Flashen ankommt

`tools/deploy.sh` prüft die **neue** Bin, bevor sie hochgeht:

```bash
grep -aqF '/ota-upload' dist/ergo.0.1.0-dev.esp32s3.bin \
  || die "kein Rückweg in der Bin"
```

Das ist genau der Test, den Punkt 2 des Flash-Plans verlangt, nur nicht als
Handgriff, den man vergessen kann. Zusätzlich bricht das Skript ab, wenn das
Zielgerät `"bikeLink": true` meldet — das Feld existiert noch nicht, ab
`BleCentral` greift die Sperre automatisch.

Rollback-Bin der Sonde vorher bereitlegen:

```bash
cd /home/martin/projects/esphub/nodes/esp32.ftmsprobe && ./tools/deploy.sh --build-only
```

## Ergebnis

Ausgefüllt von der Build-Instanz am 2026-09-11, Commit `bc3d042`.

```text
pio test -e native
  test_codec    22/22 PASSED
  test_limiter  21/21 PASSED
  → 43/43 in ~1.4 s

pio run -e ergo
  → SUCCESS (~26 s)
  RAM:   48576 / 327680  (14.8 %)
  Flash: 1017157 / 1966080 (51.7 %)
  Bin:   .pio/build/ergo/firmware.bin  (1017520 bytes)
```

OTA-Routen in der Bin vorhanden (`/ota-upload` in App.cpp + UiPages) —
`tools/deploy.sh` darf also theoretisch flashen. **Noch nicht auf `.88` ausgerollt.**

Erster OTA-Versuch auf `.88`:

- [ ] `curl http://192.168.178.88/api/status` vor dem Flash (laufende Version notieren)
- [ ] `tools/deploy.sh --ota 192.168.178.88`
- [ ] nach dem Reboot `fwType = ergo`, `codecSelfTest = ok`
- [ ] Gerät im Hub sichtbar
- [ ] zweiter Roundtrip Ergo → Ergo (beweist Selbst-Recovery)
- [ ] `http://ergo-XXXXXX.local/` erreichbar

## Noch nicht im Repo

`.github/workflows/build.yml` liegt lokal, ist aber nie hochgekommen — deshalb
ist das Build-Badge im README 404. Jobs: `codec` (`pio test -e native`), danach
`firmware` (`pio run -e ergo`) mit Artefakt-Upload.

## Danach

`BleCentral` + `FtmsClient`: Scan und Connect für zwei Rollen (Bike, Gurt),
GATT-Discovery von `0x1826`, `0x2ACC`/`0x2AD6`/`0x2AD8` in `ftms::Capabilities`,
Notify auf `0x2AD2` und `0x2AD9` — und ab dann jeder Write ausschließlich durch
den Limiter.
