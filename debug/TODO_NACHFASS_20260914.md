# ANWEISUNG: Nachfass zum Doku-Aufräumen + Pulsquellen-Sperre

**Auftraggeber:** Entwurfs-Instanz · **Bezug:** `debug/TODO_DOKUMENTATION.md`,
umgesetzt in `2f163862` · **Prüfung:** 2026-09-14

Das Aufräumen ist abgenommen. 14 der 18 Punkte aus Teil A sind sauber erledigt,
`debug/` steht bei 12 Dateien, und `ENTWICKLERDOKU.md` §9–13 schließt die
inhaltliche Lücke — §12 „Regelung" mit dem Absatz „Alternative, die verworfen
wurde" je Regler ist genau das, was gefehlt hat.

Dieses Dokument hat drei Teile:

| Teil | Inhalt | Umfang |
|------|--------|--------|
| **1** | Sechs übriggebliebene Doku-Stellen | klein, eng umgrenzt |
| **2** | Pulsquellen-Sperre für `HR_HOLD` und Reha — **Codeänderung** | sicherheitsrelevant |
| **3** | Pflichtenheft §11/§14/§15 und die Kriterientabelle in `STATE.md` | nur Doku, aber inhaltlich wichtig |

---

# Teil 1 — Was beim Aufräumen übrig geblieben ist

## 1.1 · Das Pflichtenheft hat die UI-Korrektur nicht mitbekommen

`WEBINTERFACE.md` §8 ist richtiggestellt. Derselbe Text steht aber ein zweites
Mal in `PFLICHTENHEFT.md`, **Zeilen 680–685**, und dort unverändert:

> „Umsetzung wie in der Familie: PROGMEM im `UiPages.h`-Muster, SSE für
> Live-Daten, Canvas für Charts, keine Fremdbibliotheken, **keine Build-Kette**.
> […] Editor und Debug-Panel werden **nachgeladen** statt mitgeliefert; reicht
> es trotzdem nicht, **wandert die UI nach LittleFS**."

Vier überholte Aussagen in sechs Zeilen. Tatsächlich: Quelle ist
`web/index.html`, `src/web/UiPages.h` ist ein Stub auf `include/UiPagesGz.h`,
`platformio.ini` fährt `extra_scripts = pre:tools/pio_pack_ui.py`, und der
Firmware-Build braucht damit **Python**. Die LittleFS-Frage ist durch gzip
entschieden.

**Zu tun.** Denselben Absatz wie in `WEBINTERFACE.md` §8 einsetzen, oder — besser
— hier kürzen und auf `WEBINTERFACE.md` §8 verweisen. Der Text zweimal zu
pflegen hat schon einmal nicht funktioniert.

## 1.2 · Die API-Tabelle im Pflichtenheft §8 ist unmarkiertes Zielbild

Die Tabelle ab Zeile 691 nennt Endpunkte, die unter diesen Namen nicht
existieren:

| in der Tabelle | tatsächlich in `App.cpp` |
|----------------|--------------------------|
| `/api/history` | — (Historie über `/api/session/list`, `/last`) |
| `/api/ble/remember` | — (`/api/ble/connect` merkt selbst) |
| `/api/test/list` \| `/start` \| `/abort` | — (nur `/api/test/result`, `/accept-ftp`) |
| `/api/calib/start` \| `/abort` \| `/get` \| `/put` | `/api/calib/sweep/start` \| `/stop`, `/api/calib/map`, `/clear` |
| `/api/debug` \| `/debug/log` \| `/debug/phase` | `/api/debug/ring` \| `/clear` \| `/export` |
| `/api/session/get` \| `/delete` \| `/export` | `/api/session/last` \| `/annotate` |
| `/api/workout/load` \| `/upload` | `/api/workout/start` \| `/import` |
| `/api/config` | `/api/config/get` \| `/save` |

Im `README.md` ist die Liste korrigiert — dort gehört sie auch hin.

**Zu tun.** Das Konzept darf ein Zielbild beschreiben; es muss nur dastehen. Ein
Satz über der Tabelle:

> Diese Tabelle ist die Zielform aus Revision 4. Die **gebaute** API steht im
> [README](../../README.md#api); Namen weichen an mehreren Stellen ab.

Keine Umbenennung im Code — die gebauten Namen sind in Betrieb, in der UI und in
`BRIDGE.md` verankert.

## 1.3 · Das Changelog hat drei Lücken, und zwar systematisch

Es fehlen **`0.3.9-dev`**, **`0.3.10-dev`** und **`0.3.20`**. Das sind genau die
drei Versionen, deren Notizen nicht gelöscht, sondern behalten
(`UPDATE_NACHTEST_RESULTS.md`) oder archiviert
(`archiv/UPDATE_NACHTEST_PROBE.md`, `archiv/UPDATE_NACHTEST_FORCE.md`) wurden.
Das Changelog wurde offenbar nur aus den **gelöschten** Dateien gebaut.

`0.3.20` wiegt am schwersten: `GERAETEPROFIL.md` §9 datiert eine
Verhaltensänderung mit „`requestControlOnReconnect=true` (auch nach NVS-Load,
**ab 0.3.20**)" — eine Aussage, die auf einen Changelog-Eintrag zeigt, den es
nicht gibt.

**Zu tun.** Drei Sektionen nachtragen, Inhalt aus den drei genannten Dateien.
Das sind die Versionen, die den Hardware-Beweis möglich gemacht haben
(Probe-API, `force=1`, Geräteprofil-Default) — sie gehören zu den wichtigsten
im ganzen Changelog. Dabei prüfen, ob `0.1.1` aus `STATE.md` eine echte
Ergo-Version ist oder ein Verschreiber; `0.1.4` ist die **Sonden**-Firmware
(Rollback-Bin) und gehört nicht ins Ergo-Changelog.

## 1.4 · `captures/README.md` nennt eine Datei, die nicht im Repo ist

Die Tabelle führt `nachtest-20260913T145717Z.jsonl` als „Nachtest-Abend /
Ergänzung". Im Ordner liegen vier Mitschnitte, dieser nicht.

**Zu tun.** Entweder die Datei nachliefern oder die Zeile entfernen. Falls der
Mitschnitt existiert, aber bewusst nicht eingecheckt ist (Größe), gehört genau
das in die Hinweisspalte. Der Linkcheck findet es nicht — es ist kein
Markdown-Link, sondern ein Dateiname in einer Tabelle.

## 1.5 · Die vierte Paketzahl lebt noch, und das Trio braucht einen Halbsatz

`test/test_codec/fixtures_synth.h` Zeile 6 sagt weiter „über **832** Pakete
hinweg nichts anderes schickt". Das ist die Zahl, die aus dem README entfernt
wurde.

Außerdem: 274 geparste und 383 eindeutige Pakete lesen sich wie ein Fehler,
solange nicht dasteht, dass die Zahlen aus verschiedenen Bezügen kommen —
**eindeutig** kann nicht größer sein als **geparst**, wenn beide dieselbe Menge
zählen.

**Zu tun.** Alle vier Zahlen einmal an **einer** Stelle erklären (Vorschlag:
Kopf von `fixtures_ibd.h`, worauf README und Pflichtenheft schon zeigen): was
832 zählt, was 274 zählt, was 383 zählt und was die 5 sind. Wenn eine der
Zahlen nicht mehr belegbar ist, **weg damit** — Regel 1 aus dem ersten Auftrag.

## 1.6 · Bekannte Grenze des Linkchecks (nur notieren)

`tools/docs_html.py` prüft Anker nur als Warnung, mit Kommentar im Code
(„many renderers slug differently"). Vertretbar, heißt aber: ein veralteter
`#anker`-Verweis macht die CI nicht rot. Das gehört als eine Zeile in den
Kopfkommentar des Skripts, damit niemand mehr Schutz annimmt als da ist.

---

# Teil 2 — Pulsquellen-Sperre für `HR_HOLD` und Reha

## 2.1 · Entscheidung

**`HR_HOLD` und das Reha-Programm dürfen nur mit einer echten Gurtquelle
laufen.** Die Pulsquelle des Bikes (`HrSource::Machine`, 5-kHz-GymLink über das
HR-Feld in `2AD2`) wird für diese beiden Modi abgelehnt.

Begründung, aus `ENTWICKLERDOKU.md` §5: der Bike-Wert liegt im Mittel **+25 bpm**
über dem Gurt. Ein Reha-Pulsdeckel von 130 bpm greift damit faktisch bei 105 —
die Regelung schneidet Leistung weg, wo keine Gefahr ist, und der Fahrer lernt,
dem Deckel nicht zu glauben. Bei einem Deckel, der als Sicherheitsnetz gedacht
ist, ist ein systematischer Versatz in dieser Größe kein Vorsichtshinweis,
sondern ein Ausschlussgrund.

Zulässig sind `HrSource::Strap` und `HrSource::Relay` — beide sind ein echter
`0x180D`-Link mit RR-Intervallen. Abgelehnt werden `Machine` und `None`.

## 2.2 · Der eigentliche Fallstrick: der stille Rückfall

`App::resolveHrSource()` (`src/app/App.cpp:321`) liefert `Strap`, solange ein
frisches Gurtsample da ist, **und fällt sonst auf `Machine` zurück**:

```cpp
if (hrc.hasSample() && !hrc.stale(now)) return ergo::HrSource::Strap;
…
return ergo::HrSource::Machine;   // Bike-Feld, wenn das Bike einen Wert hat
```

Eine Sperre allein beim Moduswechsel wäre daher **wirkungslos**: man startet
Reha korrekt mit Gurt, der Gurt geht auf halber Strecke aus, und die Regelung
läuft lautlos auf der Bike-Quelle weiter — mit 25 bpm Versatz, unter einem
Pulsdeckel, und ohne dass irgendetwas es meldet. Genau der Fall, den `HrLossPolicy`
abdeckt, nur dass der Verlust nie erkannt wird, weil ein Ersatzwert da ist.

**Das ist der wichtigere Teil dieser Änderung.** Er muss mit.

## 2.3 · Was zu bauen ist

### a) Die Entscheidung hosttestbar machen

Die Sperre gehört **nicht** als `if` nach `App.cpp`, sonst ist sie nicht
prüfbar. `HrController` und `RehaController` sind Arduino-frei und bekommen
Messwerte als Parameter — dieselbe Bauform hier:

- Ein Eingang je Regler, etwa `bool hrSourceAcceptable`, oder — schöner, weil
  sprechend — die Quelle selbst als `ergo::HrSource` mit einer freien Funktion
  `bool hrUsableForControl(HrSource)` in `BleTypes.h`.
- Die Regler behandeln „Quelle nicht zulässig" **wie Pulsverlust**: kein neuer
  Pfad, keine zweite Semantik. `HrController` setzt `lost`, `RehaController`
  setzt `lost` und lässt `capActive` fallen. Damit greift die vorhandene
  `HrLossPolicy` je Profil, die bereits getestet ist.
- `hrUsableForControl` ist eine Zeile und gehört trotzdem mit einem Satz
  kommentiert: **warum** `Machine` nicht zählt, mit Verweis auf den +25-bpm-Befund.
  Sonst dreht das in einem Jahr jemand zurück.

### b) Eintritt sperren

Vier Stellen in `src/app/App.cpp`:

| Stelle | Route |
|--------|-------|
| ~1070, Modusprüfung bei ~1115 | `/api/control/mode` für `hr` und `reha` |
| ~1158 | `HrHold`-Zweig |
| ~1187 | `Reha`-Zweig |
| 1527 / 1552 | `/api/control/hr`, `/api/control/reha` |

Abweisung mit **HTTP 409** und benanntem Grund — dasselbe Muster, mit dem die
Bridge die Exklusiv-Steuerung ablehnt (`BRIDGE.md` §3). Der Grund muss die
Quelle nennen, nicht nur „geht nicht":

```json
{ "ok": false, "reason": "hr_source_not_trusted", "hrSource": "machine" }
```

### c) Laufender Betrieb

In den Schleifenpfaden, die heute `resolveHrSource() != None` prüfen
(`App.cpp` ~2732, ~2859, ~4333, ~4412): wenn der aktive Modus `HrHold` oder
`Reha` ist, gilt eine nicht zulässige Quelle als **kein Puls**. Der vorhandene
Verlustpfad übernimmt, inklusive Meldung in der UI und Eintrag in die Session.

**Nicht** stillschweigend in `MANUAL_ERG` oder `OFF` fallen, ohne es zu sagen —
was auch immer die Verlustpolitik des Profils vorsieht, es muss sichtbar sein.

### d) Anzeige

- `/api/status`: ein Feld, aus dem die UI ohne Eigenlogik ablesen kann, ob die
  Modi verfügbar sind — etwa `hrUsable: true|false` neben dem bestehenden
  `hrSource`. Ergänzt `ENTWICKLERDOKU.md` §10.
- WebUI: die Bedienelemente für `HR_HOLD` und Reha deaktiviert, mit
  Klartextgrund am Element. Nicht nur ausgrauen — der Fahrer soll lesen können,
  warum: *„Pulsquelle Bike (+25 bpm gegen Gurt) — für Pulsregelung nicht
  zugelassen. Gurt koppeln."*
- Der bestehende `hrDelta` in `App.cpp:3129` ist dafür die ehrlichste
  Begleitanzeige und sollte im Debug-Reiter sichtbar bleiben.

### e) Hosttests

Neu oder ergänzt in `test/test_hr/` und `test/test_reha/`:

1. Reha mit `Machine` startet nicht.
2. Reha läuft mit `Strap`, Quelle wechselt auf `Machine` → `lost`, `capActive`
   fällt, Interventionszähler steigt **nicht** (es ist kein Eingriff, es ist ein
   Verlust).
3. `HR_HOLD` mit `Relay` läuft — `Relay` ist zulässig, das ist die Topologie aus
   `esp32.heartrate` v0.3 und darf nicht mit abgeschnitten werden.
4. Quelle kehrt auf `Strap` zurück → Wiederaufnahme nach der Politik des
   Profils, kein Sprung in der Zielleistung.

Punkt 3 ist der, der bei einer schnellen Umsetzung am ehesten kaputtgeht.

### f) Workout mit Pulsdeckel — kleinere Entscheidung, bitte so

Ein Workout-Schritt kann `hrSoft`/`hrMax` tragen. Dort ist das Wattziel die
Hauptsache und der Deckel ein Schutz. Deshalb **nicht** das ganze Workout
verweigern: das Workout läuft auf Watt, der **Deckel wird nicht bewaffnet**, und
das ist in der Ride-Ansicht sichtbar („Pulsdeckel inaktiv: keine Gurtquelle").
`WorkoutEngine` braucht dafür keinen neuen Zustand, nur die Information.

## 2.4 · Doku, die mitgezogen werden muss

| Datei | Was |
|-------|-----|
| `ENTWICKLERDOKU.md` §11 Sicherheit | Absatz: zulässige Pulsquellen je Modus, und dass der Rückfall als Verlust gilt |
| `ENTWICKLERDOKU.md` §12 Regelung | bei `HrController` und `RehaController` den neuen Eingang nennen |
| `ENTWICKLERDOKU.md` §10 Statusobjekt | `hrUsable` |
| `ENTWICKLERDOKU.md` §5 | den +25-bpm-Befund vom Vorsichtshinweis zur Regel aufwerten — er ist jetzt Code |
| `WEBINTERFACE.md` §4 | beim Pulsverlust-Verhalten die neue Ablehnung ergänzen |
| `BEDIENUNG.md` | eine Zeile unter „Drei Dinge, die überraschen": Pulsregelung braucht den Gurt |
| `CHANGELOG.md` | Eintrag unter der nächsten Version |
| `STATE.md` §5 | Punkt schließen |
| `PFLICHTENHEFT.md` §12 | falls ein Abnahmekriterium den Pulsdeckel betrifft, Quellenbedingung ergänzen |

## 2.5 · Was ausdrücklich **nicht** gebaut wird

**Kein Herausrechnen des Versatzes.** +25 bpm ist ein Mittelwert aus einer
einzigen Session, ohne Streuung, ohne zweiten Probanden, und die GymLink-Kette
ist träge. Einen Offset zu subtrahieren würde aus einem erkennbaren Problem ein
unsichtbares machen. Wenn das später kommen soll, braucht es vorher eine
Messreihe mit beiden Quellen parallel über mehrere Sessions — und die wäre ein
eigener Nachtest, kein Nebenprodukt.

Die Bike-Quelle bleibt für **Anzeige, Aufzeichnung und Zonen** zugelassen. Sie
kostet kein Verbindungsbudget und ist als Gegenprobe wertvoll. Gesperrt ist nur
die **Regelung**.

---

# Teil 3 — Die letzten drei Abschnitte des Pflichtenhefts, und die Kriterientabelle

Beim Aufräumen sind §7 bis §13 überarbeitet worden, **§11 („v0.0"), §14 und §15
aber nicht**. Diese drei beschreiben weiter die Welt vor den Hardware-Tests. Es
ist derselbe Fehler wie bei 1.1: die Korrektur ist an einer Stelle gelandet und
an der zweiten nicht.

Vorab, weil es die Arbeit erleichtert: ich habe die §12-Kriterien 9–22 gegen
`src/` geprüft. **Für jedes einzelne existiert der Mechanismus im Code.** Es ist
nichts „nicht angefangen". Was fehlt, ist die Abnahme.

## 3.1 · §11, Abschnitt „v0.0 — Messen"

> „Offen sind die sechs Punkte aus NACHTESTS.md, davon **zwei blockierend**:
> Stufen-Sweep und Kadenzabhängigkeit."

Beide sind gelaufen. **Zu tun:** auf „fünf beantwortet, Test 6 offen", mit
Verweis auf `NACHTESTS.md`. Der Rest von §11 ist in Ordnung — der Absatz „In der
Praxis ist die Entscheidung gefallen" ist gut und bleibt.

## 3.2 · §14 Risiken — drei Zeilen sind überholt

| Zeile | Ist | Soll |
|-------|-----|------|
| „**Stufe 16 erreicht nur ~130 W** — **Projektkritisch** […] Test 1 entscheidet." | offenes Projektrisiko | **entschärft.** Test 1: ~170 W @ 60 rpm, ~245 W @ 80 rpm. Widerstandskanal trägt v0.1 vollständig; `0x11` ist Option für feine Last und Spitzen, nicht Pflicht gegen einen Deckel |
| „Drei BLE-Links […] Am Bike ist der Dual-Link trotzdem **noch zu messen** (Test 5)" | offen | gemessen, stabil unter Last — `HW_NACHTEST_20260913.md` |
| „Encoding **auf einem Datenpunkt** — `04 <sint16>` ruht auf einer belastbaren Messung." | dünn | ein vollständiger Sweep plus Steuer-Journal mit **0 Widersprüchen** |

Die Zeile „Verhalten des Bikes bei Client-Abbruch — Test 6 lief nie" **bleibt
wie sie ist.** Sie ist die einzige im Kapitel, die noch stimmt, und sie ist die
wichtigste.

Das Risikokapitel soll dabei nicht leergeräumt werden. Ein Risiko, das sich
aufgelöst hat, ist eine Information — mit „entschärft" plus Messverweis stehen
lassen, so wie es bei den unteren beiden Zeilen des Kapitels ohnehin schon
gemacht wurde.

## 3.3 · §15 Offene Punkte — sortieren, nicht kürzen

**Zu schließen, mit Begründung und Verweis:**

- „Blockierend: **Test 1** Stufen-Sweep, **Test 2** Kadenzabhängigkeit" — beide
  erledigt. Die Überschrift „Blockierend" wird damit frei; Test 6 ist das
  einzige verbleibende blockierende Element, und zwar für den Hub-Watchdog.
- „Flash-Budget der UI: reicht PROGMEM, oder muss sie nach LittleFS? **Wird am
  ersten Build gemessen.**" — gemessen: 74,9 %, entschieden durch gzip.
- „**Wie träge der Puls über die 5-kHz-Kette** ins `2AD2`-Feld kommt. Entscheidet,
  ob die budgetfreie Quelle für `HR_HOLD` taugt oder nur fürs Dashboard." —
  **beantwortet und entschieden.** Sie taugt nur fürs Dashboard: +25 bpm gegen
  den Gurt, und Teil 2 dieses Auftrags sperrt sie für die Regelung. Diesen Punkt
  mit genau dieser Begründung schließen und auf `ENTWICKLERDOKU.md` §11 zeigen.
- „Ob die Rampe mit 20 W/min über 16 Stufen brauchbar fein ist" — teilweise
  beantwortet: die Kennfläche gibt die Stufenweite je Kadenzband her. Entweder
  mit dieser Einschränkung schließen oder als Frage an die ERG-Fahrer-Abnahme
  weiterreichen. Nicht unverändert stehen lassen.

**Offen und bleibt offen** (bitte unangetastet, das sind echte Entscheidungen):
D1 Mini gegen S3-only, Session-Format JSON gegen TCX/FIT, Kalibrierung je
Fahrer, Relay-Knoten getrennt oder integriert.

**Nachzutragen ist ein Punkt, der fehlt:** der **SIM-Passthrough** aus §11 v0.3
(„`0x11` der App durchreichen, falls Test 4 es hergibt") ist nicht gebaut — ich
habe keinen Code dafür gefunden. Test 4 hat die Bedingung erfüllt (`0x11` wirkt
ab ~3 %). Damit ist er der **einzige Punkt im ganzen Plan, dessen Vorbedingung
erfüllt ist und der trotzdem offen steht**, und er gehört als solcher notiert —
in §15 und in `STATE.md` unter „Offen".

Ebenfalls nicht gebaut, aber planmäßig v0.4 und damit kein Rückstand:
HRV-Readiness und TCX/FIT-Export.

## 3.4 · `STATE.md` §5 — die Kriterientabelle verkauft das Projekt unter Wert

Die Tabelle hat jetzt alle 24 Zeilen, gut. Drei davon sagen aber „**nicht
angefangen** / nicht formal abgenommen", und der erste Teil ist falsch. Belege
aus `src/`:

| Kriterium | Mechanismus im Code |
|---|---|
| 9, 10, 21 (`HR_HOLD`, Pulsverlust) | `HrController`; `HrLossPolicy` dreiwertig (`Freeze`, `Stop`, `reduce`), je Profil gesetzt — Reha-Profil auf `Stop` (`App.cpp:142/157/184`) |
| 11 (Not-Stop) | `/api/control/stop`, Limiter als einziger Schreibpfad |
| 12 (Auto-Pause) | `autoPauses`-Zähler, `autoPauseS` je Workout (`App.cpp:534/1770`) |
| 14 (Disconnect → MyWhoosh) | Reconnect LOST → READY belegt (`HW_NACHTEST_20260913.md`) |
| 15, 16 (SSE-Session, Archiv) | SSE läuft; `SessionStore` auf LittleFS, Hub-IOs |
| 17 (Tablet-Lesbarkeit) | Tablet-Ride-Layout gebaut |
| 19 (Reha-Deckel) | `capActive()`, `interventions()` (`App.cpp:473/474`) |
| 20 (harte Grenzen) | zweistufig: `absMaxLevelTenths`/`absMaxPowerW` im Limiter, `maxPowerW`/`maxHr` je Profil |
| 22 (Zonenschiene) | `zoneTimeS` für laufende **und** letzte Session (`App.cpp:542/558`) |

**Zu tun.** Die Sammelzeilen auflösen und **zwei Zustände unterscheiden**:

- `gebaut, Fahrer-Abnahme offen` — für 7, 9, 10, 12, 14, 15, 16, 17, 19, 20,
  21, 22
- `offen` — nur, wo wirklich nichts da ist

Wer `STATE.md` heute liest, hält ein Drittel der Abnahme für ungebaut. Das ist
für ein Einstiegsdokument der schlechtere von zwei möglichen Fehlern: es führt
dazu, dass jemand etwas nachbaut, was schon da ist.

Kriterium 17 verdient eine eigene Kennzeichnung, etwa `nur menschlich
beurteilbar` — „auf einem Tablet in zwei Metern lesbar" kann nichts anderes
sein als ein Blick, und es sollte nicht neben Messkriterien auf eine Freigabe
warten.

---

# Definition of Done

1. Teil 1 abgearbeitet; 1.5 notfalls mit der Notiz, welche Zahl nicht belegbar war.
2. `hrUsableForControl` existiert, ist kommentiert, und die Sperre greift an
   allen vier Eintrittsstellen **und** im laufenden Betrieb.
3. Die vier Hosttests laufen, `pio test -e native` bleibt grün, Fallzahl im
   `README.md` nachgezogen.
4. `pio run -e ergo` baut; neue Flash-Zahl in `STATE.md`, falls sie sich
   merklich bewegt.
5. Die UI nennt den Grund im Klartext, nicht nur durch Ausgrauen.
6. §11 „v0.0", §14 und §15 nachgezogen; erledigte Risiken als „entschärft" mit
   Messverweis stehengelassen, nicht gelöscht.
7. `STATE.md` §5 unterscheidet `gebaut, Fahrer-Abnahme offen` von `offen`; keine
   Zeile sagt mehr „nicht angefangen", wo Code existiert. SIM-Passthrough steht
   in `STATE.md` unter „Offen".
8. `python tools/docs_html.py --check` grün.
9. Notiz für die Entwurfs-Instanz — insbesondere, ob beim Bauen etwas
   aufgefallen ist, das gegen die Entscheidung in 2.1 spricht.

# Hinweis zur Prüfbarkeit

Die Entwurfs-Instanz hat **kein Python und keine Toolchain**. Build, Hosttests
und der Linkcheck können dort nicht gefahren werden — die Ergebnisse gehören
darum in die Abschlussnotiz, nicht als „läuft" ohne Zahl. Bei der letzten
Übergabe war `tools/docs_html.py --check` die einzige Zusage, die nicht
gegengeprüft werden konnte.
