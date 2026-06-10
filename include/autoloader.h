#pragma once

/* -----------------------------------------------------------------------
 * ps5-autoloader — version & config
 * ----------------------------------------------------------------------- */

#define AUTOLOADER_VERSION "0.1.0"

/* Port that elfldr (socksrv) listens on for incoming ELF payloads */
#define ELFLDR_PORT 9021

/* Config file looked for inside each search base */
#define AUTOLOAD_CONFIG_SUBPATH "ps5_autoloader/autoload.txt"

/* Fallback search base when no USB config is found */
#define DATA_BASE "/data"

/* USB mount bases — searched in order, highest priority first */
static const char * const USB_BASES[] = {
    "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3",
    "/mnt/usb4", "/mnt/usb5", "/mnt/usb6", "/mnt/usb7"
};
#define USB_COUNT 8

/* YouTube app title IDs (PPSA01650–01652 cover all regional variants) */
static const char * const YOUTUBE_TITLE_IDS[] = {
    "PPSA01650", "PPSA01651", "PPSA01652"
};
#define YOUTUBE_TITLE_ID_COUNT 3

/* BD Disc Player title ID and process name */
#define DISC_PLAYER_TITLE_ID "NPXS40140"
#define DISC_PLAYER_PROCESS  "SceDiscPlayer"

/* How long to poll for elfldr readiness before giving up */
#define ELFLDR_WAIT_RETRIES  50
#define ELFLDR_WAIT_DELAY_US 200000   /* 200 ms */
