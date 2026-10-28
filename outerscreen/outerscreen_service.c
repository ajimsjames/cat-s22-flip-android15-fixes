/*
 * Cat S22 Flip Outer Display Daemon
 * - Dedicated standalone init daemon running under root.
 * - Unblanks /dev/graphics/fb2
 * - Forces lcd-backlight2 to 255
 * - Sends frames to SPI controller via FBIOPUT_VSCREENINFO and FBIOPAN_DISPLAY
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
#include <errno.h>
#include <sys/stat.h>

#define FB_DEV          "/dev/graphics/fb2"
#define BACKLIGHT_NODE  "/sys/devices/platform/soc/1a00000.qcom,mdss_mdp/1a00000.qcom,mdss_mdp:qcom,mdss_fb_secondary/leds/lcd-backlight2/brightness"
#define BACKLIGHT_ALT   "/sys/class/leds/lcd-backlight2/brightness"
#define LOG_FILE        "/data/local/tmp/outerscreen.log"

static void log_msg(const char *msg) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (fp) {
        chmod(LOG_FILE, 0666);
        fprintf(fp, "%s\n", msg);
        fclose(fp);
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

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    log_msg("--- outerscreen_service started ---");

    int fb_fd = -1;
    while (fb_fd < 0) {
        fb_fd = open(FB_DEV, O_RDWR);
        if (fb_fd < 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Failed to open fb2: errno=%d (%s)", errno, strerror(errno));
            log_msg(buf);
            sleep(1);
        }
    }
    log_msg("Opened fb2 successfully");

    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "FBIOGET_VSCREENINFO failed: errno=%d", errno);
        log_msg(buf);
        close(fb_fd);
        return 1;
    }

    size_t screensize = 128 * 128 * 4;
    uint32_t *fb = (uint32_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb == MAP_FAILED) {
        char buf[128];
        snprintf(buf, sizeof(buf), "mmap failed: errno=%d", errno);
        log_msg(buf);
        close(fb_fd);
        return 1;
    }
    log_msg("mmap successful");

    int cycle = 0;
    for (;;) {
        // Enforce unblank and backlight 255 every cycle
        ioctl(fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);
        set_backlight(255);

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);

        // Fill background: Pip-Boy dark green (0xFF002200)
        for (int i = 0; i < 128 * 128; i++) {
            fb[i] = 0xFF002200;
        }

        // Draw border
        for (int x = 4; x < 124; x++) {
            fb[4 * 128 + x] = 0xFF10B981;
            fb[123 * 128 + x] = 0xFF10B981;
        }
        for (int y = 4; y < 124; y++) {
            fb[y * 128 + 4] = 0xFF10B981;
            fb[y * 128 + 123] = 0xFF10B981;
        }

        // Center animated block
        uint32_t center_color = (cycle % 2 == 0) ? 0xFFFFFFFF : 0xFF10B981;
        for (int cy = 44; cy < 84; cy++) {
            for (int cx = 44; cx < 84; cx++) {
                fb[cy * 128 + cx] = center_color;
            }
        }

        msync(fb, screensize, MS_SYNC);

        vinfo.yoffset = 0;
        ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
        ioctl(fb_fd, FBIOPAN_DISPLAY, &vinfo);

        if (cycle % 30 == 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Cycle %d alive (time %02d:%02d:%02d)", 
                     cycle, tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
            log_msg(buf);
        }

        cycle++;
        sleep(1);
    }

    munmap(fb, screensize);
    close(fb_fd);
    return 0;
}
