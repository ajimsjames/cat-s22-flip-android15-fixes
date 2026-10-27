#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

#define FB_DEV "/dev/graphics/fb2"
#define BL_DEV "/sys/class/leds/lcd-backlight2/brightness"

static volatile int g_is_flip_closed = 0;

static void set_backlight(int val) {
    int fd = open(BL_DEV, O_WRONLY);
    if (fd >= 0) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d\n", val);
        write(fd, buf, len);
        close(fd);
    }
}

// -------------------------------------------------------------
// Retro Cyber Phosphor Palette
// -------------------------------------------------------------
#define COL_BG         0xFF000000 // Pure OLED Black
#define COL_GREEN      0xFF00D95A // Vivid Phosphor Green
#define COL_DIM_GREEN  0xFF008F3B // Medium-contrast Legible Green
#define COL_DARK_GREEN 0xFF002B11 // Dark Bar Track
#define COL_CYAN       0xFF00C8E0 // Crisp Cyber Cyan
#define COL_AMBER      0xFFD4A500 // Crisp Amber
#define COL_RED        0xFFD42B2B // Alert Red
#define COL_BORDER     0xFF00421A // Subtle Terminal Border

// -------------------------------------------------------------
// Embedded 5x7 Monospace Font
// -------------------------------------------------------------
static const uint8_t font5x7[128][5] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x00, 0x00, 0x5F, 0x00, 0x00},
    ['"'] = {0x00, 0x07, 0x00, 0x07, 0x00},
    ['#'] = {0x14, 0x7F, 0x14, 0x7F, 0x14},
    ['%'] = {0x23, 0x13, 0x08, 0x64, 0x62},
    ['+'] = {0x08, 0x08, 0x3E, 0x08, 0x08},
    ['-'] = {0x08, 0x08, 0x08, 0x08, 0x08},
    ['.'] = {0x00, 0x60, 0x60, 0x00, 0x00},
    ['/'] = {0x20, 0x10, 0x08, 0x04, 0x02},
    [':'] = {0x00, 0x36, 0x36, 0x00, 0x00},
    ['<'] = {0x08, 0x14, 0x22, 0x41, 0x00},
    ['='] = {0x14, 0x14, 0x14, 0x14, 0x14},
    ['>'] = {0x00, 0x41, 0x22, 0x14, 0x08},
    ['['] = {0x00, 0x7F, 0x41, 0x41, 0x00},
    [']'] = {0x00, 0x41, 0x41, 0x7F, 0x00},
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
    ['a'] = {0x20, 0x54, 0x54, 0x54, 0x78},
    ['b'] = {0x7F, 0x48, 0x44, 0x44, 0x38},
    ['c'] = {0x38, 0x44, 0x44, 0x44, 0x20},
    ['d'] = {0x38, 0x44, 0x44, 0x48, 0x7F},
    ['e'] = {0x38, 0x54, 0x54, 0x54, 0x18},
    ['h'] = {0x7F, 0x08, 0x04, 0x04, 0x78},
    ['m'] = {0x7C, 0x04, 0x18, 0x04, 0x78},
    ['s'] = {0x48, 0x54, 0x54, 0x54, 0x20},
};

static void draw_char(uint32_t *fb, int x, int y, char c, int scale, uint32_t color) {
    if ((unsigned char)c >= 128) return;
    const uint8_t *glyph = font5x7[(unsigned char)c];
    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                for (int dy = 0; dy < scale; dy++) {
                    for (int dx = 0; dx < scale; dx++) {
                        int px = x + col * scale + dx;
                        int py = y + row * scale + dy;
                        if (px >= 0 && px < 128 && py >= 0 && py < 128) {
                            fb[px + py * 128] = color;
                        }
                    }
                }
            }
        }
    }
}

static void draw_text(uint32_t *fb, int x, int y, const char *str, int scale, uint32_t color) {
    int cur_x = x;
    while (*str) {
        draw_char(fb, cur_x, y, *str, scale, color);
        cur_x += 6 * scale;
        str++;
    }
}

static void draw_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= 128) continue;
        for (int i = x; i < x + w; i++) {
            if (i >= 0 && i < 128) {
                fb[i + j * 128] = color;
            }
        }
    }
}

static void draw_line_h(uint32_t *fb, int x1, int x2, int y, uint32_t color) {
    if (y < 0 || y >= 128) return;
    for (int x = x1; x <= x2; x++) {
        if (x >= 0 && x < 128) fb[x + y * 128] = color;
    }
}

static void draw_segmented_bar(uint32_t *fb, int x, int y, int width, int height, float fraction, uint32_t active_color) {
    int total_segments = 15;
    float gap = 1.5f;
    float seg_w = ((float)width - (gap * (total_segments - 1))) / (float)total_segments;
    int active_segs = (int)(fraction * total_segments);
    if (active_segs < 0) active_segs = 0;
    if (active_segs > total_segments) active_segs = total_segments;

    for (int i = 0; i < total_segments; i++) {
        int seg_x = x + (int)(i * (seg_w + gap));
        uint32_t col = (i < active_segs) ? active_color : COL_DARK_GREEN;
        draw_rect(fb, seg_x, y, (int)seg_w, height, col);
    }
}

// Read battery stats
static void read_battery(int *capacity, int *voltage_mv, int *current_ma, int *is_charging, float *temp_c) {
    *capacity = 75; *voltage_mv = 3900; *current_ma = 0; *is_charging = 0; *temp_c = 28.0f;

    FILE *f = fopen("/sys/class/power_supply/battery/capacity", "r");
    if (!f) f = fopen("/sys/class/power_supply/bms/capacity", "r");
    if (f) { fscanf(f, "%d", capacity); fclose(f); }

    f = fopen("/sys/class/power_supply/battery/voltage_now", "r");
    if (f) { int v = 0; fscanf(f, "%d", &v); *voltage_mv = v > 100000 ? v / 1000 : v; fclose(f); }

    f = fopen("/sys/class/power_supply/battery/current_now", "r");
    if (f) { int c = 0; fscanf(f, "%d", &c); *current_ma = abs(c) > 1000 ? abs(c) / 1000 : abs(c); fclose(f); }

    f = fopen("/sys/class/power_supply/battery/status", "r");
    if (f) {
        char status[32] = {0};
        fscanf(f, "%31s", status);
        if (strstr(status, "Charging")) *is_charging = 1;
        fclose(f);
    }

    f = fopen("/sys/class/power_supply/battery/temp", "r");
    if (f) { int t = 0; fscanf(f, "%d", &t); *temp_c = t > 100 ? (float)t / 10.0f : (float)t; fclose(f); }
}

// Read RAM stats
static void read_ram(int *used_mb, int *total_mb, int *used_pct) {
    *used_mb = 1100; *total_mb = 1890; *used_pct = 58;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;
    char line[128];
    long total = 0, avail = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line + 9, "%ld", &total);
        else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%ld", &avail);
    }
    fclose(f);
    if (total > 0) {
        *total_mb = (int)(total / 1024);
        int avail_mb = (int)(avail / 1024);
        *used_mb = *total_mb - avail_mb;
        *used_pct = (*used_mb * 100) / *total_mb;
    }
}

// Read CPU load and uptime
static void read_system_telemetry(char *load_str, int *up_h, int *up_m) {
    strcpy(load_str, "0.50");
    *up_h = 0; *up_m = 0;
    FILE *f = fopen("/proc/loadavg", "r");
    if (f) {
        fscanf(f, "%4s", load_str);
        fclose(f);
    }
    struct timespec ts;
    if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
        *up_h = (int)(ts.tv_sec / 3600);
        *up_m = (int)((ts.tv_sec % 3600) / 60);
    }
}

// Non-blocking asynchronous event sender
static void send_key_async(const char *keycmd) {
    if (fork() == 0) {
        execl("/system/bin/input", "input", "keyevent", keycmd, (char *)NULL);
        _exit(1);
    }
}

// Real-time kernel hall event monitor thread
static void *hall_monitor_thread(void *arg) {
    (void)arg;
    for (;;) {
        FILE *fp = popen("logcat -T 1 -v brief", "r");
        if (!fp) {
            sleep(1);
            continue;
        }
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            if (strstr(buf, "hall near")) {
                if (!g_is_flip_closed) {
                    g_is_flip_closed = 1;
                    send_key_async("KEYCODE_SLEEP");
                }
            } else if (strstr(buf, "hall far")) {
                if (g_is_flip_closed) {
                    g_is_flip_closed = 0;
                    send_key_async("KEYCODE_WAKEUP");
                }
            }
        }
        pclose(fp);
        sleep(1);
    }
    return NULL;
}

int main(void) {
    int fb_fd = open(FB_DEV, O_RDWR);
    if (fb_fd < 0) {
        perror("open " FB_DEV);
        return 1;
    }

    struct fb_var_screeninfo vinfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    long screensize = 128 * 128 * 4;
    uint32_t *fb = (uint32_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb == MAP_FAILED) {
        perror("mmap");
        close(fb_fd);
        return 1;
    }
    vinfo.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;

    // Start background kernel hall monitor thread
    pthread_t tid;
    pthread_create(&tid, NULL, hall_monitor_thread, NULL);

    time_t rawtime;
    struct tm *info;
    const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    const char *months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

    int prev_closed = -1;

    for (;;) {
        if (g_is_flip_closed) {
            if (prev_closed != 1) {
                set_backlight(255);
                prev_closed = 1;
            }

            // 1. Clear background
            for (int i = 0; i < 128 * 128; i++) fb[i] = COL_BG;

            // Outer border
            draw_line_h(fb, 0, 127, 0, COL_BORDER);
            draw_line_h(fb, 0, 127, 127, COL_BORDER);

            // 2. Header
            draw_text(fb, 4, 3, "[ CAT OS / TELEMETRY ]", 1, COL_DIM_GREEN);
            draw_line_h(fb, 1, 126, 12, COL_BORDER);

            // 3. Digital Clock (HH:MM:SS) & Date
            time(&rawtime);
            info = localtime(&rawtime);

            char time_str[16];
            snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", info->tm_hour, info->tm_min, info->tm_sec);
            draw_text(fb, 14, 17, time_str, 2, COL_GREEN);

            char date_str[24];
            snprintf(date_str, sizeof(date_str), "%s %02d %s", days[info->tm_wday], info->tm_mday, months[info->tm_mon]);
            draw_text(fb, 26, 35, date_str, 1, COL_DIM_GREEN);
            draw_line_h(fb, 1, 126, 45, COL_BORDER);

            // 4. Battery Row + Segmented Bar
            int bat_cap, bat_volt, bat_ma, is_charging;
            float bat_temp;
            read_battery(&bat_cap, &bat_volt, &bat_ma, &is_charging, &bat_temp);

            char bat_label[24];
            snprintf(bat_label, sizeof(bat_label), "BAT %d%%", bat_cap);
            uint32_t bat_col = (bat_cap <= 20) ? COL_RED : COL_GREEN;
            draw_text(fb, 4, 49, bat_label, 1, bat_col);

            char bat_rate[24];
            snprintf(bat_rate, sizeof(bat_rate), "%c%dmA", is_charging ? '+' : '-', bat_ma);
            draw_text(fb, 76, 49, bat_rate, 1, is_charging ? COL_CYAN : COL_DIM_GREEN);

            draw_segmented_bar(fb, 4, 58, 120, 4, (float)bat_cap / 100.0f, bat_col);

            // 5. RAM Row + Segmented Bar
            int ram_used, ram_total, ram_pct;
            read_ram(&ram_used, &ram_total, &ram_pct);

            char ram_label[24];
            snprintf(ram_label, sizeof(ram_label), "RAM %d%%", ram_pct);
            draw_text(fb, 4, 66, ram_label, 1, COL_CYAN);

            char ram_mb[24];
            snprintf(ram_mb, sizeof(ram_mb), "%dM/%dM", ram_used, ram_total);
            draw_text(fb, 66, 66, ram_mb, 1, COL_DIM_GREEN);

            draw_segmented_bar(fb, 4, 75, 120, 4, (float)ram_pct / 100.0f, COL_CYAN);
            draw_line_h(fb, 1, 126, 83, COL_BORDER);

            // 6. Deep Hardware Telemetry
            char volt_temp_str[32];
            snprintf(volt_temp_str, sizeof(volt_temp_str), "V:%.2fV  T:%.0fC", (float)bat_volt / 1000.0f, bat_temp);
            draw_text(fb, 4, 87, volt_temp_str, 1, COL_GREEN);

            char load_str[8];
            int up_h, up_m;
            read_system_telemetry(load_str, &up_h, &up_m);

            char sys_str[32];
            snprintf(sys_str, sizeof(sys_str), "CPU:%s UP:%dh%02dm", load_str, up_h, up_m);
            draw_text(fb, 4, 98, sys_str, 1, COL_DIM_GREEN);
            draw_line_h(fb, 1, 126, 110, COL_BORDER);

            // 7. Footer Status
            const char *status_msg = is_charging ? "> STATUS: CHARGING [AC]" : "> STATUS: RUNNING NORMAL";
            draw_text(fb, 4, 115, status_msg, 1, is_charging ? COL_CYAN : COL_DIM_GREEN);

            // Push to SPI hardware
            ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
            ioctl(fb_fd, FBIOPAN_DISPLAY, &vinfo);

            usleep(500000); // 500ms snappy refresh
        } else {
            if (prev_closed != 0) {
                set_backlight(0);
                prev_closed = 0;
            }
            usleep(100000); // 100ms low CPU idle sleep
        }
    }

    munmap(fb, screensize);
    close(fb_fd);
    return 0;
}
