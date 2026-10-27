#define __NR_read          3
#define __NR_write         4
#define __NR_open          5
#define __NR_close         6
#define __NR_ioctl         54
#define __NR_nanosleep     162
#define __NR_clock_gettime 263
#define __NR_exit          1
#define __NR_fork          2
#define __NR_execve        11
#define __NR_wait4         114

#include <linux/input.h>
#include <linux/uinput.h>

static inline long sc1(long n, long a) {
    register long r0 __asm__("r0") = a;
    register long r7 __asm__("r7") = n;
    __asm__ __volatile__("svc #0" : "+r"(r0) : "r"(r7) : "memory");
    return r0;
}
static inline long sc2(long n, long a, long b) {
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r7 __asm__("r7") = n;
    __asm__ __volatile__("svc #0" : "+r"(r0) : "r"(r7), "r"(r1) : "memory");
    return r0;
}
static inline long sc3(long n, long a, long b, long c) {
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r7 __asm__("r7") = n;
    __asm__ __volatile__("svc #0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory");
    return r0;
}
static inline long sc4(long n, long a, long b, long c, long d) {
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r3 __asm__("r3") = d;
    register long r7 __asm__("r7") = n;
    __asm__ __volatile__("svc #0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2), "r"(r3) : "memory");
    return r0;
}

static long _open(const char *path, int flags) { return sc2(__NR_open, (long)path, flags); }
static long _read(int fd, void *buf, long len) { return sc3(__NR_read, fd, (long)buf, len); }
static long _write(int fd, const void *buf, long len) { return sc3(__NR_write, fd, (long)buf, len); }
static long _close(int fd) { return sc1(__NR_close, fd); }
static long _ioctl(int fd, unsigned long req, long arg) { return sc3(__NR_ioctl, fd, (long)req, arg); }
static void _exit(int code) { sc1(__NR_exit, code); __builtin_unreachable(); }

struct _timespec { long tv_sec; long tv_nsec; };
static long long mono_ns(void) {
    struct _timespec ts;
    sc2(__NR_clock_gettime, 1, (long)&ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void _memset(void *p, int v, long n) {
    unsigned char *b = p;
    while (n--) *b++ = (unsigned char)v;
}

#define INPUT_DEV       "/dev/input/event1"
#define UINPUT_DEV      "/dev/uinput"
#define DEBOUNCE_NS     120000000LL       /* 120 ms filter */
#define LONG_PRESS_NS   500000000LL       /* 500 ms for long-press Menu */
#define MAX_KEYS        600
#define O_RDONLY        0
#define O_WRONLY        1
#define O_NONBLOCK      2048

#define KEY_SPEAKER_RAW 248  /* KEY_MICMUTE */
#define KEY_CUSTOM_TAB  15   /* KEY_TAB */
#define KEY_CUSTOM_MENU 139  /* KEY_MENU */

static const int matrix_keys[] = {
    0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009,
    0x000a, 0x000b, 0x000e, 0x001c, 0x0067, 0x0069, 0x006a, 0x006c,
    0x009e, 0x00a9, 0x00ac, 0x00f8, 0x020a, 0x020b, 0x020c, 0x020d,
    0x0244,
    15,     /* KEY_TAB */
    139     /* KEY_MENU */
};
#define NUM_KEYS (sizeof(matrix_keys)/sizeof(matrix_keys[0]))

static void emit_key(int ui, int code, int val) {
    struct input_event ev;
    _memset(&ev, 0, sizeof(ev));
    ev.type = EV_KEY;
    ev.code = code;
    ev.value = val;
    _write(ui, &ev, sizeof(ev));

    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    _write(ui, &ev, sizeof(ev));
}

void _start(void) {
    int src = (int)_open(INPUT_DEV, O_RDONLY);
    if (src < 0) _exit(1);

    _ioctl(src, EVIOCGRAB, 1);

    int ui = (int)_open(UINPUT_DEV, O_WRONLY | O_NONBLOCK);
    if (ui < 0) _exit(2);

    _ioctl(ui, UI_SET_EVBIT, EV_KEY);
    _ioctl(ui, UI_SET_EVBIT, EV_SYN);
    _ioctl(ui, UI_SET_EVBIT, EV_MSC);
    _ioctl(ui, UI_SET_MSCBIT, MSC_SCAN);

    for (unsigned int i = 0; i < NUM_KEYS; i++) {
        _ioctl(ui, UI_SET_KEYBIT, matrix_keys[i]);
    }

    struct uinput_user_dev uid;
    _memset(&uid, 0, sizeof(uid));
    const char *nm = "Cat_Keypad_Clean";
    for (int i = 0; nm[i]; i++) uid.name[i] = nm[i];
    uid.id.bustype = 0x0003; /* BUS_USB */
    uid.id.vendor  = 0x18d1;
    uid.id.product = 0x0001;
    uid.id.version = 0x0000;
    _write(ui, &uid, sizeof(uid));
    _ioctl(ui, UI_DEV_CREATE, 0);

    struct _timespec slp = {0, 300000000};
    sc2(__NR_nanosleep, (long)&slp, 0);

    long long last_down[MAX_KEYS];
    long long last_up[MAX_KEYS];
    int       last_val[MAX_KEYS];
    _memset(last_down, 0, sizeof(last_down));
    _memset(last_up,   0, sizeof(last_up));
    for (int i = 0; i < MAX_KEYS; i++) last_val[i] = -1;

    long long spk_down_time = 0;
    int spk_is_down = 0;

    struct input_event ev;
    for (;;) {
        long n = _read(src, &ev, sizeof(ev));
        if (n < (long)sizeof(ev)) continue;

        if (ev.type != EV_KEY) {
            _write(ui, &ev, sizeof(ev));
            continue;
        }

        int code = ev.code;
        int val  = ev.value;

        if (code < 0 || code >= MAX_KEYS) {
            _write(ui, &ev, sizeof(ev));
            continue;
        }

        long long now = mono_ns();

        if (val == 1) {
            if (last_val[code] == 1 && (now - last_down[code]) < DEBOUNCE_NS) continue;
            if (last_val[code] == 0 && (now - last_up[code])   < DEBOUNCE_NS) continue;
            last_down[code] = now;
            last_val[code]  = 1;
        } else if (val == 0) {
            if (last_val[code] == 0 && (now - last_up[code]) < DEBOUNCE_NS) continue;
            if (last_val[code] != 1) continue;
            last_up[code]  = now;
            last_val[code] = 0;
        }

        /* Special handling for Speaker button */
        if (code == KEY_SPEAKER_RAW) {
            if (val == 1) {
                spk_down_time = now;
                spk_is_down = 1;
            } else if (val == 0) {
                if (spk_is_down) {
                    spk_is_down = 0;
                    long long duration = now - spk_down_time;
                    if (duration >= LONG_PRESS_NS) {
                        /* Long press -> MENU */
                        emit_key(ui, KEY_CUSTOM_MENU, 1);
                        emit_key(ui, KEY_CUSTOM_MENU, 0);
                    } else {
                        /* Short press -> TAB */
                        emit_key(ui, KEY_CUSTOM_TAB, 1);
                        emit_key(ui, KEY_CUSTOM_TAB, 0);
                    }
                }
            }
            continue;
        }

        _write(ui, &ev, sizeof(ev));
    }

    _ioctl(ui, UI_DEV_DESTROY, 0);
    _exit(0);
}
