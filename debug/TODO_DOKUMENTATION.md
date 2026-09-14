# ANWEISUNG: Dokumentation prüfen, ausmisten und lesbar machen

**Auftraggeber:** Entwurfs-Instanz · **Stand des Audits:** 2026-09-14,
Repo-HEAD `1a8998ec202560cdf87be5065035b7df7427df99`

Dieses Dokument ist ein **Auftrag**, kein Protokoll. Es hat vier Teile:

| Teil | Inhalt |
|------|--------|
| **A** | 18 belegte Unstimmigkeiten mit Fundort, Ist, Soll und Quelle |
| **B** | Ausmisten: 61 Dateien in `debug/`, konkrete Zuordnung je Datei |
| **C** | Was fehlt und aufgenommen werden sollte |
| **D** | Bewertung der Ausführlichkeit — wo zu dünn, wo zu breit |
| **E** | Die lesbare HTML-Variante, mit Spezifikation |

---

## Regeln für diese Arbeit

1. **Keine Zahl ohne Quelle.** Jede korrigierte Kennzahl muss auf eine benannte
   Messung, einen Build oder eine Codestelle zeigen. Wo die Quelle fehlt, wird
   die Zahl **entfernt**, nicht geschätzt. Eine plausible Zahl ohne Herkunft ist
   genau das, was dieses Projekt schon einmal teuer bezahlt hat.
2. **Historie nicht löschen, sondern kennzeichnen.** Mehrere Abschnitte sind
   überholt, aber sie erklären, *warum* die Architektur so aussieht. Sie werden
   als historisch markiert und mit dem Ergebnis versehen — nicht gestrichen.
   Das gilt besonders für `GERAETEPROFIL.md` §7 und §8.
3. **Konzept und Stand bleiben getrennt.** `PFLICHTENHEFT.md` beschreibt, was
   gebaut werden soll, `STATE.md` was gebaut ist. Wo das Konzept eine Aussage
   über den Stand macht, wird die Aussage entfernt und verlinkt, nicht
   aktualisiert. Sonst veralten beide Dokumente gemeinsam.
4. **Keine Codeänderungen in diesem Auftrag**, mit einer Ausnahme: die beiden
   Kommentarzeilen in `test/test_codec/fixtures_ibd.h`, die eine falsche
   Paketzahl nennen (A1), und das neue Werkzeug aus Teil E.
5. Am Ende steht eine Notiz für die Entwurfs-Instanz: was geändert wurde und
   welche Zahlen noch offen sind.

---

# Teil A — Belegte Unstimmigkeiten

## A1 · Die Fixture-Paketzahl existiert in vier Varianten

| Fundort | Behauptung |
|---------|-----------|
| `docs/ergometer/GERAETEPROFIL.md` §5 | „kein Flag-Wechsel über **274** Pakete" |
| `docs/ergometer/PFLICHTENHEFT.md` §9 | „gegen die **274** aufgezeichneten Pakete" |
| `docs/ergometer/PFLICHTENHEFT.md` §12 Kriterium 4 | „dekodiert alle **274** Fixture-Pakete" |
| `test/test_codec/fixtures_ibd.h` Kopf | „durch alle **383** eindeutigen Pakete" |
| `README.md` Abschnitt Build und Tests | „über **832** aufgezeichnete Pakete hinweg" |
| tatsächlicher Inhalt von `fixtures_ibd.h` | **5** Einträge (handgeprüfter Kernsatz) |

**Zu tun.** Drei Größen auseinanderhalten und je einmal festlegen: wie viele
`0x2AD2`-Pakete der Laborlauf **insgesamt** aufgezeichnet hat, wie viele davon
**eindeutig** sind, und wie viele im eingecheckten Fixture-Satz stehen. Die
Zahlen kommen aus `nodes/esp32.ftmsprobe/.../bike-data.jsonl` beziehungsweise
aus `make-fixtures.py`, nicht aus dem Gedächtnis.

**Wichtiger noch:** Abnahmekriterium 4 ist so, wie es dasteht, **nicht
prüfbar** — es verlangt 274 Pakete, der eingecheckte Satz hat 5, und die CI
prüft mit `--verify-curated` etwas Drittes. Kriterium 4 muss so umformuliert
werden, dass es beschreibt, was die CI tatsächlich tut. Der Beleg dafür liegt in
`debug/UPDATE_FIXTURE_6A.md` und `.github/workflows/build.yml`.

## A2 · Die Bridge ist v0.3, sechs Stellen sagen v0.2

| Fundort | Stelle |
|---------|--------|
| `PFLICHTENHEFT.md` Kopf | „NimBLE als GATT Central — in **v0.2** zusätzlich als Peripheral" |
| `PFLICHTENHEFT.md` §1 | „gibt sie in **v0.2** als Bridge an MyWhoosh weiter" |
| `PFLICHTENHEFT.md` §9 Modultabelle | „`FtmsServer` … (**v0.2**)" |
| `PFLICHTENHEFT.md` §10 ini-Kommentar | „ROLE_PERIPHERAL / ROLE_BROADCASTER NICHT deaktivieren (Bridge **v0.2**)" |
| `NACHTESTS.md` Test 4 | „kann die Bridge in **v0.2** diese Kommandos einfach durchreichen" |
| `BLE-SCAN.md` Ergebnistabelle | „v0.1 wie geplant, Bridge in **v0.2** unkompliziert" |

`PFLICHTENHEFT.md` §11 sagt v0.3, und die Firmware steht bei `0.3.23-dev`.

**Zu tun.** Alle sechs auf v0.3. Zusätzlich in §11 einen Satz ergänzen: die dort
als offen bezeichnete Scope-Entscheidung zwischen v0.2 und v0.3 ist **in der
Praxis entschieden** — Editor, geführte Tests, Progression und Ghost sind in
v0.1.0 mitgekommen, die Bridge folgte als 0.3.x. Das ist eine Information, die
ein Leser sonst nur durch Rückrechnen aus Changelogs bekommt.

## A3 · Der `platformio.ini`-Ausschnitt im Pflichtenheft bricht die harte Regel 2

`PFLICHTENHEFT.md` §10 zeigt:

```ini
[env:ergo-s3]
build_flags =
  ${env.build_flags}
```

Tatsächlich heißt das Environment `[env:ergo]`, und `${env.build_flags}` setzt
eine `[env]`-Sektion voraus, die **nicht existieren darf** — genau das ist
Regel 2 in `STATE.md` §6 und der Inhalt von `debug/PLATFORMIO_FIX.md`. Wer den
Ausschnitt kopiert, zerlegt die Hosttests.

**Zu tun.** Ausschnitt gegen den echten Block aus `platformio.ini` tauschen und
eine Warnzeile darüber setzen. Ein Codebeispiel im Konzept, das gegen eine harte
Regel desselben Projekts verstößt, ist schlimmer als kein Beispiel.

## A4 · Die Modultabelle in `PFLICHTENHEFT.md` §9 nennt Bausteine, die anders heißen

| Tabelle | tatsächlich im Code |
|---------|---------------------|
| `CalibTable` | `PowerMap` (`src/control/PowerMap.{h,cpp}`) |
| `HrProfile` | `HrClient` (`src/ble/HrClient.{h,cpp}`) |
| `DebugLog` | `DebugRing` **plus** `ControlJournal` |
| `Metrics` | kein eigenes Modul; verteilt auf `SessionTracker` und `Zone` |

Vorhanden, in der Tabelle aber gar nicht genannt: `ControlMode`,
`SweepRunner`, `RehaController`, `WorkoutJson`, `ZwoImport`, `BridgeAssist`,
`SimAssist`, `CyclingCodec`, `Zone`, `Progression`, `FtpCareer`,
`SessionTracker`, `SessionSummary`, `SessionStore`.

**Zu tun.** Tabelle gegen `src/` abgleichen. Zwei Spalten ergänzen: **Pfad** und
**hosttestbar ja/nein**. Letzteres ist keine Kosmetik — die Liste in `STATE.md`
§6.3 nennt 24 Arduino-freie Bausteine und `platformio.ini` führt sie ebenfalls;
drei Listen derselben Sache in drei Dateien driften garantiert auseinander.
Besser: `PFLICHTENHEFT.md` §9 nennt die Bausteine, und die Frage „welche sind
hosttestbar" wird **nur** in `platformio.ini` beantwortet, mit Verweis darauf.

## A5 · `GERAETEPROFIL.md` §8 widerspricht §9 im selben Dokument

§8 heißt „Das Leistungsraster — die **offene** Kernfrage", rechnet Stufe 16 auf
„etwa **130 W**" hoch, nennt das „ein Problem" und schließt mit „Drei
Möglichkeiten, und nur eine Messung entscheidet."

§9, zwei Abschnitte später, sagt: Stufen-Sweep 60 rpm **ok**. Gemessen sind
**170 W** bei 60 rpm und **245 W** bei 80 rpm
(`debug/HW_TEST1_60RPM.md`, `debug/HW_TEST2_80RPM.md`).

**Zu tun.** §8 umschreiben auf „gemessen und entschieden": Variante 3 (Kadenz)
trifft zu, und die Kurve liegt zusätzlich **über** der linearen Extrapolation.
Die alte Extrapolation als ausdrücklich markierten historischen Absatz
stehenlassen — sie erklärt, warum `0x11` einmal als Pflicht galt und heute
Option ist. Die Tabelle der belastbaren Punkte (Grundlast 25 W, Stufe 10 bei
89 W) durch die Sweep-Tabelle ersetzen oder ergänzen.

## A6 · `GERAETEPROFIL.md` §6 und §9 widersprechen sich bei `0x05`

§6 Tabelle: „`05 64 00` Target Power 100 W … Success **ohne Wirkungsbeleg**".
§9: „Watt-Nachtest `05` am Draht — **NO_EFFECT** trotz Success (`force=1`)".

**Zu tun.** §6 auf NO_EFFECT mit Datum und Quelle
(`debug/UPDATE_NACHTEST_RESULTS.md`). „Ohne Wirkungsbeleg" war der Stand vom
10.09., jetzt ist es ein Befund.

## A7 · `GERAETEPROFIL.md` §7 ist überholt formuliert

Der Abschnitt endet mit „Damit steht die Encoding-Frage auf **einem** sauberen
Datenpunkt. Das Ergebnis ist plausibel, aber dünn." Inzwischen liegen 8 gültige
Sweep-Punkte bei 60 rpm, 3 bei 80 rpm und eine Kennfläche mit 62 belegten
Zellen vor.

Ebenso: „Sauber wäre … das Urteil auf Watt pro Kadenz stützen statt auf Watt
allein." Das **ist** gebaut — `src/control/ControlJournal.{h,cpp}`.

**Zu tun.** §7 als Ursprungsgeschichte des Steuer-Journals kennzeichnen und mit
zwei Sätzen abschließen: die Frage ist entschieden, und die Forderung am Ende
ist umgesetzt. Verweis auf `ControlJournal` und auf `KALIBRIERUNG.md`.

## A8 · `NACHTESTS.md` ist als Vorab-Dokument formuliert, obwohl fünf von sechs Tests gelaufen sind

| Stelle | Problem |
|--------|---------|
| Titel | „Nachtests — **vor** `esp32.ergo` v0.1" |
| Einleitung | „Sechs Fragen sind **offen**" |
| Einleitung | „Reihenfolge ist bewusst: Test 1 und 2 zuerst" — beide gelaufen |
| Test 3 | „**Wahrscheinliches** Ergebnis: `80 05 01` und keine Wirkung" — gemessen |
| Test 5 | „Der erste Lauf hat nur belegt, dass der H9 **scanbar** war" — Dual-Link gemessen |
| Abschluss | „**Erst danach lohnt das `esp32.ergo`-Repo.**" — Repo existiert, v0.1.0 released |
| „Bug in der Wirkungsauswertung" | „Für den **nächsten Sondenlauf** relevant" — als `ControlJournal` umgesetzt |
| Banner | verweist auf `STATE.md` §4 für „vier der sechs Tests brauchen keinen Fahrer" — §4 wurde umgeschrieben und trägt diese Aussage nicht mehr |
| Banner | `guardAllowSim` „entspricht der noch **fehlenden** Freigabe für `0x11`" — `allowSimulation` existiert und wurde am 13.09. benutzt |
| Test 1 Entscheidungstabelle | drei offene Zweige; der gemessene (130–200 W) ist nicht markiert |
| Test 6 | beschreibt nur `probe-run.py crash`; das Ergo-Äquivalent fehlt |

**Zu tun.** `NACHTESTS.md` wird von einem Plan zu einem **Ergebnisdokument mit
einer offenen Zeile**. Konkret:

- Titel und Einleitung auf den Stand: fünf beantwortet, Test 6 offen.
- Je Test einen Kasten **Ergebnis** direkt unter der Frage, mit Datum und
  Quelle. Die Entscheidungstabellen bleiben, der eingetretene Zweig wird
  markiert — daran sieht man, dass die Vorhersage getroffen hat.
- Der Banner oben wird überflüssig, sobald die Ergebnisse im Text stehen.
  Ersatzlos entfernen, statt ihn zu pflegen.
- **Test 6 braucht eine Ergo-Anleitung**, weil er der einzige offene ist und
  Sicherheitsfolgen hat: Mitschnitt an (`POST /api/probe/arm`), Stufe ~10 setzen,
  Ring **vorher** über `GET /api/debug/export` sichern (der Ring liegt im RAM
  und ist nach dem Reset weg), dann Reset **ohne** `08 01`, danach Kurbel von
  Hand prüfen. Die drei Ausgänge und ihre Folgen für den Hub-Watchdog stehen
  schon da und bleiben.
- Der Abschnitt „Bug in der Wirkungsauswertung" wird zwei Sätze lang: was der
  Fehler war, und dass `ControlJournal` ihn strukturell erledigt.

## A9 · `README.md` beschreibt einen Testumfang von vorgestern

| Stelle | Ist | Soll |
|--------|-----|------|
| „**Zwei** Hostsuiten: `test_codec` … `test_limiter`" | 2 | **23 Suiten, 240 `RUN_TEST`-Fälle** |
| „Sollwerte … aus `tools/ftms.py` der Sonde" | falscher Pfad | `tools/ref/ftms.py`, im Repo mitgeliefert |
| Docs-Tabelle | listet 4 Dokumente | `ENTWICKLERDOKU.md`, `KALIBRIERUNG.md`, `BRIDGE.md`, `STATE.md` fehlen |
| „NACHTESTS.md \| sechs **offene** Messungen" | | eine offene |
| „zehn Reiter … **sechs** tragen Inhalt, **vier** sind Platzhalter" | | nach v0.1.0 prüfen und neu zählen |
| API-Tabelle „Editor (**v0.2**)", „Geführte Tests (**v0.2**)" | | beide in v0.1.0 ausgeliefert |

Die Zahl 240 stammt aus `RUN_TEST(`-Zählung über `test/`; nachzählen mit:

```bash
grep -rc '^\s*RUN_TEST(' test/ | awk -F: '{s+=$2} END {print s}'
```

## A10 · Die API-Tabelle im README nennt Endpunkte, die es nicht gibt, und verschweigt rund 25, die es gibt

`src/app/App.cpp` registriert **66** Routen.

**Dokumentiert, aber nicht vorhanden:**
`POST /api/control/target` · `GET /api/history` · `GET/POST /api/test/list`
`/start` · `POST /api/ble/remember` · `GET /api/workout/load`

**Vorhanden, aber nicht dokumentiert:**
`/api/bridge` · `/api/device` · `/api/devices` · `/api/ftp-career` + `/accept`
`/decline` `/set` · `/api/probe/arm` `/clear` `/mark` · `/api/profile/delete` ·
`/api/progression/get` `/accept` `/decline` · `/api/session/list` `/last`
`/annotate` · `/api/control/hr` `/reha` `/sim` `/start` ·
`/api/workout/favorite` `/import` `/resume` `/stop` `/tags`

Besonders unglücklich: die **Probe-API** (`/api/probe/*`) ist das Werkzeug, mit
dem die Nachtests gefahren wurden, und `ENTWICKLERDOKU.md` §6.4 empfiehlt sie —
im README steht sie nicht. Dasselbe für `/api/bridge`, obwohl `BRIDGE.md` seine
Felder im Detail beschreibt.

**Zu tun.** Tabelle aus der Quelle neu erzeugen und thematisch gruppieren
(Shell · BLE · Steuerung · Kalibrierung · Profile · Workouts · Session ·
Bridge · Probe/Debug · OTA). Extraktion:

```bash
grep -o 'server\.on("[^"]*"' src/app/App.cpp | sed 's/.*("//;s/"//' | sort -u
```

Achtung: `/ota-upload` wird mit der vierargumentigen `server.on`-Form über
mehrere Zeilen registriert und fällt bei naiver Suche durch das Raster. Es
existiert (`src/app/App.cpp`, `handleOtaUploadFinish`) — harte Regel 1 ist
erfüllt. Beim Aufräumen nicht versehentlich als fehlend melden.

## A11 · `WEBINTERFACE.md` §8 beschreibt eine Umsetzung, die es nicht mehr gibt

| Aussage in §8 | Wirklichkeit |
|---------------|--------------|
| „Alles PROGMEM im `UiPages.h`-Muster der Familie" | `src/web/UiPages.h` ist ein **298-Byte-Stub**, der nur `UiPagesGz.h` einbindet; Quelle der Wahrheit ist `web/index.html` |
| „**keine Build-Kette**" | `platformio.ini` hat `extra_scripts = pre:tools/pio_pack_ui.py`, das `tools/pack_ui.py` in Python ausführt |
| „Editor und Debug-Panel werden **nachgeladen**, nicht in die erste Seite gepackt" | gegen `web/index.html` prüfen — bei einer einzigen gzip-Seite vermutlich nicht mehr zutreffend |
| „Wenn das Budget nicht reicht, wandert die UI nach **LittleFS**" | anders entschieden: gzip-PROGMEM, laut `STATE.md` §9 rund 33 kB statt 110 kB |

**Zu tun.** §8 neu schreiben, mit der Entscheidung **an dieser Stelle**
festgehalten und nicht nur in `STATE.md`: das Budget wurde nicht durch
Auslagerung gelöst, sondern durch Komprimierung, und LittleFS bleibt nur noch
relevant, wenn die UI ohne Firmware-OTA austauschbar sein soll.

Dabei den neuen Weg beschreiben, weil er nirgends dokumentiert ist:
`web/index.html` → `tools/pack_ui.py` → `include/UiPagesGz.h` → PROGMEM, mit
`tools/pio_pack_ui.py` als PlatformIO-Vorstufe. Und die Folge klar benennen:
**der Firmware-Build braucht jetzt Python.** Das gehört auch in `STATE.md` §1,
wo bisher nur steht, dass die Entwurfs-Instanz keines hat.

## A12 · Drei Flash-Zahlen, keine davon datiert genug

| Fundort | Zahl |
|---------|------|
| `debug/UPDATE_CAPS_FIX.md` | 64,9 % (vor Bridge, vor gzip) |
| `debug/RELEASE_v0.1.0.md` | „~**75** %" plus „LittleFS-Auslagerung empfohlen" |
| `STATE.md` §3 | „Flash ~**72** % nach Bridge-MVP" |

**Zu tun.** Eine aktuelle Zahl, aus einem benannten Build von `0.3.23-dev`. Die
CI baut die Firmware inzwischen, die Zahl ist also beschaffbar. Die alten
Werte bleiben in ihren Protokollen stehen — dort sind sie richtig. Nur
`STATE.md` führt den gültigen Wert, mit Version und Datum daneben.

Dabei die Empfehlung aus `RELEASE_v0.1.0.md` („LittleFS-Auslagerung empfohlen
vor v0.2-Wachstum") auflösen: durch gzip erledigt oder weiterhin offen?

## A13 · Drei Pfadkonventionen für dieselben Sonden-Rohdaten

| Fundort | Pfad |
|---------|------|
| `GERAETEPROFIL.md` Kopf | `docs/ergometer/scan-20260910/` „jenes Repos" |
| `ENTWICKLERDOKU.md` §3.2 und §11 | `nodes/esp32.ftmsprobe/docs/ergometer/scan-20260910/` |
| `README.md` Build-Abschnitt | `../nodes/esp32.ftmsprobe/docs/ergometer/scan-20260910` |

`nodes/…` ist ein Monorepo-Pfad, der in **diesem** Repository nicht existiert.

**Zu tun.** Eine Konvention, einmal erklärt: ob der Pfad relativ zu einem
Monorepo-Checkout oder zu einem Nachbar-Clone gilt. Am saubersten in
`ENTWICKLERDOKU.md` §11 einmal definieren und aus den anderen Dateien darauf
verweisen, statt den Pfad dreimal zu schreiben.

## A14 · `STATE.md` unterläuft langsam seinen eigenen Zweck

Die Datei wurde als **der eine Einstiegspunkt** angelegt, gegen das
Verweisdickicht in `debug/`. Inzwischen stehen rund 25 Zeilen Linkliste vor dem
ersten Abschnitt.

Konkrete Punkte:

- **Kopf:** die Linkliste vor §0 auflösen. Sie ist ein Changelog und gehört in
  das Changelog aus Teil B. Im Kopf bleiben: Stand, Version, und ein Verweis auf
  `ENTWICKLERDOKU.md` und das Changelog. Drei Zeilen, nicht fünfundzwanzig.
- **§1:** „MAC `68B6B329339C` (OTA **0.3.0-dev** ggf. ausstehend)" steht direkt
  über „Aktuelle Bin: `dist/ergo.0.3.23-dev…`". Widersprüchlich, Klammer weg.
- **§2/§3:** §2 heißt „Stufenwirkung — erledigt (2026-09-12)", §3 „Hardware
  2026-09-12 (Kurz)". Zwei Abschnitte zum selben Tag mit unterschiedlichem
  Schnitt. Zusammenlegen oder §3 in das Changelog verschieben.
- **§4/§5:** überwiegend durchgestrichene Zeilen. Durchstreichungen sind ein
  Arbeitsstand, kein Dokument. Erledigtes ins Changelog, hier bleibt nur das
  Offene — vier Zeilen statt zwei Tabellen.
- **§7 ist unvollständig und dadurch irreführend.** Die Tabelle führt
  Kriterien 1–8, 13 und 18. `PFLICHTENHEFT.md` §12 hat **24** Kriterien
  (Labels 1…22 plus 6a und 6b). Für 9–12, 14–17 und 19–22 steht nichts da —
  der Leser kann nicht unterscheiden zwischen „offen" und „vergessen".
  Alle 24 Zeilen aufnehmen, notfalls mit „nicht angefangen".
- **§8** nennt `NACHTESTS.md` ohne den Hinweis, dass es überarbeitet wird, und
  hebt `HW_TEST_REPORT.md` hervor, obwohl `HW_TEST1_60RPM.md`,
  `HW_TEST2_80RPM.md` und `HW_NACHTEST_20260913.md` die tragenden
  Messprotokolle sind.

## A15 · `ENTWICKLERDOKU.md` — kleinere Punkte in einem guten Dokument

Das Dokument ist die stärkste Neuerung der letzten Tage. Die Punkte sind
Feinschliff, keine Kritik am Aufbau.

- **Kopf nennt „Firmware 0.3.21-dev"**, §6.4 spricht von 0.3.23 und `STATE.md`
  von 0.3.23-dev. Der eigene Kopf ist zwei Versionen hinter dem Inhalt. Besser:
  eine Zeile „gültig für 0.3.2x" plus Datum, statt einer Patchversion, die bei
  jedem Release nachgezogen werden muss.
- **`ceilingW`** erscheint als 224 (§7 und `KALIBRIERUNG.md`) und als 226
  (`HW_TEST2_80RPM.md`). Eine Zahl mit Datum, die andere als Momentaufnahme
  kennzeichnen.
- **„~480 Stützstellen" (§7) gegen „62 Zellen belegt" (`KALIBRIERUNG.md`).**
  Beides kann stimmen, wenn „Stützstellen" Einzelmessungen und „Zellen"
  Rasterfelder sind — dann muss genau das dastehen. Im Moment liest es sich wie
  ein Widerspruch.
- **§4.2 zeigt den GATT-Baum mit Auslassung** (`180A Device Information …`),
  `GERAETEPROFIL.md` §2 listet ihn vollständig. Zwei GATT-Darstellungen, die
  auseinanderdriften können. In der Entwicklerdoku kürzen und verlinken.
- **Die Persistenz ist nur in einer Tabellenzeile erwähnt.** Siehe C1 — das ist
  die größte inhaltliche Lücke und gehört in dieses Dokument.

## A16 · `docs/ergometer/README.md`

- Titel „Planung **und Entwicklerdoku**", obwohl `ENTWICKLERDOKU.md` eine
  eigene Datei ist und der Ordner die Planungsebene bildet. Titel schärfen.
- Die Indextabelle listet `STATE.md` nicht (nur im Kasten darüber) und
  `kalibrierung-map-20260913.json` gar nicht, obwohl die Datei im Ordner liegt.
- Schlusssatz „`HrServer` **ist** die direkte Vorlage für `FtmsServer`" —
  `FtmsServer` existiert, also Vergangenheit: „war die Vorlage".

## A17 · `BLE-SCAN.md` ist an eine Person adressiert, nicht an ein Repository

„Fünf Minuten am Bike", „Bitte diese **sechs Punkte** zurückmelden", „Daraus
leite **ich** den Parser und die Steuerlogik ab". Das Dokument soll laut eigenem
Kopf „als Vorlage für weitere Geräte stehen bleiben" — in dieser Form kann es
das nicht.

**Entscheidung nötig** (siehe auch Teil B): entweder in eine allgemeine Anleitung
umschreiben, die nicht von diesem einen Bike und diesem einen Auftrag spricht,
oder als historisches Dokument kennzeichnen und in `docs/ergometer/archiv/`
verschieben. Ich empfehle das Umschreiben: die Checkliste ist inhaltlich gut,
und ein zweites Gerät ist realistisch. Dabei die Zeile „Bridge in v0.2" aus A2
mitkorrigieren und den Hinweis ergänzen, dass `04 0A` beim Varon quittiert wird
und **nicht** wirkt — genau der Fall, den Schritt 4 der Checkliste abfragt.

## A18 · Die Nummerierung der Abnahmekriterien ist keine Anzahl

`PFLICHTENHEFT.md` §12 läuft über die Labels 1…22, mit zusätzlich eingeschobenen
6a und 6b — **24 Kriterien bei Höchstlabel 22**.

**Zu tun.** Entweder durchnummerieren auf 1…24 und jede Referenz mitziehen, oder
die Labels so lassen und **einmal** dazuschreiben, wie viele es sind und warum
6a/6b eingeschoben wurden. Zweiteres ist billiger und ehrlicher. In jedem Fall
darf keine Stelle im Projekt eine Kriterienanzahl aus dem Höchstlabel ableiten.

---

# Teil B — Ausmisten

`debug/` enthält **61 Markdown-Dateien**. Rund vierzig davon sind 11 bis 33
Zeilen lang und beschreiben je eine Dev-Version. Das ist kein Dokumentenbestand,
das ist ein Changelog in Dateiform.

## B1 · Ein Changelog anlegen

Neu: **`CHANGELOG.md`** im Wurzelverzeichnis, eine Sektion je Version,
absteigend, im Format „`## 0.3.17-dev` — was, warum, Beleg". Der Inhalt kommt
aus den bestehenden Notizen; jede Sektion drei bis sechs Zeilen. Wo eine Notiz
eine Begründung enthält, die sonst verloren geht, wandert der Satz mit.

Danach werden die folgenden Dateien **gelöscht**, weil ihr Inhalt vollständig im
Changelog aufgeht:

```
UPDATE_BRIDGE.md              UPDATE_BRIDGE_EXCLUSIVE.md
UPDATE_BRIDGE_FAST_RAMP.md    UPDATE_BRIDGE_RESIST.md
UPDATE_CONTROL_UI.md          UPDATE_DEV_UI.md
UPDATE_DEVICE_STORE.md        UPDATE_ERG.md
UPDATE_ERG_SIM_ASSIST.md      UPDATE_ERG_SLEW.md
UPDATE_FTP_BUILTINS.md        UPDATE_FTP_CAREER.md
UPDATE_GHOST.md               UPDATE_HR_HOLD.md
UPDATE_INTERVAL_EDITOR.md     UPDATE_NIGHT.md
UPDATE_PROFILE_MODE.md        UPDATE_PROFILE_PERSIST.md
UPDATE_PROFILES_UI.md         UPDATE_PROGRESSION.md
UPDATE_REHA.md                UPDATE_RIDE_RAMP.md
UPDATE_RIDE_UI.md             UPDATE_SESSION.md
UPDATE_SIM_MODE.md            UPDATE_TABLET_RIDE.md
UPDATE_TESTS_UI.md            UPDATE_UI_RIDE.md
UPDATE_UI_TESTRUNNER.md       UPDATE_WORKOUT.md
UPDATE_WORKOUT_EDITOR.md      UPDATE_WORKOUT_META.md
UPDATE_WORKOUT_TAGS.md        UPDATE_WORKOUT_UI.md
UPDATE_WORKOUT_UX.md          UPDATE_ZONES.md
UPDATE_ZWO_IMPORT.md          RELEASE_v0.3.22.md
RELEASE_v0.3.23.md
```

Das sind **39 Dateien**. Sie sind nicht wertlos — sie sind bereits eingelöst:
ihr Inhalt steckt in `BRIDGE.md`, `ENTWICKLERDOKU.md`, `WEBINTERFACE.md` und im
Code. Die Git-Historie behält sie ohnehin.

Zwei Ausnahmen aus dieser Liste prüfen, bevor gelöscht wird:
`UPDATE_ERG_SLEW.md` und `UPDATE_BRIDGE_RESIST.md` enthalten Parameterwerte
(Slew-Zeiten, das 3-Sekunden-Fenster gegen ERG-Spam). Falls diese Werte nicht
vollständig in `BRIDGE.md` §4 stehen, gehören sie **dorthin**, bevor die Notiz
verschwindet.

## B2 · Überholte Anweisungsdokumente archivieren

Diese Dateien sind Aufträge, die erledigt sind. Sie zu löschen wäre falsch — sie
dokumentieren, *wie* das Projekt geführt wurde. Sie zu belassen ist auch falsch,
weil sie sich als Anweisung lesen.

Neu: **`debug/archiv/`**. Dorthin verschieben, mit einer `archiv/README.md`, die
in fünf Zeilen sagt: erledigte Aufträge und Protokolle, als Anweisung nicht mehr
gültig, Stand jeweils im Kopf.

```
TODO_WIFI_OTA.md          TODO_PFLICHTENHEFT.md     RECOMMENDATIONS.md
UPDATE_CONNECTIVITY.md    UPDATE_BLE.md             UPDATE_LIMITER.md
FIRST_BUILD.md            HW_TEST_REPORT.md
```

`RECOMMENDATIONS.md` trägt bereits „ersetzt" im Titel — genau der Zustand, für
den es das Archiv gibt.

## B3 · Was in `debug/` bleibt

| Datei | Warum sie bleibt |
|-------|------------------|
| `README.md` | Wegweiser, muss nach dem Umbau neu geschrieben werden |
| `RELEASE_v0.1.0.md` | einziges echtes Release-Dokument, mit Messwerten |
| `HW_TEST1_60RPM.md` | Primärmessung, aus `NACHTESTS.md` und `ENTWICKLERDOKU.md` verlinkt |
| `HW_TEST2_80RPM.md` | dito |
| `HW_TEST2_LIGHT.md` | Vorläufer, aus `ENTWICKLERDOKU.md` §7.2 verlinkt |
| `HW_NACHTEST_20260913.md` | Primärprotokoll Nachtest 4/5/Reconnect |
| `UPDATE_NACHTEST_RESULTS.md` | Primärprotokoll Nachtest 3 am Draht |
| `UPDATE_NACHTEST_PROBE.md` | beschreibt die Probe-API — **gehört inhaltlich ins README** (A10), danach archivieren |
| `UPDATE_NACHTEST_FORCE.md` | dokumentiert `force=1`, den bewussten Limiter-Bypass — sicherheitsrelevant, gehört ins `GERAETEPROFIL.md` §3, danach archivieren |
| `UPDATE_CAPS_FIX.md` | die teuerste Lektion des Projekts, aus README und `STATE.md` verlinkt |
| `UPDATE_FIXTURE_6A.md` | Beleg für Abnahmekriterium 6a, aus der CI referenziert |
| `PLATFORMIO_FIX.md` | beschreibt eine **weiterhin scharfe** Falle (harte Regel 2) |
| `commands.sh` | Werkzeug |
| `captures/`, `calib/` | Rohdaten, unverändert lassen |

Damit geht `debug/` von 61 auf **13 Markdown-Dateien** plus Archiv.

## B4 · Ein Wort zu `debug/captures/`

`nachtest-20260913T085205Z.jsonl` ist **536 kB** und damit größer als der
gesamte übrige Dokumentenbestand. Als Beleg für ein Primärprotokoll ist das in
Ordnung, und Git kommt damit zurecht. Aber es sollte eine Zeile in
`captures/README.md` geben: welcher Mitschnitt zu welchem Protokoll gehört, mit
welcher Firmware und welchem `ibdEvery` aufgezeichnet, und ob er als
Codec-Fixture taugt. Ohne das ist es in einem halben Jahr nur noch ein großes
File.

`powermap-latest.json` und `powermap-20260912T124351Z.json` sind
**byte-identisch**. „latest" als Dauerkopie veraltet lautlos. Entweder löschen
und im Dokument auf die datierte Datei zeigen, oder als Symlink führen.

## B5 · Was auf keinen Fall gelöscht wird

`docs/ergometer/` bleibt vollständig. Alle acht Dokumente dort haben eine
eigene Rolle, auch `BLE-SCAN.md` (nach A17). Die Planungsebene ist kein
Ausmist-Ziel — sie ist der Grund, warum dieses Projekt in drei Tagen so weit
gekommen ist.

---

# Teil C — Was fehlt

## C1 · Persistenz — die größte Lücke

Im gesamten Dokumentenbestand gibt es **eine** Zeile zur Persistenz:
`PFLICHTENHEFT.md` §4 nennt „NVS-Namespace `esphub`". Der Code benutzt **fünf**
NVS-Namespaces —

```
esphub      geteilt mit der Familie (Gerätename, WLAN)
ergo        Config dieses Knotens
ergodev     DeviceStore, Geräteprofile je MAC
ergomap     Kennfläche, Slot je Gerät
ergoprofs   Nutzerprofile
```

— plus LittleFS für Workouts und Sessions.

**Neuer Abschnitt in `ENTWICKLERDOKU.md`:** je Namespace die Schlüssel, das
Format, die Versionierung und das Verhalten beim Upgrade. Dazu das
LittleFS-Layout: welche Pfade, welche Dateinamen, was passiert beim Volllaufen.

Warum das wichtig ist: ein Formatwechsel ohne Migration macht Profile oder die
Kennfläche lautlos unbrauchbar, und die Kennfläche ist ein Messergebnis, das
Stunden auf dem Rad gekostet hat. `PowerMap` hat eine Magic und eine
Versionsnummer — das ist dokumentationswürdig, weil es die einzige Stelle ist,
an der ein Upgrade schiefgehen darf, ohne dass jemand es merkt.

## C2 · Das Statusobjekt

`/api/status` ist die Schnittstelle, an der UI, Hub und jedes Debugging hängen.
Der README beschreibt unter „Hub-IOs" eine Teilmenge der Felder. Die Struktur
selbst — `ftms`, `calib`, `debug`, `bridge`, `limiter`, `session`, `profile` —
ist nirgends vollständig beschrieben.

**Vorschlag:** ein kommentiertes Beispiel-JSON in `ENTWICKLERDOKU.md`, erzeugt
aus einem echten Aufruf bei stehendem Bike-Link, mit den Feldern, die man ohne
Quellcode nicht erraten kann: `target_reachable`, `clientRole`, `resistIgnored`,
`levelWantTenths`, `contradictory`, `hrSource`, `passive.accept`. Nicht jedes
Feld — die, die eine Entscheidung tragen.

## C3 · Ein Sicherheitsabschnitt

Die harten Regeln stehen in `STATE.md` §6 und `ENTWICKLERDOKU.md` §9, aber es
gibt keine Stelle, die zusammenhängend beschreibt, **was schiefgehen kann und
was dann passiert**. Das Material ist vollständig vorhanden, nur verstreut:

- Not-Stop-Pfade und wer sie auslösen darf (auch die Bridge, auch bei Lock)
- Verhalten bei Pulsverlust je Profil
- Verhalten bei Bike-Verlust unter Last
- Der Hub-Watchdog und warum Nachtest 6 ihn entscheidet
- `force=1` und `allowUntrustedPower` — die bewussten Bypässe, wer sie setzen
  darf und dass sie nach einem Test zurückgestellt gehören
- Die Stufenrampe und warum die Bridge sie auf 500 ms verkürzen darf

Ein Gerät, das einem Menschen unter dem Fuß Widerstand stellt, sollte einen
solchen Abschnitt haben. Er gehört in `ENTWICKLERDOKU.md`, nicht in `STATE.md` —
er veraltet langsam.

## C4 · Der Befund Bike-HR gegen Gurt ist zu leise dokumentiert

`ENTWICKLERDOKU.md` §5 nennt „Mittel Bike-HR − Strap ≈ +25 bpm" und schließt mit
„für `HR_HOLD` nur mit Vorsicht". Für einen Reha-Pulsdeckel ist ein
systematischer Fehler von 25 Schlägen kein Vorsichtshinweis, sondern ein
Ausschlussgrund.

**Zu tun.** Zwei Sätze in `ENTWICKLERDOKU.md` §5 und in `WEBINTERFACE.md` §4
(Pulsverlust-Verhalten): welche Quellen für `HR_HOLD` und den Reha-Deckel
zugelassen sind. Und eine Frage an den Code, die ich nicht beantworten konnte:
**verweigert die Firmware `HR_HOLD` und Reha, wenn `hrSource=machine` ist?**
Falls ja, dokumentieren. Falls nein, ist das ein Befund für `STATE.md` §5.

## C5 · Eine Einstiegsseite für Menschen

Alle Dokumente sprechen zu jemandem, der das Projekt baut. Es gibt keine Seite
für jemanden, der das Gerät **benutzt**: Profil wählen, Bike verbinden, fahren,
Workout starten, Session ansehen. `WEBINTERFACE.md` beschreibt das Design,
nicht die Bedienung.

Das ist kein Mangel, solange zwei Personen das Gerät benutzen, die es kennen.
Es wird einer, sobald jemand Drittes davorsteht — und `README.md` verspricht mit
Donate-Button und GPL implizit ein Publikum. Ein kurzes `BEDIENUNG.md`, eine
Seite, mit den fünf Handgriffen und den drei Dingen, die überraschen
(Konsolendisplay bleibt aus, nur ein Central, Stufe ist ein Schattenwert).

## C6 · Kleinere Ergänzungen

- **Ein Glossar.** `W/rpm`, `Kennfläche`, `Schattenwert`, `Stützstelle`,
  `Kadenzband`, `Ceiling`, `Slew`, `Takeover`, `Observer`/`Controller`,
  `unjudged`, `contradictory`. Ein Dutzend Zeilen in `ENTWICKLERDOKU.md`.
- **Die Rolle der drei Werkzeuge in `tools/`** — `hand-proof.sh`, `hw_smoke.sh`,
  `nachtest_watch.sh` sind nirgends beschrieben. `hand-proof.sh` klingt nach
  genau dem Handkurbel-Beweis und sollte in `NACHTESTS.md` verlinkt sein.
- **Warum `min_spiffs.csv`** und wie groß die App-Partition dadurch ist. Die
  Zahl 1966080 taucht in Protokollen auf, nie mit Begründung.

---

# Teil D — Ist die Doku ausführlich genug?

**Kurz: ja, an den richtigen Stellen sogar auffallend gut — aber sie ist
unausgewogen, und sie hat eine Schlagseite zum Protokoll.**

## Was überdurchschnittlich ist

`GERAETEPROFIL.md` und `ENTWICKLERDOKU.md` erklären nicht nur, was gilt, sondern
warum es nicht anders geht, und sie unterscheiden gemessen von angenommen. Die
Auswertung der Bedienungsanleitung in `GERAETEPROFIL.md` §11 — die Erkenntnis,
dass der Watt-Modus der Konsole selbst eine Emulation ist — ist der Grund, warum
`PowerController` von Anfang an richtig entworfen wurde. Solche Abschnitte
findet man in Hobbyprojekten fast nie.

`BRIDGE.md` ist ein Musterbeispiel: es hält Betriebsregeln, die aus einer Live-
Session folgen, mitsamt dem Satz, der sie ausgelöst hat („Apps kämpfen").

## Was zu dünn ist

Die Persistenz (C1), das Statusobjekt (C2), die Sicherheitsbetrachtung (C3) und
die Werkzeuge in `tools/` (C6). Das sind genau die Bereiche, in denen ein
Nachfolger — oder dieselbe Person in sechs Monaten — Zeit verliert, weil die
Antwort nur im Code steht.

## Was zu breit ist

Die 39 Einzelnotizen in `debug/`. Sie erzeugen den Eindruck, gut dokumentiert zu
sein, und kosten trotzdem Zeit, weil jede Suche 61 Treffer liefert und man jeden
davon auf Aktualität prüfen muss. `STATE.md` warnt selbst davor
(„nicht als ‚aktuell' lesen") — eine Warnung, die nötig ist, weil die Struktur
sie nötig macht.

## Die eigentliche Schwäche

**Es fehlt eine Schicht zwischen Konzept und Protokoll: die Beschreibung des
gebauten Systems.** `PFLICHTENHEFT.md` sagt, was werden soll.
`debug/*` sagt, was an einem Tag getan wurde. `ENTWICKLERDOKU.md` füllt die
Lücke zu etwa zwei Dritteln — es beschreibt Technologien, Messungen und
Entscheidungen, aber nicht durchgehend die **Funktionsweise** der Bausteine.

Konkret: es gibt keine Stelle, an der steht, wie `PowerController` aus einem
Wattziel eine Stufe macht, wie `HrController` regelt, was `WorkoutEngine` als
Schrittzustand führt, oder wie `RehaController` Leistungsziel und Pulsdeckel
gegeneinander abwägt. Das sind die vier Bausteine, die das Gerät zu einem
Trainingsrechner machen, und sie sind nur als Name dokumentiert. Ihre Header
tragen laut Stichprobe gute Kommentare — aber ein Leser findet sie nur, wenn er
weiß, dass er sie suchen muss.

**Empfehlung:** ein Abschnitt „Regelung" in `ENTWICKLERDOKU.md`, je Baustein
zehn bis fünfzehn Zeilen: Eingänge, Ausgang, Zustand, Grenzen, und die eine
Entscheidung, die man hätte anders treffen können. Das ist der einzige größere
Schreibauftrag in diesem Dokument und der wertvollste.

---

# Teil E — Die lesbare Variante

Gewünscht ist eine im Browser lesbare Fassung. Ich schlage **ein einziges,
in sich geschlossenes HTML-Dokument** vor, erzeugt aus den Markdown-Quellen.

## E1 · Warum ein Generator und nicht ein statischer Export

Weil der Generator nebenbei **die Querverweise prüft**. Mehrere Punkte aus Teil A
sind gebrochene oder veraltete Verweise — genau die Fehlerklasse, die ein
Linkcheck automatisch findet. Ein Werkzeug, das die Doku baut und dabei jeden
`.md`-Link validiert, verhindert den nächsten Durchgang dieses Audits.

## E2 · Spezifikation `tools/docs_html.py`

**Aufruf**

```bash
python tools/docs_html.py                  # baut docs/HANDBUCH.html
python tools/docs_html.py --check          # nur Linkprüfung, Exitcode 1 bei Fehler
```

**Eingabe:** eine **explizite, geordnete Liste** im Skript — keine Glob-Suche.
Die Reihenfolge ist redaktionell und soll sichtbar im Code stehen:

```
STATE.md
CHANGELOG.md
README.md
docs/ergometer/ENTWICKLERDOKU.md
docs/ergometer/PFLICHTENHEFT.md
docs/ergometer/GERAETEPROFIL.md
docs/ergometer/KALIBRIERUNG.md
docs/ergometer/NACHTESTS.md
docs/ergometer/BRIDGE.md
docs/ergometer/WEBINTERFACE.md
docs/ergometer/BLE-SCAN.md
debug/RELEASE_v0.1.0.md
debug/UPDATE_CAPS_FIX.md
debug/HW_TEST1_60RPM.md
debug/HW_TEST2_80RPM.md
debug/HW_NACHTEST_20260913.md
debug/UPDATE_NACHTEST_RESULTS.md
debug/PLATFORMIO_FIX.md
```

**Ausgabe:** `docs/HANDBUCH.html`, eine Datei, ohne Netzzugriff lesbar. Keine
CDN-Abhängigkeit — wer die Doku liest, sitzt womöglich neben dem Bike in einem
Keller.

**Rendering:** Python-Markdown mit den Erweiterungen `tables`, `fenced_code`,
`toc`, `attr_list`, `sane_lists`, `admonition`. Das ist eine Zeile in
`requirements-docs.txt` (`markdown>=3.5`) und in der CI schon verfügbar, weil
dort ohnehin Python läuft. Falls `markdown` fehlt, soll das Skript mit einer
klaren Meldung abbrechen statt halb zu rendern.

**Linkumschreibung.** Alle `.md`-Verweise werden auf Anker im Dokument
umgeschrieben: `GERAETEPROFIL.md` → `#doc-geraeteprofil`,
`../../STATE.md` → `#doc-state`, `NACHTESTS.md#test-6` →
`#doc-nachtests--test-6`. Relative Pfade (`../../debug/…`) vorher normalisieren.

**Prüfungen — das ist der Kern.** Abbruch mit Exitcode 1 und Fundortangabe bei:

1. einem `.md`-Link auf eine Datei, die nicht in der Liste steht
2. einem `.md`-Link auf eine Datei, die nicht existiert
3. einem `#anker`-Verweis, der im Ziel nicht vorkommt
4. einer Datei im Repo, die `.md` ist und weder in der Liste noch unter
   `debug/archiv/` liegt — so fällt eine neue Notiz auf, statt lautlos
   unverlinkt zu bleiben

Regel 4 ist die eigentliche Versicherung gegen einen zweiten Wildwuchs.

**Layout.** Feste Seitenleiste mit zweistufigem Inhaltsverzeichnis (Dokument und
`##`-Ebene), Inhalt daneben mit begrenzter Zeilenlänge. Die Farben aus der
Geräte-UI übernehmen, damit Doku und Gerät zusammengehören:

```
--bg:#101419  --fg:#E6EAF2  --dim:#8A94A6
--accent:#E2802F  --ok:#4CAF63  --bad:#C9304A  --edge:#232A34
```

Tabellen mit `font-variant-numeric: tabular-nums` — die Doku besteht zu einem
guten Teil aus Messtabellen, und Ziffern, die springen, liest niemand gern.
Codeblöcke mit Umbruch statt waagerechtem Scrollen. Dazu ein `@media print`, das
die Seitenleiste ausblendet und Links als Fußnoten auflöst, damit ein PDF für
die Ablage entsteht.

**Kopfzeile der Ausgabe:** Erzeugungsdatum, `git rev-parse --short HEAD`, die
Firmware-Version aus `platformio.ini` und ein Satz, dass die Datei generiert ist
und Änderungen in die Markdown-Quellen gehören.

**`.gitignore`:** `docs/HANDBUCH.html` wird **nicht** eingecheckt. Sie ist ein
Build-Ergebnis. Einchecken würde bei jedem Doku-Kommentar einen 300-kB-Diff
erzeugen.

## E3 · In die CI

In `.github/workflows/build.yml`, Job `native-tests`, nach dem
Fixture-Roundtrip:

```yaml
      - name: Doku-Links prüfen
        run: |
          pip install markdown
          python tools/docs_html.py --check
```

Damit ist die Doku so geprüft wie der Code — und das ist der Punkt, an dem
dieses Audit sich nicht wiederholen muss.

---

# Definition of Done

1. Teil A vollständig abgearbeitet; für jeden Punkt entweder die Korrektur oder
   eine Zeile, warum sie nicht möglich war (typisch: Zahl nicht beschaffbar).
2. `CHANGELOG.md` existiert und deckt 0.1.0 bis 0.3.23-dev ab.
3. `debug/` enthält 13 Markdown-Dateien plus `archiv/` plus `captures/` und
   `calib/` mit je einer `README.md`.
4. `STATE.md` hat einen dreizeiligen Kopf, keine Durchstreichungen im
   laufenden Text und eine vollständige Kriterientabelle mit 24 Zeilen.
5. `ENTWICKLERDOKU.md` hat die neuen Abschnitte: Persistenz (C1), Statusobjekt
   (C2), Sicherheit (C3), Regelung (Teil D), Glossar (C6).
6. `tools/docs_html.py` baut `docs/HANDBUCH.html` und läuft mit `--check` grün.
7. Der Linkcheck läuft in der CI.
8. Notiz für die Entwurfs-Instanz: was geändert wurde, welche Zahlen noch offen
   sind, und welche Fragen aus C4 der Code beantworten muss.

# Was nicht angefasst wird

- `src/` und `test/`, mit der einen Ausnahme der falschen Paketzahl im Kopf von
  `fixtures_ibd.h`
- `docs/ergometer/kalibrierung-map-20260913.json` und alles unter
  `debug/captures/` und `debug/calib/` — Rohdaten bleiben Rohdaten
- Die harten Regeln in `STATE.md` §6 und `ENTWICKLERDOKU.md` §9. Sie dürfen
  umformuliert, aber nicht abgeschwächt werden. Wenn eine Regel überholt
  erscheint, gehört das in die Abschlussnotiz — nicht in eine stille Änderung.
