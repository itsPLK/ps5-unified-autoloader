#pragma once

/**
 * Kill the YouTube app (PPSA01650/01651/01652) if it is currently running.
 * Uses SIGKILL directly — no suspend needed.
 * Returns 0 if not found or successfully killed, -1 on error.
 */
int kill_youtube_app(void);

/**
 * Kill the BD Disc Player (NPXS40140) if it is currently running.
 * Performs: suspend → sleep(5) → sleep(3) → SIGKILL → LncUtil kill.
 * The timing delays are critical for a clean teardown.
 * Returns 0 if not found or successfully killed, -1 on error.
 */
int kill_disc_player(void);

extern char g_entry_point_id[16];
