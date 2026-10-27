/*
 * Cat S22 Flip Futuristic OLED Daemon (Cyber HUD Edition)
 * - Display: 128x128 OLED via /dev/graphics/fb2
 * - Always-On: Continual backlight enforcement at 255.
 * - Hall Sensor: Strictly controls internal display sleep/wake.
 * - Futuristic Cyberpunk HUD Design:
 *     - Sci-Fi Corner Brackets: [┌ ┐, └ ┘]
 *     - Status Header: Cyber telemetry (SYS:OK, 4G NET, segmented power cell gauge)
 *     - Center Hologram Clock: High-visibility digital font with framed [:SS] micro-timer
 *     - Telemetry Date: "28-OCT // TUE"
 *     - Transmission/In-Call HUD:
 *         - Incoming: Cyber transmission alert banner, caller id, PTT trigger prompt
 *         - Active Call: Live COMM timer MM:SS, dynamic equalizer visualizer wave simulation
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
#define COLOR_CYAN      0x0000E5FF
#define COLOR_NEON_BLUE 0x000099FF
#define COLOR_GREEN     0x0000FF66
#define COLOR_AMBER     0x00FFB300
#define COLOR_RED       0x00FF3333
#define COLOR_DARKGRAY  0x00333333
#define COLOR_MIDGRAY   0x00666666
#define COLOR_LIGHTGRAY 0x00AAAAAA

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
    ['{'] = {0x00, 0x08, 0x36, 0x41, 0x00},
    ['|'] = {0x00, 0x00, 0x7F, 0x00, 0x00},
    ['}'] = {0x00, 0x41, 0x36, 0x08, 0x00},
    ['~'] = {0x08, 0x08, 0x2A, 0x1C, 0x08},
};

static void draw_pixel(uint32_t *fb, int x, int y, uint32_t color) {
    if (x >= 0 && x < 128 && y >= 0 && y < 128) {
        fb[y * 128 + x] = color;
    }
}

static void draw_char(uint32_t *fb, int x, int y, char c, uint32_t color, int scale) {
    if ((unsigned char)c > 127) c = ' ';
    const uint8_t *glyph = font5x7[(int)c];
    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                for (int sx = 0; sx < scale; sx++) {
                    for (int sy = 0; sy < scale; sy++) {
                        draw_pixel(fb, x + col * scale + sx, y + row * scale + sy, color);
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
    int total_width = len * 6 * scale;
    int x = (128 - total_width) / 2;
    if (x < 0) x = 0;
    draw_string(fb, x, y, str, color, scale);
}

static void fill_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            draw_pixel(fb, i, j, color);
        }
    }
}

static void draw_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t color) {
    for (int i = x; i < x + w; i++) {
        draw_pixel(fb, i, y, color);
        draw_pixel(fb, i, y + h - 1, color);
    }
    for (int j = y; j < y + h; j++) {
        draw_pixel(fb, x, j, color);
        draw_pixel(fb, x + w - 1, j, color);
    }
}

static void draw_hline(uint32_t *fb, int x, int y, int len, uint32_t color) {
    for (int i = 0; i < len; i++) draw_pixel(fb, x + i, y, color);
}

static void draw_vline(uint32_t *fb, int x, int y, int len, uint32_t color) {
    for (int j = 0; j < len; j++) draw_pixel(fb, x, y + j, color);
}

// Sci-Fi Corner Brackets [┌ ┐, └ ┘]
static void draw_sci_fi_corners(uint32_t *fb, int x, int y, int w, int h, int len, uint32_t color) {
    // Top-left
    draw_hline(fb, x, y, len, color);
    draw_vline(fb, x, y, len, color);
    // Top-right
    draw_hline(fb, x + w - len, y, len, color);
    draw_vline(fb, x + w - 1, y, len, color);
    // Bottom-left
    draw_hline(fb, x, y + h - 1, len, color);
    draw_vline(fb, x, y + h - len, len, color);
    // Bottom-right
    draw_hline(fb, x + w - len, y + h - 1, len, color);
    draw_vline(fb, x + w - 1, y + h - len, len, color);
}

static void clear_screen(uint32_t *fb) {
    memset(fb, 0, 128 * 128 * 4);
}

static void set_backlight(int brightness) {
    int fd = open(BACKLIGHT_NODE, O_WRONLY);
    if (fd >= 0) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d\n", brightness);
        write(fd, buf, len);
        close(fd);
    }
}

static int get_battery(void) {
    FILE *fp = fopen("/sys/class/power_supply/battery/capacity", "r");
    if (!fp) return -1;
    int cap = 0;
    if (fscanf(fp, "%d", &cap) != 1) cap = -1;
    fclose(fp);
    return cap;
}

static int is_charging(void) {
    FILE *fp = fopen("/sys/class/power_supply/battery/status", "r");
    if (!fp) return 0;
    char status[32] = {0};
    if (fscanf(fp, "%31s", status) == 1) {
        fclose(fp);
        return (strcmp(status, "Charging") == 0 || strcmp(status, "Full") == 0);
    }
    fclose(fp);
    return 0;
}

static int poll_telephony(char *caller_num, size_t max_len) {
    FILE *fp = popen("dumpsys telephony.registry 2>/dev/null", "r");
    if (!fp) return 0;

    char line[256];
    int call_state = 0; // 0 = IDLE, 1 = RINGING, 2 = OFFHOOK
    caller_num[0] = '\0';

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "mCallState=1") || strstr(line, "mDataConnectionState=1")) {
            call_state = 1;
        } else if (strstr(line, "mCallState=2")) {
            call_state = 2;
        }

        char *incoming = strstr(line, "mCallIncomingNumber=");
        if (incoming) {
            sscanf(incoming, "mCallIncomingNumber=%s", caller_num);
        }
    }
    pclose(fp);
    return call_state;
}

static void *hall_monitor_thread(void *arg) {
    int fd = open("/dev/input/event0", O_RDONLY);
    if (fd < 0) return NULL;

    struct {
        struct timeval time;
        uint16_t type;
        uint16_t code;
        int32_t value;
    } ev;

    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == 0x05 && ev.code == 0x00) { // EV_SW, SW_LID
            if (ev.value == 1) {
                // Lid Closed -> Internal display sleep (Outer stays lit!)
                system("input keyevent KEYCODE_SLEEP &");
            } else if (ev.value == 0) {
                // Lid Opened -> Internal display wake
                system("input keyevent KEYCODE_WAKEUP &");
            }
        }
    }
    close(fd);
    return NULL;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);

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
    int anim_tick = 0;

    for (;;) {
        // Enforce always-on outer display brightness every single second
        set_backlight(255);

        clear_screen(fb);

        char incoming_num[64] = {0};
        int call_state = poll_telephony(incoming_num, sizeof(incoming_num));

        if (call_state == 1) {
            /* === CYBER TRANSMISSION ALERT HUD (INCOMING CALL) === */
            draw_sci_fi_corners(fb, 2, 2, 124, 124, 8, COLOR_AMBER);

            // Flashing header banner
            if (anim_tick % 2 == 0) {
                fill_rect(fb, 8, 8, 112, 16, COLOR_AMBER);
                draw_string_centered(fb, 12, ">> TRANSMISSION <<", COLOR_BLACK, 1);
            } else {
                draw_rect(fb, 8, 8, 112, 16, COLOR_AMBER);
                draw_string_centered(fb, 12, "INCOMING CALL", COLOR_AMBER, 1);
            }

            // Caller Telemetry ID
            char num_display[32];
            if (incoming_num[0]) {
                snprintf(num_display, sizeof(num_display), "%.15s", incoming_num);
            } else {
                snprintf(num_display, sizeof(num_display), "UNKNOWN_SRC");
            }
            draw_string_centered(fb, 42, "SRC TELEMETRY:", COLOR_LIGHTGRAY, 1);
            draw_string_centered(fb, 56, num_display, COLOR_CYAN, 2);

            // PTT Engagement Prompt
            fill_rect(fb, 10, 84, 108, 16, COLOR_GREEN);
            draw_string_centered(fb, 88, "[PTT] ENGAGE COMMS", COLOR_BLACK, 1);

            draw_string_centered(fb, 106, "END: REJECT", COLOR_RED, 1);
            call_start_time = 0;

        } else if (call_state == 2) {
            /* === ACTIVE COMMS TERMINAL HUD === */
            if (last_call_state != 2 || call_start_time == 0) {
                call_start_time = time(NULL);
            }
            long call_duration = (long)(time(NULL) - call_start_time);
            if (call_duration < 0) call_duration = 0;

            draw_sci_fi_corners(fb, 2, 2, 124, 124, 8, COLOR_CYAN);

            // Top Status Pill
            fill_rect(fb, 12, 6, 104, 14, COLOR_GREEN);
            draw_string_centered(fb, 9, "LIVE COMM LINK", COLOR_BLACK, 1);

            // Live MM:SS Duration Timer
            char timer_buf[32];
            snprintf(timer_buf, sizeof(timer_buf), "%02ld:%02ld", call_duration / 60, call_duration % 60);
            draw_string_centered(fb, 30, timer_buf, COLOR_WHITE, 3);

            // Simulated Audio Frequency Wave Visualizer
            int eq_bars[] = {4, 12, 8, 16, 22, 14, 20, 10, 18, 6, 14, 8};
            int bar_count = sizeof(eq_bars) / sizeof(eq_bars[0]);
            int base_y = 78;
            for (int b = 0; b < bar_count; b++) {
                int h = eq_bars[(b + anim_tick) % bar_count];
                fill_rect(fb, 18 + b * 8, base_y - h / 2, 4, h, COLOR_CYAN);
            }

            draw_string_centered(fb, 88, "SPK: ROUTED ACTIVE", COLOR_NEON_BLUE, 1);
            draw_string_centered(fb, 106, "[PTT] TERMINATE", COLOR_RED, 1);

        } else {
            /* === FUTURISTIC CYBER HUD WATCHFACE (ALWAYS-ON) === */
            call_start_time = 0;

            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);

            // 1. Sci-Fi Outer Framing Brackets
            draw_sci_fi_corners(fb, 2, 2, 124, 124, 8, COLOR_CYAN);

            // 2. Top Telemetry Header
            draw_string(fb, 8, 6, "4G+", COLOR_CYAN, 1);
            draw_string(fb, 28, 6, "SYS:OK", COLOR_GREEN, 1);

            // Segmented Energy Cell Bar & Battery %
            int bat = get_battery();
            int chg = is_charging();
            char bat_txt[12];
            if (chg) {
                snprintf(bat_txt, sizeof(bat_txt), "+%d%%", bat >= 0 ? bat : 0);
            } else {
                snprintf(bat_txt, sizeof(bat_txt), "%d%%", bat >= 0 ? bat : 0);
            }
            draw_string(fb, 94, 6, bat_txt, (bat > 20) ? COLOR_GREEN : COLOR_RED, 1);

            // 5-block energy cell gauge
            int num_segments = 5;
            int active_segments = (bat > 0) ? (bat * num_segments / 100) : 0;
            if (active_segments > num_segments) active_segments = num_segments;
            for (int s = 0; s < num_segments; s++) {
                uint32_t seg_color = (s < active_segments) ? ((bat > 20) ? COLOR_GREEN : COLOR_RED) : COLOR_DARKGRAY;
                fill_rect(fb, 72 + s * 4, 8, 3, 5, seg_color);
            }

            // Tech Divider
            draw_hline(fb, 6, 17, 116, COLOR_MIDGRAY);
            draw_pixel(fb, 64, 17, COLOR_CYAN); // Center focus pip

            // 3. Center Holographic Clock (Large 3x Scale)
            char time_buf[16];
            strftime(time_buf, sizeof(time_buf), "%H:%M", tm_info);
            draw_string(fb, 12, 30, time_buf, COLOR_WHITE, 3);

            // Futuristic micro-timer box [:SS]
            draw_rect(fb, 98, 34, 22, 18, COLOR_NEON_BLUE);
            char sec_buf[8];
            snprintf(sec_buf, sizeof(sec_buf), "%02d", tm_info->tm_sec);
            draw_string(fb, 102, 39, sec_buf, COLOR_AMBER, 1);

            // Tech Sub-Divider with Accent Ticks
            draw_hline(fb, 6, 66, 116, COLOR_MIDGRAY);
            draw_pixel(fb, 6, 65, COLOR_CYAN);
            draw_pixel(fb, 121, 65, COLOR_CYAN);

            // 4. Military/Aero Date Telemetry: "28-OCT // TUE"
            char date_line[32];
            char month_str[8], day_str[8];
            strftime(month_str, sizeof(month_str), "%b", tm_info);
            for (int i = 0; month_str[i]; i++) if (month_str[i] >= 'a' && month_str[i] <= 'z') month_str[i] -= 32;
            strftime(day_str, sizeof(day_str), "%a", tm_info);
            for (int i = 0; day_str[i]; i++) if (day_str[i] >= 'a' && day_str[i] <= 'z') day_str[i] -= 32;

            snprintf(date_line, sizeof(date_line), "%02d-%s // %s", tm_info->tm_mday, month_str, day_str);
            draw_string_centered(fb, 76, date_line, COLOR_CYAN, 1);

            // Secondary Info Bar: "CAT-S22  ::  STANDBY"
            draw_rect(fb, 8, 94, 112, 16, COLOR_DARKGRAY);
            draw_string_centered(fb, 98, "CAT-S22 // STANDBY", COLOR_LIGHTGRAY, 1);

            // Footer Micro Status Pips
            for (int p = 0; p < 7; p++) {
                uint32_t pip_color = ((anim_tick % 7) == p) ? COLOR_CYAN : COLOR_DARKGRAY;
                draw_pixel(fb, 46 + p * 6, 118, pip_color);
            }
        }

        last_call_state = call_state;
        anim_tick++;

        // Commit framebuffer to display viewport
        vinfo.yoffset = 0;
        ioctl(fb_fd, FBIOPAN_DISPLAY, &vinfo);

        sleep(1);
    }

    munmap(fb, screensize);
    close(fb_fd);
    return 0;
}
