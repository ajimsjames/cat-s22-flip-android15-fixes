/*
 * Cat S22 Flip PTT Hardware Service
 * - Reads PTT / Side button key events directly from /dev/input/event1
 * - Detects long press (>500ms):
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
#include <linux/input.h>

#define INPUT_DEV "/dev/input/event1"
#define PTT_KEY_CODE 248 /* KEY_MICMUTE / PTT */
#define LONG_PRESS_MS 500

static int get_call_state(char *number_out, size_t max_len) {
    /* Returns: 0 = IDLE, 1 = RINGING, 2 = OFFHOOK */
    FILE *fp = popen("dumpsys telephony.registry | grep -E 'mCallState|mCallIncomingNumber' | head -n 2", "r");
    if (!fp) return 0;

    char line[256];
    int state = 0;
    if (number_out && max_len > 0) number_out[0] = '\0';

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "mCallState")) {
            char *p = strchr(line, '=');
            if (p) state = atoi(p + 1);
        } else if (strstr(line, "mCallIncomingNumber")) {
            char *p = strchr(line, '=');
            if (p && number_out) {
                p++;
                while (*p == ' ' || *p == '\t') p++;
                size_t len = strlen(p);
                while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r' || p[len-1] == ' ')) {
                    p[--len] = '\0';
                }
                strncpy(number_out, p, max_len - 1);
                number_out[max_len - 1] = '\0';
            }
        }
    }
    pclose(fp);
    return state;
}

static void trigger_speaker_answer(void) {
    /* 1. Accept Call via telecom */
    system("telecom accept-call 2>/dev/null || input keyevent KEYCODE_CALL");
    usleep(300000);
    /* 2. Route audio to speaker */
    system("cmd media_session dispatch --key 79 2>/dev/null");
    system("service call audio 33 i32 1 2>/dev/null"); /* setSpeakerphoneOn(true) */
}

static void trigger_hangup(void) {
    system("telecom end-call 2>/dev/null || input keyevent KEYCODE_ENDCALL");
}

static void trigger_torch_toggle(void) {
    system("cmd statusbar toggle-flashlight 2>/dev/null");
}

int main(void) {
    signal(SIGCHLD, SIG_IGN);

    int fd = -1;
    while (fd < 0) {
        fd = open(INPUT_DEV, O_RDONLY);
        if (fd < 0) sleep(1);
    }

    struct input_event ev;
    struct timespec press_time = {0, 0};
    int is_pressed = 0;

    for (;;) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n < (ssize_t)sizeof(ev)) {
            close(fd);
            sleep(1);
            fd = open(INPUT_DEV, O_RDONLY);
            continue;
        }

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
                    /* LONG PRESS DETECTED */
                    if (call_state == 1) {
                        /* Incoming Call -> Answer in Speakerphone mode */
                        trigger_speaker_answer();
                    } else if (call_state == 2) {
                        /* Active Call -> Hang up / End Call */
                        trigger_hangup();
                    }
                } else {
                    /* SHORT PRESS */
                    if (call_state == 0) {
                        /* Idle -> Toggle Torch */
                        trigger_torch_toggle();
                    }
                }
            }
        }
    }

    close(fd);
    return 0;
}
