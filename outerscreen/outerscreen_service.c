#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#define FB_DEV "/dev/graphics/fb2"
#define BL_DEV "/sys/class/leds/lcd-backlight2/brightness"
#define HALL_DEV "/dev/input/event3"
#define SHM_PATH "/dev/shm_outerscreen"
#define CMD_PATH "/dev/shm_outerscreen_cmd"

static int current_max_brightness = 255;
static int force_screen_on = 1;

static void set_backlight(int val) {
    int fd = open(BL_DEV, O_WRONLY);
    if (fd >= 0) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d\n", val);
        write(fd, buf, len);
        close(fd);
    }
}

// 5x7 Font for Digits and colon
static const uint8_t font_digits[11][7] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x00}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00, 0x00, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46, 0x00, 0x00}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31, 0x00, 0x00}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10, 0x00, 0x00}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39, 0x00, 0x00}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00, 0x00}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03, 0x00, 0x00}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E, 0x00, 0x00}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00}  // : (index 10)
};

static void draw_digit(uint32_t *fb, int start_x, int start_y, int digit_idx, int scale, uint32_t color) {
    if (digit_idx < 0 || digit_idx > 10) return;
    for (int col = 0; col < 5; col++) {
        uint8_t line = font_digits[digit_idx][col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                for (int dy = 0; dy < scale; dy++) {
                    for (int dx = 0; dx < scale; dx++) {
                        int px = start_x + col * scale + dx;
                        int py = start_y + row * scale + dy;
                        if (px >= 0 && px < 128 && py >= 0 && py < 128) {
                            fb[px + py * 128] = color;
                        }
                    }
                }
            }
        }
    }
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

    int hall_fd = open(HALL_DEV, O_RDONLY | O_NONBLOCK);
    int is_flip_closed = 1;
    if (hall_fd >= 0) {
        unsigned long swbit = 0;
        if (ioctl(hall_fd, EVIOCGSW(sizeof(swbit)), &swbit) >= 0) {
            is_flip_closed = (swbit & 1) ? 1 : 0;
        }
    }

    int shm_fd = open(SHM_PATH, O_CREAT | O_RDWR, 0666);
    if (shm_fd >= 0) {
        ftruncate(shm_fd, screensize + 4);
        chmod(SHM_PATH, 0666);
    }
    uint32_t *shm_buf = NULL;
    if (shm_fd >= 0) {
        shm_buf = (uint32_t *)mmap(0, screensize + 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    }

    int cmd_fd = open(CMD_PATH, O_CREAT | O_RDWR, 0666);
    if (cmd_fd >= 0) {
        ftruncate(cmd_fd, 64);
        chmod(CMD_PATH, 0666);
    }

    time_t rawtime;
    struct tm *info;

    for (;;) {
        // Poll hall switch
        if (hall_fd >= 0) {
            struct input_event ev;
            while (read(hall_fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_SW && ev.code == 0) {
                    is_flip_closed = ev.value;
                }
            }
        }

        // Poll brightness command from CatSystemManager
        if (cmd_fd >= 0) {
            char cmd_buf[64] = {0};
            lseek(cmd_fd, 0, SEEK_SET);
            int nr = read(cmd_fd, cmd_buf, sizeof(cmd_buf) - 1);
            if (nr > 0) {
                if (strncmp(cmd_buf, "BRIGHTNESS=", 11) == 0) {
                    int b = atoi(cmd_buf + 11);
                    if (b >= 10 && b <= 255) {
                        current_max_brightness = b;
                    }
                    lseek(cmd_fd, 0, SEEK_SET);
                    write(cmd_fd, "DONE\n\0\0\0\0", 8);
                } else if (strncmp(cmd_buf, "FORCE_ON", 8) == 0) {
                    force_screen_on = 1;
                    lseek(cmd_fd, 0, SEEK_SET);
                    write(cmd_fd, "DONE\n\0\0\0\0", 8);
                } else if (strncmp(cmd_buf, "FORCE_OFF", 9) == 0) {
                    force_screen_on = 0;
                    lseek(cmd_fd, 0, SEEK_SET);
                    write(cmd_fd, "DONE\n\0\0\0\0", 8);
                } else if (strncmp(cmd_buf, "TOGGLE", 6) == 0) {
                    force_screen_on = !force_screen_on;
                    lseek(cmd_fd, 0, SEEK_SET);
                    write(cmd_fd, "DONE\n\0\0\0\0", 8);
                }
            }
        }

        if (is_flip_closed || force_screen_on) {
            set_backlight(current_max_brightness);

            // If CatSystemManager sent a live frame, blit it; otherwise draw standalone clock
            if (shm_buf != NULL && shm_buf[0] == 0xCA705C22) {
                memcpy(fb, &shm_buf[1], (128 * 128) * sizeof(uint32_t));
            } else {
                time(&rawtime);
                info = localtime(&rawtime);

                for (int i = 0; i < 128 * 128; i++) {
                    fb[i] = 0xFF121212;
                }

                int h1 = info->tm_hour / 10;
                int h2 = info->tm_hour % 10;
                int m1 = info->tm_min / 10;
                int m2 = info->tm_min % 10;

                int scale = 3;
                int y_pos = 45;
                int x_base = 15;

                draw_digit(fb, x_base, y_pos, h1, scale, 0xFFFFFFFF);
                draw_digit(fb, x_base + 18, y_pos, h2, scale, 0xFFFFFFFF);
                draw_digit(fb, x_base + 36, y_pos, 10, scale, 0xFFFFCC00);
                draw_digit(fb, x_base + 50, y_pos, m1, scale, 0xFFFFFFFF);
                draw_digit(fb, x_base + 68, y_pos, m2, scale, 0xFFFFFFFF);
            }

            // Continuous hardware SPI push
            ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
            ioctl(fb_fd, FBIOPAN_DISPLAY, &vinfo);
        } else {
            set_backlight(0);
        }

        sleep(1);
    }

    if (shm_buf && shm_buf != MAP_FAILED) munmap(shm_buf, screensize + 4);
    if (shm_fd >= 0) close(shm_fd);
    if (cmd_fd >= 0) close(cmd_fd);
    munmap(fb, screensize);
    close(fb_fd);
    if (hall_fd >= 0) close(hall_fd);
    return 0;
}
