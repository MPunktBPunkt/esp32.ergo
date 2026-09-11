# `esp32.ergo` — Planung

Steuerung des HAMMER Varon XTR II (BLE-Name `TC174`) über BLE/FTMS, verteilt
über den ESP-Hub wie die übrigen Projekte der Familie.

> Dieser Ordner ist die **Planungsebene** und beschreibt, was gebaut werden
> soll. Was davon gebaut *ist*, steht in [`STATE.md`](../../STATE.md).

| Datei | Inhalt |
|-------|--------|
| [PFLICHTENHEFT.md](PFLICHTENHEFT.md) | Das Konzept. Revision 4. |
| [WEBINTERFACE.md](WEBINTERFACE.md) | Designkonzept der WebUI: Zonen, Profile, Workout-Editor, Tests |
| [GERAETEPROFIL.md](GERAETEPROFIL.md) | Was das Bike wirklich kann — gemessen, plus Auswertung der Bedienungsanleitung |
| [NACHTESTS.md](NACHTESTS.md) | Sechs offene Messungen, zwei davon blockierend |
| [BLE-SCAN.md](BLE-SCAN.md) | Erster Protokolltest (erledigt), Vorlage für weitere Geräte |

## Stand

Der Protokolltest ist gelaufen. Werkzeug war die eigens gebaute Sonde
[`esp32.ftmsprobe`](https://github.com/MPunktBPunkt/esp32.ftmsprobe), deren
BLE-Schicht inzwischen als `BleCentral` plus `FtmsClient` in `esp32.ergo`
steht — die Firmware läuft auf Hardware, siehe [`STATE.md`](../../STATE.md).

Vier Befunde bestimmen alles Weitere:

1. **Standard-FTMS, Control Point offen.** Kein FitShow. Die Grundannahme trägt.
2. **Kein Set Target Power.** Die einzige Stellgröße ist die Widerstandsstufe —
   16 diskrete Schritte. Alles Watt-basierte muss der ESP32 emulieren.
3. **Stufe → Watt ist eine Fläche, keine Kurve.** Die Bedienungsanleitung
   beschreibt den Watt-Modus der Konsole als Widerstandsanpassung über die
   Kadenz. Das beworbene „drehzahlunabhängig" gilt für dieses Programm, nicht
   für die Stufen. Bei fester Stufe steigt die Leistung mit der Kadenz.
4. **Der Leistungsbereich ist unklar.** Zwei belastbare Punkte legen grob 7 W pro
   Stufe nahe, was Stufe 16 bei etwa 130 W verorten würde. Das Bike ist für 400 W
   ausgelegt. Der Stufen-Sweep aus [NACHTESTS.md](NACHTESTS.md) entscheidet, ob
   der Widerstandskanal für mehr als Grundlagentraining reicht.

## Topologie

`esp32.heartrate` v0.3 hat ein HR-Relay bekommen. Das löst zwei Probleme auf
einmal: der Polar H9 erlaubt nur einen BLE-Client, und MyWhoosh will den Puls
getrennt gekoppelt haben statt über das Ergometer.

```
Polar H9 ──BLE──▶ ESP #1  heartrate (Relay)  ──BLE──▶ MyWhoosh   (Puls)
                     │                       └──BLE──▶ ESP #2     (Puls)
                     └── 5-kHz-GymLink ──▶ Bike-Konsole (HR-Feld in 2AD2)

Varon XTR II ──BLE──▶ ESP #2  ergo  ──BLE Peripheral──▶ MyWhoosh (FTMS)
                         │
                         └── WiFi ──▶ WebUI + ioBroker-Hub
```

Nebenbei ist damit die Peripheral-Rolle in der Familie erprobt: `heartrate-s3`
fährt drei BLE-Links plus WiFi plus SSE-UI, und das ohne PSRAM. `HrServer` ist
die direkte Vorlage für `FtmsServer`.

Dieser Satz stand hier ursprünglich: *„Kein `esp32.ergo`-Repo, bis Test 1 und
Test 2 der Nachtests gelaufen sind."* Er ist überholt — das Repo existiert, weil
sich herausgestellt hat, dass die Tests selbst Firmware brauchen: der
Stufen-Sweep protokolliert sich inzwischen über den Reiter Kalibrierung, statt
von Hand mitgeschrieben zu werden. Die beiden Tests sind weiterhin offen und
weiterhin blockierend für alles Watt-basierte.
