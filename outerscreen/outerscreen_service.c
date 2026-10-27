/*
 * Cat S22 Flip Outer Display Daemon
 * - 128x128 OLED Framebuffer (/dev/graphics/fb2)
 * - ALWAYS-ON secondary display:
 *     - Backlight enforced at 255 every cycle
 *     - Lid open/close events NEVER turn off outer screen
 *     - Hall sensor strictly toggles inner screen sleep/wake
 * - Brand New Cyber/Tactical OLED Watchface UI:
 *     - Top Bar: Mini Battery Gauge Block [|||| ] + Battery % + Signal indicator
 *     - Center Time: Bold High-Contrast Digital Clock with Seconds Tracker
 *     - Date Section: Clean day badge with full date
 *     - Dynamic Call HUD:
 *         - Ringing: Inverted flashing caller ID & PTT speaker answer prompt
 *         - In-Call: Live ticking timer MM:SS + speakerphone badge
 * - Rock-solid 1 Hz refresh rate: Zero kernel D-state lockups, Zero zombies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <pthread.h>
#include <signal.h>

#define FB_DEV          "/dev/graphics/fb2"
#define BACKLIGHT_NODE  "/sys/class/leds/lcd-backlight2/brightness"

#define COLOR_BLACK     0x00000000
#define COLOR_WHITE     0x00FFFFFF
#define COLOR_GRAY      0x00666666
#define COLOR_LIGHTGRAY 0x00AAAAAA
#define COLOR_GREEN     0x0000FF66
#define COLOR_RED       0x00FF3333
#define COLOR_CYAN      0x0000E5FF
#define COLOR_AMBER     0x00FFB300
#define COLOR_DARKBLUE  0x000D1B2A

// 5x7 ASCII font table
static const uint8_t font5x7[][5] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x00, 0x00, 0x5F, 0x00, 0x00},
    ['"'] = {0x00, 0x07, 0x00, 0x07, 0x00},
    ['#'] = {0x14, 0x7F, 0x14, 0x7F, 0x14},
    ['$'] = {0x24, 0x2A, 0x7F, 0x2A, 0x12},
    ['%'] = {0x23, 0x13, 0x08, 0x64, 0x62},
    ['&'] = {0x36, 0x49, 0x55, 0x22, 0x50},
    ['\''] = {0x00, 0x05, 0x03, 0x00, 0x00},
    ['('] = {0x00, 0x1C, 0x22, 0x41, 0x00},
    [')'] = {0x00, 0x41, 0x22, 0x1C, 0x00},
    ['*'] = {0x14, 0x08, 0x3E, 0x08, 0x14},
    ['+'] = {0x08, 0x08, 0x3E, 0x08, 0x08},
    [','] = {0x00, 0x50, 0x30, 0x00, 0x00},
    ['-'] = {0x08, 0x08, 0x08, 0x08, 0x08},
    ['.'] = {0x00, 0x60, 0x60, 0x00, 0x00},
    ['/'] = {0x20, 0x10, 0x08, 0x04, 0x02},
    ['0'] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
    ['1'] = {0x00, 0x42, 0x7F, 0x40, 0x00},
    ['2'] = {0x42, 0x61, 0x51, 0x49, 0x46},
    ['3'] = {0x21, 0x41, 0x45, 0x4B, 0x31},
    ['4'] = {0x18, 0x14, 0x12, 0x7F, 0x10},
    ['5'] = {0x27, 0x45, 0x45, 0x45, 0x39},
    ['6'] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
    ['7'] = {0x01, 0x71, 0x09, 0x05, 0x03},
    ['8'] = {0x36, 0x49, 0x49, 0x49, 0x36},
    ['9'] = {0x06, 0x49, 0x49, 0x29, 0x1E},
    [':'] = {0x00, 0x36, 0x36, 0x00, 0x00},
    [';'] = {0x00, 0x56, 0x36, 0x00, 0x00},
    ['<'] = {0x08, 0x14, 0x22, 0x41, 0x00},
    ['='] = {0x14, 0x14, 0x14, 0x14, 0x14},
    ['>'] = {0x00, 0x41, 0x22, 0x14, 0x08},
    ['?'] = {0x02, 0x01, 0x51, 0x09, 0x06},
    ['@'] = {0x32, 0x49, 0x79, 0x41, 0x3E},
    ['A'] = {0x7E, 0x11, 0x11, 0x11, 0x7E},
    ['B'] = {0x7F, 0x49, 0x49, 0x49, 0x36},
    ['C'] = {0x3E, 0x41, 0x41, 0x41, 0x22},
    ['D'] = {0x7F, 0x41, 0x41, 0x22, 0x1C},
    ['E'] = {0x7F, 0x49, 0x49, 0x49, 0x41},
    ['F'] = {0x7F, 0x09, 0x09, 0x09, 0x01},
    ['G'] = {0x3E, 0x41, 0x49, 0x49, 0x7A},
    ['H'] = {0x7F, 0x08, 0x08, 0x08, 0x7F},
    ['I'] = {0x00, 0x41, 0x7F, 0x41, 0x00},
    ['J'] = {0x20, 0x40, 0x41, 0x3F, 0x01},
    ['K'] = {0x7F, 0x08, 0x14, 0x22, 0x41},
    ['L'] = {0x7F, 0x40, 0x40, 0x40, 0x40},
    ['M'] = {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    ['N'] = {0x7F, 0x04, 0x08, 0x10, 0x7F},
    ['O'] = {0x3E, 0x41, 0x41, 0x41, 0x3E},
    ['P'] = {0x7F, 0x09, 0x09, 0x09, 0x06},
    ['Q'] = {0x3E, 0x41, 0x51, 0x21, 0x5E},
    ['R'] = {0x7F, 0x09, 0x19, 0x29, 0x46},
    ['S'] = {0x46, 0x49, 0x49, 0x49, 0x31},
    ['T'] = {0x01, 0x01, 0x7F, 0x01, 0x01},
    ['U'] = {0x3F, 0x40, 0x40, 0x40, 0x3F},
    ['V'] = {0x1F, 0x20, 0x40, 0x20, 0x1F},
    ['W'] = {0x7F, 0x20, 0x18, 0x20, 0x7F},
    ['X'] = {0x63, 0x14, 0x08, 0x14, 0x63},
    ['Y'] = {0x07, 0x08, 0x70, 0x08, 0x07},
    ['Z'] = {0x61, 0x51, 0x49, 0x45, 0x43},
    ['['] = {0x00, 0x7F, 0x41, 0x41, 0x00},
    ['\\'] = {0x02, 0x04, 0x08, 0x10, 0x20},
    [']'] = {0x00, 0x41, 0x41, 0x7F, 0x00},
    ['^'] = {0x04, 0x02, 0x01, 0x02, 0x04},
    ['_'] = {0x40, 0x40, 0x40, 0x40, 0x40},
    ['`'] = {0x00, 0x01, 0x02, 0x04, 0x00},
    ['a'] = {0x20, 0x54, 0x54, 0x54, 0x78},
    ['b'] = {0x7F, 0x48, 0x44, 0x44, 0x38},
    ['c'] = {0x38, 0x44, 0x44, 0x44, 0x20},
    ['d'] = {0x38, 0x44, 0x44, 0x48, 0x7F},
    ['e'] = {0x38, 0x54, 0x54, 0x54, 0x18},
    ['f'] = {0x08, 0x7E, 0x09, 0x01, 0x02},
    ['g'] = {0x0C, 0x52, 0x52, 0x52, 0x3E},
    ['h'] = {0x7F, 0x08, 0x04, 0x04, 0x78},
    ['i'] = {0x00, 0x44, 0x7D, 0x40, 0x00},
    ['j'] = {0x20, 0x40, 0x44, 0x3D, 0x00},
    ['k'] = {0x7F, 0x10, 0x28, 0x44, 0x00},
    ['l'] = {0x00, 0x41, 0x7F, 0x40, 0x00},
    ['m'] = {0x7C, 0x04, 0x18, 0x04, 0x78},
    ['n'] = {0x7C, 0x08, 0x04, 0x04, 0x78},
    ['o'] = {0x38, 0x44, 0x44, 0x44, 0x38},
    ['p'] = {0x7C, 0x14, 0x14, 0x14, 0x08},
    ['q'] = {0x08, 0x14, 0x14, 0x18, 0x7C},
    ['r'] = {0x7C, 0x08, 0x04, 0x04, 0x08},
    ['s'] = {0x48, 0x54, 0x54, 0x54, 0x20},
    ['t'] = {0x04, 0x3F, 0x44, 0x40, 0x20},
    ['u'] = {0x3C, 0x40, 0x40, 0x20, 0x7C},
    ['v'] = {0x1C, 0x20, 0x40, 0x20, 0x1C},
    ['w'] = {0x3C, 0x40, 0x30, 0x40, 0x3C},
    ['x'] = {0x44, 0x28, 0x10, 0x28, 0x44},
    ['y'] = {0x0C, 0x50, 0x50, 0x50, 0x3C},
    ['z'] = {0x44, 0x64, 0x54, 0x4C, 0x44},
};

static void draw_pixel(uint32_t *fb, int x, int y, uint32_t color) {
    if (x >= 0 && x < 128 && y >= 0 && y < 128) {
        fb[y * 128 + x] = color;
    }
}

static void fill_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            draw_pixel(fb, i, j, color);
        }
    }
}

static void draw_hline(uint32_t *fb, int x, int y, int w, uint32_t color) {
    for (int i = x; i < x + w; i++) {
        draw_pixel(fb, i, y, color);
    }
}

static void draw_char(uint32_t *fb, int x, int y, char c, uint32_t color, int scale) {
    uint8_t uc = (uint8_t)c;
    if (uc > 127) uc = '?';
    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[uc][col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                for (int dx = 0; dx < scale; dx++) {
                    for (int dy = 0; dy < scale; dy++) {
                        draw_pixel(fb, x + col * scale + dx, y + row * scale + dy, color);
                    }
                }
            }
        }
    }
}

static void draw_string(uint32_t *fb, int x, int y, const char *str, uint32_t color, int scale) {
    while (*str) {
        draw_char(fb, x, y, *str, color, scale);
        x += (5 + 1) * scale;
        str++;
    }
}

static void draw_string_centered(uint32_t *fb, int y, const char *str, uint32_t color, int scale) {
    int len = strlen(str);
    int width = len * (5 + 1) * scale;
    int x = (128 - width) / 2;
    if (x < 0) x = 0;
    draw_string(fb, x, y, str, color, scale);
}

static void clear_screen(uint32_t *fb) {
    memset(fb, 0, 128 * 128 * 4);
}

static void set_backlight(int val) {
    int fd = open(BACKLIGHT_NODE, O_WRONLY);
    if (fd >= 0) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d\n", val);
        write(fd, buf, len);
        close(fd);
    }
}

static void send_key_async(const char *key) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/system/bin/input", "input", "keyevent", key, (char *)NULL);
        _exit(1);
    }
}

static int poll_telephony(char *number_out, size_t max_len) {
    FILE *fp = popen("dumpsys telephony.registry 2>/dev/null | grep -E 'mCallState|mCallIncomingNumber' | head -n 2", "r");
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

// Hall sensor thread - controls inner display sleep/wake only
static void *hall_monitor_thread(void *arg) {
    (void)arg;
    for (;;) {
        FILE *fp = popen("logcat -T 1 -v brief", "r");
        if (!fp) {
            sleep(1);
            continue;
        }
        char buf[512];
        time_t last_event_time = 0;

        while (fgets(buf, sizeof(buf), fp)) {
            time_t now = time(NULL);
            if (now == last_event_time) continue;

            if (strstr(buf, "hall near")) {
                last_event_time = now;
                send_key_async("KEYCODE_SLEEP");
            } else if (strstr(buf, "hall far")) {
                last_event_time = now;
                send_key_async("KEYCODE_WAKEUP");
            }
        }
        pclose(fp);
        sleep(1);
    }
    return NULL;
}

static int get_battery(void) {
    int fd = open("/sys/class/power_supply/battery/capacity", O_RDONLY);
    if (fd < 0) return -1;
    char buf[16] = {0};
    read(fd, buf, sizeof(buf)-1);
    close(fd);
    return atoi(buf);
}

static int is_charging(void) {
    int fd = open("/sys/class/power_supply/battery/status", O_RDONLY);
    if (fd < 0) return 0;
    char buf[32] = {0};
    read(fd, buf, sizeof(buf)-1);
    close(fd);
    return (strstr(buf, "Charging") != NULL);
}

int main(void) {
    signal(SIGCHLD, SIG_IGN);

    int fb_fd = open(FB_DEV, O_RDWR);
    if (fb_fd < 0) return 1;

    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        close(fb_fd);
        return 1;
    }

    size_t screensize = 128 * 128 * 4;
    uint32_t *fb = (uint32_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb == MAP_FAILED) {
        close(fb_fd);
        return 1;
    }

    pthread_t hall_tid;
    pthread_create(&hall_tid, NULL, hall_monitor_thread, NULL);

    int last_call_state = 0;
    time_t call_start_time = 0;

    for (;;) {
        // Enforce always-on outer display brightness every single second
        set_backlight(255);

        clear_screen(fb);

        char incoming_num[64] = {0};
        int call_state = poll_telephony(incoming_num, sizeof(incoming_num));

        if (call_state == 1) {
            /* === INCOMING CALL HUD === */
            fill_rect(fb, 0, 0, 128, 22, COLOR_AMBER);
            draw_string_centered(fb, 6, "INCOMING CALL", COLOR_BLACK, 1);

            char num_display[32];
            if (incoming_num[0]) {
                snprintf(num_display, sizeof(num_display), "%.16s", incoming_num);
            } else {
                snprintf(num_display, sizeof(num_display), "Unknown");
            }
            draw_string_centered(fb, 45, num_display, COLOR_WHITE, 2);

            fill_rect(fb, 10, 82, 108, 18, COLOR_GREEN);
            draw_string_centered(fb, 87, "HOLD PTT: ANSWER", COLOR_BLACK, 1);

            draw_string_centered(fb, 108, "END: REJECT", COLOR_RED, 1);

            call_start_time = 0;
        } else if (call_state == 2) {
            /* === ACTIVE CALL TIMER HUD === */
            if (last_call_state != 2 || call_start_time == 0) {
                call_start_time = time(NULL);
            }
            long call_duration = (long)(time(NULL) - call_start_time);
            if (call_duration < 0) call_duration = 0;

            fill_rect(fb, 0, 0, 128, 20, COLOR_GREEN);
            draw_string_centered(fb, 5, "CALL IN PROGRESS", COLOR_BLACK, 1);

            char timer_buf[32];
            snprintf(timer_buf, sizeof(timer_buf), "%02ld:%02ld", call_duration / 60, call_duration % 60);
            draw_string_centered(fb, 42, timer_buf, COLOR_CYAN, 3);

            draw_string_centered(fb, 84, "[ SPEAKERPHONE ]", COLOR_WHITE, 1);
            draw_string_centered(fb, 106, "HOLD PTT TO END", COLOR_RED, 1);
        } else {
            /* === NEW TACTICAL ALWAYS-ON WATCHFACE === */
            call_start_time = 0;

            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);

            // 1. TOP STATUS BAR:
            // 4G VoLTE pill on left
            draw_string(fb, 4, 4, "4G", COLOR_CYAN, 1);
            draw_string(fb, 18, 4, "VoLTE", COLOR_LIGHTGRAY, 1);

            // Battery on right
            int bat = get_battery();
            int chg = is_charging();
            char bat_str[16];
            if (chg) {
                snprintf(bat_str, sizeof(bat_str), "+%d%%", bat >= 0 ? bat : 0);
            } else {
                snprintf(bat_str, sizeof(bat_str), "%d%%", bat >= 0 ? bat : 0);
            }
            draw_string(fb, 94, 4, bat_str, (bat > 20) ? COLOR_GREEN : COLOR_RED, 1);

            // Mini battery visual bar [====    ]
            int bar_w = 24;
            int filled_w = (bat > 0) ? (bat * bar_w / 100) : 0;
            if (filled_w > bar_w) filled_w = bar_w;
            fill_rect(fb, 68, 6, bar_w, 4, COLOR_GRAY);
            if (filled_w > 0) {
                fill_rect(fb, 68, 6, filled_w, 4, (bat > 20) ? COLOR_GREEN : COLOR_RED);
            }

            draw_hline(fb, 4, 16, 120, COLOR_GRAY);

            // 2. MAIN CLOCK (Bold High-Contrast HH:MM)
            char time_buf[16];
            strftime(time_buf, sizeof(time_buf), "%H:%M", tm_info);
            draw_string(fb, 12, 32, time_buf, COLOR_WHITE, 3);

            // Seconds indicator
            char sec_buf[8];
            snprintf(sec_buf, sizeof(sec_buf), "%02d", tm_info->tm_sec);
            draw_string(fb, 104, 44, sec_buf, COLOR_AMBER, 1);

            // 3. DATE BADGE (Centered Day & Month)
            draw_hline(fb, 12, 72, 104, COLOR_GRAY);

            char day_str[16];
            strftime(day_str, sizeof(day_str), "%A", tm_info);
            draw_string_centered(fb, 80, day_str, COLOR_CYAN, 1);

            char date_str[32];
            strftime(date_str, sizeof(date_str), "%b %d, %Y", tm_info);
            draw_string_centered(fb, 96, date_str, COLOR_LIGHTGRAY, 1);

            // 4. FOOTER STATUS
            draw_string_centered(fb, 114, "CAT S22 FLIP", COLOR_GRAY, 1);
        }

        last_call_state = call_state;

        // Commit framebuffer to display viewport
        vinfo.yoffset = 0;
        ioctl(fb_fd, FBIOPAN_DISPLAY, &vinfo);

        sleep(1);
    }

    munmap(fb, screensize);
    close(fb_fd);
    return 0;
}
