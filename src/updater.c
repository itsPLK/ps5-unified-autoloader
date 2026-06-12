#include "autoloader.h"
#include "notification.h"
#include "updater.h"
#include "zip.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define UPDATE_FILE "ps5_autoloader_update.zip"
#define DATA_AUTOLOADER_DIR "/data/ps5_autoloader"
#define PATH_BUF_SIZE 1024

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int find_update_zip(char *out_path, size_t out_size) {
    for (int i = 0; i < USB_COUNT; i++) {
        char path[PATH_BUF_SIZE];
        snprintf(path, sizeof(path), "%s/%s", USB_BASES[i], UPDATE_FILE);

        if (path_exists(path)) {
            snprintf(out_path, out_size, "%s", path);
            return 1;
        }
    }

    out_path[0] = '\0';
    return 0;
}

static int ensure_dir(const char *path) {
    struct stat st;

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }

        printf("[autoloader] Update path exists but is not a directory: %s\n", path);
        fflush(stdout);
        autoloader_notify("Update path is not a directory:\n%s", path);
        return -1;
    }

    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return 0;
    }

    printf("[autoloader] Failed to create update directory: %s\n", path);
    fflush(stdout);
    autoloader_notify("Failed to create update dir:\n%s", path);
    return -1;
}

static void remove_update_zip(const char *path) {
    if (unlink(path) == 0) {
        printf("[autoloader] Removed update zip: %s\n", path);
        fflush(stdout);
        autoloader_notify("Removed update zip");
        return;
    }

    printf("[autoloader] Failed to remove update zip: %s\n", path);
    fflush(stdout);
    autoloader_notify("Failed to remove update zip:\n%s", path);
}

int apply_autoloader_update_from_usb(void) {
    char update_zip_path[PATH_BUF_SIZE];

    if (!find_update_zip(update_zip_path, sizeof(update_zip_path))) {
        return 0;
    }

    printf("[autoloader] Found update zip: %s\n", update_zip_path);
    fflush(stdout);
    autoloader_notify("Updating ps5_autoloader from USB");

    if (ensure_dir(DATA_AUTOLOADER_DIR) != 0) {
        return 1;
    }

    int ret = zip_extract(update_zip_path, DATA_AUTOLOADER_DIR, NULL, NULL);
    if (ret != 0) {
        printf("[autoloader] Update extract failed: %s\n", zip_strerror(ret));
        fflush(stdout);
        autoloader_notify("Update extract failed:\n%s", zip_strerror(ret));
        return 1;
    }

    printf("[autoloader] Update extracted to %s\n", DATA_AUTOLOADER_DIR);
    fflush(stdout);
    autoloader_notify("Update extracted to /data/ps5_autoloader");
    remove_update_zip(update_zip_path);
    usleep((useconds_t)1000000U);
    return 1;
}
