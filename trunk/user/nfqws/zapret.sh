#!/bin/sh

NFQWS_BIN="/usr/bin/nfqws"
NFQWS_BIN_OPT="/opt/bin/nfqws"
CONFDIR="/etc/storage/zapret"
CONFDIR_EXAMPLE="/usr/share/zapret"
CONFFILE="$CONFDIR/config"
PIDFILE="/var/run/zapret.pid"

readonly HOSTLIST_MARKER="<HOSTLIST>"
readonly HOSTLIST_NOAUTO_MARKER="<HOSTLIST_NOAUTO>"

### default config

HOSTLIST_NOAUTO="
    --hostlist=/etc/storage/zapret/user.list
    --hostlist=/etc/storage/zapret/auto.list
    --hostlist-exclude=/etc/storage/zapret/exclude.list
"
HOSTLIST="
    --hostlist=/etc/storage/zapret/user.list
    --hostlist-exclude=/etc/storage/zapret/exclude.list
    --hostlist-auto=/etc/storage/zapret/auto.list
"
ISP_INTERFACE=
IPV6_ENABLED=0
TCP_PORTS=80,443
UDP_PORTS=443,50000:50099
NFQUEUE_NUM=200
LOG_LEVEL=0
USER="nobody"

###

for i in "a1" "a2" "a3" "a4" "b1" "b2" "b3" "b4" ; do
    disk_path="/media/AiDisk_${i}"
    if [ -d "${disk_path}" ] && grep -q ${disk_path} /proc/mounts ; then
        if [ -f "${disk_path}$NFQWS_BIN_OPT" ]; then
            NFQWS_BIN="${disk_path}$NFQWS_BIN_OPT"
            chmod +x "$NFQWS_BIN"
            break
        fi
    fi
done

test -f "$NFQWS_BIN" || exit
test -f "$CONFDIR" && rm -f "$CONFDIR"
test -d "$CONFDIR" || mkdir -p "$CONFDIR"
# copy all non-existent config files to storage except fake dir
cp -n "${CONFDIR_EXAMPLE}"/* -t "$CONFDIR" >/dev/null 2>&1

source "$CONFFILE"

_MANGLE_RULES() ( echo "
-A POSTROUTING -o $IFACE -p tcp -m multiport --dports $TCP_PORTS -m connbytes --connbytes-dir=original --connbytes-mode=packets --connbytes 1:9 -m mark ! --mark 0x40000000/0x40000000 -j NFQUEUE --queue-num $NFQUEUE_NUM --queue-bypass
-A PREROUTING  -i $IFACE -p tcp -m multiport --sports $TCP_PORTS -m connbytes --connbytes-dir=reply    --connbytes-mode=packets --connbytes 1:6 -m mark ! --mark 0x40000000/0x40000000 -j NFQUEUE --queue-num $NFQUEUE_NUM --queue-bypass
-A POSTROUTING -o $IFACE -p udp -m multiport --dports $UDP_PORTS -m connbytes --connbytes-dir=original --connbytes-mode=packets --connbytes 1:9 -m mark ! --mark 0x40000000/0x40000000 -j NFQUEUE --queue-num $NFQUEUE_NUM --queue-bypass
")

_NAT_RULES() ( echo "
-A POSTROUTING -o $IFACE -p udp -m mark --mark 0x40000000/0x40000000 -j MASQUERADE
")

log() {
  echo "$@"
  test -f "$PIDFILE" && _pid="[$(cat "$PIDFILE" 2>/dev/null)]"
  logger -t "zapret${_pid}" "$@"
}

error() {
  log "$@"
  exit 1
}

is_running() {
  PID_RUNNING=$(pgrep -nf "`basename $NFQWS_BIN`.*daemon" 2>/dev/null)

  if [ -z "$PID_RUNNING" ]; then
    return 1
  fi

  if [ ! -f "$PIDFILE" ]; then
    return 1
  fi

  PID_SAVED=$(cat "$PIDFILE" 2>/dev/null)
  if [ "$PID_RUNNING" -ne "$PID_SAVED" ]; then
    return 1
  fi

  if ! kill -0 $PID_SAVED; then
    return 1
  fi

  # 0 = true, 1 = false
  return 0
}

status_service() {
  if is_running; then
    echo 'Service nfqws is running'
  else
    echo 'Service nfqws is stopped'
  fi
}

kernel_modules() {
  KERNEL=$(uname -r)

  # Try to load all modules (OpenWRT or Padavan)
  modprobe -a -q nfnetlink_queue xt_connbytes xt_NFQUEUE &> /dev/null

  if [ -z "$(lsmod 2>/dev/null | grep "nfnetlink_queue ")" ]; then
    nfnetlink_mod_path=$(find "/lib/modules/$KERNEL" -name "nfnetlink_queue.ko*")

    if [ -n "$nfnetlink_mod_path" ]; then
      insmod "$nfnetlink_mod_path" &> /dev/null
      echo "nfnetlink_queue.ko loaded"
    else
      echo "Cannot find nfnetlink_queue.ko module"
    fi
  fi

  if [ -z "$(lsmod 2>/dev/null | grep "xt_connbytes ")" ]; then
    connbytes_mod_path=$(find "/lib/modules/$KERNEL" -name "xt_connbytes.ko*")

    if [ -n "$connbytes_mod_path" ]; then
      insmod "$connbytes_mod_path" &> /dev/null
      echo "xt_connbytes.ko loaded"
    else
      echo "Cannot find xt_connbytes.ko module"
    fi
  fi

  if [ -z "$(lsmod 2>/dev/null | grep "xt_NFQUEUE ")" ]; then
    nfqueue_mod_path=$(find "/lib/modules/$KERNEL" -name "xt_NFQUEUE.ko*")

    if [ -n "$nfqueue_mod_path" ]; then
      insmod "$nfqueue_mod_path" &> /dev/null
      echo "xt_NFQUEUE.ko loaded"
    else
      echo "Cannot find xt_NFQUEUE.ko module"
    fi
  fi
}

_replace_str()
{
  local a=$(echo "$1" | sed 's/\//\\\//g')
  local b=$(echo "$2" | tr '\n' ' ' | sed 's/\//\\\//g')
  shift; shift
  echo "$@" | tr '\n' ' ' | sed "s/$a/$b/g; s/[ \t]\{1,\}/ /g"
}

_startup_args() {
  local args="--user=$USER --qnum=$NFQUEUE_NUM"

  # Logging
  if [ "$LOG_LEVEL" -eq "1" ]; then
    args="--debug=syslog $args"
  fi

  NFQWS_ARGS="$(grep -v '^#' /etc/storage/zapret/strategy)"
  NFQWS_ARGS=$(_replace_str "$HOSTLIST_MARKER" "$HOSTLIST" "$NFQWS_ARGS")
  NFQWS_ARGS=$(_replace_str "$HOSTLIST_NOAUTO_MARKER" "$HOSTLIST_NOAUTO" "$NFQWS_ARGS")
  echo "$args $NFQWS_ARGS"
}

firewall_stop() {
  eval "$(iptables-save -t mangle 2>/dev/null | grep 0x40000000 | sed 's/^-[A,I]/iptables -t mangle -D/g')"
  eval "$(iptables-save -t nat 2>/dev/null | grep 0x40000000 | sed 's/^-[A,I]/iptables -t nat -D/g')"
  eval "$(ip6tables-save -t mangle 2>/dev/null | grep 0x40000000 | sed 's/^-[A,I]/ip6tables -t mangle -D/g')"
}

firewall_start() {
  firewall_stop

  unset IF_LOG
  unset FW_MANGLE
  unset FW_NAT

  _ISP_IF=$(
    echo "$ISP_INTERFACE,$(ip -4 r s default | cut -d ' ' -f5)" |\
    tr " " "\n" | tr "," "\n" | sort -u
  );

  for IFACE in ${_ISP_IF}; do
    FW_MANGLE="$FW_MANGLE$(_MANGLE_RULES)"
    FW_NAT="$FW_NAT$(_NAT_RULES)"
    IF_LOG="$IF_LOG $IFACE"
  done

  [ -n "$FW_MANGLE" ] &&\
  iptables-restore -n  2>/dev/null <<EOF
*mangle
$(echo "$FW_MANGLE")
COMMIT
*nat
$(echo "$FW_NAT")
COMMIT
EOF

  if [ -n "$IPV6_ENABLED" ] && [ "$IPV6_ENABLED" -ne "1" ]; then return; fi

  unset FW_MANGLE

  _ISP_IF=$(
    echo "$ISP_INTERFACE,$(ip -6 r s default | cut -d ' ' -f5)" |\
    tr " " "\n" | tr "," "\n" | sort -u
  );

  for IFACE in ${_ISP_IF}; do
    FW_MANGLE="$FW_MANGLE$(_MANGLE_RULES)"
    IF_LOG="$IF_LOG $IFACE"
  done

  [ -n "$FW_MANGLE" ] &&\
  ip6tables-restore -n  2>/dev/null <<EOF
*mangle
$(echo "$FW_MANGLE")
COMMIT
EOF
}

system_config() {
  sysctl -w net.netfilter.nf_conntrack_checksum=0 &> /dev/null
  sysctl -w net.netfilter.nf_conntrack_tcp_be_liberal=1 &> /dev/null
}

start() {
  if is_running; then
    echo 'Service nfqws is already running' >&2
    return 1
  fi

  kernel_modules

  res=$($NFQWS_BIN --daemon --pidfile=$PIDFILE $(_startup_args) 2>&1) ||\
    error "failed to start nfqws service: $res"

  firewall_start
  system_config

  if [ -n "$IF_LOG" ]; then
    log "Service nfqws is started on "$(echo $IF_LOG | tr " " "\n" | sort -u)" interface(s)"
  else
    log "Service nfqws is started without iptables rules: unknown interface(s)"
  fi

  echo "$res" | grep -iv "loading" | while read i; do
    log "$i"
  done
}

stop() {
  firewall_stop

  if ! is_running; then
    echo 'Service zapret is not running' >&2
    return 1
  fi

  killall -q -s 15 $(basename "$NFQWS_BIN") && rm -f "$PIDFILE"
  if is_running; then
    log 'Service nfqws is not stopped'
  else
    log 'Service nfqws is stopped'
  fi
}

case "$1" in
  start)
    start
    ;;
  stop)
    stop
    ;;
  status)
    status_service
    ;;
  restart)
    stop
    start
    ;;
  firewall-start)
    firewall_start
    ;;
  firewall-stop)
    firewall_stop
    ;;
  kernel-modules)
    kernel_modules
    ;;
  *)
    echo "Usage: $0 {start|stop|restart|status}"
esac
