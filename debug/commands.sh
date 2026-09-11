#!/usr/bin/env bash
# Copy-paste / Source für die andere Cursor-Instanz.
# PlatformIO liegt hier: /home/martin/.venvs/pio/bin/pio

set -euo pipefail
export PATH="/home/martin/.venvs/pio/bin:${PATH}"
cd "$(dirname "$0")/.."

echo "== pio version =="
pio --version

echo "== native codec tests =="
pio test -e native

echo "== firmware ergo =="
pio run -e ergo

echo "== size =="
ls -la .pio/build/ergo/firmware.bin
