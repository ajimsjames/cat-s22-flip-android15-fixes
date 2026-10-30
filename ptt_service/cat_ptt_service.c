/*
 * Cat S22 Flip PTT Hardware Service (Multi-Device Auto-Discovery)
 * - Auto-detects and opens both /dev/input/event1 and uinput virtual devices
 * - Does NOT use EVIOCGRAB so it co-exists with keypad debounce
 * - Detects press duration:
 *     SHORT PRESS (<500ms):
 *         - If idle: Toggles torch / flashlight
 *     LONG PRESS (500ms – 1999ms):
 *         - If incoming call (RINGING): Answers call via Speakerphone
 *         - If active call (OFFHOOK): Hangs up / ends call
 *     LONG-LONG PRESS (>=2000ms, idle only):
 *         - 1st press: Kill all outerscreen processes (outerscreen_service + cat_watchdog)
 *           Outer display shows black/off - useful to clear frozen/zombie processes
 *         - 2nd press: Restart outerscreen_service + cat_watchdog via init
 * Zero root / Zero Magisk required - runs as native init service.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <poll.h>
#include <dirent.h>
#include <linux/input.h>

#define PTT_KEY_CODE       523  /* KEY_NUMERIC_B — actual PTT button keycode on Cat S22 Flip event7 */
#define SHORT_PRESS_MS     500
#define LONG_LONG_PRESS_MS 2000

/* Toggle: 0 = outerscreen is running, 1 = outerscreen killed (next press restarts) */
static int outerscreen_killed = 0;

static int get_call_state(char *number_out, size_t max_len) {
    /* Returns: 0 = IDLE, 1 = RINGING, 2 = OFFHOOK */
    FILE *fp = popen("dumpsys telephony.registry 2>/dev/null", "r");
    if (!fp) return 0;

    char line[256];
    int state = 0;
    if (number_out && max_len > 0) number_out[0] = '\0';

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "mCallState=1") || strstr(line, "mDataConnectionState=1")) {
            state = 1;
        } else if (strstr(line, "mCallState=2")) {
            state = 2;
        }

        char *incoming = strstr(line, "mCallIncomingNumber=");
        if (incoming && number_out && max_len > 0) {
            sscanf(incoming, "mCallIncomingNumber=%s", number_out);
        }
    }
    pclose(fp);
    return state;
}

static void trigger_speaker_answer(void) {
    /* 1. Accept call */
    system("telecom accept-call 2>/dev/null || input keyevent KEYCODE_CALL");
    usleep(350000);
    /* 2. Route speaker */
    system("service call audio 33 i32 1 2>/dev/null"); /* setSpeakerphoneOn(true) */
    system("cmd media_session dispatch --key 79 2>/dev/null");
}

static void trigger_hangup(void) {
    system("telecom end-call 2>/dev/null || input keyevent KEYCODE_ENDCALL");
}

static void trigger_torch_toggle(void) {
    system("cmd statusbar toggle-flashlight 2>/dev/null");
}

/* Kill all outerscreen services and the watchdog to prevent auto-restart */
static void outerscreen_kill_all(void) {
    /* Kill watchdog first so it doesn't restart outerscreen immediately */
    system("kill -9 $(pidof cat_watchdog) 2>/dev/null; killall -9 cat_watchdog 2>/dev/null");
    usleep(100000);
    system("kill -9 $(pidof outerscreen_service) 2>/dev/null; killall -9 outerscreen_service 2>/dev/null");
    outerscreen_killed = 1;
}

/* Restart outerscreen_service and cat_watchdog via init */
static void outerscreen_restart(void) {
    /* Re-start via Android init (registered in cat_hardware.rc) */
    system("start outerscreen 2>/dev/null");
    usleep(500000);
    system("start cat_watchdog 2>/dev/null");
    outerscreen_killed = 0;
}

int main(void) {
    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    /* Open all potential input event devices in non-blocking mode */
    struct pollfd fds[16];
    int num_fds = 0;

    for (int i = 0; i < 16; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            fds[num_fds].fd = fd;
            fds[num_fds].events = POLLIN;
            num_fds++;
        }
    }

    if (num_fds == 0) return 1;

    struct timespec press_time = {0, 0};
    int is_pressed = 0;

    for (;;) {
        int ret = poll(fds, num_fds, 1000);
        if (ret < 0) continue;

        for (int i = 0; i < num_fds; i++) {
            if (fds[i].revents & POLLIN) {
                struct input_event ev;
                while (read(fds[i].fd, &ev, sizeof(ev)) == sizeof(ev)) {
                    if (ev.type == EV_KEY && ev.code == PTT_KEY_CODE) {
                        if (ev.value == 1) { /* Key Down */
                            clock_gettime(CLOCK_MONOTONIC, &press_time);
                            is_pressed = 1;
                        } else if (ev.value == 0 && is_pressed) { /* Key Up */
                            struct timespec release_time;
                            clock_gettime(CLOCK_MONOTONIC, &release_time);
                            long elapsed_ms = (release_time.tv_sec - press_time.tv_sec) * 1000 +
                                              (release_time.tv_nsec - press_time.tv_nsec) / 1000000;
                            is_pressed = 0;

                            char incoming_num[64] = {0};
                            int call_state = get_call_state(incoming_num, sizeof(incoming_num));

                            if (elapsed_ms >= LONG_LONG_PRESS_MS) {
                                /* LONG-LONG PRESS (>=2s): outerscreen kill / restart toggle */
                                if (call_state == 0) {
                                    if (!outerscreen_killed) {
                                        outerscreen_kill_all();
                                    } else {
                                        outerscreen_restart();
                                    }
                                }
                            } else if (elapsed_ms >= SHORT_PRESS_MS) {
                                /* LONG PRESS (500ms–1999ms): call actions */
                                if (call_state == 1) {
                                    trigger_speaker_answer();
                                } else if (call_state == 2) {
                                    trigger_hangup();
                                }
                            } else {
                                /* SHORT PRESS (<500ms): torch toggle when idle */
                                if (call_state == 0) {
                                    trigger_torch_toggle();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < num_fds; i++) close(fds[i].fd);
    return 0;
}
