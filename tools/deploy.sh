#!/usr/bin/env bash
# Build und Ausrollen der Ergo-Firmware.
#
#   ./deploy.sh --build-only
#   ./deploy.sh --ota 192.168.178.88
#   ./deploy.sh --hub 192.168.178.113:8093 --mac 68B6B329339C
#
# Der erste Flash eines leeren Boards laeuft per USB ("pio run -t upload") oder
# ueber das Hub-UI. Danach braucht es kein Kabel mehr.
#
# Zwei Dinge macht dieses Skript, die deploy.sh der Sonde nicht macht:
#
#   1. Es weigert sich, eine Bin auszurollen, in der `/ota-upload` nicht
#      vorkommt. Ein OTA-Flash ohne Rueckweg ist auf einem Geraet ohne Kabel
#      ein Totalverlust — und genau das waere mit dem Platzhalter-main.cpp
#      passiert, der bis 0.1.0-dev in diesem Repo lag.
#   2. Es bricht ab, wenn das Zielgeraet einen offenen Bike-Link meldet. Ein
#      Neustart unter Last laesst das Ergometer gebremst stehen.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_NAME="ergo"
NAME="ergo"
FAMILY="esp32s3"
OTA_IP=""
HUB=""
MAC=""
BUILD_ONLY=0
SKIP_TESTS=0

die() { printf '\033[31mFehler:\033[0m %s\n' "$1" >&2; exit 1; }
step() { printf '\n\033[1m==> %s\033[0m\n' "$1"; }
warn() { printf '\033[33m    Achtung:\033[0m %s\n' "$1"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --ota) OTA_IP="${2:-}"; shift 2;;
        --hub) HUB="${2:-}"; shift 2;;
        --mac) MAC="${2:-}"; shift 2;;
        --build-only) BUILD_ONLY=1; shift;;
        --skip-tests) SKIP_TESTS=1; shift;;
        -h|--help) sed -n '2,19p' "$0"; exit 0;;
        *) die "unbekannte Option: $1";;
    esac
done

command -v pio >/dev/null 2>&1 || command -v platformio >/dev/null 2>&1 \
    || die "platformio nicht gefunden (z. B. /home/martin/.venvs/pio/bin/pio)"
PIO="$(command -v pio || command -v platformio)"
command -v curl >/dev/null 2>&1 || die "curl nicht gefunden"

VERSION="$(grep -oP 'FW_VERSION=\\"\K[^\\]+' "$ROOT/platformio.ini" | head -n1 || true)"
[ -n "$VERSION" ] || die "FW_VERSION nicht aus platformio.ini lesbar"

if [ "$SKIP_TESTS" -eq 0 ]; then
    step "Hosttests (Codec + Limiter)"
    "$PIO" test -d "$ROOT" -e native
fi

step "Build $ENV_NAME (v$VERSION)"
"$PIO" run -d "$ROOT" -e "$ENV_NAME"

SRC="$ROOT/.pio/build/$ENV_NAME/firmware.bin"
[ -f "$SRC" ] || die "Bin nicht gefunden: $SRC"

# Namensschema der Familie: {name}.{version}.{family}.bin
BIN="$NAME.$VERSION.$FAMILY.bin"
mkdir -p "$ROOT/dist"
cp "$SRC" "$ROOT/dist/$BIN"
SIZE="$(stat -c %s "$ROOT/dist/$BIN")"
printf '    dist/%s  (%s kB)\n' "$BIN" "$((SIZE / 1024))"

step "Rueckweg pruefen"
if grep -aqF '/ota-upload' "$ROOT/dist/$BIN"; then
    echo "    /ota-upload ist in der Bin enthalten"
else
    die "die neue Bin enthaelt kein /ota-upload — ein OTA-Flash waere ein Remote-Brick"
fi
if grep -aqF '/api/status' "$ROOT/dist/$BIN"; then
    echo "    /api/status ist in der Bin enthalten"
else
    warn "kein /api/status gefunden — die Verifikation unten wird fehlschlagen"
fi

if [ "$BUILD_ONLY" -eq 1 ]; then
    step "Fertig (nur Build)"
    exit 0
fi

if [ -z "$OTA_IP" ] && [ -z "$HUB" ]; then
    step "Kein Ziel angegeben"
    echo "    --ota <geraete-ip>              direkter Upload"
    echo "    --hub <host:port> --mac <MAC>   Ablage im Hub plus OTA-Push"
    exit 0
fi

# ── Weg 1: direkt an das Geraet ──────────────────────────────────────────────
if [ -n "$OTA_IP" ]; then
    step "OTA direkt an $OTA_IP"
    STATUS="$(curl -fsS --max-time 5 "http://$OTA_IP/api/status" 2>/dev/null || true)"
    BEFORE="$(printf '%s' "$STATUS" | grep -oP '"version":"\K[^"]+' | head -n1 || echo '?')"
    TYPE="$(printf '%s' "$STATUS" | grep -oP '"fwType":"\K[^"]+' | head -n1 || echo '?')"
    echo "    laufend: $TYPE v$BEFORE"
    [ -n "$STATUS" ] || die "$OTA_IP antwortet nicht auf /api/status — kein OTA ins Blinde"

    # Feld existiert erst, wenn BleCentral da ist; fehlt es, blockiert nichts.
    LINK="$(printf '%s' "$STATUS" | grep -oP '"bikeLink":\K(true|false)' | head -n1 || true)"
    if [ "${LINK:-false}" = "true" ]; then
        die "das Geraet hat einen offenen Bike-Link — erst Stop senden und trennen"
    fi

    curl -fsS --max-time 120 -F "firmware=@$ROOT/dist/$BIN" "http://$OTA_IP/ota-upload" \
        || die "Upload fehlgeschlagen"
    echo "    Upload ok, warte auf Neustart ..."
fi

# ── Weg 2: ueber den Hub ─────────────────────────────────────────────────────
if [ -n "$HUB" ]; then
    [ -n "$MAC" ] || die "--hub braucht --mac (siehe http://$HUB/ Reiter Geraete)"
    step "Firmware in die Hub-Ablage"
    curl -fsS --max-time 120 -F "firmware=@$ROOT/dist/$BIN" \
        "http://$HUB/api/firmware-upload" || die "firmware-upload fehlgeschlagen"
    echo
    step "OTA-Push planen fuer $MAC"
    curl -fsS --max-time 20 -X POST -H 'Content-Type: application/json' \
        -d "{\"mac\":\"$MAC\",\"firmware\":\"$BIN\"}" \
        "http://$HUB/api/ota-push" || die "ota-push fehlgeschlagen"
    echo
    echo "    Wird beim naechsten Heartbeat uebertragen (Standard 30 s)."
fi

# ── Ergebnis pruefen ─────────────────────────────────────────────────────────
if [ -n "$OTA_IP" ]; then
    step "Warte auf die neue Version"
    for _ in $(seq 1 30); do
        sleep 3
        AFTER="$(curl -fsS --max-time 4 "http://$OTA_IP/api/status" 2>/dev/null || true)"
        GOT="$(printf '%s' "$AFTER" | grep -oP '"version":"\K[^"]+' | head -n1 || true)"
        GOT_TYPE="$(printf '%s' "$AFTER" | grep -oP '"fwType":"\K[^"]+' | head -n1 || true)"
        if [ "$GOT" = "$VERSION" ] && [ "$GOT_TYPE" = "$NAME" ]; then
            SELFTEST="$(printf '%s' "$AFTER" | grep -oP '"codecSelfTest":"\K[^"]+' | head -n1 || true)"
            echo "    $GOT_TYPE v$GOT laeuft, Codec-Selbsttest: ${SELFTEST:-?}"
            [ "$SELFTEST" = "ok" ] || warn "Selbsttest nicht ok — Serial-Log pruefen"
            exit 0
        fi
        printf '.'
    done
    echo
    die "Geraet meldet nach 90 s nicht $NAME v$VERSION — Rollback-Bin bereithalten"
fi
