# BLE-Scan — Checkliste für ein unbekanntes Ergometer

> **Historischer Lauf Varon XTR II:** 2026-09-10 mit der Sonde
> [`esp32.ftmsprobe`](https://github.com/MPunktBPunkt/esp32.ftmsprobe).
> Ergebnis: **Ausgang 1** — Standard-FTMS, Control Point offen, kein FitShow,
> aber **kein** Set Target Power. Details: [GERAETEPROFIL.md](GERAETEPROFIL.md).
> Nachtests: [NACHTESTS.md](NACHTESTS.md). Diese Datei ist die **Vorlage** für
> weitere Geräte.

Ziel: in wenigen Minuten entscheiden, ob der Coach auf Standard-FTMS aufsetzen
kann oder ob ein proprietäres Protokoll (z. B. FitShow) nötig wird.

Werkzeug: **nRF Connect for Mobile** oder die Laborsonde. Konsole einschalten
und **treten**, sonst sendet das Gerät oft keine Werte. Andere Apps
(MyWhoosh, Kinomap) vorher schließen — viele Bikes erlauben nur ein Central.

---

## Schritt 1 — Scannen

**Notieren:** Advertising-Name, MAC, beworbene Service-UUIDs.

Typische Namen: `FS-…`, `HAMMER…`, `VARON…`, `iConsole…`, oder nur MAC.

## Schritt 2 — Verbinden und Services auflisten

| UUID | Bedeutung | Erwartung |
|------|-----------|-----------|
| `0x1826` | Fitness Machine (FTMS) | Jackpot |
| `0x1818` | Cycling Power | oft parallel |
| `0x1816` | CSC | |
| `0x180D` | Heart Rate | eher selten am Bike |
| `0xFFF0` / `0xFFE0` / `0xFF00` | proprietär (FitShow o. ä.) | Fallback |

**Notieren:** alle Service-UUIDs inkl. 128-Bit-Vendor-UUIDs.

## Schritt 3 — Werte lesen (nur wenn `0x1826` da ist)

| UUID | Name | Was tun |
|------|------|---------|
| `0x2ACC` | Feature | Read → Bytes |
| `0x2AD2` | Indoor Bike Data | Notify an, treten, Bytes |
| `0x2AD6` | Resistance Range | Read |
| `0x2AD8` | Power Range | Read |
| `0x2AD9` | Control Point | Properties (Write? Indicate?) |
| `0x2ADA` | Status | Notify |

## Schritt 4 — Der entscheidende Test

Am Control Point `0x2AD9` zuerst **Indications aktivieren**, dann schreiben:

| # | Schreiben | Erwartete Antwort | Bedeutung |
|---|-----------|-------------------|-----------|
| 1 | `00` | `80 00 01` | Steuerung freigegeben |
| 2 | `07` | `80 07 01` | Start/Resume |
| 3 | `05 64 00` | `80 05 01` | Ziel 100 W (sint16 LE) |
| 4 | `04 0A` | `80 04 01` | Stufe 10 (1-Byte-Form) |
| 5 | `08 01` | `80 08 01` | Stop/Pause |

Bei 3 und 4 **mittreten** und prüfen, ob der Widerstand **wirklich** anzieht.
`80 xx 01` allein heißt nur „verstanden“.

**Varon-Befund (zur Orientierung):** `05` wird mit Success quittiert und wirkt
**nicht** (NO_EFFECT). `04 0A` (1 Byte) wird quittiert und wirkt **nicht**; die
wirkende Form ist sint16 in Zehnteln, z. B. `04 64 00` für Stufe 10,0 — siehe
[GERAETEPROFIL.md](GERAETEPROFIL.md) und [UPDATE_CAPS_FIX.md](../../debug/UPDATE_CAPS_FIX.md).

Falls `04 0A` nicht greift: sint16-Variante testen. Beide Formen notieren.

---

## Was zurückmelden

1. Advertising-Name und beworbene Services  
2. Ist `0x1826` vorhanden?  
3. Bytes von `0x2ACC`, `0x2AD6`, `0x2AD8`  
4. Einige `0x2AD2`-Notify-Pakete beim Treten (Rohbytes)  
5. Antwort auf `00` am Control Point  
6. Zieht der Widerstand bei `05 …` (Watt) und/oder `04 …` (Stufe) an?

Aus diesen Punkten folgen Parser und Steuerstrategie (`FtmsCapabilities`).

---

## Die drei möglichen Ergebnisse

| Ergebnis | Folge |
|----------|--------|
| `0x1826` da, `00` → `80 00 01`, Stufe wirkt | Standard-FTMS. Coach wie geplant; Bridge (v0.3) möglich, falls Watt fehlt → Emulation. |
| `0x1826` da, Control Point verweigert | Lesen ja, Steuern nein. Nur Dashboard, oder proprietären Weg prüfen. |
| Nur proprietäre Services | FitShow-Fall — Mitschnitt nötig. Referenz: `fitshowbike` in [qdomyos-zwift](https://github.com/cagnulein/qdomyos-zwift). |

## Nebenbei: Pulsgurt

H9 o. ä.: `0x180D` → `0x2A37` Notify. Unabhängig vom Bike-Protokoll; im
Projektfamilie schon über `esp32.heartrate` / `HrClient` abgedeckt.
