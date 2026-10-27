/*
 * Cat S22 Flip PTT Hardware Service (Multi-Device Auto-Discovery)
 * - Auto-detects and opens both /dev/input/event1 and uinput virtual devices
 * - Does NOT use EVIOCGRAB so it co-exists with keypad debounce
 * - Detects long-press (>500ms):
 *     - If incoming call (RINGING): Answers call via Speakerphone
 *     - If active call (OFFHOOK): Hangs up / ends call
 * - If idle: Short press toggles torch / flashlight
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

#define PTT_KEY_CODE    248 /* KEY_MICMUTE / PTT */
#define LONG_PRESS_MS   500

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

                            if (elapsed_ms >= LONG_PRESS_MS) {
                                /* LONG PRESS (>500ms) */
                                if (call_state == 1) {
                                    trigger_speaker_answer();
                                } else if (call_state == 2) {
                                    trigger_hangup();
                                }
                            } else {
                                /* SHORT PRESS */
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
