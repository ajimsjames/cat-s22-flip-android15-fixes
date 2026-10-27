/*
 * Cat S22 Flip Keypad Light & Hall Sensor Controller Daemon
 * - Dedicated standalone init daemon running under root.
 * - Monitors /dev/input/event3 (hall_sensor):
 *     - Value = 0 (hall near / flip closed):
 *         - Turns OFF keypad backlight (/sys/devices/platform/soc/soc:keyboard_light/leds/keyboard_light/brightness -> 0)
 *     - Value = 1 (hall far / flip opened):
 *         - Turns ON keypad backlight (/sys/devices/platform/soc/soc:keyboard_light/leds/keyboard_light/brightness -> 255)
 * - Zero display interference: DOES NOT TOUCH THE OUTER DISPLAY.
 * - Zero Zygote / Android system server dependency: Direct kernel sysfs writing with debouncing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/input.h>

#define HALL_DEV        "/dev/input/event3"
#define KEYLIGHT_SYSFS  "/sys/devices/platform/soc/soc:keyboard_light/leds/keyboard_light/brightness"
#define KEYLIGHT_ALT    "/sys/class/leds/keyboard_light/brightness"

static void set_keypad_light(int val) {
    int fd = open(KEYLIGHT_SYSFS, O_WRONLY);
    if (fd < 0) {
        fd = open(KEYLIGHT_ALT, O_WRONLY);
    }
    if (fd >= 0) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d\n", val);
        write(fd, buf, len);
        close(fd);
    }
}

int main(void) {
    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    int fd = -1;
    while (fd < 0) {
        fd = open(HALL_DEV, O_RDONLY);
        if (fd < 0) sleep(1);
    }

    /* Start with keypad light ON by default */
    set_keypad_light(255);

    struct input_event ev;
    int last_val = -1;

    for (;;) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n < (ssize_t)sizeof(ev)) {
            close(fd);
            sleep(1);
            fd = open(HALL_DEV, O_RDONLY);
            continue;
        }

        /* EV_SW = 0x05, SW_LID = 0x00 */
        if (ev.type == 0x05 && ev.code == 0x00) {
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
