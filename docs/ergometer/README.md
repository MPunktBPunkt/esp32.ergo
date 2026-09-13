# `esp32.ergo` — Planung und Entwicklerdoku

Steuerung des HAMMER Varon XTR II (BLE-Name `TC174`) über BLE/FTMS, verteilt
über den ESP-Hub wie die übrigen Projekte der Familie.

> **Lebender Stand** (was gebaut *ist*): [`STATE.md`](../../STATE.md).  
> **Technische Gesamtschau** (Probe-Weg, GATT, Kalibrierung, Entscheidungen):
> [`ENTWICKLERDOKU.md`](ENTWICKLERDOKU.md).  
> Die übrigen Dateien hier sind Konzept bzw. vertiefende Kanondocs.

| Datei | Inhalt |
|-------|--------|
| [ENTWICKLERDOKU.md](ENTWICKLERDOKU.md) | **Einstieg Technik:** Stack, Probe, Bike/H9-Daten, Debug-Entscheidungen, Kalibriertabellen |
| [KALIBRIERUNG.md](KALIBRIERUNG.md) | Aktuelle Kennfläche (Heatmap-Tabelle + JSON-Snapshot) |
| [PFLICHTENHEFT.md](PFLICHTENHEFT.md) | Das Konzept. Revision 4. |
| [BRIDGE.md](BRIDGE.md) | Bridge-Betrieb: Observer/Controller, Exklusiv-Steuerung, MyWhoosh |
| [WEBINTERFACE.md](WEBINTERFACE.md) | Designkonzept der WebUI: Zonen, Profile, Workout-Editor, Tests |
| [GERAETEPROFIL.md](GERAETEPROFIL.md) | Was das Bike wirklich kann — gemessen, plus Auswertung der Bedienungsanleitung |
| [NACHTESTS.md](NACHTESTS.md) | Sechs Messfragen und Entscheidungslogik |
| [BLE-SCAN.md](BLE-SCAN.md) | Erster Protokolltest (erledigt), Vorlage für weitere Geräte |

## Stand

Der Protokolltest ist gelaufen. Werkzeug war die eigens gebaute Sonde
[`esp32.ftmsprobe`](https://github.com/MPunktBPunkt/esp32.ftmsprobe), deren
BLE-Schicht inzwischen als `BleCentral` plus `FtmsClient` in `esp32.ergo`
steht — die Firmware läuft auf Hardware, siehe [`STATE.md`](../../STATE.md)
und die [ENTWICKLERDOKU](ENTWICKLERDOKU.md).

Vier Befunde bestimmen alles Weitere:

1. **Standard-FTMS, Control Point offen.** Kein FitShow. Die Grundannahme trägt.
2. **Kein Set Target Power.** Die einzige Stellgröße ist die Widerstandsstufe —
   16 diskrete Schritte. Alles Watt-basierte muss der ESP32 emulieren.
3. **Stufe → Watt ist eine Fläche, keine Kurve.** Bei fester Stufe steigt die
   Leistung mit der Kadenz (Test 1 @ 60 rpm, Test 2 @ 80 rpm gemessen).
4. **Leistungsbereich gemessen.** @ 60 rpm Stufe 16 ≈ 170 W; @ 80 rpm ≈ 245 W.
   Spitzen darüber: höhere Kadenz und/oder Simulation `0x11`.

## Topologie

`esp32.heartrate` v0.3 hat ein HR-Relay bekommen. Das löst zwei Probleme auf
einmal: der Polar H9 erlaubt nur einen BLE-Client, und MyWhoosh will den Puls
getrennt gekoppelt haben statt über das Ergometer.

```
Polar H9 ──BLE──▶ ESP #1  heartrate (Relay)  ──BLE──▶ MyWhoosh   (Puls)
                     │                       └──BLE──▶ ESP #2     (Puls)
                     └── 5-kHz-GymLink ──▶ Bike-Konsole (HR-Feld in 2AD2)

Varon XTR II ──BLE──▶ ESP #2  ergo  ──BLE Peripheral──▶ MyWhoosh (FTMS)
                         │
                         └── WiFi ──▶ WebUI + ioBroker-Hub
```

Nebenbei ist damit die Peripheral-Rolle in der Familie erprobt: `heartrate-s3`
fährt drei BLE-Links plus WiFi plus SSE-UI, und das ohne PSRAM. `HrServer` ist
die direkte Vorlage für `FtmsServer`.
