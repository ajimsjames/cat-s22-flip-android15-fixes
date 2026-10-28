/*
 * Cat S22 Flip Outer Display Hardware & Service Watchdog Daemon
 * Checks outerscreen_service heartbeat every 2 seconds.
 * If frozen, hung, or heartbeat stale > 10 seconds:
 *   - Kills stalled service
 *   - Re-initializes /dev/graphics/fb2 unblank and lcd-backlight2
 *   - Restarts outerscreen_service cleanly
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <signal.h>

#define HEARTBEAT_FILE "/dev/shm_outerscreen_heartbeat"
#define FB_DEV         "/dev/graphics/fb2"
#define BACKLIGHT_NODE "/sys/devices/platform/soc/1a00000.qcom,mdss_mdp/1a00000.qcom,mdss_mdp:qcom,mdss_fb_secondary/leds/lcd-backlight2/brightness"
#define BACKLIGHT_ALT  "/sys/class/leds/lcd-backlight2/brightness"
#define TIMEOUT_SEC    10

static void reset_hardware(void) {
    int bfd = open(BACKLIGHT_NODE, O_WRONLY);
    if (bfd < 0) bfd = open(BACKLIGHT_ALT, O_WRONLY);
    if (bfd >= 0) {
        write(bfd, "255\n", 4);
        close(bfd);
    }

    int fbfd = open(FB_DEV, O_RDWR);
    if (fbfd >= 0) {
        ioctl(fbfd, FBIOBLANK, FB_BLANK_UNBLANK);
        close(fbfd);
    }
}

static time_t get_heartbeat(void) {
    struct stat st;
    if (stat(HEARTBEAT_FILE, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    // Initial grace period for system to stabilize after boot
    sleep(12);

    for (;;) {
        sleep(2);

        time_t now = time(NULL);
        time_t hb = get_heartbeat();

        // Check if outerscreen_service process exists
        int proc_running = (system("pidof outerscreen_service >/dev/null 2>&1") == 0);

        if (!proc_running || (hb > 0 && (now - hb) > TIMEOUT_SEC)) {
            // Heartbeat expired or process died -> Auto Recovery
            system("killall -9 outerscreen_service 2>/dev/null");
            usleep(200000);

            // Re-arm hardware
            reset_hardware();

            // Refresh heartbeat timestamp to give restart a fresh window
            close(open(HEARTBEAT_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666));

            // Start service via init
            system("start outerscreen");

            // Sleep grace period
            sleep(5);
        }
    }

    return 0;
}
