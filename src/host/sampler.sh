# SPDX-License-Identifier: GPL-3.0-or-later
# QindaLlamaWatch host sampler. Fed to `sh -s -- <interval>` on the server host
# (over ssh, or locally) and left running; it prints one framed sample per
# interval and never writes anything on the host.
#
# AGENT-CONTRACT: the frame format below is parsed by HostSampleParser
# (src/host/hostsampleparser.cpp). Lines beginning "@@" are markers; a
# "@@<section>" line starts a raw section that runs to the next marker.
# Scalar readings are "=key value" lines. Changing a marker or key here means
# changing the parser and tests/tst_hostsampleparser.cpp in the same commit.
#
# AGENT-GUARD: keep the per-sample cost tiny. The host being watched is busy
# serving a model; this loop uses shell builtins (`read`) for single-value
# files and forks only cat/pidof/sleep. Do not add find, ps, awk or python to
# the loop body.

iv=${1:-1}
# Files vanish (a server restarting) and permissions differ between hosts;
# a missing reading is simply absent from the frame, never an error line.
exec 2>/dev/null

one() { v=; read -r v < "$2"; echo "=$1 $v"; }

gpu=
for d in /sys/class/drm/card[0-9]/device /sys/class/drm/card[0-9][0-9]/device; do
    if [ -r "$d/mem_info_vram_total" ]; then gpu=$d; break; fi
done
ghw=
if [ -n "$gpu" ]; then
    for h in "$gpu"/hwmon/hwmon*; do
        if [ -d "$h" ]; then ghw=$h; break; fi
    done
fi
ctemp=
for h in /sys/class/hwmon/hwmon*; do
    n=; read -r n < "$h/name"
    case $n in k10temp|zenpower|coretemp) ctemp=$h/temp1_input; break ;; esac
done
nif=
if [ -d /sys/class/net/tailscale0 ]; then
    nif=tailscale0
else
    # The default-route interface, read with builtins from the route table.
    while read -r ifc dst rest; do
        if [ "$dst" = "00000000" ]; then nif=$ifc; break; fi
    done < /proc/net/route
fi
nv=
if [ -z "$gpu" ] && command -v nvidia-smi >/dev/null; then nv=1; fi

host=; read -r host < /proc/sys/kernel/hostname
echo "@@hello"
echo "=version 1"
echo "=host $host"
echo "=netif $nif"
echo "=clk_tck $(getconf CLK_TCK)"
if [ -n "$gpu" ]; then one gpu_name "$gpu/product_name"; fi
if [ -n "$nv" ]; then echo "=gpu_name nvidia"; fi

while :; do
    echo "@@begin"
    echo "@@stat"
    while read -r line; do
        case $line in cpu*) echo "$line" ;; *) break ;; esac
    done < /proc/stat
    echo "@@meminfo"
    cat /proc/meminfo
    echo "@@scalars"
    one loadavg /proc/loadavg
    one uptime /proc/uptime
    if [ -n "$ctemp" ]; then one cpu_temp "$ctemp"; fi
    echo "@@freq"
    cat /sys/devices/system/cpu/cpu[0-9]*/cpufreq/scaling_cur_freq
    echo "@@scalars"
    if [ -n "$nif" ]; then
        one net_rx "/sys/class/net/$nif/statistics/rx_bytes"
        one net_tx "/sys/class/net/$nif/statistics/tx_bytes"
    fi
    if [ -n "$gpu" ]; then
        one gpu_busy "$gpu/gpu_busy_percent"
        one vram_used "$gpu/mem_info_vram_used"
        one vram_total "$gpu/mem_info_vram_total"
        one gtt_used "$gpu/mem_info_gtt_used"
        if [ -n "$ghw" ]; then
            one gpu_temp "$ghw/temp1_input"
            one gpu_hot "$ghw/temp2_input"
            if [ -r "$ghw/power1_average" ]; then
                one gpu_power "$ghw/power1_average"
            else
                one gpu_power "$ghw/power1_input"
            fi
            one gpu_fan "$ghw/fan1_input"
        fi
        echo "@@sclk"
        cat "$gpu/pp_dpm_sclk"
        echo "@@mclk"
        cat "$gpu/pp_dpm_mclk"
    fi
    if [ -n "$nv" ]; then
        echo "@@nvidia"
        nvidia-smi --query-gpu=utilization.gpu,memory.used,memory.total,temperature.gpu,power.draw,fan.speed,clocks.sm,clocks.mem,name \
            --format=csv,noheader,nounits
    fi
    for p in $(pidof llama-server || pgrep -x llama-server); do
        echo "@@proc $p"
        while read -r k v rest; do
            case $k in VmRSS:) echo "=rss_kib $v" ;; esac
        done < "/proc/$p/status"
        one pstat "/proc/$p/stat"
        # DRM fdinfo is readable only as the server's user or root; without
        # it the monitor falls back to device-wide VRAM figures.
        for fd in /proc/$p/fdinfo/*; do
            cid=; vram=; gtt=
            while read -r k v rest; do
                case $k in
                    drm-client-id:) cid=$v ;;
                    drm-memory-vram:) vram=$v ;;
                    drm-memory-gtt:) gtt=$v ;;
                esac
            done < "$fd"
            if [ -n "$cid" ]; then echo "=drm $cid ${vram:-0} ${gtt:-0}"; fi
        done
    done
    echo "@@end"
    sleep "$iv" || exit 0
done
