# Webinterface `esp32.ergo` — Designkonzept

Diese UI ist nicht die Beigabe zur Firmware, sie ist das Produkt. Während des
Trainings ist sie die **einzige** Anzeige, weil das Konsolendisplay des Bikes
bei bestehendem BLE-Link abschaltet. Sie muss also alles zeigen, was sonst die
Konsole zeigt — und dabei so aussehen, dass man sie freiwillig anschaut.

Gehört zum [Pflichtenheft](PFLICHTENHEFT.md), §7 dort ist die Kurzfassung.

---

## 1. Die vier Leitentscheidungen

**Die Zone führt die Optik.** Das ist die eine Sache, die MyWhoosh richtig
macht: man erkennt die Belastung, ohne eine Zahl zu lesen. Der ganze
Hintergrund, der Akzent und das Glühen der Hero-Zahl folgen der aktiven Zone.
Aus zwei Metern und außer Atem liest man Farbe, nicht Ziffern.

**Tablet-First, nicht Desktop-verkleinert.** Gelesen wird aus zwei Metern
Entfernung, mit Puls 150, verschwitzten Fingern und ohne Maus. Keine
Hover-Zustände, keine Tooltips, kein Bedienelement kleiner als ein Daumen,
keine Information, die man sich aus zwei Werten zusammenrechnen muss.

**Eine Hero-Zahl, und die hängt am Profil.** Für Leistungstraining ist es Watt.
Für Reha-Training ist es der Puls. Das ist keine Einstellung für Feinschmecker,
sondern der Unterschied zwischen „hilfreich" und „gefährlich" — wer auf eine
Pulsgrenze trainiert, muss den Puls groß sehen und nicht suchen.

**Ehrlichkeit über den Stellweg.** Das Bike hat 16 Widerstandsstufen und
liefert keine Rückmeldung darüber. Was der ESP32 gestellt hat, was davon
ankommt und wo die Decke ist, gehört sichtbar in die UI. Eine UI, die ein
unerreichbares Wattziel schön anzeigt, lügt.

---

## 2. Visuelle Sprache

### Grundton

Dunkler Graphit als Basis, darüber ein atmosphärischer Verlauf in der Farbe der
aktiven Zone. Nicht flächig bunt — die Zone glüht aus dem Hintergrund und um
die Hero-Zahl, die Flächen selbst bleiben ruhig.

| Element | Behandlung |
|---------|-----------|
| Grund | Graphit `#0E1116`, Karten `#161A21` mit 1 px Kante `#232936` |
| Zonen-Ambient | radialer Verlauf hinter der Hero-Zone, Zonenfarbe bei 12–18 % Deckkraft |
| Hero-Zahl | Zonenfarbe, leichtes Glühen, Ziffern tabellarisch (kein Springen) |
| Text | `#E6EAF2` primär, `#8A94A6` sekundär |
| Display-Font | Syne, für Hero-Zahlen und Marke |
| Mono-Font | IBM Plex Mono, für alle Messwerte und Metadaten |
| Motion | Zonenwechsel 400 ms Überblendung, Zahlen nie animiert hochzählen |

### Zonenpalette

| Zone | Leistung (% FTP) | Farbe | Name |
|------|------------------|-------|------|
| Z1 | < 55 | `#3FB8B0` Teal | Aktive Erholung |
| Z2 | 56–75 | `#4CAF63` Grün | Grundlage |
| Z3 | 76–90 | `#D8B23A` Gelb | Tempo |
| Z4 | 91–105 | `#E2802F` Orange | Schwelle |
| Z5 | 106–120 | `#DE5334` Rot-Orange | VO2max |
| Z6 | 121–150 | `#C9304A` Rot | Anaerob |
| Z7 | > 150 | `#A63FB0` Violett | Neuromuskulär |

Pulszonen nutzen dieselben fünf ersten Farben. Das ist bewusst: es gibt immer
nur **eine** führende Zone, die die Optik bestimmt, und die legt das Profil
fest. Die zweite Zone wird numerisch und als Position gezeigt, nie als
zweite Ambient-Farbe. Zwei konkurrierende Farbsysteme auf einem Schirm sind
unlesbar.

**Farbe ist niemals der einzige Träger.** Jede Zone zeigt zusätzlich ihr Kürzel
(`Z3`) und ihren Namen. Das hilft bei Farbfehlsichtigkeit und bei Sonnenlicht
auf dem Display.

---

## 3. Ride — die Trainingsansicht

### Layout

```
┌──────────────────────────────────────────────────────────────────┐
│  ◐ MARTIN        ● READY   ERG 220 W   BRIDGE ○   ⬤ SSE         │
│  TC174 · HR-Relay · 00:24:11 · 8,4 km · 312 kJ                  │
├───────────────────────────────────┬──────────────────────────────┤
│                                   │  ♥  148        Z3  Tempo    │
│         ╭─────────────╮           │     BPM        78 % HRmax   │
│         │     214     │           │                              │
│         │    WATT     │           │  ⟳   87 rpm    ▁▃▅▃▁        │
│         ╰─────────────╯           │      halten                  │
│          Ziel 220  ·  −6 W        │                              │
│                                   │  ⚙   12/16     ▓▓▓▓▓▓▓▓▓▓░░ │
│   Z3 TEMPO                        │      Stufe     Reserve 4     │
├───────────────────────────────────┴──────────────────────────────┤
│  Z1 ▓▓  Z2 ▓▓▓▓▓▓▓  Z3 ▓▓▓▓▓▓▓▓▓▓▓▓▉  Z4 ░  Z5 ░  Z6 ░  Z7 ░   │
│  3:10     8:42        11:20 ▲aktuell    –     –     –     –      │
├──────────────────────────────────────────────────────────────────┤
│  4×4 Norweger  ·  Block 2/4 Work  ·  01:12  ·  gesamt 18:40     │
│  ▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   │
├──────────────────────────────────────────────────────────────────┤
│  Leistung: Ist vs Ziel                              15 min      │
│  ████████████████ canvas ██████████████████████████████████     │
├───────────────────────────────┬──────────────────────────────────┤
│  Puls + Zonenband             │  Kadenz + Stufe                 │
├───────────────────────────────┴──────────────────────────────────┤
│  [ ■ STOP ]  [ ⏸ ]     Watt −5 / +5     Stufe − / +             │
└──────────────────────────────────────────────────────────────────┘
```

### Die Zonenschiene

Die Leiste unter der Hero-Zone ist das Element, das MyWoosh-Gefühl erzeugt:
sieben Segmente, jedes füllt sich mit der **in dieser Zone verbrachten Zeit**,
und ein Marker zeigt, wo man gerade steht. Man sieht auf einen Blick zwei
Dinge — die aktuelle Belastung und die Verteilung der ganzen Einheit. Am Ende
ist diese Leiste von selbst die Auswertung.

### Die Kadenz-Kachel verdient Erklärung

Auf diesem Bike hängt die Leistung bei fester Stufe **an der Kadenz**. Läuft
die Kadenz weg, muss `PowerController` die Stufe nachziehen, und das dauert.
Deshalb bekommt die Kadenz einen Hinweis, den kein kommerzielles Produkt
braucht: eine kleine Zielspanne und ein Wort dazu — `halten`, `schneller`,
`langsamer`. Wer die Kadenz ruhig hält, bekommt eine viel genauere
Leistungsregelung. Das ist keine Gängelung, sondern die Erklärung dafür, warum
ERG hier manchmal zappelt.

### Die Stufen-Kachel ist die Ehrlichkeitsanzeige

16 Segmente, die gestellte Stufe gefüllt, dazu die verbleibende Reserve. Steht
sie auf `16/16`, ist die Decke erreicht und das Wattziel nicht fahrbar — dann
wird die Kachel rot umrandet und das Ziel in der Kopfzeile durchgestrichen
gezeigt. Kein stilles Klemmen.

Da das Bike die Stufe nicht zurückmeldet, ist der Wert ein Schattenwert. Die
UI kennzeichnet das durch ein kleines `~` — was der ESP32 gestellt hat, nicht
was gemessen wurde.

### Der Ist-vs-Ziel-Chart

Zielleistung als Stufenlinie, gefahrene Leistung als gefüllte Fläche darunter,
beides in Zonenfarbe. Bei unerreichbarem Ziel wird die Zielstufenlinie
gestrichelt. Die Geisterlinie der besten früheren Session desselben Workouts
läuft als dünne helle Kurve mit.

Canvas, selbst gezeichnet — wie in rfmonitor und heartrate, keine
Chart-Bibliothek.

### Reha-Variante der Ride-Seite

Für ein Profil mit Pulsführung wechselt die Hero-Zahl auf den Puls, und die
Leistung rutscht in die Nebenspalte. Zusätzlich erscheint der Deckel als
sichtbare Linie:

```
┌──────────────────────────────────────────────────────────────────┐
│  ◐ ANNA          ● READY   PHYSIO 60 W · Puls ≤ 120             │
├───────────────────────────────────┬──────────────────────────────┤
│                                   │  ⚡  58 W                    │
│              112                  │      Ziel 60                 │
│              BPM                  │                              │
│         ▏▏▏▏▏▏▏▏▏░░  120          │  ⟳   54 rpm   halten         │
│         Deckel in 8 Schlägen      │  ⚙   7/16                    │
│                                   │                              │
│   Z2 RUHIG · Deckel aktiv nein    │                              │
├───────────────────────────────────┴──────────────────────────────┤
│  Physio 10 min  ·  06:12 von 10:00  ·  Deckel griff 0×          │
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░                   │
└──────────────────────────────────────────────────────────────────┘
```

Der Balken unter der Pulszahl ist eine Annäherungsanzeige an den Deckel, nicht
eine Zone. Er füllt sich zum Grenzwert hin und wird ab dem Anfahrband bernstein.
Greift der Deckel und die Leistung wird gesenkt, sagt die UI das in Worten —
nicht nur durch eine sinkende Zahl.

---

## 4. Profile

Zwei Menschen, ein Bike. Das ist keine Komfortfunktion: eine Pulsobergrenze,
die am falschen Profil hängt, ist ein Sicherheitsproblem.

### Was ein Profil enthält

| Gruppe | Felder |
|--------|--------|
| Identität | Name, Farbe, optional Initiale als Avatar |
| Leistung | FTP in W, Datum und Herkunft (Test, Schätzung, manuell) |
| Puls | HRmax, Ruhepuls, optional LTHR, Zonenmodell (% HRmax oder % LTHR) |
| Körper | Gewicht für W/kg |
| Darstellung | **Führende Zone**: Leistung oder Puls |
| Grenzen | max. Leistung, max. Puls, max. Stufe — harte Klemmen |
| Verhalten | Vorgabe-Kadenz, Reaktion bei Pulsverlust |
| Verlauf | Sessions, Bestleistungen, Testhistorie, Physio-Progression |

Die **Kennfläche Stufe × Kadenz → Watt gehört nicht ins Profil.** Sie ist eine
Eigenschaft des Bikes und wird geteilt. Nur so lernt sie aus beiden Nutzern
mit.

### Profilwahl

Die Profilkarten sind der erste Schirm vor jeder Einheit — große Karten mit
Namen und Farbe, ein Fingertipp. **Es gibt kein stilles Standardprofil.** Ohne
gewähltes Profil lässt sich keine Session starten, weil sonst die Grenzen des
letzten Nutzers weitergelten würden.

Das aktive Profil steht permanent oben links mit seiner Farbe. Ein Wechsel
mitten in einer Session ist nicht möglich — erst beenden, dann wechseln.

### Unterschiedliches Verhalten bei Pulsverlust

Fällt der Gurt aus, ist die richtige Reaktion je Person verschieden:

| Einstellung | Verhalten | Für wen |
|-------------|-----------|---------|
| `freeze` | Stufe halten, warnen, nach Timeout auf `MANUAL_LEVEL` | Leistungstraining |
| `reduce` | Stufe auf ein sicheres Niveau senken, warnen | **Vorgabe für Reha-Profile** |
| `stop` | Session pausieren, Last weg | maximale Vorsicht |

**Pulsquelle:** Für `HR_HOLD` und Reha-Deckel sind nur **Gurt** oder **HR-Relay**
zulässig (`hrUsable`). Bike-HR (`machine`, ≈ +25 bpm gegen Strap) wird abgelehnt:
HTTP 409 beim Start, während der Fahrt als Pulsverlust (gleiche Politik wie
Gurtabriss). Die UI deaktiviert die Bedienelemente und nennt den Grund im
Klartext.

---

## 5. Workouts

Beides ist gewünscht und beides kommt: Datei **und** Editor. Die Datei ist die
Wahrheit, der Editor schreibt sie.

### Schritt-Typen und Ziele

| Ziel | Feld | Bemerkung |
|------|------|-----------|
| Absolute Leistung | `power: 60` | für Reha und feste Vorgaben |
| Relative Leistung | `ftp_pct: 88` | macht das Workout zwischen Profilen übertragbar |
| Widerstandsstufe | `level: 8` | direkt, ohne Regelung |
| Zielpuls | `hr: 140` | geregelt |
| Pulsdeckel | `limit: { hr_max: 120, hr_soft: 115 }` | **zusätzlich** zu einem Leistungsziel |

Die letzte Zeile ist der Kern des Physio-Programms: ein Leistungsziel **mit**
einer Pulsobergrenze. Der Deckel greift nicht erst bei 120, sondern fängt bei
115 an gegenzuhalten — Puls reagiert träge, ein harter Schwellwert bei 120
würde regelmäßig darüber schießen.

```json
{
  "name": "Physio Grundlage",
  "profile_hint": "anna",
  "progression": { "field": "duration_s", "step": 60, "max": 1800 },
  "steps": [
    { "type": "steady", "duration_s": 120, "target": { "power": 40 },
      "limit": { "hr_max": 120, "hr_soft": 115 }, "label": "Einfahren" },
    { "type": "steady", "duration_s": 600, "target": { "power": 60 },
      "limit": { "hr_max": 120, "hr_soft": 115 }, "label": "Hauptteil" },
    { "type": "steady", "duration_s": 120, "target": { "power": 35 },
      "limit": { "hr_max": 120 }, "label": "Ausfahren" }
  ]
}
```

### Progression

Das Feld `progression` ist der Punkt, an dem aus einem Workout ein
Reha-Programm wird. Nach jeder abgeschlossenen Einheit fragt die UI, ob der
Hauptteil beim nächsten Mal eine Minute länger sein soll — und zwar nur, wenn
die Einheit **sauber** war: Deckel hat nicht gegriffen, Zielleistung wurde
gehalten, Puls blieb im Band. War sie nicht sauber, wird nicht gesteigert und
die UI sagt warum.

Dazu eine eigene Ansicht: **Physio-Progression**. Eine Zeile je Einheit mit
Datum, Dauer, mittlerer Leistung, mittlerem und maximalem Puls und der Anzahl
der Deckel-Eingriffe. Darüber ein Verlauf der Dauer über die Wochen. Das ist
die Kurve, die man dem Physiotherapeuten zeigt, und sie motiviert mehr als
jede Punktzahl.

### Editor im Webinterface

Kein Formular-Stapel, sondern eine Zeitachse, die man baut und sofort sieht:

```
┌──────────────────────────────────────────────────────────────────┐
│  Workout: 4×4 Norweger                    [ Speichern ] [ … ]   │
├──────────────────────────────────────────────────────────────────┤
│  Vorschau                        Dauer 38:00 · Ø 62 % FTP       │
│      ▁▁▁▁▁▁▁▁▁█████▁▁▁▁█████▁▁▁▁█████▁▁▁▁█████▁▁▁▁▁▁▁          │
│      ↑ Auswahl                                                  │
├──────────────────────────────────────────────────────────────────┤
│  1  Einfahren     10:00   55 % FTP                       ⋮⋮  ✎  │
│  2  ▼ 4× Block                                           ⋮⋮  ✎  │
│       Work        4:00    Puls 165                              │
│       Rest        3:00    50 % FTP                              │
│  3  Ausfahren      5:00   Rampe 52 % → 35 % FTP          ⋮⋮  ✎  │
│                                                                  │
│  [ + Dauerschritt ]  [ + Rampe ]  [ + Intervallblock ]          │
├──────────────────────────────────────────────────────────────────┤
│  Für Profil prüfen:  ◐ Martin ▾     Spitze 268 W · Stufe 15/16   │
│  ⚠ Block 3 verlangt 272 W — über der gemessenen Stufendecke      │
└──────────────────────────────────────────────────────────────────┘
```

Drei Dinge machen den Editor nützlich statt nur vorhanden. Die **Vorschau**
zeichnet das Profil sofort, sodass man die Form des Trainings sieht statt einer
Tabelle. Die **Machbarkeitsprüfung** rechnet das Workout gegen das gewählte
Profil und die gemessene Kennfläche und warnt, bevor man es fährt — nicht
mitten im Intervall. Und der **Umschalter zwischen % FTP und absoluten Watt**
sorgt dafür, dass dasselbe Workout für beide Nutzer taugt oder eben bewusst
nicht.

Dateiwege bleiben offen: Download als JSON, Upload per Datei, und der Editor
liest jede Datei, die von Hand geschrieben wurde. Import von `.zwo` kommt in
v0.3.

---

## 6. Tests

MyWhoosh bietet FTP- und Rampentests, und sie gehören hierher — mit einem
Vorbehalt, der zur Hardware gehört.

| Test | Ablauf | Ergebnis |
|------|--------|----------|
| **Rampe** | Start 60 W, alle 60 s um 20 W höher, bis Abbruch | MAP aus der besten Minute, FTP ≈ 0,75 × MAP |
| **20 Minuten** | 20 min gleichmäßig maximal, nach strukturiertem Einfahren | FTP = 0,95 × Ø Leistung |
| **Recovery** | 60 s Pulsabfall nach Belastung | Erholungsnote, wie die Konsolenfunktion |

Geführt wird jeder Test wie ein Workout, nur mit eigener Ergebnisseite:
Vorher-Hinweis, Countdown, während des Tests eine reduzierte Anzeige ohne
Ablenkung, danach das Ergebnis mit Vergleich zum letzten Wert und der Frage,
ob der neue FTP ins Profil übernommen werden soll. **Nie automatisch
überschreiben** — FTP ist ein Wert, den man bewusst setzt.

### Der Vorbehalt

Beide Tests brauchen Leistungen deutlich über der Grundlage. Ob die 16
Widerstandsstufen dorthin reichen, ist noch nicht gemessen — die
Extrapolation aus dem Laborlauf deutet auf eine Decke um 130 W. Solange das so
ist, gilt:

- Die Tests werden **gebaut**, aber die UI prüft vor dem Start, ob die nötige
  Leistung nach der Kennfläche erreichbar ist, und sagt sonst ab.
- Die Rampe mit 20 W pro Minute bedeutet bei etwa 7 W pro Stufe rund drei
  Stufen pro Minute. Das ist grob, aber fahrbar. Wird `0x11` Simulation
  nutzbar, wird die Rampe damit deutlich feiner.
- Für ein Reha-Profil sind diese Tests standardmäßig **ausgeblendet**. Ein
  Maximaltest ist dort kein Feature, sondern ein Risiko.

---

## 7. Die übrigen Seiten

### Betrieb vs. Entwickler (ab 0.3.18)

| Rolle | Tabs | Default |
|-------|------|---------|
| **Betrieb** | Ride, Workouts, Tests, Verlauf, Profile, Geräte, Einstellungen, OTA | immer sichtbar |
| **Entwickler** | Kalibrierung, Debug | nur mit `showDevUi` |

Einstellung: Checkbox **„Entwickler-UI anzeigen“** (NVS). Darunter erscheinen
Simulation/`0x11`, ERG-Assist und Gerätequirks. Hash auf versteckte Reiter
fällt auf Ride zurück. Ab **0.3.21**: Ride-Modi mit Alltags-Labels, Ziel-Panel
zum aktiven Modus, Start-Freigabe nach STOP sichtbar; Sim-Modus-Button nur mit
Entwickler-UI. Nachtests in Kalibrierung gruppiert (`force` als Gefahr).
Details: [UPDATE_UI_RIDE.md](../../CHANGELOG.md).

| Tab | Inhalt |
|-----|--------|
| **Ride** | §3, die Trainingsansicht |
| **Workouts** | Bibliothek, Editor (§5), Upload und Download |
| **Tests** | §6, geführte Tests und Testhistorie |
| **Verlauf** | Sessions je Profil, Kennzahlen, Zonenverteilung, Physio-Progression |
| **Profile** | §4, anlegen, bearbeiten, Grenzen setzen |
| **Geräte** | Bike und Pulsquelle verbinden, Feature-Bits, gemerkte MACs; Bridge-Status (beobachtet / steuert) |
| **Kalibrierung** | Sweep / Heatmap — *Entwickler* |
| **Debug** | Rohbytes, Steuer-Journal — *Entwickler* |
| **Einstellungen** | Hub, Bridge; optional Entwickler-Blöcke |
| **OTA** | Firmware-Upload wie die Schwesterprojekte |

**Bridge in der UI (ab 0.3.14).** Connline und Geräte-Karte zeigen die Rolle:
„App beobachtet“ (`observer`) vs. „App steuert“ (`controller` / Exklusiv).
Bei Controller sind Coach-Last-Buttons gesperrt; STOP bleibt. Fachregeln:
[BRIDGE.md](BRIDGE.md).

Die Kennfläche als **Heatmap** über Stufe und Kadenz ist dabei mehr als Zierde:
man sieht sofort, welche Bereiche gemessen und welche noch geraten sind. Ab
**0.3.23** markiert die Matrix live Zeile (Stufe) und Spalte (Kadenzband); die
Schnittzelle zeigt, wohin der nächste Lernpunkt geht. Das passive Lernen füllt
sie mit jeder Fahrt weiter. Stufe ± bleibt im Fahren-Dock — unter der Matrix
keine eigenen Buttons.

---

## 8. Umsetzung

**Entscheidung (Stand 0.1.2 / 0.3.x):** Die UI liegt als **gzip-PROGMEM** in der
Firmware, nicht als Klartext-`UiPages.h` und nicht auf LittleFS.

Pfad der Wahrheit:

```
web/index.html  →  tools/pack_ui.py  →  include/UiPagesGz.h  →  PROGMEM
```

`platformio.ini` setzt `extra_scripts = pre:tools/pio_pack_ui.py`. Der
Firmware-Build **braucht Python**. `src/web/UiPages.h` ist nur ein Stub, der
`UiPagesGz.h` einbindet.

Damit ist das Flash-Budget anders gelöst als im ursprünglichen Konzept
(~33 kB gzip statt ~110 kB Klartext). LittleFS bleibt nur relevant, wenn die UI
**ohne** Firmware-OTA austauschbar sein soll — nicht als Notlösung für Größe.

Live-Daten: SSE. Charts: Canvas. Keine Fremd-UI-Bibliotheken. Eine Seite,
clientseitige Tabs. Fonts über CDN mit `system-ui` als Rückfall.

---

## 9. Abnahmekriterien der UI

1. Die aktive Zone ist aus zwei Metern **ohne Lesen einer Zahl** erkennbar.
2. Jede Zone trägt zusätzlich Kürzel und Namen; die UI ist bei
   Graustufendarstellung noch bedienbar.
3. Die Hero-Zahl folgt dem Profil — Watt bei einem Leistungsprofil, Puls bei
   einem Pulsprofil.
4. Die Zonenschiene zeigt Zeit je Zone und die aktuelle Position und stimmt am
   Ende mit der Sessionauswertung überein.
5. Ein unerreichbares Ziel ist als unerreichbar sichtbar, und die Stufenkachel
   zeigt `16/16` mit Warnrand.
6. Ohne gewähltes Profil lässt sich keine Session starten.
7. Ein Profilwechsel während einer laufenden Session wird abgelehnt.
8. Der Pulsdeckel ist als Annäherung sichtbar, bevor er greift, und sein
   Eingriff wird in Worten gemeldet.
9. Der Editor zeichnet die Vorschau live und warnt vor dem Speichern, wenn ein
   Schritt für das gewählte Profil nicht fahrbar ist.
10. Ein im Editor gebautes Workout ist als JSON herunterladbar, und die
    heruntergeladene Datei lässt sich unverändert wieder einlesen.
11. Tests sind für Reha-Profile ausgeblendet.
12. Stop ist auf jeder Unterseite der Ride-Ansicht ohne Scrollen erreichbar.
13. Bedienelemente sind mindestens 44 × 44 px; keine Funktion hängt an Hover.
14. 45 Minuten offene UI mit laufendem SSE ohne Reload, ohne Speicherzuwachs
    im Browser.
