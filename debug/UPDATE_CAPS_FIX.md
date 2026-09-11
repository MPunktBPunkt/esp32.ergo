# Update — `caps.wide` war falsch, und warum das so lange unsichtbar blieb

Antwort auf Punkt 1 aus [HW_TEST_REPORT.md](HW_TEST_REPORT.md) „Für die
Entwurfs-Instanz": *`caps.wide=false` am Varon vs. sint16-Annahme klären
(wirkt Success allein?)*.

**Nein, Success wirkt nicht.** Der Befund ist ein echter Fehler, und er ist der
wichtigste aus der ganzen Session.

## Was passiert ist

`Capabilities::needsWideResistance()` hat das Drahtformat von `0x04` aus dem
**Stellbereich** hergeleitet:

```cpp
// vorher
return !hasResistanceRange || resistance.maxRaw > 255 || resistance.minRaw < 0;
```

Der Varon meldet `0x2AD6` als `0A00A0000A00`, also 10 bis 160. Das passt
bequem in ein uint8. Also war `wide == false`, also hat
`encodeSetTargetResistance()` die **2-Byte-Form** `04 <uint8>` erzeugt — und
`GERAETEPROFIL.md` sagt zu genau dieser Form: wird quittiert, wirkt nicht.

Damit ist der Hardware-Test erklärt. Jeder Stufen-Write ging als `04 64`
hinaus statt als `04 64 00`, das Bike hat mit `80 04 01` Success geantwortet,
und die Last hat sich nicht bewegt. Die Rampe, der Limiter, das Steuer-Journal,
der Schattenwert — alles hat korrekt gearbeitet, nur die Bytes waren die
falschen. Deshalb war die Wirkung „noch offen": sie war nicht offen, sie war
nicht da.

Das ist ausgerechnet die Falle, die im Pflichtenheft als Regel steht: *eine
Erfolgsquittung beweist nichts*. Die Regel stand im Dokument und in einem
Kommentar — nur nicht im Code an der Stelle, die sie betraf.

## Die eigentliche Ursache

Ein Stellbereich kann keine Formatfrage beantworten. „Passt der Wert in ein
uint8" und „versteht das Gerät die uint8-Form" sind zwei verschiedene Fragen,
und die zweite lässt sich nicht herleiten, nur messen. Nach FTMS ist Set Target
Resistance Level sint16 in Zehnteln; Geräte, die nur die schmale Form nehmen,
sind die Ausnahme.

Jetzt ist Spec-Treue der Standard und die schmale Form die ausdrückliche
Ausnahme aus dem Geräteprofil:

```cpp
// nachher
if (resistanceFormat == ResistanceFormat::Uint8) {
    return hasResistanceRange && (resistance.maxRaw > 255 || resistance.minRaw < 0);
}
return true;
```

`deriveCapabilities()` setzt `resistanceFormat = Sint16`, sobald das Gerät
überhaupt ein Stufenziel meldet. `Uint8` kommt künftig aus dem Geräteprofil —
nach einer Messung, die belegt, dass die Stufe greift, nicht nach einer
Quittung.

Der Limiter bleibt unberührt: er reicht die Breite durch, die er bekommt
(`len == 2` oder `len == 3`), und klemmt in beiden Formen korrekt.

## Warum es niemand gemerkt hat

`needsWideResistance()` ist die eine Funktion, die entscheidet, welche Bytes
das Gerät erreichen — und sie hatte **keinen Test**. Codec, Limiter, Kennfläche
und Sweep hatten je eine Suite, die Capability-Ableitung nicht.

Neu: `test/test_caps/test_caps.cpp`, 13 Fälle. Der erste nagelt genau diesen
Fehler fest:

```cpp
// Der Bereich passt in ein uint8 …
TEST_ASSERT_TRUE(c.resistance.maxRaw <= 255);
// … und genau deshalb ging früher die wirkungslose Form hinaus.
TEST_ASSERT_TRUE(c.needsWideResistance());
```

Die übrigen sichern das Wattziel-Vertrauen (melden **und** Bereich
veröffentlichen), die Stellweg-Arithmetik, kaputte Bereiche mit Schrittweite
null oder Maximum unter Minimum, und dass die Stufe des Varon ein Schattenwert
bleibt.

## Geändert

| Datei | Was |
| --- | --- |
| `src/ble/FtmsCapabilities.cpp` | `needsWideResistance()` liest das Format, nicht den Bereich; `deriveCapabilities()` setzt `Sint16` als Standard |
| `src/ble/FtmsCapabilities.h` | Kommentar an der Funktion: was sie beantwortet und was nicht |
| `test/test_caps/test_caps.cpp` | neu, 13 Fälle |
| `src/ble/DebugRing.{h,cpp}` | neu — Rohbyte-Ring im JSONL-Format der Sonde |
| `src/control/ControlJournal.{h,cpp}` | neu — kadenznormiertes Steuer-Journal |
| `src/ble/FtmsClient.{h,cpp}` | zwei Taps: Rohbytes in `onNotify` und `send()`, `noteWrite` in `send()`, `respCount()` |
| `src/app/App.{h,cpp}` | `loopDebug()`, `appendDebugJson()`, `registerDebugRoutes()` |
| `src/web/UiPages.h` | Debug-Reiter: Karten „Steuer-Journal" und „Mitschnitt" |
| `test/test_journal/test_journal.cpp` | neu, 15 Fälle |
| `test/test_ring/test_ring.cpp` | neu, 12 Fälle |
| `platformio.ini` | `DebugRing.cpp` und `ControlJournal.cpp` in den nativen Filter |

## Dazu: die Firmware urteilt ab jetzt selbst

Zwei Bausteine, die beide aus genau diesem Fehler entstanden sind.

**Das Steuer-Journal** (`src/control/ControlJournal.{h,cpp}`, Abnahmekriterium
6b) beurteilt nach jedem abgesetzten Write, ob er gewirkt hat — auf Basis von
**Watt pro Kadenz**, nicht Watt. Bei fester Stufe ist die Leistung ungefähr
Drehmoment mal Kadenz; Watt durch rpm ist damit ein Maß für das Drehmoment, und
genau das stellt die Stufe. Schneller treten erhöht die Leistung, nicht die
Stufe. Bei zu niedriger oder zwischen den Fenstern weggelaufener Kadenz fällt
**kein Urteil** — „weiß nicht" ist ein Ergebnis.

Der Punkt daran: Quittung und Wirkung werden getrennt geführt. Der Zustand
„Gerät meldet Success, Messung sieht nichts" hat jetzt einen Namen
(`contradictory()`), erscheint rot im Debug-Reiter und im Log als
`[JRN] WIDERSPRUCH`. Dieser eine Satz hätte die letzte Session gerettet.

Das Journal nagelt außerdem den alten Fehler der Sonde fest: `probe-run.py`
hatte zweimal „wirkt" gemeldet, einmal bei Kadenz null und einmal bei mehr als
verdoppelter Kadenz. Beide Fälle sind als Testfall drin und kommen jetzt als
`UNJUDGED` beziehungsweise `NO_EFFECT` heraus.

**Der Mitschnitt** (`src/ble/DebugRing.{h,cpp}`, Abnahmekriterium 6a) schreibt
die Rohbytes von 0x2AD2 und 0x2AD9 mit, einschließlich der abgesetzten Writes —
und zwar `v.data` nach dem Limiter, also was das Gerät wirklich gesehen hat,
nicht was die Absicht war. Ausgabe über `GET /api/debug/export` als JSONL im
Format der Sonde; `tools/make-fixtures.py` liest das unverändert als
`bike-data.jsonl`. Damit werden Fixtures aus einer echten Fahrt statt aus einem
Laborlauf.

256 Datensätze à 32 Byte = 8 kB RAM. Ausgedünnt wird nur der Messstrom
(Standard: jedes fünfte 0x2AD2, also gut zehn Minuten Reichweite); jedes neue
Flagwort und der gesamte Steuerverkehr bleiben vollständig. Der Ring ist nach
dem Booten **aus** und wird bewusst nicht in der Konfiguration gemerkt.

Neue Endpunkte: `POST /api/debug/ring?on=1&every=5`, `POST /api/debug/clear`,
`GET /api/debug/export`.

## Ergebnis

Ausgefüllt von der Build-Instanz am 2026-09-11 nach Pull `6b3d86e`.

```text
pio test -e native
  test_codec     22/22 PASSED   (1 Assertion in test_caps_varon angepasst → Sint16)
  test_limiter   21/21 PASSED
  test_caps      13/13 PASSED
  test_powermap  19/19 PASSED
  test_sweep     17/17 PASSED
  test_journal   15/15 PASSED
  test_ring      12/12 PASSED
  → 119/119
```

```text
pio run -e ergo
  → SUCCESS
  RAM:   71032 / 327680  (21.7 %)
  Flash: 1289845 / 1966080 (65.6 %)
  UI GET /: 39640 Bytes
```

OTA auf `.88` OK (Bike vorher disconnect wegen `bikeLink`-Sperre in deploy.sh).

### Hardware nach Flash

- [x] Reconnect TC174 → `READY`, Caps `wide=true`, `levels=16`
- [x] Mitschnitt: Write `041400` (= sint16 Stufe 2,0) und Antwort `800401` Success  
      — **nicht** mehr die schmale `04 14`
- [x] Journal/Debug in `/api/status`, Export JSONL liefert Zeilen
- [ ] Hand-an-Kurbel-Beweis Stufe 1 vs 16 (Wirkung) — braucht jemanden vor Ort

Lokaler Fix mitgepusht: `test/test_codec/test_codec.cpp` erwartete noch
`needsWideResistance()==false` / `Unknown` — widerspricht dem Caps-Fix.

## Hardware-Test — und er braucht keinen Fahrer

Die ganze letzte Session hat nicht belegt, dass eine Stufe wirkt. Das ist der
einzige Punkt, der zählt. Er ist aber **qualitativ**, und dafür muss niemand
treten: die Kurbel von Hand drehen genügt.

- [x] Flashen, Bike verbinden, `POST /api/control/request`
- [x] Mitschnitt: Write-Bytes sind sint16 (`041400`), Success `800401`
- [ ] Stufe 1 setzen, Kurbel ~20 s gleichmäßig von Hand drehen
- [ ] Stufe 16 setzen, ~20 s gleichmäßig von Hand drehen
- [ ] Handgefühl + Journal (`worked` / ggf. `contradictory`)
- [ ] **Mitschnitt einschalten** (Debug-Reiter) — dann ist der Lauf hinterher
      auswertbar, auch wenn live niemand mitliest
- [ ] Stufe 1 setzen, Kurbel gleichmäßig von Hand drehen, ~20 s
- [ ] Stufe 16 setzen, wieder gleichmäßig von Hand drehen, ~20 s
- [ ] **Ist der Widerstand von Hand deutlich verschieden?** Das Journal sagt es
      zusätzlich selbst: `wirkt` oder `keine Wirkung` im Debug-Reiter. Das Bike
      meldet auch bei Handbetrieb Werte — im letzten Bericht 12 W bei 21 rpm.
- [ ] Bleibt es bei `keine Wirkung` **mit** Erfolgsquittung, ist sint16
      ebenfalls falsch. Nächster Verdacht dann: fehlende `Start/Resume`-Freigabe
      oder ein Gerätemodus an der Konsole.

Die Konsole taugt nicht als Rückmeldung: laut `GERAETEPROFIL.md` ist ihr Display
aus, solange der BLE-Link steht, und die Stufe meldet das Bike in 0x2AD2 nicht
zurück. Bleibt das Handgefühl plus das Journal.

Ebenfalls ohne Fahrer möglich: Reconnect nach Bike aus/an, Nachtest 5
(Dual-Link, Gurt umlegen und sitzen bleiben) und Nachtest 6 (Crash unter Last,
hohe Stufe setzen und dem ESP den Strom ziehen).

Einen Fahrer brauchen nur noch die **Zahlen**: `Test 1 · 60 rpm` und
`Test 2 · 80 rpm` über den Reiter Kalibrierung, plus Nachtest 3.

**Risiko unverändert:** erster Versuch mit leerem Sattel, Hand am Netzschalter.
Der Unterschied zur letzten Session ist, dass die Stufen ab jetzt vermutlich
tatsächlich greifen — vorher haben sie es nicht.

## Noch offen aus dem Bericht

Punkt 2 der Build-Instanz („nach Stop kann `Start/Resume` plus erneuter Tritt
nötig sein") ist ein UI-Hinweis und noch nicht umgesetzt. Er gehört sinnvoll
erst dazu, wenn klar ist, ob `Start/Resume` am Varon tatsächlich nötig ist —
das zeigt der Test oben nebenbei.
