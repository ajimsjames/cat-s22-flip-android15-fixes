/*
 * Cat S22 Flip Keypad Light & Hall Sensor Controller Daemon
 * - Dedicated standalone init daemon running under root.
 * - Monitors /dev/input/event3 (hall_sensor):
 *     - Value = 0 (hall near / flip closed):
 *         - Turns OFF keypad backlight
 *     - Value = 1 (hall far / flip opened):
 *         - Turns ON keypad backlight
 * - Includes /data/local/tmp/cat_keylight.log for live debugging!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <linux/input.h>

#define LOG_FILE        "/data/local/tmp/cat_keylight.log"
#define HALL_DEV        "/dev/input/event3"
#define KEYLIGHT_SYSFS  "/sys/devices/platform/soc/soc:keyboard_light/leds/keyboard_light/brightness"
#define KEYLIGHT_ALT    "/sys/class/leds/keyboard_light/brightness"

static void log_msg(const char *msg) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (fp) {
        fprintf(fp, "%s\n", msg);
        fclose(fp);
    }
}

static void set_keypad_light(int val) {
    char logbuf[128];
    int fd = open(KEYLIGHT_SYSFS, O_WRONLY);
    if (fd < 0) {
        snprintf(logbuf, sizeof(logbuf), "Failed to open primary sysfs: errno=%d (%s)", errno, strerror(errno));
        log_msg(logbuf);
        fd = open(KEYLIGHT_ALT, O_WRONLY);
    }
    if (fd >= 0) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d\n", val);
        int wr = write(fd, buf, len);
        close(fd);
        snprintf(logbuf, sizeof(logbuf), "Wrote %d to keyboard_light (ret=%d)", val, wr);
        log_msg(logbuf);
    } else {
        snprintf(logbuf, sizeof(logbuf), "Failed to open alt sysfs: errno=%d (%s)", errno, strerror(errno));
        log_msg(logbuf);
    }
}

int main(void) {
    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    log_msg("--- cat_hall_keylight starting ---");

    int fd = -1;
    while (fd < 0) {
        fd = open(HALL_DEV, O_RDONLY);
        if (fd < 0) {
            char logbuf[128];
            snprintf(logbuf, sizeof(logbuf), "Failed to open HALL_DEV (%s): errno=%d (%s)", HALL_DEV, errno, strerror(errno));
            log_msg(logbuf);
            sleep(1);
        }
    }
    log_msg("Opened HALL_DEV successfully");

    /* Initial state set */
    set_keypad_light(255);

    struct input_event ev;
    int last_val = -1;

    for (;;) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n < (ssize_t)sizeof(ev)) {
            char logbuf[128];
            snprintf(logbuf, sizeof(logbuf), "Short read n=%zd (errno=%d: %s)", n, errno, strerror(errno));
            log_msg(logbuf);
            close(fd);
            sleep(1);
            fd = open(HALL_DEV, O_RDONLY);
            continue;
        }

        /* EV_SW = 0x05, SW_LID = 0x00 */
        if (ev.type == 0x05 && ev.code == 0x00) {
            char logbuf[128];
            snprintf(logbuf, sizeof(logbuf), "Got SW_LID event value=%d", ev.value);
            log_msg(logbuf);
            if (ev.value != last_val) {
                last_val = ev.value;
                if (ev.value == 0) {
                    /* Flip Closed -> Keypad light OFF immediately */
                    set_keypad_light(0);
                } else if (ev.value == 1) {
                    /* Flip Opened -> Keypad light ON immediately */
                    set_keypad_light(255);
                }
            }
        }
    }

    close(fd);
    return 0;
}
