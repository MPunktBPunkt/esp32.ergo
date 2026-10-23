# Geräteprofil Hammer Varon XTR II — gemessen

Quelle: Laborlauf 2026-09-10 mit [`esp32.ftmsprobe`](https://github.com/MPunktBPunkt/esp32.ftmsprobe),
Rohdaten in `docs/ergometer/scan-20260910/` jenes Repos.

Diese Datei ist die verdichtete Fassung für `esp32.ergo` — nur das, was gemessen
wurde, plus die Stellen, an denen die Messung die Erwartung widerlegt hat.

---

## 1. Identität

| Feld | Wert |
|------|------|
| BLE-Name (`2A00`) | `TC174` |
| MAC | `c2:32:a5:1e:bf:b5`, AddrType 0 (Public) |
| Manufacturer (`2A29`) | `ASR` |
| Model (`2A24`) | `BLE-1.0` |
| Firmware / Software | `6.1.2` / `6.3.0` |
| Vendor-Info (`2C01`) | `app-1.3.4-20211216.0001` |

„Hammer", „Varon" und „XTR" kommen im BLE nirgends vor. Erkennung über
gemerkte MAC, Name `TC174` oder Hersteller `ASR`.

**Advertising ist unzuverlässig.** Im Scan-Snapshot des Laufs steht
`name: "(nicht im Scan)"`, `rssi: null`, `services: []` — das Bike war während
des Links nicht sichtbar. Verbunden wurde über die MAC. Für `esp32.ergo` heißt
das: MAC **und** AddrType merken, Discovery darf sich nicht auf Advertising
verlassen, der Name kommt nach dem Connect aus `2A00`.

## 2. GATT

```
1800 Generic Access          2A00 2A01 2AC9
1801 Generic Attribute       2A05(I) 2B29 2B2A
180A Device Information      2A29 2A24 2A25 2A27 2A26 2A28 2A23 2A2A 2A50
1850 Vendor                  2C00(WriteNR) 2C01(R+N)
1826 Fitness Machine Service 2ACE(N) 2AD9(W+N) 2AD6(R) 2ACC(R) 2AD2(N)
```

Kein FitShow, kein `0xFFF0`. Nur ein schmaler Vendor-Service `0x1850`, der für
uns irrelevant ist.

### Vier Abweichungen von der Spec

1. **`2AD9` hat Notify statt Indicate.** CCCD wird auf `0x0001` gesetzt. Jeder
   Client, der stur auf Indicate wartet, läuft in einen Timeout.
2. **`2AD8` Supported Power Range fehlt.** Keine Wattgrenzen vom Gerät.
3. **`2AD2` sendet kein Resistance-Level-Feld**, obwohl `2ACC` die Fähigkeit
   „Resistance Level" meldet. Die aktuelle Stufe ist nicht rücklesbar.
4. **`2ACE` Cross Trainer Data streamt parallel zu `2AD2`.** Redundant.

## 3. Fähigkeiten — `2ACC` = `A646000004200000`

| Wort | Roh | Inhalt |
|------|-----|--------|
| Machine Features | `0x46A6` | Cadence, Total Distance, Pace, Resistance Level, Expended Energy, Heart Rate, Power Measurement |
| Target Setting | `0x2004` | **Resistance Target**, **Indoor Bike Simulation Parameters** |

```
supports_resistance_target = true
supports_sim              = true     (Nachtest 2026-09-13: wirkt ab ~3 %)
supports_power_target     = FALSE
supports_hr_target        = false
```

**Das ist der wichtigste Befund des ganzen Laufs.** Es gibt kein
Set-Target-Power. Klassisches ERG über Opcode `05` ist nicht vorgesehen, und
passend dazu fehlt `2AD8`. Produktiv bleibt `0x05` gesperrt; Draht-Nachtest
nur über bewussten Limiter-Bypass:
`POST /api/control/power?watt=100&raw=1&force=1` setzt `allowUntrustedPower`
**nur für diesen Write** und stellt ihn zurück (Verdict weiter `Clamp` /
`UNTRUSTED`). Ohne `force` bleibt Deny. Details historisch:
[`debug/archiv/UPDATE_NACHTEST_FORCE.md`](../../debug/archiv/UPDATE_NACHTEST_FORCE.md).
Nach einem Test den Bypass nicht dauerhaft anlassen.

## 4. Stellgröße — `2AD6` = `0A00A0000A00`

Drei sint16 little endian: `10, 160, 10`.

| Lesart | min | max | step |
|--------|-----|-----|------|
| Spec, 0,1er-Stufen | 1,0 | 16,0 | 1,0 |
| Ganzstufen | 10 | 160 | 10 |

Der Wirkungstest entscheidet für die 0,1er-Lesart: `04 64 00` (= 100 roh =
Stufe 10,0) wirkt, `04 0A` wirkt nicht.

Damit ist die Stellgröße **16 diskrete Stufen**. Das Bike wird mit 41 Stufen
und 20–400 W beworben — über FTMS ist davon nur ein grobes Raster erreichbar.

### Kommando

```
04 <lo> <hi>        sint16 LE, Einheit 0,1 Stufe
Stufe  5,0  →  04 32 00
Stufe 10,0  →  04 64 00
Stufe 16,0  →  04 A0 00
```

## 5. Live-Stream — `2AD2`

Flags **konstant** `0x0B54`, Pakete immer 19 Byte. Kein variables Layout,
kein Flag-Wechsel über 274 Pakete.

```
Offset  Feld              Format      Einheit
0–1     Flags = 0x0B54    uint16      —
2–3     Speed             uint16      0,01 km/h
4–5     Cadence           uint16      0,5 rpm  → Wert/2
6–8     Distance          uint24      m
9–10    Power             sint16      W
11–12   Energy total      uint16      kcal
13–14   Energy per hour   uint16      kcal/h
15      Energy per minute uint8       kcal/min
16      Heart Rate        uint8       bpm
17–18   Elapsed Time      uint16      s
```

Nicht gesendet: Average-Felder, **Resistance Level**, MET, Remaining Time.

Fixture-Paket: `540BDE087A002A09001A000B00000000519A01`
→ 22,7 km/h · 61 rpm · 2346 m · 26 W · 11 kcal · HR 81 · 410 s

Das HR-Feld kommt vom Empfänger des Bikes (5 kHz / Ohrsensor), nicht vom
Polar H9. Für RR-Intervalle und HRV bleibt der eigene H9-Link über `0x180D`.

## 6. Control Point — `2AD9`

Ablauf: **erst** Notify auf `2AD9` abonnieren, **dann** `00`. Ohne Abo kommt
keine Antwort.

| Gesendet | Antwort | Ergebnis |
|----------|---------|----------|
| `00` Request Control | `80 00 01` | Success — Steuerung offen |
| `07` Start/Resume | `80 07 01` | Success |
| `05 64 00` Target Power 100 W | `80 05 01` | Success, **NO_EFFECT** (2026-09-13, `force=1` — [UPDATE_NACHTEST_RESULTS.md](../../debug/UPDATE_NACHTEST_RESULTS.md)) |
| `04 0A` Resistance uint8 | `80 04 01` | Success, **keine Wirkung** |
| `04 64 00` Resistance sint16 | `80 04 01` | Success, **wirkt** |
| `08 01` Stop | `80 08 01` | Success |

`0x11` Simulation (Nachtest 2026-09-13, Firmware 0.3.9-dev, Stufe 7, ~75 rpm):

| Grade | Urteil | Δ W/rpm |
|------:|--------|--------|
| 1 % | NO_EFFECT (Success, contradictory) | ~0 % |
| 3 % | WORKS | ~+25 % |
| 6 % | WORKS | ~+26 % (Peak ~174 W) |

Achtung: `80 xx 01` bedeutet nur „Kommando verstanden". Das Gerät quittiert
auch `05`, das es laut Feature-Bits nicht kann. Success ist kein Wirkungsbeleg.

---

## 7. Nachrechnung der Wirkungsmessung *(Ursprung von ControlJournal)*

> **Historisch** — Laborlauf 2026-09-10. Dieser Abschnitt erklärt, *warum*
> `ControlJournal` Watt pro Kadenz bewertet. Stand der Kennfläche und der
> Encoding-Frage: siehe Abschluss unten und [KALIBRIERUNG.md](KALIBRIERUNG.md).

Die Sonde hat drei Vorher/Nachher-Fenster von je 12 s gemessen. Ihr
automatisches Urteil lautete dreimal „wirkt". Zwei davon hielten der Prüfung
nicht stand, weil die Verdict-Logik die Kadenz nicht herausrechnete.

| # | Kommando | Power vor→nach | Kadenz vor→nach | Belastbar? |
|---|----------|----------------|-----------------|------------|
| 1 | `05 64 00` | 23,7 → **0,0** W | 55,8 → **0,0** rpm | **Nein.** Der Fahrer hat aufgehört zu treten. Ohne Kadenz keine Leistung. Das Fenster sagt nichts über Opcode `05`. |
| 2 | `04 0A` | 10,6 → 24,3 W | 25,7 → **58,1** rpm | **Nein.** Die Kadenz hat sich mehr als verdoppelt. Bei ~59 rpm lag die Leistung vorher (Test 3) bei 24,9 W und im Ruhefenster bei 25,3 W — also unverändert. Der Write hat nichts getan. |
| 3 | `04 64 00` | 24,9 → **88,9** W | 59,1 → **51,2** rpm | **Ja.** Leistung +64 W, obwohl die Kadenz um 8 rpm *fiel*. Eindeutig. |

Damals stand die Encoding-Frage auf **einem** sauberen Datenpunkt — plausibel,
aber dünn.

Nebenbei erklärt sich Test 2 elegant: erwartet das Gerät sint16 und bekommt ein
Byte, dann ist der Wert entweder ungültig oder er liest sich als roh 10 =
Stufe **1,0** — dem Minimum. Beide Deutungen sagen „keine Änderung gegenüber
Grundlast", und genau das wurde gemessen.

**Damalige Forderung:** Ein Wirkungsurteil darf nur zählen, wenn die Kadenz in
beiden Fenstern stabil ist — besser Leistung *pro Kadenz* statt Leistung allein.

**Stand:** Die Encoding-Frage ist entschieden (`04` sint16 wirkt; siehe Sweeps
in §8 und §9). Die Forderung ist umgesetzt in
`src/control/ControlJournal.{h,cpp}` und in der Kennfläche
([KALIBRIERUNG.md](KALIBRIERUNG.md): 99 Zellen belegt, 8/8 Bänder,
Sweeps bei 60 und 80 rpm).

## 8. Das Leistungsraster — gemessen und entschieden

**Stand:** Variante 3 (Kadenz) trifft zu — bei fester Stufe steigt die Leistung
mit der Kadenz. Zusätzlich liegt die Kurve bei 60 rpm **über** der frühen
linearen Extrapolation (~130 W): Stufe 16 liefert ~170 W @ 60 rpm und ~245 W
@ 80 rpm. Quellen: [HW_TEST1_60RPM.md](../../debug/HW_TEST1_60RPM.md),
[HW_TEST2_80RPM.md](../../debug/HW_TEST2_80RPM.md). `0x11` ist Option für
feinere Last und Spitzen, nicht mehr Pflicht gegen einen Deckel bei ~130 W.

### Belastbare Punkte

| Stufe | Leistung | Kadenz | Quelle |
|-------|----------|--------|--------|
| Grundlast (≈ 1,0) | 25 W | 59 rpm | Labor 2026-09-10 (§7, Punkt 3 Ruhefenster / Grundlast) |
| 10,0 | 89 W (77–108) | 51 rpm | Labor 2026-09-10 (§7, `04 64 00`) |
| 2,0 … 16,0 | 30,7 … **170,2 W** | ~60 rpm | Sweep 2026-09-11 — [HW_TEST1_60RPM.md](../../debug/HW_TEST1_60RPM.md) |
| 16,0 | **244,6 W** (~245 W) | ~82 rpm | Sweep 2026-09-13 — [HW_TEST2_80RPM.md](../../debug/HW_TEST2_80RPM.md) |
| 8,0 | 122,9 W | ~79 rpm | dieselbe Quelle; leicht: 122,7 W — [HW_TEST2_LIGHT.md](../../debug/HW_TEST2_LIGHT.md) |
| 16,0 | **339 W** | 100–110 rpm | Heatmap 2026-09-15, passiv — [KALIBRIERUNG.md](KALIBRIERUNG.md) |

Sweep 60 rpm (gültig): Stufe 2→16 in Zweistufenschritten ca. **+20 W** je Schritt,
nahezu linear bis 170,2 W ([HW_TEST1_60RPM.md](../../debug/HW_TEST1_60RPM.md)).

### Historisch: die offene Extrapolation (vor den Sweeps)

Aus den beiden Laborpunkten Grundlast 25 W und Stufe 10 ≈ 89 W: neun Stufen
ergaben +64 W, grob **7 W pro Stufe**. Linear extrapoliert landete Stufe 16,0
bei etwa **130 W**. Das wirkte wie ein Problem — zu wenig für alles oberhalb
von Grundlage — und machte `0x11` zeitweise zur Pflicht-Kandidatin.

Drei Möglichkeiten standen damals offen:

1. Kennlinie **progressiv**, Stufe 16 deutlich über der Extrapolation.
2. Kennlinie **linear**, Deckel ~130 W → `0x11` einziger Weg zu höheren Lasten.
3. Leistung hängt **stark von der Kadenz** ab (Fläche Stufe×Kadenz).

Variante 3 war nach der Bedienungsanleitung die wahrscheinlichste (§11) und
ist gemessen bestätigt; die 60-rpm-Kurve liegt zudem über der ~130-W-
Extrapolation. Deshalb ist `0x11` heute Option, nicht Pflicht — siehe
[NACHTESTS.md](NACHTESTS.md) Test 1 und 2.

## 9. Offen / nachgeführt (Stand 2026-09-13 Abend)

| Thema | Stand |
|-------|--------|
| Stufen-Sweep 60 rpm | **ok** — [HW_TEST1_60RPM.md](../../debug/HW_TEST1_60RPM.md) |
| Kadenzfläche 80 rpm | **ok** grob — [HW_TEST2_80RPM.md](../../debug/HW_TEST2_80RPM.md); Stufe 12 optional |
| Watt-Nachtest `05` am Draht | **NO_EFFECT** trotz Success (`force=1`) — [UPDATE_NACHTEST_RESULTS.md](../../debug/UPDATE_NACHTEST_RESULTS.md) |
| `0x11` Simulation | **ok** — wirksam ab ~3 %; 1 % Success ohne Wirkung |
| Dual-Link Bike + H9 | **ok** unter Last; Bike-HR ≈ Strap +~25 bpm |
| Reconnect | **ok** — LOST → READY |
| Crash unter Last | **offen** — Hub-Watchdog / Nachtest 6 |
| Bridge / MyWhoosh | **offen** — Fahrer-Abnahme |

Geräteprofil-Default Labor-Varon (`c2:32:a5:1e:bf:b5` / `TC174`):
`resistanceFormat=sint16`, `powerTrusted=0`, `requestControlOnReconnect=true`
(auch nach NVS-Load, ab 0.3.20).

## 10. Randbedingungen

| Thema | Befund |
|-------|--------|
| Verbindungen | **Nur ein Central.** MyWhoosh, Kinomap und nRF müssen zu sein. |
| Konsolendisplay | Aus, solange der BLE-Link steht |
| Sichtbarkeit | Bike advertised während des Links oft nicht → Connect per MAC |
| CCCD-Write | Manche Subscribes scheitern mit `response=true`, Fallback auf `response=false` nötig |
| NimBLE 1.4 | `getStartHandle`/`getEndHandle` sind privat, nicht benutzbar |

---

## 11. Was die Bedienungsanleitung dazu sagt

Quelle: [`manuals.hammer.de/10006.pdf`](https://manuals.hammer.de/10006.pdf),
Abschnitte 5.4, 5.5, 5.6.2.2 und 5.6.2.4. Zwei Sätze darin sind für die
Regelung wichtiger als alles andere im Datenblatt.

### Der Watt-Modus ist selbst eine Emulation

> „Im Watt Programm wird der Widerstand basierend auf der RPM angepasst, dass
> die Leistung konstant bleibt. Je höher die RPM, desto geringer der
> Widerstand, je geringer die RPM, desto höher der Widerstand."

Das erklärt zwei Dinge auf einmal.

Erstens: das beworbene „wattgesteuert, drehzahlunabhängig, 20–400 W in
5-W-Schritten" ist eine Eigenschaft des **Konsolenprogramms**, nicht der
Widerstandsstufen. Die Konsole rechnet aus Kadenz und Zielleistung selbst eine
Stufe aus. Genau deshalb gibt es auch kein FTMS-Feature-Bit für Set Target
Power — die Maschine kennt kein Wattziel, ihr Programm kennt eines.

Zweitens, und das ist die Konsequenz für uns: bei **fester** Stufe steigt die
Leistung mit der Kadenz. Die Kennlinie Stufe → Watt ist damit keine Tabelle,
sondern eine Fläche über Stufe und Kadenz. Variante 3 aus §8 ist gemessen
bestätigt ([NACHTESTS.md](NACHTESTS.md) Test 2).

Der positive Teil daran: `PowerController` in `esp32.ergo` macht dann genau
das, was die Konsole intern auch macht. Das Verfahren ist vom Hersteller
vorgemacht, nur mit besseren Daten — wir messen die tatsächliche Leistung
zurück, die Konsole rechnet blind.

### Pulsgesteuertes Training kann die Konsole — aber nur einseitig

> „Der Computer passt den Widerstand basierend auf Ihrer Herzfrequenz an.
> Sobald sich der Puls über dem maximalen Wert befindet, blinkt dieser Wert und
> es ertönt ein Signalton. **Erst wenn sich der Wert 5 Schläge oberhalb des
> maximalen Wertes befindet wird der Widerstandswert reduziert.**"

Die Programme HR1, HR2 und IND existieren also wirklich. HR2 zielt auf 90 % der
maximalen Trainingsfrequenz, bei IND ist der Wert frei. (Die Anleitung ist bei
HR1 widersprüchlich: § 5.1 nennt 65 %, § 5.6.2.4 nennt 60 %.)

Was die Konsole damit regelt, ist aber **kein Pulshalten, sondern eine
Pulsobergrenze mit 5 Schlägen Hysterese**: sie *senkt* den Widerstand, wenn man
darüber liegt, und hebt ihn nicht an, wenn man darunter liegt. Ein Zielpuls von
unten anzufahren geht damit nicht.

Drei Einschränkungen kommen hinzu: es braucht eine eigene Pulsquelle am Gerät
(5-kHz-Gurt oder Ohrsensor), es läuft nur an der Konsole, und über BLE ist es
nicht erreichbar — das Target-Setting-Wort hat kein Heart-Rate-Bit, und sobald
ein BLE-Link steht, ist das Konsolendisplay aus.

Für `esp32.ergo` heißt das: `HR_HOLD` mit einem beidseitigen Regler ist nicht
der Nachbau eines vorhandenen Features, sondern eine **bessere Version** davon.

### Woher die 81 bpm in den Messdaten kamen

Im ganzen Laborlauf trug das HR-Feld in `2AD2` plausible 78–84 bpm. Die
Erklärung liefert der [Polar H9](https://support.polar.com/e_manuals/h9-heart-rate-sensor/polar-h9-user-manual-english/manual.pdf):
er sendet **gleichzeitig** über Bluetooth LE, ANT+ und 5-kHz-GymLink, und
GymLink ist ab Werk eingeschaltet.

Der Gurt hat also parallel den 5-kHz-Empfänger des Bikes gefüttert, während die
Sonde am BLE-Link hing. Daraus folgt Angenehmes:

- Das Bike liefert im Live-Stream **kostenlos** einen Pulswert, ohne einen
  BLE-Link zu verbrauchen.
- Dieser Wert ist als Gegenprobe zum BLE-Pfad brauchbar.
- RR-Intervalle und damit HRV kommen weiterhin nur über BLE.
- Die GymLink-Quelle kostet kein Verbindungsbudget — der BLE-Link des H9 tut es,
  und **BLE lässt beim H9 nur einen Client zu**. Genau das löst das HR-Relay in
  `esp32.heartrate` v0.3.
