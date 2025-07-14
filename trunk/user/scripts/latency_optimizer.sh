#!/bin/sh
#
# Low Latency Network Optimization Script for Padavan-NG
# This script applies various kernel and network optimizations for minimal latency
#

PROC_NET_CORE="/proc/sys/net/core"
PROC_NET_IPV4="/proc/sys/net/ipv4"
PROC_SYS_KERNEL="/proc/sys/kernel"
PROC_SYS_VM="/proc/sys/vm"

# Default values optimized for low latency
DEFAULT_NETDEV_BUDGET=16
DEFAULT_NETDEV_MAX_BACKLOG=2000
DEFAULT_RPS_SOCK_FLOW_ENTRIES=8192
DEFAULT_RME_DEFAULT_QDISC="fq_codel"

log_message() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | logger -t "latency_optimizer"
    echo "$1"
}

check_smp_support() {
    local cpu_count=$(grep -c "^processor" /proc/cpuinfo)
    if [ "$cpu_count" -gt 1 ]; then
        log_message "SMP system detected with $cpu_count CPUs"
        return 0
    else
        log_message "Single CPU system detected"
        return 1
    fi
}

optimize_network_core() {
    log_message "Optimizing network core parameters..."
    
    # Reduce NAPI budget for lower latency
    if [ -w "$PROC_NET_CORE/netdev_budget" ]; then
        echo $DEFAULT_NETDEV_BUDGET > $PROC_NET_CORE/netdev_budget
        log_message "Set netdev_budget to $DEFAULT_NETDEV_BUDGET"
    fi
    
    # Optimize backlog size
    if [ -w "$PROC_NET_CORE/netdev_max_backlog" ]; then
        echo $DEFAULT_NETDEV_MAX_BACKLOG > $PROC_NET_CORE/netdev_max_backlog
        log_message "Set netdev_max_backlog to $DEFAULT_NETDEV_MAX_BACKLOG"
    fi
    
    # Configure RPS for multi-CPU systems
    if check_smp_support; then
        if [ -w "$PROC_NET_CORE/rps_sock_flow_entries" ]; then
            echo $DEFAULT_RPS_SOCK_FLOW_ENTRIES > $PROC_NET_CORE/rps_sock_flow_entries
            log_message "Set rps_sock_flow_entries to $DEFAULT_RPS_SOCK_FLOW_ENTRIES"
        fi
    fi
    
    # Enable timestamping for latency measurement
    if [ -w "$PROC_NET_CORE/netdev_tstamp_prequeue" ]; then
        echo 0 > $PROC_NET_CORE/netdev_tstamp_prequeue
        log_message "Disabled netdev timestamp prequeue for lower latency"
    fi
}

optimize_tcp_stack() {
    log_message "Optimizing TCP stack for low latency..."
    
    # Disable Nagle's algorithm for lower latency
    if [ -w "$PROC_NET_IPV4/tcp_low_latency" ]; then
        echo 1 > $PROC_NET_IPV4/tcp_low_latency
        log_message "Enabled TCP low latency mode"
    fi
    
    # Reduce TIME_WAIT recycling for faster connection reuse
    if [ -w "$PROC_NET_IPV4/tcp_tw_reuse" ]; then
        echo 1 > $PROC_NET_IPV4/tcp_tw_reuse
        log_message "Enabled TCP TIME_WAIT reuse"
    fi
    
    # Optimize TCP buffer sizes
    if [ -w "$PROC_NET_IPV4/tcp_rmem" ]; then
        echo "4096 32768 262144" > $PROC_NET_IPV4/tcp_rmem
        log_message "Optimized TCP read buffer sizes"
    fi
    
    if [ -w "$PROC_NET_IPV4/tcp_wmem" ]; then
        echo "4096 32768 262144" > $PROC_NET_IPV4/tcp_wmem
        log_message "Optimized TCP write buffer sizes"
    fi
}

optimize_interrupt_handling() {
    log_message "Optimizing interrupt handling..."
    
    # Check if IRQ affinity module is loaded
    if [ -f "/proc/latency_irq_affinity" ]; then
        log_message "IRQ affinity optimizer is active"
    else
        log_message "Loading IRQ affinity optimization module..."
        modprobe latency_irq 2>/dev/null
    fi
    
    # Configure RPS for network interfaces
    if check_smp_support; then
        for iface in $(ls /sys/class/net/ | grep -E "eth|raeth"); do
            if [ -d "/sys/class/net/$iface/queues" ]; then
                for queue in /sys/class/net/$iface/queues/rx-*/rps_cpus; do
                    if [ -w "$queue" ]; then
                        # Distribute across all CPUs except CPU 0 (reserved for system)
                        echo "e" > "$queue"  # CPUs 1,2,3 in hex
                        log_message "Configured RPS for $iface queue $(basename $(dirname $queue))"
                    fi
                done
                
                # Configure XPS for TX queues
                for queue in /sys/class/net/$iface/queues/tx-*/xps_cpus; do
                    if [ -w "$queue" ]; then
                        echo "e" > "$queue"  # CPUs 1,2,3 in hex
                        log_message "Configured XPS for $iface queue $(basename $(dirname $queue))"
                    fi
                done
            fi
        done
    fi
}

optimize_qdisc() {
    log_message "Optimizing queueing disciplines..."
    
    # Apply CAKE or fq_codel to network interfaces
    for iface in $(ls /sys/class/net/ | grep -E "eth|raeth"); do
        if ip link show "$iface" >/dev/null 2>&1; then
            # Try CAKE first, fall back to fq_codel
            if tc qdisc replace dev "$iface" root cake bandwidth 1000Mbit 2>/dev/null; then
                log_message "Applied CAKE qdisc to $iface"
            elif tc qdisc replace dev "$iface" root fq_codel target 2ms interval 20ms 2>/dev/null; then
                log_message "Applied fq_codel qdisc to $iface with low-latency settings"
            else
                log_message "Warning: Could not apply optimized qdisc to $iface"
            fi
        fi
    done
}

optimize_cpu_frequency() {
    log_message "Optimizing CPU frequency for latency..."
    
    # Use enhanced cpufreq utility if available
    if [ -x "/usr/sbin/mt7621_cpufreq_enhanced" ]; then
        /usr/sbin/mt7621_cpufreq_enhanced -p latency
        log_message "Applied latency-optimized CPU frequency profile"
    elif [ -x "/usr/sbin/mt7621_cpufreq" ]; then
        /usr/sbin/mt7621_cpufreq 1200
        log_message "Set CPU frequency to 1200MHz for latency optimization"
    fi
    
    # Disable CPU frequency scaling if governor is available
    if [ -d "/sys/devices/system/cpu/cpu0/cpufreq" ]; then
        echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
        log_message "Set CPU governor to performance mode"
    fi
}

optimize_memory_management() {
    log_message "Optimizing memory management..."
    
    # Reduce swappiness for better responsiveness
    if [ -w "$PROC_SYS_VM/swappiness" ]; then
        echo 1 > $PROC_SYS_VM/swappiness
        log_message "Set swappiness to 1"
    fi
    
    # Optimize dirty page handling
    if [ -w "$PROC_SYS_VM/dirty_ratio" ]; then
        echo 5 > $PROC_SYS_VM/dirty_ratio
        log_message "Set dirty_ratio to 5%"
    fi
    
    if [ -w "$PROC_SYS_VM/dirty_background_ratio" ]; then
        echo 2 > $PROC_SYS_VM/dirty_background_ratio
        log_message "Set dirty_background_ratio to 2%"
    fi
    
    # Reduce kernel timer slack for better timing precision
    if [ -w "$PROC_SYS_KERNEL/timer_migration" ]; then
        echo 0 > $PROC_SYS_KERNEL/timer_migration
        log_message "Disabled timer migration"
    fi
}

show_status() {
    log_message "=== Low Latency Optimization Status ==="
    
    echo "Network Core Settings:"
    [ -r "$PROC_NET_CORE/netdev_budget" ] && echo "  netdev_budget: $(cat $PROC_NET_CORE/netdev_budget)"
    [ -r "$PROC_NET_CORE/netdev_max_backlog" ] && echo "  netdev_max_backlog: $(cat $PROC_NET_CORE/netdev_max_backlog)"
    
    echo "CPU Information:"
    echo "  CPU count: $(grep -c "^processor" /proc/cpuinfo)"
    if [ -r "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq" ]; then
        echo "  Current frequency: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq) kHz"
    fi
    
    echo "Network Interfaces:"
    for iface in $(ls /sys/class/net/ | grep -E "eth|raeth"); do
        if ip link show "$iface" >/dev/null 2>&1; then
            qdisc=$(tc qdisc show dev "$iface" | head -n1 | awk '{print $2}')
            echo "  $iface: qdisc=$qdisc"
        fi
    done
    
    if [ -r "/proc/latency_irq_affinity" ]; then
        echo "IRQ Affinity Status:"
        head -n 10 /proc/latency_irq_affinity | tail -n +6
    fi
}

apply_optimizations() {
    log_message "Starting low latency optimization..."
    
    optimize_network_core
    optimize_tcp_stack
    optimize_interrupt_handling
    optimize_qdisc
    optimize_cpu_frequency
    optimize_memory_management
    
    log_message "Low latency optimization completed successfully!"
}

case "$1" in
    start|optimize)
        apply_optimizations
        ;;
    status)
        show_status
        ;;
    stop)
        log_message "Restoring default settings (not implemented)"
        ;;
    *)
        echo "Usage: $0 {start|optimize|status|stop}"
        echo "  start/optimize - Apply low latency optimizations"
        echo "  status         - Show current optimization status"
        echo "  stop           - Restore default settings"
        exit 1
        ;;
esac

exit 0