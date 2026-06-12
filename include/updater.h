#pragma once

/* Extracts ps5_autoloader_update.zip from USB to /data/ps5_autoloader.
 * Returns 1 if an update zip was found, 0 if no update zip was present.
 */
int apply_autoloader_update_from_usb(void);
