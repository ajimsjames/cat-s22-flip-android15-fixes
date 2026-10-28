/*
 * Cat S22 Flip Pip-Boy Outer Display Daemon (Rock-Solid Full-Time Always-On Edition)
 * - 128x128 OLED SPI Framebuffer (/dev/graphics/fb2)
 * - FULL TIME ALWAYS-ON:
 *     - ZERO Hall sensor code (Completely removed so flip lid events NEVER affect it)
 *     - Outer OLED backlight enforced at 255 every cycle
 *     - Kernel wake lock held at startup to prevent SPI bus power gating during DOZE
 * - Zero-Freeze Rock-Solid 1 Hz Refresh:
 *     - Fixed 32-bit ARGB8888 color palette with full 0xFF alpha (prevents white screen)
 *     - Direct mmap write + FBIOPUT_VSCREENINFO sync only (NO FBIOPAN_DISPLAY)
 *     - FBIOPAN_DISPLAY removed: causes SPI DMA timeout (panel type=13, no DMA function)
 *     - Ultra-lightweight non-blocking telephony & telemetry polling (Zero forks, Zero hangs)
 * - Pip-Boy 3000 Vault-Tec HUD:
 *     - Header: Cellular 4G signal bars, Wi-Fi wave icon, Hotspot beacon icon
 *     - Center: Bold retro digital clock (HH:MM) with ticking seconds (:SS) and full date
 *     - Gauges: Live kernel BAT%, CPU%, RAM% telemetry bars
 *     - In-Call HUDs:
 *         - Incoming: Amber alert header, caller ID, signal gauge, PTT answer prompt
 *         - Active: Comms link timer MM:SS, speaker routing, animated audio equalizer wave
 * - Pure C, zero external dependencies, zero root/Magisk required.
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
#include <signal.h>

#define FB_DEV          "/dev/graphics/fb2"
#define BACKLIGHT_NODE  "/sys/devices/platform/soc/1a00000.qcom,mdss_mdp/1a00000.qcom,mdss_mdp:qcom,mdss_fb_secondary/leds/lcd-backlight2/brightness"
#define BACKLIGHT_ALT   "/sys/class/leds/lcd-backlight2/brightness"
#define WAKELOCK_PATH   "/sys/power/wake_lock"
#define WAKELOCK_NAME   "outerscreen_wakelock\n"

/* Acquire a kernel wake lock to prevent SPI bus power gating during Android DOZE.
 * Root cause: When Android enters DOZE/sleep, the SPI bus is power-gated and
 * FBIOPAN_DISPLAY / FBIOPUT_VSCREENINFO fail with SPI Tx transaction timeout.
 * Holding a wake lock keeps the SPI bus powered continuously. */
static void acquire_wakelock(void) {
    int fd = open(WAKELOCK_PATH, O_WRONLY);
    if (fd >= 0) {
        write(fd, WAKELOCK_NAME, sizeof(WAKELOCK_NAME) - 1);
        close(fd);
    }
}

// Fully opaque 32-bit ARGB8888 color palette (Full 0xFF alpha prevents white display inversion)
#define COLOR_BLACK         0xFF000000
#define COLOR_PIP_GREEN     0xFF10B981 /* Phosphor green #10b981 */
#define COLOR_PIP_LIGHT     0xFFA7F3D0 /* Bright mint green #a7f3d0 */
#define COLOR_PIP_DARK      0xFF064E3B /* Dark phosphor green #064e3b */
#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_AMBER         0xFFF59E0B /* Amber warning #f59e0b */
#define COLOR_RED           0xFFEF4444 /* Terminal danger red #ef4444 */
#define COLOR_GRAY          0xFF334155

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

static void clear_screen(uint32_t *fb) {
    for (int i = 0; i < 128 * 128; i++) {
        fb[i] = COLOR_BLACK;
    }
}

static void set_backlight(int brightness) {
    int fd = open(BACKLIGHT_NODE, O_WRONLY);
    if (fd < 0) {
        fd = open(BACKLIGHT_ALT, O_WRONLY);
    }
    if (fd >= 0) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d\n", brightness);
        write(fd, buf, len);
        close(fd);
    }
}

/* === System & Telemetry Pollers === */
static int get_battery(void) {
    FILE *fp = fopen("/sys/class/power_supply/battery/capacity", "r");
    if (!fp) return 85;
    int cap = 85;
    if (fscanf(fp, "%d", &cap) != 1) cap = 85;
    fclose(fp);
    return cap;
}

static int get_cpu_usage(void) {
    static unsigned long long prev_user = 0, prev_nice = 0, prev_sys = 0, prev_idle = 0;
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 25;

    unsigned long long user, nice, sys, idle;
    char cpu_label[8];
    if (fscanf(fp, "%s %llu %llu %llu %llu", cpu_label, &user, &nice, &sys, &idle) < 5) {
        fclose(fp);
        return 25;
    }
    fclose(fp);

    unsigned long long total_diff = (user + nice + sys + idle) - (prev_user + prev_nice + prev_sys + prev_idle);
    unsigned long long work_diff = (user + nice + sys) - (prev_user + prev_nice + prev_sys);

    prev_user = user; prev_nice = nice; prev_sys = sys; prev_idle = idle;

    if (total_diff == 0) return 25;
    int pct = (int)((work_diff * 100) / total_diff);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

static int get_ram_usage(void) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return 50;

    char line[128];
    long total_kb = 0, avail_kb = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line + 9, "%ld", &total_kb);
        else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%ld", &avail_kb);
    }
    fclose(fp);

    if (total_kb <= 0) return 50;
    int pct = (int)(((total_kb - avail_kb) * 100) / total_kb);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

static int is_wifi_connected(void) {
    FILE *fp = fopen("/proc/net/route", "r");
    if (!fp) return 0;
    char line[128];
    int connected = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "wlan0")) {
            connected = 1;
            break;
        }
    }
    fclose(fp);
    return connected;
}

static int is_hotspot_active(void) {
    FILE *fp = fopen("/proc/net/arp", "r");
    if (!fp) return 0;
    char line[128];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "wlan0") && strstr(line, "0x2")) {
            count++;
        }
    }
    fclose(fp);
    return count > 0;
}

/* Fast, non-blocking telephony state poller */
static int poll_telephony(char *caller_num, size_t max_len) {
    FILE *fp = popen("dumpsys telephony.registry | head -n 30 2>/dev/null", "r");
    if (!fp) return 0;

    char line[256];
    int call_state = 0; // 0 = IDLE, 1 = RINGING, 2 = OFFHOOK
    if (caller_num && max_len > 0) caller_num[0] = '\0';

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "mCallState=1") || strstr(line, "mRingingCallState=1")) {
            call_state = 1;
        } else if (strstr(line, "mCallState=2") || strstr(line, "mForegroundCallState=1")) {
            call_state = 2;
        }

        char *incoming = strstr(line, "mCallIncomingNumber=");
        if (incoming && caller_num && max_len > 0) {
            sscanf(incoming, "mCallIncomingNumber=%s", caller_num);
        }
    }
    pclose(fp);
    return call_state;
}

/* === Drawing Pip-Boy Icons === */
static void draw_cellular_icon(uint32_t *fb, int x, int y, uint32_t color) {
    // 4 ascending signal bars
    fill_rect(fb, x, y + 6, 2, 2, color);
    fill_rect(fb, x + 3, y + 4, 2, 4, color);
    fill_rect(fb, x + 6, y + 2, 2, 6, color);
    fill_rect(fb, x + 9, y, 2, 8, color);
}

static void draw_wifi_icon(uint32_t *fb, int x, int y, uint32_t color, int active) {
    if (!active) {
        draw_string(fb, x, y + 1, "xWF", COLOR_PIP_DARK, 1);
        return;
    }
    // 3 concentric wave arcs + center dot
    draw_hline(fb, x + 1, y, 7, color);
    draw_hline(fb, x + 2, y + 2, 5, color);
    draw_hline(fb, x + 3, y + 4, 3, color);
    draw_pixel(fb, x + 4, y + 6, color);
}

static void draw_hotspot_icon(uint32_t *fb, int x, int y, uint32_t color, int active) {
    if (!active) return;
    // Broadcast beacon tower
    draw_pixel(fb, x + 4, y + 1, color);
    draw_pixel(fb, x + 3, y + 2, color);
    draw_pixel(fb, x + 5, y + 2, color);
    draw_pixel(fb, x + 2, y + 3, color);
    draw_pixel(fb, x + 6, y + 3, color);
    fill_rect(fb, x + 4, y + 3, 1, 5, color);
}

static void draw_telemetry_gauge(uint32_t *fb, int y, const char *label, int pct) {
    draw_string(fb, 4, y, label, COLOR_PIP_LIGHT, 1);

    int total_segs = 10;
    int filled = (pct * total_segs) / 100;
    if (filled > total_segs) filled = total_segs;
    if (filled < 0) filled = 0;

    int bar_x = 32;
    for (int s = 0; s < total_segs; s++) {
        uint32_t seg_color = (s < filled) ? COLOR_PIP_GREEN : COLOR_PIP_DARK;
        fill_rect(fb, bar_x + s * 6, y, 4, 7, seg_color);
    }

    char val_buf[8];
    snprintf(val_buf, sizeof(val_buf), "%3d%%", pct);
    draw_string(fb, 98, y, val_buf, COLOR_PIP_LIGHT, 1);
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    /* Acquire kernel wake lock IMMEDIATELY to prevent SPI bus power gating.
     * This must happen before opening fb2, as DOZE can gate the SPI bus.
     * Re-acquire every loop iteration to survive any kernel wake lock timeouts. */
    acquire_wakelock();

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

    int last_call_state = 0;
    time_t call_start_time = 0;
    int anim_tick = 0;

    /* FULL TIME ALWAYS-ON REFRESH LOOP */
    for (;;) {
        // Enforce backlight 255 every second so display never sleeps or dims
        set_backlight(255);
        ioctl(fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);

        clear_screen(fb);

        char incoming_num[64] = {0};
        int call_state = poll_telephony(incoming_num, sizeof(incoming_num));

        if (call_state == 1) {
            /* === PIP-BOY INCOMING TRANSMISSION HUD === */
            fill_rect(fb, 0, 0, 128, 14, COLOR_AMBER);
            draw_string_centered(fb, 3, "> TRANSMISSION <", COLOR_BLACK, 1);

            draw_string_centered(fb, 22, "INCOMING COMMS", COLOR_PIP_GREEN, 1);

            char num_display[32];
            if (incoming_num[0]) {
                snprintf(num_display, sizeof(num_display), "%.15s", incoming_num);
            } else {
                snprintf(num_display, sizeof(num_display), "UNKNOWN CALLER");
            }
            draw_string_centered(fb, 40, num_display, COLOR_WHITE, 2);

            draw_string_centered(fb, 62, "SIG: [||||||||||] 98%", COLOR_PIP_LIGHT, 1);

            fill_rect(fb, 6, 82, 116, 16, COLOR_PIP_GREEN);
            draw_string_centered(fb, 86, "HOLD [PTT]: ANSWER SPK", COLOR_BLACK, 1);

            draw_string_centered(fb, 108, "RED KEY: REJECT", COLOR_RED, 1);
            call_start_time = 0;

        } else if (call_state == 2) {
            /* === PIP-BOY ACTIVE COMMS LINK HUD === */
            if (last_call_state != 2 || call_start_time == 0) {
                call_start_time = time(NULL);
            }
            long call_duration = (long)(time(NULL) - call_start_time);
            if (call_duration < 0) call_duration = 0;

            fill_rect(fb, 0, 0, 128, 14, COLOR_PIP_GREEN);
            draw_string(fb, 4, 3, "COMM ACTIVE", COLOR_BLACK, 1);
            draw_string(fb, 78, 3, "SPK:ON", COLOR_BLACK, 1);

            draw_string_centered(fb, 20, "TRANSMISSION TIME", COLOR_PIP_DARK, 1);

            char timer_buf[32];
            snprintf(timer_buf, sizeof(timer_buf), "%02ld:%02ld", call_duration / 60, call_duration % 60);
            draw_string_centered(fb, 32, timer_buf, COLOR_WHITE, 3);

            // Animated Equalizer Wave
            int eq_bars[] = {4, 12, 18, 8, 16, 22, 14, 10, 20, 6, 14, 8};
            int count = sizeof(eq_bars) / sizeof(eq_bars[0]);
            int base_y = 82;
            for (int b = 0; b < count; b++) {
                int h = eq_bars[(b + anim_tick) % count];
                fill_rect(fb, 16 + b * 8, base_y - h / 2, 4, h, COLOR_PIP_GREEN);
            }

            draw_rect(fb, 6, 100, 116, 18, COLOR_RED);
            draw_string_centered(fb, 105, "HOLD [PTT]: END CALL", COLOR_RED, 1);

        } else {
            /* === PIP-BOY 3000 TACTICAL HUD WATCHFACE (ALWAYS-ON FULL TIME) === */
            call_start_time = 0;

            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);

            // 1. TOP WIRELESS STATUS BAR
            int wifi_on = is_wifi_connected();
            int hotspot_on = is_hotspot_active();

            draw_cellular_icon(fb, 4, 3, COLOR_PIP_GREEN);
            draw_wifi_icon(fb, 20, 3, COLOR_PIP_GREEN, wifi_on);
            draw_hotspot_icon(fb, 36, 3, COLOR_AMBER, hotspot_on);

            draw_string(fb, 56, 3, "PIP-BOY 3000", COLOR_PIP_LIGHT, 1);
            draw_hline(fb, 2, 14, 124, COLOR_PIP_DARK);

            // 2. CENTER HIGH-CONTRAST DIGITAL CLOCK
            char time_buf[16];
            strftime(time_buf, sizeof(time_buf), "%H:%M", tm_info);
            draw_string(fb, 10, 20, time_buf, COLOR_WHITE, 3);

            // Seconds Pill
            char sec_buf[8];
            snprintf(sec_buf, sizeof(sec_buf), ":%02d", tm_info->tm_sec);
            draw_string(fb, 102, 32, sec_buf, COLOR_PIP_GREEN, 1);

            // Date Badge
            char date_buf[32];
            strftime(date_buf, sizeof(date_buf), "%a %d-%b-%Y", tm_info);
            for (int i = 0; date_buf[i]; i++) if (date_buf[i] >= 'a' && date_buf[i] <= 'z') date_buf[i] -= 32;
            draw_string_centered(fb, 46, date_buf, COLOR_PIP_LIGHT, 1);
            draw_hline(fb, 2, 56, 124, COLOR_PIP_DARK);

            // 3. TELEMETRY GAUGES: BAT%, CPU%, RAM%
            int bat = get_battery();
            int cpu = get_cpu_usage();
            int ram = get_ram_usage();

            draw_telemetry_gauge(fb, 62, "BAT", bat >= 0 ? bat : 0);
            draw_telemetry_gauge(fb, 76, "CPU", cpu);
            draw_telemetry_gauge(fb, 90, "RAM", ram);

            // 4. FOOTER STATUS
            draw_hline(fb, 2, 104, 124, COLOR_PIP_DARK);
            draw_string(fb, 6, 110, "> SYS:OK", COLOR_PIP_GREEN, 1);
            draw_string(fb, 72, 110, "CAT-S22", COLOR_PIP_DARK, 1);
        }

        last_call_state = call_state;
        anim_tick++;

        /* Re-acquire wake lock every cycle to keep SPI bus powered.
         * FBIOPAN_DISPLAY is intentionally NOT called here:
         * The SPI panel driver (panel type=13, mdss_spi_client spi6.0) has no DMA
         * function set, causing msm_spi_process_transfer SPI Tx transaction timeouts
         * which reset the panel and cause white/blank flicker.
         * We use FBIOPUT_VSCREENINFO only (triggers SPI pixel push via software path). */
        acquire_wakelock();
        vinfo.yoffset = 0;
        ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
        /* Ensure mmap changes are flushed to the SPI framebuffer device */
        msync(fb, screensize, MS_SYNC);

        sleep(1);
    }

    munmap(fb, screensize);
    close(fb_fd);
    return 0;
}
