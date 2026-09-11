# Nachtests — vor `esp32.ergo` v0.1

Der erste Lauf hat FTMS bestätigt (siehe [GERAETEPROFIL.md](GERAETEPROFIL.md)).
Sechs Fragen sind offen, und zwei davon bestimmen, ob v0.1 überhaupt
sinnvoll gebaut werden kann.

Reihenfolge ist bewusst: Test 1 und 2 zuerst, alles andere danach.

> **Was sich seit dem Schreiben dieser Datei geändert hat.** Die
> `probe-run.py`-Kommandos unten beschreiben noch die Sonde. Gefahren werden die
> Tests inzwischen mit `esp32.ergo` selbst:
>
> - **Test 1 und 2** laufen über den Reiter **Kalibrierung**. Der `SweepRunner`
>   hält Einschwingzeit und Mittelungsfenster ein, verwirft Punkte mit
>   weggelaufener Kadenz *und weist sie als verworfen aus*, und legt das Ergebnis
>   als Kennfläche persistent ab. Der Abschnitt „Bug in der Wirkungsauswertung"
>   am Ende dieser Datei ist damit in Code gegossen und nicht mehr
>   Menschendisziplin.
> - Die **Guard-Tabelle** unter „Vorbereitung" betrifft die Sonde. In `esp32.ergo`
>   übernimmt der `Limiter` diese Rolle; `guardAllowSim` entspricht dort der noch
>   fehlenden Freigabe für `0x11`.
> - **Vier der sechs Tests brauchen keinen Fahrer** — welche, steht in
>   [`STATE.md`](../../STATE.md) §4.
> - Vorher steht allerdings ein Beweis, den diese Datei noch nicht kennt: dass
>   eine gestellte Stufe überhaupt wirkt. Die erste Hardware-Session hat lauter
>   Erfolgsquittungen ohne Wirkung gesehen. `STATE.md` §2 und §3.

---

## Vorbereitung

Die Labor-Defaults der Sonde stehen dem Sweep im Weg:

| Guard | Default | Für die Tests |
|-------|---------|---------------|
| `guardMaxLevel` | **12** | auf **16** hoch, sonst endet der Sweep bei Stufe 12 |
| `guardMaxWatt` | 150 | bleibt, betrifft nur Opcode `05` |
| `guardAllowSim` | false | für Test 4 auf true |
| `guardDeadmanS` | 20 | bleibt — der Runner hält Keepalive |

MyWhoosh, Kinomap und nRF Connect schließen. Das Konsolendisplay bleibt
während der Messung aus, die Live-Werte stehen im Sonden-UI.

Und der Punkt, der alle Messungen trägt: **gleichmäßig treten und die Kadenz
halten**. Zwei der drei Wirkungsmessungen des ersten Laufs sind wertlos, weil
die Kadenz weggelaufen ist. Ein Metronom oder die Kadenzanzeige im Sonden-UI
hilft mehr als gutes Zureden.

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

**Erwartung und Entscheidung:**

| Ergebnis bei Stufe 16 | Folge |
|-----------------------|-------|
| deutlich über 200 W | Bester Fall. Widerstandskanal trägt v0.1 vollständig. |
| 130–200 W | Reicht für Grundlage und Intervalle bis Schwelle. Höhere Lasten über `0x11`. |
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

Dieser Test ist damit weniger eine offene Frage als eine Bestätigung mit
Zahlen — aber die Zahlen brauchen wir, denn sie sind die Kennfläche, auf der
`PowerController` arbeitet. Fällt er überraschend anders aus und die Leistung
bleibt über die Kadenz konstant, wird die Emulation deutlich einfacher.

Mindestens ein dritter Kadenzpunkt (etwa 100 rpm) bei zwei Stufen ist sinnvoll,
um zu sehen, ob der Zusammenhang linear ist oder abknickt.

## Test 3 — Watt-Nachtest

**Frage:** Kann das Bike doch Set Target Power, obwohl das Feature-Bit nein sagt?

```bash
tools/probe-run.py --host <ip> watt --watt 100 --no-prompt
```

**Durchgehend treten, vor und nach dem Write, ohne Pause.** Genau das ist im
ersten Lauf schiefgegangen: die Kadenz war beim Write schon auf null.

Wahrscheinliches Ergebnis: `80 05 01` und keine Wirkung. Dann ist die Frage
endgültig beantwortet und die ERG-Emulation ist gesetzt.

## Test 4 — Simulation `0x11`

**Frage:** Funktioniert der feine Steuerkanal?

Guard `allowSim` freigeben, mit **kleiner** Steigung anfangen.

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
wenn das Bike es nativ versteht, kann die Bridge in v0.2 diese Kommandos
einfach durchreichen.

Vorsicht: wenn Steigung wirkt, kann sie *stark* wirken. In Einerschritten
hochgehen, Not-Stop bereithalten.

## Test 5 — Dual-Link Bike + Puls

**Frage:** Trägt das Verbindungsbudget?

```bash
tools/probe-run.py --host <ip> dual --hr-mac <H9-MAC>
```

Der erste Lauf hat nur belegt, dass der H9 **scanbar** war. Das ist kein
Dual-Link. Und das gesamte Konzept — v0.1 mit zwei Links, v0.2 mit drei —
hängt daran.

Zu prüfen: beide Notify-Ströme gleichzeitig stabil, über mindestens 10 Minuten,
bei laufendem WLAN und offenem SSE. Wenn hier Pakete wegbrechen, ist das eine
Architekturfrage und keine Detailfrage.

Zweiter Durchgang gegen das **HR-Relay** von `esp32.heartrate` v0.3 statt gegen
den H9 direkt: der Relay-Knoten hält den Gurt und gibt ihn als eigener
`0x180D`-Sensor weiter. Für die Sonde ist das nur ein anderer Peer, für die
Topologie macht es den Unterschied — siehe Pflichtenheft §3.

### Nebenbei: die beiden Pulsquellen vergleichen

Der H9 sendet BLE, ANT+ und 5-kHz-GymLink gleichzeitig, GymLink ab Werk an.
Das Bike hat einen 5-kHz-Empfänger und legt den Wert ins HR-Feld von `2AD2` —
so kamen die 78–84 bpm in den ersten Lauf.

Während des Dual-Tests also beide Werte mitschreiben: HR aus `2A37` über BLE
und HR aus `2AD2` vom Bike. Interessant ist, wie weit sie auseinanderliegen und
wie träge die GymLink-Kette ist. Ist der Bike-Wert brauchbar, hat `esp32.ergo`
eine Pulsquelle, die **kein Verbindungsbudget kostet** — allerdings ohne
RR-Intervalle und damit ohne HRV.

## Test 6 — Crash unter Last

**Frage:** Was macht das Bike, wenn der Client wegbricht, während eine hohe
Stufe gesetzt ist?

```bash
tools/probe-run.py --host <ip> crash
```

Stufe auf etwa 10 setzen, dann den Crash auslösen — ohne vorheriges `08 01`.

Drei mögliche Ausgänge, alle mit Folgen für den Limiter:

| Verhalten des Bikes | Folge für `esp32.ergo` |
|---------------------|------------------------|
| Last fällt auf Grundlast | Entspannt. Watchdog-Reboot ist unkritisch. |
| Last bleibt stehen, Konsole übernimmt | Akzeptabel, aber der Fahrer muss es wissen. |
| Last bleibt stehen und ist nicht bedienbar | **Ernst.** Dann braucht jeder Reboot-Pfad ein garantiertes `08 01` davor, und der Hub-Watchdog bleibt aus. |

Das Sonden-Log liegt im RAM und ist nach dem Crash weg — `probe-run.py crash`
holt es vorher ab. Diese Reihenfolge nicht selbst nachbauen.

---

## Was danach festgezogen wird

| Messung | Legt fest |
|---------|-----------|
| Test 1 | Leistungsbereich, Kalibriertabelle, ob v0.1 trägt |
| Test 2 | Architektur von `PowerController` — Tabelle oder Regler |
| Test 3 | Ob `MANUAL_ERG` emuliert werden muss (Erwartung: ja) |
| Test 4 | Zweiter Steuerkanal, Passthrough-Fähigkeit der Bridge |
| Test 5 | Verbindungsbudget für v0.1 und v0.2 |
| Test 6 | Reboot- und Fehlerpfade im Limiter |

Erst danach lohnt das `esp32.ergo`-Repo.

## Bug in der Wirkungsauswertung

Für den nächsten Sondenlauf relevant: die Verdict-Logik von `probe-run.py`
urteilt „wirkt" allein aus dem Leistungsdelta und rechnet die Kadenz nicht
heraus. Im ersten Lauf hat sie deshalb zweimal einen Effekt bescheinigt, wo
keiner war — einmal bei Kadenz null, einmal bei mehr als verdoppelter Kadenz.
Details in [GERAETEPROFIL.md §7](GERAETEPROFIL.md).

Sauber wäre: Fenster verwerfen, wenn die mittlere Kadenz unter einer Schwelle
liegt oder sich zwischen den Fenstern um mehr als etwa 10 % ändert, und das
Urteil auf Watt pro Kadenz stützen statt auf Watt allein.
