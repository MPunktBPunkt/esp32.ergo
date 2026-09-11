# BLE-Scan Varon XTR II — Checkliste

> **Erledigt am 2026-09-10.** Ausgeführt nicht per nRF Connect, sondern mit der
> eigens gebauten Sonde [`esp32.ftmsprobe`](https://github.com/MPunktBPunkt/esp32.ftmsprobe).
> Ergebnis: **Ausgang 1** — Standard-FTMS mit offenem Control Point, kein FitShow.
> Aber ohne Set Target Power, siehe [GERAETEPROFIL.md](GERAETEPROFIL.md).
> Offen gebliebene Messungen: [NACHTESTS.md](NACHTESTS.md).
> Diese Datei bleibt als Vorlage für weitere Geräte stehen.

Fünf Minuten am Bike. Das Ergebnis entscheidet, ob `esp32.ergo` auf Standard-FTMS
aufsetzen kann oder ob FitShow reverse-engineert werden muss.

App: **nRF Connect for Mobile** (Nordic, Android/iOS, kostenlos).

Wichtig: Konsole einschalten und **treten**, damit sie Werte sendet. Solange eine
App verbunden ist, schaltet das Konsolendisplay laut Anleitung ab — das ist normal.
Vorher MyWhoosh und Kinomap schließen, sonst ist die Verbindung belegt.

---

## Schritt 1 — Scannen

Bike suchen. Namen, die hier auftauchen können: `FS-…`, `HAMMER…`, `VARON…`,
`iConsole…`, oder eine reine MAC.

**Notieren:** Advertising-Name, MAC, beworbene Service-UUIDs.

## Schritt 2 — Verbinden und Services auflisten

Nach `CONNECT` die Service-Liste durchsehen. Interessant sind:

| UUID | Bedeutung | Erwartung |
|------|-----------|-----------|
| `0x1826` | **Fitness Machine Service (FTMS)** | Das ist der Jackpot |
| `0x1818` | Cycling Power Service | Leistung, oft parallel |
| `0x1816` | Cycling Speed and Cadence | |
| `0x180D` | Heart Rate | eher nicht, Bike hat 5-kHz-Empfänger |
| `0xFFF0` / `0xFFE0` / `0xFF00` | proprietär, typisch **FitShow** | Fallback-Fall |

**Notieren:** alle Service-UUIDs, auch die 128-Bit-Vendor-UUIDs.

## Schritt 3 — Werte lesen (nur wenn `0x1826` da ist)

In `0x1826` diese Characteristics prüfen:

| UUID | Name | Was tun |
|------|------|---------|
| `0x2ACC` | Fitness Machine Feature | **Read** → Bytes notieren |
| `0x2AD2` | Indoor Bike Data | **Notify** an, treten, Bytes notieren |
| `0x2AD6` | Supported Resistance Level Range | **Read** → Bytes notieren |
| `0x2AD8` | Supported Power Range | **Read** → Bytes notieren |
| `0x2AD9` | Control Point | Properties notieren (Write? Indicate?) |
| `0x2ADA` | Fitness Machine Status | Notify an |

## Schritt 4 — Der entscheidende Test

Am Control Point `0x2AD9` zuerst **Indications aktivieren** (Doppelpfeil-Symbol),
dann schreiben:

| # | Schreiben | Erwartete Antwort | Bedeutung |
|---|-----------|-------------------|-----------|
| 1 | `00` | `80 00 01` | **Steuerung freigegeben** |
| 2 | `07` | `80 07 01` | Start/Resume |
| 3 | `05 64 00` | `80 05 01` | Ziel **100 Watt** (sint16 little endian) |
| 4 | `04 0A` | `80 04 01` | Ziel Widerstandsstufe 10 |
| 5 | `08 01` | `80 08 01` | Stop/Pause |

Bei Schritt 3 und 4 **mittreten** und beobachten, ob der Widerstand wirklich
anzieht. Die Antwort `80 xx 01` allein heißt nur „Kommando verstanden".

Kommt bei Schritt 1 etwas anderes als `80 00 01` — etwa `80 00 02`
(Op Code not supported) oder `80 00 04` (Control not permitted) — hat Hammer die
Steuerung auf die eigene App gesperrt.

Falls `04 0A` nicht greift: manche Geräte erwarten den Widerstand als sint16 in
0,1er-Schritten, also `04 64 00` für Stufe 10,0. Beide Varianten testen.

---

## Was ich brauche

Bitte diese sechs Punkte zurückmelden:

1. Advertising-Name und beworbene Services
2. Ist `0x1826` vorhanden?
3. Bytes von `0x2ACC`, `0x2AD6`, `0x2AD8`
4. Ein paar Notify-Pakete von `0x2AD2` beim Treten (Rohbytes)
5. Antwort auf `00` am Control Point
6. Zieht der Widerstand bei `05 64 00` (Watt) und/oder `04 …` (Stufe) an?

Daraus leite ich den Parser und die Steuerlogik ab.

---

## Die drei möglichen Ergebnisse

| Ergebnis | Folge für `esp32.ergo` |
|----------|------------------------|
| `0x1826` da, `00` → `80 00 01`, Widerstand reagiert | **Bester Fall.** Standard-FTMS, v0.1 wie geplant, Bridge in v0.2 unkompliziert. |
| `0x1826` da, aber Control Point verweigert | Lesen ja, Steuern nein. Dann nur Dashboard + Coaching per Anzeige, keine ERG-Regelung. Alternativ FitShow-Weg prüfen. |
| Nur proprietäre Services (`0xFFF0` o. ä.) | **FitShow-Fall.** Protokoll muss mitgeschnitten werden — deutlich mehr Aufwand. Referenz: `fitshowbike` in [qdomyos-zwift](https://github.com/cagnulein/qdomyos-zwift). |

## Nebenbei: Polar H9 gegenchecken

Wenn du schon in nRF Connect bist: H9 anlegen, `0x180D` → `0x2A37` Notify.
Das ist schon im heartrate-Projekt implementiert und dient nur der Bestätigung,
dass beide Geräte gleichzeitig scanbar sind.
