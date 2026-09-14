# `docs/ergometer/` — Kanondokumente

Konzept und Beschreibung des gebauten Systems für `esp32.ergo`.

> **Lebender Stand:** [`STATE.md`](../../STATE.md)  
> **Versionsgeschichte:** [`CHANGELOG.md`](../../CHANGELOG.md)  
> **Technik:** [`ENTWICKLERDOKU.md`](ENTWICKLERDOKU.md)

| Datei | Inhalt |
|-------|--------|
| [ENTWICKLERDOKU.md](ENTWICKLERDOKU.md) | Stack, Persistenz, Sicherheit, Regelung, Glossar |
| [KALIBRIERUNG.md](KALIBRIERUNG.md) | aktuelle Kennfläche (Tabelle + JSON) |
| [kalibrierung-map-20260913.json](kalibrierung-map-20260913.json) | Snapshot zur Kennfläche |
| [PFLICHTENHEFT.md](PFLICHTENHEFT.md) | Konzept (was werden soll), Rev. 4 |
| [BRIDGE.md](BRIDGE.md) | Bridge: Observer/Controller, Exclusive, MyWhoosh |
| [WEBINTERFACE.md](WEBINTERFACE.md) | UI-Designkonzept |
| [BEDIENUNG.md](BEDIENUNG.md) | kurze Bedienung für Nutzer |
| [GERAETEPROFIL.md](GERAETEPROFIL.md) | gemessenes Bike-Verhalten |
| [NACHTESTS.md](NACHTESTS.md) | Nachtest-Ergebnisse (Test 6 offen) |
| [BLE-SCAN.md](BLE-SCAN.md) | Scan-Vorlage für weitere Geräte |

## Stand (Kurz)

Vier Befunde tragen die Architektur (Labor + Sweeps):

1. Standard-FTMS, Control Point offen — kein FitShow.
2. Kein Set Target Power — Watt = Emulation über die Stufe.
3. Stufe → Watt ist eine **Fläche** (Kadenzabhängig).
4. Gemessen u. a.: ≈ 170 W @ Stufe 16 / 60 rpm; ≈ 245 W @ 80 rpm.

`HrServer` (heartrate) **war** die Vorlage für `FtmsServer` (Bridge).
