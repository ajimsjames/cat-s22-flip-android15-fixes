#!/system/bin/sh

# 1. End Call Button Configuration:
# Enable Power Button Ends Call (incall_power_button_behavior = 2)
# The Red End Call button (☎ / ⏻) is wired to KEY_POWER on qpnp_pon.
settings put secure incall_power_button_behavior 2

# 2. CPU Schedutil Governor Tweaks (instantaneous clock ramp-up)
for gov in /sys/devices/system/cpu/cpufreq/policy*/schedutil /sys/devices/system/cpu/cpu*/cpufreq/schedutil; do
    if [ -d "$gov" ]; then
        echo 0 > "$gov/up_rate_limit_us" 2>/dev/null
        echo 10000 > "$gov/down_rate_limit_us" 2>/dev/null
        echo 1 > "$gov/iowait_boost_enable" 2>/dev/null
    fi
done

# 3. Linux Kernel Virtual Memory & Cache Tuning
echo 60 > /proc/sys/vm/swappiness 2>/dev/null
echo 50 > /proc/sys/vm/vfs_cache_pressure 2>/dev/null
echo 15 > /proc/sys/vm/dirty_ratio 2>/dev/null
echo 5 > /proc/sys/vm/dirty_background_ratio 2>/dev/null
echo 500 > /proc/sys/vm/dirty_expire_centisecs 2>/dev/null
echo 200 > /proc/sys/vm/dirty_writeback_centisecs 2>/dev/null

# 4. I/O Scheduler & Read-Ahead Optimization for eMMC
for queue in /sys/block/mmcblk*/queue; do
    if [ -d "$queue" ]; then
        echo 512 > "$queue/read_ahead_kb" 2>/dev/null
        echo 0 > "$queue/iostats" 2>/dev/null
        echo 0 > "$queue/add_random" 2>/dev/null
    fi
done
