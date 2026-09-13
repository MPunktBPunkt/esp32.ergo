#pragma once

#ifndef FW_VERSION
#define FW_VERSION "0.3.11-dev"
#endif

// ------------------------------------------------------------- Identitaet

#ifndef DEVICE_NAME_DEFAULT
#define DEVICE_NAME_DEFAULT "Ergo"
#endif

/** fwType im Hub-Heartbeat; bestimmt den Bin-Namen ergo.<semver>.esp32s3.bin */
#ifndef FW_TYPE
#define FW_TYPE "ergo"
#endif

#ifndef WIFI_AP_NAME
#define WIFI_AP_NAME "ESP-Ergo-Setup"
#endif

#ifndef WIFI_PORTAL_TIMEOUT_S
#define WIFI_PORTAL_TIMEOUT_S 180
#endif

// -------------------------------------------------------------------- Hub

#ifndef HUB_HOST_DEFAULT
#define HUB_HOST_DEFAULT "192.168.178.113"
#endif

#ifndef HUB_PORT_DEFAULT
#define HUB_PORT_DEFAULT 8093
#endif

#ifndef NTP_SERVER_DEFAULT
#define NTP_SERVER_DEFAULT "pool.ntp.org"
#endif

/** Europe/Berlin POSIX TZ (CET/CEST) */
#ifndef TZ_DEFAULT
#define TZ_DEFAULT "CET-1CEST,M3.5.0,M10.5.0/3"
#endif

#ifndef RESET_BUTTON_PIN
#define RESET_BUTTON_PIN 0
#endif

#ifndef RESET_HOLD_SEC
#define RESET_HOLD_SEC 3
#endif

// -------------------------------------------------------------------- BLE

#ifndef ERGO_MAX_SCAN
#define ERGO_MAX_SCAN 24
#endif

/** Ohne 0x2AD2-Notify laenger als das gilt der Link als tot. */
#ifndef ERGO_DATA_STALE_MS
#define ERGO_DATA_STALE_MS 4000
#endif

// --------------------------------------------------------------- Limiter
//
// Die wirksame Klemme ist immer der Schnitt aus drei Quellen:
//
//   1. dem Bereich, den das Geraet in 0x2AD6 bzw. 0x2AD8 meldet (Laufzeit),
//   2. der Einstellung im aktiven Profil,
//   3. den absoluten Schranken hier.
//
// Die Werte unten sind KEINE Geraeteeigenschaften. Sie sind die Grenze, die
// auch ein fehlerhaft gemeldeter Bereich nicht ueberschreiten darf — der
// Betriebsbereich des angeschlossenen Ergometers kommt ausschliesslich aus
// 0x2AD6. Ein Geraet mit 32 Stufen oder einer 0..100-Skala muss ohne Reflash
// laufen.

/** Absolute Obergrenze fuer jede Stufenvorgabe, in Zehnteln. */
#ifndef ERGO_LEVEL_ABS_MAX_TENTHS
#define ERGO_LEVEL_ABS_MAX_TENTHS 1000
#endif

/** Absolute Obergrenze fuer jede Wattvorgabe. */
#ifndef ERGO_POWER_ABS_MAX_W
#define ERGO_POWER_ABS_MAX_W 600
#endif

/** Fallback-Stufenbereich, falls ein Geraet keine 0x2AD6 liefert. */
#ifndef ERGO_LEVEL_FALLBACK_MIN_TENTHS
#define ERGO_LEVEL_FALLBACK_MIN_TENTHS 10
#endif
#ifndef ERGO_LEVEL_FALLBACK_MAX_TENTHS
#define ERGO_LEVEL_FALLBACK_MAX_TENTHS 100
#endif

/** Rampenbegrenzung: hoechstens eine Stufe je zwei Sekunden. */
#ifndef ERGO_LEVEL_RAMP_MS
#define ERGO_LEVEL_RAMP_MS 2000
#endif

/** Harte Obergrenze fuer jeden Pulsdeckel, unabhaengig vom Profil. */
#ifndef ERGO_HR_CEILING_MAX
#define ERGO_HR_CEILING_MAX 190
#endif

/**
 * Deadman: bleibt der Puls laenger aus, greift das im Profil hinterlegte
 * Verhalten (freeze / reduce / stop).
 */
#ifndef ERGO_HR_LOSS_MS
#define ERGO_HR_LOSS_MS 8000
#endif

// ------------------------------------------------------------- Historie

#ifndef ERGO_HISTORY_SIZE
#define ERGO_HISTORY_SIZE 900  // 15 min bei 1 Hz
#endif

/** Ringpuffer des Debug-Modus in Paketen. */
#ifndef ERGO_DEBUG_RING
#define ERGO_DEBUG_RING 512
#endif

// -------------------------------------------------------------- Hardware

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define ERGO_BOARD_ID "esp32-s3"
#define ERGO_BOARD_LABEL "ESP32-S3"
#define ERGO_HW_TYPE "esp32s3"
#else
#error "esp32.ergo laeuft nur auf dem ESP32-S3 — siehe platformio.ini"
#endif
