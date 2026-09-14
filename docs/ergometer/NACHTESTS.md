# Nachtests — Ergebnisse (eine Zeile offen)

Der erste Lauf hat FTMS bestätigt (siehe [GERAETEPROFIL.md](GERAETEPROFIL.md)).
**Fünf der sechs Fragen sind beantwortet.** Test 6 (Crash unter Last) ist offen.
Das Repo `esp32.ergo` existiert; **v0.1.0** ist released. Die Messungen unten
laufen über die Ergo-Firmware (Kalibrier-Reiter / Probe-API), nicht mehr über
die Sonde allein.

Reihenfolge der Planung war: Test 1 und 2 zuerst — beide sind gelaufen.

---

## Vorbereitung

Die Labor-Defaults der Sonde standen dem Sweep im Weg (historisch für die
Sonden-Kommandos unten):

| Guard | Default | Für die Tests |
|-------|---------|---------------|
| `guardMaxLevel` | **12** | auf **16** hoch, sonst endet der Sweep bei Stufe 12 |
| `guardMaxWatt` | 150 | bleibt, betrifft nur Opcode `05` |
| `guardAllowSim` | false | für Test 4 auf true |
| `guardDeadmanS` | 20 | bleibt — der Runner hält Keepalive |

In `esp32.ergo` übernimmt der `Limiter` diese Rolle; `allowSimulation` ist die
Freigabe für `0x11` (am 2026-09-13 benutzt).

MyWhoosh, Kinomap und nRF Connect schließen. Das Konsolendisplay bleibt
während der Messung aus.

Und der Punkt, der alle Messungen trägt: **gleichmäßig treten und die Kadenz
halten**. Zwei der drei Wirkungsmessungen des ersten Laufs waren wertlos, weil
die Kadenz weggelaufen ist.

---

## Test 1 — Stufen-Sweep (der wichtigste)

**Frage:** Wie sieht die Kennlinie Stufe → Leistung aus, und wo endet sie?

Kadenz konstant **60 rpm** halten. Je Stufe 60 s: 20 s einschwingen, 40 s
mitteln. Stufen `1, 2, 4, 6, 8, 10, 12, 14, 16`.

```bash
tools/probe-run.py --host <ip> control --level 1
# … je Stufe, oder als Schleife über 04 <sint16 LE>
```

**Zu protokollieren:** je Stufe die mittlere Leistung, die mittlere Kadenz und
die Streuung. Ohne stabile Kadenz ist der Punkt ungültig.

> **Ergebnis** (2026-09-11,
> [HW_TEST1_60RPM.md](../../debug/HW_TEST1_60RPM.md)):
> 8 von 9 Punkten gültig. Stufe 16,0 bei 60 rpm: **Mittel 170,2 W**
> (Δ je zwei Stufen ca. +20 W, nahezu linear). Band **130–200 W** —
> Widerstandskanal trägt Grundlage und Intervalle bis Schwelle.

**Erwartung und Entscheidung:**

| Ergebnis bei Stufe 16 | Folge |
|-----------------------|-------|
| deutlich über 200 W | Bester Fall. Widerstandskanal trägt v0.1 vollständig. |
| **130–200 W** ← **gemessen (~170 W @ 60 rpm)** | Reicht für Grundlage und Intervalle bis Schwelle. Höhere Lasten über `0x11`. |
| um 130 W (lineare Extrapolation) | Widerstandskanal deckelt. `0x11` wird Pflicht, nicht Option. |

## Test 2 — Kadenzabhängigkeit

**Frage:** Ist Stufe → Watt eine Tabelle oder eine Fläche?

Denselben Sweep bei **80 rpm**, verkürzt auf Stufen `4, 8, 12, 16`.

Die Bedienungsanleitung nimmt das Ergebnis vermutlich vorweg. Sie beschreibt den
Watt-Modus so: „Im Watt Programm wird der Widerstand basierend auf der RPM
angepasst, dass die Leistung konstant bleibt. Je höher die RPM, desto geringer
der Widerstand." Das beworbene „drehzahlunabhängig" ist also eine Eigenschaft
des **Konsolenprogramms**, nicht der Stufen. Bei fester Stufe sollte die
Leistung mit der Kadenz steigen.

> **Ergebnis** (leicht 2026-09-11
> [HW_TEST2_LIGHT.md](../../debug/HW_TEST2_LIGHT.md);
> grob 2026-09-13
> [HW_TEST2_80RPM.md](../../debug/HW_TEST2_80RPM.md)):
> Bei fester Stufe steigt die Leistung mit der Kadenz — **Fläche**, keine
> Tabelle. Stufe 16 @ 80 rpm: **Mittel 244,6 W** (~245 W). Stufe 8: 122,7 W
> (leicht) / 122,9 W (grob). Stufe 12 @ 80 rpm verworfen (Kadenz nicht gehalten).

**Entscheidung:**

| Beobachtung | Folge |
|-------------|-------|
| **Leistung steigt mit Kadenz** ← **gemessen** | `PowerController` arbeitet auf einer Kennfläche Stufe×Kadenz. |
| Leistung bleibt über die Kadenz konstant | Emulation wäre einfacher (reine Stufe→Watt-Tabelle). |

## Test 3 — Watt-Nachtest

**Frage:** Kann das Bike doch Set Target Power, obwohl das Feature-Bit nein sagt?

```bash
tools/probe-run.py --host <ip> watt --watt 100 --no-prompt
```

**Durchgehend treten, vor und nach dem Write, ohne Pause.** Genau das ist im
ersten Lauf schiefgegangen: die Kadenz war beim Write schon auf null.

> **Ergebnis** (2026-09-13 Abend, FW 0.3.19-dev, `force=1`,
> [UPDATE_NACHTEST_RESULTS.md](../../debug/UPDATE_NACHTEST_RESULTS.md)):
> Draht `056400` (100 W), Quittung **Success**, Urteil **NO_EFFECT**
> (96→101 W, 80,8→84,1 rpm). Produktiv kein `0x05` ans Bike; ERG weiter über
> Stufen/`PowerMap`.

**Entscheidung:**

| Ergebnis | Folge |
|----------|-------|
| **`80 05 01` und keine Wirkung** ← **gemessen (NO_EFFECT)** | Frage endgültig beantwortet; ERG-Emulation ist gesetzt. |
| Success und messbare Laständerung | Feature-Bits wären falsch; nativer Wattpfad möglich. |

## Test 4 — Simulation `0x11`

**Frage:** Funktioniert der feine Steuerkanal?

Guard `allowSim` / `allowSimulation` freigeben, mit **kleiner** Steigung anfangen.

```bash
tools/probe-run.py --host <ip> sim --grade 1
tools/probe-run.py --host <ip> sim --grade 3
tools/probe-run.py --host <ip> sim --grade 6
```

Parameter von `0x11`: Wind (sint16, 0,001 m/s), Grade (sint16, 0,01 %),
Crr (uint8, 0,0001), Cw (uint8, 0,01 kg/m).

Das ist aus zwei Gründen der interessanteste Test. Erstens ist die Auflösung
von 0,01 % Steigung um Größenordnungen feiner als 16 Widerstandsstufen.
Zweitens ist `0x11` genau das, was MyWhoosh und Zwift im SIM-Modus senden —
wenn das Bike es nativ versteht, kann die Bridge diese Kommandos durchreichen.

Vorsicht: wenn Steigung wirkt, kann sie *stark* wirken. In Einerschritten
hochgehen, Not-Stop bereithalten.

> **Ergebnis** (2026-09-13 Vormittag, FW 0.3.9-dev, Stufe 7, ~75 rpm,
> [HW_NACHTEST_20260913.md](../../debug/HW_NACHTEST_20260913.md)):
> 1 % → **NO_EFFECT** (Success, contradictory); 3 % und 6 % → **WORKS**
> (Δ W/rpm ~+25 % / ~+26 %, Peak ~174 W @ 78 rpm). Wirksamer Steuerkanal ab ~3 %.

**Entscheidung:**

| Ergebnis | Folge |
|----------|-------|
| wirkt nicht / nur Success | Bridge muss Grade lokal in Stufen umsetzen. |
| **wirkt ab ~3 %** ← **gemessen** | Passthrough nach Freigabe (`allowSimulation`) architektonisch sinnvoll. |

## Test 5 — Dual-Link Bike + Puls

**Frage:** Trägt das Verbindungsbudget?

```bash
tools/probe-run.py --host <ip> dual --hr-mac <H9-MAC>
```

Zu prüfen: beide Notify-Ströme gleichzeitig stabil, über mindestens 10 Minuten,
bei laufendem WLAN und offenem SSE.

> **Ergebnis** (2026-09-13,
> [HW_NACHTEST_20260913.md](../../debug/HW_NACHTEST_20260913.md)):
> Dual-Link Bike + H9 unter Last **ok** (~10 Min ohne Linkabriss).
> `hrSource=strap`; Bike-HR − Strap ≈ **+25 bpm** (Mittel über 312 Ticks).
> Reconnect Bike LOST → READY ok (`reconnects: 1`); kurzer ESP-WLAN-Blip
> beim Linkverlust, kein Brick.

**Entscheidung:**

| Ergebnis | Folge |
|----------|-------|
| Pakete brechen weg | Architekturfrage (weniger Links / Relay-Pflicht). |
| **beide Ströme stabil** ← **gemessen** | Verbindungsbudget für v0.1 (zwei Links) trägt. |

Zweiter Durchgang gegen das **HR-Relay** von `esp32.heartrate` v0.3 statt gegen
den H9 direkt: der Relay-Knoten hält den Gurt und gibt ihn als eigener
`0x180D`-Sensor weiter. Für die Topologie macht es den Unterschied — siehe
Pflichtenheft §3.

### Nebenbei: die beiden Pulsquellen vergleichen

Der H9 sendet BLE, ANT+ und 5-kHz-GymLink gleichzeitig, GymLink ab Werk an.
Das Bike hat einen 5-kHz-Empfänger und legt den Wert ins HR-Feld von `2AD2`.

Während des Dual-Tests beide Werte mitschreiben: HR aus `2A37` über BLE und HR
aus `2AD2` vom Bike. Gemessen (siehe Ergebniskasten): GymLink-Kette für
Regelung ungenau/träge — Dashboard ok, `HR_HOLD` nur mit Vorsicht. RR-Intervalle
weiterhin nur über BLE.

## Test 6 — Crash unter Last *(offen)*

**Frage:** Was macht das Bike, wenn der Client wegbricht, während eine hohe
Stufe gesetzt ist?

Drei mögliche Ausgänge, alle mit Folgen für den Limiter / Hub-Watchdog:

| Verhalten des Bikes | Folge für `esp32.ergo` |
|---------------------|------------------------|
| Last fällt auf Grundlast | Entspannt. Watchdog-Reboot ist unkritisch. |
| Last bleibt stehen, Konsole übernimmt | Akzeptabel, aber der Fahrer muss es wissen. |
| Last bleibt stehen und ist nicht bedienbar | **Ernst.** Dann braucht jeder Reboot-Pfad ein garantiertes `08 01` davor, und der Hub-Watchdog bleibt aus. |

### Anleitung auf der Ergo-Firmware

Der Debug-Ring liegt im **RAM** und ist nach dem Reset weg — vorher sichern.

1. Mitschnitt an: `POST /api/probe/arm`
2. Stufe ~10 setzen (Profil mit ausreichend `maxLevel`)
3. Ring **vorher** sichern: `GET /api/debug/export`
4. Reset **ohne** vorheriges `08 01` (kein Stop)
5. Danach Kurbel von Hand drehen und prüfen, ob die Last steht / fällt /
   bedienbar ist

Hilfreich für den Handkurbel-Vergleich Stufe niedrig vs. hoch (nicht denselben
Crash-Pfad, aber denselben Hand-Beweis): [`tools/hand-proof.sh`](../../tools/hand-proof.sh).

Sonden-Äquivalent (historisch): `tools/probe-run.py --host <ip> crash` — holt
das Log vorher ab.

---

## Was die Tests festgezogen haben

| Messung | Legt fest | Stand |
|---------|-----------|-------|
| Test 1 | Leistungsbereich, Kalibriertabelle, ob v0.1 trägt | **erledigt** (~170 W @ 60 rpm) |
| Test 2 | Architektur von `PowerController` — Tabelle oder Regler | **erledigt** (Fläche) |
| Test 3 | Ob `MANUAL_ERG` emuliert werden muss | **erledigt** (ja; NO_EFFECT) |
| Test 4 | Zweiter Steuerkanal, Passthrough-Fähigkeit der Bridge | **erledigt** (wirkt ab ~3 %) |
| Test 5 | Verbindungsbudget für Dual-Link | **erledigt** |
| Test 6 | Reboot- und Fehlerpfade im Limiter / Hub-Watchdog | **offen** |

## Bug in der Wirkungsauswertung

Die frühe Verdict-Logik urteilte „wirkt“ allein aus dem Leistungsdelta und
rechnete die Kadenz nicht heraus — deshalb zweimal ein Scheineffekt (Kadenz
null bzw. verdoppelt). `ControlJournal` bewertet seither Watt pro Kadenz und
verwirft instabile Fenster; Details in [GERAETEPROFIL.md §7](GERAETEPROFIL.md)
und `src/control/ControlJournal.{h,cpp}`.
