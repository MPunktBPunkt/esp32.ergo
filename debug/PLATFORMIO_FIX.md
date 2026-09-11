# Fix: `pio test -e native` und Arduino-Framework

## Symptom

```text
Please specify `board` in `platformio.ini` to use with 'arduino' framework
Building stage has failed
native:test_codec [ERRORED]
```

Firmware `pio run -e ergo` war davon unberührt und baute bereits erfolgreich.

## Ursache

In PlatformIO erbt **jedes** Environment automatisch die Sektion `[env]`.
Die Originaldatei setzte dort u.a.:

```ini
[env]
platform = espressif32@6.4.0
framework = arduino
lib_deps = WiFiManager / ArduinoJson / NimBLE-Arduino
…
```

`[env:native]` überschrieb `platform = native`, bekam aber weiterhin
`framework = arduino` und die Arduino-Libs → PIO verlangt ein Board.

## Lösung (bereits lokal angewendet)

Gemeinsame Werte in eine **nicht** automatisch erbende Sektion `[common]` legen.
Nur `[env:ergo]` zieht Framework + Libs; `[env:native]` bleibt schlank.

Aktuelle Struktur (Kurzform):

```ini
[common]
build_flags = …
lib_deps = …

[env:ergo]
platform = espressif32@6.4.0
framework = arduino
board = esp32-s3-devkitc-1
lib_deps = ${common.lib_deps}
build_flags = ${common.build_flags} …

[env:native]
platform = native
test_framework = unity
test_build_src = yes
build_src_filter = +<ble/FtmsCodec.cpp> +<ble/FtmsCapabilities.cpp>
build_flags = -std=c++17 -Wall -Wextra -I src
```

## Verifikation

```bash
export PATH="/home/martin/.venvs/pio/bin:$PATH"
pio test -e native   # 22/22 PASSED
pio run -e ergo      # SUCCESS
```

## Hinweis für die andere Instanz

Die geänderte `platformio.ini` liegt **uncommitted** im Working Tree.
Wenn du den Fix übernimmst: committen und idealerweise CI mit beiden Jobs verdrahten.
