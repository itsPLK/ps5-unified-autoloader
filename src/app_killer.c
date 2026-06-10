#include "app_killer.h"
#include "autoloader.h"
#include "notification.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>

/* PS5 SDK: get per-process app info (title ID, app ID) */
typedef struct app_info {
    uint32_t app_id;
    uint64_t unknown1;
    char     title_id[14];
    char     unknown2[0x3c];
} app_info_t;

int sceKernelGetAppInfo(pid_t pid, app_info_t *info);

/* LncUtil externs — used for graceful app teardown */
extern int sceLncUtilGetAppIdOfRunningBigApp(void);
extern int sceLncUtilGetAppTitleId(uint32_t app_id, char *title_id);
extern int sceLncUtilSuspendApp(uint32_t app_id);
extern int sceLncUtilKillApp(uint32_t app_id);

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */

/**
 * Find the PID of a process by comm name.
 * Returns the PID on success, -1 if not found.
 */
static pid_t get_pid_by_name(const char *name) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
    size_t buf_size = 0;

    if (sysctl(mib, 4, NULL, &buf_size, NULL, 0))
        return -1;

    void *buf = malloc(buf_size);
    if (!buf)
        return -1;

    if (sysctl(mib, 4, buf, &buf_size, NULL, 0)) {
        free(buf);
        return -1;
    }

    pid_t pid = -1;
    for (void *ptr = buf; ptr < (buf + buf_size);) {
        struct kinfo_proc *ki = (struct kinfo_proc *)ptr;
        if (ki->ki_structsize < (int)sizeof(struct kinfo_proc))
            break;
        if (strncmp(ki->ki_comm, name, sizeof(ki->ki_comm)) == 0) {
            pid = ki->ki_pid;
            break;
        }
        ptr += ki->ki_structsize;
    }

    free(buf);
    return pid;
}

/* -----------------------------------------------------------------------
 * kill_youtube_app
 * ----------------------------------------------------------------------- */

/**
 * Kill the YouTube app (PPSA01650/01651/01652) if running.
 *
 * YouTube does NOT require suspending first — a direct SIGKILL to its
 * main process (eboot.bin) is sufficient and fully terminates it.
 */
int kill_youtube_app(void) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
    size_t buf_size = 0;

    if (sysctl(mib, 4, NULL, &buf_size, NULL, 0) < 0) {
        printf("[autoloader] kill_youtube: sysctl size failed\n");
        return -1;
    }

    void *buf = malloc(buf_size);
    if (!buf) {
        printf("[autoloader] kill_youtube: malloc failed\n");
        return -1;
    }

    if (sysctl(mib, 4, buf, &buf_size, NULL, 0) < 0) {
        printf("[autoloader] kill_youtube: sysctl query failed\n");
        free(buf);
        return -1;
    }

    int found = 0;
    int ret = 0;

    for (void *ptr = buf; ptr < (buf + buf_size);) {
        struct kinfo_proc *ki = (struct kinfo_proc *)ptr;
        if (ki->ki_structsize < (int)sizeof(struct kinfo_proc))
            break;

        app_info_t appinfo;
        memset(&appinfo, 0, sizeof(appinfo));

        if (sceKernelGetAppInfo(ki->ki_pid, &appinfo) == 0) {
            /* Check: title ID is one of the YT IDs AND comm is eboot.bin */
            int is_yt = 0;
            for (int i = 0; i < YOUTUBE_TITLE_ID_COUNT; i++) {
                if (strcmp(appinfo.title_id, YOUTUBE_TITLE_IDS[i]) == 0) {
                    is_yt = 1;
                    break;
                }
            }

            if (is_yt && strcmp(ki->ki_comm, "eboot.bin") == 0) {
                printf("[autoloader] kill_youtube: found %s (PID %d, AppID 0x%04x)\n",
                       ki->ki_comm, ki->ki_pid, appinfo.app_id);
                fflush(stdout);
                found = 1;

                autoloader_notify("YouTube detected. Terminating...");

                if (kill(ki->ki_pid, SIGKILL) == 0) {
                    printf("[autoloader] kill_youtube: SIGKILL sent to PID %d\n", ki->ki_pid);
                    autoloader_notify("YouTube terminated.");
                } else {
                    printf("[autoloader] kill_youtube: SIGKILL failed for PID %d\n", ki->ki_pid);
                    ret = -1;
                }
                fflush(stdout);
                break;
            }
        }

        ptr += ki->ki_structsize;
    }

    free(buf);

    if (!found) {
        printf("[autoloader] kill_youtube: no YouTube process found (not running)\n");
        fflush(stdout);
    }

    return ret;
}

/* -----------------------------------------------------------------------
 * kill_disc_player
 * ----------------------------------------------------------------------- */

/**
 * Kill the BD Disc Player (NPXS40140) if running.
 *
 * The disc player requires a careful teardown sequence with specific delays
 * to allow the home screen to become stable before the kill:
 *   1. sceLncUtilSuspendApp
 *   2. sleep(2)  — home screen transition stability
 *   3. SIGKILL on "SceDiscPlayer" process
 *   4. sleep(1)
 *   5. sceLncUtilKillApp
 */
int kill_disc_player(void) {
    /* Step 1: get the currently running big app */
    uint32_t app_id = (uint32_t)sceLncUtilGetAppIdOfRunningBigApp();
    if (app_id == 0xffffffff) {
        printf("[autoloader] kill_disc_player: no big app running\n");
        fflush(stdout);
        return 0; /* Nothing to do */
    }

    /* Step 2: verify it is the disc player by title ID */
    char title_id[16] = {0};
    if (sceLncUtilGetAppTitleId(app_id, title_id) != 0) {
        printf("[autoloader] kill_disc_player: could not get title ID\n");
        fflush(stdout);
        return 0;
    }

    if (strcmp(title_id, DISC_PLAYER_TITLE_ID) != 0) {
        printf("[autoloader] kill_disc_player: running app is %s, not disc player\n", title_id);
        fflush(stdout);
        return 0; /* Different app — leave it alone */
    }

    printf("[autoloader] kill_disc_player: disc player detected (AppID 0x%04x)\n", app_id);
    fflush(stdout);
    autoloader_notify("Disc Player detected. Terminating...");

    /* Step 3: suspend the app */
    if (sceLncUtilSuspendApp(app_id) != 0) {
        autoloader_notify("Failed to suspend Disc Player");
        printf("[autoloader] kill_disc_player: suspend failed\n");
        fflush(stdout);
        return -1;
    }
    printf("[autoloader] kill_disc_player: suspended. Waiting for home screen...\n");
    fflush(stdout);

    /* Wait for home screen transition stability */
    sleep(2);

    /* Step 4: SIGKILL on the disc player process */
    pid_t pid = get_pid_by_name(DISC_PLAYER_PROCESS);
    if (pid != -1) {
        printf("[autoloader] kill_disc_player: sending SIGKILL to %s (PID %d)\n",
               DISC_PLAYER_PROCESS, pid);
        fflush(stdout);
        if (kill(pid, SIGKILL) == 0) {
            printf("[autoloader] kill_disc_player: SIGKILL sent\n");
        } else {
            printf("[autoloader] kill_disc_player: warning — SIGKILL failed\n");
            autoloader_notify("Warning: SIGKILL to Disc Player failed");
        }
        fflush(stdout);
        sleep(1);
    } else {
        printf("[autoloader] kill_disc_player: %s process not found (may already be gone)\n",
               DISC_PLAYER_PROCESS);
        fflush(stdout);
    }

    /* Step 5: LncUtil kill */
    int result = sceLncUtilKillApp(app_id);
    if (result == 0) {
        printf("[autoloader] kill_disc_player: Disc Player fully closed\n");
        fflush(stdout);
    } else {
        /* Check if it already disappeared */
        if ((uint32_t)sceLncUtilGetAppIdOfRunningBigApp() == 0xffffffff) {
            printf("[autoloader] kill_disc_player: Disc Player closed (already gone)\n");
            fflush(stdout);
        } else {
            printf("[autoloader] kill_disc_player: LncUtil kill failed (result=%d)\n", result);
            fflush(stdout);
            autoloader_notify("Failed to kill Disc Player (result=%d)", result);
            return -1;
        }
    }

    autoloader_notify("Disc Player terminated.");
    return 0;
}
