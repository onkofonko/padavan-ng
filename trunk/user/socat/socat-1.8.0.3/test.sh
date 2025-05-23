#! /usr/bin/env bash
# source: test.sh
# Copyright Gerhard Rieger and contributors (see file CHANGES)
# Published under the GNU General Public License V.2, see file COPYING

# perform lots of tests on socat

# this script uses functions; you need a shell that supports them

# you can pass general options to socat: export OPTS="-d -d -d -d -lu"
# you can eg strace socat with: export TRACE="strace -v -tt -ff -D -x -s 1024 -o /tmp/$USER/socat.strace"
#set -vx

#TODO: Add options for interface, broadcast-interface

[ -z "$USER" ] && USER="$LOGNAME"      # HP-UX
if [ -z "$TMPDIR" ]; then
    if [ -z "$TMP" ]; then
	TMP=/tmp
    fi
    TMPDIR="$TMP"
fi
#E=-e  # Linux
if   [ $(echo "x\c") = "x" ]; then E=""
elif [ $(echo -e "x\c") = "x" ]; then E="-e"
else
    echo "cannot suppress trailing newline on echo" >&2
    exit 1
fi
ECHO="echo $E"
PRINTF="printf"

GREP_E="grep -E"
GREP_F="grep -F"

TRUE=$(type -p true)

usage() {
    $ECHO "Usage: $0 <options> [<test-spec> ...]"
    $ECHO "options:"
    $ECHO "\t-h \t\tShow this help"
    $ECHO "\t-t <sec> \tBase for timeouts in seconds, default is automatically determined"
    $ECHO "\t-v \t\tBe more verbose, show failed commands"
    $ECHO "\t-n <num> \tOnly perform test with given number"
    $ECHO "\t-N <num> \tOnly perform tests starting with given number"
    $ECHO "\t-C \t\tClear/remove left over certificates from previous runs"
    $ECHO "\t-x \t\tShow commands executed, even when test succeeded"
    $ECHO "\t-d \t\tShow log output of commands, even when they did not fail (not yet completed)"
    $ECHO "\t-D \t\tOutput some platform/system specific defines (variables)"
    $ECHO "\t--internet \tAllow tests that send packets to Internet"
    $ECHO "\t--experimental \tApply --experimental option to Socat"
    $ECHO "\t--expect-fail N1,N2,... \tIgnore failure of these tests"
    $ECHO "\ttest-spec \Number of test or name of test"
    $ECHO "Contents of environment variable OPTS are passed to Socat invocations, e.'g:"
    $ECHO "OPTS=\"-d -d -d -d -lu\" ./test.sh"
    $ECHO "TRACE=\"strace -tt -v\" 	Use trace,valgrind etc.on socat"
    $ECHO "SOCAT=/path/to/socat \tselect socat executable for test"
    $ECHO "FILAN=... PROCAN=..."
    $ECHO "Find the tests' stdout,stderr,diff in $TMPDIR/$USER/\$PID"
}

val_t=
NUMCOND=true
#NUMCOND="test \$N -gt 70"
VERBOSE=
DEBUG=
DEFS=
INTERNET=
EXPERIMENTAL=
OPT_EXPECT_FAIL= EXPECT_FAIL=
while [ "$1" ]; do
    case "X$1" in
	X-h)   usage; exit 0 ;;
	X-d)   DEBUG="1" ;;
	X-D)   DEFS="1" ;;
	X-t?*) val_t="${1#-t}" ;;
	X-t)   shift; val_t="$1" ;;
	X-v)   VERBOSE=1 ;; 	# show commands
	X-n?*) NUMCOND="test \$N -eq ${1#-n}" ;;
	X-n)   shift; NUMCOND="test \$N -eq $1" ;;
	X-N?*) NUMCOND="test \$N -gt ${1#-N}" ;;
	X-N)   shift; NUMCOND="test \$N -ge $1" ;;
	X-C)   rm -f testcert*.conf testcert.dh testcli*.* testsrv*.* testalt.* ;;
	X--internet|X-internet)	INTERNET=1 ;; 	# allow access to 3rd party Internet hosts
	X--experimental) 	EXPERIMENTAL=1 ;;
	X--expect-fail|X-expect-fail) OPT_EXPECT_FAIL=1; shift; EXPECT_FAIL="$1" ;;
	X-*)   echo "Unknown option \"$1\"" >&2
               usage >&2
               exit 1 ;;
	*) break;
    esac
    shift
done
debug=$DEBUG

# Applying patch 1.8.0.3 to 1.8.0.2 generates this non executably
[ -f ./socks5server-echo.sh ] && chmod a+x ./socks5server-echo.sh
  
[ "$DEFS" ] && echo "BASH_VERSION=\"$BASH_VERSION\"" >&2

[ "$DEFS" ] && echo "ECHO=\"$ECHO\"" >&2

UNAME=`uname`
[ "$DEFS" ] && echo "UNAME=\"$UNAME\"" >&2
UNAME_R=`uname -r`
[ "$DEFS" ] && echo "UNAME_R=\"$UNAME_R\"" >&2

#MICROS=100000
case "X$val_t" in
    X*.???????*) S="${val_t%.*}"; uS="${val_t#*.}"; uS="${uS:0:6}" ;;
    X*.*) S="${val_t%.*}"; uS="${val_t#*.}"; uS="${uS}000000"; uS="${uS:0:6}" ;;
    X*) S="${val_t}"; uS="000000" ;;
esac
MICROS=${S}${uS}
MICROS=${MICROS##0000}; MICROS=${MICROS##00}; MICROS=${MICROS##0}
# changed below again

divide_uint_by_1000000 () {
    x=$1
    if [ ${#x} -ge 7 ]; then
	echo ${x%??????}.${x: -6};
    else
	y=000000$x;
	f=${y: -6};
	echo 0.$f;
    fi
}


# output the value in seconds for n * val_t
relsecs () {
    local n="$1"
    divide_uint_by_1000000 $((n*MICROS))
}

_MICROS=$((MICROS+999999)); SECONDs="${_MICROS%??????}"
[ -z "$SECONDs" ] && SECONDs=0
[ "$DEFS" ] && echo "SECONDs=\"$SECONDs\"" >&2

withroot=0	# 1: perform privileged tests even if not run by root

[ -z "$SOCAT" ] && SOCAT="./socat"
if ! [ -x "$SOCAT" ] && ! type $SOCAT >/dev/null 2>&1; then
    echo "$SOCAT does not exist" >&2; exit 1;
fi
if [ "$SOCAT" = socat ]; then
    SOCAT=$(type -p socat) || SOCAT=$(which socat)
fi
[ "$DEFS" ] && echo "SOCAT=\"$SOCAT\"" >&2

if [ -z "$PROCAN" ]; then if test -x ./procan; then PROCAN="./procan"; elif type procan >/dev/null 2>&1; then PROCAN=procan; elif test -x ${SOCAT%/*}/procan; then PROCAN=${SOCAT%/*}/procan; else PROCAN=false; fi; fi
[ "$DEFS" ] && echo "PROCAN=\"$PROCAN\"" >&2
if [ -z "$FILAN" ]; then if test -x ./filan; then FILAN="./filan"; elif ! type filan >/dev/null 2>&1; then FILAN=filan; elif test -x ${SOCAT%/*}/filan; then FILAN=${SOCAT%/*}/filan; else FILAN=false; fi; fi
[ "$DEFS" ] && echo "FILAN=\"$FILAN\"" >&2

if ! sleep 0.1 2>/dev/null; then
    sleep () {
	$SOCAT -T "$1" PIPE PIPE
    }
fi

if [ -z "$val_t" ]; then
    # Estimate the time Socat needs for an empty run
    sleep 0.5 	# immediately after build the first runs are extremely fast
    $SOCAT /dev/null /dev/null 	# populate caches
    MILLIs=$(bash -c "time for _ in {1..3}; do $SOCAT -d0 $opts /dev/null /dev/null; done" 2>&1 |grep ^real |sed 's/.*m\(.*\)s.*/\1/' |tr -d ,.)
    while [ "${MILLIs:0:1}" = '0' ]; do MILLIs=${MILLIs##0}; done 	# strip leading '0' to avoid octal
    [ -z "$MILLIs" ] && MILLIs=1
    [ "$DEFS" ] && echo "MILLIs=\"$MILLIs\"" >&2
    MICROS=${MILLIs}000

    case $MICROS in
	???????*) val_t=${MICROS%??????}.${MICROS: -6} ;;
	*)        x=000000$MICROS; val_t=0.${x: -6} ;;
    esac
else
    # Calculate MICROS from val_t
    case "X$val_t" in
        X*.???????*) S="${val_t%.*}"; uS="${val_t#*.}"; uS="${uS:0:6}" ;;
        X*.*) S="${val_t%.*}"; uS="${val_t#*.}"; uS="${uS}000000"; uS="${uS:0:6}" ;;
        X*) S="${val_t}"; uS="000000" ;;
    esac
    MICROS=${S}${uS}
    MICROS=${MICROS##0000}; MICROS=${MICROS##00}; MICROS=${MICROS##0}
fi
export MICROS
[ "$DEFS" ] && echo "MICROS=\"$MICROS\"" >&2
[ "$DEFS" ] && echo "val_t=\"$val_t\"" >&2
opt_t="-t $val_t"
M2=$((2*MICROS))
M4=$((4*MICROS))
M8=$((8*MICROS))
case $M2 in ???????*) T2=${M2%??????}.${M2: -6};; *) x=00000$M2; T2=0.${x: -6};; esac
case $M4 in ???????*) T4=${M4%??????}.${M4: -6};; *) x=00000$M4; T4=0.${x: -6};; esac
case $M8 in ???????*) T8=${M8%??????}.${M8: -6};; *) x=00000$M8; T8=0.${x: -6};; esac
[ "$DEFS" ] && echo "M2=\"$M2\"" >&2
[ "$DEFS" ] && echo "T2=\"$T2\"" >&2
[ "$DEFS" ] && echo "T4=\"$T4\"" >&2
[ "$DEFS" ] && echo "T8=\"$T8\"" >&2

_MICROS=$((MICROS+999999)); SECONDs="${_MICROS%??????}"
[ -z "$SECONDs" ] && SECONDs=0


#PATH=$PATH:/opt/freeware/bin
#PATH=$PATH:/usr/local/ssl/bin
PATH=$PATH:/sbin 	# RHEL6:ip
case "$0" in
    */*) PATH="${0%/*}:$PATH"
esac
PATH=.:$PATH 	# for relsleep
[ "$DEFS" ] && echo "PATH=\"$PATH\"" >&2

#OPENSSL_RAND="-rand /dev/egd-pool"
#SOCAT_EGD="egd=/dev/egd-pool"
MISCDELAY=1

OPTS="$opt_t $OPTS"

if [ "$EXPERIMENTAL" ]; then
    if $SOCAT -h |grep -e --experimental >/dev/null; then
	OPTS="$OPTS --experimental"
    fi
fi

opts="$OPTS"
[ "$DEFS" ] && echo "opts=\"$opts\"" >&2

TESTS="$*"; export TESTS
if ! SOCAT_MAIN_WAIT= $SOCAT -V >/dev/null 2>&1; then
    echo "Failed to execute $SOCAT, exiting" >&2
    exit 1
fi

SOCAT_VERSION=$(SOCAT_MAIN_WAIT= $SOCAT -V |head -n 2 |tail -n 1 |sed 's/.* \([0-9][1-9]*\.[0-9][0-9]*\.[0-9][^[:space:]]*\).*/\1/')
if [ -z "$SOCAT_VERSION" ]; then
    echo "Warning: failed to retrieve Socat version" >&2
fi
[ "$DEFS" ] && echo "SOCAT_VERSION=\"$SOCAT_VERSION\"" >&2

if type ip >/dev/null 2>&1; then
    IP_V="$(ip -V)"
    [ "$DEFS" ] && echo "IP_V=\"$IP_V\""
    if echo "$IP_V" |grep -q -i -e "^ip utility, iproute2-" -e BusyBox; then
	IP=$(type -p ip)
    else
	unset IP
    fi
fi

if type ss >/dev/null 2>&1; then
    SS_V="$(ss -V)"
    [ "$DEFS" ] && echo "SS_V=\"$SS_V\""
    # On Ubuntu-10 ss has differing output format (no "LISTEN"), use netstat then
    if echo "$SS_V" |grep -q -e "^ss utility, iproute2-[2-6]" -e "^ss utility, iproute2-ss[^0]"; then
	SS=$(type -p ss)
    else
	unset SS
    fi
fi
[ "$DEFS" ] && echo "NETSTAT=\"$(type netstat 2>/dev/null)\""

# for some tests we need a network interface
if type ip >/dev/null 2>&1; then
    INTERFACE=$(ip r get 9.9.9.9 |grep ' dev ' |head -n 1 |sed "s/.*dev[[:space:]][[:space:]]*\([^[:space:]][^[:space:]]*\).*/\1/")
else
    case "$UNAME" in
	Linux)
	    if [ "$IP" ]; then
		INTERFACE="$($IP route get 9.9.9.9 |grep ' dev ' |sed -e 's/.* dev //' -e 's/ .*//')"
	    else
		INTERFACE="$(netstat -rn |grep -e "^default" -e "^0\.0\.0\.0" |awk '{print($8);}')"
	    fi ;;
	FreeBSD) INTERFACE="$(netstat -rn |grep -e "^default" -e "^0\.0\.0\.0" |awk '{print($4);}')" ;;
	*)       INTERFACE="$(netstat -rn |grep -e "^default" -e "^0\.0\.0\.0" |awk '{print($4);}')" ;;
    esac
fi
[ "$DEFS" ] && echo "INTERFACE=\"$INTERFACE\"" >&2
MCINTERFACE=$INTERFACE
[ -z "$MCINTERFACE" ] && MCINTERFACE=lo	# !!! Linux only - and not always
[ "$DEFS" ] && echo "MCINTERFACE=\"$MCINTERFACE\"" >&2

#LOCALHOST=192.168.58.1
LOCALHOST=localhost 	# attention: on FreeBSD-10 localhost resolves primarily to IPv6
LOCALHOST4=127.0.0.1
LOCALHOST6="[::1]"
#IPPROTO=$(awk '{print($2);}' /etc/protocols |sort -n |tail -n 1)
#IPPROTO=$(($PROTO+1))
IPPROTO=$((144+RANDOM/2048))
[ "$DEFS" ] && echo "IPPROTO=\"$IPPROTO\"" >&2
_PORT=12001
SOURCEPORT=2002
REUSEADDR=reuseaddr 	# use this with LISTEN addresses and bind options

# get some system constants for use in tests
SOCK_DGRAM="$($PROCAN -c |grep "^#define[[:space:]]*SOCK_DGRAM[[:space:]]" |cut -d' ' -f3)"
[ "$DEFS" ] && echo "SOCK_DGRAM=\"$SOCK_DGRAM\"" >&2
FOPEN_MAX=$($PROCAN -c 2>/dev/null |grep '^#define[ ][ ]*FOPEN_MAX' |awk '{print($3);}')
[ "$DEFS" ] && echo "FOPEN_MAX=\"$FOPEN_MAX\"" >&2
PF_INET6="$($PROCAN -c |grep "^#define[[:space:]]*PF_INET6[[:space:]]" |cut -d' ' -f3)"
[ "$DEFS" ] && echo "PF_INET6=\"$PF_INET6\"" >&2
TIOCEXCL="$($PROCAN -c |grep "^#define[[:space:]]*TIOCEXCL[[:space:]]" |{ read _ _ v; echo "$v"; })"
[ "$DEFS" ] && echo "TIOCEXCL=\"$TIOCEXCL\"" >&2
SOL_SOCKET="$($PROCAN -c |grep "^#define[[:space:]]*SOL_SOCKET[[:space:]]" |cut -d' ' -f3)"
[ "$DEFS" ] && echo "SOL_SOCKET=\"$SOL_SOCKET\"" >&2
SO_REUSEADDR="$($PROCAN -c |grep "^#define[[:space:]]*SO_REUSEADDR[[:space:]]" |cut -d' ' -f3)"
[ "$DEFS" ] && echo "SO_REUSEADDR=\"$SO_REUSEADDR\"" >&2
TCP_MAXSEG="$($PROCAN -c |grep "^#define[[:space:]]*TCP_MAXSEG[[:space:]]" |cut -d' ' -f3)"
[ "$DEFS" ] && echo "TCP_MAXSEG=\"$TCP_MAXSEG\"" >&2
SIZE_T=$($PROCAN |grep "^[^[:space:]]*size_t" |awk '{print($3);}')
[ "$DEFS" ] && echo "SIZE_T=\"$SIZE_T\"" >&2
#AI_ADDRCONFIG=; if [ "$($SOCAT -hhh |grep ai-addrconfig)" ]; then AI_ADDRCONFIG="ai-addrconfig=0"; fi
#[ "$DEFS" ] && echo "AI_ADDRCONFIG=\"$AI_ADDRCONFIG\"" >&2

CAT="cat"
OD_C="od -c"

toupper () {
    case ${BASH_VERSION:0:1} in
	[1-3]) echo "$@" |tr a-z A-Z ;;
	[4-9]) echo "${@^^*}" ;;
    esac
}

tolower () {
    case ${BASH_VERSION:0:1} in
	[1-3]) echo "$@" |tr A-Z a-z ;;
	[4-9]) echo "${@,,*}" ;;
    esac
}

# calculate the time i*MICROS, output as float number for us with -t
reltime () {
    local n="$1"
    local S uS
    local N=$((n*MICROS))
    case "$N" in
	*???????) S="${N%??????}"; uS="${N:${#N}-6}" ;;
	*) S=0; uS="00000$N"; uS="${uS:${#uS}-6}" ;;
    esac
    echo "$S.$uS"
}

# A sleep with configurable clocking ($val_t)
# val_t should be at least the time that a Socat invocation, no action, and
# termination takes
relsleep () {
    #sleep $(($1*MICROS/1000000))
    sleep $(divide_uint_by_1000000 $(($1*MICROS)) )
}

cat >relsleep <<-'EOF'
#! /usr/bin/env bash
    n="$1"
    N=$((n*MICROS))
    case "$N" in
	*???????) S="${N%??????}"; uS="${N:${#N}-6}" ;;
	*) S=0; uS="00000$N"; uS="${uS:${#uS}-6}" ;;
    esac
    sleep "$S.$uS"
EOF
chmod a+x relsleep

if type ping6 >/dev/null 2>&1; then
    PING6=ping6
else
    PING6="ping -6"
fi

F_n="%3d"	# format string for test numbers
export LC_ALL=C	# for timestamps format...
export LANG=C
export LANGUAGE=C	# knoppix
case "$UNAME" in
HP-UX|OSF1)
    echo "$SOCAT -u stdin stdout" >cat.sh
    chmod a+x cat.sh
    CAT=./cat.sh
    ;;
SunOS)
    # /usr/bin/tr doesn't handle the a-z range syntax (needs [a-z]), use
    # /usr/xpg4/bin/tr instead
    alias tr=/usr/xpg4/bin/tr
    ;;
*)
    CAT="cat"
    ;;
esac
[ "$DEFS" ] && echo "CAT=\"$CAT\"" >&2

TRUE=$(type -p true)
#E=-e	# Linux
if   [ $(echo "x\c") = "x" ]; then E=""
elif [ $(echo -e "x\c") = "x" ]; then E="-e"
else
    echo "cannot suppress trailing newline on echo" >&2
    exit 1
fi
ECHO="echo $E"
PRINTF="printf"

GREP_E="grep -E"
GREP_F="grep -F"

# some OSes need special options
case "$UNAME" in
#HP-UX)
#    # on HP-UX, the default options (below) hang some tests (former 14, 15)
#    PTYOPTS=
#    PTYOPTS2=
#    ;;
SunOS)
    PTYOPTS="perm=600"
    PTYOPTS2="echo=0,opost=0"
	;;
*)
    PTYOPTS="echo=0,opost=0"
    #PTYOPTS2="raw,echo=0"
    PTYOPTS2="cfmakeraw"
    #PTYOPTS2="rawer"
    ;;
esac
[ "$DEFS" ] && echo "PTYOPTS=\"$PTYOPTS\"" >&2
[ "$DEFS" ] && echo "PTYOPTS2=\"$PTYOPTS2\"" >&2

# for some tests we need an unprivileged user id to su to
if [ "$SUDO_USER" ]; then
    SUBSTUSER="$SUDO_USER"
else
    SUBSTUSER="$(grep -v '^[^:]*:^[^:]*:0:' /etc/passwd |tail -n 1 |cut -d: -f1)"
fi
[ "$DEFS" ] && echo "SUBSTUSER=\"$SECONDs\"" >&2

if [ -z "$SS" ]; then
# non-root users might miss ifconfig in their path
case "$UNAME" in
AIX)   IFCONFIG=/usr/sbin/ifconfig ;;
FreeBSD) IFCONFIG=/sbin/ifconfig ;;
HP-UX) IFCONFIG=/usr/sbin/ifconfig ;;
Linux) IFCONFIG=/sbin/ifconfig ;;
NetBSD)IFCONFIG=/sbin/ifconfig ;;
OpenBSD)IFCONFIG=/sbin/ifconfig ;;
OSF1)  IFCONFIG=/sbin/ifconfig ;;
SunOS) IFCONFIG=/sbin/ifconfig ;;
Darwin)IFCONFIG=/sbin/ifconfig ;;
DragonFly) IFCONFIG=/sbin/ifconfig ;;
*)     IFCONFIG=/sbin/ifconfig ;;
esac
fi
[ "$DEFS" ] && echo "SS=\"$SS\"" >&2
[ "$DEFS" ] && echo "IFCONFIG=\"$IFCONFIG\"" >&2

# need output like "644"
case "$UNAME" in
    #Linux) fileperms() { stat -L --print "%a\n" "$1" 2>/dev/null; } ;;
    FreeBSD) fileperms() { stat -L -x "$1" |grep ' Mode:' |sed 's/.* Mode:[[:space:]]*([0-9]\([0-7][0-7][0-7]\).*/\1/'; } ;;
    *) fileperms() {
	    local p s=0 c
	    p="$(ls -l -L "$1" |awk '{print($1);}')"
	    p="${p:1:9}"
	    while [ "$p" ]; do c=${p:0:1}; p=${p:1}; [ "x$c" == x- ]; let "s=2*s+$?"; done
	    printf "%03o\n" $s;
	} ;;
esac

# need user (owner) of filesystem entry
case "$UNAME" in
    Linux) fileuser() { stat -L --print "%U\n" "$1" 2>/dev/null; } ;;
    FreeBSD) fileuser() { ls -l "$1" |awk '{print($3);}'; } ;;
    *) fileuser() { ls -l "$1" |awk '{print($3);}'; } ;;
esac

if2addr4() {
    local IF="$1"
    if [ "$IP" ]; then
	$IP address show dev "$IF" |grep "inet " |sed -e "s/.*inet //" -e "s/ .*//"
    else
	$IFCONFIG "$BROADCASTIF" |grep 'inet ' |awk '{print($2);}' |cut -d: -f2
    fi
}

if2bc4() {
    local IF="$1"
    if [ "$IP" ]; then
	$IP address show dev "$IF" |grep ' inet .* brd ' |awk '{print($4);}'
    else
	$IFCONFIG "$IF" |grep 'broadcast ' |sed 's/.*broadcast/broadcast/' |awk '{print($2);}'
    fi
}

# for some tests we need a second local IPv4 address
case "$UNAME" in
Linux)
  if [ "$IP" ]; then
    BROADCASTIF=$($IP r get 9.9.9.9 |grep ' dev ' |sed 's/.*\<dev[[:space:]][[:space:]]*\([a-z0-9][a-z0-9]*\).*/\1/')
  else
    BROADCASTIF=$(route -n |grep '^0.0.0.0 ' |awk '{print($8);}')
  fi
    [ -z "$BROADCASTIF" ] && BROADCASTIF=eth0
    SECONDADDR=127.1.0.1
    SECONDMASK=255.255.0.0
    BCADDR=127.255.255.255
    BCIFADDR=$(if2addr4 $BROADCASTIF) ;;
FreeBSD|NetBSD|OpenBSD)
    MAINIF=$($IFCONFIG -a |grep '^[a-z]' |grep -v '^lo0: ' |head -1 |cut -d: -f1)
    BROADCASTIF="$MAINIF"
    SECONDADDR=$($IFCONFIG "$BROADCASTIF" |grep 'inet ' |sed 's|/.*||' |awk '{print($2);}')
    BCIFADDR="$SECONDADDR"
    BCADDR=$($IFCONFIG "$BROADCASTIF" |grep 'broadcast ' |sed 's/.*broadcast/broadcast/' |awk '{print($2);}') ;;
HP-UX)
    MAINIF=lan0	# might use "netstat -ni" for this
    BROADCASTIF="$MAINIF"
    SECONDADDR=$($IFCONFIG $MAINIF |tail -n 1 |awk '{print($2);}')
    BCADDR=$($IFCONFIG $BROADCASTIF |grep 'broadcast ' |sed 's/.*broadcast/broadcast/' |awk '{print($2);}') ;;
SunOS)
    MAINIF=$($IFCONFIG -a |grep '^[a-z]' |grep -v '^lo0: ' |head -1 |cut -d: -f1)
    BROADCASTIF="$MAINIF"
    #BROADCASTIF=hme0
    #BROADCASTIF=eri0
    #SECONDADDR=$($IFCONFIG $BROADCASTIF |grep 'inet ' |awk '{print($2);}')
    SECONDADDR=$(expr "$($IFCONFIG -a |grep 'inet ' |$GREP_F -v ' 127.0.0.1 '| head -n 1)" : '.*inet \([0-9.]*\) .*')
    #BCIFADDR="$SECONDADDR"
    #BCADDR=$($IFCONFIG $BROADCASTIF |grep 'broadcast ' |sed 's/.*broadcast/broadcast/' |awk '{print($2);}')
    ;;
DragonFly)
    MAINIF=$($IFCONFIG -a |grep -v ^lp |grep '^[a-z]' |grep -v '^lo0: ' |head -1 |cut -d: -f1)
    BROADCASTIF="$MAINIF"
    SECONDADDR=$($IFCONFIG "$BROADCASTIF" |grep 'inet ' |awk '{print($2);}')
    BCIFADDR="$SECONDADDR"
    BCADDR=$($IFCONFIG "$BROADCASTIF" |grep 'broadcast ' |sed 's/.*broadcast/broadcast/' |awk '{print($2);}') ;;
#AIX|FreeBSD|Solaris)
*)
    SECONDADDR=$(expr "$($IFCONFIG -a |grep 'inet ' |$GREP_F -v ' 127.0.0.1 ' |head -n 1)" : '.*inet \([0-9.]*\) .*')
    ;;
esac
# for generic sockets we need this address in hex form
if [ "$SECONDADDR" ]; then
    SECONDADDRHEX="$(printf "%02x%02x%02x%02x\n" $(echo "$SECONDADDR" |tr '.' ' '))"
fi

# for some tests we need a second local IPv6 address
case "$UNAME" in
Linux) if [ "$IP" ]; then
	   SECONDIP6ADDR=$(expr "$($IP address |grep 'inet6 ' |$GREP_F -v ' ::1/128 '| head -n 1)" : '.*inet6 \([0-9a-f:][0-9a-f:]*\)/.*')
       else
	   SECONDIP6ADDR=$(expr "$($IFCONFIG -a |grep 'inet6 ' |$GREP_F -v ' ::1/128 '| head -n 1)" : '.*inet \([0-9.]*\) .*')
       fi ;;
*)
    SECONDIP6ADDR=$(expr "$($IFCONFIG -a |grep 'inet6 ' |$GREP_F -v ' ::1/128 '| head -n 1)" : '.*inet \([0-9.]*\) .*')
    ;;
esac
if [ -z "$SECONDIP6ADDR" ]; then
#    case "$TESTS" in
#	*%root2%*) $IFCONFIG eth0 ::2/128
#    esac
    SECONDIP6ADDR="$LOCALHOST6"
else
    SECONDIP6ADDR="[$SECONDIP6ADDR]"
fi

case "$TERM" in
vt100|vt320|linux|xterm|cons25|dtterm|aixterm|sun-color|xterm-color|xterm-256color|screen)
	# there are different behaviours of printf (and echo)
	# on some systems, echo behaves different than printf...
	if [ "$($PRINTF "\0101")" = "A" ]; then
		RED="\0033[31m"
		GREEN="\0033[32m"
		YELLOW="\0033[33m"
		NORMAL="\0033[39m"
	else 	# "\101"
		RED="\033[31m"
		GREEN="\033[32m"
		YELLOW="\033[33m"
		NORMAL="\033[39m"
	fi
	OK="${GREEN}OK${NORMAL}"
	FAILED="${RED}FAILED${NORMAL}"
	NO_RESULT="${YELLOW}NO RESULT${NORMAL}"
	CANT="$NO_RESULT"
	;;
*)	OK="OK"
	FAILED="FAILED"
	NO_RESULT="NO RESULT"
	CANT="$NO_RESULT"
	;;
esac

if [ -x /usr/xpg4/bin/id ]; then
    # SunOS has rather useless tools in its default path
    PATH="/usr/xpg4/bin:$PATH"
fi


[ -z "$TESTS" ] && TESTS="consistency functions filan"
# use '%' as separation char
TESTS="%$(echo " $TESTS " |tr ' ' '%')%"

[ -z "$USER" ] && USER="$LOGNAME"	# HP-UX
if [ -z "$TMPDIR" ]; then
    if [ -z "$TMP" ]; then
	TMP=/tmp
    fi
    TMPDIR="$TMP"
fi
TD="$TMPDIR/$USER/$$"; td="$TD"
rm -rf "$TD" || (echo "cannot rm $TD" >&2; exit 1)
mkdir -p "$TD"
#trap "rm -r $TD" 0 3

echo "Using temp directory $TD"

RESULTS="$TD/results.txt" 	# file for list of results

case "$TESTS" in
*%consistency%*)
# test if addresses are sorted alphabetically:
$ECHO "testing if address array is sorted...\c"
TF="$TD/socat-q"
IFS="$($ECHO ' \n\t')"
if ! $SOCAT -hhh >/dev/null; then
    echo "Failed: $SOCAT -hhh" >&2
    exit 2
fi
$SOCAT -hhh |sed -n '/^   address-head:/,/^   opts:/ p' |grep -v -e "^   address-head:" -e "^   opts:" |sed -e 's/^[[:space:]]*//' -e 's/[: ].*//' |grep -v '^<' >"$TF"
$SOCAT -hhh |sed -n '/^   address-head:/,/^   opts:/ p' |grep -v -e "^   address-head:" -e "^   opts:" |sed -e 's/^[[:space:]]*//' -e 's/[: ].*//' |grep -v '^<' |LC_ALL=C sort |diff "$TF" - >"$TF-diff"
if [ -s "$TF-diff" ]; then
    $ECHO "\n*** address array is not sorted. Wrong entries:" >&2
    cat "$TD/socat-q-diff" >&2
    exit 1
else
    echo " ok"
fi
#/bin/rm "$TF"
#/bin/rm "$TF-diff"
esac

case "$TESTS" in
*%consistency%*)
# test if address options array ("optionnames") is sorted alphabetically:
$ECHO "testing if address options are sorted...\c"
TF="$TD/socat-qq"
$SOCAT -hhh |sed '1,/opt:/ d' |awk '{print($1);}' >"$TF"
LC_ALL=C sort "$TF" |diff "$TF" - >"$TF-diff"
if [ -s "$TF-diff" ]; then
    $ECHO "\n*** option array is not sorted. Wrong entries:" >&2
    cat "$TD/socat-qq-diff" >&2
    exit 1
else
    echo " ok"
fi
/bin/rm "$TF"
/bin/rm "$TF-diff"
esac

case "$TESTS" in
*%consistency%*)
    # Test if help shows option types without inconsistency
    $ECHO "testing if help shows option types correctly...\c"
    TF="$TD/socat-hhh"
    LINE="$($SOCAT -hhh |grep "^[[:space:]]*ip-add-source-membership\>")"
    if [ -z "$LINE" ]; then
	$ECHO $CANT
    else
	TYPE="$($ECHO "$LINE" |sed 's/^.*type=\([^[:space:]][^[:space:]]*\).*/\1/')"
	if [ "$TYPE" != "IP-MREQ-SOURCE" ]; then
	    $ECHO "\n*** help does not show option types correctly" >&2
	    exit 1
	else
	    echo " ok"
	fi
    fi
esac

case "$TESTS" in
*%consistency%*)
    # Test if help shows option phases without inconsistency
    $ECHO "testing if help shows option phases correctly...\c"
    TF="$TD/socat-hhh"
    LINE="$($SOCAT -hhh |grep "^[[:space:]]*dash\>")"
    if [ -z "$LINE" ]; then
	$ECHO $CANT
    else
	PHASE="$($ECHO "$LINE" |sed 's/^.*phase=\([^[:space:]][^[:space:]]*\).*/\1/')"
	if [ "$PHASE" != "PREEXEC" ]; then
	    $ECHO "\n*** help does not show option phases correctly" >&2
	    exit 1
	else
	    echo " ok"
	fi
    fi
esac

case "$TESTS" in
*%consistency%*)
    # Test if help shows option groups without inconsistency
    $ECHO "testing if help shows option groups correctly...\c"
    TF="$TD/socat-hhh"
    LINE="$($SOCAT -hhh |grep "^[[:space:]]*udplite-recv-cscov\>")"
    if [ -z "$LINE" ]; then
	$ECHO $CANT
    else
	GROUP="$($ECHO "$LINE" |sed 's/^.*groups=\([^[:space:]][^[:space:]]*\).*/\1/')"
	if [ "$GROUP" != "UDPLITE" ]; then
	    $ECHO "\n*** help does not show option groups correctly" >&2
	    exit 1
	else
	    echo " ok"
	fi
    fi
esac

#==============================================================================

N=1
numOK=0
numFAIL=0
numCANT=0
listOK=
listFAIL=
listCANT=

ok () {
    numOK=$((numOK+1))
    listOK="$listOK $N"
    do_result OK
}

cant () {
    numCANT=$((numCANT+1))
    listCANT="$listCANT $N"
    do_result CANT
}

failed () {
    numFAIL=$((numFAIL+1))
    listFAIL="$listFAIL $N"
    do_result FAILED
}

do_result () {
    #echo "RESULTS=\"$RESULTS\"" >&2; exit
    echo "$N $NAME $1" >>$RESULTS
}

#==============================================================================
# test if selected socat features work ("FUNCTIONS")

testecho () {
    local N="$1"
    local NAME="$2"
    local title="$3"
    local arg1="$4";	[ -z "$arg1" ] && arg1="-"
    local arg2="$5";	[ -z "$arg2" ] && arg2="echo"
    local opts="$6"
    local T="$7";	[ -z "$T" ] && T=0 	# fractional seconds
    local tf="$td/test$N.stdout"
    local te="$td/test$N.stderr"
    local tdiff="$td/test$N.diff"
    local da="test$N $(date) $RANDOM"
    if ! eval $NUMCOND; then :; else
    #local cmd="$TRACE $SOCAT $opts $arg1 $arg2"
    #$ECHO "testing $title (test $N)... \c"
    $PRINTF "test $F_n %s... " $N "$title"
    #echo "$da" |$cmd >"$tf" 2>"$te"
    { sleep $T; echo "$da"; sleep $T; } | { $TRACE $SOCAT $opts "$arg1" "$arg2" >"$tf" 2>"$te"; echo $? >"$td/test$N.rc"; } &
    pid1=$!
    #sleep 5 && kill $pid1 2>/dev/null &
#    rc2=$!
    wait $pid1
#    kill $rc2 2>/dev/null
    if [ "$(cat "$td/test$N.rc")" != 0 ]; then
	$PRINTF "$FAILED: $TRACE $SOCAT:\n"
	echo "$TRACE $SOCAT $opts $arg1 $arg2" >&2
	cat "$te" >&2
	failed
    elif echo "$da" |diff - "$tf" >"$tdiff" 2>&1; then
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$SOCAT $opts $arg1 $arg2" >&2; fi
	if [ -n "$debug" ]; then cat $te >&2; fi
	ok
    else
	$PRINTF "$FAILED:\n"
	echo "$TRACE $SOCAT $opts $arg1 $arg2" >&2
	cat "$te" >&2
	echo diff: >&2
	cat "$tdiff" >&2
	failed
    fi
    fi # NUMCOND
}

# test if call to od and throughput of data works - with graceful shutdown and
# flush of od buffers
testod () {
    local num="$1"
    local NAME="$2"
    local title="$3"
    local arg1="$4";	[ -z "$arg1" ] && arg1="-"
    local arg2="$5";	[ -z "$arg2" ] && arg2="echo"
    local opts="$6"
    local T="$7";	[ -z "$T" ] && T=0 	# fractional seconds
    local tf="$td/test$N.stdout"
    local te="$td/test$N.stderr"
    local tr="$td/test$N.ref"
    local tdiff="$td/test$N.diff"
    local dain="$(date) $RANDOM"
    if ! eval $NUMCOND; then :; else
    echo "$dain" |$OD_C >"$tr"
#    local daout="$(echo "$dain" |$OD_C)"
    $PRINTF "test $F_n %s... " $num "$title"
    (sleep $T; echo "$dain"; sleep $T) |$TRACE $SOCAT $opts "$arg1" "$arg2" >"$tf" 2>"$te"
    if [ "$?" != 0 ]; then
	$PRINTF "$FAILED: $TRACE $SOCAT:\n"
	echo "$TRACE $SOCAT $opts $arg1 $arg2"
	cat "$te"
	failed
#    elif echo "$daout" |diff - "$tf" >"$tdiff" 2>&1; then
    elif diff "$tr" "$tf" >"$tdiff" 2>&1; then
	$PRINTF "$OK\n"
	if [ -n "$debug" ]; then cat $te; fi
	ok
    else
	$PRINTF "$FAILED: diff:\n"
	echo "$TRACE $SOCAT $opts $arg1 $arg2"
	cat "$te"
	cat "$tdiff"
	failed
    fi
    fi # NUMCOND
}

# bash before version 3 aborts scripts that contain unquoted '=~'
# Therefore we create a shell script and quotedly fill it with '=~' for newer
# bashes [regexp regular expressions]
mkdir -p $td/bin
rm -f $td/bin/re_match
if [ "${BASH_VERSION%%[.]*}" -le 2 ]; then
    echo '[ -n "$(echo "$1" |sed -n "/$2/ p")" ]' >$td/bin/re_match
else
    echo '[[ "$1" =~ $2 ]]' >$td/bin/re_match
fi
chmod a+x $td/bin/re_match
PATH=$PATH:$td/bin


# test if the socat executable has these features compiled in
# print the first missing address type
testfeats () {
    local a A;
    for a in $@; do
	A=$(echo "$a" |tr 'a-z-' 'A-Z_')
	if [ "$A" = "IP" ]; then
	    if SOCAT_MAIN_WAIT= $SOCAT -V |grep "#define WITH_IP4 1\$" >/dev/null ||
		    SOCAT_MAIN_WAIT= $SOCAT -V |grep "#define WITH_IP6 1\$" >/dev/null; then
		shift
		continue
	    else
		echo "$a"
		return 1
	    fi
	fi
	if SOCAT_MAIN_WAIT= $SOCAT -V |grep "#define WITH_$A 1\$" >/dev/null; then
#	    if [[ "$A" =~ OPENSSL.* ]]; then
	    if re_match "$A" "OPENSSL.*"; then
		gentestcert testsrv
		gentestcert testcli
	    fi
	    shift
	    continue
	fi
	echo "$a"
	return 1
    done
    return 0
}

# test if the socat executable has these address types compiled in
# print the first missing address type
testaddrs () {
    local a A;
    for a in $@; do
	A=$(echo "$a" |tr 'a-z' 'A-Z')
	# the ::::: forces syntax errer and prevents the address from doing anything
	if ! $SOCAT $A::::: /dev/null 2>&1 </dev/null |grep -q "E unknown device/address"; then
	    shift
	    continue
	fi
	echo "$a"
	return 1
    done
    return 0
}

# test if the socat executable has these options compiled in
# print the first missing option
testoptions () {
    local a A;
    for a in $@; do
	A=$(echo "$a" |tr 'a-z' 'A-Z')
	if $SOCAT -hhh |grep "[^a-z0-9-]$a[^a-z0-9-]" >/dev/null; then
	    shift
	    continue
	fi
	echo "$a"
	return 1
    done
    return 0
}

# check if the given pid exists and has child processes
# if yes: prints child process lines to stdout, returns 0
# if not: prints ev.message to stderr, returns 1
childprocess () {
    local l
    case "$1" in
	[1-9]*) ;;
	*) echo "childprocess \"$1\": not a number" >&2; exit 2 ;;
    esac
    case "$UNAME" in
    AIX)     l="$(ps -fade |grep "^........ ...... $(printf %6u $1)")" ;;
    FreeBSD) l="$(ps -faje |grep "^........ ..... $(printf %5u $1)")" ;;
    HP-UX)   l="$(ps -fade |grep "^........ ..... $(printf %5u $1)")" ;;
    Linux)   l="$(ps -fade |grep "^........ ..... $(printf %5u $1)")" ;;
#    NetBSD)  l="$(ps -aj   |grep "^........ ..... $(printf %4u $1)")" ;;
    NetBSD)  l="$(ps -aj   |grep "^[^ ][^ ]*[ ][ ]*..... $(printf %5u $1)")" ;;
    OpenBSD) l="$(ps -aj   |grep "^........ ..... $(printf %5u $1)")" ;;
    SunOS)   l="$(ps -fade |grep "^........ ..... $(printf %5u $1)")" ;;
    DragonFly)l="$(ps -faje |grep "^[^ ][^ ]*[ ][ ]*..... $(printf %5u $1)")" ;;
    CYGWIN*)  l="$(ps -pafe |grep "^[^ ]*[ ][ ]*[^ ][^ ]*[ ][ ]*$1[ ]")" ;;
    *)       l="$(ps -fade |grep "^[^ ][^ ]*[ ][ ]*[0-9][0-9]**[ ][ ]*$(printf %5u $1) ")" ;;    esac
    if [ -z "$l" ]; then
	return 1;
    fi
    echo "$l"
    return 0
}

# return a list of child process pids [killchild]
childpids () {
    local recursive i
    if [ "X$1" = "X-r" ]; then recursive=1; shift; fi
    case "$1" in
	[1-9]*) ;;
	*) echo "childpids \"$1\": not a number" >&2; exit 2 ;;
    esac
    case "$UNAME" in
    AIX)     l="$(ps -fade |grep "^........ ...... $(printf %6u $1)" |awk '{print($2);}')" ;;
    FreeBSD) l="$(ps -fl   |grep "^[^ ][^ ]*[ ][ ]*[0-9][0-9]*[ ][ ]*$1[ ]" |awk '{print($2);}')" ;;
    HP-UX)   l="$(ps -fade |grep "^........ ..... $(printf %5u $1)" |awk '{print($2);}')" ;;
#    Linux)   l="$(ps -fade |grep "^........ ..... $(printf %5u $1)" |awk '{print($2);}')" ;;
    Linux)   l="$(ps -fade |grep "^[^[:space:]][^[:space:]]*[[:space:]][[:space:]]*[^[:space:]][^[:space:]]*[[:space:]][[:space:]]*$1 " |awk '{print($2);}')" ;;
#    NetBSD)  l="$(ps -aj   |grep "^........ ..... $(printf %4u $1)" |awk '{print($2);}')" ;;
    NetBSD)  l="$(ps -aj   |grep "^[^ ][^ ]*[ ][ ]*..... $(printf %5u $1)" |awk '{print($2);}')" ;;
    OpenBSD) l="$(ps -aj   |grep "^........ ..... $(printf %5u $1)" |awk '{print($2);}')" ;;
    SunOS)   l="$(ps -fade |grep "^........ ..... $(printf %5u $1)" |awk '{print($2);}')" ;;
    DragonFly)l="$(ps -faje |grep "^[^ ][^ ]*[ ][ ]*..... $(printf %5u $1)" |awk '{print($2);}')" ;;
    CYGWIN*)  l="$(ps -pafe |grep "^[^ ]*[ ][ ]*[^ ][^ ]*[ ][ ]*$1[ ]" |awk '{print($2);}')" ;;
    *)       l="$(ps -fade |grep "^[^ ][^ ]*[ ][ ]*[0-9][0-9]*[ ][ ]*$(printf %5u $1) " |awk '{print($2);}')" ;;    esac
    if [ -z "$l" ]; then
	return 1;
    fi
    if [ "$recursive" ]; then
	for i in $l; do
	    l="$l $(childpids -r $i)"
	done
    fi
    echo "$l"
    return 0
}

# check if the given process line refers to a defunct (zombie) process
# yes: returns 0
# no: returns 1
isdefunct () {
    local l
    case "$UNAME" in
    AIX)     l="$(echo "$1" |grep ' <defunct>$')" ;;
    FreeBSD) l="$(echo "$1" |grep ' <defunct>$')" ;;
    HP-UX)   l="$(echo "$1" |grep ' <defunct>$')" ;;
    Linux)   l="$(echo "$1" |grep ' <defunct>$')" ;;
    SunOS)   l="$(echo "$1" |grep ' <defunct>$')" ;;
    DragonFly)l="$(echo "$1" |grep ' <defunct>$')" ;;
    *)       l="$(echo "$1" |grep ' <defunct>$')" ;;
    esac
    [ -n "$l" ];
}

# check if UNIX socket protocol is available on host
runsunix () {
    return 0;
    $TRACE $SOCAT /dev/null UNIX-LISTEN:"$td/unix.socket" 2>"$td/unix.stderr" &
    pid=$!
    relsleep 1
    kill "$pid" 2>/dev/null
    test ! -s "$td/unix.stderr"
}

unset HAVENOT_IP4
# check if an IP4 loopback interface exists
runsip4 () {
    [ -n "$HAVENOT_IP4" ] && return $HAVENOT_IP4
    local l
    case "$UNAME" in
    AIX)   l=$($IFCONFIG lo0 |$GREP_F 'inet 127.0.0.1 ') ;;
    FreeBSD) l=$($IFCONFIG lo0 |$GREP_F 'inet 127.0.0.1 ') ;;
    HP-UX) l=$($IFCONFIG lo0 |$GREP_F 'inet 127.0.0.1 ') ;;
    Linux) if [ "$IP" ]; then
	       l=$($IP address |$GREP_E ' inet 127.0.0.1/')
	   else
	       l=$($IFCONFIG |$GREP_E 'inet (addr:)?127\.0\.0\.1 ')
	   fi ;;
    NetBSD)l=$($IFCONFIG -a |grep 'inet 127\.0\.0\.1\>');;
    OpenBSD)l=$($IFCONFIG -a |$GREP_F 'inet 127.0.0.1 ');;
    OSF1)  l=$($IFCONFIG -a |grep ' inet ') ;;
    SunOS) l=$($IFCONFIG -a |grep 'inet ') ;;
    Darwin)l=$($IFCONFIG lo0 |$GREP_F 'inet 127.0.0.1 ') ;;
    DragonFly)l=$($IFCONFIG -a |$GREP_F 'inet 127.0.0.1 ');;
    CYGWIN*) l=$(ipconfig |grep IPv4);;
    *)     l=$($IFCONFIG -a |grep ' ::1[^:0-9A-Fa-f]') ;;
    esac
    [ -z "$l" ] && return 1
    # existence of interface might not suffice, check for routeability:
    case "$UNAME" in
    Darwin) ping -c 1 127.0.0.1 >/dev/null 2>&1; l="$?" ;;
    Linux)  ping -c 1 127.0.0.1 >/dev/null 2>&1; l="$?" ;;
    *) if [ -n "$l" ]; then l=0; else l=1; fi ;;
    esac
    HAVENOT_IP4=$l
    if [ "$HAVENOT_IP4" -ne 0 ]; then
	echo IP4
    fi
    return $l;
}

unset HAVENOT_IP6
# check if an IP6 loopback interface exists
runsip6 () {
    [ -n "$HAVENOT_IP6" ] && return $HAVENOT_IP6
    local l
    case "$UNAME" in
    AIX)   l=$($IFCONFIG lo0 |grep 'inet6 ::1[/%]') ;;
    HP-UX) l=$($IFCONFIG lo0 |grep ' inet6 ') ;;
    Linux) if [ "$IP" ]; then
	       l="$($IP address |$GREP_E 'inet6 ::1/128')"
	   else
	       l="$($IFCONFIG |$GREP_E 'inet6 (addr: )?::1/?')"
	   fi ;;
    NetBSD)l=$($IFCONFIG -a |grep 'inet6 ::1\>');;
    OSF1)  l=$($IFCONFIG -a |grep ' inet6 ') ;;
    SunOS) l=$($IFCONFIG -a |grep 'inet6 ') ;;
    Darwin)l=$($IFCONFIG lo0 |grep 'inet6 ::1 ') ;;
    CYGWIN*) l=$(ipconfig |grep IPv6);;
    *)     l=$($IFCONFIG -a |grep ' ::1[^:0-9A-Fa-f]') ;;
    esac
    [ -z "$l" ] && return 1
    # existence of interface might not suffice, check for routeability:
    case "$UNAME" in
    Darwin) $PING6 -c 1 ::1 >/dev/null 2>&1; l="$?" ;;
    Linux)  $PING6 -c 1 ::1 >/dev/null 2>&1; l="$?" ;;
    *) if [ -n "$l" ]; then l=0; else l=1; fi ;;
    esac
    HAVENOT_IP6=$l
    if [ "$HAVENOT_IP6" -ne 0 ]; then
	echo IP6
    fi
    return "$HAVENOT_IP6"
}

# check if TCP on IPv4 is available on host
runstcp4 () {
    runsip4 >/dev/null || { echo TCP4; return 1; }
    $SOCAT -h |grep -i ' TCP4-' >/dev/null || return 1
    return 0;
}

# check if TCP on IPv6 is available on host
runstcp6 () {
    runsip6 >/dev/null || { echo TCP6; return 1; }
    $SOCAT -h |grep -i ' TCP6-' >/dev/null || return 1
    return 0;
}

# check if UDP on IPv4 is available on host
runsudp4 () {
    runsip4 >/dev/null || { echo UDP4; return 1; }
    $SOCAT -h |grep -i ' UDP4-' >/dev/null || return 1
    return 0;
}

# check if UDP on IPv6 is available on host
runsudp6 () {
    runsip6 >/dev/null || { echo UDP6; return 1; }
    $SOCAT -h |grep -i ' UDP6-' >/dev/null || return 1
    return 0;
}

# check if SCTP on IPv4 is available on host
runssctp4 () {
    runsip4 >/dev/null || { echo SCTP4; return 1; }
    $SOCAT -h |grep -i ' SCTP4-' >/dev/null || return 1
    $SOCAT /dev/null SCTP4-L:0,accept-timeout=0.001 2>/dev/null || return 1;
    return 0;
}

# check if SCTP on IPv6 is available on host
runssctp6 () {
    runsip6 >/dev/null || { echo SCTP6; return 1; }
    $SOCAT -h |grep -i ' SCTP6-' >/dev/null || return 1
    $SOCAT /dev/null SCTP6-L:0,accept-timeout=0.001 2>/dev/null || return 1;
    return 0;
}

# check if DCCP on IPv4 is available on host
runsdccp4 () {
    runsip4 >/dev/null || { echo DCCP4; return 1; }
    $SOCAT -h |grep -i ' DCCP4-' >/dev/null || return 1
    $SOCAT /dev/null DCCP4-L:0,accept-timeout=0.001 2>/dev/null || return 1;
    return 0;
}

# check if DCCP on IPv6 is available on host
runsdccp6 () {
    runsip6 >/dev/null || { echo DCCP6; return 1; }
    $SOCAT -h |grep -i ' DCCP6-' >/dev/null || return 1
    $SOCAT /dev/null DCCP6-L:0,accept-timeout=0.001 2>/dev/null || return 1;
    return 0;
}

# check if UDPLITE on IPv4 is available on host
runsudplite4 () {
    runsip4 >/dev/null || { echo UDPLITE4; return 1; }
    $SOCAT -u -T 0.001 /dev/null UDPLITE4-SENDTO:$LOCALHOST4:0 2>/dev/null || return 1;
    return 0;
}

# check if UDPLITE on IPv6 is available on host
runsudplite6 () {
    runsip6 >/dev/null || { echo UDPLITE6; return 1; }
    $SOCAT -u -T 0.001 /dev/null UDPLITE6-SENDTO:$LOCALHOST6:0 2>/dev/null || return 1;
    return 0;
}

# check if UNIX domain sockets work
runsunix () {
    # for now...
    return 0;
}

routesip6 () {
    runsip6 >/dev/null || { echo route6; return 1; }
    $PING6 -c 1 2606:4700:4700::1111 >/dev/null 2>&1 || { echo route6; return 1; }
    return 0;
}


# SSL needs runsip6(), thus moved down

# SSL certificate contents
TESTCERT_CONF=testcert.conf
TESTCERT6_CONF=testcert6.conf
TESTALT_CONF=testalt.conf
#
TESTCERT_COMMONNAME="$LOCALHOST"
TESTCERT_COMMONNAME6="$LOCALHOST6"
TESTCERT_COUNTRYNAME="XY"
TESTCERT_LOCALITYNAME="Lunar Base"
TESTCERT_ORGANIZATIONALUNITNAME="socat"
TESTCERT_ORGANIZATIONNAME="dest-unreach"
TESTCERT_SUBJECT="C = $TESTCERT_COUNTRYNAME, CN = $TESTCERT_COMMONNAME, O = $TESTCERT_ORGANIZATIONNAME, OU = $TESTCERT_ORGANIZATIONALUNITNAME, L = $TESTCERT_LOCALITYNAME"
TESTCERT_ISSUER="C = $TESTCERT_COUNTRYNAME, CN = $TESTCERT_COMMONNAME, O = $TESTCERT_ORGANIZATIONNAME, OU = $TESTCERT_ORGANIZATIONALUNITNAME, L = $TESTCERT_LOCALITYNAME"
RSABITS=2048 	# Ubuntu-20.04 with OpenSSL-1.1.1f does not work with 1024 nor 1536
DSABITS=2048
cat >$TESTCERT_CONF <<EOF
prompt=no

[ req ]
default_bits = $RSABITS
distinguished_name=Test

[ Test ]
countryName=$TESTCERT_COUNTRYNAME
commonName=$TESTCERT_COMMONNAME
O=$TESTCERT_ORGANIZATIONNAME
OU=$TESTCERT_ORGANIZATIONALUNITNAME
L=$TESTCERT_LOCALITYNAME

EOF

cat >$TESTCERT6_CONF <<EOF
prompt=no

[ req ]
default_bits = $RESBITS
distinguished_name=Test

[ Test ]
countryName=$TESTCERT_COUNTRYNAME
commonName=$TESTCERT_COMMONNAME6
O=$TESTCERT_ORGANIZATIONNAME
OU=$TESTCERT_ORGANIZATIONALUNITNAME
L=$TESTCERT_LOCALITYNAME

EOF

cat >$TESTALT_CONF <<EOF
# config for generation of self signed certificate with IP addresses in
# SubjectAltNames
prompt=no

[ req ]
default_bits       = $RSABITS
distinguished_name = subject
x509_extensions    = x509_ext

[ subject ]
countryName=$TESTCERT_COUNTRYNAME
commonName=servername
O=$TESTCERT_ORGANIZATIONNAME
OU=$TESTCERT_ORGANIZATIONALUNITNAME
L=$TESTCERT_LOCALITYNAME

[ x509_ext ]
subjectAltName     = @alternate_names

[ alternate_names ]
DNS.1 = localhost
DNS.2 = localhost4
DNS.3 = localhost6
IP.1  = 127.0.0.1
EOF

if runsip6; then
   cat >>$TESTALT_CONF <<EOF
IP.2  = ::1
EOF
fi


# clean up from previous runs
rm -f testcli.{crt,key,pem}
rm -f testsrv.{crt,key,pem}
rm -f testcli6.{crt,key,pem}
rm -f testsrv6.{crt,key,pem}
rm -f testalt.{crt,key,pem}

OPENSSL_S_CLIENT_4=
OPENSSL_S_CLIENT_DTLS=
init_openssl_s_client () {
    if openssl s_client -help 2>&1 |grep -q ' -4 '; then
	OPENSSL_S_CLIENT_4="-4"
    else
	OPENSSL_S_CLIENT_4=" "
    fi
    if openssl s_client -help 2>&1 | grep -q ' -dtls1_2 '; then
	OPENSSL_S_CLIENT_DTLS="-dtls1_2"
    elif openssl s_client -help 2>&1 | grep -q ' -dtls1 '; then
	OPENSSL_S_CLIENT_DTLS="-dtls1"
    elif openssl s_client -help 2>&1 | grep -q ' -dtls '; then
	OPENSSL_S_CLIENT_DTLS="-dtls"
    else
	OPENSSL_S_CLIENT_DTLS=
    fi
}

OPENSSL_S_SERVER_4=
OPENSSL_S_SERVER_DTLS=
OPENSSL_S_SERVER_NO_IGN_EOF=
init_openssl_s_server () {
    if openssl s_server -help 2>&1 |grep -q ' -4 '; then
	OPENSSL_S_SERVER_4="-4"
    else
	OPENSSL_S_SERVER_4=" "
    fi
    if openssl s_server -help 2>&1 | grep -q ' -dtls1_2 '; then
	OPENSSL_S_SERVER_DTLS="-dtls1_2"
    elif openssl s_server -help 2>&1 | grep -q ' -dtls1 '; then
	OPENSSL_S_SERVER_DTLS="-dtls1"
    elif openssl s_server -help 2>&1 | grep -q ' -dtls '; then
	OPENSSL_S_SERVER_DTLS="-dtls"
    else
	OPENSSL_S_SERVER_DTLS=
    fi
    if openssl s_server -help 2>&1 | grep -q ' -no-ign_eof '; then
	OPENSSL_S_SERVER_NO_IGN_EOF="-no-ign_eof"
    else
	OPENSSL_S_SERVER_NO_IGN_EOF=" "
    fi
}


# Perform a couple of checks to make sure the test has a chance of a useful
# result:
# platform is supported, features compiled in, addresses and options
# available; needs root; is allowed to access the internet
checkconds() {
    local unames="$(echo "$1")" 	# must be one of... exa: "Linux,FreeBSD"
    local root="$2" 				# "root" or ""
    local progs="$(echo "$3" |tr 'A-Z,' 'a-z ')"	# exa: "nslookup"
    local feats="$(echo "$4" |tr 'a-z,' 'A-Z ')" 	# list of req.features (socat -V)
    local addrs="$(echo "$5" |tr 'a-z,' 'A-Z ')" 	# list of req.addresses (socat -h)
    local opts="$(echo "$6" |tr 'A-Z,' 'a-z ')" 	# list of req.options (socat -hhh)
    local runs="$(echo "$7" |tr , ' ')" 		# list of req.protocols, exa: "sctp6"
    local inet="$8" 					# when "internet": needs allowance
    local i

    if [ "$unames" ]; then
	local uname="$(echo $UNAME |tr 'A-Z' 'a-z')"
	for i in $unames; do
	    if [ "$uname" = "$(echo "$i" |tr 'A-Z,' 'a-z ')" ]; then
		# good, mark as passed
		i=
		break;
	    fi
	done
	[ "$i" ] && { echo "Only on (one of) $unames"; return 255; }
    fi

    if [ "$progs" ]; then
	for i in $progs; do
	    if ! type  >/dev/null 2>&1; then
		echo "Program $i not available"
		return 255
	    fi
	done
    fi

    if [ "$feats" ]; then
	if ! F=$(testfeats $feats); then
	    echo "Feature $F not configured in $SOCAT"
	    return 255
	fi
    fi

    if [ "$addrs" ]; then
	if ! A=$(testaddrs - $addrs); then
	    echo "Address $A not available in $SOCAT"
	    return 255
	fi
    fi

    if [ "$opts" ]; then
	if ! o=$(testoptions $opts); then
	    echo "Option $o not available in $SOCAT"
	    return 255
	fi
    fi

    if [ "$runs" ]; then
	for i in $runs; do
	    if ! runs$i >/dev/null; then
		echo "$i not available on host"
		return 255;
	    fi
	done
    fi

    if [ "$inet" ]; then
	if [ -z "$INTERNET" ]; then
	    echo "Use test.sh option --internet"
	    return 255
	fi
    fi

    # Only at the end, so we get a better overview of missing features
    if [ "$root" = "root" ]; then
	if [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
	    echo "Must be root"
	    return 255
	fi
    fi

    return 0
}


# wait until an IP4 protocol is ready
waitip4proto () {
    local proto="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	Linux) if [ "$SS" ]; then
		   l=$($SS -n -w -l |grep '^\(raw\|UNCONN\) .* .*[0-9*]:'$proto' [ ]*0\.0\.0\.0:\*')
	       else
		   l=$(netstat -n -w -l |grep '^raw .* .*[0-9*]:'$proto' [ ]*0\.0\.0\.0:\*')
	       fi ;;
#	FreeBSD) l=$(netstat -an |$GREP_E '^raw46? .*[0-9*]\.'$proto' .* \*\.\*') ;;
#	NetBSD)  l=$(netstat -an |grep '^raw .*[0-9*]\.'$proto' [ ]* \*\.\*') ;;
#	OpenBSD) l=$(netstat -an |grep '^raw .*[0-9*]\.'$proto' [ ]* \*\.\*') ;;
#	Darwin) case "$(uname -r)" in
#		[1-5]*) l=$(netstat -an |grep '^raw.* .*[0-9*]\.'$proto' .* \*\.\*') ;;
#		*) l=$(netstat -an |grep '^raw4.* .*[0-9*]\.'$proto' .* \*\.\* .*') ;;
#		esac ;;
	AIX)	 # does not seem to show raw sockets in netstat
		 relsleep 5;  return 0 ;;
#	SunOS)   l=$(netstat -an -f inet -P raw |grep '.*[1-9*]\.'$proto' [ ]*Idle') ;;
#	HP-UX)   l=$(netstat -an |grep '^raw        0      0  .*[0-9*]\.'$proto' .* \*\.\* ') ;;
#	OSF1)    l=$(/usr/sbin/netstat -an |grep '^raw        0      0  .*[0-9*]\.'$proto' [ ]*\*\.\*') ;;
	*)       #l=$(netstat -an |grep -i 'raw .*[0-9*][:.]'$proto' ') ;;
		 relsleep 5;  return 0 ;;
	esac
	[ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	  \( \( $logic -eq 0 \) -a -z "$l" \) ] && return 0
	sleep $val_t
	timeout=$((timeout-1))
    done

    $ECHO "!protocol $proto timed out! \c" >&2
    return 1
}

# we need this misleading function name for canonical reasons
waitip4port () {
    waitip4proto "$1" "$2" "$3"
}

# wait until an IP6 protocol is ready
waitip6proto () {
    local proto="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	Linux)
		if [ "$SS" ]; then
		    l=$($SS -n -w -l |grep '^\(raw\|UNCONN\) .* \*:'$proto' [ ]*\*:\*')
		else
		    l=$(netstat -n -w -l |grep '^raw[6 ] .* .*:[0-9*]*:'$proto' [ ]*:::\*')
		fi ;;
#	FreeBSD) l=$(netstat -an |$GREP_E '^raw46? .*[0-9*]\.'$proto' .* \*\.\*') ;;
#	NetBSD)  l=$(netstat -an |grep '^raw .*[0-9*]\.'$proto' [ ]* \*\.\*') ;;
#	OpenBSD) l=$(netstat -an |grep '^raw .*[0-9*]\.'$proto' [ ]* \*\.\*') ;;
#	Darwin) case "$(uname -r)" in
#		[1-5]*) l=$(netstat -an |grep '^raw.* .*[0-9*]\.'$proto' .* \*\.\*') ;;
#		*) l=$(netstat -an |grep '^raw4.* .*[0-9*]\.'$proto' .* \*\.\* .*') ;;
#		esac ;;
	AIX)	 # does not seem to show raw sockets in netstat
		 relsleep 5;  return 0 ;;
#	SunOS)   l=$(netstat -an -f inet -P raw |grep '.*[1-9*]\.'$proto' [ ]*Idle') ;;
#	HP-UX)   l=$(netstat -an |grep '^raw        0      0  .*[0-9*]\.'$proto' .* \*\.\* ') ;;
#	OSF1)    l=$(/usr/sbin/netstat -an |grep '^raw        0      0  .*[0-9*]\.'$proto' [ ]*\*\.\*') ;;
	*)       #l=$(netstat -an |$GREP_E -i 'raw6? .*[0-9*][:.]'$proto' ') ;;
		 relsleep 5;  return 0 ;;
	esac
	[ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	  \( \( $logic -eq 0 \) -a -z "$l" \) ] && return 0
	sleep $val_t
	timeout=$((timeout-1))
    done

    $ECHO "!protocol $proto timed out! \c" >&2
    return 1
}

# we need this misleading function name for canonical reasons
waitip6port () {
    waitip6proto "$1" "$2" "$3"
}

# Check if a TCP port is in use
# exits with 0 when it is not used
checktcpport () {
    local port="$1"
    local l
    case "$UNAME" in
    Linux) if [ "$SS" ]; then
	       l=$($SS -a -n -t |grep ".*:$port\>")
	   else
	       l=$(netstat -a -n -t |grep '^tcp.* .*[0-9*]:'$port' .*')
	   fi ;;
    FreeBSD) l=$(netstat -an |grep '^tcp.* .*[0-9*]\.'$port' .* .*') ;;
    NetBSD)  l=$(netstat -an |grep '^tcp.* .*[0-9*]\.'$port' [ ]* .* [ ]*.*') ;;
    Darwin) case "$(uname -r)" in
	[1-5]*) l=$(netstat -an |grep '^tcp.* .*[0-9*]\.'$port' .* .* .*') ;;
	*) l=$(netstat -an |grep '^tcp.* .*[0-9*]\.'$port' .* .* .*') ;;
	esac ;;
    AIX)     l=$(netstat -an |grep '^tcp.*      0      0 .*[*0-9]\.'$port' .*') ;;
    SunOS)   l=$(netstat -an -f inet -P tcp |grep '.*[1-9*]\.'$port' .*\*                0 .*') ;;
    HP-UX)   l=$(netstat -an |grep '^tcp        0      0  .*[0-9*]\.'$port' .*') ;;
    OSF1)    l=$(/usr/sbin/netstat -an |grep '^tcp        0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*') ;;
    CYGWIN*) l=$(netstat -an -p TCP |grep '^  TCP    [0-9.]*:'$port' .*') ;;
    DragonFly)l=$(netstat -ant |grep '^tcp.* .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]*') ;;
    *)       l=$(netstat -an |grep -i 'tcp .*[0-9*][:.]'$port' .*') ;;
    esac
    [ -z "$l" ] && return 0
    return 1
}

waittcpport () {
    local port="$1"
    local logic="$2" 	# 0..wait until free; 1..wait until listening (default)
    local timeout="$3"
    while true; do
#echo "timeout=\"$timeout\"" >&2
	if [ "$logic" = 0 ]; then
	    if checktcpport $1; then break; fi
	else
	    if ! checktcpport $1; then break; fi
	fi
	if [ $timeout -le 0 ]; then return 1; fi
	sleep 1
	let --timeout;
    done
    return 0;
}

checktcp4port () {
    checktcpport $1
}

# wait until a TCP4 listen port is ready
waittcp4port () {
    local port="$1"
    local logic="$2" 	# 0..wait until free; 1..wait until listening (default)
    local timeout="$3"
    local l
    local vx=+; case $- in *vx*) set +vx; vx=-; esac	# no tracing here
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while true; do
	case "$UNAME" in
	Linux) if [ "$SS" ]; then
	       l=$($SS -l -n -t |grep "^LISTEN .*:$port\>")
	   else
	       l=$(netstat -a -n -t |grep '^tcp .* .*[0-9*]:'$port' .* LISTEN')
	   fi ;;
	FreeBSD) l=$(netstat -an |grep '^tcp4.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
	NetBSD)  l=$(netstat -an |grep '^tcp .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]* LISTEN.*') ;;
	Darwin) case "$(uname -r)" in
		[1-5]*) l=$(netstat -an |grep '^tcp.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
		*) l=$(netstat -an |grep '^tcp4.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
		esac ;;
	AIX)     l=$(netstat -an |grep '^tcp[^6]       0      0 .*[*0-9]\.'$port' .* LISTEN$') ;;
	SunOS)   l=$(netstat -an -f inet -P tcp |grep '.*[1-9*]\.'$port' .*\* .* 0 .* LISTEN') ;;
	HP-UX)   l=$(netstat -an |grep '^tcp        0      0  .*[0-9*]\.'$port' .* LISTEN$') ;;
	OSF1)    l=$(/usr/sbin/netstat -an |grep '^tcp        0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*LISTEN') ;;
	CYGWIN*) l=$(netstat -an -p TCP |grep '^  TCP    [0-9.]*:'$port' .* LISTENING') ;;
	DragonFly)  l=$(netstat -ant |grep '^tcp4 .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]* LISTEN.*') ;;
	*)       l=$(netstat -an |grep -i 'tcp .*[0-9*][:.]'$port' .* listen') ;;
	esac
	if [ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	    \( \( $logic -eq 0 \) -a -z "$l" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	if [ $timeout -le 0 ]; then
	    set ${vx}vx
	    return 1
	fi
	sleep $val_t
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    set ${vx}vx
    return 1
}

# Check if a UDP4 port is in use
# exits with 0 when it is not used
checkudpport () {
    local port="$1"
    local l
    case "$UNAME" in
    Linux) if [ "$SS" ]; then
	       l=$($SS -a -n -u |grep ".*:$port\>")
	   else
	       l=$(netstat -a -n -u |grep '^udp.* .*[0-9*]:'$port' .*')
	   fi ;;
    FreeBSD) l=$(netstat -an |grep '^udp.* .*[0-9*]\.'$port' .* \*\.\* .*') ;;
    NetBSD)  l=$(netstat -an |grep '^udp.* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]*.*') ;;
    Darwin) case "$(uname -r)" in
	[1-5]*) l=$(netstat -an |grep '^udp.* .*[0-9*]\.'$port' .* \*\.\* .*') ;;
	*) l=$(netstat -an |grep '^udp.* .*[0-9*]\.'$port' .* \*\.\* .*') ;;
	esac ;;
    AIX)     l=$(netstat -an |grep '^udp.*       0      0 .*[*0-9]\.'$port' .*') ;;
    SunOS)   l=$(netstat -an -f inet -P udp |grep '.*[1-9*]\.'$port' .*\*                0 .*') ;;
    HP-UX)   l=$(netstat -an |grep '^udp.*       0      0  .*[0-9*]\.'$port' .*') ;;
    OSF1)    l=$(/usr/sbin/netstat -an |grep '^udp.*       0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*') ;;
    CYGWIN*) l=$(netstat -an -p UDP |grep '^  UDP    [0-9.]*:'$port' .*') ;;
    DragonFly)l=$(netstat -ant |grep '^udp.* .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]*') ;;
    *)       l=$(netstat -an |grep -i 'udp .*[0-9*][:.]'$port' .*') ;;
    esac
    [ -z "$l" ] && return 0
    return 1
}

checkudp4port () {
    checkudpport $1
}

# wait until a UDP4 port is ready
waitudp4port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    local vx=+; case $- in *vx*) set +vx; vx=-; esac	# no tracing here
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	Linux) if [ "$SS" ]; then
	       l=$($SS -4 -l -n -u |grep "^UNCONN .*:$port\>")
	   else
	       l=$(netstat -a -n -u -l |grep '^udp .* .*[0-9*]:'$port' [ ]*0\.0\.0\.0:\*')
	   fi ;;
	FreeBSD) l=$(netstat -an |$GREP_E '^udp46? .*[0-9*]\.'$port' .* \*\.\*') ;;
	NetBSD)  l=$(netstat -an |grep '^udp .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
	OpenBSD) l=$(netstat -an |grep '^udp .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
	Darwin) case "$(uname -r)" in
		[1-5]*) l=$(netstat -an |grep '^udp.* .*[0-9*]\.'$port' .* \*\.\*') ;;
		*) l=$(netstat -an |grep '^udp4.* .*[0-9*]\.'$port' .* \*\.\* .*') ;;
		esac ;;
	AIX)	 l=$(netstat -an |grep '^udp[4 ]       0      0 .*[*0-9]\.'$port' .* \*\.\*[ ]*$') ;;
	SunOS)   l=$(netstat -an -f inet -P udp |grep '.*[1-9*]\.'$port' [ ]*Idle') ;;
	HP-UX)   l=$(netstat -an |grep '^udp        0      0  .*[0-9*]\.'$port' .* \*\.\* ') ;;
	OSF1)    l=$(/usr/sbin/netstat -an |grep '^udp        0      0  .*[0-9*]\.'$port' [ ]*\*\.\*') ;;
	DragonFly) l=$(netstat -an |grep '^udp4 .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]*') ;;
	*)       l=$(netstat -an |grep -i 'udp .*[0-9*][:.]'$port' ') ;;
	esac
	if [ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	    \( \( $logic -eq 0 \) -a -z "$l" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	sleep $val_t
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    set ${vx}vx
    return 1
}

# Check if a SCTP port is in use
# exits with 0 when it is not used
checksctpport () {
    local port="$1"
    local l
    case "$UNAME" in
    Linux) if [ "$SS" ]; then
	       l=$($SS -a -n |grep "^sctp.*:$port\>")
	   else
	       l=$(netstat -a -n |grep '^sctp.* .*[0-9*]:'$port' .*')
	   fi ;;
    FreeBSD) l=$(netstat -an |grep '^sctp.* .*[0-9*]\.'$port' .* \*\.\* .*') ;;
    NetBSD)  l=$(netstat -an |grep '^sctp.* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]*.*') ;;
    Darwin) case "$(uname -r)" in
	[1-5]*) l=$(netstat -an |grep '^sctp.* .*[0-9*]\.'$port' .* \*\.\* .*') ;;
	*) l=$(netstat -an |grep '^sctp.* .*[0-9*]\.'$port' .* \*\.\* .*') ;;
	esac ;;
    AIX)     l=$(netstat -an |grep '^sctp.*      0      0 .*[*0-9]\.'$port' .*') ;;
    SunOS)   l=$(netstat -an -f inet -P sctp |grep '.*[1-9*]\.'$port' .*\*                0 .*') ;;
    HP-UX)   l=$(netstat -an |grep '^sctp        0      0  .*[0-9*]\.'$port' .*') ;;
    OSF1)    l=$(/usr/sbin/netstat -an |grep '^sctp.*      0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*') ;;
    CYGWIN*) l=$(netstat -an -p SCTP |grep '^  SCTP   [0-9.]*:'$port' .*') ;;
    DragonFly)l=$(netstat -ant |grep '^sctp.* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]*') ;;
    *)       l=$(netstat -an |grep -i 'sctp.*[0-9*][:.]'$port' .*') ;;
    esac
    [ -z "$l" ] && return 0
    return 1
}

checksctp4port () {
    checksctpport $1
}

# wait until an SCTP4 listen port is ready
waitsctp4port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    local vx=+; case $- in *vx*) set +vx; vx=-; esac	# no tracing here
     [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	Linux) if [ "$SS" ]; then
		   l=$($SS -4 -a -n 2>/dev/null |grep "^sctp.*LISTEN .*:$port\>")
	       else
		   l=$(netstat -n -a |grep '^sctp .*[0-9*]:'$port' .* LISTEN')
	       fi ;;
#	FreeBSD) l=$(netstat -an |grep '^tcp4.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
#	NetBSD)  l=$(netstat -an |grep '^tcp .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]* LISTEN.*') ;;
#	Darwin) case "$(uname -r)" in
#		[1-5]*) l=$(netstat -an |grep '^tcp.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
#		*) l=$(netstat -an |grep '^tcp4.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
#		esac ;;
#	AIX)	 l=$(netstat -an |grep '^tcp[^6]       0      0 .*[*0-9]\.'$port' .* LISTEN$') ;;
	SunOS)   l=$(netstat -an -f inet -P sctp |grep '.*[1-9*]\.'$port' .*\*                0 .* LISTEN') ;;
#	HP-UX)   l=$(netstat -an |grep '^tcp        0      0  .*[0-9*]\.'$port' .* LISTEN$') ;;
#	OSF1)    l=$(/usr/sbin/netstat -an |grep '^tcp        0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*LISTEN') ;;
#	CYGWIN*) l=$(netstat -an -p TCP |grep '^  TCP    [0-9.]*:'$port' .* LISTENING') ;;
	*)       l=$(netstat -an |grep -i 'sctp .*[0-9*][:.]'$port' .* listen') ;;
	esac
	if [ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	    \( \( $logic -eq 0 \) -a -z "$l" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	sleep $val_t
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    set ${vx}vx
    return 1
}

# wait until a UDPLITE4 port is ready
waitudplite4port () {
    local port="$1"
    local logic="$2" # 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    local vx=+; case $- in *vx*) set +vx; vx=-; esac # no tracing here
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	    Linux) #if [ "$SS" ]; then
		#l=$($SS -4 -l -n -u |grep "^UNCONN .*:$port\>")
		#else
		# On Ubuntu-20 only netstat shows udplite ports
		if ! netstat -nU >/dev/null 2>&1; then
                    return 0    # speculative
		fi
		l=$(netstat -a -n -U -l |grep '^udpl .* .*[0-9*]:'$port' [ ]*0\.0\.0\.0:\*')
		#fi
		;;
	    FreeBSD) l=$(netstat -an |$GREP_E '^udpl46? .*[0-9*]\.'$port' .* \*\.\*') ;;
	    NetBSD)  l=$(netstat -an |grep '^udpl .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
	    OpenBSD) l=$(netstat -an |grep '^udpl .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
	    #Darwin) case "$(uname -r)" in
	    #   [1-5]*) l=$(netstat -an |grep '^udp.* .*[0-9*]\.'$port' .* \*\.\*') ;;
	    #   *) l=$(netstat -an |grep '^udp4.* .*[0-9*]\.'$port' .* \*\.\* .*') ;;
	    #   esac ;;
	    #AIX)        l=$(netstat -an |grep '^udp[4 ]       0      0 .*[*0-9]\.'$port' .* \*\.\*[ ]*$') ;;
	    #SunOS)   l=$(netstat -an -f inet -P udp |grep '.*[1-9*]\.'$port' [ ]*Idle') ;;
	    #HP-UX)   l=$(netstat -an |grep '^udp        0      0  .*[0-9*]\.'$port' .* \*\.\* ') ;;
	    #OSF1)    l=$(/usr/sbin/netstat -an |grep '^udp        0      0  .*[0-9*]\.'$port' [ ]*\*\.\*') ;;
	    #DragonFly) l=$(netstat -an |grep '^udp4 .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]*') ;;
	    *)       l=$(netstat -an |grep -i 'udp .*[0-9*][:.]'$port' ') ;;
	esac
	if [ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
		\( \( $logic -eq 0 \) -a -z "$l" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	relsleep 1
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    set ${vx}vx
    return 1
}

# wait until an DCCP4 listen port is ready
waitdccp4port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    local vx=+; case $- in *vx*) set +vx; vx=-; esac	# no tracing here
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	Linux)
	    # On Ubuntu-20, only ss shows DCCP ports
		if [ "$SS" ]; then
		   l=$($SS -4 -a -n 2>/dev/null |grep "^dccp.*LISTEN .*:$port\>")
	       else
		   l=$(netstat -n -a |grep '^dccp .*[0-9*]:'$port' .* LISTEN')
	       fi ;;
#	FreeBSD) l=$(netstat -an |grep '^tcp4.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
#	NetBSD)  l=$(netstat -an |grep '^tcp .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]* LISTEN.*') ;;
#	Darwin) case "$(uname -r)" in
#		[1-5]*) l=$(netstat -an |grep '^tcp.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
#		*) l=$(netstat -an |grep '^tcp4.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
#		esac ;;
#	AIX)	 l=$(netstat -an |grep '^tcp[^6]       0      0 .*[*0-9]\.'$port' .* LISTEN$') ;;
	SunOS)   l=$(netstat -an -f inet -P dccp |grep '.*[1-9*]\.'$port' .*\*                0 .* LISTEN') ;;
#	HP-UX)   l=$(netstat -an |grep '^tcp        0      0  .*[0-9*]\.'$port' .* LISTEN$') ;;
#	OSF1)    l=$(/usr/sbin/netstat -an |grep '^tcp        0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*LISTEN') ;;
#	CYGWIN*) l=$(netstat -an -p TCP |grep '^  TCP    [0-9.]*:'$port' .* LISTENING') ;;
	*)       l=$(netstat -an |grep -i 'dccp .*[0-9*][:.]'$port' .* listen') ;;
	esac
	if [ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	    \( \( $logic -eq 0 \) -a -z "$l" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	sleep $val_t
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    set ${vx}vx
    return 1
}

# check if a TCP6 port is in use
# exits with 0 when it is not used
checktcp6port () {
    checktcpport $1
}

# wait until a tcp6 listen port is ready
waittcp6port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    local vx=+; case $- in *vx*) set +vx; vx=-; esac	# no tracing here
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	Linux) if [ "$SS" ]; then
		   l=$($SS -6 -n -t -l |grep "^LISTEN .*:$port\>")
		   #l=$($SS -6 -n -t -l |grep "^tcp6* .*:$port\>")
	       else
		   l=$(netstat -an |$GREP_E '^tcp6? .* [0-9a-f:%]*:'$port' .* LISTEN')
	       fi ;;
	FreeBSD) l=$(netstat -an |$GREP_E -i 'tcp(6|46) .*[0-9*][:.]'$port' .* listen') ;;
	NetBSD)  l=$(netstat -an |grep '^tcp6 .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
	OpenBSD) l=$(netstat -an |grep -i 'tcp6 .*[0-9*][:.]'$port' .* listen') ;;
	Darwin)  l=$(netstat -an |$GREP_E '^tcp4?6 +[0-9]+ +[0-9]+ +[0-9a-z:%*]+\.'$port' +[0-9a-z:%*.]+ +LISTEN') ;;
	AIX)	 l=$(netstat -an |grep '^tcp[6 ]       0      0 .*[*0-9]\.'$port' .* LISTEN$') ;;
	SunOS)   l=$(netstat -an -f inet6 -P tcp |grep '.*[1-9*]\.'$port' .*\* [ ]* 0 .* LISTEN') ;;
	#OSF1)    l=$(/usr/sbin/netstat -an |grep '^tcp6       0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*LISTEN') /*?*/;;
	DragonFly)  l=$(netstat -ant |grep '^tcp6 .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]* LISTEN.*') ;;
	*)       l=$(netstat -an |grep -i 'tcp6 .*:'$port' .* listen') ;;
	esac
	if [ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	    \( \( $logic -eq 0 \) -a -z "$l" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	sleep $val_t
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    #echo set ${vx}vx >&2
    set ${vx}vx
    return 1
}

# Check if a UDP6 port is in use
# exits with 0 when it is not used
checkudp6port () {
    checkudpport $1
}

# wait until a UDP6 port is ready
waitudp6port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    local vx=+; case $- in *vx*) set +vx; vx=-; esac	# no tracing here
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	    Linux) if [ "$SS" ]; then
	       # CAUTION!!! ss from iproute2 4.15.0-2ubuntu on 18-04 changes
	       # the output format when writing to pipe
	       l=$($SS -6 -u -l -n |grep "^UNCONN.*:$port\>")
	   else
	       l=$(netstat -an |$GREP_E '^udp6? .* .*[0-9*:%]:'$port' [ ]*:::\*')
	   fi ;;
	FreeBSD) l=$(netstat -an |$GREP_E '^udp(6|46) .*[0-9*]\.'$port' .* \*\.\*') ;;
	NetBSD)  l=$(netstat -an |grep '^udp6 .* \*\.'$port' [ ]* \*\.\*') ;;
	OpenBSD) l=$(netstat -an |grep '^udp6 .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
	Darwin)  l=$(netstat -an |$GREP_E '^udp4?6 +[0-9]+ +[0-9]+ +[0-9a-z:%*]+\.'$port' +[0-9a-z:%*.]+') ;;
	AIX)	 l=$(netstat -an |grep '^udp[6 ]       0      0 .*[*0-9]\.'$port' .* \*\.\*[ ]*$') ;;
	SunOS)   l=$(netstat -an -f inet6 -P udp |grep '.*[1-9*]\.'$port' [ ]*Idle') ;;
	#HP-UX)   l=$(netstat -an |grep '^udp        0      0  .*[0-9*]\.'$port' ') ;;
	#OSF1)    l=$(/usr/sbin/netstat -an |grep '^udp6       0      0  .*[0-9*]\.'$port' [ ]*\*\.\*') ;;
	DragonFly) l=$(netstat -ant |grep '^udp6 .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]*') ;;
	*)       l=$(netstat -an |grep -i 'udp .*[0-9*][:.]'$port' ') ;;
	esac
	if [ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	    \( \( $logic -eq 0 \) -a -z "$l" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	sleep $val_t
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    set ${vx}vx
    return 1
}

# Check if a SCTP6 port is in use
# exits with 0 when it is not used
checksctp6port () {
    checksctpport $1
}

# wait until a sctp6 listen port is ready
# not all (Linux) variants show this in netstat
waitsctp6port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    local vx=+; case $- in *vx*) set +vx; vx=-; esac	# no tracing here
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	Linux) if [ "$SS" ]; then
		   l=$($SS -6 -a -n 2>/dev/null |grep "^sctp .*LISTEN .*:$port\>")
	       else
		   l=$(netstat -an |grep '^sctp[6 ] .* \*:'$port' .* LISTEN')
	       fi ;;
#	FreeBSD) l=$(netstat -an |grep -i 'tcp[46][6 ] .*[0-9*][:.]'$port' .* listen') ;;
#	NetBSD)  l=$(netstat -an |grep '^tcp6 .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
#	OpenBSD) l=$(netstat -an |grep -i 'tcp6 .*[0-9*][:.]'$port' .* listen') ;;
#	AIX)	 l=$(netstat -an |grep '^tcp[6 ]       0      0 .*[*0-9]\.'$port' .* LISTEN$') ;;
	SunOS)   l=$(netstat -an -f inet6 -P sctp |grep '.*[1-9*]\.'$port' .*\* [ ]* 0 .* LISTEN') ;;
#	#OSF1)    l=$(/usr/sbin/netstat -an |grep '^tcp6       0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*LISTEN') /*?*/;;
	*)       l=$(netstat -an |grep -i 'stcp6 .*:'$port' .* listen') ;;
	esac
	if [ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	    \( \( $logic -eq 0 \) -a -z "$l" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	sleep $val_t
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    set ${vx}vx
    return 1
}

# wait until a UDPLITE6 port is ready
waitudplite6port () {
    local port="$1"
    local logic="$2" # 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    local vx=+; case $- in *vx*) set +vx; vx=-; esac # no tracing here
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	    Linux) #if [ "$SS" ]; then
		#l=$($SS -6 -u -l -n |grep "^UNCONN .*:$port\>")
		#else
		if ! netstat -nU >/dev/null 2>&1; then
                    return 0    # speculative
		fi
		l=$(netstat -an |$GREP_E '^udpl6? .* .*[0-9*:%]:'$port' [ ]*:::\*')
		#fi
		;;
	    FreeBSD) l=$(netstat -an |$GREP_E '^udpl(6|46) .*[0-9*]\.'$port' .* \*\.\*') ;;
	    NetBSD)  l=$(netstat -an |grep '^udpl6 .* \*\.'$port' [ ]* \*\.\*') ;;
	    OpenBSD) l=$(netstat -an |grep '^udpl6 .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
	    Darwin)  l=$(netstat -an |$GREP_E '^udpl4?6 +[0-9]+ +[0-9]+ +[0-9a-z:%*]+\.'$port' +[0-9a-z:%*.]+') ;;
	    #AIX)        l=$(netstat -an |grep '^udp[6 ]       0      0 .*[*0-9]\.'$port' .* \*\.\*[ ]*$') ;;
	    #SunOS)   l=$(netstat -an -f inet6 -P udp |grep '.*[1-9*]\.'$port' [ ]*Idle') ;;
	    #HP-UX)   l=$(netstat -an |grep '^udp        0      0  .*[0-9*]\.'$port' ') ;;
	    #OSF1)    l=$(/usr/sbin/netstat -an |grep '^udp6       0      0  .*[0-9*]\.'$port' [ ]*\*\.\*') ;;
	    #DragonFly) l=$(netstat -ant |grep '^udp6 .* .*[0-9*]\.'$port' [ ]* \*\.\* [ ]*') ;;
	    *)       l=$(netstat -an |grep -i 'udp .*[0-9*][:.]'$port' ') ;;
	esac
	if [ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
		\( \( $logic -eq 0 \) -a -z "$l" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	relsleep 1
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    set ${vx}vx
    return 1
}

# wait until a dccp6 listen port is ready
# not all (Linux) variants show this in netstat
waitdccp6port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    local vx=+; case $- in *vx*) set +vx; vx=-; esac	# no tracing here
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case "$UNAME" in
	Linux) if [ "$SS" ]; then
		   l=$($SS -6 -a -n 2>/dev/null |grep "^dccp .*LISTEN .*:$port\>")
	       else
		   l=$(netstat -an |grep '^dccp[6 ] .* [0-9a-f:]*:'$port' .* LISTEN')
	       fi ;;
#	FreeBSD) l=$(netstat -an |grep -i 'tcp[46][6 ] .*[0-9*][:.]'$port' .* listen') ;;
#	NetBSD)  l=$(netstat -an |grep '^tcp6 .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
#	OpenBSD) l=$(netstat -an |grep -i 'tcp6 .*[0-9*][:.]'$port' .* listen') ;;
#	AIX)	 l=$(netstat -an |grep '^tcp[6 ]       0      0 .*[*0-9]\.'$port' .* LISTEN$') ;;
	SunOS)   l=$(netstat -an -f inet6 -P dccp |grep '.*[1-9*]\.'$port' .*\* [ ]* 0 .* LISTEN') ;;
#	#OSF1)    l=$(/usr/sbin/netstat -an |grep '^tcp6       0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*LISTEN') /*?*/;;
	*)       l=$(netstat -an |grep -i 'stcp6 .*:'$port' .* listen') ;;
	esac
	if [ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	    \( \( $logic -eq 0 \) -a -z "$l" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	sleep $val_t
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    set ${vx}vx
    return 1
}

# we need this misleading function name for canonical reasons
waitunixport () {
    waitfile "$1" "$2" "$3"
}

# Not implemented
waitabstractport () {
    relsleep 5
}

# wait until a filesystem entry exists
waitfile () {
    local crit=-e
    case "X$1" in X-*) crit="$1"; shift ;; esac
    local file="$1"
    local logic="$2"	# 0..wait until gone; 1..wait until exists (default);
			# 2..wait until not empty
    local timeout="$3"
    local vx=+; case $- in *vx*) set +vx; vx=-; esac	# no tracing here
    [ "$logic" ] || logic=1
    [ "$logic" -eq 2 ] && crit=-s
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	if [ \( $logic -ne 0 -a $crit "$file" \) -o \
	    \( $logic -eq 0 -a ! $crit "$file" \) ]; then
	    set ${vx}vx
	    return 0
	fi
	sleep $val_t
	timeout=$((timeout-1))
    done

    echo "file $file timed out" >&2
    set ${vx}vx
    return 1
}

# system dependent values
case "$UNAME" in
    SunOS) SOCK_SEQPACKET=6 ;;
    *)     SOCK_SEQPACKET=5 ;;
esac


HAVEDNS=1
if [ "$INTERNET" ]; then
    # No "-s 24" on Solaris
    if ! ping -c 1 "9.9.9.9" >/dev/null 2>&1; then
	echo "$0: Option --internet but no connectivity" >&2
	HAVEDNS=
    elif type nslookup >/dev/null 2>&1; then
	if ! nslookup server-4.dest-unreach.net. |grep '^Name:' >/dev/null 2>&1; then
	    echo "$0: Option --internet but broken DNS (cannot resolve server-4.dest-unreach.net)" >&2
	    HAVEDNS=
	fi
    elif type host >/dev/null 2>&1; then
	if ! host server-4.dest-unreach.net. |grep "has address" >/dev/null 2>&1; then
	    echo "$0: Option --internet but broken DNS (cannot resolve server-4.dest-unreach.net)" >&2
	    HAVEDNS=
	fi
    fi
fi

# generate a test certificate and key
gentestcert () {
    local name="$1"
    if ! [ -f testcert.dh ]; then
	openssl dhparam -out testcert.dh $RSABITS
    fi
    if [ -s $name.key -a -s $name.crt -a -s $name.pem ]; then return; fi
    openssl genrsa $OPENSSL_RAND -out $name.key $RSABITS >/dev/null 2>&1
    #openssl req -new -config $TESTCERT_CONF -key $name.key -x509 -out $name.crt -days 3653 -extensions v3_ca >/dev/null 2>&1
    openssl req -new -config $TESTCERT_CONF -key $name.key -x509 -out $name.crt -days 3653 >/dev/null 2>&1
    cat $name.key $name.crt testcert.dh >$name.pem
}

# generate a test DSA key and certificate
gentestdsacert () {
    local name="$1"
    if [ -s $name.key -a -s $name.crt -a -s $name.pem ]; then return; fi
    openssl dsaparam -out $name-dsa.pem $DSABITS >/dev/null 2>&1
    openssl dhparam -dsaparam -out $name-dh.pem $DSABITS >/dev/null 2>&1
    openssl req -newkey dsa:$name-dsa.pem -keyout $name.key -nodes -x509 -config $TESTCERT_CONF -out $name.crt -days 3653 >/dev/null 2>&1
    cat $name-dsa.pem $name-dh.pem $name.key $name.crt >$name.pem
}

# generate a test EC key and certificate
gentesteccert () {
    local name="$1"
    if [ -s $name.key -a -s $name.crt -a -s $name.pem ]; then return; fi
    openssl ecparam -name secp521r1 -out $name-ec.pem >/dev/null 2>&1
    chmod 0400 $name-ec.pem
    openssl req -newkey ec:$name-ec.pem -keyout $name.key -nodes -x509 -config $TESTCERT_CONF -out $name.crt -days 3653 >/dev/null 2>&1
    cat $name-ec.pem $name.key $name.crt >$name.pem
}

gentestcert6 () {
    local name="$1"
    if [ -s $name.key -a -s $name.crt -a -s $name.pem ]; then return; fi
    cat $TESTCERT_CONF |
    { echo "# automatically generated by $0"; cat; } |
    sed 's/\(commonName\s*=\s*\).*/\1[::1]/' >$TESTCERT6_CONF
    openssl genrsa $OPENSSL_RAND -out $name.key $RSABITS >/dev/null 2>&1
    openssl req -new -config $TESTCERT6_CONF -key $name.key -x509 -out $name.crt -days 3653 >/dev/null 2>&1
    cat $name.key $name.crt >$name.pem
}

# generate a server certificate and key with SubjectAltName
gentestaltcert () {
    local name="$1"
    if ! [ -f testcert.dh ]; then
	openssl dhparam -out testcert.dh $RSABITS
    fi
    if [ -s $name.key -a -s $name.crt -a -s $name.pem ]; then return; fi
    openssl genrsa $OPENSSL_RAND -out $name.key $RSABITS >/dev/null 2>&1
    openssl req -new -config $TESTALT_CONF -key $name.key -x509 -out $name.crt -days 3653 >/dev/null 2>&1
    cat $name.key $name.crt testcert.dh >$name.pem
}

#------------------------------------------------------------------------------
# Begin of functional tests

NAME=UNISTDIO
case "$TESTS " in
*%$N%*|*%functions%*|*%stdio%*|*%$NAME%*)
TEST="$NAME: unidirectional throughput from stdin to stdout"
testecho "$N" "$NAME" "$TEST" "stdin" "stdout" "$opts -u"
esac
N=$((N+1))

#------------------------------------------------------------------------------
# Begin of common tests

NAME=UNPIPESTDIO
case "$TESTS" in
*%$N%*|*%functions%*|*%stdio%*|*%$NAME%*)
TEST="$NAME: stdio with simple echo via internal pipe"
testecho "$N" "$NAME" "$TEST" "stdio" "pipe" "$opts"
esac
N=$((N+1))


NAME=UNPIPESHORT
case "$TESTS" in
*%$N%*|*%functions%*|*%stdio%*|*%$NAME%*)
TEST="$NAME: short form of stdio ('-') with simple echo via internal pipe"
testecho "$N" "$NAME" "$TEST" "-" "pipe" "$opts"
esac
N=$((N+1))


NAME=DUALSTDIO
case "$TESTS" in
*%$N%*|*%functions%*|*%stdio%*|*%$NAME%*)
TEST="$NAME: split form of stdio ('stdin!!stdout') with simple echo via internal pipe"
testecho "$N" "$NAME" "$TEST" "stdin!!stdout" "pipe" "$opts"
esac
N=$((N+1))


NAME=DUALSHORTSTDIO
case "$TESTS" in
*%$N%*|*%functions%*|*%stdio%*|*%$NAME%*)
TEST="$NAME: short split form of stdio ('-!!-') with simple echo via internal pipe"
testecho "$N" "$NAME" "$TEST" "-!!-" "pipe" "$opts"
esac
N=$((N+1))


NAME=DUALFDS
case "$TESTS" in
*%$N%*|*%functions%*|*%fd%*|*%$NAME%*)
TEST="$NAME: file descriptors with simple echo via internal pipe"
testecho "$N" "$NAME" "$TEST" "0!!1" "pipe" "$opts"
esac
N=$((N+1))


NAME=NAMEDPIPE
case "$TESTS" in
*%$N%*|*%functions%*|*%pipe%*|*%$NAME%*)
TEST="$NAME: simple echo via named pipe"
# with MacOS, this test hangs if nonblock is not used. Is an OS bug.
tp="$td/pipe$N"
# note: the nonblock is required by MacOS 10.1(?), otherwise it hangs (OS bug?)
testecho "$N" "$NAME" "$TEST" "" "pipe:$tp,nonblock" "$opts"
esac
N=$((N+1))


NAME=DUALPIPE
case "$TESTS" in
*%$N%*|*%functions%*|*%pipe%*|*%$NAME%*)
TEST="$NAME: simple echo via named pipe, specified twice"
tp="$td/pipe$N"
testecho "$N" "$NAME" "$TEST" "" "pipe:$tp,nonblock!!pipe:$tp" "$opts"
esac
N=$((N+1))


NAME=FILE
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%file%*|*%ignoreeof%*|*%$NAME%*)
TEST="$NAME: simple echo via file"
tf="$td/file$N"
testecho "$N" "$NAME" "$TEST" "" "$tf,ignoreeof!!$tf" "$opts"
esac
N=$((N+1))


NAME=EXECSOCKETPAIR
case "$TESTS" in
*%$N%*|*%functions%*|*%exec%*|*%socketpair%*|*%$NAME%*)
TEST="$NAME: simple echo via exec of cat with socketpair"
testecho "$N" "$NAME" "$TEST" "" "EXEC:$CAT" "$opts"
esac
N=$((N+1))

NAME=SYSTEMSOCKETPAIR
case "$TESTS" in
*%$N%*|*%functions%*|*%system%*|*%socketpair%*|*%$NAME%*)
TEST="$NAME: simple echo via system() of cat with socketpair"
testecho "$N" "$NAME" "$TEST" "" "SYSTEM:$CAT" "$opts" "$val_t"
esac
N=$((N+1))


NAME=EXECPIPES
case "$TESTS" in
*%$N%*|*%functions%*|*%exec%*|*%pipe%*|*%$NAME%*)
TEST="$NAME: simple echo via exec of cat with pipes"
testecho "$N" "$NAME" "$TEST" "" "EXEC:$CAT,pipes" "$opts"
esac
N=$((N+1))

NAME=SYSTEMPIPES
case "$TESTS" in
*%$N%*|*%functions%*|*%system%*|*%pipes%*|*%$NAME%*)
TEST="$NAME: simple echo via system() of cat with pipes"
testecho "$N" "$NAME" "$TEST" "" "SYSTEM:$CAT,pipes" "$opts"
esac
N=$((N+1))


NAME=EXECPTY
case "$TESTS" in
*%$N%*|*%functions%*|*%exec%*|*%pty%*|*%$NAME%*)
TEST="$NAME: simple echo via exec of cat with pseudo terminal"
if ! eval $NUMCOND; then :;
elif ! testfeats pty >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
    cant
else
testecho "$N" "$NAME" "$TEST" "" "EXEC:$CAT,pty,$PTYOPTS,$PTYOPTS2" "$opts"
fi
esac
N=$((N+1))

NAME=SYSTEMPTY
case "$TESTS" in
*%$N%*|*%functions%*|*%system%*|*%pty%*|*%$NAME%*)
TEST="$NAME: simple echo via system() of cat with pseudo terminal"
if ! eval $NUMCOND; then :;
elif ! testfeats pty >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
    cant
else
testecho "$N" "$NAME" "$TEST" "" "SYSTEM:$CAT,pty,$PTYOPTS,$PTYOPTS2" "$opts"
fi
esac
N=$((N+1))


NAME=SYSTEMPIPESFDS
case "$TESTS" in
*%$N%*|*%functions%*|*%system%*|*%pipes%*|*%$NAME%*)
TEST="$NAME: simple echo via system() of cat with pipes, non stdio"
testecho "$N" "$NAME" "$TEST" "" "SYSTEM:$CAT>&9 <&8,pipes,fdin=8,fdout=9" "$opts"
esac
N=$((N+1))


NAME=DUALSYSTEMFDS
case "$TESTS" in
*%$N%*|*%functions%*|*%system%*|*%socketpair%*|*%$NAME%*)
TEST="$NAME: echo via dual system() of cat"
testecho "$N" "$NAME" "$TEST" "SYSTEM:$CAT>&6,fdout=6!!system:$CAT<&7,fdin=7" "" "$opts" "$val_t"
esac
N=$((N+1))


# test: send EOF to exec'ed sub process, let it finish its operation, and
# check if the sub process returns its data before terminating.
NAME=EXECSOCKETPAIRFLUSH
# idea: have socat exec'ing od; send data and EOF, and check if the od'ed data
# arrives.
case "$TESTS" in
*%$N%*|*%functions%*|*%exec%*|*%socketpair%*|*%$NAME%*)
TEST="$NAME: call to od via exec with socketpair"
testod "$N" "$NAME" "$TEST" "" "EXEC:$OD_C" "$opts"
esac
N=$((N+1))

NAME=SYSTEMSOCKETPAIRFLUSH
case "$TESTS" in
*%$N%*|*%functions%*|*%system%*|*%socketpair%*|*%$NAME%*)
TEST="$NAME: call to od via system() with socketpair"
testod "$N" "$NAME" "$TEST" "" "SYSTEM:$OD_C" "$opts" $val_t
esac
N=$((N+1))


NAME=EXECPIPESFLUSH
case "$TESTS" in
*%$N%*|*%functions%*|*%exec%*|*%pipes%*|*%$NAME%*)
TEST="$NAME: call to od via EXEC with pipes"
testod "$N" "$NAME" "$TEST" "" "EXEC:$OD_C,pipes" "$opts"
esac
N=$((N+1))

NAME=SYSTEMPIPESFLUSH
case "$TESTS" in
*%$N%*|*%functions%*|*%system%*|*%pipes%*|*%$NAME%*)
TEST="$NAME: call to od via system() with pipes"
testod "$N" "$NAME" "$TEST" "" "SYSTEM:$OD_C,pipes" "$opts" "$val_t"
esac
N=$((N+1))


## LATER:
#NAME=EXECPTYFLUSH
#case "$TESTS" in
#*%$N%*|*%functions%*|*%exec%*|*%pty%*|*%$NAME%*)
#TEST="$NAME: call to od via exec with pseudo terminal"
#if ! testfeats pty >/dev/null; then
#    $PRINTF "test $F_n $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
#    cant
#else
#testod "$N" "$NAME" "$TEST" "" "exec:$OD_C,pty,$PTYOPTS" "$opts"
#fi
#esac
#N=$((N+1))


## LATER:
#NAME=SYSTEMPTYFLUSH
#case "$TESTS" in
#*%$N%*|*%functions%*|*%system%*|*%pty%*|*%$NAME%*)
#TEST="$NAME: call to od via system() with pseudo terminal"
#if ! testfeats pty >/dev/null; then
#    $PRINTF "test $F_n $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
#    cant
#else
#testod "$N" "$NAME" "$TEST" "" "system:$OD_C,pty,$PTYOPTS" "$opts"
#fi
#esac
#N=$((N+1))


NAME=SYSTEMPIPESFDSFLUSH
case "$TESTS" in
*%$N%*|*%functions%*|*%system%*|*%pipes%*|*%$NAME%*)
TEST="$NAME: call to od via system() with pipes, non stdio"
testod "$N" "$NAME" "$TEST" "" "SYSTEM:$OD_C>&9 <&8,pipes,fdin=8,fdout=9" "$opts" "$val_t"
esac
N=$((N+1))

NAME=DUALSYSTEMFDSFLUSH
case "$TESTS" in
*%$N%*|*%functions%*|*%system%*|*%pipes%*|*%$NAME%*)
TEST="$NAME: call to od via dual system()"
testod "$N" "$NAME" "$TEST" "SYSTEM:$OD_C>&6,fdout=6!!SYSTEM:$CAT<&7,fdin=7" "pipe" "$opts" "$val_t"
esac
N=$((N+1))


NAME=RAWIP4SELF
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%rawip%*|*%root%*|*%$NAME%*)
TEST="$NAME: simple echo via self receiving raw IPv4 protocol"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP4 not available${NORMAL}\n" $N
    cant
elif ! feat=$(testfeats rawip) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}RAWIP not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
    testecho "$N" "$NAME" "$TEST" "" "IP4:127.0.0.1:$IPPROTO" "$opts"
fi
esac
N=$((N+1))

NAME=RAWIPX4SELF
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%rawip%*|*%root%*|*%$NAME%*)
TEST="$NAME: simple echo via self receiving raw IP protocol, v4 by target"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP4 not available${NORMAL}\n" $N
    cant
elif ! feat=$(testfeats rawip) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}RAWIP not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
    testecho "$N" "$NAME" "$TEST" "" "IP:127.0.0.1:$IPPROTO" "$opts"
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=RAWIP6SELF
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%rawip%*|*%root%*|*%$NAME%*)
TEST="$NAME: simple echo via self receiving raw IPv6 protocol"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP6 not available${NORMAL}\n" $N
    cant
elif ! feat=$(testfeats rawip) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}RAWIP not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
    testecho "$N" "$NAME" "$TEST" "" "IP6:[::1]:$IPPROTO" "$opts"
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=RAWIPX6SELF
case "$TESTS" in
*%$N%*|*%functions%*|*%ip%*|*%ip6%*|*%rawip%*|*%rawip6%*|*%root%*|*%$NAME%*)
TEST="$NAME: simple echo via self receiving raw IP protocol, v6 by target"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP6 not available${NORMAL}\n" $N
    cant
elif ! feat=$(testfeats rawip) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}RAWIP not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
    testecho "$N" "$NAME" "$TEST" "" "IP:[::1]:$IPPROTO" "$opts"
fi
esac
N=$((N+1))

newport() {
    _PORT=$((_PORT+1))
     while eval wait${1}port $_PORT 1 0 2>/dev/null; do _PORT=$((_PORT+1)); done
     #while ! eval check${1}port $_PORT 2>/dev/null; do sleep 1; _PORT=$((_PORT+1)); done
     #echo "PORT=$_PORT" >&2
     PORT=$_PORT
}

NAME=TCPSELF
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: echo via self connection of TCP IPv4 socket"
if ! eval $NUMCOND; then :;
elif [ "$UNAME" != Linux ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}only on Linux$NORMAL\n" $N
    cant
else
    newport tcp4 	# provide free port number in $PORT
    #ts="127.0.0.1:$tsl"
    testecho "$N" "$NAME" "$TEST" "" "TCP:$SECONDADDR:$PORT,sp=$PORT,bind=$SECONDADDR,reuseaddr" "$opts"
fi
esac
N=$((N+1))


NAME=UDPSELF
if ! eval $NUMCOND; then :; else
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: echo via self connection of UDP IPv4 socket"
if [ "$UNAME" != Linux ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}only on Linux$NORMAL\n" $N
    cant
else
    newport udp4 	# provide free port number in $PORT
    testecho "$N" "$NAME" "$TEST" "" "UDP:$SECONDADDR:$PORT,sp=$PORT,bind=$SECONDADDR" "$opts"
fi
esac
fi # NUMCOND
N=$((N+1))


NAME=UDP6SELF
case "$TESTS" in
*%$N%*|*%functions%*|*%udp%*|*%udp6%*|*%ip6%*|*%$NAME%*)
TEST="$NAME: echo via self connection of UDP IPv6 socket"
if ! eval $NUMCOND; then :;
elif [ "$UNAME" != Linux ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}only on Linux${NORMAL}\n" $N
    cant
elif ! testfeats udp ip6 >/dev/null || ! runsudp6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
    cant
else
    tf="$td/file$N"
    newport udp6 	# provide free port number in $PORT
    testecho "$N" "$NAME" "$TEST" "" "UDP6:[::1]:$PORT,sp=$PORT,bind=[::1]" "$opts"
fi
esac
N=$((N+1))


NAME=DUALUDPSELF
if ! eval $NUMCOND; then :; else
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: echo via two unidirectional UDP IPv4 sockets"
tf="$td/file$N"
newport udp4; PORT1=$PORT 	# get free port
newport udp4; PORT2=$PORT 	# get free port
testecho "$N" "$NAME" "$TEST" "" "UDP:127.0.0.1:$PORT2,sp=$PORT1!!UDP:127.0.0.1:$PORT1,sp=$PORT2" "$opts"
esac
fi # NUMCOND
N=$((N+1))


#function testdual {
#    local
#}


NAME=UNIXSTREAM
if ! eval $NUMCOND; then :; else
case "$TESTS" in
*%$N%*|*%functions%*|*%unix%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to UNIX domain socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
ts="$td/test$N.socket"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts UNIX-LISTEN:$ts PIPE"
CMD2="$TRACE $SOCAT $opts -!!- UNIX-CONNECT:$ts"
printf "test $F_n $TEST... " $N
$CMD1 </dev/null >$tf 2>"${te}1" &
bg=$!	# background process id
waitfile "$ts"
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2=$?
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   cat "${te}1"
   echo "$CMD2"
   echo "rc=$rc2"
   cat "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED: diff:\n"
   cat "$tdiff"
   echo "$CMD1 &"
   cat "${te}1"
   echo "$CMD2"
   cat "${te}2"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $bg 2>/dev/null
esac
fi # NUMCOND
N=$((N+1))


NAME=TCP4
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to TCP V4 socket"
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds "" "" "" \
			 "IP4 TCP LISTEN STDIO PIPE" \
			 "TCP4-LISTEN PIPE STDIN STDOUT TCP4" \
			 "so-reuseaddr" \
			 "tcp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp4; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts TCP4-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP4:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1=$!
waittcp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
	$PRINTF "$FAILED\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
	$PRINTF "$FAILED\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
fi
kill $pid1 2>/dev/null
wait
fi ;; # NUMCOND, checkconds
esac
N=$((N+1))


#et -xv
NAME=TCP6
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to TCP V6 socket"
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats IP6 TCP LISTEN STDIO PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs - TCP6-LISTEN PIPE STDIN STDOUT TCP6-CONNECT); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions so-reuseaddr ) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp6; tsl=$PORT
ts="[::1]:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts TCP6-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP6:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waittcp6port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: diff:\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    echo diff:
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
kill $pid 2>/dev/null
fi
esac
N=$((N+1))
#set +vx


# Test if TCP client with IPv4 address connects to IPv4 port
NAME=TCPX4
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to TCP socket, v4 by target"
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO PIPE IP4 TCP LISTEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs TCP TCP-LISTEN STDIN STDOUT PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions pf) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp4; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts TCP-LISTEN:$tsl,pf=ip4,$REUSEADDR PIPE"
CMD1="$TRACE $SOCAT $opts STDIN!!STDOUT TCP:$ts"
printf "test $F_n $TEST... " $N
$CMD0 >"$tf" 2>"${te}0" &
pid=$!	# background process id
waittcp4port $tsl 1
echo "$da" |$CMD1 >>"$tf" 2>>"${te}1"
if [ $? -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: diff:\n"
    cat "$tdiff"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
fi
kill $pid 2>/dev/null
fi
esac
N=$((N+1))


# Test if TCP client with IPv6 address connects to IPv6 port
NAME=TCPX6
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to TCP socket, v6 by target"
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO PIPE IP6 TCP LISTEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs TCP TCP-LISTEN STDIN STDOUT PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions pf) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
elif ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp6; tsl=$PORT
ts="[::1]:$tsl"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts TCP-LISTEN:$tsl,pf=ip6,$REUSEADDR PIPE"
CMD1="$TRACE $SOCAT $opts STDIN!!STDOUT TCP:$ts"
printf "test $F_n $TEST... " $N
$CMD0 >"$tf" 2>"${te}0" &
pid=$!	# background process id
waittcp6port $tsl 1
echo "$da" |$CMD1 >>"$tf" 2>>"${te}1"
if [ $? -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: diff:\n"
    cat "$tdiff"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
fi
kill $pid 2>/dev/null
fi
esac
N=$((N+1))

# TCP6-LISTEN may also listen for IPv4 connections. Test if option
# ipv6-v6only=0 shows this behaviour.
# On OpenBSD-7.2 ipv6-v6only=0 gives "Invalid argument"
NAME=IPV6ONLY0
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: option ipv6-v6only=0 listens on IPv4"
# create a listening TCP6 socket and try to connect to the port using TCP4
if ! eval $NUMCOND; then :;
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP4 not available${NORMAL}\n" $N
    cant
elif ! testfeats ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions ipv6-v6only); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp6; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts TCP6-LISTEN:$tsl,ipv6-v6only=0,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP4:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waittcp6port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1" "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED: diff:\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
fi
esac
N=$((N+1))

#set -vx
# TCP6-LISTEN may also listen for IPv4 connections. Test if option
# ipv6-v6only=1 turns off this behaviour.
NAME=IPV6ONLY1
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: option ipv6-v6only=1 does not listen on IPv4"
if ! eval $NUMCOND; then :;
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP4 not available${NORMAL}\n" $N
    cant
elif ! testfeats ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP6 not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions ipv6-v6only); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp6; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts TCP6-LISTEN:$tsl,ipv6-v6only=1,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts stdin!!stdout TCP4:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waittcp6port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -eq 0 ]; then
   $PRINTF "$FAILED:\n"
   cat "${te}1" "${te}2"
   failed
elif echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED:\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   failed
else
   $PRINTF "$OK\n"
   ok
fi
kill $pid; wait
wait
fi
esac
N=$((N+1))
#set +vx

NAME=ENV_LISTEN_4
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: env SOCAT_DEFAULT_LISTEN_IP for IPv4 preference on listen"
if ! eval $NUMCOND; then :;
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP4 not available${NORMAL}\n" $N
    cant
elif ! testfeats ip6 >/dev/null || ! runstcp6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions ipv6-v6only); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp4; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts TCP-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP4:$ts"
printf "test $F_n $TEST... " $N
SOCAT_DEFAULT_LISTEN_IP=4 $CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waittcp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1" "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED: diff:\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null; wait
fi
esac
N=$((N+1))

NAME=ENV_LISTEN_6
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: env SOCAT_DEFAULT_LISTEN_IP for IPv6 preference on listen"
if ! eval $NUMCOND; then :;
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP4 not available${NORMAL}\n" $N
    cant
elif ! testfeats ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp6; tsl=$PORT
ts="[::1]:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts TCP-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP6:$ts"
printf "test $F_n $TEST... " $N
SOCAT_DEFAULT_LISTEN_IP=6 $CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waittcp6port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "SOCAT_DEFAULT_LISTEN_IP=6 $CMD1 &"
   cat "${te}1" >&2
   echo "$CMD2"
   cat "${te}2" >&2
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED (diff):\n"
   echo "SOCAT_DEFAULT_LISTEN_IP=6 $CMD1 &"
   cat "${te}1" >&2
   echo "$CMD2"
   cat "${te}2" >&2
   echo "// diff:" >&2
   cat "$tdiff" >&2
   failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
kill $pid 2>/dev/null; wait
fi
esac
N=$((N+1))

NAME=LISTEN_OPTION_4
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: option -4 for IPv4 preference on listen"
if ! eval $NUMCOND; then :;
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP4 not available${NORMAL}\n" $N
    cant
elif ! testfeats ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP6 not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions ipv6-v6only); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp4; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -4 TCP-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP4:$ts"
printf "test $F_n $TEST... " $N
SOCAT_DEFAULT_LISTEN_IP=6 $CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waittcp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1" "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED: diff:\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null; wait
fi
esac
N=$((N+1))

NAME=LISTEN_OPTION_6
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: option -6 for IPv6 preference on listen"
if ! eval $NUMCOND; then :;
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP4 not available${NORMAL}\n" $N
    cant
elif ! testfeats ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp6; tsl=$PORT
ts="[::1]:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -6 TCP-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP6:$ts"
printf "test $F_n $TEST... " $N
SOCAT_DEFAULT_LISTEN_IP=4 $CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waittcp6port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1" "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED: diff:\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null; wait
wait
fi # feats
esac
N=$((N+1))

NAME=LISTEN_PF_IP4
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: pf=4 overrides option -6 on listen"
if ! eval $NUMCOND; then :;
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP4 not available${NORMAL}\n" $N
    cant
elif ! testfeats ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP6 not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions ipv6-v6only); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp4; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -6 TCP-LISTEN:$tsl,pf=ip4,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP4:$ts"
printf "test $F_n $TEST... " $N
SOCAT_DEFAULT_LISTEN_IP=6 $CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waittcp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1" "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED: diff:\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null; wait
fi
esac
N=$((N+1))

NAME=LISTEN_PF_IP6
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: pf=6 overrides option -4 on listen"
if ! eval $NUMCOND; then :;
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP4 not available${NORMAL}\n" $N
    cant
elif ! testfeats ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp6; tsl=$PORT
ts="[::1]:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -4 TCP-LISTEN:$tsl,pf=ip6,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP6:$ts"
printf "test $F_n $TEST... " $N
SOCAT_DEFAULT_LISTEN_IP=4 $CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waittcp6port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1" "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED: diff:\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null; wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=UDP4STREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%udp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to UDP V4 socket"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; tsl=$PORT
ts="$LOCALHOST:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts UDP4-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts - UDP4:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1=$!
waitudp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2=$?
kill $pid1 2>/dev/null; wait
if [ $rc2 -ne 0 ]; then
   $PRINTF "$FAILED (rc2=$rc2)\n"
   echo "$CMD1 &"
   cat "${te}1" >&2
   echo "$CMD2"
   cat "${te}2" >&2
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED (diff)\n"
   echo "$CMD1 &"
   cat "${te}1" >&2
   echo "$CMD2"
   cat "${te}2" >&2
   echo "// diff:" >&2
   cat "$tdiff" >&2
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=UDP6STREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%udp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to UDP V6 socket"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp6; tsl=$PORT
ts="$LOCALHOST6:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts UDP6-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts - UDP6:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1=$!
waitudp6port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2=$?
kill $pid1 2>/dev/null; wait
if [ $rc2 -ne 0 ]; then
   $PRINTF "$FAILED (rc2=$rc2)\n"
   echo "$CMD1 &"
   cat "${te}1" >&2
   echo "$CMD2"
   cat "${te}2" >&2
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED (diff)\n"
   echo "$CMD1 &"
   cat "${te}1" >&2
   echo "$CMD2"
   cat "${te}2" >&2
   echo "// diff:" >&2
   cat "$tdiff" >&2
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
fi ;; # ! testfeats
esac
N=$((N+1))


NAME=GOPENFILE
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%gopen%*|*%file%*|*%ignoreeof%*|*%$NAME%*)
TEST="$NAME: file opening with gopen"
if ! eval $NUMCOND; then :; else
tf1="$td/test$N.1.stdout"
tf2="$td/test$N.2.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
echo "$da" >$tf1
CMD="$TRACE $SOCAT $opts $tf1!!/dev/null /dev/null,ignoreeof!!-"
printf "test $F_n $TEST... " $N
$CMD >"$tf2" 2>"$te"
if [ $? -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD"
    cat "$te"
   failed
elif ! diff "$tf1" "$tf2" >"$tdiff"; then
    $PRINTF "$FAILED: diff:\n"
    cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi # NUMCOND
esac
N=$((N+1))


NAME=GOPENPIPE
case "$TESTS" in
*%$N%*|*%functions%*|*%gopen%*|*%pipe%*|*%ignoreeof%*|*%$NAME%*)
TEST="$NAME: pipe opening with gopen for reading"
if ! eval $NUMCOND; then :; else
tp="$td/pipe$N"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT $opts $tp!!/dev/null /dev/null,ignoreeof!!$tf"
printf "test $F_n $TEST... " $N
#mknod $tp p	# no mknod p on FreeBSD
mkfifo $tp
$CMD >$tf 2>"$te" &
#($CMD >$tf 2>"$te" || rm -f "$tp") 2>/dev/null &
bg=$!	# background process id
#relsleep 1
waitfile "$tp"
if [ ! -p "$tp" ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD"
    cat "$te"
    failed
else
#echo "$da" >"$tp"	# might hang forever
echo "$da" >"$tp" & export pid=$!; (relsleep 1; kill $pid 2>/dev/null) &
# Solaris needs more time:
relsleep 1
kill "$bg" 2>/dev/null; wait
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    if [ -s "$te" ]; then
	$PRINTF "$FAILED: $TRACE $SOCAT:\n"
	echo "$CMD"
	cat "$te"
    else
	$PRINTF "$FAILED: diff:\n"
	cat "$tdiff"
    fi
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi
wait
fi # NUMCOND
esac
N=$((N+1))


NAME=GOPENUNIXSTREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%gopen%*|*%unix%*|*%listen%*|*%$NAME%*)
TEST="$NAME: GOPEN on UNIX stream socket"
if ! eval $NUMCOND; then :; else
ts="$td/test$N.socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da1="test$N $(date) $RANDOM"
#establish a listening unix socket in background
SRV="$TRACE $SOCAT $opts -lpserver UNIX-LISTEN:\"$ts\" PIPE"
#make a connection
CMD="$TRACE $SOCAT $opts - $ts"
$PRINTF "test $F_n $TEST... " $N
eval "$SRV 2>${te}s &"
pids=$!
waitfile "$ts"
echo "$da1" |eval "$CMD" >"${tf}1" 2>"${te}1"
if [ $? -ne 0 ]; then
    kill "$pids" 2>/dev/null
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    cat "${te}s"
    echo "$CMD"
    cat "${te}1"
    failed
elif ! echo "$da1" |diff - "${tf}1" >"$tdiff"; then
    kill "$pids" 2>/dev/null
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    cat "${te}s"
    echo "$CMD"
    cat "${te}1"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi # !(rc -ne 0)
wait
fi # NUMCOND
esac
N=$((N+1))

NAME=GOPENUNIXSEQPACKET
case "$TESTS" in
*%$N%*|*%functions%*|*%gopen%*|*%unix%*|*%listen%*|*%seqpacket%*|*%$NAME%*)
TEST="$NAME: GOPEN on UNIX seqpacket socket"
if ! eval $NUMCOND; then :; else
ts="$td/test$N.socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da1="test$N $(date) $RANDOM"
#establish a listening unix socket in background
SRV="$TRACE $SOCAT $opts -lpserver UNIX-LISTEN:\"$ts\",so-type=$SOCK_SEQPACKET PIPE"
#make a connection
CMD="$TRACE $SOCAT $opts - $ts"
$PRINTF "test $F_n $TEST... " $N
eval "$SRV 2>${te}s &"
pids=$!
waitfile "$ts"
echo "$da1" |eval "$CMD" >"${tf}1" 2>"${te}1"
if [ $? -ne 0 ]; then
    kill "$pids" 2>/dev/null
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    cat "${te}s"
    echo "$CMD"
    cat "${te}1"
    failed
elif ! echo "$da1" |diff - "${tf}1" >"$tdiff"; then
    kill "$pids" 2>/dev/null
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    cat "${te}s"
    echo "$CMD"
    cat "${te}1"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi # !(rc -ne 0)
wait
fi # NUMCOND
esac
N=$((N+1))


NAME=GOPENUNIXDGRAM
case "$TESTS" in
*%$N%*|*%functions%*|*%gopen%*|*%unix%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: GOPEN on UNIX datagram socket"
if ! eval $NUMCOND; then :; else
ts="$td/test$N.socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da1="test$N $(date) $RANDOM"
#establish a receiving unix socket in background
SRV="$TRACE $SOCAT $opts -u -lpserver UNIX-RECV:\"$ts\" file:\"$tf\",create"
#make a connection
CMD="$TRACE $SOCAT $opts -u - $ts"
$PRINTF "test $F_n $TEST... " $N
eval "$SRV 2>${te}s &"
pids=$!
waitfile "$ts"
echo "$da1" |eval "$CMD" 2>"${te}1"
waitfile -s "$tf"
if [ $? -ne 0 ]; then
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    cat "${te}s"
    echo "$CMD"
    cat "${te}1"
    failed
elif ! echo "$da1" |diff - "${tf}" >"$tdiff"; then
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    cat "${te}s"
    echo "$CMD"
    cat "${te}1"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi # !(rc -ne 0)
kill "$pids" 2>/dev/null; wait
fi ;; # NUMCOND
esac
N=$((N+1))


# Test the ignoreeof option in forward (left to right) direction
NAME=IGNOREEOF
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%ignoreeof%*|*%$NAME%*)
TEST="$NAME: ignoreeof on file"
# Let Socat read from an empty file, this would terminate immediately due to
# EOF. Wait for more than one second, then append data to the file; when Socat
# transfers this data the test succeeded.
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "STDIO FILE" \
		  "STDOUT FILE" \
		  "ignoreeof" \
		  "" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
ti="$td/test$N.file"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
# Note: the bug in 1.8.0.0 and 1.8.0.1 let Socat terminate in unidirectional
# mode after 1s, in bidirectional mode with traffic in reverse direction
# (var wasaction) immediately
CMD="$TRACE $SOCAT $opts -u FILE:\"$ti\",ignoreeof -"
printf "test $F_n $TEST... " $N
touch "$ti"
$CMD >"$tf" 2>"$te" &
bg=$!
# Up to 1.8.0.1 this sleep was 0.1 and thus the test said OK despite the bug
sleep 1.1
echo "$da" >>"$ti"
sleep 1
kill $bg 2>/dev/null; wait
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD"
    cat "$te" >&2
    echo "// diff:" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$VERBOSE" ]; then echo "$CMD"; fi
    if [ -n "$DEBUG" ]; then cat "$te" >&2; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))

# Test the ignoreeof option in reverse (right to left) direction
NAME=IGNOREEOF_REV
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%ignoreeof%*|*%$NAME%*)
TEST="$NAME: ignoreeof on file right-to-left"
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "STDIO FILE" \
		  "STDOUT FILE" \
		  "ignoreeof" \
		  "" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
# Let Socat read from an empty file, this would terminate immediately due to
# EOF. Wait for more than one second, then append data to the file; when Socat
# transfers this data the test succeeded.
ti="$td/test$N.file"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$SOCAT $opts -U - FILE:\"$ti\",ignoreeof"
printf "test $F_n $TEST... " $N
touch "$ti"
$CMD >"$tf" 2>"$te" &
bg=$!
sleep 1.1
echo "$da" >>"$ti"
sleep 1
kill $bg 2>/dev/null; wait
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD"
    cat "$te" >&2
    echo "// diff:" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$VERBOSE" ]; then echo "$CMD"; fi
    if [ -n "$DEBUG" ]; then cat "$te" >&2; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=EXECIGNOREEOF
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%ignoreeof%*|*%$NAME%*)
TEST="$NAME: exec against address with ignoreeof"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
# remark: diagnostics to null, no good style
CMD="$TRACE $SOCAT $opts -lf /dev/null EXEC:$TRUE /dev/null,ignoreeof"
printf "test $F_n $TEST... " $N
$CMD >"$tf" 2>"$te"
if [ -s "$te" ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD"
    cat "$te" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$VERBOSE" ]; then echo "$CMD"; fi
    if [ -n "$DEBUG" ]; then cat "$te" >&2; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=FAKEPTY
case "$TESTS" in
*%$N%*|*%functions%*|*%pty%*|*%$NAME%*)
TEST="$NAME: generation of pty for other processes"
if ! eval $NUMCOND; then :;
elif ! testfeats pty >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
    cant
else
tt="$td/pty$N"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts PTY,$PTYOPTS,link=$tt PIPE"
CMD2="$TRACE $SOCAT $opts - $tt,$PTYOPTS2"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
pid=$!	# background process id
waitfile "$tt"
# this hangs on HP-UX, so we use a timeout
(echo "$da"; sleep 1) |$CMD2 >$tf 2>"${te}2" &
pid2=$!
#sleep 5 && kill $rc2 2>/dev/null &
wait $pid2
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    sleep 1
    echo "$CMD2"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null; wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=O_TRUNC
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: option o-trunc"
if ! eval $NUMCOND; then :; else
ff="$td/test$N.file"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT -u $opts - open:$ff,append,o-trunc"
printf "test $F_n $TEST... " $N
rm -f $ff; $ECHO "prefix-\c" >$ff
echo "$da" |$CMD >$tf 2>"$te"
rc0=$?
if ! [ $rc0 = 0 ] ||
    ! echo "$da" |diff - $ff >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD"
    cat "$te"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=FTRUNCATE
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: option ftruncate"
if ! eval $NUMCOND; then :; else
ff="$td/test$N.file"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT -u $opts - open:$ff,append,ftruncate=0"
printf "test $F_n $TEST... " $N
rm -f $ff; $ECHO "prefix-\c" >$ff
if ! echo "$da" |$CMD >$tf 2>"$te" ||
    ! echo "$da" |diff - $ff >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD"
    cat "$te"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=RIGHTTOLEFT
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: unidirectional throughput from stdin to stdout, right to left"
testecho "$N" "$NAME" "$TEST" "stdout" "stdin" "$opts -U"
esac
N=$((N+1))


# I cannot remember the clou of this test, seems rather useless
NAME=CHILDDEFAULT
case "$TESTS" in
*%$N%*|*%functions%*|*%procan%*|*%$NAME%*)
if ! eval $NUMCOND; then :
elif ! F=$(testfeats STDIO EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
else
TEST="$NAME: child process default properties"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
CMD="$TRACE $SOCAT $opts -u EXEC:$PROCAN -"
printf "test $F_n $TEST... " $N
$CMD >$tf 2>$te
MYPID=`expr "\`grep "process id =" $tf\`" : '[^0-9]*\([0-9]*\).*'`
MYPPID=`expr "\`grep "process parent id =" $tf\`" : '[^0-9]*\([0-9]*\).*'`
MYPGID=`expr "\`grep "process group id =" $tf\`" : '[^0-9]*\([0-9]*\).*'`
MYSID=`expr "\`grep "process session id =" $tf\`" : '[^0-9]*\([0-9]*\).*'`
#echo "PID=$MYPID, PPID=$MYPPID, PGID=$MYPGID, SID=$MYSID"
#if [ "$MYPID" = "$MYPPID" -o "$MYPID" = "$MYPGID" -o "$MYPID" = "$MYSID" -o \
#     "$MYPPID" = "$MYPGID" -o "$MYPPID" = "$MYSID" -o "$MYPGID" = "$MYSID" ];
if [ "$MYPID" = "$MYPPID" ];
then
    $PRINTF "$FAILED:\n"
    echo "$CMD"
    cat "$te" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD"; fi
    if [ "$DEBUG" ];   then cat "${te}" >&2; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=CHILDSETSID
case "$TESTS" in
*%$N%*|*%functions%*|*%procan%*|*%$NAME%*)
TEST="$NAME: child process with setsid"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
CMD="$TRACE $SOCAT $opts -u exec:$PROCAN,setsid -"
printf "test $F_n $TEST... " $N
$CMD >$tf 2>$te
MYPID=`grep "process id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYPPID=`grep "process parent id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYPGID=`grep "process group id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYSID=`grep "process session id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
#$ECHO "\nPID=$MYPID, PPID=$MYPPID, PGID=$MYPGID, SID=$MYSID"
# PID, PGID, and  SID must be the same
if [ "$MYPID" = "$MYPPID" -o \
     "$MYPID" != "$MYPGID" -o "$MYPID" != "$MYSID" ];
then
    $PRINTF "$FAILED\n"
    echo "$CMD"
    cat "$te"
    failed
else
    $PRINTF "$OK\n"
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=MAINSETSID
case "$TESTS" in
*%$N%*|*%functions%*|*%stdio%*|*%exec%*|*%procan%*|*%$NAME%*)
TEST="$NAME: main process with setsid"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
CMD="$TRACE $SOCAT $opts -U -,setsid EXEC:$PROCAN"
printf "test $F_n $TEST... " $N
$CMD >$tf 2>$te
MYPID=`grep "process id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYPPID=`grep "process parent id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYPGID=`grep "process group id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYSID=`grep "process session id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
#$ECHO "\nPID=$MYPID, PPID=$MYPPID, PGID=$MYPGID, SID=$MYSID"
# PPID, PGID, and  SID must be the same
if [ "$MYPID" = "$MYPPID" -o \
     "$MYPPID" != "$MYPGID" -o "$MYPPID" != "$MYSID" ];
then
    $PRINTF "$FAILED\n"
    echo "$CMD"
    cat "$te"
    failed
else
    $PRINTF "$OK\n"
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=OPENSSL_TCP4
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%$NAME%*)
TEST="$NAME: openssl connect"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! type openssl >/dev/null 2>&1; then
    $PRINTF "test $F_n $TEST... ${YELLOW}openssl executable not available${NORMAL}\n" $N
    cant
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4 	# provide free port number in $PORT
init_openssl_s_server
CMD2="$TRACE $SOCAT $opts exec:'openssl s_server $OPENSSL_S_SERVER_4 -accept "$PORT" -quiet -cert testsrv.pem' pipe"
CMD="$TRACE $SOCAT $opts - openssl:$LOCALHOST:$PORT,pf=ip4,verify=0,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
eval "$CMD2 2>\"${te}1\" &"
pid=$!	# background process id
# this might timeout when openssl opens tcp46 port like " :::$PORT"
waittcp4port $PORT
#echo "$da" |$CMD >$tf 2>"${te}2"
#note: with about OpenSSL 1.1 s_server lost the half close feature, thus:
(echo "$da"; sleep 0.1) |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null; wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=OPENSSLLISTEN_TCP4
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: openssl listen"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4 	# provide free port number in $PORT
CMD2="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,$SOCAT_EGD,cert=testsrv.crt,key=testsrv.key,verify=0 pipe"
CMD="$TRACE $SOCAT $opts - OPENSSL:$LOCALHOST:$PORT,pf=ip4,verify=0,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
eval "$CMD2 2>\"${te}1\" &"
pid=$!	# background process id
waittcp4port $PORT
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSLLISTEN_TCP6
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%listen%*|*%$NAME%*)
TEST="$NAME: openssl listen"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv6 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp6 	# provide free port number in $PORT
CMD2="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip6,$REUSEADDR,$SOCAT_EGD,cert=testsrv.crt,key=testsrv.key,verify=0 pipe"
CMD="$TRACE $SOCAT $opts - OPENSSL:$LOCALHOST6:$PORT,verify=0,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
eval "$CMD2 2>\"${te}1\" &"
pid=$!	# background process id
waittcp6port $PORT
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


while read NAMEKEYW FEAT RUNS TESTTMPL PEERTMPL WAITTMPL; do
if [ -z "$NAMEKEYW" ] || [[ "$NAMEKEYW" == \#* ]]; then continue; fi

export ts="$td/test$N.socket"
case $RUNS in tcp4|tcp6) newport $RUNS;; esac
WAITTMPL="$(echo "$WAITTMPL" |sed -e 's/\040/ /g')"
TESTADDR=$(eval echo $TESTTMPL)
PEERADDR=$(eval echo $PEERTMPL)
WAITCMD=$(eval echo $WAITTMPL)
TESTKEYW=${TESTADDR%%:*}
feat=$(tolower $FEAT)

# does our address implementation support halfclose?
NAME=${NAMEKEYW}_HALFCLOSE
case "$TESTS" in
*%$N%*|*%functions%*|*%$feat%*|*%socket%*|*%halfclose%*|*%listen%*|*%$NAME%*)
TEST="$NAME: $TESTKEYW half close"
# have a "peer" socat "peer" that executes "$OD_C" and see if EOF on the
# connecting socat  brings the result of od
if ! eval $NUMCOND; then :;
elif [ "$FEAT" != ',' ] && ! testfeats "$FEAT" >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $FEAT not configured${NORMAL}\n" $N
    cant
elif ! runs$RUNS >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$RUNS not available on host${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
case $RUNS in tcp4|tcp6) newport $RUNS;; esac
CMD2="$TRACE $SOCAT $opts \"$PEERADDR\" EXEC:'$OD_C'"
CMD="$TRACE $SOCAT -T1 $opts -t 1 - $TESTADDR"
printf "test $F_n $TEST... " $N
eval "$CMD2 2>\"${te}2\" &"
pid2=$!	# background process id
$WAITCMD
echo "$da" |$CMD >$tf 2>"${te}"
kill $pid2 2>/dev/null
wait
if ! echo "$da" |$OD_C |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD2 &"
    cat "${te}2"
    echo "$CMD"
    cat "${te}"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	echo "  $CMD2 &"
	echo "  $CMD"
    fi
   if [ -n "$debug" ]; then cat "${te}2" "${te}"; fi
   ok
fi
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))

done <<<"
UNIXCONNECT      ,       unix UNIX-CONNECT:\$ts UNIX-LISTEN:\$ts waitfile\040\$ts
UNIXCLIENT       ,       unix UNIX-CLIENT:\$ts UNIX-LISTEN:\$ts waitfile\040\$ts
GOPEN_UNIXSTREAM ,       unix GOPEN:\$ts UNIX-LISTEN:\$ts waitfile\040\$ts
UNIXLISTEN       ,       unix UNIX-LISTEN:\$ts UNIX-CONNECT:\$ts,retry=3 sleep\040\1
TCP4CONNECT      ,       tcp4 TCP4-CONNECT:\$LOCALHOST:\$PORT TCP4-LISTEN:\$PORT,$REUSEADDR waittcp4port\040\$PORT
TCP4LISTEN       ,       tcp4 TCP4-LISTEN:\$PORT,$REUSEADDR TCP4-CONNECT:\$LOCALHOST:\$PORT,retry=3
TCP6CONNECT      ,       tcp6 TCP6-CONNECT:\$LOCALHOST6:\$PORT TCP6-LISTEN:\$PORT,$REUSEADDR waittcp6port\040\$PORT
TCP6LISTEN       ,       tcp6 TCP6-LISTEN:\$PORT,$REUSEADDR TCP6-CONNECT:\$LOCALHOST6:\$PORT,retry=3
OPENSSL4CLIENT   OPENSSL tcp4 OPENSSL:\$LOCALHOST:\$PORT,pf=ip4,verify=0 OPENSSL-LISTEN:\$PORT,pf=ip4,$REUSEADDR,$SOCAT_EGD,cert=testsrv.crt,key=testsrv.key,verify=0 waittcp4port\040\$PORT
OPENSSL4SERVER   OPENSSL tcp4 OPENSSL-LISTEN:\$PORT,pf=ip4,$SOCAT_EGD,cert=testsrv.crt,key=testsrv.key,verify=0 OPENSSL:\$LOCALHOST:\$PORT,pf=ip4,$REUSEADDR,verify=0,retry=3
OPENSSL6CLIENT   OPENSSL tcp6 OPENSSL:\$LOCALHOST6:\$PORT,pf=ip6,verify=0 OPENSSL-LISTEN:\$PORT,pf=ip6,$REUSEADDR,$SOCAT_EGD,cert=testsrv.crt,key=testsrv.key,verify=0 waittcp6port\040\$PORT
OPENSSL6SERVER   OPENSSL tcp6 OPENSSL-LISTEN:\$PORT,pf=ip6,$SOCAT_EGD,cert=testsrv.crt,key=testsrv.key,verify=0 OPENSSL:\$LOCALHOST6:\$PORT,pf=ip6,$REUSEADDR,verify=0,retry=3
"


NAME=OPENSSL_SERVERAUTH
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL server authentication (hostname)"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
gentestcert testcli
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4 	# provide free port number in $PORT
CMD0="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,$SOCAT_EGD,cert=testsrv.crt,key=testsrv.key,verify=0 pipe"
CMD1="$TRACE $SOCAT $opts - OPENSSL:$LOCALHOST:$PORT,pf=ip4,verify=1,cafile=testsrv.crt,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
eval "$CMD0 2>\"${te}0\" &"
pid=$!	# background process id
waittcp4port $PORT
echo "$da" |$CMD1 >$tf 2>"${te}1"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1"
    cat "${te}1"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSL_CLIENTAUTH
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: openssl client authentication"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
gentestcert testcli
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4 	# provide free port number in $PORT
CMD2="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,verify=1,cert=testsrv.crt,key=testsrv.key,cafile=testcli.crt,$SOCAT_EGD PIPE"
CMD="$TRACE $SOCAT $opts - OPENSSL:$LOCALHOST:$PORT,pf=ip4,verify=0,cert=testcli.crt,key=testcli.key,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
eval "$CMD2 2>\"${te}1\" &"
pid=$!	# background process id
waittcp4port $PORT
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSL_FIPS_BOTHAUTH
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%fips%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL+FIPS client and server authentication"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
elif ! testoptions fips >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL/FIPS not available${NORMAL}\n" $N
    cant
else
OPENSSL_FIPS=1 gentestcert testsrvfips
OPENSSL_FIPS=1 gentestcert testclifips
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4 	# provide free port number in $PORT
CMD2="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,$REUSEADDR,fips,$SOCAT_EGD,cert=testsrvfips.crt,key=testsrvfips.key,cafile=testclifips.crt pipe"
CMD="$TRACE $SOCAT $opts - OPENSSL:$LOCALHOST:$PORT,fips,verify=1,cert=testclifips.crt,key=testclifips.key,cafile=testsrvfips.crt,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
eval "$CMD2 2>\"${te}1\" &"
pid=$!	# background process id
waittcp4port $PORT
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=OPENSSL_COMPRESS
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL compression"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
elif ! testoptions openssl-compress >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL compression option not available${NORMAL}\n" $N
    cant
else
    gentestcert testsrv
    printf "test $F_n $TEST... " $N
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    success=yes
    for srccompr in '' compress=auto compress=none; do
        for dstcompr in '' compress=auto compress=none; do
	    newport tcp4 	# provide free port number in $PORT
            CMD2="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,$SOCAT_EGD,cert=testsrv.crt,key=testsrv.key,verify=0,$dstcompr PIPE"
            CMD="$TRACE $SOCAT $opts - OPENSSL:$LOCALHOST:$PORT,pf=ip4,verify=0,$SOCAT_EGD,$srccompr"
            eval "$CMD2 2>\"${te}1\" &"
            pid=$! # background process id
            waittcp4port $PORT
            echo "$da" | $CMD >$tf 2>"${te}2"
            kill $pid 2>/dev/null
            if ! echo "$da" |diff - "$tf" >"$tdiff"; then
                success=
                break
            fi
        done
    done
    if test -z "$success"; then
        $PRINTF "$FAILED: $TRACE $SOCAT:\n"
        echo "$CMD2 &"
        echo "$CMD"
        cat "${te}1"
        cat "${te}2"
        cat "$tdiff"
        failed
    else
        $PRINTF "$OK\n"
        if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
        ok
    fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# Test the SOCKS address with IPv4
NAME=SOCKS4CONNECT_TCP4
case "$TESTS" in
*%$N%*|*%functions%*|*%socks%*|*%socks4%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socks4 connect over TCP/IPv4"
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "socks4echo.sh" \
		  "SOCKS4 IP4 TCP LISTEN STDIO" \
		  "TCP4-LISTEN EXEC STDIN SOCKS4" \
		  "so-reuseaddr" \
		  "tcp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
    newport tcp4 	# provide free port number in $PORT
    CMD0="$TRACE $SOCAT $opts TCP4-LISTEN:$PORT,$REUSEADDR EXEC:\"./socks4echo.sh\""
    CMD1="$TRACE $SOCAT $opts STDIO SOCKS4:$LOCALHOST:32.98.76.54:32109,pf=ip4,socksport=$PORT",socksuser="nobody"
    printf "test $F_n $TEST... " $N
    eval "$CMD0 2>\"${te}0\" &"
    pid0=$!	# background process id
    waittcp4port $PORT 1
    echo "$da" |$CMD1 >${tf}1 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null
    wait
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif ! echo "$da" |diff - "${tf}1" >"$tdiff"; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=SOCKS4CONNECT_TCP6
case "$TESTS" in
*%$N%*|*%functions%*|*%socks%*|*%socks4%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socks4 connect over TCP/IPv6"
if ! eval $NUMCOND; then :;
elif ! testfeats socks4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}SOCKS4 not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
# we have a normal tcp echo listening - so the socks header must appear in answer
newport tcp6 	# provide free port number in $PORT
CMD0="$TRACE $SOCAT $opts TCP6-L:$PORT,$REUSEADDR exec:\"./socks4echo.sh\""
CMD1="$TRACE $SOCAT $opts - socks4:$LOCALHOST6:32.98.76.54:32109,socksport=$PORT",socksuser="nobody"
printf "test $F_n $TEST... " $N
eval "$CMD0 2>\"${te}0\" &"
pid=$!	# background process id
waittcp6port $PORT 1
echo "$da" |$CMD1 >${tf}1 2>"${te}1"
if ! echo "$da" |diff - "${tf}1" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "// diff:" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=SOCKS4ACONNECT_TCP4
case "$TESTS" in
*%$N%*|*%functions%*|*%socks%*|*%socks4a%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socks4a connect over TCP/IPv4"
if ! eval $NUMCOND; then :;
elif ! testfeats socks4a >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}SOCKS4A not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
# we have a normal tcp echo listening - so the socks header must appear in answer
newport tcp4 	# provide free port number in $PORT
CMD2="$TRACE $SOCAT $opts TCP4-L:$PORT,$REUSEADDR EXEC:\"./socks4a-echo.sh\""
CMD="$TRACE $SOCAT $opts - SOCKS4A:$LOCALHOST:localhost:32109,pf=ip4,socksport=$PORT",socksuser="nobody"
printf "test $F_n $TEST... " $N
eval "$CMD2 2>\"${te}1\" &"
pid=$!	# background process id
waittcp4port $PORT 1
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=SOCKS4ACONNECT_TCP6
case "$TESTS" in
*%$N%*|*%functions%*|*%socks%*|*%socks4a%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socks4a connect over TCP/IPv6"
if ! eval $NUMCOND; then :;
elif ! testfeats socks4a >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}SOCKS4A not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
# we have a normal tcp echo listening - so the socks header must appear in answer
newport tcp6 	# provide free port number in $PORT
CMD2="$TRACE $SOCAT $opts TCP6-L:$PORT,$REUSEADDR EXEC:\"./socks4a-echo.sh\""
CMD="$TRACE $SOCAT $opts - SOCKS4A:$LOCALHOST6:localhost:32109,socksport=$PORT",socksuser="nobody"
printf "test $F_n $TEST... " $N
eval "$CMD2 2>\"${te}1\" &"
pid=$!	# background process id
waittcp6port $PORT 1
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=PROXYCONNECT_TCP4
case "$TESTS" in
*%$N%*|*%functions%*|*%proxyconnect%*|*%proxy%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: proxy connect over TCP/IPv4"
if ! eval $NUMCOND; then :;
elif ! testfeats proxy >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}PROXY not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
    ts="$td/test$N.sh"
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
    newport tcp4 	# provide free port number in $PORT
    #CMD0="$TRACE $SOCAT tcp4-l:$PORT,crlf SYSTEM:\"read; read; $ECHO \\\"HTTP/1.0 200 OK\n\\\"; cat\""
    CMD0="$TRACE $SOCAT $opts TCP4-L:$PORT,$REUSEADDR,crlf EXEC:\"/usr/bin/env bash proxyecho.sh\""
    CMD1="$TRACE $SOCAT $opts - PROXY:$LOCALHOST:127.0.0.1:1000,pf=ip4,proxyport=$PORT"
    printf "test $F_n $TEST... " $N
    eval "$CMD0 2>\"${te}0\" &"
    pid=$!	# background process id
    waittcp4port $PORT 1
    echo "$da" |$CMD1 >"$tf" 2>"${te}1"
    rc1=$?
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	fail
    elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	fail
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
    kill $pid 2>/dev/null
    wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=PROXYCONNECT_TCP6
case "$TESTS" in
*%$N%*|*%functions%*|*%proxyconnect%*|*%proxy%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%listen%*|*%$NAME%*)
TEST="$NAME: proxy connect over TCP/IPv6"
if ! eval $NUMCOND; then :;
elif ! testfeats proxy >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}PROXY not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv6 not available${NORMAL}\n" $N
    cant
else
    ts="$td/test$N.sh"
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
    newport tcp6 	# provide free port number in $PORT
    #CMD0="$TRACE $SOCAT $opts TCP6-L:$PORT,crlf SYSTEM:\"read; read; $ECHO \\\"HTTP/1.0 200 OK\n\\\"; cat\""
    CMD0="$TRACE $SOCAT $opts TCP6-L:$PORT,$REUSEADDR,crlf EXEC:\"/usr/bin/env bash proxyecho.sh\""
    CMD1="$TRACE $SOCAT $opts - PROXY:$LOCALHOST6:127.0.0.1:1000,proxyport=$PORT"
    printf "test $F_n $TEST... " $N
    eval "$CMD0 2>\"${te}0\" &"
    pid=$!	# background process id
    waittcp6port $PORT 1
    echo "$da" |$CMD1 >"$tf" 2>"${te}1"
    rc1=$?
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff: " >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
    kill $pid 2>/dev/null
    wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=TCP4NOFORK
case "$TESTS" in
*%$N%*|*%functions%*|*%ip%*|*%ip4%*|*%tcp%*|*%tcp4%*|*%exec%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to TCP V4 socket with nofork'ed exec"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp4; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts TCP4-LISTEN:$tsl,$REUSEADDR EXEC:$CAT,nofork"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP4:$ts"
printf "test $F_n $TEST... " $N
#$CMD1 >"$tf" 2>"${te}1" &
$CMD1 >/dev/null 2>"${te}1" &
waittcp4port $tsl
#relsleep 1
echo "$da" |$CMD2 >"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=EXECCATNOFORK
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: simple echo via exec of cat with nofork"
testecho "$N" "$NAME" "$TEST" "" "EXEC:$CAT,nofork" "$opts"
esac
N=$((N+1))


NAME=SYSTEMCATNOFORK
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: simple echo via system() of cat with nofork"
testecho "$N" "$NAME" "$TEST" "" "SYSTEM:$CAT,nofork" "$opts"
esac
N=$((N+1))


NAME=NOFORKSETSID
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: simple echo via exec() of cat with nofork and setsid"
testecho "$N" "$NAME" "$TEST" "" "SYSTEM:$CAT,nofork,setsid" "$opts"
esac
N=$((N+1))

#==============================================================================
#TEST="$NAME: echo via 'connection' to UDP V4 socket"
#if ! eval $NUMCOND; then :; else
#tf="$td/file$N"
#tsl=65534
#ts="127.0.0.1:$tsl"
#da="test$N $(date) $RANDOM"
#$TRACE $SOCAT UDP-LISTEN:$tsl,$REUSEADDR PIPE &
#sleep 2
#echo "$da" |$TRACE $SOCAT stdin!!stdout UDP:$ts >"$tf"
#if [ $? -eq 0 ] && echo "$da" |diff "$tf" -; then
#   $ECHO "... test $N succeeded"
#   ok
#else
#   $ECHO "*** test $N $FAILED"
#    failed
#fi
#fi ;; # NUMCOND
#N=$((N+1))
#==============================================================================
# TEST 4 - simple echo via new file
#if ! eval $NUMCOND; then :; else
#N=4
#tf="$td/file$N"
#tp="$td/pipe$N"
#da="test$N $(date) $RANDOM"
#rm -f "$tf.tmp"
#echo "$da" |$TRACE $SOCAT - FILE:$tf.tmp,ignoreeof >"$tf"
#if [ $? -eq 0 ] && echo "$da" |diff "$tf" -; then
#   $ECHO "... test $N succeeded"
#   ok
#else
#   $ECHO "*** test $N $FAILED"
#   failed
#fi
#fi ;; # NUMCOND

#==============================================================================

NAME=TOTALTIMEOUT
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%timeout%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socat inactivity timeout"
if ! eval $NUMCOND; then :; else
#set -vx
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
newport tcp4 	# provide free port number in $PORT
CMD2="$TRACE $SOCAT $opts -T 1 TCP4-LISTEN:$PORT,$REUSEADDR pipe"
CMD="$TRACE $SOCAT $opts - TCP4-CONNECT:$LOCALHOST:$PORT"
printf "test $F_n $TEST... " $N
eval "$CMD2 2>${te}1 &"
pid=$!	# background process id
waittcp4port $PORT 1
(echo "$da"; sleep 2; echo X) |$CMD >"$tf" 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
#set +vx
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=IGNOREEOF+TOTALTIMEOUT
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%timeout%*|*%ignoreeof%*|*%$NAME%*)
TEST="$NAME: ignoreeof and inactivity timeout"
if ! eval $NUMCOND; then :; else
#set -vx
SAVEMICS=$MICROS
MICROS=1000000
ti="$td/test$N.file"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT $opts -T $(reltime 4) -u file:\"$ti\",ignoreeof -"
printf "test $F_n $TEST... " $N
touch "$ti"
$CMD >"$tf" 2>"$te" &
bg=$!	# background process id
relsleep 1
echo "$da" >>"$ti"
relsleep 8
echo X >>"$ti"
relsleep 2
kill $bg 2>/dev/null
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED (diff):\n"
    echo "$CMD &" >&2
    cat "$te" >&2
    echo "// diff:" >&2
    cat "$tdiff" >&2
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "$te"; fi
   ok
fi
wait
MICROS=$SAVEMICS
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=PROXY2SPACES
case "$TESTS" in
*%$N%*|*%functions%*|*%proxy%*|*%listen%*|*%$NAME%*)
TEST="$NAME: proxy connect accepts status with multiple spaces"
if ! eval $NUMCOND; then :;
elif ! testfeats proxy >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}PROXY not available${NORMAL}\n" $N
    cant
else
ts="$td/test$N.sh"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
newport tcp4 	# provide free port number in $PORT
#CMD2="$TRACE $SOCAT $opts TCP-L:$PORT,crlf SYSTEM:\"read; read; $ECHO \\\"HTTP/1.0 200 OK\n\\\"; cat\""
CMD0="$TRACE $SOCAT $opts TCP4-L:$PORT,reuseaddr,crlf EXEC:\"/usr/bin/env bash proxyecho.sh -w 2\""
CMD1="$TRACE $SOCAT $opts - PROXY:$LOCALHOST:127.0.0.1:1000,pf=ip4,proxyport=$PORT"
printf "test $F_n $TEST... " $N
eval "$CMD0 2>\"${te}1\" &"
pid=$!	# background process id
waittcp4port $PORT 1
echo "$da" |$CMD1 >"$tf" 2>"${te}0"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "diff:"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$debug" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$debug" ];   then cat "${te}1" >&2; fi
    ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=BUG-UNISTDIO
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: for bug with address options on both stdin/out in unidirectional mode"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
ff="$td/test$N.file"
printf "test $F_n $TEST... " $N
>"$ff"
#$TRACE $SOCAT $opts -u /dev/null -,setlk <"$ff"  2>"$te"
CMD="$TRACE $SOCAT $opts -u /dev/null -,setlk"
$CMD <"$ff"  2>"$te"
if [ "$?" -eq 0 ]; then
    $PRINTF "$OK\n"
    ok
else
    if [ "$UNAME" = "Linux" ]; then
	$PRINTF "$FAILED\n"
	echo "$CMD"
	cat "$te"
	failed
    else
	$PRINTF "${YELLOW}failed (don't care)${NORMAL}\n"
	cant
    fi
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=SINGLEEXECOUTSOCKETPAIR
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: inheritance of stdout to single exec with socketpair"
testecho "$N" "$NAME" "$TEST" "-!!exec:cat" "" "$opts" 1
esac
N=$((N+1))

NAME=SINGLEEXECOUTPIPE
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: inheritance of stdout to single exec with pipe"
testecho "$N" "$NAME" "$TEST" "-!!exec:cat,pipes" "" "$opts" 1
esac
N=$((N+1))

NAME=SINGLEEXECOUTPTY
case "$TESTS" in
*%$N%*|*%functions%*|*%pty%*|*%$NAME%*)
TEST="$NAME: inheritance of stdout to single exec with pty"
if ! eval $NUMCOND; then :;
elif ! testfeats pty >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
    cant
else
testecho "$N" "$NAME" "$TEST" "-!!exec:cat,pty,raw" "" "$opts" 1
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=SINGLEEXECINSOCKETPAIR
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: inheritance of stdin to single exec with socketpair"
testecho "$N" "$NAME" "$TEST" "exec:cat!!-" "" "$opts"
esac
N=$((N+1))

NAME=SINGLEEXECINPIPE
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: inheritance of stdin to single exec with pipe"
testecho "$N" "$NAME" "$TEST" "exec:cat,pipes!!-" "" "$opts"
esac
N=$((N+1))

NAME=SINGLEEXECINPTYDELAY
case "$TESTS" in
*%$N%*|*%functions%*|*%pty%*|*%$NAME%*)
TEST="$NAME: inheritance of stdin to single exec with pty, with delay"
if ! eval $NUMCOND; then :;
elif ! testfeats pty >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
    cant
else
testecho "$N" "$NAME" "$TEST" "exec:cat,pty,raw!!-" "" "$opts" $MISCDELAY
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=SINGLEEXECINPTY
case "$TESTS" in
*%$N%*|*%functions%*|*%pty%*|*%$NAME%*)
TEST="$NAME: inheritance of stdin to single exec with pty"
if ! eval $NUMCOND; then :;
elif ! testfeats pty >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
    cant
else
# T value needed (only) by AIX
testecho "$N" "$NAME" "$TEST" "exec:cat,pty,raw!!-" "" "$opts" 0.1
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=READLINE
#set -vx
case "$TESTS" in
*%$N%*|*%functions%*|*%pty%*|*%readline%*|*%sigint%*|*%$NAME%*)
TEST="$NAME: readline with password and sigint"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats readline pty); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
SAVETERM="$TERM"; TERM=	# 'cause console might print controls even in raw
SAVEMICS=$MICROS
#MICROS=2000000
ts="$td/test$N.sh"
to="$td/test$N.stdout"
tpi="$td/test$N.inpipe"
tpo="$td/test$N.outpipe"
te="$td/test$N.stderr"
tr="$td/test$N.ref"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
# The feature that we really want to test is in the readline.sh script
READLINE_LOG=; if grep -e -lf ./readline.sh >/dev/null; then READLINE_LOG="-lf $td/test$N.rl-log"; fi
CMD="$TRACE $SOCAT -lpwrapper $opts -t1 OPEN:$tpi,nonblock!!OPEN:$tpo EXEC:\"./readline.sh -nh $READLINE_LOG ./readline-test.sh\",pty,ctty,setsid,raw,echo=0,isig"
#echo "$CMD" >"$ts"
#chmod a+x "$ts"
printf "test $F_n $TEST... " $N
rm -f "$tpi" "$tpo"
mkfifo "$tpi"
touch "$tpo"
#
# during development of this test, the following command line succeeded:
# ECHO="echo -e" SOCAT=./socat
# (sleep 1; $ECHO "user\n\c"; sleep 1; $ECHO "password\c"; sleep 1; $ECHO "\n\c"; sleep 1; $ECHO "test 1\n\c"; sleep 1; $ECHO "\003\c"; sleep 1; $ECHO "test 2\n\c"; sleep 1; $ECHO "exit\n\c"; sleep 1) |$TRACE $SOCAT -d -d -d -d -lf/tmp/$USER/debug1 -v -x - exec:'./readline.sh ./readline-test.sh',pty,ctty,setsid,raw,echo=0,isig
#
# the following cat, in case of socat failure, reads the pipe to prevent below writer from hanging
PATH=${SOCAT%socat}:$PATH eval "$CMD 2>$te || cat $tpi >/dev/null &"
pid=$!	# background process id
relsleep 1

(
relsleep 3
$ECHO "user\n\c"
relsleep 1
$ECHO "password\c"
relsleep 1
$ECHO "\n\c"
relsleep 1
$ECHO "test 1\n\c"
relsleep 1
$ECHO "\003\c"
relsleep 1
$ECHO "test 2\n\c"
relsleep 1
$ECHO "exit\n\c"
relsleep 1
) >"$tpi"

cat >$tr <<EOF
readline feature test program
Authentication required
Username: user
Password: 
prog> test 1
executing test 1
prog> ./readline-test.sh got SIGINT
test 2
executing test 2
prog> exit
EOF

#0 if ! sed 's/.*\r//g' "$tpo" |diff -q "$tr" - >/dev/null 2>&1; then
#0 if ! sed 's/.*'"$($ECHO '\r\c')"'/</g' "$tpo" |diff -q "$tr" - >/dev/null 2>&1; then
kill $pid 2>/dev/null	# necc on OpenBSD
wait
if ! tr "$($ECHO '\r \c')" "% " <$tpo |sed 's/%$//g' |sed 's/.*%//g' |diff "$tr" - >"$tdiff" 2>&1; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD" 2>&1
    cat "$te" 2>&1
    echo diff:  2>&1
    cat "$tdiff" 2>&1
    failed
else
   $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    ok
fi
wait
MICROS=$SAVEMICS
TERM="$SAVETERM"
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=GENDERCHANGER
case "$TESTS" in
*%$N%*|*%functions%*|*%listen%*|*%retry%*|*%$NAME%*)
TEST="$NAME: TCP4 \"gender changer\""
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4; PORT1=$PORT
newport tcp4; PORT2=$PORT
newport tcp4; PORT3=$PORT
# this is the server in the protected network that we want to reach
CMD1="$TRACE $SOCAT -lpserver $opts TCP4-L:$PORT1,reuseaddr,bind=$LOCALHOST ECHO"
# this is the double client in the protected network
CMD2="$TRACE $SOCAT -lp2client $opts TCP4:$LOCALHOST:$PORT2,retry=10,interval=1 TCP4:$LOCALHOST:$PORT1"
# this is the double server in the outside network
CMD3="$TRACE $SOCAT -lp2server $opts TCP4-L:$PORT3,reuseaddr,bind=$LOCALHOST TCP4-L:$PORT2,reuseaddr,bind=$LOCALHOST"
# this is the outside client that wants to use the protected server
CMD4="$TRACE $SOCAT -lpclient $opts -t1 - tcp4:$LOCALHOST:$PORT3"
printf "test $F_n $TEST... " $N
eval "$CMD1 2>${te}1 &"
pid1=$!
eval "$CMD2 2>${te}2 &"
pid2=$!
eval "$CMD3 2>${te}3 &"
pid3=$!
waittcp4port $PORT1 1 &&
waittcp4port $PORT3 1
sleep 1
echo "$da" |$CMD4 >$tf 2>"${te}4"
kill $pid1 $pid2 $pid3 $pid4 2>/dev/null
wait
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2 &"
    cat "${te}2" >&2
    echo "$CMD3 &"
    cat "${te}3" >&2
    echo "$CMD4"
    cat "${te}4" >&2
    echo diff: >&2
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2 &"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD3 &"; fi
    if [ "$DEBUG" ];   then cat "${te}3" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD4"; fi
    if [ "$DEBUG" ];   then cat "${te}4" >&2; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=OUTBOUNDIN
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%proxy%*|*%fork%*|*%listen%*|*%retry%*|*%$NAME%*)
TEST="$NAME: gender changer via SSL through HTTP proxy, oneshot"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats openssl proxy); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat" |tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
gentestcert testcli
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4; PORT1=$PORT
newport tcp4; PORT2=$PORT
newport tcp4; PORT3=$PORT
newport tcp4; PORT4=$PORT
newport tcp4; PORT5=$PORT
# this is the server in the protected network that we want to reach
CMD1="$TRACE $SOCAT $opts -lpserver TCP4-L:$PORT1,reuseaddr,bind=$LOCALHOST ECHO"
# this is the proxy in the protected network that provides a way out
CMD2="$TRACE $SOCAT $opts -lpproxy TCP4-L:$PORT2,reuseaddr,bind=$LOCALHOST,fork EXEC:./proxy.sh"
# this is our proxy connect wrapper in the protected network
CMD3="$TRACE $SOCAT $opts -lpwrapper TCP4-L:$PORT3,reuseaddr,bind=$LOCALHOST,fork PROXY:$LOCALHOST:$LOCALHOST:$PORT4,pf=ip4,proxyport=$PORT2,resolve"
# this is our double client in the protected network using SSL
#CMD4="$TRACE $SOCAT $opts -lp2client SSL:$LOCALHOST:$PORT3,pf=ip4,retry=10,interval=1,cert=testcli.pem,cafile=testsrv.crt,$SOCAT_EGD TCP4:$LOCALHOST:$PORT1"
CMD4="$TRACE $SOCAT $opts -lp2client SSL:$LOCALHOST:$PORT3,pf=ip4,cert=testcli.pem,cafile=testsrv.crt,$SOCAT_EGD TCP4:$LOCALHOST:$PORT1"
# this is the double server in the outside network
CMD5="$TRACE $SOCAT $opts -lp2server -t1 tcp4-l:$PORT5,reuseaddr,bind=$LOCALHOST ssl-l:$PORT4,pf=ip4,reuseaddr,bind=$LOCALHOST,$SOCAT_EGD,cert=testsrv.pem,cafile=testcli.crt"
# this is the outside client that wants to use the protected server
CMD6="$TRACE $SOCAT $opts -lpclient -t5 - tcp4:$LOCALHOST:$PORT5"
printf "test $F_n $TEST... " $N
eval "$CMD1 2>${te}1 &"
pid1=$!
eval "$CMD2 2>${te}2 &"
pid2=$!
eval "$CMD3 2>${te}3 &"
pid3=$!
waittcp4port $PORT1 1 || $PRINTF "$FAILED: port $PORT1\n" >&2 </dev/null
waittcp4port $PORT2 1 || $PRINTF "$FAILED: port $PORT2\n" >&2 </dev/null
waittcp4port $PORT3 1 || $PRINTF "$FAILED: port $PORT3\n" >&2 </dev/null
eval "$CMD5 2>${te}5 &"
pid5=$!
waittcp4port $PORT5 1 || $PRINTF "$FAILED: port $PORT5\n" >&2 </dev/null
echo "$da" |$CMD6 >$tf 2>"${te}6" &
pid6=$!
waittcp4port $PORT4 1 || $PRINTF "$FAILED: port $PORT4\n" >&2 </dev/null
eval "$CMD4 2>${te}4 &"
pid4=$!
wait $pid6
if ! (echo "$da"; sleep 2) |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD2 &"
    cat "${te}2"
    echo "$CMD3 &"
    cat "${te}3"
    echo "$CMD5 &"
    cat "${te}5"
    echo "$CMD6"
    cat "${te}6"
    echo "$CMD4 &"
    cat "${te}4"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}1" "${te}2" "${te}3" "${te}4" "${te}5" "${te}6"; fi
    ok
fi
kill $pid1 $pid2 $pid3 $pid4 $pid5 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# test the TCP gender changer with almost production requirements: a double
# client repeatedly tries to connect to a double server via SSL through an HTTP
# proxy. the double servers SSL port becomes active for one connection only
# after a (real) client has connected to its TCP port. when the double client
# succeeded to establish an SSL connection, it connects with its second client
# side to the specified (protected) server. all three consecutive connections
# must function for full success of this test.
#PORT=$((RANDOM+16184))
#!
NAME=INTRANETRIPPER
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%proxy%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: gender changer via SSL through HTTP proxy, daemons"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats openssl proxy); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
gentestcert testcli
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da1="test$N.1 $(date) $RANDOM"
da2="test$N.2 $(date) $RANDOM"
da3="test$N.3 $(date) $RANDOM"
newport tcp4; PORT1=$PORT
newport tcp4; PORT2=$PORT
newport tcp4; PORT3=$PORT
newport tcp4; PORT4=$PORT
newport tcp4; PORT5=$PORT
# this is the server in the protected network that we want to reach
CMD1="$TRACE $SOCAT $opts -lpserver -t$(reltime 100) TCP4-L:$PORT1,reuseaddr,bind=$LOCALHOST,fork ECHO"
# this is the proxy in the protected network that provides a way out
# note: the proxy.sh script starts one or two more socat processes without
# setting the program name 
export SOCAT_OPTS="$OPTS" 	# for proxy.sh
CMD2="$TRACE $SOCAT $opts -lpproxy -t$(reltime 100) TCP4-L:$PORT2,reuseaddr,bind=$LOCALHOST,fork EXEC:./proxy.sh"
# this is our proxy connect wrapper in the protected network
CMD3="$TRACE $SOCAT $opts -lpwrapper -t$(reltime 30) TCP4-L:$PORT3,reuseaddr,bind=$LOCALHOST,fork PROXY:$LOCALHOST:$LOCALHOST:$PORT4,pf=ip4,proxyport=$PORT2,resolve"
# this is our double client in the protected network using SSL
CMD4="$TRACE $SOCAT $opts -lp2client -t$(reltime 30) SSL:$LOCALHOST:$PORT3,retry=10,interval=$(reltime 10),cert=testcli.pem,cafile=testsrv.crt,verify,fork,$SOCAT_EGD TCP4:$LOCALHOST:$PORT1,forever,interval=$(reltime 1)"
# This is the double server in the outside network; accept-timeout because it likes to remain hanging on BSD
CMD5="$TRACE $SOCAT $opts -lp2server -t$(reltime 40) TCP4-L:$PORT5,reuseaddr,bind=$LOCALHOST,backlog=3,accept-timeout=4,fork SSL-L:$PORT4,pf=ip4,reuseaddr,bind=$LOCALHOST,$SOCAT_EGD,cert=testsrv.pem,cafile=testcli.crt,retry=20,interval=$(reltime 5)"
# this is the outside client that wants to use the protected server
CMD6="$TRACE $SOCAT $opts -lpclient -t$(reltime 60) - TCP4:$LOCALHOST:$PORT5,retry=3,interval=$(reltime 10)"
printf "test $F_n $TEST... " $N
# start the intranet infrastructure
eval "$CMD1 2>\"${te}1\" &"
pid1=$!
eval "$CMD2 2>\"${te}2\" &"
pid2=$!
waittcp4port $PORT1 1 50 || $PRINTF "$FAILED: port $PORT1\n" >&2 </dev/null
waittcp4port $PORT2 1 50 || $PRINTF "$FAILED: port $PORT2\n" >&2 </dev/null
# initiate our internal measures
eval "$CMD3 2>\"${te}3\" &"
pid3=$!
eval "$CMD4 2>\"${te}4\" &"
pid4=$!
waittcp4port $PORT3 1 50 || $PRINTF "$FAILED: port $PORT3\n" >&2 </dev/null
# now we start the external daemon
eval "$CMD5 2>\"${te}5\" &"
pid5=$!
waittcp4port $PORT5 1 50 || $PRINTF "$FAILED: port $PORT5\n" >&2 </dev/null
# and this is the outside client:
{ echo "$da1"; relsleep 100; } |$CMD6 >${tf}_1 2>"${te}6_1" &
pid6_1=$!
relsleep 20
{ echo "$da2"; relsleep 100; } |$CMD6 >${tf}_2 2>"${te}6_2" &
pid6_2=$!
relsleep 20
{ echo "$da3"; relsleep 100; } |$CMD6 >${tf}_3 2>"${te}6_3" &
pid6_3=$!
wait $pid6_1 $pid6_2 $pid6_3
kill $pid1 $pid2 $pid3 $pid4 $pid5 $(childpids $pid5) 2>/dev/null
# (On BSDs a child of pid5 likes to hang)
#
echo "$da1" |diff - "${tf}_1" >"${tdiff}1"
echo "$da2" |diff - "${tf}_2" >"${tdiff}2"
echo "$da3" |diff - "${tf}_3" >"${tdiff}3"
if test -s "${tdiff}1" -o -s "${tdiff}2" -o -s "${tdiff}3"; then
  # FAILED only when none of the three transfers succeeded
  if test -s "${tdiff}1" -a -s "${tdiff}2" -a -s "${tdiff}3"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD2 &"
    cat "${te}2"
    echo "$CMD3 &"
    cat "${te}3"
    echo "$CMD4 &"
    cat "${te}4"
    echo "$CMD5 &"
    cat "${te}5"
    echo "$CMD6 &"
    cat "${te}6_1"
    cat "${tdiff}1"
    echo "$CMD6 &"
    cat "${te}6_2"
    cat "${tdiff}2"
    echo "$CMD6 &"
    cat "${te}6_3"
    cat "${tdiff}3"
    failed
  else
    $PRINTF "$OK ${YELLOW}(partial failure)${NORMAL}\n"
    if [ -n "$debug" ]; then cat "${te}1" "${te}2" "${te}3" "${te}4" "${te}5" ${te}6*; fi
    ok
  fi
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}1" "${te}2" "${te}3" "${te}4" "${te}5" ${te}6*; fi
    ok
fi
kill $pid1 $pid2 $pid3 $pid4 $pid5 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# let us test the security features with -s, retry, and fork
# method: first test without security feature if it works
#   then try with security feature, must fail

# test the security features of a server address
testserversec () {
    local N="$1"
    local title="$2"
    local opts="$3"
    local arg1="$4"	# the server address
    local secopt0="$5"	# option without security for server, mostly empty
    local secopt1="$6"	# the security option for server, to be tested
    local arg2="$7"	# the client address
    local ipvers="$8"	# IP version, for check of listen port
    local proto="$9"	# protocol, for check of listen port
    local port="${10}"	# start client when this port is listening
    local expect="${11}"	# expected behaviour of client: 0..empty output; -1..error; *: any of these
    local T="${12}";	[ -z "$T" ] && T=0
    local tf="$td/test$N.stdout"
    local te="$td/test$N.stderr"
    local tdiff1="$td/test$N.diff1"
    local tdiff2="$td/test$N.diff2"
    local da="test$N.1 $(date) $RANDOM"
    local stat result

    $PRINTF "test $F_n %s... " $N "$title"
    # first: without security
    # start server
    $TRACE $SOCAT $opts "$arg1,$secopt0" echo 2>"${te}1" &
    spid=$!
    if [ "$port" ] && ! wait${proto}${ipvers}port $port 1; then
	kill $spid 2>/dev/null
	$PRINTF "$NO_RESULT (ph.1 server not working):\n"
	echo "$TRACE $SOCAT $opts \"$arg1,$secopt0\" echo &"
	cat "${te}1"
	cant
	wait; return
    fi
    # now use client
    (echo "$da"; sleep $T) |$TRACE $SOCAT $opts - "$arg2" >"$tf" 2>"${te}2"
    stat="$?"
    kill $spid 2>/dev/null
    #killall $TRACE $SOCAT 2>/dev/null
    if [ "$stat" != 0 ]; then
	$PRINTF "$NO_RESULT (ph.1 function fails): $TRACE $SOCAT:\n"
	echo "$TRACE $SOCAT $opts \"$arg1,$secopt0\" echo &"
	cat "${te}1"
	echo "$TRACE $SOCAT $opts - \"$arg2\""
	cat "${te}2"
	cant
	wait; return
    elif echo "$da" |diff - "$tf" >"$tdiff1" 2>&1; then
	:	# function without security is ok, go on
    else
	$PRINTF "$NO_RESULT (ph.1 function fails): diff:\n"
	echo "$TRACE $SOCAT $opts $arg1,$secopt0 echo &"
	cat "${te}1"
	echo "$TRACE $SOCAT $opts - $arg2"
	cat "${te}2"
	cat "$tdiff1"
	cant
	wait; return
    fi

    # then: with security
    if [ "$port" ] && ! wait${proto}${ipvers}port $port 0; then
	$PRINTF "$NO_RESULT (ph.1 port remains in use)\n"
	cant
	wait; return
    fi
    wait

#set -vx
    # assemble address w/ security option; on dual, take read part:
    case "$arg1" in
    *!!*) arg="${arg1%!!*},$secopt1!!${arg1#*!!}" ;;
    *)    arg="$arg1,$secopt1" ;;
    esac
    # start server
    # use -s to make sure that it fails due to a sec violation, not some other failure
    CMD3="$TRACE $SOCAT $opts -s $arg echo"
    $CMD3 2>"${te}3" &
    spid=$!
    if [ "$port" ] && ! wait${proto}${ipvers}port $port 1; then
	kill $spid 2>/dev/null
	$PRINTF "$NO_RESULT (ph.2 server not working)\n"
	wait
	echo "$CMD3"
	cat "${te}3"
	cant
	return
    fi
    # now use client
    da="test$N.2 $(date) $RANDOM"
    (echo "$da"; sleep $T) |$TRACE $SOCAT $opts - "$arg2" >"$tf" 2>"${te}4"
    stat=$?
    kill $spid 2>/dev/null
#set +vx
    #killall $TRACE $SOCAT 2>/dev/null
    if [ "$stat" != 0 ]; then
	result=-1;	# socat had error
    elif [ ! -s "$tf" ]; then
	result=0;	# empty output
    elif echo "$da" |diff - "$tf" >"$tdiff2" 2>&1; then
	result=1;	# output is copy of input
    else
	result=2;	# output differs from input
    fi
    if [ "$expect" != '1' -a "$result" -eq 1 ]; then
	    $PRINTF "$FAILED: SECURITY BROKEN\n"
	    echo "$TRACE $SOCAT $opts $arg echo"
	    cat "${te}3"
	    echo "$TRACE $SOCAT $opts - $arg2"
	    cat "${te}4"
	    cat "$tdiff2"
	    failed
    elif [ "X$expect" != 'X*' -a X$result != X$expect ]; then
	case X$result in
	X-1) $PRINTF "$NO_RESULT (ph.2 client error): $TRACE $SOCAT:\n"
	    echo "$TRACE $SOCAT $opts $arg echo"
	    cat "${te}3"
	    echo "$TRACE $SOCAT $opts - $arg2"
	    cat "${te}4"
	    cant
	    ;;
	X0) $PRINTF "$NO_RESULT (ph.2 diff failed): diff:\n"
	    echo "$TRACE $SOCAT $opts $arg echo"
	    cat "${te}3"
	    echo "$TRACE $SOCAT $opts - $arg2"
	    cat "${te}4"
	    cat "$tdiff2"
	    cant
	    ;;
	X1) $PRINTF "$FAILED: SECURITY BROKEN\n"
	    echo "$TRACE $SOCAT $opts $arg echo"
	    cat "${te}3"
	    echo "$TRACE $SOCAT $opts - $arg2"
	    cat "${te}4"
	    cat "$tdiff2"
	    failed
	    ;;
	X2) $PRINTF "$FAILED: diff:\n"
	    echo "$TRACE $SOCAT $opts $arg echo"
	    cat "${te}3"
	    echo "$TRACE $SOCAT $opts - $arg2"
	    cat "${te}4"
	    cat "$tdiff2"
	    failed
	    ;;
	esac
    else
	$PRINTF "$OK\n"
	[ "$VERBOSE" ] && echo "  $TRACE $SOCAT $opts $arg echo"
	[ "$debug" ] && cat ${te}3
	[ "$VERBOSE" ] && echo "  $TRACE $SOCAT $opts - $arg2"
	[ "$debug" ] && cat ${te}4
	ok
    fi
    wait
#set +vx
}


NAME=TCP4RANGEBITS
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%range%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP4-L with RANGE option"
if ! eval $NUMCOND; then :;
elif [ -z "$SECONDADDR" ]; then
    # we need access to a second addresses
    $PRINTF "test $F_n $TEST... ${YELLOW}need a second IPv4 address${NORMAL}\n" $N
    cant
else
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP4-L:$PORT,reuseaddr,fork,retry=1" "" "range=$SECONDADDR/32" "TCP4:127.0.0.1:$PORT" 4 tcp $PORT 0
fi ;; # $SECONDADDR, NUMCOND
esac
N=$((N+1))

NAME=TCP4RANGEMASK
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%range%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP4-L with RANGE option"
if ! eval $NUMCOND; then :;
elif [ -z "$SECONDADDR" ]; then
    # we need access to a second addresses
    $PRINTF "test $F_n $TEST... ${YELLOW}need a second IPv4 address${NORMAL}\n" $N
    cant
else
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP4-L:$PORT,reuseaddr,fork,retry=1" "" "range=$SECONDADDR:255.255.255.255" "TCP4:127.0.0.1:$PORT" 4 tcp $PORT 0
fi ;; # $SECONDADDR, NUMCOND
esac
N=$((N+1))

# like TCP4RANGEMASK, but the "bad" address is within the same class A network
NAME=TCP4RANGEMASKHAIRY
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%range%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP4-L with RANGE option"
if ! eval $NUMCOND; then :; else
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP4-L:$PORT,reuseaddr,fork,retry=1" "" "range=127.0.0.0:255.255.0.0" "TCP4:$SECONDADDR:$PORT,bind=$SECONDADDR" 4 tcp $PORT 0
fi ;; # Linux, NUMCOND
esac
N=$((N+1))


NAME=TCP4SOURCEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%sourceport%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP4-L with SOURCEPORT option"
if ! eval $NUMCOND; then :; else
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP4-L:$PORT,reuseaddr,fork,retry=1" "" "sp=$PORT" "TCP4:127.0.0.1:$PORT" 4 tcp $PORT 0
fi ;; # NUMCOND
esac
N=$((N+1))

NAME=TCP4LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%lowport%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP4-L with LOWPORT option"
if ! eval $NUMCOND; then :; else
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP4-L:$PORT,reuseaddr,fork,retry=1" "" "lowport" "TCP4:127.0.0.1:$PORT" 4 tcp $PORT 0
fi ;; # NUMCOND
esac
N=$((N+1))

NAME=TCP4WRAPPERS_ADDR
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%tcpwrap%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP4-L with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip4 libwrap) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: $SECONDADDR" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP4-L:$PORT,reuseaddr,fork,retry=1" "" "hosts-allow=$ha,hosts-deny=$hd" "TCP4:127.0.0.1:$PORT" 4 tcp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=TCP4WRAPPERS_NAME
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%tcpwrap%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP4-L with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip4 libwrap) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: $LOCALHOST" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP4-L:$PORT,reuseaddr,fork,retry=1" "" "hosts-allow=$ha,hosts-deny=$hd" "TCP4:$SECONDADDR:$PORT,bind=$SECONDADDR" 4 tcp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=TCP6RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%range%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP6-L with RANGE option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
newport tcp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP6-L:$PORT,reuseaddr,fork,retry=1" "" "range=[::2]/128" "TCP6:[::1]:$PORT" 6 tcp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=TCP6SOURCEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%sourceport%*|*%listen%|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP6-L with SOURCEPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
newport tcp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP6-L:$PORT,reuseaddr,fork,retry=1" "" "sp=$PORT" "TCP6:[::1]:$PORT" 6 tcp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=TCP6LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%lowport%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP6-L with LOWPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
newport tcp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP6-L:$PORT,reuseaddr,fork,retry=1" "" "lowport" "TCP6:[::1]:$PORT" 6 tcp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=TCP6TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%tcpwrap%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of TCP6-L with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip6 libwrap && runstcp6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: [::2]" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport tcp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "TCP6-L:$PORT,reuseaddr,fork,retry=1" "" "hosts-allow=$ha,hosts-deny=$hd" "TCP6:[::1]:$PORT" 6 tcp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=UDP4RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%range%*|*%listen%*|*%fork%*|*%$NAME%*)
TEST="$NAME: security of UDP4-L with RANGE option"
if ! eval $NUMCOND; then :; else
newport udp4 	# provide free port number in $PORT
#testserversec "$N" "$TEST" "$opts" "UDP4-L:$PORT,reuseaddr,fork" "" "range=$SECONDADDR/32" "UDP4:127.0.0.1:$PORT" 4 udp $PORT 0
testserversec "$N" "$TEST" "$opts" "UDP4-L:$PORT,reuseaddr" "" "range=$SECONDADDR/32" "UDP4:127.0.0.1:$PORT" 4 udp $PORT 0
fi ;; # NUMCOND
esac
N=$((N+1))

NAME=UDP4SOURCEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%sourceport%*|*%listen%*|*%$NAME%*)
TEST="$NAME: security of UDP4-L with SOURCEPORT option"
if ! eval $NUMCOND; then :; else
newport udp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP4-L:$PORT,reuseaddr" "" "sp=$PORT" "UDP4:127.0.0.1:$PORT" 4 udp $PORT 0
fi ;; # NUMCOND
esac
N=$((N+1))

NAME=UDP4LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%lowport%*|*%listen%*|*%$NAME%*)
TEST="$NAME: security of UDP4-L with LOWPORT option"
if ! eval $NUMCOND; then :; else
newport udp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP4-L:$PORT,reuseaddr" "" "lowport" "UDP4:127.0.0.1:$PORT" 4 udp $PORT 0
fi ;; # NUMCOND
esac
N=$((N+1))

NAME=UDP4TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%tcpwrap%*|*%listen%*|*%$NAME%*)
TEST="$NAME: security of UDP4-L with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip4 libwrap) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: $SECONDADDR" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport udp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP4-L:$PORT,reuseaddr" "" "tcpwrap-etc=$td" "UDP4:127.0.0.1:$PORT" 4 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=UDP6RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%range%*|*%listen%*|*%fork%*|*%$NAME%*)
TEST="$NAME: security of UDP6-L with RANGE option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
newport udp6 	# provide free port number in $PORT
#testserversec "$N" "$TEST" "$opts" "UDP6-L:$PORT,reuseaddr,fork" "" "range=[::2]/128" "UDP6:[::1]:$PORT" 6 udp $PORT 0
testserversec "$N" "$TEST" "$opts" "UDP6-L:$PORT,reuseaddr" "" "range=[::2]/128" "UDP6:[::1]:$PORT" 6 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP6SOURCEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%sourceport%*|*%listen%*|*%$NAME%*)
TEST="$NAME: security of UDP6-L with SOURCEPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
    cant
else
newport udp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP6-L:$PORT,reuseaddr" "" "sp=$PORT" "UDP6:[::1]:$PORT" 6 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP6LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%lowport%*|*%listen%*|*%$NAME%*)
TEST="$NAME: security of UDP6-L with LOWPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
    cant
else
newport udp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP6-L:$PORT,reuseaddr" "" "lowport" "UDP6:[::1]:$PORT" 6 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP6TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%tcpwrap%*|*%listen%*|*%$NAME%*)
TEST="$NAME: security of UDP6-L with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip6 libwrap && runsip6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: [::2]" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport udp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP6-L:$PORT,reuseaddr" "" "lowport" "UDP6:[::1]:$PORT" 6 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSLTCP4_RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%openssl%*|*%range%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of SSL-L over TCP/IPv4 with RANGE option"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "SSL-L:$PORT,pf=ip4,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv.crt,key=testsrv.key" "" "range=$SECONDADDR/32" "SSL:$LOCALHOST:$PORT,pf=ip4,cafile=testsrv.crt,$SOCAT_EGD" 4 tcp $PORT -1
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSLTCP4_SOURCEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%openssl%*|*%sourceport%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of SSL-L with SOURCEPORT option"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "SSL-L:$PORT,pf=ip4,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv.crt,key=testsrv.key" "" "sp=$PORT" "SSL:$LOCALHOST:$PORT,pf=ip4,cafile=testsrv.crt,$SOCAT_EGD" 4 tcp $PORT -1
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSLTCP4_LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%openssl%*|*%lowport%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of SSL-L with LOWPORT option"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "SSL-L:$PORT,pf=ip4,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv.crt,key=testsrv.key" "" "lowport" "SSL:$LOCALHOST:$PORT,pf=ip4,cafile=testsrv.crt,$SOCAT_EGD" 4 tcp $PORT -1
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSLTCP4_TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%openssl%*|*%tcpwrap%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of SSL-L with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 tcp libwrap openssl); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: $SECONDADDR" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "SSL-L:$PORT,pf=ip4,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv.crt,key=testsrv.key" "" "tcpwrap-etc=$td" "SSL:$LOCALHOST:$PORT,pf=ip4,cafile=testsrv.crt,$SOCAT_EGD" 4 tcp $PORT -1
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSLCERTSERVER
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%openssl%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of SSL-L with client certificate"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
gentestcert testcli
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts -4" "SSL-L:$PORT,pf=ip4,reuseaddr,fork,retry=1,$SOCAT_EGD,verify,cert=testsrv.crt,key=testsrv.key" "cafile=testcli.crt" "cafile=testsrv.crt" "SSL:$LOCALHOST:$PORT,pf=ip4,cafile=testsrv.crt,cert=testcli.pem,$SOCAT_EGD" 4 tcp $PORT '*'
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSLCERTCLIENT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%openssl%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of SSL with server certificate"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
gentestcert testcli
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts -t 0.5 -lu -d" "SSL:$LOCALHOST:$PORT,pf=ip4,fork,retry=2,verify,cert=testcli.pem,$SOCAT_EGD" "cafile=testsrv.crt" "cafile=testcli.crt" "SSL-L:$PORT,pf=ip4,reuseaddr,$SOCAT_EGD,cafile=testcli.crt,cert=testsrv.crt,key=testsrv.key" 4 tcp "" -1
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=OPENSSLTCP6_RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%openssl%*|*%range%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of SSL-L over TCP/IPv6 with RANGE option"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! feat=$(testfeats tcp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
gentestcert6 testsrv6
newport tcp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "SSL-L:$PORT,pf=ip6,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv6.crt,key=testsrv6.key" "" "range=[::2]/128" "SSL:[::1]:$PORT,cafile=testsrv6.crt,$SOCAT_EGD" 6 tcp $PORT -1
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSLTCP6_SOURCEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%openssl%*|*%sourceport%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of SSL-L over TCP/IPv6 with SOURCEPORT option"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! feat=$(testfeats tcp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
gentestcert6 testsrv6
newport tcp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "SSL-L:$PORT,pf=ip6,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv6.crt,key=testsrv6.key" "" "sp=$PORT" "SSL:[::1]:$PORT,cafile=testsrv6.crt,$SOCAT_EGD" 6 tcp $PORT -1
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSLTCP6_LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%openssl%*|*%lowport%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of SSL-L over TCP/IPv6 with LOWPORT option"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! feat=$(testfeats tcp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
gentestcert6 testsrv6
newport tcp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "SSL-L:$PORT,pf=ip6,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv6.crt,key=testsrv6.key" "" "lowport" "SSL:[::1]:$PORT,cafile=testsrv6.crt,$SOCAT_EGD" 6 tcp $PORT -1
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSLTCP6_TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%openssl%*|*%tcpwrap%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of SSL-L over TCP/IPv6 with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip6 tcp libwrap openssl && runsip6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
gentestcert6 testsrv6
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: [::2]" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport tcp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "SSL-L:$PORT,pf=ip6,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv6.crt,key=testsrv6.key" "" "tcpwrap-etc=$td" "SSL:[::1]:$PORT,cafile=testsrv6.crt,$SOCAT_EGD" 6 tcp $PORT -1
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# test security with the openssl-commonname option on client side
NAME=OPENSSL_CN_CLIENT_SECURITY
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%openssl%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of client openssl-commonname option"
# connect using non matching server name/address with commonname
# options, this should succeed. Then without this option, should fail
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
gentestcert testcli
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts -t 0.5 -4" "SSL:127.0.0.1:$PORT,fork,retry=2,verify,cafile=testsrv.crt" "commonname=$LOCALHOST" "" "SSL-L:$PORT,pf=ip4,$REUSEADDR,cert=testsrv.crt,key=testsrv.key,verify=0" 4 tcp "" '*'
fi ;; # testfeats, NUMCOND
esac
N=$((N+1))

# test security with the openssl-commonname option on server side
NAME=OPENSSL_CN_SERVER_SECURITY
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%openssl%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: security of server openssl-commonname option"
# connect using with client certificate to server, this should succeed.
# Then use the server with a non matching openssl-commonname option,
# this must fail
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
gentestcert testcli
newport tcp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts -4" "SSL-L:$PORT,pf=ip4,reuseaddr,cert=testsrv.crt,key=testsrv.key,cafile=testcli.crt" "" "commonname=onlyyou" "SSL:$LOCALHOST:$PORT,pf=ip4,$REUSEADDR,verify=0,cafile=testsrv.crt,cert=testcli.crt,key=testcli.key" 4 tcp "$PORT" '*'
fi ;; # testfeats, NUMCOND
esac
N=$((N+1))


NAME=OPENSSL_FIPS_SECURITY
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%openssl%*|*%fips%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: OpenSSL restrictions by FIPS"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
elif ! testoptions fips >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL/FIPS not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
gentestcert testcli
newport tcp4 	# provide free port number in $PORT
# openssl client accepts a "normal" certificate only when not in fips mode
testserversec "$N" "$TEST" "$opts" "SSL:$LOCALHOST:$PORT,fork,retry=2,verify,cafile=testsrv.crt" "" "fips" "SSL-L:$PORT,pf=ip4,$REUSEADDR,cert=testsrv.crt,key=testsrv.key" 4 tcp "" -1
fi ;; # testfeats, NUMCOND
esac
N=$((N+1))


NAME=UNIEXECEOF
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: give exec'd write-only process a chance to flush (-u)"
testod "$N" "$NAME" "$TEST" "" EXEC:"$OD_C" "$opts -u"
esac
N=$((N+1))


NAME=REVEXECEOF
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: give exec'd write-only process a chance to flush (-U)"
testod "$N" "$NAME" "$TEST" EXEC:"$OD_C" "-" "$opts -U"
esac
N=$((N+1))


NAME=FILANDIR
case "$TESTS" in
*%$N%*|*%filan%*|*%$NAME%*)
TEST="$NAME: check type printed for directories"
if ! eval $NUMCOND; then :; else
te="$td/test$N.stderr"
printf "test $F_n $TEST... " $N
type=$($FILAN -f . 2>$te |tail -n 1 |awk '{print($2);}')
if [ "$type" = "dir" ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    cat "$te"
    failed
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# Test if Filan can determine UNIX domain socket in file system
NAME=FILANSOCKET
case "$TESTS" in
*%$N%*|*%filan%*|*%unix%*|*%listen%*|*%$NAME%*)
TEST="$NAME: capability to analyze named unix socket"
# Run Filan on a listening UNIX domain socket.
# When its output gives "socket" as type (2nd column), the test succeeded
if ! eval $NUMCOND; then :; else
ts="$td/test$N.socket"
te1="$td/test$N.stderr1"	# socat
te2="$td/test$N.stderr2"	# filan
printf "test $F_n $TEST... " $N
$TRACE $SOCAT $opts UNIX-LISTEN:"$ts" /dev/null </dev/null 2>"$te1" &
spid=$!
waitfile "$ts" 1
type=$($FILAN -f "$ts" 2>$te2 |tail -n 1 |awk '{print($2);}')
if [ "$type" = "socket" ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	echo "$SOCAT $opts UNIX-LISTEN:\"$ts\" /dev/null </dev/null 2>\"$te1\""
	echo "$FILAN -f "$ts" 2>$te2 |tail -n 1 |awk '{print(\$2);}'"
    fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$SOCAT $opts UNIX-LISTEN:\"$ts\" /dev/null </dev/null 2>\"$te1\"" >&2
    cat "$te1"
    echo "$FILAN -f "$ts" 2>$te2 |tail -n 1 |awk '{print(\$2);}'" >&2
    cat "$te2"
    failed
fi
kill $spid 2>/dev/null
wait
fi ;; # NUMCOND
esac
N=$((N+1))


testptywaitslave () {
    local N="$1"
    local TEST="$2"
    local PTYTYPE="$3"	# ptmx or openpty
    local opts="$4"

    local tp="$td/test$N.pty"
    local ts="$td/test$N.socket"
    local tf="$td/test$N.file"
    local tdiff="$td/test$N.diff"
    local te1="$td/test$N.stderr1"
    local te2="$td/test$N.stderr2"
    local te3="$td/test$N.stderr3"
    local te4="$td/test$N.stderr4"
    local da="test$N $(date) $RANDOM"
printf "test $F_n $TEST... " $N
#    set -vx
# first generate a pty, then a socket
($TRACE $SOCAT $opts -lpsocat1 PTY,$PTYTYPE,pty-wait-slave,pty-interval=$val_t,link="$tp" UNIX-LISTEN:"$ts" 2>"$te1"; rm -f "$tp") 2>/dev/null &
pid=$!
waitfile "$tp" 1 100
# if pty was non-blocking, the socket is active, and socat1 will term
$TRACE $SOCAT $opts -T 10 -lpsocat2 FILE:/dev/null UNIX-CONNECT:"$ts" 2>"$te2"
# if pty is blocking, first socat is still active and we get a connection now
#((echo "$da"; sleep 2) |$TRACE $SOCAT -lpsocat3 $opts - file:"$tp",$PTYOPTS2 >"$tf" 2>"$te3") &
( (waitfile "$ts" 1 100; echo "$da"; sleep 1) |$TRACE $SOCAT -lpsocat3 $opts - FILE:"$tp",$PTYOPTS2 >"$tf" 2>"$te3") &
waitfile "$ts" 1 100
# but we need an echoer on the socket
$TRACE $SOCAT $opts -lpsocat4 UNIX:"$ts" ECHO 2>"$te4"
# now $tf file should contain $da
#kill $pid 2>/dev/null
wait
#
if echo "$da" |diff - "$tf"> "$tdiff"; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
        echo "  $TRACE $SOCAT $opts -T 10 -lpsocat2 FILE:/dev/null UNIX-CONNECT:\"$ts\"" 2>"$te2"
	echo "  $TRACE $SOCAT $opts -lpsocat1 PTY,$PTYTYPE,pty-wait-slave,link=\"$tp\" UNIX-LISTEN:\"$ts\"" >&2
	echo "  $TRACE $SOCAT -lpsocat3 $opts - file:\"$tp\",$PTYOPTS2" >&2
    fi
    ok
else
    $PRINTF "${YELLOW}FAILED${NORMAL}\n"
    cat "$te1"
    #cat "$te2"	# not of interest
    cat "$te3"
    cat "$te4"
    cat "$tdiff"
    cant
fi
set +vx
}

NAME=PTMXWAITSLAVE
PTYTYPE=ptmx
case "$TESTS" in
*%$N%*|*%functions%*|*%pty%*|*%unix%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test if master pty ($PTYTYPE) waits for slave connection"
if ! eval $NUMCOND; then :; else
if ! feat=$(testfeats pty); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions "$PTYTYPE" pty-wait-slave); then
    $PRINTF "test $F_n $TEST... ${YELLOW}option $(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
   testptywaitslave "$N" "$TEST" "$PTYTYPE" "$opts"
fi
fi ;; # NUMCOND
esac
N=$((N+1))

NAME=OPENPTYWAITSLAVE
PTYTYPE=openpty
case "$TESTS" in
*%$N%*|*%functions%*|*%pty%*|*%unix%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test if master pty ($PTYTYPE) waits for slave connection"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats pty); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions "$PTYTYPE" pty-wait-slave); then
    $PRINTF "test $F_n $TEST... ${YELLOW}option $(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
   testptywaitslave "$N" "$TEST" "$PTYTYPE" "$opts"
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# Test the connect-timeout address option
NAME=CONNECTTIMEOUT
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%tcp%*|*%timeout%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test the connect-timeout option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions connect-timeout); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
# We need a hanging connection attempt, guess an address for this
case "$UNAME" in
Linux) HANGIP=1.0.0.1 ;;
*) HANGIP=255.255.255.254 ;;
esac
te1="$td/test$N.stderr1"
tk1="$td/test$N.kill1"
te2="$td/test$N.stderr2"
tk2="$td/test$N.kill2"
$PRINTF "test $F_n $TEST... " $N
# First, try to make socat hang and see if it can be killed
#$TRACE $SOCAT $opts - TCP:$HANGIP:1 >"$te1" 2>&1 </dev/null &
CMD="$TRACE $SOCAT $opts - TCP:$HANGIP:1"
$CMD >"$te1" 2>$te1 </dev/null &
pid1=$!
relsleep 2
if ! kill $pid1 2>"$tk1"; then
    $PRINTF "${YELLOW}does not hang${NORMAL}\n"
    echo "$CMD" >&2
    cat "$te1" >&2
    cant
else
# Second, set connect-timeout and see if socat exits before kill
CMD="$TRACE $SOCAT $opts - TCP:$HANGIP:1,connect-timeout=$(reltime 1)"
$CMD >"$te1" 2>$te2 </dev/null &
pid2=$!
relsleep 10
if kill $pid2 2>"$tk2"; then
    $PRINTF "$FAILED (\n"
    echo "$CMD" >&2
    cat "$te2" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	echo "$CMD" >&2
    fi
    ok
fi
fi
wait
fi ;; # testfeats, NUMCOND
esac
N=$((N+1))


# version 1.7.0.0 had a bug with the connect-timeout option: while it correctly
# terminated a hanging connect attempt, it prevented a successful connection
# establishment from being recognized by socat, instead the timeout occurred
NAME=CONNECTTIMEOUT_CONN
if ! eval $NUMCOND; then :; else
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%tcp%*|*%timeout%*|*%listen%*|*%$NAME%*)
TEST="$NAME: TCP4 connect-timeout option when server replies"
# just try a connection that is expected to succeed with the usual data
# transfer; with the bug it will fail
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp4; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts TCP4-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIO TCP4:$ts,connect-timeout=1"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1=$!
waittcp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2=$?
kill $pid1 2>/dev/null
wait
if [ $rc2 -ne 0 ]; then
    $PRINTF "$FAILED (rc2=$rc2)\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED (diff)\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    echo "// diff:" >&2
    cat "$tdiff" >&2
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi ;;
esac
fi # NUMCOND
N=$((N+1))


NAME=OPENSSLLISTENDSA
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%listen%*|*%$NAME%*)
TEST="$NAME: openssl listen with DSA certificate"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
else
SRVCERT=testsrvdsa
gentestdsacert $SRVCERT
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4 	# provide free port number in $PORT
CMD2="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,$SOCAT_EGD,cert=$SRVCERT.pem,key=$SRVCERT.key,verify=0 pipe"
CMD="$TRACE $SOCAT $opts - openssl:$LOCALHOST:$PORT,pf=ip4,verify=0,$SOCAT_EGD"
$PRINTF "test $F_n $TEST... " $N
eval "$CMD2 2>\"${te}1\" &"
pid=$!	# background process id
waittcp4port $PORT
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED\n"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat ${te}1 ${te}2; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # testfeats, NUMCOND
esac
N=$((N+1))


# derive signal number from signal name
# kill -l should provide the info
signum () {
  if [ ! "$BASH_VERSION" -o -o posix ]; then
    # we expect:
    for i in $(POSIXLY_CORRECT=1 kill -l); do echo "$i"; done |grep -n -i "^$1$" |cut -d: -f1
  else
    # expect:
    # " 1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL"
    signam="$1"
    kill -l </dev/null |
    while read l; do printf "%s %s\n%s %s\n%s %s\n%s %s\n" $l; done |
    grep -e "SIG$signam\$" |
    cut -d ')' -f 1
  fi
}

# problems with QUIT, INT (are blocked in system() )
for signam in TERM ILL; do
NAME=EXITCODESIG$signam
case "$TESTS" in
*%$N%*|*%functions%*|*%pty%*|*%signal%*|*%$NAME%*)
TEST="$NAME: exit status when dying on SIG$signam"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats pty); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat" |tr a-z A-Z) not available${NORMAL}\n" $N
    cant
else
SIG="$(signum $signam)"
te="$td/test$N.stderr"
tpp="$td/test$N.ppid"
tp="$td/test$N.pid"
$PRINTF "test $F_n $TEST... " $N
(sleep 1; kill -"$SIG" "$(cat "$tpp")") &
# a simple "system:echo $PPID..." does not work on NetBSD, OpenBSD
#$TRACE $SOCAT $opts echo SYSTEM:'exec /usr/bin/env bash -c "echo \$PPID '">$tpp"'; echo \$$ '">$tp; read x\"",nofork 2>"$te"; stat=$?
tsh="$td/test$N.sh"
cat <<EOF >"$tsh"
#! /usr/bin/env bash
echo \$PPID >"$tpp"
echo \$\$ >"$tp"
read x
EOF
chmod a+x "$tsh"
#$TRACE $SOCAT $opts echo SYSTEM:"exec \"$tsh\"",pty,setsid,nofork 2>"$te"; stat=$?
CMD="$TRACE $SOCAT $opts ECHO SYSTEM:\"exec\\\ \\\"$tsh\\\"\",pty,setsid,nofork"
$TRACE $SOCAT $opts ECHO SYSTEM:"exec \"$tsh\"",pty,setsid,nofork 2>"$te"
stat=$?
sleep 1; kill -INT $(cat $tp)
wait
if [ "$stat" -eq $((128+$SIG)) ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD"
    cat "$te"
    failed
fi
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))
done


NAME=READBYTES
#set -vx
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: restrict reading from file with bytes option"
if ! eval $NUMCOND; then :;
elif false; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
tr="$td/test$N.ref"
ti="$td/test$N.in"
to="$td/test$N.out"
te="$td/test$N.err"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
#
CMD="$TRACE $SOCAT $opts -u open:$ti,readbytes=100 -"
printf "test $F_n $TEST... " $N
rm -f "$tf" "$ti" "$to"
#
echo "AAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAAA" >"$tr"	# 100 bytes
cat "$tr" "$tr" >"$ti"			# 200 bytes
$CMD >"$to" 2>"$te"
if ! diff "$tr" "$to" >"$tdiff" 2>&1; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD"
    cat "$te"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=UDPLISTENFORK
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%udp%*|*%listen%*|*%fork%*|*%$NAME%*)
TEST="$NAME: UDP socket rebinds after first connection"
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO IP4 UDP PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO UDP4-CONNECT UDP4-LISTEN PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions bind so-reuseaddr fork) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da1="test$N $(date) $RANDOM"
da2="test$N $(date) $RANDOM"
#establish a listening and forking udp socket in background
newport udp4 	# provide free port number in $PORT
#processes hang forever without -T
CMD0="$TRACE $SOCAT -T $(reltime 5) $opts -lpserver UDP4-LISTEN:$PORT,bind=$LOCALHOST,$REUSEADDR,fork PIPE"
#make a first and a second connection
CMD1="$TRACE $SOCAT $opts -lpclient - UDP4-CONNECT:$LOCALHOST:$PORT"
$PRINTF "test $F_n $TEST... " $N
eval "$CMD0 2>${te}0 &"
pids=$!
waitudp4port "$PORT"
echo "$da1" |eval "$CMD1" >"${tf}1" 2>"${te}1"
if [ $? -ne 0 ]; then
    kill "$pids" 2>/dev/null
    $PRINTF "$NO_RESULT (first conn failed):\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    cant
elif ! echo "$da1" |diff - "${tf}1" >"$tdiff"; then
    kill "$pids" 2>/dev/null
    $PRINTF "$NO_RESULT (first conn failed); diff:\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    cat "$tdiff"
    cant
else
relsleep 2		# UDP-LISTEN sleeps 1s
echo "$da2" |eval "$CMD1" >"${tf}2" 2>"${te}2"
rc="$?"; kill "$pids" 2>/dev/null
if [ $rc -ne 0 ]; then
    $PRINTF "$FAILED:\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
elif ! echo "$da2" |diff - "${tf}2" >"$tdiff"; then
    $PRINTF "$FAILED: diff\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "diff:"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
fi # !( $? -ne 0)
fi # !(rc -ne 0)
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# Is a listen address capable of forking two child processes and have both
# active?
while read PROTOV MAJADDR MINADDR; do
if [ -z "$PROTOV" ] || [[ "$PROTOV" == \#* ]]; then continue; fi
protov="$(echo "$PROTOV" |tr A-Z a-z)"
proto="${protov%%[0-9]}"
NAME=${PROTOV}LISTENFORK
case "$TESTS" in
*%$N%*|*%functions%*|*%$protov%*|*%$proto%*|*%listen%*|*%fork%*|*%$NAME%*)
TEST="$NAME: $PROTOV listen handles 2 concurrent connections"
# Have a listening address with fork option. connect with client1, send a piece
# of data, wait 1s, connect with client2, send another piece of data, wait 1s,
# and send another piece of data with client1. The server processes append all
# data to the same file. Check all data are written to the file in correct
# order.
if ! eval $NUMCOND; then :;
#elif ! feat=$(testfeats $PROTOV); then
#    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$PROTOV" |tr a-z A-Z) not available${NORMAL}\n" $N
#    cant
elif ! runs$protov >/dev/null; then
     $PRINTF "test $F_n $TEST... ${YELLOW}$PROTOV not available${NORMAL}\n" $N
     cant
else
ts="$td/test$N.sock"
tref="$td/test$N.ref"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da1a="test$N $(date) 1a $RANDOM"
da1b="test$N $(date) 1b $RANDOM"
da2="test$N $(date) 2 $RANDOM"
case "$MAJADDR" in
    "FILE")
	tla="$ts"
	tca="$ts"
	waitproto="file"
	waitfor="$ts" ;;
esac
case "$MINADDR" in
    "PORT")
	newport $protov 	# provide free port number in $PORT
	tla="$PORT,bind=$MAJADDR"
	tca="$MAJADDR:$PORT"
	waitproto="${protov}port"
	waitfor="$PORT" ;;
esac
#set -xv
echo -e "$da1a\n$da2\n$da1b" >"$tref"
# establish a listening and forking listen socket in background
# UDP processes hang forever without -T
CMD0="$TRACE $SOCAT -T $(reltime 20) $opts -lpserver $PROTOV-LISTEN:$tla,$REUSEADDR,fork PIPE"
# make a first and a second connection
CMD1="$TRACE $SOCAT $opts -t $(reltime 1) -lpclient - $PROTOV-CONNECT:$tca"
$PRINTF "test $F_n $TEST... " $N
eval "$CMD0 2>${te}0 &"
pid0=$!
wait$waitproto "$waitfor" 1 2
(echo "$da1a"; relsleep 10; echo "$da1b") |eval "$CMD1" >>"${tf}" 2>"${te}1" &
relsleep 5
# trailing sleep req for sctp because no half close
(echo "$da2"; relsleep 5) |eval "$CMD1" >>"${tf}" 2>"${te}2" &
relsleep 10
kill $pid0 2>/dev/null
wait
if ! diff -u "$tref" "$tf" >"$tdiff"; then
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD1"
    cat "${te}2" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi # !(rc -ne 0)
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))
done <<<"
TCP4  $LOCALHOST PORT
TCP6  $LOCALHOST6 PORT
UDP4  $LOCALHOST PORT
UDP6  $LOCALHOST6 PORT
SCTP4 $LOCALHOST PORT
SCTP6 $LOCALHOST6 PORT
UNIX FILE ,
"


NAME=UNIXTOSTREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%unix%*|*%listen%*|*%$NAME%*)
TEST="$NAME: generic UNIX client connects to stream socket"
if ! eval $NUMCOND; then :; else
ts="$td/test$N.socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da1="test$N $(date) $RANDOM"
#establish a listening unix socket in background
SRV="$TRACE $SOCAT $opts -lpserver UNIX-LISTEN:\"$ts\" PIPE"
#make a connection
CLI="$TRACE $SOCAT $opts -lpclient - UNIX:\"$ts\""
$PRINTF "test $F_n $TEST... " $N
eval "$SRV 2>${te}s &"
pids=$!
waitfile "$ts"
echo "$da1" |eval "$CLI" >"${tf}1" 2>"${te}1"
if [ $? -ne 0 ]; then
    kill "$pids" 2>/dev/null
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    echo "$CLI"
    cat "${te}s" "${te}1"
    failed
elif ! echo "$da1" |diff - "${tf}1" >"$tdiff"; then
    kill "$pids" 2>/dev/null
    $PRINTF "$FAILED; diff:\n"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    ok
fi # !(rc -ne 0)
wait
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=UNIXTODGRAM
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%unix%*|*%recv%*|*%$NAME%*)
TEST="$NAME: generic UNIX client connects to datagram socket"
if ! eval $NUMCOND; then :; else
ts1="$td/test$N.socket1"
ts2="$td/test$N.socket2"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da1="test$N $(date) $RANDOM"
#establish a receiving unix datagram socket in background
SRV="$TRACE $SOCAT $opts -lpserver UNIX-RECVFROM:\"$ts1\" PIPE"
#make a connection
CLI="$TRACE $SOCAT $opts -lpclient - UNIX:\"$ts1\",bind=\"$ts2\""
#CLI="$TRACE $SOCAT $opts -lpclient - UNIX:\"$ts1\""
$PRINTF "test $F_n $TEST... " $N
eval "$SRV 2>${te}s &"
pids=$!
waitfile "$ts1"
echo "$da1" |eval "$CLI" >"${tf}1" 2>"${te}1"
rc=$?
kill $pids 2>/dev/null
wait
if [ $rc -ne 0 ]; then
    kill "$pids" 2>/dev/null
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    cat "${te}s"
    echo "$CLI"
    cat "${te}1" "${te}1"
    failed
elif ! echo "$da1" |diff - "${tf}1" >"$tdiff"; then
    kill "$pids" 2>/dev/null
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    cat "${te}s"
    echo "$CLI"
    cat "${te}1"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    ok
fi # !(rc -ne 0)
fi ;; # NUMCOND
esac
N=$((N+1))


# there was an error in address EXEC with options pipes,stderr
NAME=EXECPIPESSTDERR
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: simple echo via exec of cat with pipes,stderr"
# this test is known to fail when logging is enabled with OPTS/opts env var.
SAVE_opts="$opts"
opts="$(echo "$opts" |sed 's/-dd*//g')"
testecho "$N" "$NAME" "$TEST" "" "EXEC:$CAT,pipes,stderr" "$opts"
opts="$SAVE_opts"
esac
N=$((N+1))

# EXEC and SYSTEM with stderr injected socat messages into the data stream. 
NAME=EXECSTDERRLOG
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: simple echo via exec of cat with pipes,stderr"
SAVE_opts="$opts"
# make sure at least two -d are there
case "$opts" in
*-d*-d*) ;;
*-d*) opts="$opts -d" ;;
*) opts="-d -d" ;;
esac
testecho "$N" "$NAME" "$TEST" "" "exec:$CAT,pipes,stderr" "$opts"
opts="$SAVE_opts"
esac
N=$((N+1))
#TTY=$(tty); ps -fade |grep "${TTY#/*/}\>" >/tmp/ps.out


NAME=SIMPLEPARSE
case "$TESTS" in
*%$N%*|*%functions%*|*%PARSE%*|*%$NAME%*)
TEST="$NAME: invoke socat from socat"
testecho "$N" "$NAME" "$TEST" "" exec:"$SOCAT - exec\:$CAT,pipes" "$opts" "$val_t"
esac
N=$((N+1))


NAME=FULLPARSE
case "$TESTS" in
*%$N%*|*%functions%*|*%parse%*|*%$NAME%*)
TEST="$NAME: correctly parse special chars"
if ! eval $NUMCOND; then :; else
$PRINTF "test $F_n $TEST... " $N
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
# a string where commas are hidden in nesting lexical constructs
# if they are scanned incorrectly, socat will see an "unknown option"
dain='(,)[,]{,}","([),])hugo'
daout='(,)[,]{,},([),])hugo'
$TRACE "$SOCAT" $opts -u "exec:echo $dain" - >"$tf" 2>"$te"
rc=$?
echo "$daout" |diff "$tf" - >"$tdiff"
if [ "$rc" -ne 0 ]; then
    $PRINTF "$FAILED:\n"
    echo "$TRACE $SOCAT" -u "exec:echo $da" -
    cat "$te"
    failed
elif [ -s "$tdiff" ]; then
    $PRINTF "$FAILED:\n"
    echo diff:
    cat "$tdiff"
    if [ -n "$debug" ]; then cat $te; fi
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat $te; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))

NAME=NESTEDSOCATEXEC
case "$TESTS" in
*%parse%*|*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: does lexical analysis work sensibly (exec)"
testecho "$N" "$NAME" "$TEST" "" "exec:'$SOCAT - exec:$CAT,pipes'" "$opts" 1
esac
N=$((N+1))

NAME=NESTEDSOCATSYSTEM
case "$TESTS" in
*%parse%*|*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: does lexical analysis work sensibly (system)"
testecho "$N" "$NAME" "$TEST" "" "system:\"$SOCAT - exec:$CAT,pipes\"" "$opts" 1
esac
N=$((N+1))


NAME=TCP6BYTCP4
case "$TESTS" in
*%$N%*|*%functions%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%listen%*|*%$NAME%*)
TEST="$NAME: TCP4 mapped into TCP6 address space"
if ! eval $NUMCOND; then :;
elif true; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature removed${NORMAL}\n" $N
    cant
elif ! testfeats tcp ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp6; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts TCP6-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT TCP6:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waittcp6port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED: diff:\n"
   cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
kill $pid 2>/dev/null; wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# test the UDP4-SENDTO and UDP4-RECVFROM addresses together
NAME=UDP4DGRAM
case "$TESTS" in
*%$N%*|*%functions%*|*%udp%*|*%udp4%*|*%ip4%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: UDP/IPv4 sendto and recvfrom"
# start a UDP4-RECVFROM process that echoes data, and send test data using
# UDP4-SENDTO. The sent data should be returned.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; ts1p=$PORT
ts1a="127.0.0.1"
ts1="$ts1a:$ts1p"
newport udp4; ts2p=$PORT
ts2="127.0.0.1:$ts2p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts UDP4-RECVFROM:$ts1p,reuseaddr,bind=$ts1a PIPE"
CMD2="$TRACE $SOCAT $opts - UDP4-SENDTO:$ts1,bind=$ts2"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
pid1="$!"
waitudp4port $ts1p 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2="$?"
kill "$pid1" 2>/dev/null; wait;
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   cat "${te}1"
   echo "$CMD2"
   cat "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   echo "$CMD1 &"
   cat "${te}1"
   echo "$CMD2"
   cat "${te}2"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=UDP6DGRAM
case "$TESTS" in
*%$N%*|*%functions%*|*%udp%*|*%udp6%*|*%ip6%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: UDP/IPv6 datagram"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp6; ts1p=$PORT
tsa="[::1]"
ts1="$tsa:$ts1p"
newport udp6; ts2p=$PORT
ts2="$tsa:$ts2p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts UDP6-RECVFROM:$ts1p,reuseaddr,bind=$tsa PIPE"
CMD2="$TRACE $SOCAT $opts - UDP6-SENDTO:$ts1,bind=$ts2"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
waitudp6port $ts1p 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat ${te}1 ${te}2; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=RAWIP4RECVFROM
case "$TESTS" in
*%$N%*|*%functions%*|*%ip%*|*%ip4%*|*%rawip%*|*%rawip4%*|*%dgram%*|*%root%*|*%$NAME%*)
TEST="$NAME: raw IPv4 datagram"
if ! eval $NUMCOND; then :;
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1p=$IPPROTO; #IPPROTO=$((IPPROTO+1))
ts1a="127.0.0.1"
ts1="$ts1a:$ts1p"
ts2a="$SECONDADDR"
ts2="$ts2a:$ts2p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts IP4-RECVFROM:$ts1p,reuseaddr,bind=$ts1a PIPE"
CMD2="$TRACE $SOCAT $opts - IP4-SENDTO:$ts1,bind=$ts2a"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
pid1=$!
waitip4proto $ts1p 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2=$?
kill $pid1 2>/dev/null;  wait
if [ $rc2 -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # root, NUMCOND
esac
N=$((N+1))


if false; then
NAME=RAWIP6RECVFROM
case "$TESTS" in
*%$N%*|*%functions%*|*%ip%*|*%ip6%*|*%rawip%*|*%rawip6%*|*%dgram%*|*%root%*|*%$NAME%*)
TEST="$NAME: raw IPv6 datagram by self addressing"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip6 rawip && runsip6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1p=$IPPROTO; #IPPROTO=$((IPPROTO+1))
tsa="[::1]"
ts1="$tsa:$ts1p"
ts2="$tsa"
da="test$N $(date) $RANDOM"
#CMD1="$TRACE $SOCAT $opts IP6-RECVFROM:$ts1p,reuseaddr,bind=$tsa PIPE"
CMD2="$TRACE $SOCAT $opts - IP6-SENDTO:$ts1,bind=$ts2"
printf "test $F_n $TEST... " $N
#$CMD1 2>"${te}1" &
waitip6proto $ts1p 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
#   echo "$CMD1 &"
#   cat "${te}1"
   echo "$CMD2"
   cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "$te"; fi
   ok
fi
fi ;; # root, NUMCOND
esac
N=$((N+1))
fi #false


NAME=UNIXDGRAM
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%unix%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: UNIX datagram"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1="$td/test$N.socket1"
ts2="$td/test$N.socket2"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts UNIX-RECVFROM:$ts1,reuseaddr PIPE"
CMD2="$TRACE $SOCAT $opts - UNIX-SENDTO:$ts1,bind=$ts2"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
pid1="$!"
waitfile $ts1 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2=$?
kill "$pid1" 2>/dev/null; wait
if [ $rc2 -ne 0 ]; then
    $PRINTF "$FAILED (rc=$rc2)\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    echo "// diff:" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=UDP4RECV
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%ip4%*|*%dgram%*|*%udp%*|*%udp4%*|*%recv%*|*%$NAME%*)
TEST="$NAME: UDP/IPv4 receive"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; ts1p=$PORT
ts1a="127.0.0.1"
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -u UDP4-RECV:$ts1p,reuseaddr -"
CMD2="$TRACE $SOCAT $opts -u - UDP4-SENDTO:$ts1"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1="$!"
waitudp4port $ts1p 1
echo "$da" |$CMD2 2>>"${te}2"
rc2="$?"
#ls -l $tf
i=0; while [ ! -s "$tf" -a "$i" -lt 10 ]; do  relsleep 1; i=$((i+1));  done
kill "$pid1" 2>/dev/null; wait
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=UDP6RECV
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%dgram%*|*%udp%*|*%udp6%*|*%recv%*|*%$NAME%*)
TEST="$NAME: UDP/IPv6 receive"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp6; ts1p=$PORT
ts1a="[::1]"
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -u UDP6-RECV:$ts1p,reuseaddr -"
CMD2="$TRACE $SOCAT $opts -u - UDP6-SENDTO:$ts1"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1="$!"
waitudp6port $ts1p 1
echo "$da" |$CMD2 2>>"${te}2"
rc2="$?"
#ls -l $tf
i=0; while [ ! -s "$tf" -a "$i" -lt 10 ]; do  relsleep 1; i=$((i+1));  done
kill "$pid1" 2>/dev/null; wait
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=RAWIP4RECV
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%dgram%*|*%rawip%*|*%rawip4%*|*%recv%*|*%root%*|*%$NAME%*)
TEST="$NAME: raw IPv4 receive"
if ! eval $NUMCOND; then :;
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1p=$IPPROTO; #IPPROTO=$((IPPROTO+1))
ts1a="127.0.0.1"
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -u IP4-RECV:$ts1p,reuseaddr -"
CMD2="$TRACE $SOCAT $opts -u - IP4-SENDTO:$ts1"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1="$!"
waitip4proto $ts1p 1
echo "$da" |$CMD2 2>>"${te}2"
rc2="$?"
#ls -l $tf
i=0; while [ ! -s "$tf" -a "$i" -lt 10 ]; do  relsleep 1; i=$((i+1));  done
kill "$pid1" 2>/dev/null; wait
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND, root
esac
N=$((N+1))


NAME=RAWIP6RECV
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%dgram%*|*%rawip%*|*%rawip6%*|*%recv%*|*%root%*|*%$NAME%*)
TEST="$NAME: raw IPv6 receive"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip6 rawip && runsip6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1p=$IPPROTO; #IPPROTO=$((IPPROTO+1))
ts1a="[::1]"
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -u IP6-RECV:$ts1p,reuseaddr -"
CMD2="$TRACE $SOCAT $opts -u - IP6-SENDTO:$ts1"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1="$!"
waitip6proto $ts1p 1
echo "$da" |$CMD2 2>>"${te}2"
rc2="$?"
i=0; while [ ! -s "$tf" -a "$i" -lt 10 ]; do  relsleep 1; i=$((i+1));  done
kill "$pid1" 2>/dev/null; wait
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND, root
esac
N=$((N+1))


NAME=UNIXRECV
case "$TESTS" in
*%$N%*|*%functions%*|*%unix%*|*%dgram%*|*%recv%*|*%$NAME%*)
TEST="$NAME: UNIX receive"
if ! eval $NUMCOND; then :; else
ts="$td/test$N.socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1="$ts"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -u UNIX-RECV:$ts1,reuseaddr -"
CMD2="$TRACE $SOCAT $opts -u - UNIX-SENDTO:$ts1"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1="$!"
waitfile $ts1 1
echo "$da" |$CMD2 2>>"${te}2"
rc2="$?"
i=0; while [ ! -s "$tf" -a "$i" -lt 10 ]; do  relsleep 1; i=$((i+1));  done
kill "$pid1" 2>/dev/null; wait
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=UDP4RECVFROM_SOURCEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%sourceport%*|*%$NAME%*)
TEST="$NAME: security of UDP4-RECVFROM with SOURCEPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip4) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP4 not available${NORMAL}\n" $N
    cant
else
newport udp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP4-RECVFROM:$PORT,reuseaddr" "" "sp=$PORT" "UDP4-SENDTO:127.0.0.1:$PORT" 4 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP4RECVFROM_LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%lowport%*|*%$NAME%*)
TEST="$NAME: security of UDP4-RECVFROM with LOWPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip4) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP4 not available${NORMAL}\n" $N
    cant
else
newport udp4 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP4-RECVFROM:$PORT,reuseaddr" "" "lowport" "UDP4-SENDTO:127.0.0.1:$PORT" 4 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP4RECVFROM_RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%fork%*|*%udp%*|*%udp4%*|*%ip4%*|*%range%*|*%$NAME%*)
TEST="$NAME: security of UDP4-RECVFROM with RANGE option"
newport udp4 	# provide free port number in $PORT
#testserversec "$N" "$TEST" "$opts" "UDP4-RECVFROM:$PORT,reuseaddr,fork" "" "range=$SECONDADDR/32" "UDP4-SENDTO:127.0.0.1:$PORT" 4 udp $PORT 0
if ! eval $NUMCOND; then :; else
testserversec "$N" "$TEST" "$opts" "UDP4-RECVFROM:$PORT,reuseaddr" "" "range=$SECONDADDR/32" "UDP4-SENDTO:127.0.0.1:$PORT" 4 udp $PORT 0
fi ;; # NUMCOND
esac
N=$((N+1))

NAME=UDP4RECVFROM_TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%fork%*|*%udp%*|*%udp4%*|*%ip4%*|*%tcpwrap%*|*%$NAME%*)
TEST="$NAME: security of UDP4-RECVFROM with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 udp libwrap) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: $SECONDADDR" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport udp4 	# provide free port number in $PORT
#testserversec "$N" "$TEST" "$opts" "UDP4-RECVFROM:$PORT,reuseaddr,fork" "" "tcpwrap=$d" "UDP4-SENDTO:127.0.0.1:$PORT" 4 udp $PORT 0
testserversec "$N" "$TEST" "$opts" "UDP4-RECVFROM:$PORT,reuseaddr" "" "tcpwrap-etc=$td" "UDP4-SENDTO:127.0.0.1:$PORT" 4 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=UDP4RECV_SOURCEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%sourceport%*|*%$NAME%*)
TEST="$NAME: security of UDP4-RECV with SOURCEPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip4) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP4 not available${NORMAL}\n" $N
    cant
else
newport udp4; PORT1=$PORT
newport udp4; PORT2=$PORT
newport udp4; PORT3=$PORT
# we use the forward channel (PORT1) for testing, and have a backward channel
# (PORT2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "UDP4-RECV:$PORT1,reuseaddr!!UDP4-SENDTO:127.0.0.1:$PORT2" "" "sp=$PORT3" "UDP4-RECV:$PORT2!!UDP4-SENDTO:127.0.0.1:$PORT1" 4 udp $PORT1 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP4RECV_LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%lowport%*|*%$NAME%*)
TEST="$NAME: security of UDP4-RECV with LOWPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip4) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP4 not available${NORMAL}\n" $N
    cant
else
newport udp4; PORT1=$PORT
newport udp4; PORT2=$PORT
# we use the forward channel (PORT1) for testing, and have a backward channel
# (PORT2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "UDP4-RECV:$PORT1,reuseaddr!!UDP4-SENDTO:127.0.0.1:$PORT2" "" "lowport" "UDP4-RECV:$PORT2!!UDP4-SENDTO:127.0.0.1:$PORT1" 4 udp $PORT1 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP4RECV_RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%range%*|*%$NAME%*)
TEST="$NAME: security of UDP4-RECV with RANGE option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip4) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP4 not available${NORMAL}\n" $N
    cant
else
newport udp4; PORT1=$PORT
newport udp4; PORT2=$PORT
# we use the forward channel (PORT1) for testing, and have a backward channel
# (PORT2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "UDP4-RECV:$PORT1,reuseaddr!!UDP4-SENDTO:127.0.0.1:$PORT2" "" "range=$SECONDADDR/32" "UDP4-RECV:$PORT2!!UDP4-SENDTO:127.0.0.1:$PORT1" 4 udp $PORT1 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP4RECV_TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%tcpwrap%*|*%$NAME%*)
TEST="$NAME: security of UDP4-RECV with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip4 libwrap) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
newport udp4; PORT1=$PORT
newport udp4; PORT2=$PORT
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: $SECONDADDR" >"$ha"
$ECHO "ALL: ALL" >"$hd"
# we use the forward channel (PORT1) for testing, and have a backward channel
# (PORT2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "UDP4-RECV:$PORT1,reuseaddr!!UDP4-SENDTO:127.0.0.1:$PORT2" "" "tcpwrap-etc=$td" "UDP4-RECV:$PORT2!!UDP4-SENDTO:127.0.0.1:$PORT1" 4 udp $PORT1 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=UDP6RECVFROM_SOURCEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%sourceport%*|*%$NAME%*)
TEST="$NAME: security of UDP6-RECVFROM with SOURCEPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
    cant
else
newport udp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP6-RECVFROM:$PORT,reuseaddr" "" "sp=$PORT" "UDP6-SENDTO:[::1]:$PORT" 6 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP6RECVFROM_LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%lowport%*|*%$NAME%*)
TEST="$NAME: security of UDP6-RECVFROM with LOWPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
    cant
else
newport udp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP6-RECVFROM:$PORT,reuseaddr" "" "lowport" "UDP6-SENDTO:[::1]:$PORT" 6 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP6RECVFROM_RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%fork%*|*%udp%*|*%udp6%*|*%ip6%*|*%range%*|*%$NAME%*)
TEST="$NAME: security of UDP6-RECVFROM with RANGE option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
newport udp6 	# provide free port number in $PORT
#testserversec "$N" "$TEST" "$opts" "UDP6-RECVFROM:$PORT,reuseaddr,fork" "" "range=[::2]/128" "UDP6-SENDTO:[::1]:$PORT" 6 udp $PORT 0
testserversec "$N" "$TEST" "$opts" "UDP6-RECVFROM:$PORT,reuseaddr" "" "range=[::2]/128" "UDP6-SENDTO:[::1]:$PORT" 6 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP6RECVFROM_TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%tcpwrap%*|*%$NAME%*)
TEST="$NAME: security of UDP6-RECVFROM with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6 libwrap && runsip6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: [::2]" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport udp6 	# provide free port number in $PORT
testserversec "$N" "$TEST" "$opts" "UDP6-RECVFROM:$PORT,reuseaddr" "" "tcpwrap-etc=$td" "UDP6-SENDTO:[::1]:$PORT" 6 udp $PORT 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=UDP6RECV_SOURCEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%sourceport%*|*%$NAME%*)
TEST="$NAME: security of UDP6-RECV with SOURCEPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
    cant
else
newport udp6; PORT1=$PORT
newport udp6; PORT2=$PORT
newport udp6; PORT3=$PORT
# we use the forward channel (PORT1) for testing, and have a backward channel
# (PORT2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "UDP6-RECV:$PORT1,reuseaddr!!UDP6-SENDTO:[::1]:$PORT2" "" "sp=$PORT3" "UDP6-RECV:$PORT2!!UDP6-SENDTO:[::1]:$PORT1" 6 udp $PORT1 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP6RECV_LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%lowport%*|*%$NAME%*)
TEST="$NAME: security of UDP6-RECV with LOWPORT option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
    cant
else
newport udp6; PORT1=$PORT
newport udp6; PORT2=$PORT
# we use the forward channel (PORT1) for testing, and have a backward channel
# (PORT2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "UDP6-RECV:$PORT1,reuseaddr!!UDP6-SENDTO:[::1]:$PORT2" "" "lowport" "UDP6-RECV:$PORT2!!UDP6-SENDTO:[::1]:$PORT1" 6 udp $PORT1 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP6RECV_RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%range%*|*%$NAME%*)
TEST="$NAME: security of UDP6-RECV with RANGE option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
    cant
else
newport udp6; PORT1=$PORT
newport udp6; PORT2=$PORT
# we use the forward channel (PORT1) for testing, and have a backward channel
# (PORT2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "UDP6-RECV:$PORT1,reuseaddr!!UDP6-SENDTO:[::1]:$PORT2" "" "range=[::2]/128" "UDP6-RECV:$PORT2!!UDP6-SENDTO:[::1]:$PORT1" 6 udp $PORT1 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=UDP6RECV_TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp6%*|*%ip6%*|*%tcpwrap%*|*%$NAME%*)
TEST="$NAME: security of UDP6-RECV with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6 libwrap && runsip6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: [::2]" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport udp6; PORT1=$PORT
newport udp6; PORT2=$PORT
# we use the forward channel (PORT1) for testing, and have a backward channel
# (PORT2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "UDP6-RECV:$PORT1,reuseaddr!!UDP6-SENDTO:[::1]:$PORT2" "" "tcpwrap-etc=$td" "UDP6-RECV:$PORT2!!UDP6-SENDTO:[::1]:$PORT1" 6 udp $PORT1 0
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=IP4RECVFROM_RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%fork%*|*%ip%*|*%ip4%*|*%range%*|*%root%*|*%$NAME%*)
TEST="$NAME: security of IP4-RECVFROM with RANGE option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 rawip) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
newport udp4 	# provide free port number in $PORT
#testserversec "$N" "$TEST" "$opts" "IP4-RECVFROM:$IPPROTO,reuseaddr,fork" "" "range=$SECONDADDR/32" "IP4-SENDTO:127.0.0.1:$IPPROTO" 4 ip $IPPROTO 0
testserversec "$N" "$TEST" "$opts" "IP4-RECVFROM:$IPPROTO,reuseaddr!!UDP4-SENDTO:127.0.0.1:$PORT" "" "range=$SECONDADDR/32" "UDP4-RECV:$PORT!!IP4-SENDTO:127.0.0.1:$IPPROTO" 4 ip $IPPROTO 0
fi ;; # NUMCOND, feats, root
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))

NAME=IP4RECVFROM_TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%fork%*|*%ip%*|*%ip4%*|*%tcpwrap%*|*%root%*|*%$NAME%*)
TEST="$NAME: security of IP4-RECVFROM with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 rawip libwrap) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: $SECONDADDR" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport udp4 	# provide free port number in $PORT
#testserversec "$N" "$TEST" "$opts" "IP4-RECVFROM:$IPPROTO,reuseaddr,fork" "" "tcpwrap-etc=$td" "IP4-SENDTO:127.0.0.1:$IPPROTO" 4 ip $IPPROTO 0
testserversec "$N" "$TEST" "$opts" "IP4-RECVFROM:$IPPROTO,reuseaddr!!UDP4-SENDTO:127.0.0.1:$PORT" "" "tcpwrap-etc=$td" "UDP4-RECV:$PORT!!IP4-SENDTO:127.0.0.1:$IPPROTO" 4 ip $IPPROTO 0
fi # NUMCOND, feats, root
 ;;
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))


NAME=IP4RECV_RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%ip%*|*%ip4%*|*%range%*|*%root%*|*%$NAME%*)
TEST="$NAME: security of IP4-RECV with RANGE option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 rawip) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP4 not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
IPPROTO1=$IPPROTO; #IPPROTO=$((IPPROTO+1))
IPPROTO2=$((IPPROTO+1))
# we use the forward channel (IPPROTO1) for testing, and have a backward channel
# (IPPROTO2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "ip4-recv:$IPPROTO1,reuseaddr!!ip4-sendto:127.0.0.1:$IPPROTO2" "" "range=$SECONDADDR/32" "ip4-recv:$IPPROTO2!!ip4-sendto:127.0.0.1:$IPPROTO1" 4 ip $IPPROTO1 0
fi ;; # NUMCOND, feats, root
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))



NAME=IP4RECV_TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%ip%*|*%ip4%*|*%tcpwrap%*|*%root%*|*%$NAME%*)
TEST="$NAME: security of IP4-RECV with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 rawip libwrap) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
IPPROTO1=$IPPROTO; #IPPROTO=$((IPPROTO+1))
IPPROTO2=$((IPPROTO+1))
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: $SECONDADDR" >"$ha"
$ECHO "ALL: ALL" >"$hd"
# we use the forward channel (IPPROTO1) for testing, and have a backward channel
# (IPPROTO2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "ip4-recv:$IPPROTO1,reuseaddr!!ip4-sendto:127.0.0.1:$IPPROTO2" "" "tcpwrap-etc=$td" "ip4-recv:$IPPROTO2!!ip4-sendto:127.0.0.1:$IPPROTO1" 4 ip $IPPROTO1 0
fi ;; # NUMCOND, feats, root
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))


NAME=IP6RECVFROM_RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%fork%*|*%ip%*|*%ip6%*|*%range%*|*%root%*|*%$NAME%*)
TEST="$NAME: security of IP6-RECVFROM with RANGE option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip6 rawip && runsip6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
newport udp6 	# provide free port number in $PORT
#testserversec "$N" "$TEST" "$opts" "IP6-RECVFROM:$IPPROTO,reuseaddr,fork" "" "range=[::2]/128" "IP6-SENDTO:[::1]:$IPPROTO" 6 ip $IPPROTO 0
testserversec "$N" "$TEST" "$opts" "IP6-RECVFROM:$IPPROTO,reuseaddr!!UDP6-SENDTO:[::1]:$PORT" "" "range=[::2]/128" "UDP6-RECV:$PORT!!IP6-SENDTO:[::1]:$IPPROTO" 6 ip $IPPROTO 0
fi ;; # NUMCOND, feats
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))

NAME=IP6RECVFROM_TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%fork%*|*%ip%*|*%ip6%*|*%tcpwrap%*|*%root%*|*%$NAME%*)
TEST="$NAME: security of IP6-RECVFROM with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip6 rawip libwrap && runsip6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: [::2]" >"$ha"
$ECHO "ALL: ALL" >"$hd"
newport udp6 	# provide free port number in $PORT
#testserversec "$N" "$TEST" "$opts" "IP6-RECVFROM:$IPPROTO,reuseaddr,fork" "" "tcpwrap-etc=$td" "IP6-SENDTO:[::1]:$IPPROTO" 6 ip $IPPROTO 0
testserversec "$N" "$TEST" "$opts" "IP6-RECVFROM:$IPPROTO,reuseaddr!!UDP6-SENDTO:[::1]:$PORT" "" "tcpwrap-etc=$td" "UDP6-RECV:$PORT!!IP6-SENDTO:[::1]:$IPPROTO" 6 ip $IPPROTO 0
fi ;; # NUMCOND, feats
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))


NAME=IP6RECV_RANGE
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%ip%*|*%ip6%*|*%range%*|*%root%*|*%$NAME%*)
TEST="$NAME: security of IP6-RECV with RANGE option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip6 rawip) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}raw IP6 not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
IPPROTO1=$IPPROTO; #IPPROTO=$((IPPROTO+1))
IPPROTO2=$((IPPROTO+1))
# we use the forward channel (IPPROTO1) for testing, and have a backward channel
# (IPPROTO2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "ip6-recv:$IPPROTO1,reuseaddr!!ip6-sendto:[::1]:$IPPROTO2" "" "range=[::2]/128" "ip6-recv:$IPPROTO2!!ip6-sendto:[::1]:$IPPROTO1" 6 ip $IPPROTO1 0
fi ;; # NUMCOND, feats
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))

NAME=IP6RECV_TCPWRAP
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%ip%*|*%ip6%*|*%tcpwrap%*|*%root%*|*%$NAME%*)
TEST="$NAME: security of IP6-RECV with TCPWRAP option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip6 rawip libwrap && runsip6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
IPPROTO1=$IPPROTO; #IPPROTO=$((IPPROTO+1))
IPPROTO2=$((IPPROTO+1))
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat: [::2]" >"$ha"
$ECHO "ALL: ALL" >"$hd"
# we use the forward channel (IPPROTO1) for testing, and have a backward channel
# (IPPROTO2) to get the data back, so we get the classical echo behaviour
testserversec "$N" "$TEST" "$opts" "ip6-recv:$IPPROTO1,reuseaddr!!ip6-sendto:[::1]:$IPPROTO2" "" "tcpwrap-etc=$td" "ip6-recv:$IPPROTO2!!ip6-sendto:[::1]:$IPPROTO1" 6 ip $IPPROTO1 0
fi ;; # NUMCOND, feats
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))


NAME=O_NOATIME_FILE
case "$TESTS" in
*%$N%*|*%functions%*|*%open%*|*%noatime%*|*%$NAME%*)
TEST="$NAME: option O_NOATIME on file"
# idea: create a file with o-noatime option; one second later create a file
# without this option (using touch); one second later read from the first file.
# Then we check which file has the later ATIME stamp. For this check we use
# "ls -ltu" because it is more portable than "test ... -nt ..."
if ! eval $NUMCOND; then :;
elif ! testoptions o-noatime >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}o-noatime not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.file"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
$PRINTF "test $F_n $TEST... " $N
CMD="$TRACE $SOCAT $opts -u open:\"${tf}1\",o-noatime /dev/null"
# generate a file
touch "${tf}1"
sleep 1
# generate a reference file
touch "${tf}2"
sleep 1
# read from the first file
$CMD 2>"$te"
if [ $? -ne 0 ]; then # command failed
    $PRINTF "${FAILED}:\n"
    echo "$CMD"
    cat "$te"
    failed
else
# check which file has a later atime stamp
if [ $(ls -ltu "${tf}1" "${tf}2" |head -1 |sed 's/.* //') != "${tf}2" ];
then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD"
   cat "$te"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "$te"; fi
   ok
fi # wrong time stamps
fi # command ok
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=O_NOATIME_FD
case "$TESTS" in
*%$N%*|*%functions%*|*%noatime%*|*%$NAME%*)
TEST="$NAME: option O_NOATIME on file descriptor"
# idea: use a fd of a file with o-noatime option; one second later create a file
# without this option (using touch); one second later read from the first file.
# Then we check which file has the later ATIME stamp. For this check we use
# "ls -ltu" because it is more portable than "test ... -nt ..."
if ! eval $NUMCOND; then :;
elif ! testoptions o-noatime >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}o-noatime not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.file"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
$PRINTF "test $F_n $TEST... " $N
touch ${tf}1
CMD="$TRACE $SOCAT $opts -u -,o-noatime /dev/null <${tf}1"
# generate a file, len >= 1
touch "${tf}1"
sleep 1
# generate a reference file
touch "${tf}2"
sleep 1
# read from the first file
sh -c "$CMD" 2>"$te"
rc=$?
if [ $rc -ne 0 ]; then # command failed
    $PRINTF "${FAILED} (rc=$rc):\n"
    echo "$CMD"
    cat "$te" >&2
    failed
else
# check which file has a later atime stamp
if [ $(ls -ltu "${tf}1" "${tf}2" |head -1 |sed 's/.* //') != "${tf}2" ];
then
    $PRINTF "$FAILED (bad order):\n"
    echo "$CMD" >&2
    cat "$te"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD"; fi
    if [ "$DEBUG" ];   then cat "${te}" >&2; fi
    ok
fi # wrong time stamps
fi # command ok
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=FS_NOATIME
case "$TESTS" in
*%$N%*|*%functions%*|*%fs%*|*%noatime%*|*%$NAME%*)
TEST="$NAME: extended file system options using fs noatime option"
# idea: create a file with fs-noatime option; one second later create a file
# without this option (using touch); one second later read from the first file.
# Then we check which file has the later ATIME stamp. For this check we use
# "ls -ltu" because it is more portable than "test ... -nt ..."
if ! eval $NUMCOND; then :;
elif ! testoptions fs-noatime >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}fs-noatime not available${NORMAL}\n" $N
    cant
else
ts="$td/test$N.socket"
tf="$td/test$N.file"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1="$ts"
da="test$N $(date) $RANDOM"
$PRINTF "test $F_n $TEST... " $N
CMD0="$TRACE $SOCAT $opts -u /dev/null create:\"${tf}1\""
CMD="$TRACE $SOCAT $opts -u /dev/null create:\"${tf}1\",fs-noatime"
# check if this is a capable FS; lsattr does other things on AIX, thus socat
$CMD0 2>"${te}0"
if [ $? -ne 0 ]; then
    $PRINTF "${YELLOW} cannot test${NORMAL}\n"
    cant
else
# generate a file with noatime, len >= 1
$CMD 2>"$te"
if [ $? -ne 0 ]; then # command failed
    $PRINTF "${YELLOW}impotent file system?${NORMAL}\n"
    echo "$CMD"
    cat "$te"
    cant
else
sleep 1
# generate a reference file
touch "${tf}2"
sleep 1
# read from the first file
cat "${tf}1" >/dev/null
# check which file has a later atime stamp
#if [ $(ls -ltu "${tf}1" "${tf}2" |head -n 1 |awk '{print($8);}') != "${tf}2" ];
if [ $(ls -ltu "${tf}1" "${tf}2" |head -n 1 |sed "s|.*\\($td.*\\)|\1|g") != "${tf}2" ];
then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD"
   cat "$te"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "$te"; fi
   ok
fi
fi # not impotent
fi # can test
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=COOLWRITE
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%timeout%*|*%coolwrite%*|*%$NAME%*)
TEST="$NAME: option cool-write"
if ! eval $NUMCOND; then :;
elif ! testoptions cool-write >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}option cool-write not available${NORMAL}\n" $N
    cant
else
#set -vx
ti="$td/test$N.pipe"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
# a reader that will terminate after 1 byte
CMD1="$TRACE $SOCAT $opts -u pipe:\"$ti\",readbytes=1 /dev/null"
CMD="$TRACE $SOCAT $opts -u - file:\"$ti\",cool-write"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
bg=$!	# background process id
sleep 1
(echo .; sleep 1; echo) |$CMD 2>"$te"
rc=$?
kill $bg 2>/dev/null; wait
if [ $rc -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD &"
    cat "$te"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "$te"; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# test if option coolwrite can be applied to bidirectional address stdio
# this failed up to socat 1.6.0.0
NAME=COOLSTDIO
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%timeout%*|*%coolwrite%*|*%$NAME%*)
TEST="$NAME: option cool-write on bidirectional stdio"
# this test starts a socat reader that terminates after receiving one+ 
# bytes (option readbytes); and a test process that sends two bytes via
# named pipe to the receiving process and, a second later, sends another
# byte. The last write will fail with "broken pipe"; if option coolwrite
# has been applied successfully, socat will terminate with 0 (OK),
# otherwise with error.
if ! eval $NUMCOND; then :;
elif ! testoptions cool-write >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}option cool-write not available${NORMAL}\n" $N
    cant
else
#set -vx
ti="$td/test$N.pipe"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
# a reader that will terminate after 1 byte
CMD1="$TRACE $SOCAT $opts -u pipe:\"$ti\",readbytes=1 /dev/null"
CMD="$TRACE $SOCAT $opts -,cool-write pipe >\"$ti\""
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
bg=$!	# background process id
sleep 1
(echo .; sleep 1; echo) |eval "$CMD" 2>"$te"
rc=$?
kill $bg 2>/dev/null; wait
if [ $rc -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD"
    cat "$te"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "$te"; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=TCP4ENDCLOSE
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%tcp%*|*%listen%*|*%fork%*|*%$NAME%*)
TEST="$NAME: end-close keeps TCP V4 socket open"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp4; p0=$PORT
newport tcp4; p1=$PORT
da2a="$(date) $RANDOM"
da2b="$(date) $RANDOM"
CMD0="$TRACE $SOCAT -lp collector $opts -u TCP4-LISTEN:$p0,$REUSEADDR,bind=$LOCALHOST -"
CMD1="$TRACE $SOCAT -lp forker $opts -U TCP4:$LOCALHOST:$p0,end-close TCP4-LISTEN:$p1,bind=$LOCALHOST,$REUSEADDR,fork"
CMD2="$TRACE $SOCAT -lp client $opts -u - TCP4-CONNECT:$LOCALHOST:$p1"
printf "test $F_n $TEST... " $N
$CMD0 >"${tf}0" 2>"${te}0" &
pid0=$!
waittcp4port $p0 1
$CMD1 2>"${te}1" &
pid1=$!
relsleep 1
waittcp4port $p1 1
echo "$da2a" |$CMD2 2>>"${te}2a"
rc2a=$?
echo "$da2b" |$CMD2 2>>"${te}2b"
rc2b=$?
sleep 1
kill "$pid0" "$pid1" 2>/dev/null
wait
if [ $rc2a -ne 0 -o $rc2b -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2a" >&2
    echo "$CMD2"
    cat "${te}2b" >&2
    failed
elif ! $ECHO "$da2a\n$da2b" |diff - "${tf}0" >"$tdiff"; then
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2a" >&2
    echo "$CMD2"
    cat "${te}2b" >&2
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2b" >&2; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=EXECENDCLOSE
case "$TESTS" in
*%$N%*|*%functions%*|*%exec%*|*%listen%*|*%unix%*|*%fork%*|*%$NAME%*)
TEST="$NAME: end-close keeps EXEC child running"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
ts="$td/test$N.sock"
tdiff="$td/test$N.diff"
da1a="$(date) $RANDOM"
da1b="$(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts - UNIX-CONNECT:$ts"
CMD="$TRACE $SOCAT $opts EXEC:"$CAT",end-close UNIX-LISTEN:$ts,fork"
printf "test $F_n $TEST... " $N
$CMD 2>"${te}2" &
pid2=$!
waitfile $ts 1
echo "$da1a" |$CMD1 2>>"${te}1a" >"$tf"
relsleep 1
echo "$da1b" |$CMD1 2>>"${te}1b" >>"$tf"
#relsleep 1
kill "$pid2" 2>/dev/null
wait
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1a" "${te}1b" "${te}2"
    failed
elif ! $ECHO "$da1a\n$da1b" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   cat "${te}1a" "${te}1b" "${te}2"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1a" "${te}1b" "${te}2"; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# up to 1.7.0.0 option end-close led to an error with some address types due to
# bad internal handling. here we check it for address PTY
NAME=PTYENDCLOSE
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%pty%*|*%$NAME%*)
TEST="$NAME: PTY handles option end-close"
# with the bug, socat exits with error. we invoke socat in a no-op mode and
# check its return status.
if ! eval $NUMCOND; then :;
 else
tf="$td/test$N.stout"
te="$td/test$N.stderr"
# -t  must be longer than 0.1 on OpenBSD
CMD="$TRACE $SOCAT $opts -d -d -t 0.5 /dev/null pty,end-close"
printf "test $F_n $TEST... " $N
# AIX reports the pty writeable for select() only when its slave side has been
# opened, therefore we run this process in background and check its NOTICE
# output for the PTY name
{ $CMD 2>"${te}"; echo $? >"$td/test$N.rc0"; } &
waitfile "${te}"
sleep 0.5	# 0.1 is too few for FreeBSD-10
PTY=$(grep "N PTY is " $te |sed 's/.*N PTY is //')
# So this for AIX? but "cat" hangs on OpenBSD, thus use socat with timeout instead
[ -e "$PTY" ] && $SOCAT -T 0.1 -u $PTY,o-nonblock - >/dev/null 2>/dev/null
rc=$(cat "$td/test$N.rc0")
if [ "$rc" = 0 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD"
    cat "${te}"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the shut-null and null-eof options
NAME=SHUTNULLEOF
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%$NAME%*)
TEST="$NAME: options shut-null and null-eof"
# Run a receiving background process with option null-eof. 
# Start a sending process with option shut-null that sends a test record to the
# receiving process and then terminates.
# Send another test record.
# When the receiving process only received and stored the first test record the
# test succeeded
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport udp4 	# provide free port number in $PORT
CMD0="$TRACE $SOCAT $opts -u UDP4-RECV:$PORT,null-eof CREAT:$tf"
CMD1="$TRACE $SOCAT $opts -u - UDP4-SENDTO:127.0.0.1:$PORT,shut-null"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waitudp4port $PORT 1
{ echo "$da"; sleep 0.1; } |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
{ echo "xyz"; sleep 0.1; } |$CMD1 >"${tf}2" 2>"${te}2"
rc2=$?
kill $pid0 2>/dev/null; wait
if [ $rc1 != 0 -o $rc2 != 0 ]; then
    $PRINTF "$FAILED (client(s) failed)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD1"
    cat "${te}2" >&2
    failed
elif echo "$da" |diff - "${tf}" >"$tdiff"; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
else
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    echo "// diff:" >&2
    cat "${tdiff}" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=UDP6LISTENBIND
# this tests for a bug in (up to) 1.5.0.0:
#    with udp*-listen, the bind option supported only IPv4
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%ip6%*|*%ipapp%*|*%udp%*|*%udp6%*|*%listen%*|*%$NAME%*)
TEST="$NAME: UDP6-LISTEN with bind"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats udp ip6) || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp6; tsl=$PORT
ts="$LOCALHOST6:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts UDP6-LISTEN:$tsl,$REUSEADDR,bind=$LOCALHOST6 PIPE"
CMD2="$TRACE $SOCAT $opts - UDP6:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1=$!
waitudp6port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2=$?
kill $pid1 2>/dev/null; wait
if [ $rc2 -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1" "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=TCPWRAPPERS_MULTIOPTS
# this tests for a bug in 1.5.0.0 that let socat fail when more than one 
# tcp-wrappers related option was specified in one address
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%tcpwrap%*|*%listen%*|*%$NAME%*)
TEST="$NAME: use of multiple tcpwrapper enabling options"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip4 libwrap) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
ha="$td/hosts.allow"
$ECHO "test : ALL : allow" >"$ha"
newport tcp4 	# provide free port number in $PORT
CMD1="$TRACE $SOCAT $opts TCP4-LISTEN:$PORT,$REUSEADDR,hosts-allow=$ha,tcpwrap=test pipe"
CMD2="$TRACE $SOCAT $opts - TCP4:$LOCALHOST:$PORT"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
pid1=$!
waittcp4port $PORT
echo "$da" |$CMD2 >"$tf" 2>"${te}2"
rc2=$?
kill $pid1 2>/dev/null; wait
if [ $rc2 -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=TCPWRAPPERS_TCP6ADDR
# this tests for a bug in 1.5.0.0 that brought false results with tcp-wrappers
# and IPv6 when 
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%tcpwrap%*|*%listen%*|*%$NAME%*)
TEST="$NAME: specification of TCP6 address in hosts.allow"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats tcp ip6 libwrap && runsip6); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
ha="$td/hosts.allow"
hd="$td/hosts.deny"
$ECHO "socat : [::1] : allow" >"$ha"
$ECHO "ALL : ALL : deny" >"$hd"
newport tcp6 	# provide free port number in $PORT
CMD1="$TRACE $SOCAT $opts TCP6-LISTEN:$PORT,$REUSEADDR,tcpwrap-etc=$td,tcpwrappers=socat pipe"
CMD2="$TRACE $SOCAT $opts - TCP6:[::1]:$PORT"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
pid1=$!
waittcp6port $PORT
echo "$da" |$CMD2 >"$tf" 2>"${te}2"
rc2=$?
kill $pid1 2>/dev/null; wait
if [ $rc2 -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=UDP4BROADCAST
case "$TESTS" in
*%$N%*|*%functions%*|*%udp%*|*%udp4%*|*%ip4%*|*%dgram%*|*%broadcast%*|*%$NAME%*)
TEST="$NAME: UDP/IPv4 broadcast"
if ! eval $NUMCOND; then :;
elif [ -z "$BCADDR" ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}dont know a broadcast address${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; ts1p=$PORT
#ts1="$BCADDR/8:$ts1p"
ts1="$BCADDR:$ts1p"
newport udp4; ts2p=$PORT
ts2="$BCIFADDR:$ts2p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts UDP4-RECVFROM:$ts1p,reuseaddr,broadcast PIPE"
#CMD2="$TRACE $SOCAT $opts - UDP4-BROADCAST:$ts1"
CMD2="$TRACE $SOCAT $opts - UDP4-DATAGRAM:$ts1,broadcast"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
pid1="$!"
waitudp4port $ts1p 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2="$?"
kill "$pid1" 2>/dev/null; wait;
if [ "$rc2" -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    echo "$CMD2"
    cat "${te}1"
    cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED\n"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$tut" ]; then
	echo "$CMD1 &"
	echo "$CMD2"
    fi
    if [ -n "$debug" ]; then cat $te; fi
    ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=IP4BROADCAST
# test a local broadcast of a raw IPv4 protocol.
# because we receive - in addition to the regular reply - our own broadcast,
# we use a token XXXX that is changed to YYYY in the regular reply packet.
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%rawip%*|*%rawip4%*|*%ip4%*|*%dgram%*|*%broadcast%*|*%root%*|*%$NAME%*)
TEST="$NAME: raw IPv4 broadcast"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 rawip) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}raw IP4 not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
elif [ -z "$BCADDR" ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}dont know a broadcast address${NORMAL}\n" $N
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1p=$IPPROTO
#ts1="$BCADDR/8:$ts1p"
ts1="$BCADDR:$ts1p"
ts2p=$ts1p
ts2="$BCIFADDR"
da="test$N $(date) $RANDOM XXXX"
sh="$td/test$N-sed.sh"
echo 'sed s/XXXX/YYYY/' >"$sh"
chmod a+x "$sh"
# EXEC need not work with script (musl libc), so use SYSTEM
CMD1="$TRACE $SOCAT $opts IP4-RECVFROM:$ts1p,reuseaddr,broadcast SYSTEM:$sh"
#CMD2="$TRACE $SOCAT $opts - IP4-BROADCAST:$ts1"
CMD2="$TRACE $SOCAT $opts - IP4-DATAGRAM:$ts1,broadcast"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
pid1="$!"
waitip4port $ts1p 1
echo "$da" |$CMD2 2>>"${te}2" |grep -v XXXX >>"$tf"
rc2="$?"
kill "$pid1" 2>/dev/null; wait;
if [ "$rc2" -ne 0 ]; then
    $PRINTF "$FAILED (rc2=$rc2):\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
elif ! echo "$da" | sed 's/XXXX/YYYY/'|diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    echo "// diff:" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD2"; fi
	if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi ;; # NUMCOND, feats
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))


#NAME=UDP4BROADCAST_RANGE
#case "$TESTS" in
#*%$N%*|*%functions%*|*%security%*|*%udp%*|*%udp4%*|*%ip4%*|*%dgram%*|*%broadcast%*|*%range%*|*%$NAME%*)
#TEST="$NAME: security of UDP4-BROADCAST with RANGE option"
#if ! eval $NUMCOND; then :;
#elif [ -z "$BCADDR" ]; then
#    $PRINTF "test $F_n $TEST... ${YELLOW}dont know a broadcast address${NORMAL}\n" $N
#else
#newport udp4 	# provide free port number in $PORT
#testserversec "$N" "$TEST" "$opts" "UDP4-BROADCAST:$BCADDR/8:$PORT" "" "range=127.1.0.0:255.255.0.0" "udp4:127.1.0.0:$PORT" 4 udp $PORT 0
#fi ;; # NUMCOND, feats
#esac
#N=$((N+1))


NAME=UDP4MULTICAST_UNIDIR
case "$TESTS" in
*%$N%*|*%functions%*|*%udp%*|*%udp4%*|*%ip4%*|*%dgram%*|*%multicast%*|*%$NAME%*)
TEST="$NAME: UDP/IPv4 multicast, send only"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 udp) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs UDP4-RECV UDP4-SENDTO); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $a not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions ip-add-membership bind) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; ts1p=$PORT
ts1a="$SECONDADDR"
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT -u $opts UDP4-RECV:$ts1p,reuseaddr,ip-add-membership=224.255.255.254:$ts1a -"
CMD2="$TRACE $SOCAT -u $opts - UDP4-SENDTO:224.255.255.254:$ts1p,bind=$ts1a"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1"  >"${tf}" &
pid1="$!"
waitudp4port $ts1p 1
echo "$da" |$CMD2 2>>"${te}2"
rc2="$?"
relsleep 1
kill "$pid1" 2>/dev/null; wait;
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   cat "${te}1" >&2
   echo "$CMD2"
   cat "${te}2" >&2
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   echo "$CMD1 &"
   cat "${te}1" >&2
   echo "$CMD2"
   cat "${te}2" >&2
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=IP4MULTICAST_UNIDIR
case "$TESTS" in
*%$N%*|*%functions%*|*%rawip%*|*%ip4%*|*%dgram%*|*%multicast%*|*%root%*|*%$NAME%*)
TEST="$NAME: IPv4 multicast"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 rawip) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1p=$IPPROTO
ts1a="$SECONDADDR"
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT -u $opts IP4-RECV:$ts1p,reuseaddr,ip-add-membership=224.255.255.254:$ts1a -"
CMD2="$TRACE $SOCAT -u $opts - IP4-SENDTO:224.255.255.254:$ts1p,bind=$ts1a"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1"  >"${tf}" &
pid1="$!"
waitip4proto $ts1p 1
relsleep 1
echo "$da" |$CMD2 2>>"${te}2"
rc2="$?"
#relsleep 1
sleep 1
kill "$pid1" 2>/dev/null; wait;
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))

if true; then
# This test succeeds, e.g., on CentOS-7 with kernel 3.10.0, Ubuntu-16.04 with 4.4.0
# but fails, e.g., on Ubuntu-18.04 with kernel 4.15.0, CentOS-8 with 4.10.0
NAME=UDP6MULTICAST_UNIDIR
case "$TESTS" in
*%$N%*|*%functions%*|*%udp%*|*%udp6%*|*%ip6%*|*%dgram%*|*%multicast%*|*%$NAME%*)
TEST="$NAME: UDP/IPv6 multicast"
if ! eval $NUMCOND; then :;
elif ! f=$(testfeats ip6 udp); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $f not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs - STDIO UDP6-RECV UDP6-SENDTO); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $a not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions ipv6-join-group) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv6 does not work on $HOSTNAME${NORMAL}\n" $N
    cant
elif ! echo |$SOCAT -u -t 0.1 - UDP6-SENDTO:[ff02::1]:12002 >/dev/null 2>&1; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv6 multicasting does not work${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp6; ts1p=$PORT
if1="$MCINTERFACE"
ts1a="[::1]"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT -u $opts UDP6-RECV:$ts1p,$REUSEADDR,ipv6-join-group=[ff02::2]:$if1 -"
#CMD2="$TRACE $SOCAT -u $opts - UDP6-SENDTO:[ff02::2]:$ts1p,bind=$ts1a"
CMD2="$TRACE $SOCAT -u $opts - UDP6-SENDTO:[ff02::2]:$ts1p"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1"  >"${tf}" &
pid1="$!"
waitudp6port $ts1p 1
echo "$da" |$CMD2 2>>"${te}2"
rc2="$?"
relsleep 1
kill "$pid1" 2>/dev/null; wait;
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   cat "${te}1"
   echo "$CMD2"
   cat "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
#    if ! [ "$UNAME" = Linux ] || ! [[ $(uname -r) =~ ^2\.* ]] || ! [[ ^3\.* ]] || ! [[ ^4\.[0-4]\.* ]]; then
#   $PRINTF "${YELLOW}works only on Linux up to about 4.4${NORMAL}\n" $N
#   cant
#    else
   $PRINTF "$FAILED\n"
   echo "$CMD1 &"
   cat "${te}1"
   echo "$CMD2"
   cat "${te}2"
   cat "$tdiff"
   failed
#    fi
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))
fi # false

NAME=UDP4MULTICAST_BIDIR
case "$TESTS" in
*%$N%*|*%functions%*|*%udp%*|*%udp4%*|*%ip4%*|*%dgram%*|*%multicast%*|*%$NAME%*)
TEST="$NAME: UDP/IPv4 multicast, with reply"
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; ts1p=$PORT
ts1a="$SECONDADDR"
ts1="$ts1a:$ts1p"
newport udp4; ts2p=$PORT
ts2="$BCIFADDR:$ts2p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts UDP4-RECVFROM:$ts1p,reuseaddr,ip-add-membership=224.255.255.254:$ts1a PIPE"
#CMD2="$TRACE $SOCAT $opts - UDP4-MULTICAST:224.255.255.254:$ts1p,bind=$ts1a"
CMD2="$TRACE $SOCAT $opts - UDP4-DATAGRAM:224.255.255.254:$ts1p,bind=$ts1a"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
pid1="$!"
waitudp4port $ts1p 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2="$?"
kill "$pid1" 2>/dev/null; wait;
if [ "$rc2" -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    echo diff: >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))

NAME=IP4MULTICAST_BIDIR
case "$TESTS" in
*%$N%*|*%functions%*|*%rawip%*|*%ip4%*|*%dgram%*|*%multicast%*|*%root%*|*%$NAME%*)
TEST="$NAME: IPv4 multicast, with reply"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 rawip) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1p=$IPPROTO
ts1a="$SECONDADDR"
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts IP4-RECVFROM:$ts1p,reuseaddr,ip-add-membership=224.255.255.254:$ts1a PIPE"
#CMD2="$TRACE $SOCAT $opts - IP4-MULTICAST:224.255.255.254:$ts1p,bind=$ts1a"
CMD2="$TRACE $SOCAT $opts - IP4-DATAGRAM:224.255.255.254:$ts1p,bind=$ts1a"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}1" &
pid1="$!"
waitip4port $ts1p 1
relsleep 1	# give process a chance to add multicast membership
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2="$?"
kill "$pid1" 2>/dev/null; wait;
if [ "$rc2" -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    echo "$CMD2"
    cat "${te}1"
    cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED\n"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$tut" ]; then
	echo "$CMD1 &"
	echo "$CMD2"
    fi
    if [ -n "$debug" ]; then cat $te; fi
    ok
fi
fi ;; # NUMCOND, feats
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))


NAME=TUNREAD
case "$TESTS" in
*%$N%*|*%functions%*|*%tun%*|*%root%*|*%$NAME%*)
TEST="$NAME: reading data sent through tun interface"
#idea: create a TUN interface and send a datagram to one of the addresses of
# its virtual network. On the tunnel side, read the packet and compare its last
# bytes with the datagram payload
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 tun) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
tl="$td/test$N.lock"
da="test$N $(date) $RANDOM"
dalen=$((${#da}+1))
TUNNET=10.255.255
newport udp4 	# provide free port number in $PORT
CMD1="$TRACE $SOCAT $opts -u - UDP4-SENDTO:$TUNNET.2:$PORT"
#CMD="$TRACE $SOCAT $opts -u -L $tl TUN,ifaddr=$TUNNET.1,netmask=255.255.255.0,iff-up=1 -"
CMD="$TRACE $SOCAT $opts -u -L $tl TUN:$TUNNET.1/24,iff-up=1 -"
printf "test $F_n $TEST... " $N
$CMD 2>"${te}" |tail -c $dalen >"${tf}" &
sleep 1
echo "$da" |$CMD1 2>"${te}1"
sleep 1
kill "$(cat $tl 2>/dev/null)" 2>/dev/null
wait
if [ $? -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD &"
    echo "$CMD1"
    cat "${te}" "${te}1"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED\n"
    echo "$CMD &"
    echo "$CMD1"
    cat "$tdiff"
    cat "${te}" "${te}1"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}" "${te}1"; fi
    ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# use the INTERFACE address on a tun/tap device and transfer data fully
# transparent 
NAME=TUNINTERFACE
case "$TESTS" in
*%$N%*|*%functions%*|*%tun%*|*%interface%*|*%root%*|*%$NAME%*)
TEST="$NAME: pass data through tun interface using INTERFACE"
#idea: create a TUN interface and send a raw packet on the interface side.
# It should arrive unmodified on the tunnel side.
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats ip4 tun interface) || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
tl="$td/test$N.lock"
da="$(date) $RANDOM"
TUNNET=10.255.255
TUNNAME=tun9
CMD0="$TRACE $SOCAT $opts -L $tl TUN:$TUNNET.1/24,iff-up=1,tun-type=tun,tun-name=$TUNNAME PIPE"
CMD1="$TRACE $SOCAT $opts - INTERFACE:$TUNNAME"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}1" &
pid0="$!"
#waitinterface "$TUNNAME"
relsleep 1
{ echo "$da"; relsleep 1; } |$CMD1 >"$tf" 2>"${te}"
rc1=$?
relsleep 1
kill $pid0 2>/dev/null
wait
if [ "$rc1" -ne 0 ]; then
    $PRINTF "$FAILED (rc1=$rc1):\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "// diff:"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=ABSTRACTSTREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%unix%*|*%abstract%*|*%connect%*|*%listen%*|*%$NAME%*)
TEST="$NAME: abstract UNIX stream socket, listen and connect"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats abstract-unixsocket); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
ts="$td/test$N.socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da1="test$N $(date) $RANDOM"
#establish a listening abstract unix socket
SRV="$TRACE $SOCAT $opts -lpserver ABSTRACT-LISTEN:\"$ts\",$REUSEADDR PIPE"
#make a connection
CMD="$TRACE $SOCAT $opts - ABSTRACT-CONNECT:$ts"
$PRINTF "test $F_n $TEST... " $N
touch "$ts"	# make a file with same name, so non-abstract fails
eval "$SRV 2>${te}s &"
pids=$!
#waitfile "$ts"
sleep 1
echo "$da1" |eval "$CMD" >"${tf}1" 2>"${te}1"
if [ $? -ne 0 ]; then
    kill "$pids" 2>/dev/null
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    cat "${te}s"
    echo "$CMD"
    cat "${te}1"
    failed
elif ! echo "$da1" |diff - "${tf}1" >"$tdiff"; then
    kill "$pids" 2>/dev/null
    $PRINTF "$FAILED:\n"
    echo "$SRV &"
    cat "${te}s"
    echo "$CMD"
    cat "${te}1"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi # !(rc -ne 0)
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=ABSTRACTDGRAM
case "$TESTS" in
*%$N%*|*%functions%*|*%unix%*|*%abstract%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: abstract UNIX datagram"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats abstract-unixsocket); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1="$td/test$N.socket1"
ts2="$td/test$N.socket2"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts ABSTRACT-RECVFROM:$ts1,reuseaddr PIPE"
#CMD2="$TRACE $SOCAT $opts - ABSTRACT-SENDTO:$ts1,bind=$ts2"
CMD2="$TRACE $SOCAT $opts - ABSTRACT-SENDTO:$ts1,bind=$ts2"
printf "test $F_n $TEST... " $N
touch "$ts1"	# make a file with same name, so non-abstract fails
$CMD1 2>"${te}1" &
pid1="$!"
sleep 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2=$?
kill "$pid1" 2>/dev/null; wait
if [ $rc2 -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


NAME=ABSTRACTRECV
case "$TESTS" in
*%$N%*|*%functions%*|*%unix%*|*%abstract%*|*%dgram%*|*%recv%*|*%$NAME%*)
TEST="$NAME: abstract UNIX datagram receive"
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats abstract-unixsocket); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
ts="$td/test$N.socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1="$ts"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -u ABSTRACT-RECV:$ts1,reuseaddr -"
CMD2="$TRACE $SOCAT $opts -u - ABSTRACT-SENDTO:$ts1"
printf "test $F_n $TEST... " $N
touch "$ts1"	# make a file with same name, so non-abstract fails
$CMD1 >"$tf" 2>"${te}1" &
pid1="$!"
#waitfile $ts1 1
sleep 1
echo "$da" |$CMD2 2>>"${te}2"
rc2="$?"
i=0; while [ ! -s "$tf" -a "$i" -lt 10 ]; do  relsleep 1; i=$((i+1));  done
kill "$pid1" 2>/dev/null; wait
if [ "$rc2" -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# bind with Linux abstract UNIX domain addresses bound to filesystem socket
# instead of abstract namespace
NAME=ABSTRACT_BIND
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%unix%*|*%abstract%*|*%$NAME%*)
TEST="$NAME: abstract bind"
# open an abstract client address with bind option, bind to the target socket.
# send a datagram. 
# when socat outputs the datagram it got the test succeeded
if ! eval $NUMCOND; then :; 
elif [ "$UNAME" != Linux ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}only on Linux${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1="$td/test$N.sock1"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts - ABSTRACT-SENDTO:$ts1,bind=$ts1"
printf "test $F_n $TEST... " $N
echo "$da" |$CMD1 >$tf 2>"${te}1"
rc1=$?
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD1"
    echo "rc=$rc1" >&2
    cat "${te}1" >&2
    failed
elif echo "$da" |diff -q - $tf; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD1" >&2
    cat "${te}1" >&2
    echo "$da" |diff - "$tf" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=OPENSSLREAD
# socat determined availability of data using select(). With openssl, the
# following situation might occur:
# a SSL data block with more than 8192 bytes (socats default blocksize) 
# arrives; socat calls SSL_read, and the SSL routine reads the complete block.
# socat then reads 8192 bytes from the SSL layer, the rest remains buffered.
# If the TCP connection stays idle for some time, the data in the SSL layer
# keeps there and is not transferred by socat until the socket indicates more
# data or EOF.
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socat handles data buffered by openssl"
#idea: have a socat process (server) that gets an SSL block that is larger than
# socat transfer block size; keep the socket connection open and kill the
# server process after a short time; if not the whole data block has been
# transferred, the test has failed.
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats openssl) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.out"
te="$td/test$N.err"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
SRVCERT=testsrv
gentestcert "$SRVCERT"
newport tcp4 	# provide free port number in $PORT
CMD1="$TRACE $SOCAT $opts -u -T 1 -b $($ECHO "$da\c" |wc -c) OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,cert=$SRVCERT.pem,verify=0 -"
CMD2="$TRACE $SOCAT $opts -u - OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,verify=0"
printf "test $F_n $TEST... " $N
#
$CMD1 2>"${te}1" >"$tf" &
pid=$!	# background process id
waittcp4port $PORT
(echo "$da"; sleep 2) |$CMD2 2>"${te}2"
kill "$pid" 2>/dev/null; wait
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1"
    cat "${te}1"
    echo "$CMD2"
    cat "${te}2"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
wait
fi # NUMCOND, featsesac
 ;;
esac
N=$((N+1))


# test: there is a bug with the readbytes option: when the socket delivered
# exactly that many bytes as specified with readbytes and the stays idle (no
# more data, no EOF), socat waits for more data instead of generating EOF on
# this in put stream.
NAME=READBYTES_EOF
#set -vx
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: trigger EOF after that many bytes, even when socket idle"
#idea: we deliver that many bytes to socat; the process should terminate then.
# we try to transfer data in the other direction then; if transfer succeeds,
# the process did not terminate and the bug is still there.
if ! eval $NUMCOND; then :;
elif false; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
tr="$td/test$N.ref"
ti="$td/test$N.in"
to="$td/test$N.out"
te="$td/test$N.err"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
CMD="$TRACE $SOCAT $opts SYSTEM:\"echo A; sleep $((2*SECONDs))\",readbytes=2!!- -!!/dev/null"
printf "test $F_n $TEST... " $N
(relsleep 2; echo) |eval "$CMD" >"$to" 2>"$te"
if test -s "$to"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# test: there was a bug with exec:...,pty that did not kill the exec'd sub
# process under some circumstances.
NAME=EXECPTYKILL
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%exec%*|*%pty%*|*%listen%*|*%unix%*|*%fork%*|*%$NAME%*)
TEST="$NAME: exec:...,pty explicitly kills sub process"
# we want to check if the exec'd sub process is killed in time
# for this we have a shell script that generates a file after two seconds;
# it should be killed after one second, so if the file was generated the test
# has failed
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
ts="$td/test$N.sock"
tda="$td/test$N.data"
tsh="$td/test$N.sh"
tdiff="$td/test$N.diff"
cat >"$tsh" <<EOF
relsleep $SECONDs; echo; relsleep $SECONDs;  touch "$tda"; echo
EOF
chmod a+x "$tsh"
CMD1="$TRACE $SOCAT $opts -t $(reltime 1) -U UNIX-LISTEN:$ts,fork EXEC:$tsh,pty"
CMD="$TRACE $SOCAT $opts -t $(reltime 1) /dev/null UNIX-CONNECT:$ts"
printf "test $F_n $TEST... " $N
$CMD1 2>"${te}2" &
pid1=$!
relsleep 1
waitfile $ts 1
$CMD 2>>"${te}1" >>"$tf"
relsleep 2
kill "$pid1" 2>/dev/null
wait
if [ $? -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    echo "$CMD2"
    cat "${te}1" "${te}2"
    failed
elif [ -f "$tda" ]; then
    $PRINTF "$FAILED\n"
    cat "${te}1" "${te}2"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# test if service name resolution works; this was buggy in 1.5 and 1.6.0.0
NAME=TCP4SERVICE
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%tcp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to TCP V4 socket"
# select a tcp entry from /etc/services, have a server listen on the port 
# number and connect using the service name; with the bug, connection will to a
# wrong port
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
# find a service entry we do not need root for (>=1024; here >=1100 for ease)
SERVENT="$(grep '^[a-z][a-z]*[^!-~][^!-~]*[1-9][1-9][0-9][0-9]/tcp' /etc/services |head -n 1)"
SERVICE="$(echo $SERVENT |cut -d' ' -f1)"
_PORT="$PORT"
PORT="$(echo $SERVENT |sed 's/.* \([1-9][0-9]*\).*/\1/')"
tsl="$PORT"
ts="127.0.0.1:$SERVICE"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts TCP4-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts stdin!!stdout TCP4:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1=$!
waittcp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   cat "${te}1"
   echo "$CMD2"
   cat "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid1 2>/dev/null
wait
PORT=$_PORT
fi ;; # NUMCOND
esac
N=$((N+1))


# test: up to socat 1.6.0.0, the highest file descriptor supported in socats
# transfer engine was FOPEN_MAX-1; this usually worked fine but would fail when
# socat was invoked with many file descriptors already opened. socat would 
# just hang in the select() call. Thanks to Daniel Lucq for reporting this
# problem. 
# FOPEN_MAX on different OS's:
#   OS			FOPEN_	ulimit	ulimit	FD_
#			MAX	-H -n	-S -n	SETSIZE
#   Linux 2.6:		16	1024	1024	1024
#   HP-UX 11.11:	60	2048	2048	2048
#   FreeBSD:		20	11095	11095	1024
#   Cygwin:		20	unlimit	256	64
#   AIX:		32767	65534		65534
#   SunOS 8:		20			1024
#   musl libc:		1024
NAME=EXCEED_FOPEN_MAX
case "$TESTS" in
*%$N%*|*%functions%*|*%maxfds%*|*%$NAME%*)
TEST="$NAME: more than FOPEN_MAX FDs in use"
# this test opens a number of FDs before socat is invoked. socat will have to
# allocate higher FD numbers and thus hang if it cannot handle them.
if ! eval $NUMCOND; then :;
elif [ "$UNAME" != Linux ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}only on Linux${NORMAL}\n" $N
    cant
else
REDIR=
#set -vx
if [ -z "$FOPEN_MAX" ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}could not determine FOPEN_MAX${NORMAL}\n" "$N"
    cant
else
    if [ $FOPEN_MAX -lt 270 ]; then
	OPEN_FILES=$FOPEN_MAX	# more than the highest FOPEN_MAX
    else
	OPEN_FILES=269 		# bash tends to SIGSEGV on higher value
				# btw, the test is obsolete anyway
    fi
i=3; while [ "$i" -lt "$OPEN_FILES" ]; do
    REDIR="$REDIR $i>&2"
    i=$((i+1))
done
#echo "$REDIR"
#testecho "$N" "$NAME" "$TEST" "" "pipe" "$opts -T 3" "" 1 
#set -vx
eval testecho "\"$N\"" "\"$NAME\"" "\"$TEST\"" "\"\"" "pipe" "\"$opts -T $((2*SECONDs))\"" 1 $REDIR
#set +vx
fi # could determine FOPEN_MAX
fi ;; # NUMCOND
esac
N=$((N+1))


# there was a bug with udp-listen and fork: terminating sub processes became
# zombies because the master process did not catch SIGCHLD
NAME=UDP4LISTEN_SIGCHLD
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%udp%*|*%zombie%*|*%signal%*|*%listen%*|*%fork%*|*%$NAME%*)
TEST="$NAME: test if UDP4-LISTEN child becomes zombie"
# idea: run a udp-listen process with fork and -T. Connect once, so a sub
# process is forked off. Make some transfer and wait until the -T timeout is
# over. Now check for the child process: if it is zombie the test failed. 
# Correct is that child process terminated
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; tsl=$PORT
ts="$LOCALHOST:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -T 0.5 UDP4-LISTEN:$tsl,$REUSEADDR,fork PIPE"
CMD2="$TRACE $SOCAT $opts - UDP4:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1=$!
waitudp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2=$?
sleep 1
#read -p ">"
l="$(childprocess $pid1)"
kill $pid1 2>/dev/null; wait
if [ $rc2 -ne 0 ]; then
    $PRINTF "$NO_RESULT (client failed)\n"	# already handled in test UDP4STREAM
    cant
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$NO_RESULT (diff failed)\n"	# already handled in test UDP4STREAM
    cant
elif $(isdefunct "$l"); then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    echo "$CMD2"
    cat "${te}1" "${te}2"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))
#set +vx

# there was a bug with udp-recvfrom and fork: terminating sub processes became
# zombies because the master process caught SIGCHLD but did not wait()
NAME=UDP4RECVFROM_SIGCHLD
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%fork%*|*%ip4%*|*%udp%*|*%dgram%*|*%zombie%*|*%signal%*|*%$NAME%*)
TEST="$NAME: test if UDP4-RECVFROM child becomes zombie"
# idea: run a udp-recvfrom process with fork and -T. Send it one packet, so a
# sub process is forked off. Make some transfer and wait until the -T timeout
# is over. Now check for the child process: if it is zombie the test failed. 
# Correct is that child process terminated
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; tsl=$PORT
ts="$LOCALHOST:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -T 0.5 UDP4-RECVFROM:$tsl,reuseaddr,fork PIPE"
CMD2="$TRACE $SOCAT $opts - UDP4-SENDTO:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1=$!
waitudp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
rc2=$?
sleep 1
#read -p ">"
l="$(childprocess $pid1)"
#echo "l=\"$l\""
kill $pid1 2>/dev/null; wait
if [ $rc2 -ne 0 ]; then
    $PRINTF "$NO_RESULT\n"	# already handled in test UDP4DGRAM
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    cant
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$NO_RESULT\n"	# already handled in test UDP4DGRAM
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    cant
elif $(isdefunct "$l"); then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    cat "${te}1" "${te}2"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# test: there was a bug with ip*-recv and bind option: it would not bind, and
# with the first received packet an error:
# socket_init(): unknown address family 0
# occurred
NAME=RAWIP4RECVBIND
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%dgram%*|*%rawip%*|*%rawip4%*|*%recv%*|*%root%*|*%$NAME%*)
TEST="$NAME: raw IPv4 receive with bind"
# idea: start a socat process with ip4-recv:...,bind=... and send it a packet
# if the packet passes the test succeeded
if ! eval $NUMCOND; then :;
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts1p=$IPPROTO; #IPPROTO=$((IPPROTO+1))
ts1a="127.0.0.1"
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -u IP4-RECV:$ts1p,bind=$ts1a,reuseaddr -"
CMD2="$TRACE $SOCAT $opts -u - IP4-SENDTO:$ts1"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1="$!"
waitip4proto $ts1p 1
echo "$da" |$CMD2 2>>"${te}2"
rc2="$?"
#ls -l $tf
i=0; while [ ! -s "$tf" -a "$i" -lt 10 ]; do  relsleep 1; i=$((i+1));  done
kill "$pid1" 2>/dev/null; wait
if [ "$rc2" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "${te}1"
   cat "${te}2"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND, root
esac
#IPPROTO=$((IPPROTO+1))
N=$((N+1))


# there was a bug in *-recvfrom with fork: due to an error in the appropriate
# signal handler the master process would hang after forking off the first
# child process.
NAME=UDP4RECVFROM_FORK
case "$TESTS" in
*%$N%*|*%functions%*|*%fork%*|*%ip4%*|*%udp%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: test if UDP4-RECVFROM handles more than one packet"
# idea: run a UDP4-RECVFROM process with fork and -T. Send it one packet;
# send it a second packet and check if this is processed properly. If yes, the
# test succeeded.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; tsp=$PORT
ts="$LOCALHOST:$tsp"
da2a="test$N $(date) $RANDOM"
da2b="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -T 2 UDP4-RECVFROM:$tsp,reuseaddr,fork PIPE"
CMD2="$TRACE $SOCAT $opts -T 1 - UDP4-SENDTO:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >/dev/null 2>"${te}1" &
pid1=$!
waitudp4port $tsp 1
echo "$da2a" |$CMD2 >/dev/null 2>>"${te}2a"	# this should always work
rc2a=$?
echo "$da2b" |$CMD2 >"$tf" 2>>"${te}2b"		# this would fail when bug
rc2b=$?
kill $pid1 2>/dev/null; wait
if [ $rc2b -ne 0 ]; then
    $PRINTF "$NO_RESULT\n"
    cant
elif ! echo "$da2b" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &" >&2
    cat "${te}1" >&2
    echo "$CMD2" >&2
    cat "${te}2b" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}1" "${te}2" "${te}3"; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# there was a bug in parsing the arguments of exec: consecutive spaces resulted
# in additional empty arguments
NAME=EXECSPACES
case "$TESTS" in
*%$N%*|*%functions%*|*%exec%*|*%parse%*|*%$NAME%*)
TEST="$NAME: correctly parse exec with consecutive spaces"
if ! eval $NUMCOND; then :; else
$PRINTF "test $F_n $TEST... " $N
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
da="test$N $(date)  $RANDOM"	# with a double space
tdiff="$td/test$N.diff"
# put the test data as first argument after two spaces. expect the data in the
# first argument of the exec'd command.
$TRACE $SOCAT $opts -u "exec:\"bash -c \\\"echo \\\\\\\"\$1\\\\\\\"\\\"  \\\"\\\" \\\"$da\\\"\"" - >"$tf" 2>"$te"
rc=$?
echo "$da" |diff - "$tf" >"$tdiff"
if [ "$rc" -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    cat "$te"
    failed
elif [ -s "$tdiff" ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo diff:
    cat "$tdiff"
    if [ -n "$debug" ]; then cat $te; fi
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat $te; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# a bug was found in the way UDP-LISTEN handles the listening socket:
# when UDP-LISTEN continued to listen after a packet had been dropped by, e.g.,
# range option, the old listen socket would not be closed but a new one created.
NAME=UDP4LISTENCONT
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%ip4%*|*%udp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: let range drop a packet and see if old socket is closed"
# idea: run a UDP4-LISTEN process with range option. Send it one packet from an
# address outside range and check if two listening sockets are open then
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; tp=$PORT
da1="test$N $(date) $RANDOM"
a1="$LOCALHOST"
a2="$SECONDADDR"
#CMD0="$TRACE $SOCAT $opts UDP4-LISTEN:$tp,bind=$a1,range=$a2/32 PIPE"
CMD0="$TRACE $SOCAT $opts UDP4-LISTEN:$tp,$REUSEADDR,range=$a2/32 PIPE"
CMD1="$TRACE $SOCAT $opts - UDP-CONNECT:$a1:$tp"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid1=$!
waitudp4port $tp 1
echo "$da1" |$CMD1 >"${tf}1" 2>"${te}1"	# this should fail
rc1=$?
waitudp4port $tp 1
if [ "$SS" ]; then
   nsocks="$($SS -anu |grep ":$PORT\>" |wc -l)"
else
   nsocks="$(netstat -an |grep "^udp.*[:.]$PORT\>" |wc -l)"
fi
kill $pid1 2>/dev/null; wait
if [ $rc1 -ne 0 ]; then
    $PRINTF "$NO_RESULT\n"
    cant
elif [ $nsocks -eq 0 ]; then
    $PRINTF "$NO_RESULT\n"
    cant
elif [ $nsocks -ne 1 ]; then
    $PRINTF "$FAILED ($nsocks listening sockets)\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0" "${te}1"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}0" "${te}1" "${te}2"; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# during wait for next poll time option ignoreeof blocked the data transfer in
# the reverse direction
NAME=IGNOREEOFNOBLOCK
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%socket%*|*%ignoreeof%*|*%$NAME%*)
TEST="$NAME: ignoreeof does not block other direction"
# have socat poll in ignoreeof mode. while it waits one second for next check,
# we send data in the reverse direction and then the total timeout fires.
# it the data has passed, the test succeeded.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts /dev/null,ignoreeof!!- -!!/dev/null"
printf "test $F_n $TEST... " $N
(sleep 0.333333; echo "$da") |$CMD0 >"$tf" 2>"${te}0"
rc0=$?
if [ $rc0 != 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    failed
elif echo "$da" |diff - "$tf" >/dev/null; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    failed
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# test the escape option
NAME=ESCAPE
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%escape%*|*%$NAME%*)
TEST="$NAME: escape character triggers EOF"
# idea: start socat just echoing input, but apply escape option. send a string
# containing the escape character and check if the output is truncated
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT $opts -,escape=27 pipe"
printf "test $F_n $TEST... " $N
$ECHO "$da\n\x1bXYZ" |$CMD >"$tf" 2>"$te"
if [ $? -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD"
    cat "$te"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: diff:\n"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat $te; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))

# test the escape option combined with ignoreeof
NAME=ESCAPE_IGNOREEOF
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%ignoreeof%*|*%escape%*|*%$NAME%*)
TEST="$NAME: escape character triggers EOF"
# idea: start socat just echoing input, but apply escape option. send a string
# containing the escape character and check if the output is truncated
if ! eval $NUMCOND; then :; else
ti="$td/test$N.file"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT -T 5 $opts file:$ti,ignoreeof,escape=27!!- pipe"
printf "test $F_n $TEST... " $N
>"$ti"
$CMD >"$tf" 2>"$te" &
$ECHO "$da\n\x1bXYZ" >>"$ti"
sleep 1
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: diff:\n"
    cat "$tdiff"
    cat "$te"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat $te; fi
    ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# test: logging of ancillary message
while read PF KEYW ADDR IPPORT SCM_ENABLE SCM_RECV SCM_TYPE SCM_NAME ROOT SCM_VALUE
do
if [ -z "$PF" ] || [[ "$PF" == \#* ]]; then continue; fi
#
pf="$(echo "$PF" |tr A-Z a-z)"
proto="$(echo "$KEYW" |tr A-Z a-z)"
NAME=${KEYW}SCM_$SCM_TYPE
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%$pf%*|*%dgram%*|*%udp%*|*%$proto%*|*%recv%*|*%ancillary%*|*%$ROOT%*|*%$NAME%*)
TEST="$NAME: $KEYW log ancillary message $SCM_TYPE $SCM_NAME"
# idea: start a socat process with *-RECV:..,... , ev. with ancillary message
# enabling option and send it a packet, ev. with some option. check the info log
# for the appropriate output.
if ! eval $NUMCOND; then :;
#elif [[ "$PF" == "#*" ]]; then :
elif [ "$ROOT" = root -a $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
elif ! feat=$(testfeats ${KEYW%[46]} IP${KEYW##*[A-Z]}); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$KEYW not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! runs${proto} >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$KEYW not available on host${NORMAL}\n" $N
    cant
elif ! testoptions $SCM_RECV >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}option $SCM_RECV not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
case "X$IPPORT" in
    "XPORT")
    newport $proto; tra="$PORT"		# test recv address
    tsa="$ADDR:$PORT"	# test sendto address
    ;;
    "XPROTO")
    tra="$IPPROTO"		# test recv address
    tsa="$ADDR:$IPPROTO"	# test sendto address
    #IPPROTO=$((IPPROTO+1))
    ;;
    *)
    tra="$(eval echo "$ADDR")"	# resolve $N
    tsa="$tra"
esac
CMD0="$TRACE $SOCAT $opts -d -d -d -u $KEYW-RECV:$tra,reuseaddr,$SCM_RECV -"
CMD1="$TRACE $SOCAT $opts -u - $KEYW-SENDTO:$tsa,$SCM_ENABLE"
printf "test $F_n $TEST... " $N
# is this option supported?
if $SOCAT -hhh |grep "[[:space:]]$SCM_RECV[[:space:]]" >/dev/null; then
  if [ "$SCM_VALUE" = "timestamp" ]; then
    secs="$(date '+%S')"
    if [ "$secs" -ge 58 -a "$secs" -le 59 ]; then
      dsecs=$((60-secs))
      #echo "Sleeping $dsecs seconds to avoid minute change in timestamp" >/dev/tty
      sleep $dsecs
    fi
  fi
$CMD0 >"$tf" 2>"${te}0" &
pid0="$!"
wait${proto}port $tra 1
echo "XYZ" |$CMD1 2>"${te}1"
rc1="$?"
sleep 1
i=0; while [ ! -s "${te}0" -a "$i" -lt 10 ]; do  relsleep 1; i=$((i+1));  done
kill "$pid0" 2>/dev/null; wait
# do not show more messages than requested
case "$opts" in
*-d*-d*-d*-d*) LEVELS="[EWNID]" ;;
*-d*-d*-d*)    LEVELS="[EWNI]" ;;
*-d*-d*)       LEVELS="[EWN]" ;;
*-d*)          LEVELS="[EW]" ;;
*)             LEVELS="[E]" ;;
esac
if [ "$SCM_VALUE" = "timestamp" ]; then
    SCM_VALUE="$(date '+%a %b %e %H:%M:.. %Y'), ...... usecs"
fi
if [ "$rc1" -ne 0 ]; then
    $PRINTF "$NO_RESULT: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    echo "$CMD1"
    grep " $LEVELS " "${te}0"
    grep " $LEVELS " "${te}1"
    cant
elif ! grep "ancillary message: $SCM_TYPE: $SCM_NAME=" ${te}0 >/dev/null; then
    $PRINTF "$FAILED\n"
    echo "variable $SCM_TYPE: $SCM_NAME not set"
    echo "$CMD0 &"
    echo "$CMD1"
    grep " $LEVELS " "${te}0"
    grep " $LEVELS " "${te}1"
    failed
elif ! grep "ancillary message: $SCM_TYPE: $SCM_NAME=$SCM_VALUE\$" ${te}0 >/dev/null; then
    $PRINTF "$FAILED\n"
    badval="$(grep "ancillary message: $SCM_TYPE: $SCM_NAME" ${te}0 |sed 's/.*=//g')"
    echo "variable $SCM_TYPE: $SCM_NAME has value \"$badval\" instead of pattern \"$SCM_VALUE\"" >&2
    echo "$CMD0 &"
    echo "$CMD1"
    grep " $LEVELS " "${te}0"
    grep " $LEVELS " "${te}1"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then grep " $LEVELS " "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "echo XYZ |$CMD1"; fi
    if [ "$DEBUG" ];   then grep " $LEVELS " "${te}1" >&2; fi
    ok
fi
else # option is not supported
    $PRINTF "${YELLOW}$SCM_RECV not available${NORMAL}\n"
    cant
fi # option is not supported
fi # NUMCOND, root, feats
 ;;
esac
N=$((N+1))
#
done <<<"
IP4  UDP4 127.0.0.1 PORT  ip-options=x01000000 ip-recvopts       IP_OPTIONS     options   user x01000000
IP4  UDP4 127.0.0.1 PORT  ,                    so-timestamp      SCM_TIMESTAMP  timestamp user timestamp
IP4  UDP4 127.0.0.1 PORT  ip-ttl=53            ip-recvttl        IP_TTL         ttl       user 53
IP4  UDP4 127.0.0.1 PORT  ip-tos=7             ip-recvtos        IP_TOS         tos       user 7
IP4  UDP4 127.0.0.1 PORT  ,                    ip-pktinfo        IP_PKTINFO     locaddr   user 127.0.0.1
IP4  UDP4 127.0.0.1 PORT  ,                    ip-pktinfo        IP_PKTINFO     dstaddr   user 127.0.0.1
IP4  UDP4 127.0.0.1 PORT  ,                    ip-pktinfo        IP_PKTINFO     if        user lo
IP4  UDP4 127.0.0.1 PORT  ,                    ip-recvif         IP_RECVIF      if        user lo0
IP4  UDP4 127.0.0.1 PORT  ,                    ip-recvdstaddr    IP_RECVDSTADDR dstaddr   user 127.0.0.1
IP4  IP4  127.0.0.1 PROTO ip-options=x01000000 ip-recvopts       IP_OPTIONS     options   root x01000000
IP4  IP4  127.0.0.1 PROTO ,                    so-timestamp      SCM_TIMESTAMP  timestamp root timestamp
IP4  IP4  127.0.0.1 PROTO ip-ttl=53            ip-recvttl        IP_TTL         ttl       root 53
IP4  IP4  127.0.0.1 PROTO ip-tos=7             ip-recvtos        IP_TOS         tos       root 7
IP4  IP4  127.0.0.1 PROTO ,                    ip-pktinfo        IP_PKTINFO     locaddr   root 127.0.0.1
IP4  IP4  127.0.0.1 PROTO ,                    ip-pktinfo        IP_PKTINFO     dstaddr   root 127.0.0.1
IP4  IP4  127.0.0.1 PROTO ,                    ip-pktinfo        IP_PKTINFO     if        root lo
IP4  IP4  127.0.0.1 PROTO ,                    ip-recvif         IP_RECVIF      if        root lo0
IP4  IP4  127.0.0.1 PROTO ,                    ip-recvdstaddr    IP_RECVDSTADDR dstaddr   root 127.0.0.1
IP6  UDP6 [::1]     PORT  ,                    so-timestamp      SCM_TIMESTAMP  timestamp user timestamp
IP6  UDP6 [::1]     PORT  ,                    ipv6-recvpktinfo  IPV6_PKTINFO   dstaddr   user [[]0000:0000:0000:0000:0000:0000:0000:0001[]]
IP6  UDP6 [::1]     PORT  ipv6-unicast-hops=35 ipv6-recvhoplimit IPV6_HOPLIMIT  hoplimit  user 35
IP6  UDP6 [::1]     PORT  ipv6-tclass=0xaa     ipv6-recvtclass   IPV6_TCLASS    tclass    user x000000aa
IP6  IP6  [::1]     PROTO ,                    so-timestamp      SCM_TIMESTAMP  timestamp root timestamp
IP6  IP6  [::1]     PROTO ,                    ipv6-recvpktinfo  IPV6_PKTINFO   dstaddr   root [[]0000:0000:0000:0000:0000:0000:0000:0001[]]
IP6  IP6  [::1]     PROTO ipv6-unicast-hops=35 ipv6-recvhoplimit IPV6_HOPLIMIT  hoplimit  root 35
IP6  IP6  [::1]     PROTO ipv6-tclass=0xaa     ipv6-recvtclass   IPV6_TCLASS    tclass    root x000000aa
#UNIX UNIX $td/test\$N.server - ,               so-timestamp      SCM_TIMESTAMP  timestamp user timestamp
"
# This one fails, apparently due to a Linux weakness:
# UNIX so-timestamp


# test: setting of environment variables that describe a stream socket
# connection: SOCAT_SOCKADDR, SOCAT_PEERADDR; and SOCAT_SOCKPORT,
# SOCAT_PEERPORT when applicable
while read KEYW FEAT SEL TEST_SOCKADDR TEST_PEERADDR PORTMETHOD; do
if [ -z "$KEYW" ] || [[ "$KEYW" == \#* ]]; then continue; fi
#
protov="$(echo "$KEYW" |tr A-Z a-z)"
proto="${protov%%[0-9]}"
NAME=${KEYW}LISTENENV
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%$SEL%*|*%$proto%*|*%$protov%*|*%envvar%*|*%listen%*|*%$NAME%*)
TEST="$NAME: $KEYW-LISTEN sets environment variables with socket addresses"
# have a server accepting a connection and invoking some shell code. The shell
# code extracts and prints the SOCAT related environment vars.
# outside code then checks if the environment contains the variables correctly
# describing the peer and local sockets.
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats $FEAT); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat" |tr a-z A-Z) not available${NORMAL}\n" $N
    cant
elif ! runs${protov} >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
TEST_SOCKADDR="$(echo "$TEST_SOCKADDR" |sed "s/\$N/$N/g")"	# actual vars
tsa="$TEST_SOCKADDR"	# test server address
if [ "$PORTMETHOD" == PORT ]; then
    newport $proto; tsp="$PORT"; 	# test server port
    tsa1="$tsp"; tsa2="$tsa"; tsa="$tsa:$tsp"	# tsa2 used for server bind=
    TEST_SOCKPORT=$tsp
else
    tsa1="$tsa"; tsa2=				# tsa1 used for addr parameter
fi
TEST_PEERADDR="$(echo "$TEST_PEERADDR" |sed "s/\$N/$N/g")"	# actual vars
tca="$TEST_PEERADDR"	# test client address
if [ $PORTMETHOD = PORT ]; then
    newport $proto; tcp="$PORT"; 	# test client port
    tca="$tca:$tcp"
    TEST_PEERPORT=$tcp
fi
#CMD0="$TRACE $SOCAT $opts -u $KEYW-LISTEN:$tsa1 SYSTEM:\"export -p\""
CMD0="$TRACE $SOCAT $opts -u -lpsocat $KEYW-LISTEN:$tsa1,$REUSEADDR SYSTEM:\"echo SOCAT_SOCKADDR=\\\$SOCAT_SOCKADDR; echo SOCAT_PEERADDR=\\\$SOCAT_PEERADDR; echo SOCAT_SOCKPORT=\\\$SOCAT_SOCKPORT; echo SOCAT_PEERPORT=\\\$SOCAT_PEERPORT; sleep 1\""
CMD1="$TRACE $SOCAT $opts -u - $KEYW-CONNECT:$tsa,bind=$tca"
printf "test $F_n $TEST... " $N
eval "$CMD0 2>\"${te}0\" >\"$tf\" &"
pid0=$!
wait${protov}port $tsa1 1
{ echo; sleep 0.1; } |$CMD1 2>"${te}1"
rc1=$?
waitfile "$tf" 2
kill $pid0 2>/dev/null; wait
#set -vx
if [ $rc1 != 0 ]; then
    $PRINTF "$NO_RESULT (client failed):\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1"
    cat "${te}1"
    cant
elif [ "$(grep SOCAT_SOCKADDR "${tf}" |sed -e 's/^[^=]*=//' |sed -e "s/[\"']//g")" = "$TEST_SOCKADDR" -a \
    "$(grep SOCAT_PEERADDR "${tf}" |sed -e 's/^[^=]*=//' -e "s/[\"']//g")" = "$TEST_PEERADDR" -a \
    \( "$PORTMETHOD" = ',' -o "$(grep SOCAT_SOCKPORT "${tf}" |sed -e 's/^[^=]*=//' |sed -e 's/"//g')" = "$TEST_SOCKPORT" \) -a \
    \( "$PORTMETHOD" = ',' -o "$(grep SOCAT_PEERPORT "${tf}" |sed -e 's/^[^=]*=//' |sed -e 's/"//g')" = "$TEST_PEERPORT" \) \
    ]; then
    $PRINTF "$OK\n"
    if [ "$debug" ]; then
	echo "$CMD0 &"
	cat "${te}0"
	echo "$CMD1"
	cat "${te}1"
    fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1"
    cat "${te}1"
    echo -e "SOCAT_SOCKADDR=$TEST_SOCKADDR\nSOCAT_PEERADDR=$TEST_PEERADDR\nSOCAT_SOCKPORT=$TEST_SOCKPORT\nSOCAT_PEERPORT=$TEST_PEERPORT" |
    diff - "${tf}"
    failed
fi
fi # NUMCOND, feats
 ;;
esac
N=$((N+1))
#set +xv
#
done <<<"
TCP4     TCP  tcp     127.0.0.1                                 $SECONDADDR                               PORT
TCP6     IP6  tcp     [0000:0000:0000:0000:0000:0000:0000:0001] [0000:0000:0000:0000:0000:0000:0000:0001] PORT
UDP6     IP6  udp     [0000:0000:0000:0000:0000:0000:0000:0001] [0000:0000:0000:0000:0000:0000:0000:0001] PORT
SCTP4    SCTP sctp    127.0.0.1                                 $SECONDADDR                               PORT
SCTP6    SCTP sctp    [0000:0000:0000:0000:0000:0000:0000:0001] [0000:0000:0000:0000:0000:0000:0000:0001] PORT
UNIX     UNIX unix    $td/test\$N.server                        $td/test\$N.client                        ,
"
# this one fails due to weakness in socats UDP4-LISTEN implementation:
#UDP4 $LOCALHOST $SECONDADDR $((PORT+4)) $((PORT+5))


# test: environment variables from ancillary message
while read PF KEYW SEL ADDR IPPORT SCM_ENABLE SCM_RECV SCM_ENVNAME ROOT SCM_VALUE
do
if [ -z "$PF" ] || [[ "$PF" == \#* ]]; then continue; fi
#
pf="$(echo "$PF" |tr A-Z a-z)"
proto="$(echo "$KEYW" |tr A-Z a-z)"
NAME=${KEYW}ENV_$SCM_ENVNAME
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%$pf%*|*%dgram%*|*%$SEL%*|*%$proto%*|*%recv%*|*%ancillary%*|*%envvar%*|*%$ROOT%*|*%$NAME%*)
#set -vx
TEST="$NAME: $KEYW ancillary message sets env SOCAT_$SCM_ENVNAME"
# idea: start a socat process with *-RECVFROM:..,... , ev. with ancillary
# message  enabling option and send it a packet, ev. with some option. write
# the resulting environment to a file and check its contents for the
# appropriate variable.
if ! eval $NUMCOND; then :;
elif [ "$ROOT" = root -a $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
elif [ "$PF" = "IP6" ] && ( ! feat=$(testfeats ip6) || ! runsip6 ) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IP6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
case "X$IPPORT" in
    "XPORT")
    newport $proto; tra="$PORT"		# test recv address
    tsa="$ADDR:$tra"	# test sendto address
    ;;
    "XPROTO")
    tra="$IPPROTO"		# test recv address
    tsa="$ADDR:$IPPROTO"	# test sendto address
    #IPPROTO=$((IPPROTO+1))
    ;;
    *)
    tra="$(eval echo "$ADDR")"	# resolve $N
    tsa="$tra"
esac
#CMD0="$TRACE $SOCAT $opts -u $KEYW-RECVFROM:$tra,reuseaddr,$SCM_RECV SYSTEM:\"export -p\""
# without that ultra escaped quote the test failed for IPv6 when there was file ./1
CMD0="$TRACE $SOCAT $opts -u -lpsocat $KEYW-RECVFROM:$tra,reuseaddr,$SCM_RECV SYSTEM:\"echo \\\\\\\"\\\$SOCAT_$SCM_ENVNAME\\\\\\\"\""
CMD1="$TRACE $SOCAT $opts -u - $KEYW-SENDTO:$tsa,$SCM_ENABLE"
printf "test $F_n $TEST... " $N
# is this option supported?
if $SOCAT -hhh |grep "[[:space:]]$SCM_RECV[[:space:]]" >/dev/null; then
  if [ "$SCM_VALUE" = "timestamp" ]; then
    secs="$(date '+%S')"
    if [ "$secs" -ge 58 -a "$secs" -le 59 ]; then
      dsecs=$((60-secs))
      #echo "Sleeping $dsecs seconds to avoid minute change in timestamp" >/dev/tty
      sleep $dsecs
    fi
  fi
eval "$CMD0 >\"$tf\" 2>\"${te}0\" &"
pid0="$!"
wait${proto}port $tra 1
{ echo "XYZ"; sleep 0.1; } |$CMD1 2>"${te}1"
rc1="$?"
waitfile "$tf" 2
#i=0; while [ ! -s "${te}0" -a "$i" -lt 10 ]; do  relsleep 1; i=$((i+1));  done
kill "$pid0" 2>/dev/null; wait
# do not show more messages than requested
if [ "$SCM_VALUE" = "timestamp" ]; then
    SCM_VALUE="$(date '+%a %b %e %H:%M:.. %Y'), ...... usecs"
    #echo "\"$SCM_VALUE\"" >&2  # debugging
fi
if [ "$rc1" -ne 0 ]; then
    $PRINTF "$NO_RESULT: $SOCAT:\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    cant
#elif ! $GREP_E "^export SOCAT_$SCM_ENVNAME=[\"']?$SCM_VALUE[\"']?\$" ${tf} >/dev/null; then
#elif ! eval echo "$TRACE $SOCAT_\$SCM_VALUE" |diff - "${tf}" >/dev/null; then
elif ! expr "$(cat "$tf")" : "$SCM_VALUE\$" >/dev/null; then
    $PRINTF "$FAILED\n"
    echo "logged value \"$(cat "$tf")\" instead of $SCM_VALUE"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "{ echo XYZ; sleep 0.1; } |$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
fi
else # option is not supported
    $PRINTF "${YELLOW}$SCM_RECV not available${NORMAL}\n"
    cant
fi # option is not supported
fi ;; # NUMCOND, feats
esac
N=$((N+1))
#
done <<<"
IP4  UDP4     udp     127.0.0.1 PORT  ip-options=x01000000 ip-recvopts       IP_OPTIONS     user x01000000
IP4  UDP4     udp     127.0.0.1 PORT  ,                    so-timestamp      TIMESTAMP      user timestamp
IP4  UDP4     udp     127.0.0.1 PORT  ip-ttl=53            ip-recvttl        IP_TTL         user 53
IP4  UDP4     udp     127.0.0.1 PORT  ip-tos=7             ip-recvtos        IP_TOS         user 7
IP4  UDP4     udp     127.0.0.1 PORT  ,                    ip-pktinfo        IP_LOCADDR     user 127.0.0.1
IP4  UDP4     udp     127.0.0.1 PORT  ,                    ip-pktinfo        IP_DSTADDR     user 127.0.0.1
IP4  UDP4     udp     127.0.0.1 PORT  ,                    ip-pktinfo        IP_IF          user lo
IP4  UDP4     udp     127.0.0.1 PORT  ,                    ip-recvif         IP_IF          user lo0
IP4  UDP4     udp     127.0.0.1 PORT  ,                    ip-recvdstaddr    IP_DSTADDR     user 127.0.0.1
IP4  IP4      rawip   127.0.0.1 PROTO ip-options=x01000000 ip-recvopts       IP_OPTIONS     root x01000000
IP4  IP4      rawip   127.0.0.1 PROTO ,                    so-timestamp      TIMESTAMP      root timestamp
IP4  IP4      rawip   127.0.0.1 PROTO ip-ttl=53            ip-recvttl        IP_TTL         root 53
IP4  IP4      rawip   127.0.0.1 PROTO ip-tos=7             ip-recvtos        IP_TOS         root 7
IP4  IP4      rawip   127.0.0.1 PROTO ,                    ip-pktinfo        IP_LOCADDR     root 127.0.0.1
IP4  IP4      rawip   127.0.0.1 PROTO ,                    ip-pktinfo        IP_DSTADDR     root 127.0.0.1
IP4  IP4      rawip   127.0.0.1 PROTO ,                    ip-pktinfo        IP_IF          root lo
IP4  IP4      rawip   127.0.0.1 PROTO ,                    ip-recvif         IP_IF          root lo0
IP4  IP4      rawip   127.0.0.1 PROTO ,                    ip-recvdstaddr    IP_DSTADDR     root 127.0.0.1
IP6  UDP6     udp     [::1]     PORT  ,                    ipv6-recvpktinfo  IPV6_DSTADDR   user [[]0000:0000:0000:0000:0000:0000:0000:0001[]]
IP6  UDP6     udp     [::1]     PORT  ipv6-unicast-hops=35 ipv6-recvhoplimit IPV6_HOPLIMIT  user 35
IP6  UDP6     udp     [::1]     PORT  ipv6-tclass=0xaa     ipv6-recvtclass   IPV6_TCLASS    user x000000aa
IP6  IP6      rawip   [::1]     PROTO ,                    ipv6-recvpktinfo  IPV6_DSTADDR   root [[]0000:0000:0000:0000:0000:0000:0000:0001[]]
IP6  IP6      rawip   [::1]     PROTO ipv6-unicast-hops=35 ipv6-recvhoplimit IPV6_HOPLIMIT  root 35
IP6  IP6      rawip   [::1]     PROTO ipv6-tclass=0xaa     ipv6-recvtclass   IPV6_TCLASS    root x000000aa
#UNIX UNIX $td/test\$N.server - ,               so-timestamp      TIMESTAMP      user timestamp
"


# test the SOCKET-CONNECT address (against TCP4-LISTEN)
NAME=SOCKET_CONNECT_TCP4
case "$TESTS" in
*%$N%*|*%functions%*|*%generic%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socket connect with TCP/IPv4"
# start a TCP4-LISTEN process that echoes data, and send test data using
# SOCKET-CONNECT, selecting TCP/IPv4. The sent data should be returned.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp4; ts0p=$PORT
ts0a="127.0.0.1"
ts1p=$(printf "%04x" $ts0p);
ts1a="7f000001" # "127.0.0.1"
ts1="x${ts1p}${ts1a}x0000000000000000"
newport tcp4; ts1b=$(printf "%04x" $PORT)
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts TCP4-LISTEN:$ts0p,$REUSEADDR,bind=$ts0a PIPE"
CMD1="$TRACE $SOCAT $opts - SOCKET-CONNECT:2:6:$ts1,bind=x${ts1b}00000000x0000000000000000"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" &
pid0="$!"
waittcp4port $ts0p 1
echo "$da" |$CMD1 >>"$tf" 2>>"${te}1"
rc1="$?"
kill "$pid0" 2>/dev/null; wait;
if [ "$rc1" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# test the SOCKET-CONNECT address (against TCP6-LISTEN)
NAME=SOCKET_CONNECT_TCP6
case "$TESTS" in
*%$N%*|*%functions%*|*%generic%*|*%tcp6%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socket connect with TCP/IPv6"
if ! eval $NUMCOND; then :;
elif ! testfeats tcp ip6 >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
    cant
else
# start a TCP6-LISTEN process that echoes data, and send test data using
# SOCKET-CONNECT, selecting TCP/IPv6. The sent data should be returned.
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp6; ts0p=$PORT
ts0a="[::1]"
ts1p=$(printf "%04x" $ts0p);
ts1a="00000000000000000000000000000001" # "[::1]"
ts1="x${ts1p}x00000000x${ts1a}x00000000"
newport tcp6; ts1b=$(printf "%04x" $PORT)
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts TCP6-LISTEN:$ts0p,$REUSEADDR,bind=$ts0a PIPE"
CMD1="$TRACE $SOCAT $opts - SOCKET-CONNECT:$PF_INET6:6:$ts1,bind=x${ts1b}x00000000x00000000000000000000000000000000x00000000"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" &
pid0="$!"
waittcp6port $ts0p 1
echo "$da" |$CMD1 >>"$tf" 2>>"${te}1"
rc1="$?"
kill "$pid0" 2>/dev/null; wait;
if [ "$rc1" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))

# test the SOCKET-CONNECT address (against UNIX-LISTEN)
NAME=SOCKET_CONNECT_UNIX
case "$TESTS" in
*%$N%*|*%functions%*|*%generic%*|*%unix%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socket connect with UNIX domain"
# start a UNIX-LISTEN process that echoes data, and send test data using
# SOCKET-CONNECT, selecting UNIX socket. The sent data should be returned.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
ts0="$td/test$N.server"
ts1="$td/test$N.client"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts UNIX-LISTEN:$ts0,$REUSEADDR PIPE"
CMD1="$TRACE $SOCAT $opts - SOCKET-CONNECT:1:0:\\\"$ts0\\\0\\\",bind=\\\"$ts1\\\0\\\""
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" &
pid0="$!"
waitfile $ts0 1
echo "$da" |$CMD1 >>"$tf" 2>>"${te}1"
rc1="$?"
kill "$pid0" 2>/dev/null; wait;
if [ "$rc1" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))

# test the SOCKET-LISTEN address (with TCP4-CONNECT)
NAME=SOCKET_LISTEN
case "$TESTS" in
*%$N%*|*%functions%*|*%generic%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socket recvfrom with TCP/IPv4"
# start a SOCKET-LISTEN process that uses TCP/IPv4 and echoes data, and
# send test data using TCP4-CONNECT. The sent data should be returned.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport tcp4; ts1p=$PORT
ts1a="127.0.0.1"
ts0p=$(printf "%04x" $ts1p);
ts0a="7f000001" # "127.0.0.1"
ts0="x${ts0p}${ts0a}x0000000000000000"
newport tcp4; ts1b=$PORT
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts SOCKET-LISTEN:2:6:$ts0,$REUSEADDR PIPE"
CMD1="$TRACE $SOCAT $opts - TCP4-CONNECT:$ts1,bind=:$ts1b"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" &
pid0="$!"
#sleep 1
waittcp4port $ts1p 1
echo "$da" |$CMD1 >>"$tf" 2>>"${te}1"
rc1="$?"
kill "$pid0" 2>/dev/null; wait;
if [ "$rc1" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))

# test the SOCKET-SENDTO address (against UDP4-RECVFROM)
NAME=SOCKET_SENDTO
case "$TESTS" in
*%$N%*|*%functions%*|*%generic%*|*%socket%*|*%ip4%*|*%udp%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: socket sendto with UDP/IPv4"
# start a UDP4-RECVFROM process that echoes data, and send test data using
# SOCKET-SENDTO, selecting UDP/IPv4. The sent data should be returned.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; ts0p=$PORT
ts0a="127.0.0.1"
ts1p=$(printf "%04x" $ts0p);
ts1a="7f000001" # "127.0.0.1"
ts1="x${ts1p}${ts1a}x0000000000000000"
newport udp4; ts1b=$(printf "%04x" $PORT)
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts UDP4-RECVFROM:$ts0p,reuseaddr,bind=$ts0a PIPE"
CMD1="$TRACE $SOCAT $opts - SOCKET-SENDTO:2:$SOCK_DGRAM:17:$ts1,bind=x${ts1b}x00000000x0000000000000000"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" &
pid0="$!"
waitudp4port $ts0p 1
echo "$da" |$CMD1 >>"$tf" 2>>"${te}1"
rc1="$?"
kill "$pid0" 2>/dev/null; wait;
if [ "$rc1" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))

# test the SOCKET-RECVFROM address (with UDP4-SENDTO)
NAME=SOCKET_RECVFROM
case "$TESTS" in
*%$N%*|*%functions%*|*%generic%*|*%socket%*|*%ip4%*|*%udp%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: socket recvfrom with UDP/IPv4"
# start a SOCKET-RECVFROM process that uses UDP/IPv4 and echoes data, and
# send test data using UDP4-SENDTO. The sent data should be returned.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; ts1p=$PORT
ts1a="127.0.0.1"
ts0p=$(printf "%04x" $ts1p);
ts0a="7f000001" # "127.0.0.1"
ts0="x${ts0p}${ts0a}x0000000000000000"
newport udp4; ts1b=$PORT
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts SOCKET-RECVFROM:2:$SOCK_DGRAM:17:$ts0,reuseaddr PIPE"
CMD1="$TRACE $SOCAT $opts - UDP4-SENDTO:$ts1,bind=:$ts1b"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" &
pid0="$!"
sleep 1	# waitudp4port $ts1p 1
echo "$da" |$CMD1 >>"$tf" 2>>"${te}1"
rc1="$?"
kill "$pid0" 2>/dev/null; wait;
if [ "$rc1" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# test the SOCKET-RECV address (with UDP4-SENDTO)
NAME=SOCKET_RECV
case "$TESTS" in
*%$N%*|*%functions%*|*%generic%*|*%socket%*|*%ip4%*|*%udp%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: socket recv with UDP/IPv4"
# start a SOCKET-RECV process that uses UDP/IPv4 and writes received data to file, and
# send test data using UDP4-SENDTO.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; ts1p=$PORT
ts1a="127.0.0.1"
ts0p=$(printf "%04x" $ts1p);
ts0a="7f000001" # "127.0.0.1"
ts0="x${ts0p}${ts0a}x0000000000000000"
newport udp4; ts1b=$PORT
ts1="$ts1a:$ts1p"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -u SOCKET-RECV:2:$SOCK_DGRAM:17:$ts0,reuseaddr -"
CMD1="$TRACE $SOCAT $opts -u - UDP4-SENDTO:$ts1,bind=:$ts1b"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" >"$tf" &
pid0="$!"
sleep 1	# waitudp4port $ts1p 1
echo "$da" |$CMD1 2>>"${te}1"
rc1="$?"
sleep 1
kill "$pid0" 2>/dev/null; wait;
if [ "$rc1" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))

# test SOCKET-DATAGRAM (with UDP4-DATAGRAM)
NAME=SOCKET_DATAGRAM
case "$TESTS" in
*%$N%*|*%functions%*|*%generic%*|*%socket%*|*%ip4%*|*%udp%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: socket datagram via UDP/IPv4"
# start a UDP4-DATAGRAM process that echoes data, and send test data using
# SOCKET-DATAGRAM, selecting UDP/IPv4. The sent data should be returned.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp4; ts0p=$PORT
newport udp4; ts1p=$PORT
ts0a="127.0.0.1"
ts1b=$(printf "%04x" $ts0p);
ts1a="7f000001" # "127.0.0.1"
ts0b=$(printf "%04x" $ts0p)
ts1b=$(printf "%04x" $ts1p)
ts1="x${ts0b}${ts1a}x0000000000000000"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts UDP4-DATAGRAM:$ts0a:$ts1p,bind=:$ts0p,reuseaddr PIPE"
CMD1="$TRACE $SOCAT $opts - SOCKET-DATAGRAM:2:$SOCK_DGRAM:17:$ts1,bind=x${ts1b}x00000000x0000000000000000"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" &
pid0="$!"
waitudp4port $ts0p 1
echo "$da" |$CMD1 2>>"${te}1" >"$tf"
rc1="$?"
kill "$pid0" 2>/dev/null; wait;
if [ "$rc1" -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   echo "$CMD0 &"
   cat "${te}0"
   echo "$CMD1"
   cat "${te}1"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat $te; fi
   ok
fi
fi ;; # NUMCOND
esac
N=$((N+1))

NAME=SOCKETRANGEMASK
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%generic%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%socket%*|*%range%*|*%listen%*|*%fork%*|*%retry%*|*%$NAME%*)
TEST="$NAME: security of generic socket-listen with RANGE option"
if ! eval $NUMCOND; then :;
elif [ -z "$SECONDADDR" ]; then
    # we need access to more loopback addresses
    $PRINTF "test $F_n $TEST... ${YELLOW}need a second IPv4 address${NORMAL}\n" $N
    cant
else
newport tcp4; ts1p=$(printf "%04x" $PORT);
testserversec "$N" "$TEST" "$opts" "SOCKET-LISTEN:2:6:x${ts1p}x00000000x0000000000000000,$REUSEADDR,fork,retry=1" "" "range=x0000x7f000000:x0000xffffffff" "SOCKET-CONNECT:2:6:x${ts1p}x${SECONDADDRHEX}x0000000000000000" 4 tcp $PORT 0
fi ;; # NUMCOND, $SECONDADDR
esac
N=$((N+1))


# test the generic ioctl-void option
NAME=IOCTL_VOID
case "$TESTS" in
*%$N%*|*%functions%*|*%pty%*|*%generic%*|*%$NAME%*)
TEST="$NAME: test the ioctl-void option"
# there are not many ioctls that apply to non global resources and do not
# require root. TIOCEXCL seems to fit:
# process 0 provides a pty;
# process 1 opens it with the TIOCEXCL ioctl; 
# process 2 opens it too and fails with "device or resource busy" only when the
# previous ioctl was successful
if ! eval $NUMCOND; then :;
elif [ -z "$TIOCEXCL" ]; then
    # we use the numeric value of TIOCEXL which is system dependent
    $PRINTF "test $F_n $TEST... ${YELLOW}no value of TIOCEXCL${NORMAL}\n" $N
    cant
else
tp="$td/test$N.pty"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts PTY,LINK=$tp pipe"
CMD1="$TRACE $SOCAT $opts - file:$tp,ioctl-void=$TIOCEXCL,raw,echo=0"
CMD2="$TRACE $SOCAT $opts - file:$tp,raw,echo=0"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waitfile $tp 1
(echo "$da"; sleep 2) |$CMD1 >"$tf" 2>"${te}1" &	# this should always work
pid1=$!
sleep 1.0
$CMD2 >/dev/null 2>"${te}2" </dev/null
rc2=$?
kill $pid0 $pid1 2>/dev/null; wait
if ! echo "$da" |diff - "$tf" >/dev/null; then
    $PRINTF "${YELLOW}phase 1 failed${NORMAL}\n"
    echo "$CMD0 &"
    echo "$CMD1"
    echo "$da" |diff - "$tf"
    cant
elif [ $rc2 -eq 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    echo "$CMD1"
    echo "$CMD2"
    cat "${te}0" "${te}1" "${te}2"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}0" "${te}1" "${te}2"; fi
    ok
fi
fi # NUMCOND, TIOCEXCL
;;
esac
N=$((N+1))


# Test the generic setsockopt option
NAME=SETSOCKOPT
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%tcp%*|*%generic%*|*%listen%*|*%fork%*|*%$NAME%*)
TEST="$NAME: test the setsockopt option"
# Set the TCP_MAXSEG (MSS) option with a reasonable value, this should succeed.
# Then try again with TCP_MAXSEG=1, this fails at least on Linux.
# Thus:
# process 0 provides a tcp listening,forking socket
# process 1 connects to this port using reasonably MSS, data transfer should
# succeed.
# Then,
# process 2 connects to this port using a very small MSS, this should fail
if ! eval $NUMCOND; then :;
elif [ -z "$TCP_MAXSEG" ]; then
    # we use the numeric value of TCP_MAXSEG which might be system dependent
    $PRINTF "test $F_n $TEST... ${YELLOW}value of TCPMAXSEG not known${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts TCP4-L:$PORT,so-reuseaddr,fork PIPE"
CMD1="$TRACE $SOCAT $opts - TCP4:$LOCALHOST:$PORT,setsockopt=6:$TCP_MAXSEG:512"
CMD2="$TRACE $SOCAT $opts - TCP4:$LOCALHOST:$PORT,setsockopt=6:$TCP_MAXSEG:1"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
(echo "$da"; relsleep 1) |$CMD1 >"${tf}1" 2>"${te}1" 	# this should always work
rc1=$?
relsleep 1
(echo "$da"; relsleep 1) |$CMD2 >"${tf}2" 2>"${te}2" 	# this should fail
rc2=$?
kill $pid0 $pid1 $pid2 2>/dev/null; wait
if ! echo "$da" |diff - "${tf}1" >"$tdiff"; then
    $PRINTF "${YELLOW}phase 1 failed${NORMAL}\n"
    echo "$CMD0 &"
    cat ${te}0
    echo "$CMD1"
    cat ${te}1
    cat "$tdiff"
    cant
elif [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat ${te}0
    echo "$CMD1"
    cat ${te}1
    failed
elif [ $rc2 -eq 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat ${te}0
    echo "$CMD2"
    cat ${te}2
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}0" "${te}1" "${te}2"; fi
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test the generic setsockopt-listen option
# This test, with setsockopt-int, no longer worked due to fix for options on
# listening sockets
# Now it got a chance again using new option setsockopt-listen
#NAME=SETSOCKOPT_INT
NAME=SETSOCKOPT_LISTEN
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%tcp%*|*%generic%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test the setsockopt-listen option"
# there are not many socket options that apply to non global resources, do not
# require root, do not require a network connection, and can easily be
# tested. SO_REUSEADDR seems to fit:
# process 0 provides a tcp listening socket with reuseaddr;
# process 1 connects to this port; thus the port is connected but no longer
# listening
# process 2 tries to listen on this port with SO_REUSEADDR, will fail if the
# (generically specified) SO_REUSEADDR socket options did not work
# process 3 connects to this port; only if it is successful the test is ok
if ! eval $NUMCOND; then :;
elif [ -z "$SO_REUSEADDR" ]; then
    # we use the numeric value of SO_REUSEADDR which might be system dependent
    $PRINTF "test $F_n $TEST... ${YELLOW}value of SO_REUSEADDR not known${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts TCP4-L:$PORT,setsockopt-listen=$SOL_SOCKET:$SO_REUSEADDR:1 PIPE"
CMD1="$TRACE $SOCAT $opts - TCP4:$LOCALHOST:$PORT"
CMD2="$CMD0"
CMD3="$CMD1"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"	# this should always work
rc1=$?
kill $pid0 2>/dev/null; wait
$CMD2 >/dev/null 2>"${te}2" &
pid2=$!
waittcp4port $PORT 1
echo "$da" |$CMD3 >"${tf}3" 2>"${te}3"
rc3=$?
kill $pid2 2>/dev/null; wait
if ! echo "$da" |diff - "${tf}1" >"${tdiff}1"; then
    $PRINTF "${YELLOW}phase 1 failed${NORMAL}\n"
    echo "$CMD0 &"
    cat ${te}0
    echo "$CMD1"
    cat ${te}1
    cat "${tdiff}1"
    cant
elif [ $rc3 -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat ${te}0
    echo "$CMD1"
    cat ${te}1
    echo "$CMD2 &"
    cat ${te}2
    echo "$CMD3"
    cat ${te}3
    failed
elif ! echo "$da" |diff - "${tf}3" >"${tdiff}3"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat ${te}0
    echo "$CMD1"
    cat ${te}1
    echo "$CMD2 &"
    cat ${te}2
    echo "$CMD3"
    cat ${te}3
    cat "${tdiff}3"
    cant
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}0" "${te}1" "${te}2" "${te}3"; fi
    ok
fi
fi # NUMCOND, SO_REUSEADDR
 ;;
esac
N=$((N+1))


NAME=SCTP4STREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%sctp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to SCTP V4 socket"
if ! eval $NUMCOND; then :;
elif ! testfeats sctp ip4 >/dev/null || ! runsip4 >/dev/null || ! runssctp4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}SCTP4 not available${NORMAL}\n" $N
    cant
elif [ "$UNAME" = Linux ] && ! grep ^sctp /proc/modules >/dev/null; then
    # RHEL5 based systems became unusable when an sctp socket was created but
    # module sctp not loaded
    $PRINTF "test $F_n $TEST...${YELLOW}load sctp module!${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport sctp4; tsl=$PORT
ts="127.0.0.1:$tsl"
da=$(date)
CMD1="$TRACE $SOCAT $opts SCTP4-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT SCTP4:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1=$!
waitsctp4port $tsl 1
# SCTP does not seem to support half close, so we give it 1s to finish
(echo "$da"; sleep 1) |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   cat "${te}1"
   echo "$CMD2"
   cat "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid1 2>/dev/null
wait
fi # NUMCOND, feats
 ;;
esac
N=$((N+1))

NAME=SCTP6STREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%sctp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: echo via connection to SCTP V6 socket"
if ! eval $NUMCOND; then :;
elif ! testfeats sctp ip6 >/dev/null || ! runsip6 >/dev/null || ! runssctp6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}SCTP6 not available${NORMAL}\n" $N
    cant
elif [ "$UNAME" = Linux ] && ! grep ^sctp /proc/modules >/dev/null; then
    $PRINTF "test $F_n $TEST...${YELLOW}load sctp module!${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport sctp6; tsl=$PORT
ts="[::1]:$tsl"
da=$(date)
CMD1="$TRACE $SOCAT $opts SCTP6-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT SCTP6:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid=$!	# background process id
waitsctp6port $tsl 1
# SCTP does not seem to support half close, so we let it 1s to finish
(echo "$da"; sleep 1) |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
   $PRINTF "$FAILED: $TRACE $SOCAT:\n"
   echo "$CMD1 &"
   cat "${te}1"
   echo "$CMD2"
   cat "${te}2"
   failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $PRINTF "$FAILED: diff:\n"
   cat "$tdiff"
   failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
fi # NUMCOND, feats
 ;;
esac
N=$((N+1))


if type openssl >/dev/null 2>&1; then
    OPENSSL_METHOD=$(openssl s_client -help 2>&1 |$GREP_E -e '-tls1_[012]' |sed -e 's/.*\(-tls1_[012]\).*/\1/' |sort |tail -n 1)
    #OPENSSL_METHOD=$(openssl s_client -help 2>&1 |$GREP_E -o -e '-tls1(_[012])?' |sort |tail -n 1)
    [ -z "$OPENSSL_METHOD" ] && OPENSSL_METHOD="-tls1" 	# just so
fi

# Old versions have DTLS hang, new versions cannot by client renegotiation, ...
OPENSSL_VERSION="$(openssl version)"
OPENSSL_VERSION="${OPENSSL_VERSION#* }"
OPENSSL_VERSION="${OPENSSL_VERSION%%[ -]*}"
[ "$DEFS" ] && echo "OPENSSL_VERSION=\"$OPENSSL_VERSION\"" >&2

# socat up to 1.7.1.1 (and 2.0.0-b3) terminated with error when an openssl peer
# performed a renegotiation. Test if this is fixed.
# Note: the renegotiation feature in OpenSSL exists only up to TLSv1.2
NAME=OPENSSLRENEG1
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%openssl%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL connections survive renogotiation"
# connect with s_client to socat ssl-l; force a renog, then transfer data. When
# data is passed the test succeeded
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! type openssl >/dev/null 2>&1; then
    $PRINTF "test $F_n $TEST... ${YELLOW}openssl executable not available${NORMAL}\n" $N
    cant
elif ! re_match "$(echo $OPENSSL_VERSION |awk '{print($2);}')" '[01].*'; then
    # openssl s_client apparently provides renegotiation only up to version 1.2
    $PRINTF "test $F_n $TEST... ${YELLOW}not with OpenSSL $OPENSSL_VERSION${NORMAL}\n" $N
    cant
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,cert=testsrv.crt,key=testsrv.key,verify=0 PIPE"
#CMD1="openssl s_client -port $PORT -verify 0" 	# not with openssl 1.1.0g
CMD1="openssl s_client $OPENSSL_S_CLIENT_4 $OPENSSL_METHOD -port $PORT"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
(echo "R"; sleep 1; echo "$da"; sleep 1) |$CMD1 2>"${te}1" |$GREP_F "$da" >"${tf}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if echo "$da" |diff - ${tf}1 >"$tdiff"; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
elif grep -i "Connection refused" "${te}1" >/dev/null; then
    $PRINTF "$CANT (conn failed)\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    cant
else
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "// diff:" >&2
    cat "$tdiff" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# socat up to 1.7.1.1 (and 2.0.0-b3) terminated with error when an openssl peer
# performed a renegotiation. The first temporary fix to this problem might
# leave socat in a blocking ssl-read state. Test if this has been fixed.
# Note: the renegotiation feature in OpenSSL exists only up to TLSv1.2
NAME=OPENSSLRENEG2
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%openssl%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL connections do not block after renogotiation"
# connect with s_client to socat ssl-l; force a renog, then transfer data from
# socat to the peer. When data is passed this means that the former ssl read no
# longer blocks and the test succeeds
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! type openssl >/dev/null 2>&1; then
    $PRINTF "test $F_n $TEST... ${YELLOW}openssl executable not available${NORMAL}\n" $N
    cant
elif ! re_match "$(echo $OPENSSL_VERSION |awk '{print($2);}')" '[01].*'; then
    # openssl s_client apparently provides renegotiation only up to version 1.2
    $PRINTF "test $F_n $TEST... ${YELLOW}not with OpenSSL $OPENSSL_VERSION${NORMAL}\n" $N
    cant
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
init_openssl_s_client
newport tcp4
CMD0="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,cert=testsrv.crt,key=testsrv.key,verify=0 SYSTEM:\"sleep 1; echo \\\\\\\"\\\"$da\\\"\\\\\\\"; sleep 1\"!!STDIO"
#CMD1="openssl s_client -port $PORT -verify 0" 	# not with openssl 1.1.0g
CMD1="openssl s_client $OPENSSL_S_CLIENT_4 $OPENSSL_METHOD -port $PORT"
printf "test $F_n $TEST... " $N
eval "$CMD0 >/dev/null 2>\"${te}0\" &"
pid0=$!
waittcp4port $PORT 1
(echo "R"; sleep 2) |$CMD1 2>"${te}1" |$GREP_F "$da" >"${tf}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if echo "$da" |diff - ${tf}1 >"$tdiff"; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
elif grep -i "Connection refused" "${te}1" >/dev/null; then
    $PRINTF "$CANT (conn failed)\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    cant
else
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "// diff:" >&2
    cat "$tdiff" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# socat up to 1.7.1.2 had a stack overflow vulnerability that occurred when
# command line arguments (whole addresses, host names, file names) were longer
# than 512 bytes.
NAME=HOSTNAMEOVFL
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%security%*|*%socket%*|*%$NAME%*)
TEST="$NAME: stack overflow on overly long host name"
# provide a long host name to TCP-CONNECT and check socats exit code
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
# prepare long data - perl might not be installed
rm -f "$td/test$N.dat"
i=0; while [ $i -lt 64 ]; do  echo -n "AAAAAAAAAAAAAAAA" >>"$td/test$N.dat"; i=$((i+1)); done
newport tcp4
CMD0="$TRACE $SOCAT $opts TCP-CONNECT:$(cat "$td/test$N.dat"):$PORT STDIO"
printf "test $F_n $TEST... " $N
$CMD0 </dev/null 1>&0 2>"${te}0"
rc0=$?
if [ $rc0 -lt 128 ] || [ $rc0 -eq 255 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# socat up to 1.7.1.2 had a stack overflow vulnerability that occurred when
# command line arguments (whole addresses, host names, file names) were longer
# than 512 bytes.
NAME=FILENAMEOVFL
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%security%*|*%openssl%*|*%$NAME%*)
TEST="$NAME: stack overflow on overly long file name"
# provide a 600 bytes long key file option to OPENSSL-CONNECT and check socats exit code
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
i=0; while [ $i -lt 64 ]; do  echo -n "AAAAAAAAAAAAAAAA" >>"$td/test$N.dat"; i=$((i+1)); done
newport tcp4
CMD0="$TRACE $SOCAT $opts OPENSSL:localhost:$PORT,key=$(cat "$td/test$N.dat") STDIO"
printf "test $F_n $TEST... " $N
$CMD0 </dev/null 1>&0 2>"${te}0"
rc0=$?
if [ $rc0 -lt 128 ] || [ $rc0 -eq 255 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# socat up to 1.7.3.0 had a stack overflow vulnerability that occurred when
# command line arguments (whole addresses, host names, file names) were longer
# than 512 bytes and specially crafted.
NAME=NESTEDOVFL
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%security%*|*%exec%*|*%$NAME%*)
TEST="$NAME: stack overflow on overly long nested arg"
# provide a long host name to TCP-CONNECT and check socats exit code
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
# prepare long data - perl might not be installed
rm -f "$td/test$N.dat"
i=0; while [ $i -lt 64 ]; do  echo -n "AAAAAAAAAAAAAAAA" >>"$td/test$N.dat"; i=$((i+1)); done
CMD0="$TRACE $SOCAT $opts EXEC:[$(cat "$td/test$N.dat")] STDIO"
printf "test $F_n $TEST... " $N
$CMD0 </dev/null 1>&0 2>"${te}0"
rc0=$?
if [ $rc0 -lt 128 ] || [ $rc0 -eq 255 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# test for a bug in gopen that lead to crash or warning when opening a unix
# domain socket with GOPEN
NAME=GOPEN_UNIX_CRASH
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%gopen%*|*%unix%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: check crash when connecting to a unix domain socket using address GOPEN"
# a unix domain server is started in background. the check process connects to
# its socket. when this process crashes or issues a warning the bug is present.
# please note that a clean behaviour does not proof anything; behaviour of bug
# depends on the value of an uninitialized var
#set -vx
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
ts="$td/test$N.sock"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts UNIX-LISTEN:$ts PIPE"
CMD1="$TRACE $SOCAT $opts -d - GOPEN:$ts"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" </dev/null &
pid0=$!
waitunixport "$ts" 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    failed
elif grep -q ' W ' "${te}1"; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    failed
elif  ! echo "$da" |diff - ${tf}1 >"$tdiff"; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# test if socat keeps an existing file where it wanted to create a UNIX socket
NAME=UNIXLISTEN_KEEPFILE
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%unix%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: socat keeps an existing file where it wanted to create a UNIX socket"
# we create a file and start socat with UNIX-LISTEN on this file. expected
# behaviour: socat exits immediately with error, but keeps the file
# up to 1.7.1.3, it removed the file
if ! eval $NUMCOND; then :; else
tf="$td/test$N.file"
te="$td/test$N.stderr"
CMD0="$TRACE $SOCAT $opts -u UNIX-LISTEN:$tf /dev/null"
printf "test $F_n $TEST... " $N
rm -f "$tf"; touch "$tf"
$CMD0 >/dev/null 2>"${te}0"
rc0=$?
if [ $rc0 -ne 0 -a -f "$tf" ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# PTY address allowed to specify address parameters but ignored them
NAME=PTY_VOIDARG
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%pty%*|*%$NAME%*)
TEST="$NAME: check if address params of PTY produce error"
# invoke socat with address PTY and some param; expect an error
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts /dev/null PTY:/tmp/xyz"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0"
rc0=$?
if [ $rc0 -ne 0 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# incomplete writes were reported but led to data loss
NAME=INCOMPLETE_WRITE
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%$NAME%*)
TEST="$NAME: check if incomplete writes are handled properly"
# write to a nonblocking fd a block that is too large for atomic write
# and check if all data arrives
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tp="$td/test$N.pipe"
tw="$td/test$N.wc-c"
# this is the size we write() in one call; data is never stored on disk, so
# make it large enough to exceed any atomic write size; but higher number might
# take much time
# Note: in OpenBSD-4 the PIPE does not deliver EOF, thus -T
bytes=100000	# for Linux 2.6.? this must be >65536
CMD0="$TRACE $SOCAT $opts -u -T 2 PIPE:$tp STDOUT"
CMD1="$TRACE $SOCAT $opts -u -b $bytes OPEN:/dev/zero,readbytes=$bytes FILE:$tp,o-nonblock"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" |wc -c >"$tw" &
pid=$!
waitfile "$tp"
$CMD1 2>"${te}1" >"${tf}1"
rc1=$?
wait
if [ $rc1 -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
elif [ ! -e "$tw" ]; then 
	$PRINTF "$NO_RESULT (no wc -c output)\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	cant
elif [ "$bytes" -eq $(cat "$tw") ]; then
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
else
	$PRINTF "$FAILED (incomplete)\n"
	echo "transferred only $(cat $tw) of $bytes bytes" >&2
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=OPENSSL_ANULL
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL server with cipher aNULL "
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD2="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,$REUSEADDR,$SOCAT_EGD,ciphers=aNULL,verify=0 pipe"
CMD="$TRACE $SOCAT $opts - openssl:$LOCALHOST:$PORT,ciphers=aNULL,verify=0,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
eval "$CMD2 2>\"${te}1\" &"
pid=$!	# background process id
waittcp4port $PORT
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "${YELLOW}FAILED${NORMAL}\n"
    #echo "$CMD2 &"
    #echo "$CMD"
    #cat "${te}1"
    #cat "${te}2"
    #cat "$tdiff"
    ok
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


while read KEYW FEAT ADDR IPPORT; do
if [ -z "$KEYW" ] || [[ "$KEYW" == \#* ]]; then continue; fi
RUNS=$(tolower $KEYW)
PROTO=$KEYW
proto="$(tolower "$PROTO")"
feat="$(tolower "$FEAT")"
# test the max-children option on really connection oriented sockets
NAME=${KEYW}_L_MAXCHILDREN
case "$TESTS" in
*%$N%*|*%functions%*|*%fork%*|*%maxchildren%*|*%$feat%*|*%$proto%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: max-children option with $PROTO-LISTEN"
# start a listen process with max-children=1; connect with a client, let it
# sleep some time before sending data; connect with second client that sends
# data immediately. If max-children is working correctly the first data should
# arrive first because the second process has to wait.
if ! eval $NUMCOND; then :;
elif ! testfeats "$FEAT" >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$FEAT not available${NORMAL}\n" $N
    cant
elif ! runs$RUNS >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(toupper $RUNS) not available${NORMAL}\n" $N
    cant
else
case "X$IPPORT" in
    "XPORT")
    newport $proto
    tsl=$PORT 		# test socket listen address
    tsc="$ADDR:$PORT"	# test socket connect address
    ;;
    *)
    tsl="$(eval echo "$ADDR")"	# resolve $N
    tsc=$tsl
esac
#ts="$td/test$N.sock"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -U FILE:$tf,o-trunc,o-creat,o-append $PROTO-LISTEN:$tsl,$REUSEADDR,fork,max-children=1"
CMD1="$TRACE $SOCAT $opts -u - $PROTO-CONNECT:$tsc,shut-null"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
wait${proto}port $tsl 1
(echo "$da 1"; relsleep 2) |$CMD1 >"${tf}1" 2>"${te}1" &
pid1=$!
relsleep 1
echo "$da 2" |$CMD1 >"${tf}2" 2>"${te}2" &
pid2=$!
relsleep 2
kill $pid1 $pid2 $pid0 2>/dev/null; wait
if echo -e "$da 1\n$da 2" |diff - $tf >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "(echo \"$da 1\"; sleep 2) |$CMD1"
    echo "echo \"$da 2\" |$CMD1"
    cat "${te}0"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))
done <<<"
TCP4  TCP  127.0.0.1 PORT
TCP6  TCP  [::1]     PORT
SCTP4 SCTP 127.0.0.1 PORT
SCTP6 SCTP [::1]     PORT
UNIX  unix  $td/test\$N.server -
"
# debugging this hanging test was difficult - following lessons learned:
# kill <parent> had no effect when child process existed
# strace -f (on Fedora-23) sometimes writes/pads? blocks with \0, overwriting client traces
# using the TRACE feature lets above kill command kill strace, not socat
# care for timing, understand what you want :-)


# test the max-children option on pseudo connected sockets
while read KEYW FEAT SEL ADDR IPPORT SHUT; do
if [ -z "$KEYW" ] || [[ "$KEYW" == \#* ]]; then continue; fi
RUNS=$(tolower $KEYW)
PROTO=$KEYW
proto="$(tolower "$PROTO")"
# test the max-children option on pseudo connected sockets
NAME=${KEYW}_L_MAXCHILDREN
case "$TESTS" in
*%$N%*|*%functions%*|*%fork%*|*%maxchildren%*|*%$SEL%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: max-children option with $PROTO-LISTEN"
# start a listen process with max-children=1; connect with a client, let it
# send data and then sleep; connect with second client that wants to send
# data immediately, but keep first client active until server terminates.
#If max-children is working correctly only the first data should
# arrive.
if ! eval $NUMCOND; then :;
elif ! testfeats "$FEAT" >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$FEAT not available${NORMAL}\n" $N
    cant
elif ! runs$RUNS >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(toupper $RUNS) not available${NORMAL}\n" $N
    cant
else
case "X$IPPORT" in
    "XPORT")
    newport $proto
    tsl=$PORT 		# test socket listen address
    tsc="$ADDR:$PORT"	# test socket connect address
    ;;
    *)
    tsl="$(eval echo "$ADDR")"	# resolve $N
    tsc=$tsl
esac
#ts="$td/test$N.sock"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
# on some Linux distributions it hangs, thus -T option here
CMD0="$TRACE $SOCAT $opts -U -T $(reltime 4) FILE:$tf,o-trunc,o-creat,o-append $PROTO-LISTEN:$tsl,$REUSEADDR,fork,max-children=1"
CMD1="$TRACE $SOCAT $opts -u - $PROTO-CONNECT:$tsc,$SHUT"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
wait${proto}port $tsl 1
(echo "$da 1"; relsleep 3) |$CMD1 >"${tf}1" 2>"${te}1" &
pid1=$!
relsleep 1
echo "$da 2" |$CMD1 >"${tf}2" 2>"${te}2" &
pid2=$!
relsleep 1
cpids="$(childpids $pid0)"
kill $pid1 $pid2 $pid0 $cpids 2>/dev/null; wait
if echo -e "$da 1" |diff - $tf >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "(echo \"$da 1\"; sleep 2) |$CMD1"
    echo "echo \"$da 2\" |$CMD1"
    cat "${te}0"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))
done <<<"
UDP4      UDP      udp      127.0.0.1 PORT shut-null
UDP6      UDP      udp      [::1]     PORT shut-null
"
# debugging this hanging test was difficult - following lessons learned:
# kill <parent> had no effect when child process existed
# strace -f (on Fedora-23) sometimes writes/pads? blocks with \0, overwriting client traces
# using the TRACE feature lets above kill command kill strace, not socat
# care for timing, understand what you want :-)


# socat up to 1.7.2.0 had a bug in xioscan_readline() that could be exploited
# to overflow a heap based buffer (socat security advisory 3)
# problem reported by Johan Thillemann
NAME=READLINE_OVFL
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%security%*|*%readline%*|*%pty%*|*%$NAME%*)
TEST="$NAME: test for buffer overflow in readline prompt handling"
# address 1 is the readline where write data was handled erroneous
# address 2 provides data to trigger the buffer overflow
# when no SIGSEGV or so occurs the test succeeded (bug fixed)
if ! eval $NUMCOND; then :;
elif ! feat=$(testfeats readline pty); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
ti="$td/test$N.data"
CMD0="$SOCAT $opts READLINE $ti"
printf "test $F_n $TEST... " $N
# prepare long data - perl might not be installed
#perl -e 'print "\r","Z"x513' >"$ti"
echo $E -n "\rA" >"$ti"
i=0; while [ $i -lt 32 ]; do echo -n "AAAAAAAAAAAAAAAA" >>"$ti"; let i=i+1; done
$TRACE $SOCAT - SYSTEM:"$CMD0; echo rc=\$? >&2",pty >/dev/null 2>"${te}0"
rc=$?
rc0="$(grep ^rc= "${te}0" |sed 's/.*=//')"
if [ $rc -ne 0 ]; then
    $PRINTF "${YELLOW}framework failed${NORMAL}\n"
elif [ $rc0 -eq 0 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    grep -v ^rc= "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Does Socat have -d0 option?
opt_d0=
if $SOCAT -h |grep -e -d0 >/dev/null; then
    opt_d0="-d0"
fi

# socat up to 1.7.2.1 did only shutdown() but not close() an accept() socket 
# that was rejected due to range, tcpwrap, lowport, or sourceport option.
# This file descriptor leak could be used for a denial of service attack.
NAME=FDLEAK
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%security%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: file descriptor leak with range option"
# have a TCP-LISTEN with range option; connect with wrong source address until
# "open files" limit would exceed. When server continues operation the bug is
# not present.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
RLIMIT_NOFILE="$(ulimit -n)"
#if ! [[ "$RLIMIT_NOFILE" =~ ^[0-9][0-9]*$ ]]; then
if ! re_match "$RLIMIT_NOFILE" '^[0-9][0-9]*$'; then
    $PRINTF "${YELLOW}cannot determine ulimit -n${NORMAL}"
else
if [ $RLIMIT_NOFILE -gt 1024 ]; then
    ulimit -n 1024 	# 65536 takes too long
    RLIMIT_NOFILE="$(ulimit -n)"
fi
newport tcp4
CMD0="$TRACE $SOCAT $opt_d0 $opts TCP4-LISTEN:$PORT,$REUSEADDR,range=$LOCALHOST:255.255.255.255 PIPE"
#CMD0="$TRACE $SOCAT $opts TCP-LISTEN:$PORT,pf=ip4,$REUSEADDR,range=$LOCALHOST4:255.255.255.255 PIPE"
CMD1="$TRACE $SOCAT $opts -t 0 /dev/null TCP4:$SECONDADDR:$PORT,bind=$SECONDADDR"
CMD2="$TRACE $SOCAT $opts - TCP:$LOCALHOST4:$PORT,bind=$LOCALHOST4"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
while [ $RLIMIT_NOFILE -gt 0 ]; do
    $CMD1 >/dev/null 2>>"${te}1"
    let RLIMIT_NOFILE=RLIMIT_NOFILE-1
done
echo "$da" |$CMD2 >"${tf}2" 2>"${te}2"
rc2=$?
kill $pid0 2>/dev/null; wait
echo -e "$da" |diff "${tf}2" - >$tdiff
if [ $rc2 -ne 0 ]; then
    $PRINTF "$FAILED (rc2=$rc2)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
elif [ -f "$tdiff" -a ! -s "$tdiff" ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
fi
fi # ulimit -n
fi # NUMCOND
 ;;
esac
N=$((N+1))


if false; then	# this overflow is not reliably reproducable
# Socat up to 2.0.0-b6 did not check the length of the PROXY-CONNECT command
# line parameters when copying them into the HTTP request buffer. This could
# lead to a buffer overflow.
NAME=PROXY_ADDR_OVFL
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%security%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: proxy address parameters overflow"
# invoke socat PROXY-CONNECT with long proxy server and target server names. If it terminates with exit code >= 128 it is vulnerable
# However, even if vulnerable it often does not crash. Therefore we try to use a boundary check program like ElectricFence; only with its help we can tell that clean run proofs absence of vulnerability
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
EF=; for p in ef; do
    if type ef >/dev/null 2>&1; then
	EF="ef "; break
    fi
done
newport tcp4
CMD0="$TRACE $SOCAT $opts TCP-LISTEN:$PORT,$REUSEADDR FILE:/dev/null"
#CMD1="$EF $TRACE $SOCAT $opts FILE:/dev/null PROXY-CONNECT:$(perl -e "print 'A' x 256"):$(perl -e "print 'A' x 256"):80"
CMD1="$EF $TRACE $SOCAT $opts FILE:/dev/null PROXY-CONNECT:localhost:$(perl -e "print 'A' x 384"):80,proxyport=$PORT"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
$CMD1 >/dev/null 2>"${te}1"
rc1=$?
if [ $rc1 -lt 128 ]; then
    if [ "$EF" ]; then
	$PRINTF "$OK\n"
	ok
    else
	$PRINTF "$UNKNOWN $RED(install ElectricFEnce!)$NORMAL\n"
	cant
    fi
else
    $PRINTF "$FAILED\n"
    echo "$CMD1"
    cat "${te}"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))
fi	# false


# LISTEN addresses in socat up to 1.7.2.1 applied many file descriptor, socket,
# and TCP options only to the listening socket instead of the connection socket.
NAME=LISTEN_KEEPALIVE
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%listen%*|*%keepalive%*|*%socket%*|*%listen%*|*%fork%*|*%$NAME%*)
TEST="$NAME: keepalive option is applied to connection socket"
# Instance 0 has TCP-LISTEN with option so-keepalive and invokes filan after
# accept(). filan writes its output to the socket. instance 1 connects to 
# instance 0. The value of the sockets so-keepalive option is checked, it must
# be 1
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
#tdiff="$td/test$N.diff"
#da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts TCP4-LISTEN:$PORT,$REUSEADDR,so-keepalive EXEC:\"$FILAN -i 1\",nofork"
CMD1="$TRACE $SOCAT $opts - TCP4:$LOCALHOST:$PORT"
printf "test $F_n $TEST... " $N
eval $CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
KEEPALIVE="$(cat "${tf}1" |tail -n +2 |sed -e "s/.*KEEPALIVE=//" -e "s/[[:space:]].*//")"
rc1=$?
kill $pid0 2>/dev/null; wait
if [ -z "$KEEPALIVE" ]; then
    $PRINTF "$NO_RESULT\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    cant
elif [ "$KEEPALIVE" = "1" ]; then
    $PRINTF "$OK\n";
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
else
    $PRINTF "$FAILED (KEEPALIVE=$KEEPALIVE)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# OPENSSL-CONNECT with bind option failed on some systems (eg.FreeBSD, but not
# Linux) with "Invalid argument".
NAME=OPENSSL_CONNECT_BIND
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%openssl%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test OPENSSL-CONNECT with bind option"
# have a simple SSL server that just echoes data.
# connect with socat using OPENSSL-CONNECT with bind, send data and check if the
# reply is identical.
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf0="$td/test$N.0.stdout"
te0="$td/test$N.0.stderr"
tf1="$td/test$N.1.stdout"
te1="$td/test$N.1.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,cert=testsrv.pem,verify=0 PIPE"
CMD1="$TRACE $SOCAT $opts - OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,bind=$LOCALHOST,verify=0"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"$te0" &
pid0=$!
waittcp4port $PORT 1
echo "$da" |$CMD1 >"$tf1" 2>"$te1"
rc1=$?
kill $pid0 2>/dev/null; wait
if [ "$rc1" -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "$te0"
    cat "$te1"
    failed
elif ! echo "$da" |diff - $tf1 >"$tdiff"; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# socat up to version 1.7.2.3
# had a bug that converted a bit mask of 0 internally to 0xffffffff
NAME=TCP4RANGE_0BITS
case "$TESTS" in
*%$N%*|*%functions%*|*%fork%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%range%*|*%listen%*|*%retry%*|*%$NAME%*)
TEST="$NAME: correct evaluation of range mask 0"
if ! eval $NUMCOND; then :;
elif [ -z "$SECONDADDR" ]; then
    # we need access to a second address
    $PRINTF "test $F_n $TEST... ${YELLOW}need a second IPv4 address${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
#testserversec "$N" "$TEST" "$opts" "tcp4-l:$PORT,reuseaddr,fork,retry=1" "" "range=$SECONDADDR/32" "tcp4:127.0.0.1:$PORT" 4 tcp $PORT 0
CMD0="$TRACE $SOCAT $opts -u TCP4-LISTEN:$PORT,$REUSEADDR,range=127.0.0.1/0 CREATE:$tf"
CMD1="$TRACE $SOCAT $opts -u - TCP4-CONNECT:$SECONDADDR:$PORT,bind=$SECONDADDR"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
sleep 1
kill $pid0 2>/dev/null; wait
if [ $rc1 != 0 ]; then
    $PRINTF "${YELLOW}invocation failed${NORMAL}\n"
    cant
elif ! [ -f "$tf" ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "${YELLOW}diff failed${NORMAL}\n"
    cant
else
    $PRINTF "$OK\n"
    ok
fi

fi ;; # $SECONDADDR, NUMCOND
esac
N=$((N+1))


# test: OPENSSL sets of environment variables with important values of peer certificate
newport tcp4
while read ssldist MODE MODULE FIELD TESTADDRESS PEERADDRESS VALUE; do
if [ -z "$ssldist" ] || [[ "$ssldist" == \#* ]]; then continue; fi
#
SSLDIST=$(toupper $ssldist)
NAME="ENV_${SSLDIST}_${MODE}_${MODULE}_${FIELD}"
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%tcp%*|*%$ssldist%*|*%envvar%*|*%listen%*|*%$NAME%*)
TEST="$NAME: $SSLDIST sets env SOCAT_${SSLDIST}_${MODULE}_${FIELD}"
# have a server accepting a connection and invoking some shell code. The shell
# code extracts and prints the SOCAT related environment vars.
# outside code then checks if the environment contains the variables correctly
# describing the desired field.
FEAT=$(echo "$ssldist" |tr a-z A-Z)
if ! eval $NUMCOND; then :;
elif ! testfeats $FEAT >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$FEAT not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
gentestcert testsrv
gentestcert testcli
test_proto=tcp4
case "$MODE" in
    SERVER)
	CMD0="$SOCAT $opts -u -lp socat $TESTADDRESS SYSTEM:\"echo SOCAT_${SSLDIST}_${MODULE}_${FIELD}=\\\$SOCAT_${SSLDIST}_${MODULE}_${FIELD}; sleep 1\""
	CMD1="$SOCAT $opts -u /dev/null $PEERADDRESS"
	printf "test $F_n $TEST... " $N
	eval "$CMD0 2>\"${te}0\" >\"$tf\" &"
	pid0=$!
	wait${test_proto}port $PORT 1
	{ $CMD1 2>"${te}1"; sleep 1; }
	rc1=$?
	waitfile "$tf" 2
	kill $pid0 2>/dev/null; wait
	;;
    CLIENT)
	CMD0="$SOCAT $opts -u /dev/null $PEERADDRESS"
	CMD1="$SOCAT $opts -u -lp socat $TESTADDRESS SYSTEM:\"echo SOCAT_${SSLDIST}_${MODULE}_${FIELD}=\\\$SOCAT_${SSLDIST}_${MODULE}_${FIELD}; sleep 1\""
	printf "test $F_n $TEST... " $N
	$CMD0 2>"${te}0" &
	pid0=$!
	wait${test_proto}port $PORT 1
	eval "$CMD1 2>\"${te}1\" >\"$tf\""
	rc1=$?
	waitfile "$tf" 2
	kill $pid0 2>/dev/null; wait
	;;
esac
if [ $rc1 != 0 ]; then
    $PRINTF "$NO_RESULT (client failed):\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1"
    cat "${te}1"
    cant
elif effval="$(grep SOCAT_${SSLDIST}_${MODULE}_${FIELD} "${tf}" |sed -e 's/^[^=]*=//' |sed -e "s/[\"']//g")";
    [ "$effval" = "$VALUE" ]; then
    $PRINTF "$OK\n"
    if [ "$debug" ]; then
	echo "$CMD0 &"
	cat "${te}0"
	echo "$CMD1"
	cat "${te}1"
    fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "expected \"$VALUE\", got \"$effval\"" >&2
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1"
    cat "${te}1"
    failed
fi
fi # NUMCOND, feats
 ;;
esac
N=$((N+1))
#
done <<<"
openssl SERVER X509 ISSUER  OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,bind=$LOCALHOST,cert=testsrv.pem,cafile=testcli.crt,verify=1 OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cert=testcli.pem,cafile=testsrv.crt,verify=1 $TESTCERT_ISSUER
openssl SERVER X509 SUBJECT OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,bind=$LOCALHOST,cert=testsrv.pem,cafile=testcli.crt,verify=1 OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cert=testcli.pem,cafile=testsrv.crt,verify=1 $TESTCERT_SUBJECT
openssl SERVER X509 COMMONNAME             OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,bind=$LOCALHOST,cert=testsrv.pem,cafile=testcli.crt,verify=1 OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cert=testcli.pem,cafile=testsrv.crt,verify=1 $TESTCERT_COMMONNAME
openssl SERVER X509 COUNTRYNAME            OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,bind=$LOCALHOST,cert=testsrv.pem,cafile=testcli.crt,verify=1 OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cert=testcli.pem,cafile=testsrv.crt,verify=1 $TESTCERT_COUNTRYNAME
openssl SERVER X509 LOCALITYNAME           OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,bind=$LOCALHOST,cert=testsrv.pem,cafile=testcli.crt,verify=1 OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cert=testcli.pem,cafile=testsrv.crt,verify=1 $TESTCERT_LOCALITYNAME
openssl SERVER X509 ORGANIZATIONALUNITNAME OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,bind=$LOCALHOST,cert=testsrv.pem,cafile=testcli.crt,verify=1 OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cert=testcli.pem,cafile=testsrv.crt,verify=1 $TESTCERT_ORGANIZATIONALUNITNAME
openssl SERVER X509 ORGANIZATIONNAME       OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,bind=$LOCALHOST,cert=testsrv.pem,cafile=testcli.crt,verify=1 OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cert=testcli.pem,cafile=testsrv.crt,verify=1 $TESTCERT_ORGANIZATIONNAME
openssl CLIENT X509 SUBJECT OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cert=testcli.pem,cafile=testsrv.crt,verify=1 OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,bind=$LOCALHOST,cert=testsrv.pem,cafile=testcli.crt,verify=1 $TESTCERT_SUBJECT
openssl CLIENT X509 ISSUER  OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cert=testcli.pem,cafile=testsrv.crt,verify=1 OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,bind=$LOCALHOST,cert=testsrv.pem,cafile=testcli.crt,verify=1 $TESTCERT_ISSUER
"


###############################################################################
# tests: option umask with "passive" NAMED group addresses
while read addr fileopt addropts proto diropt ADDR2; do
if [ -z "$addr" ] || [[ "$addr" == \#* ]]; then continue; fi
# some passive (listening...) filesystem based addresses did not implement the
# umask option
ADDR=$(toupper $addr)
ADDR_=${ADDR/-/_}
#PROTO=$(toupper $proto)
if [ "$diropt" = "." ]; then diropt=; fi
if [ "$fileopt" = "." ]; then fileopt=; fi
if [ "$addropts" = "." ]; then addropts=; fi
NAME=${ADDR_}_UMASK
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%proto%*|*%socket%*|*%$proto%*|*%umask%*|*%listen%*|*%$NAME%*)
TEST="$NAME: $ADDR applies option umask"
# start a socat process with passive/listening file system entry. Check the
# permissions of the FS entry, then terminate the process.
# Test succeeds when FS entry exists and has expected permissions.
if ! eval $NUMCOND; then :; else
                            if [ $ADDR = PTY ]; then  set -xv; fi
tlog="$td/test$N.log"
te0="$td/test$N.0.stderr"
tsock="$td/test$N.sock"
if [ -z "$fileopt" ]; then
    CMD0="$TRACE $SOCAT $opts $diropt $ADDR:$tsock,$addropts,unlink-close=0,umask=177 $ADDR2"
else
    CMD0="$TRACE $SOCAT $opts $diropt $ADDR,$fileopt=$tsock,$addropts,unlink-close=0,umask=177 $ADDR2"
fi
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"$te0" &
pid0=$!
wait${proto} $tsock 1 2>"$tlog"
ERRNOENT=; if ! [ -e "$tsock" ]; then  ERRNOENT=1;  fi
perms=$(fileperms "$tsock")
kill $pid0 2>>"$tlog"
wait
if [ "$ERRNOENT" ]; then
    $PRINTF "${RED}no entry${NORMAL}\n"
    echo "$CMD0 &"
    cat "$te0"
    cat "$tlog"
    failed
elif [ "$perms" != "600" ]; then
    $PRINTF "${RED}perms \"$perms\", expected \"600\" ${NORMAL}\n"
    echo "$CMD0 &"
    cat "$te0"
    failed
else
    $PRINTF "$OK\n"
    ok
fi
                               set +xv
fi # NUMCOND
 ;;
esac
N=$((N+1))
#
done <<<"
# address     fileopt addropts waitfor direction ADDR2
create        .       .        file     -U       FILE:/dev/null
open          .       creat    file     .        FILE:/dev/null
gopen         .       creat    file     .        FILE:/dev/null
unix-listen   .       .        unixport .        FILE:/dev/null
unix-recvfrom .       .        unixport .        FILE:/dev/null
unix-recv     .       .        unixport -u       FILE:/dev/null
pipe          .       .        file     -u       FILE:/dev/null
# pty does not seem to honor umask:
#pty           link    .        file     .        PIPE
"


# Tests: option perm with "passive" NAMED group addresses
# Note tests UNIX_RECVFROM_PERM and UNIX_RECV_PERM had chmod() applied after
# bind() due to an error but succeeded. After a correction with  Socat 1.8.0.0
# the perm option is applied as fchown() call which does not affect the FS
# entry on Freebsd (10.3) and OpenIndiana (2021-04), so they fail now
while read addr fileopt addropts feat waitfor diropt; do
if [ -z "$addr" ] || [[ "$addr" == \#* ]]; then continue; fi
# test if passive (listening...) filesystem based addresses implement option perm
ADDR=$(toupper $addr)
ADDR_=${ADDR/-/_}
if [ "$diropt" = "." ]; then diropt=; fi
if [ "$fileopt" = "." ]; then fileopt=; fi
if [ "$addropts" = "." ]; then addropts=; fi
NAME=${ADDR_}_PERM
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%$feat%*|*%ignoreeof%*|*%perm%*|*%listen%*|*%$NAME%*)
TEST="$NAME: $ADDR applies option perm"
# start a socat process with passive/listening file system entry. Check the
# permissions of the FS entry, then terminate the process.
# Test succeeds when FS entry exists and has expected permissions.
if ! eval $NUMCOND; then :; else
tlog="$td/test$N.log"
te0="$td/test$N.0.stderr"
tsock="$td/test$N.sock"
#                                      set -vx
if [ -z "$fileopt" ]; then
    CMD0="$TRACE $SOCAT $opts $diropt $ADDR:$tsock,$addropts,perm=511 FILE:/dev/null,ignoreeof"
else
    CMD0="$TRACE $SOCAT $opts $diropt $ADDR,$fileopt=$tsock,$addropts,perm=511 FILE:/dev/null,ignoreeof"
fi
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"$te0" &
pid0=$!
wait${waitfor} $tsock 1 2>"$tlog"
ERRNOENT=; if ! [ -e "$tsock" ]; then  ERRNOENT=1;  fi
perms=$(fileperms "$tsock")
kill $pid0 2>>"$tlog"
wait
if [ "$ERRNOENT" ]; then
    $PRINTF "${RED}no entry${NORMAL}\n"
    echo "$CMD0 &"
    cat "$te0"
    cat "$tlog"
    failed
elif [ "$perms" != "511" ]; then
    $PRINTF "${RED}perms \"$perms\", expected \"511\" ${NORMAL}\n"
    echo "$CMD0 &"
    cat "$te0"
    failed
else
    $PRINTF "$OK\n"
    ok
fi
                                      set +vx
fi # NUMCOND
 ;;
esac
N=$((N+1))
#
done <<<"
# address     fileopt addropts feat	waitfor direction
create        .       .        file	file     -U
open          .       creat    file	file     .
gopen         .       creat    file	file     .
unix-listen   .       .        unix	unixport .
unix-recvfrom .       .        unix	unixport .
unix-recv     .       .        unix	unixport -u
pipe          .       .        pipe	file     -u
pty           link    .        pty	file	 .
"


# tests: option user with "passive" NAMED group addresses
while read addr fileopt addropts feat waitfor diropt; do
if [ -z "$addr" ] || [[ "$addr" == \#* ]]; then continue; fi
# test if passive (listening...) filesystem based addresses implement option user
ADDR=$(toupper $addr)
ADDR_=${ADDR/-/_}
if [ "$diropt" = "." ]; then diropt=; fi
if [ "$fileopt" = "." ]; then fileopt=; fi
if [ "$addropts" = "." ]; then addropts=; fi
NAME=${ADDR_}_USER
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%$feat%*|*%root%*|*%ignoreeof%*|*%listen%*|*%$NAME%*)
TEST="$NAME: $ADDR applies option user"
# start a socat process with passive/listening file system entry with user option.
# Check the owner of the FS entry, then terminate the process.
# Test succeeds when FS entry exists and has expected owner.
if ! eval $NUMCOND; then :;
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
else
tlog="$td/test$N.log"
te0="$td/test$N.0.stderr"
tsock="$td/test$N.sock"
#                                      set -vx
if [ -z "$fileopt" ]; then
    CMD0="$TRACE $SOCAT $opts $diropt $ADDR:$tsock,$addropts,user=$SUBSTUSER FILE:/dev/null,ignoreeof"
else
    CMD0="$TRACE $SOCAT $opts $diropt $ADDR,$fileopt=$tsock,$addropts,user=$SUBSTUSER FILE:/dev/null,ignoreeof"
fi
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"$te0" &
pid0=$!
wait${waitfor} $tsock 1 2>"$tlog"
ERRNOENT=; if ! [ -e "$tsock" ]; then  ERRNOENT=1;  fi
user=$(fileuser "$tsock")
kill $pid0 2>>"$tlog"
wait
if [ "$ERRNOENT" ]; then
    $PRINTF "${FAILED}(no entry)\n"
    echo "$CMD0 &"
    cat "$te0" >&2
    cat "$tlog" >&2
    failed
elif [ "$user" != "$SUBSTUSER" ]; then
    $PRINTF "${FAILED}(user \"$user\", expected \"$SUBSTUSER\")\n"
    echo "$CMD0 &"
    cat "$te0" >&2
    failed
else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
fi
                                      set +vx
fi # NUMCOND
 ;;
esac
N=$((N+1))
#
done <<<"
# address     fileopt addropts feat	waitfor direction
create        .       .        file	file     -U
open          .       creat    file	file     .
gopen         .       creat    file	file     .
unix-listen   .       .        unix	unixport .
unix-recvfrom .       .        unix	unixport .
unix-recv     .       .        unix	unixport -u
pipe          .       .        pipe	file     -u
pty           link    .        pty	file     .
"


# tests: is "passive" filesystem entry removed at the end? (without fork)
while read addr fileopt addropts feat waitfor diropt crit ADDR2; do
if [ -z "$addr" ] || [[ "$addr" == \#* ]]; then continue; fi
# some passive (listening...) filesystem based addresses did not remove the file
# system entry at the end
ADDR=$(toupper $addr)
ADDR_=${ADDR/-/_}
if [ "$diropt" = "." ]; then diropt=; fi
if [ "$fileopt" = "." ]; then fileopt=; fi
if [ "$addropts" = "." ]; then addropts=; fi
# $ADDR removes the file system entry when the process is terminated
NAME=${ADDR_}_REMOVE
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%feat%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: $ADDR removes socket entry when terminated while waiting for connection"
# start a socat process with listening unix domain socket etc. Terminate the
# process and check if the file system socket entry still exists.
# Test succeeds when entry does not exist.
if ! eval $NUMCOND; then :; else
tlog="$td/test$N.log"
te0="$td/test$N.0.stderr"
tsock="$td/test$N.sock"
if [ -z "$fileopt" ]; then
    CMD0="$TRACE $SOCAT $opts $diropt $ADDR:$tsock,$addropts $ADDR2"
else
    CMD0="$TRACE $SOCAT $opts $diropt $ADDR,$fileopt=$tsock,$addropts $ADDR2"
fi
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"$te0" &
pid0=$!
wait${waitfor} "$crit" $tsock 1 2>"$tlog"
kill $pid0 2>>"$tlog"
rc1=$?
wait >>"$tlog"
if [ $rc1 != 0 ]; then
    $PRINTF "${YELLOW}setup failed${NORMAL}\n"
    echo "$CMD0 &"
    cat "$te0"
    cat "$tlog"
    cant
elif ! [ $crit $tsock ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "$te0"
    cat "$tlog"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))
#
done <<<"
# address     fileopt addropts feat	waitfor direction crit ADDR2
unix-listen   .       .        unix	unixport .        -e   FILE:/dev/null
unix-recvfrom .       .        unix	unixport .        -e   FILE:/dev/null
unix-recv     .       .        unix	unixport -u       -e   FILE:/dev/null
pipe          .       .        pipe	file     -u       -e   FILE:/dev/null
pty           link    .        pty	file     .        -L   PIPE
"


# tests: is "passive" filesystem entry removed at the end? (with fork)
while read addr fileopt addropts proto diropt crit ADDR2; do
if [ -z "$addr" ] || [[ "$addr" == \#* ]]; then continue; fi
# some passive (listening...) filesystem based addresses with fork did not remove
# the file system entry at the end
ADDR=$(toupper $addr)
ADDR_=${ADDR/-/_}
#PROTO=$(toupper $proto)
if [ "$diropt" = "." ]; then diropt=; fi
if [ "$fileopt" = "." ]; then fileopt=; fi
if [ "$addropts" = "." ]; then addropts=; fi
# $ADDR with fork removes the file system entry when the process is terminated
NAME=${ADDR_}_REMOVE_FORK
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%fork%*|*%unix%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: $ADDR with fork removes socket entry when terminated during accept"
# start a socat process with listening unix domain socket etc and option fork.
# Terminate the process and check if the file system socket entry still exists.
# Test succeeds when entry does not exist.
if ! eval $NUMCOND; then :; else
tlog="$td/test$N.log"
te0="$td/test$N.0.stderr"
tsock="$td/test$N.sock"
if [ -z "$fileopt" ]; then
    CMD0="$TRACE $SOCAT $opts $diropt $ADDR:$tsock,fork,$addropts $ADDR2"
else
    CMD0="$TRACE $SOCAT $opts $diropt $ADDR,fork,$fileopt=$tsock,$addropts $ADDR2"
fi
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"$te0" &
pid0=$!
wait${proto} "$crit" $tsock 1 2>"$tlog"
kill $pid0 2>>"$tlog"
rc1=$?
wait
if [ $rc1 != 0 ]; then
    $PRINTF "${YELLOW}setup failed${NORMAL}\n"
    echo "$CMD0 &"
    cat "$te0"
    cat "$tlog"
    cant
elif ! [ $crit $tsock ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "$te0"
    cat "$tlog"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))
#
done <<<"
# address     fileopt addropts waitfor direction crit ADDR2
unix-listen   .       .        unixport .        -e   FILE:/dev/null
unix-recvfrom .       .        unixport .        -e   FILE:/dev/null
"


# bug fix: SYSTEM address child process shut down parents sockets including
# SSL connection under some circumstances.
NAME=SYSTEM_SHUTDOWN
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%system%*|*%openssl%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: SYSTEM address does not shutdown its parents addresses"
# start an OpenSSL echo server using SYSTEM:cat
# start an OpenSSL client that sends data
# when the client receives its data and terminates without error the test succeeded
# in case of the bug the client issues an error like:
# SSL_connect(): error:1408F119:SSL routines:SSL3_GET_RECORD:decryption failed or bad record mac
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,cert=testsrv.pem,verify=0 SYSTEM:cat"
CMD1="$SOCAT $opts - OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,verify=0"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "rc1=$rc1"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    failed
elif ! echo "$da" |diff - "${tf}1" >"$tdiff" 2>&1; then
    $PRINTF "$FAILED\n"
    echo "diff:"
    cat "$tdiff"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    failed
else
    $PRINTF "$OK\n"
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# test if TCP4-LISTEN with empty port arg terminates with error
NAME=TCP4_NOPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%tcp%*|*%tcp4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test if TCP4-LISTEN with empty port arg bails out"
# run socat with TCP4-LISTEN with empty port arg. Check if it terminates
# immediately with return code 1
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
t0rc="$td/test$N.rc"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$SOCAT $opts TCP4-LISTEN: /dev/null"
printf "test $F_n $TEST... " $N
{ $CMD0 >/dev/null 2>"${te}0"; echo $? >"$t0rc"; } & 2>/dev/null
pid0=$!
sleep 1
kill $pid0 2>/dev/null; wait
if [ ! -f "$t0rc" ]; then
    $PRINTF "$FAILED\n"
    echo "no return code of CMD0 stored" >&2
    echo "$CMD0 &"
    cat "${te}0"
    failed
elif ! echo 1 |diff - "$t0rc" >"$tdiff"; then
    $PRINTF "$FAILED\n"
    echo "CMD0 exited with $(cat $t0rc), expected 1"
    echo "$CMD0 &"
    cat "${te}0"
    failed
else
    $PRINTF "$OK\n"
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# tests of various SSL methods; from TLS1.3 this method is not avail in OpenSSL:
OPENSSL_METHODS_OBSOLETE="SSL3 SSL23"
OPENSSL_METHODS_EXPECTED="TLS1 TLS1.1 TLS1.2 DTLS1 DTLS1.2"

# The OPENSSL_METHOD_DTLS1 test hangs sometimes, probably depending on the openssl version.
OPENSSL_VERSION_GOOD=1.0.2 	# this is just a guess.
				# known bad:  1.0.1e
				# known good: 1.0.2j

# test if the obsolete SSL methods can be used with OpenSSL
for method in $OPENSSL_METHODS_OBSOLETE; do

NAME=OPENSSL_METHOD_$method
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%openssl%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test OpenSSL method $method"
# Start a socat process with obsoelete OpenSSL method, it should fail
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! socat -hhh |grep -q "^[[:space:]]*openssl-method[[:space:]]"; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option openssl-method not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$SOCAT $opts OPENSSL-LISTEN:$PORT,$REUSEADDR,openssl-method=$method,cert=testsrv.pem,verify=0 PIPE"
CMD1="$SOCAT $opts - OPENSSL-CONNECT:$LOCALHOST:$PORT,opensslmethod=$method,verify=0"
printf "test $F_n $TEST... " $N
if [ "$method" = DTLS1 -a "$(echo -e "$OPENSSL_VERSION\n1.0.2" |sort |tail -n 1)" = "$OPENSSL_VERSION_GOOD" ]; then
    $PRINTF "${YELLOW}might hang, skipping${NORMAL}\n"
    cant
else
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1 1 2>/dev/null; w0=$? 	# result of waiting for process 0
if [ $w0 -eq 0 ]; then
    echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
fi
echo "$da" |diff - "${tf}1" >"$tdiff" 2>/dev/null
if [ $w0 -eq 0 ] && [ -f "${tf}1" ] && ! [ -s "$tdiff" ]; then
    $PRINTF "${YELLOW}WARN${NORMAL} (obsolete method succeeds)\n"
    ok
else
    $PRINTF "$OK (obsolete method fails)\n"
    cat "$tdiff"
    ok
fi
    if [ "$VERBOSE" ]; then
	echo "  $CMD0"
	echo "  echo \"$da\" |$CMD1"
    fi
fi # !DTLS1 hang
fi # NUMCOND
 ;;
esac
N=$((N+1))

done

# test if the various SSL methods can be used with OpenSSL
for method in $OPENSSL_METHODS_EXPECTED; do

NAME=OPENSSL_METHOD_$method
METHFAM=$(tolower "${method%%[0-9]*}")
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%openssl%*|*%$METHFAM%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test OpenSSL method $method"
# Start a socat process listening with OpenSSL and echoing data,
# using the selected method
# Start a second socat process connecting to the listener using
# the same method, send some data and catch the reply.
# If the reply is identical to the sent data the test succeeded.
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! socat -hhh |grep -q "^[[:space:]]*openssl-method[[:space:]]"; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option openssl-method not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
#if [[ "$method" =~ DTLS* ]]; then
if re_match "$method" '^DTLS.*'; then
    newport udp4
else
    newport tcp4
fi
CMD0="$SOCAT $opts OPENSSL-LISTEN:$PORT,$REUSEADDR,openssl-method=$method,cert=testsrv.pem,verify=0 PIPE"
CMD1="$SOCAT $opts - OPENSSL-CONNECT:$LOCALHOST:$PORT,openssl-method=$method,verify=0"
printf "test $F_n $TEST... " $N
if [ "$method" = DTLS1 -a "$(echo -e "$OPENSSL_VERSION\n1.0.2" |sort |tail -n 1)" = "$OPENSSL_VERSION_GOOD" ]; then
    $PRINTF "${YELLOW}might hang, skipping${NORMAL}\n"
    cant
else
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
#if [[ "$method" =~ DTLS* ]]; then
if re_match "$method" '^DTLS.*'; then
    waitudp4port $PORT 1
else
    waittcp4port $PORT 1
fi
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if echo "$da" |diff - "${tf}1" >"$tdiff"; then 
    $PRINTF "$OK\n"
    ok
    if [ "$VERBOSE" ]; then
	echo "  $CMD0"
	echo "  echo \"$da\" |$CMD1"
    fi
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1"
    cat "${te}1"
    cat "$tdiff"
    failed
    #esac
fi
fi # !DTLS1 hang
fi # NUMCOND
 ;;
esac
N=$((N+1))

done

# test security of option openssl-set-min-proto-version
OPENSSL_LATEST_PROTO_VERSION=$(openssl s_server --help 2>&1 |grep -e -ssl[1-9] -e -tls[1-9] |awk '{print($1);}' |cut -c 2- |tr '[a-z_]' '[A-Z.]' |sort |tail -n 1)
OPENSSL_BEFORELAST_PROTO_VERSION=$(openssl s_server --help 2>&1 |grep -e -ssl[1-9] -e -tls[1-9] |awk '{print($1);}' |cut -c 2- |tr '[a-z_]' '[A-Z.]' |sort |tail -n 2 |head -n 1)

NAME=OPENSSL_MIN_VERSION
case "$TESTS" in
*%$N%*|*%functions%*|*%security%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%openssl%*|*%listen%*|*%$NAME%*)
TEST="$NAME: security of OpenSSL server with openssl-min-proto-version"
if ! eval $NUMCOND; then :;
elif ! testaddrs openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions openssl-min-proto-version); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif ! [ "$OPENSSL_LATEST_PROTO_VERSION" -a "$OPENSSL_BEFORELAST_PROTO_VERSION" -a \
         "$OPENSSL_LATEST_PROTO_VERSION" != "$OPENSSL_BEFORELAST_PROTO_VERSION" ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}cannot determine two available SSL/TLS versions${NORMAL}\n" $N
    cant
else
    gentestcert testsrv
    newport tcp4
    testserversec "$N" "$TEST" "$opts -4" "SSL-L:$PORT,pf=ip4,reuseaddr,$SOCAT_EGD,verify=0,cert=testsrv.crt,key=testsrv.key" "" "openssl-min-proto-version=$OPENSSL_LATEST_PROTO_VERSION" "SSL:$LOCALHOST:$PORT,cafile=testsrv.crt,$SOCAT_EGD,openssl-max-proto-version=$OPENSSL_BEFORELAST_PROTO_VERSION" 4 tcp $PORT -1
fi ;; # NUMCOND, $fets
esac
N=$((N+1))


# Address options fdin and fdout were silently ignored when not applicable
# due to -u or -U option. Now these combinations are caught as errors.
NAME=FDOUT_ERROR
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%$NAME%*)
TEST="$NAME: fdout bails out in write-only context"
# use EXEC in write-only context with option fdout. Expected behaviour: error
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$SOCAT $opts -u /dev/null EXEC:cat,fdout=1"
printf "test $F_n $TEST... " $N
$CMD >/dev/null 2>"${te}"
rc=$?
if [ $rc -eq 1 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD"
    cat "${te}"
    echo "command did not terminate with error!"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# test if failure exit code of SYSTEM invocation causes socat to also exit
# with !=0
NAME=SYSTEM_RC
case "$TESTS" in
*%$N%*|*%functions%*|*%system%*|*%$NAME%*)
TEST="$NAME: promote failure of SYSTEM"
# run socat with SYSTEM:false and check if socat exits with !=0
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
# shut-none makes sure that the child is not killed by parent
CMD0="$TRACE $SOCAT $opts - SYSTEM:false,shut-none"
printf "test $F_n $TEST... " $N
sleep 1 |$CMD0 >/dev/null 2>"${te}0"
rc0=$?
if [ $rc0 -eq 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
else
    $PRINTF "$OK\n"
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# test if failure exit code of EXEC invocation causes socat to also exit
# with !=0
NAME=EXEC_RC
case "$TESTS" in
*%$N%*|*%functions%*|*%exec%*|*%$NAME%*)
TEST="$NAME: promote failure of EXEC"
# run socat with EXEC:false and check if socat exits with !=0
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
# shut-none makes sure that the child is not killed by parent
CMD0="$TRACE $SOCAT $opts - EXEC:false,shut-none"
printf "test $F_n $TEST... " $N
sleep 1 |$CMD0 >/dev/null 2>"${te}0"
rc0=$?
if [ $rc0 -eq 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
else
    $PRINTF "$OK\n"
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# test the so-reuseaddr option
NAME=SO_REUSEADDR
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%tcp%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test the so-reuseaddr option"
# process 0 provides a tcp listening socket with so-reuseaddr;
# process 1 connects to this port; thus the port is connected but no longer
# listening
# process 2 tries to listen on this port with SO_REUSEADDR, will fail if the
# SO_REUSEADDR socket options did not work
# process 3 connects to this port; only if it is successful the test is ok
if ! eval $NUMCOND; then :;
elif ! feat=$(testoptions so-reuseaddr); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
newport tcp4; tp="$PORT"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts TCP4-L:$tp,$REUSEADDR PIPE"
CMD1="$TRACE $SOCAT $opts - TCP4:localhost:$tp"
CMD2="$CMD0"
CMD3="$CMD1"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $tp 1
(echo "$da"; relsleep 3) |$CMD1 >"$tf" 2>"${te}1" &	# this should always work
pid1=$!
relsleep 1
$CMD2 >/dev/null 2>"${te}2" &
pid2=$!
waittcp4port $tp 1
(echo "$da") |$CMD3 >"${tf}3" 2>"${te}3"
rc3=$?
kill $pid0 $pid1 $pid2 2>/dev/null; wait
if ! echo "$da" |diff - "$tf"; then
    $PRINTF "${YELLOW}phase 1 failed${NORMAL}\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cant
elif [ $rc3 -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    echo "$CMD1"
    echo "$CMD2 &"
    echo "$CMD3"
    cat "${te}2" "${te}3"
    failed
elif ! echo "$da" |diff - "${tf}3"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    echo "$CMD1"
    echo "$CMD2 &"
    echo "$CMD3"
    echo "$da" |diff - "${tf}3"
    cant
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}0" "${te}1" "${te}2" "${te}3"; fi
    ok
fi
fi # NUMCOND, SO_REUSEADDR
 ;;
esac
N=$((N+1))


# test the so-reuseport option
NAME=SO_REUSEPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%tcp%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test the so-reuseport option"
# process 0 provides a tcp listening socket with so-reuseport;
# process 1 provides an equivalent tcp listening socket with so-reuseport;
# process 2 connects to this port and transfers data
# process 3 connects to this port and transfers data
# test succeeds when both data transfers work
if ! eval $NUMCOND; then :;
elif ! feat=$(testoptions so-reuseport); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
else
newport tcp4; tp="$PORT"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da2="test$N $(date) $RANDOM"
da3="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts TCP4-L:$tp,$REUSEADDR,so-reuseport PIPE"
CMD1="$CMD0"
CMD2="$TRACE $SOCAT $opts - TCP4:localhost:$tp"
CMD3="$CMD2"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
$CMD1 >/dev/null 2>"${te}1" &
pid1=$!
waittcp4port $tp 1
(echo "$da2") |$CMD2 >"${tf}2" 2>"${te}2"	# this should always work
rc2=$?
(echo "$da3") |$CMD3 >"${tf}3" 2>"${te}3"
rc3=$?
kill $pid0 $pid1 $pid2 2>/dev/null; wait
if ! echo "$da2" |diff - "${tf}2"; then
    $PRINTF "${YELLOW}phase 1 failed${NORMAL}\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD2"
    cat "${te}2"
    cant
elif [ $rc3 -ne 0 ]; then
    $PRINTF "$FAILED:\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD2"
    cat "${te}2"
    echo "$CMD3"
    cat "${te}3"
    failed
elif ! echo "$da2" |diff - "${tf}2"; then
    $PRINTF "$FAILED:\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD2"
    cat "${te}2"
    echo "$CMD3"
    cat "${te}3"
    echo "$da2" |diff - "${tf}2"
    failed
elif ! echo "$da3" |diff - "${tf}3"; then
    $PRINTF "$FAILED:\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD2"
    cat "${te}2"
    echo "$CMD3"
    cat "${te}3"
    echo "$da3" |diff - "${tf}3"
    failed
else
    $PRINTF "$OK\n"
    if [ -n "$debug" ]; then cat "${te}0" "${te}1" "${te}2" "${te}3"; fi
    ok
fi
fi # NUMCOND, SO_REUSEPORT
 ;;
esac
N=$((N+1))


# Programs invoked with EXEC, nofork, and -u or -U had stdin and stdout assignment swapped. 
NAME=EXEC_NOFORK_UNIDIR
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%exec%*|*%$NAME%*)
TEST="$NAME: Programs invoked with EXEC, nofork, and -u or -U had stdin and stdout assignment swapped"
# invoke a simple echo command with EXEC, nofork, and -u
# expected behaviour: output appears on stdout
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -u /dev/null EXEC:\"echo \\\\\\\"\\\"$da\\\"\\\\\\\"\",nofork"
printf "test $F_n $TEST... " $N
eval "$CMD0" >"${tf}0" 2>"${te}0"
rc1=$?
if echo "$da" |diff - "${tf}0" >"$tdiff"; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    cat "$tdiff"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# OpenSSL ECDHE ciphers were introduced in socat 1.7.3.0 but in the same release
# they were broken by a porting effort. This test checks if OpenSSL ECDHE works
# 2019-02: this does no longer work (Ubuntu-18.04)
NAME=OPENSSL_ECDHE
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%openssl%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test OpenSSL ECDHE"
# generate a ECDHE key, start an OpenSSL server, connect with a client and try to
# pass data
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! openssl ciphers |grep -q '\<ECDHE\>'; then
    $PRINTF "test $F_n $TEST... ${YELLOW}openssl: cipher ECDHE not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
#TESTSRV=./testsrvec; gentesteccert $TESTSRV
TESTSRV=./testsrv; gentestcert $TESTSRV
newport tcp4
CMD0="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,cert=$TESTSRV.crt,key=$TESTSRV.pem,verify=0 PIPE"
CMD1="$TRACE $SOCAT $opts - OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cipher=ECDHE-ECDSA-AES256-GCM-SHA384,cafile=$TESTSRV.crt,verify=0"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait 
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "failure symptom: client error" >&2
    echo "server and stderr:" >&2
    echo "$CMD0 &"
    cat "${te}0"
    echo "client and stderr:" >&2
    echo "$CMD1"
    cat "${te}1"
    failed
elif echo "$da" |diff - "${tf}1" >"$tdiff"; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "server and stderr:" >&2
    echo "$CMD1"
    cat "${te}1"
    echo "client and stderr:" >&2
    echo "$CMD0 &"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# option ipv6-join-group "could not be used"
# fixed in 1.7.3.2
NAME=USE_IPV6_JOIN_GROUP
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%ip6%*|*%udp%*|*%udp6%*|*%dgram%*|*%multicast%*|*%$NAME%*)
TEST="$NAME: is option ipv6-join-group used"
# Invoke socat with option ipv6-join-group on UDP6 address.
# Terminate immediately, do not transfer data.
# If socat exits with 0 the test succeeds.
# Up to 1.7.3.1 it failed with "1 option(s) could not be used"
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP6 UDP GOPEN" \
		  "UDP6-RECV GOPEN" \
		  "ipv6-join-group" \
		  "udp6" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport udp6
CMD0="$TRACE $SOCAT $opts -T 0.001 -u UDP6-RECV:$PORT,ipv6-join-group=[ff02::2]:$MCINTERFACE /dev/null"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0"
rc0=$?
if [ $rc0 -eq 0 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# The fix to "Make code async-signal-safe" used internally FD 3 and FD 4.
# Using option fdin=3 did not pass data to executed program.
NAME=DIAG_FDIN
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%exec%*|*%$NAME%*)
TEST="$NAME: test use of fdin=3"
# Use FD 3 explicitly with fdin and test if Socat passes data to executed
# program
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts - SYSTEM:\"cat >&3 <&4\",fdin=4,fdout=3"
printf "test $F_n $TEST... " $N
echo "$da" |$TRACE $SOCAT $opts - SYSTEM:"cat <&3 >&4",fdin=3,fdout=4 >${tf}0 2>"${te}0"
rc0=$?
if [ $rc0 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
elif echo "$da" |diff - ${tf}0 >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    cat "$tdiff"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=SOCAT_OPT_HINT
case "$TESTS" in
*%$N%*|*%functions%*|*%$NAME%*)
TEST="$NAME: check if merging single character options is rejected"
if ! eval $NUMCOND; then :; else
te="$td/test$N.stderr"
CMD0="$TRACE $SOCAT $opts -vx FILE:/dev/null ECHO"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0"
rc0=$?
if [ "$rc0" = "1" ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0" >&2
    failed
fi
fi ;; # NUMCOND
esac
N=$((N+1))


# test for a bug in Socat version 1.7.3.3 where
# termios options of the first address were applied to the second address.
NAME=TERMIOS_PH_ALL
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%pty%*|*%termios%*|*%$NAME%*)
TEST="$NAME: are termios options applied to the correct address"
# add a termios option to the first address, a tty, and have a second address
# with pipe. If no error occurs the termios option was not applied to the pipe,
# thus the test succeeded.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -T 1 STDIO,echo=0 EXEC:cat 2>${te}0"
echo "$CMD0" >$td/test$N.sh
chmod a+x $td/test$N.sh
# EXEC need not work with script (musl libc), so use SYSTEM
CMD1="$TRACE $SOCAT $opts /dev/null SYSTEM:$td/test$N.sh,pty,$PTYOPTS"
printf "test $F_n $TEST... " $N
$CMD1  2>"${te}1"
rc0=$?
if [ $rc0 -eq 0 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    echo "$CMD1"
    cat "${te}1"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Due to a fallback logic before calling getaddrinfo(), intended to allow use
# of service (port) names with SCTP, raw socket addresses where resolved with
# socket type stream, which fails for protocol 6 (TCP)
# Fixed after 1.7.3.3
NAME=IP_SENDTO_6
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%rawip%*|*%rawip4%*|*%$NAME%*)
TEST="$NAME: IP-SENDTO::6 passes getaddrinfo()"
# invoke socat with address IP-SENDTO:*:6; when this does not fail with
# "ai_socktype not supported", the test succeeded
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
CMD0="$TRACE $SOCAT $opts -u /dev/null IP-SENDTO:127.0.0.1:6"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0"
if ! grep -q "ai_socktype not supported" ${te}0; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# test if the multiple EOF messages are fixed
NAME=MULTIPLE_EOF
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%unix%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: multiple EOF messages"
# start two processes, connected via UNIX socket. The listener gets EOF from local address immediately; the second process then sends data. If the listener reports "socket 1 (fd .*) is at EOF" only once, the test succeeded
if ! eval $NUMCOND; then :; else
ts="$td/test$N.sock"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -d -d UNIX-LISTEN:$ts /dev/null"
CMD1="$TRACE $SOCAT $opts -d -d - UNIX-CONNECT:$ts"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waitunixport $ts 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if [ $(grep "socket 2 (fd .*) is at EOF" ${te}0 |wc -l) -eq 1 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test for integer overflow with data transfer block size parameter
NAME=BLKSIZE_INT_OVERFL
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%security%*|*%$NAME%*)
TEST="$NAME: integer overflow with buffer size parameter"
# Use a buffer size that would lead to integer overflow
# Test succeeds when Socat terminates with correct error message
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
dat="$td/test$N.dat"
# calculate the minimal length with integer overflow
case $SIZE_T in
    2) CHKSIZE=32768 ;;
    4) CHKSIZE=2147483648 ;;
    8) CHKSIZE=9223372036854775808 ;;
    16) CHKSIZE=170141183460469231731687303715884105728 ;;
    *) echo "Unsupported SIZE_T=\"$SIZE_T\"" >2 ;;
esac
CMD0="$TRACE $SOCAT $opts -T 1 -b $CHKSIZE /dev/null PIPE"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0"
rc0=$?
if [ $rc0 -eq 0 ]; then
    $PRINTF "$FAILED (rc=$rc0)\n"
    echo "$CMD0"
    cat "${te}0"
    failed
elif [ $rc0 -eq 1 ]; then
    if grep -q "buffer size option (-b) to big" "${te}0"; then
	$PRINTF "$OK\n"
	ok
    else
	$PRINTF "$FAILED (rc=$rc0)\n"
	echo "$CMD0"
	cat "${te}0"
	failed
    fi
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test if unbalanced quoting in Socat addresses is detected
NAME=UNBALANCED_QUOTE
case "$TESTS" in
*%$N%*|*%functions%*|*%syntax%*|*%bugs%*|*%$NAME%*)
TEST="$NAME: Test fix of unbalanced quoting"
# Invoke Socat with an address containing unbalanced quoting. If Socat prints
# a "syntax error" message, the test succeeds
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -u FILE:$td/ab\"cd FILE:/dev/null"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0"
if grep -q -i -e "syntax error" -e "unexpected end" "${te}0"; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0" >&2; fi
    if [ "$debug" ]; then cat ${te} >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Currently (2020) SCTP has not found its way into main distributions
# /etc/services file. A fallback mechanism has been implemented in Socat
# that allows use of TCP service names when service resolution for SCTP failed.
# Furthermore, older getaddrinfo() implementations to not handle SCTP as SOCK_STREAM
# at all, fall back to unspecified socktype then.
NAME=SCTP_SERVICENAME
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%sctp%*|*%$NAME%*)
TEST="$NAME: Service name resolution works with SCTP"
# invoke socat with address SCTP4-CONNECT:$LOCALHOST:http; when this fails with
# "Connection refused", or does not fail at all, the test succeeded
if ! eval $NUMCOND; then :;
elif ! runssctp4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}SCTP4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
CMD0="$TRACE $SOCAT $opts -u /dev/null SCTP4-CONNECT:$LOCALHOST:http"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0"
if [ $? -eq 0 ]; then
    $PRINTF "$OK\n"
    ok
elif grep -q "Connection refused" ${te}0; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the o-direct option on reading
NAME=O_DIRECT
case "$TESTS" in
*%$N%*|*%functions%*|*%engine%*|*%file%*|*%$NAME%*)
TEST="$NAME: echo via file with o-direct"
# Write data to a file and read it with options o-direct (and ignoreeof)
# When the data read is the same as the data written the test succeeded.
if ! eval $NUMCOND; then :;
elif ! testoptions o-direct >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}o-direct not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.file"
to="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
$PRINTF "test $F_n $TEST... " $N
CMD="$TRACE $SOCAT $opts - $tf,o-direct,ignoreeof!!$tf"
echo "$da" |$CMD >"$to" 2>"$te"
rc=$?
if [ $rc -ne 0 ] && grep -q "Invalid argument" "$te" && [ $UNAME = Linux ]; then
    case $(stat -f $tf |grep "Type: [^[:space:]]*" |sed -e 's/.*\(Type: [^[:space:]]*\).*/\1/' |cut -c 7-) in
    #case $(stat -f $tf |grep -o "Type: [^[:space:]]*" |cut -c 7-) in
	ext2/ext3|xfs|reiserfs)
	    $PRINTF "${FAILED}\n"
	    echo "$CMD" >&2
	    cat "$te" >&2
	    failed ;;
	*) $PRINTF "${YELLOW}unsupported file system${NORMAL}\n"
	    if [ "$VERBOSE" ]; then echo "$CMD"; fi
	    if [ "$DEBUG" ];   then cat "${te}" >&2; fi
	    cant ;;
    esac
elif [ $rc -ne 0 ]; then
    $PRINTF "${FAILED}:\n"
    echo "$CMD" >&2
    cat "$te" >&2
    failed
elif ! echo "$da" |diff - "$to" >$tdiff; then
    $PRINTF "${FAILED}\n"
    echo "$CMD" >&2
    cat "$te" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    ok
fi # command ok
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# test if option unlink-close removes the bind socket file
NAME=UNIX_SENDTO_UNLINK
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%unix%*|*%$NAME%*)
TEST="$NAME: Option unlink-close with UNIX sendto socket"
# Have a recv socket with option unlink-close=0
# and a sendto socket with option unlink-close=1
# Expected beavior: the recv socket is kept, the
# sendto/bind socket is removed
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
uns="$td/test$N.server"
unc="$td/test$N.client"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -u UNIX-RECV:$uns,unlink-close=0 GOPEN:$tf"
CMD1="$TRACE $SOCAT $opts - UNIX-SENDTO:$uns,bind=$unc,unlink-close=1"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waitunixport $uns 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if test -S $uns && ! test -S $unc; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    ls -ld $uns $unc
    cat "${te}0"
    cat "${te}1"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# test if option unlink-close removes the bind socket file
NAME=UNIX_CONNECT_UNLINK
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%unix%*|*%listen%*|*%$NAME%*)
TEST="$NAME: Option unlink-close with UNIX connect socket"
# Have a listen socket with option unlink-close=0
# and a connect socket with option unlink-close=1
# Expected beavior: the listen socket entry is kept, the
# connect/bind socket is removed
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
uns="$td/test$N.server"
unc="$td/test$N.client"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -u UNIX-LISTEN:$uns,unlink-close=0 GOPEN:$tf"
CMD1="$TRACE $SOCAT $opts - UNIX-CONNECT:$uns,bind=$unc,unlink-close=1"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waitunixport $uns 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if test -S $uns && ! test -S $unc; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    ls -ld $uns $unc
    cat "${te}0"
    cat "${te}1"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# test the DTLS client feature
NAME=OPENSSL_DTLS_CLIENT
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%dtls%*|*%udp%*|*%udp4%*|*%ip4%*|*%$NAME%*)
TEST="$NAME: OpenSSL DTLS client"
# Run openssl s_server in DTLS mode, wrapped into a simple Socat echoing command.
# Start a Socat DTLS client, send data to server and check if reply is received.
if ! eval $NUMCOND; then :;
elif ! a=$(testfeats ip4 udp openssl); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $a not available${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs openssl-dtls-client); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $a not available${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
elif ! type openssl >/dev/null 2>&1; then
    $PRINTF "test $F_n $TEST... ${YELLOW}openssl executable not found${NORMAL}\n" $N
    cant
elif init_openssl_s_server; re_match "$method" '^DTLS.*' && [ -z "$OPENSSL_S_SERVER_DTLS" ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}DTLS not available in s_server${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
#set -vx
da="test$N $(date) $RANDOM"
init_openssl_s_server
newport udp4
CMD1="$TRACE openssl s_server $OPENSSL_S_SERVER_4 $OPENSSL_S_SERVER_DTLS -accept $PORT -quiet $OPENSSL_S_SERVER_NO_IGN_EOF -cert testsrv.pem"
CMD="$TRACE $SOCAT $opts -T $(reltime 3) - OPENSSL-DTLS-CLIENT:$LOCALHOST:$PORT,pf=ip4,verify=0,$SOCAT_EGD,so-rcvtimeo=2"
printf "test $F_n $TEST... " $N
( relsleep 2; echo "$da"; relsleep 1 ) |$CMD1 2>"${te}1" &
pid1=$!	# background process id
waitudp4port $PORT
$CMD >$tf 2>"$te"
kill $pid1 2>/dev/null; wait 2>/dev/null
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD"
    cat "$te"
    cat "$tdiff"
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "$te"; fi
   ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))

# test the DTLS server feature
NAME=OPENSSL_DTLS_SERVER
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%dtls%*|*%udp%*|*%udp4%*|*%ip4%*|*%socket%*|*%$NAME%*)
TEST="$NAME: OpenSSL DTLS server"
# Run a socat OpenSSL DTLS server with echo function
# Start an OpenSSL s_client, send data and check if repley is received.
if ! eval $NUMCOND; then :;
elif ! a=$(testfeats ip4 udp openssl) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$a not available${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs openssl-dtls-server); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$a not available${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
elif ! type openssl >/dev/null 2>&1; then
    $PRINTF "test $F_n $TEST... ${YELLOW}openssl executable not found${NORMAL}\n" $N
    cant
elif init_openssl_s_client; re_match "$method" '^DTLS.*' && [ -z "$OPENSSL_S_CLIENT_DTLS" ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}DTLS not available in s_client${NORMAL}\n" $N
    cant
elif re_match "$(openssl version |awk '{print($2);}')" '^0\.9\.8[a-ce]'; then
    # also on NetBSD-4 with openssl-0.9.8e
    $PRINTF "test $F_n $TEST... ${YELLOW}openssl s_client might hang${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
init_openssl_s_client
newport udp4
CMD1="$TRACE $SOCAT $opts OPENSSL-DTLS-SERVER:$PORT,pf=ip4,$REUSEADDR,cert=testsrv.crt,key=testsrv.key,verify=0 PIPE"
CMD="openssl s_client $OPENSSL_S_CLIENT_4 -host $LOCALHOST -port $PORT $OPENSSL_S_CLIENT_DTLS"
printf "test $F_n $TEST... " $N
$CMD1 >/dev/null 2>"${te}1" &
pid1=$!
waitudp4port $PORT 1
( echo "$da"; sleep 0.1 ) |$CMD 2>"$te" |grep "$da" >"$tf"
rc=$?
kill $pid1 2>/dev/null; wait
if echo "$da" |diff - $tf >"$tdiff"; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD"
    cat "$te"
    cat "$tdiff"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=OPENSSL_SERVERALTAUTH
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL server authentication with SubjectAltName (hostname)"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestaltcert testalt
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,$SOCAT_EGD,cert=testalt.crt,key=testalt.key,verify=0 pipe"
CMD1="$TRACE $SOCAT $opts - OPENSSL:$LOCALHOST:$PORT,pf=ip4,verify=1,cafile=testalt.crt,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
eval "$CMD0 2>\"${te}0\" &"
pid=$!	# background process id
waittcp4port $PORT
echo "$da" |$CMD1 >$tf 2>"${te}1"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    cat "$tdiff" >&2
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSL_SERVERALTIP4AUTH
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL server authentication with SubjectAltName (IPv4 address)"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 openssl >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestaltcert testalt
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,$REUSEADDR,pf=ip4,$SOCAT_EGD,cert=testalt.crt,key=testalt.key,verify=0 pipe"
CMD1="$TRACE $SOCAT $opts - OPENSSL:127.0.0.1:$PORT,verify=1,cafile=testalt.crt,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
eval "$CMD0 2>\"${te}0\" &"
pid=$!	# background process id
waittcp4port $PORT
echo "$da" |$CMD1 >$tf 2>"${te}1"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    cat "$tdiff" >&2
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))

NAME=OPENSSL_SERVERALTIP6AUTH
case "$TESTS" in
*%$N%*|*%functions%*|*%openssl%*|*%tcp%*|*%tcp6%*|*%ip6%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL server authentication with SubjectAltName (IPv6 address)"
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip6 openssl >/dev/null || ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv6 not available${NORMAL}\n" $N
    cant
else
gentestaltcert testalt
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp6
CMD0="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip6,$REUSEADDR,$SOCAT_EGD,cert=testalt.crt,key=testalt.key,verify=0 pipe"
CMD1="$TRACE $SOCAT $opts - OPENSSL:[::1]:$PORT,verify=1,cafile=testalt.crt,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
eval "$CMD0 2>\"${te}0\" &"
pid=$!	# background process id
waittcp6port $PORT
echo "$da" |$CMD1 >$tf 2>"${te}1"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    cat "$tdiff" >&2
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# Test the -r and -R options
NAME=OPTION_RAW_DUMP
case "$TESTS" in
*%$N%*|*%functions%*|*%option%*|*%$NAME%*)
TEST="$NAME: raw dump of transferred data"
# Start Socat transferring data from left named pipe to right and from right
# pipe to left, use options -r and -R, and check if dump files contain correct
# data
if ! eval $NUMCOND; then :;
elif [ $($SOCAT -h |grep -e ' -[rR] ' |wc -l) -lt 2 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Options -r, -R not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tp1="$td/test$N.pipe1"
tp2="$td/test$N.pipe2"
tr1="$td/test$N.raw1"
tr2="$td/test$N.raw2"
tdiff1="$td/test$N.diff1"
tdiff2="$td/test$N.diff2"
da1="test$N $(date) $RANDOM"
da2="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -r $tr1 -R $tr2 PIPE:$tp1!!/dev/null PIPE:$tp2!!/dev/null"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waitfile $tp1 1
echo "$da1" >$tp1
waitfile $tp2 1
echo "$da2" >$tp2
sleep 1
kill $pid0 2>/dev/null; wait
if ! echo "$da1" |diff - $tr1 >$tdiff1 || ! echo "$da2" |diff - $tr2 >$tdiff2; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "Left-to-right:" >&2
    cat $tdiff1 >&2
    echo "Right-to-left:" >&2
    cat $tdiff2 >&2
    failed
else
    $PRINTF "$OK\n"
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the OpenSSL SNI feature
NAME=OPENSSL_SNI
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%openssl%*|*%internet%*|*%listen%*|*%$NAME%*)
TEST="$NAME: Test the OpenSSL SNI feature"
# Connect to a server that is known to use SNI. Use an SNI name, not the
# certifications default name. When the TLS connection is established
# the test succeeded.
SNISERVER=badssl.com
if ! eval $NUMCOND; then :;
elif ! testaddrs openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions openssl-snihost); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ -z "$INTERNET" ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}use test.sh option --internet${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts FILE:/dev/null OPENSSL-CONNECT:$SNISERVER:443,pf=ip4"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0"
rc0=$?
if [ $rc0 -eq 0 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0" >&2
    cat "${te}0" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the openssl-no-sni option
NAME=OPENSSL_NO_SNI
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%openssl%*|*%internet%*|*%listen%*|*%$NAME%*)
TEST="$NAME: Test the openssl-no-sni option"
# Connect to a server that is known to use SNI. Use an SNI name, not the
# certifications default name, and use option openssl-no-sni.
# When the TLS connection failed the test succeeded.
# Please note that this test is only relevant when test OPENSSL_SNI succeeded.
SNISERVER=badssl.com
if ! eval $NUMCOND; then :;
elif ! testaddrs openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions openssl-no-sni); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif [ -z "$INTERNET" ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}use test.sh option --internet${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts FILE:/dev/null OPENSSL-CONNECT:$SNISERVER:443,openssl-no-sni"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0"
rc0=$?
if [ $rc0 -ne 0 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0" >&2
    cat "${te}0" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the accept-timeout (listen-timeout) address option
NAME=ACCEPTTIMEOUT
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%tcp%*|*%listen%*|*%timeout%*|*%$NAME%*)
TEST="$NAME: test the accept-timeout option"
if ! eval $NUMCOND; then :;
elif ! feat=$(testaddrs tcp); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
elif ! feat=$(testoptions accept-timeout); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(echo "$feat"| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
    cant
else
# Just start a process with accept-timeout 1s and check if it still runs 2s later
# but before this, we test if the process waits at all
te1="$td/test$N.stderr1"
tk1="$td/test$N.kill1"
te2="$td/test$N.stderr2"
tk2="$td/test$N.kill2"
$PRINTF "test $F_n $TEST... " $N
# First, try to make socat hang and see if it can be killed
newport tcp4
CMD1="$TRACE $SOCAT $opts TCP-LISTEN:$PORT,reuseaddr PIPE"
$CMD1 >"$te1" 2>&1 </dev/null &
pid1=$!
relsleep 1
if ! kill $pid1 2>"$tk1"; then
    $PRINTF "${YELLOW}does not hang${NORMAL}\n"
    echo $CMD1 >&2
    cat "$te1" >&2
    cat "$tk1" >&2
    cant
else
# Second, set accept-timeout and see if socat exits before kill
CMD2="$TRACE $SOCAT $opts TCP-LISTEN:$PORT,reuseaddr,accept-timeout=$(reltime 1) PIPE"
$CMD2 >"$te2" 2>&1 </dev/null &
pid2=$!
relsleep 2
if kill $pid2 2>"$tk2"; then
    $PRINTF "$FAILED\n"
    echo "$CMD2" >&2
    cat "$te2" >&2
    cat "$tk2" >&2
    failed
else
    $PRINTF "$OK\n"
    ok
fi
fi
wait
fi ;; # testaddrs, NUMCOND
esac
N=$((N+1))


# Test the modified UDP-DATAGRAM address: Now it ignores peerport by default
NAME=UDP_DATAGRAM_PEERPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%udp%*|*%socket%*|*%$NAME%*)
TEST="$NAME: test UDP-DATAGRAM ignoring peerport"
# A UDP-DATAGRAM address bound to PORT has defined peer on PORT+1
# From another Socat instance we send a packet to PORT but with source port
# PORT+2. The first instance should accept the packet
if ! eval $NUMCOND; then :
elif [ $(echo $E "$SOCAT_VERSION\n1.7.3.4" |sort -n |tail -n 1) = 1.7.3.4 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Only with Socat 1.7.4.0 or higher${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport udp4; PORT1=$PORT
newport udp4; PORT2=$PORT
newport udp4; PORT3=$PORT
CMD0="$TRACE $SOCAT $opts -u UDP-DATAGRAM:$LOCALHOST:$PORT2,bind=:$PORT1 -"
CMD1="$TRACE $SOCAT $opts -u - UDP-DATAGRAM:$LOCALHOST:$PORT1,bind=:$PORT3"
printf "test $F_n $TEST... " $N
$CMD0 >${tf}0 2>"${te}0" &
pid0=$!
waitudp4port $PORT1 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
sleep 0.1
kill $pid0 2>/dev/null; wait
if [ -f ${tf}0 ] && echo "$da" |diff - ${tf}0 >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    cat "${tdiff}" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the proxy-authorization-file option
NAME=PROXYAUTHFILE
case "$TESTS" in
*%$N%*|*%functions%*|*%proxyconnect%*|*%proxy%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: proxy-authorization-file option"
if ! eval $NUMCOND; then :;
elif ! testfeats proxy >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}PROXY not available${NORMAL}\n" $N
    cant
elif ! testfeats listen tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
elif ! testoptions proxy-authorization-file >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option proxy-authorization-file not available${NORMAL}\n" $N
    cant
else
ta="$td/test$N.auth"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
newport tcp4
CMD0="{ echo -e \"HTTP/1.0 200 OK\\n\"; sleep 2; } |$TRACE $SOCAT $opts - TCP4-L:$PORT,$REUSEADDR,crlf"
CMD1="$TRACE $SOCAT $opts FILE:/dev/null PROXY-CONNECT:$LOCALHOST:127.0.0.1:1000,pf=ip4,proxyport=$PORT,proxy-authorization-file=$ta"
printf "test $F_n $TEST... " $N
echo "user:s3cr3t" >$ta
eval "$CMD0 >${tf}0 2>${te}0 &"
pid0=$!	# background process id
waittcp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null
wait $pid0
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    cat "${tf}0" >&2
    failed
elif ! grep -q '^Proxy-authorization: Basic dXNlcjpzM2NyM3QK$' ${tf}0; then
    $PRINTF "$FAILED:\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    cat "${tf}0" >&2
    echo "Authorization string not in client request" >&2
    failed
else
   $PRINTF "$OK\n"
   if [ -n "$debug" ]; then cat "${te}1" "${te}2"; fi
   ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# Test communication via vsock loopback socket
NAME=VSOCK_ECHO
case "$TESTS" in
*%$N%*|*%functions%*|*%vsock%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test communication via VSOCK loopback socket"
# Start a listening echo server
# Connect with a client, send data and compare reply with original data
if ! eval $NUMCOND; then :;
elif ! fea=$(testfeats VSOCK); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$fea not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
#newport vsock 	# nope
CMD0="$TRACE $SOCAT $opts VSOCK-LISTEN:$PORT PIPE"
CMD1="$TRACE $SOCAT $opts - VSOCK-CONNECT:1:$PORT"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
sleep 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if [ $rc1 -ne 0 ] &&  [ "$UNAME" != Linux ]; then
    $PRINTF "${YELLOW}works only on Linux?${NORMAL}\n" $N
    cant
elif [ $rc1 -ne 0 ] && [ "$UNAME" = Linux ] && ! re_match "$UNAME_R" '^[6-9]\..*' && ! re_match "$UNAME_R" '^5\.[6-]\..*' && ! re_match "$UNAME_R" '^5\.[1-9][0-9].*'; then
    $PRINTF "${YELLOW}works only on Linux from 5.6${NORMAL}\n" $N
    cant
elif grep -q "No such device" "${te}1"; then
    $PRINTF "${YELLOW}Loopback does not work${NORMAL}\n" $N
    cant
elif [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
elif echo "$da" |diff - ${tf}1 >${tdiff}$N; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# File transfer with OpenSSL stream connection was incomplete
# Test file transfer from client to server
NAME=OPENSSL_STREAM_TO_SERVER
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%openssl%*|*%tcp%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL stream from client to server"
# Start a unidirectional OpenSSL server and stream receiver
# Start a unidirectional OpenSSL client that connects to the server and sends
# data
# Test succeeded when the data received and stored by server is the same as
# sent by the client
if ! eval $NUMCOND; then :;
elif ! a=$(testfeats ip4 tcp openssl); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$a not available${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs openssl-listen openssl-connect); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$a not available${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
ti="$td/test$N.datain"
to="$td/test$N.dataout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts -u OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,cert=testsrv.pem,verify=0 CREAT:$to"
CMD1="$TRACE $SOCAT $opts -u OPEN:$ti OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cafile=testsrv.crt"
printf "test $F_n $TEST... " $N
i=0; while [ $i -lt 100000 ]; do printf "%9u %9u %9u %9u %9u %9u %9u %9u %9u %9u\n" $i $i $i $i $i $i $i $i $i $i; let i+=100; done >$ti
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
relsleep 1
kill $pid0 2>/dev/null; wait
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
elif diff $ti $to >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    echo "diff:" >&2
    head -n 2 $tdiff >&2
    echo ... >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# File transfer with OpenSSL stream connection was incomplete
# Test file transfer from server to client
NAME=OPENSSL_STREAM_TO_CLIENT
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%openssl%*|*%tcp%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL stream from server to client"
# Start a unidirectional OpenSSL server and stream sender
# Start a unidirectional OpenSSL client that connects to the server and receives
# data
# Test succeeded when the data received and stored by client is the same as
# sent by the server
if ! eval $NUMCOND; then :;
elif ! a=$(testfeats ip4 tcp openssl); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$a not available${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs openssl-listen openssl-connect); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$a not available${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
ti="$td/test$N.datain"
to="$td/test$N.dataout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts -U OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,cert=testsrv.pem,verify=0 OPEN:$ti"
CMD1="$TRACE $SOCAT $opts -u OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,cafile=testsrv.crt CREAT:$to"
printf "test $F_n $TEST... " $N
i=0; while [ $i -lt 100000 ]; do printf "%9u %9u %9u %9u %9u %9u %9u %9u %9u %9u\n" $i $i $i $i $i $i $i $i $i $i; let i+=100; done >$ti
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
relsleep 1
kill $pid0 2>/dev/null; wait
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
elif diff $ti $to >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    echo "diff:" >&2
    head -n 2 $tdiff >&2
    echo ... >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test file transfer from client to server using DTLS
NAME=OPENSSL_DTLS_TO_SERVER
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%openssl%*|*%dtls%*|*%udp%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL DTLS transfer from client to server"
# Start a unidirectional OpenSSL DTLS server/receiver
# Start a unidirectional OpenSSL DTLS client that connects to the server and
# sends data
# Test succeeded when the data received and stored by server is the same as
# sent by the client
if ! eval $NUMCOND; then :;
elif ! a=$(testfeats ip4 udp openssl); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$a not available${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs openssl-dtls-listen openssl-dtls-connect); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$a not available${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
elif re_match "$(openssl version |awk '{print($2);}')" '^0.9.8[a-ce]'; then
    # also on NetBSD-4 with openssl-0.9.8e
    $PRINTF "test $F_n $TEST... ${YELLOW}openssl s_client might hang${NORMAL}\n" $N
    cant
else
gentestcert testsrv
ti="$td/test$N.datain"
to="$td/test$N.dataout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport udp4
CMD0="$TRACE $SOCAT $opts -u OPENSSL-DTLS-LISTEN:$PORT,pf=ip4,cert=testsrv.pem,verify=0 CREAT:$to"
CMD1="$TRACE $SOCAT $opts -u OPEN:$ti OPENSSL-DTLS-CONNECT:$LOCALHOST:$PORT,pf=ip4,cafile=testsrv.crt"
printf "test $F_n $TEST... " $N
i=0; while [ $i -lt $((2*8192)) ]; do printf "%9u %9u %9u %9u %9u %9u %9u %9u %9u %9u\n" $i $i $i $i $i $i $i $i $i $i; let i+=100; done >$ti
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waitudp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
relsleep 1
kill $pid0 2>/dev/null; wait
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
elif diff $ti $to >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    echo "diff:" >&2
    head -n 2 $tdiff >&2
    echo ... >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test file transfer from server to client using DTLS
NAME=OPENSSL_DTLS_TO_CLIENT
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%openssl%*|*%dtls%*|*%udp%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: OpenSSL DTLS transfer from server to client"
# Start a unidirectional OpenSSL DTLS server/sender
# Start a unidirectional OpenSSL DTLS client that connects to the server and
# receives data
# Test succeeded when the data received and stored by client is the same as
# sent by the server
if ! eval $NUMCOND; then :;
elif ! a=$(testfeats ip4 udp openssl); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$a not available${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs openssl-dtls-listen openssl-dtls-connect); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$a not available${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
elif re_match "$(openssl  version |awk '{print($2);}')" '^0.9.8[a-ce]'; then
    # also on NetBSD-4 with openssl-0.9.8e
    $PRINTF "test $F_n $TEST... ${YELLOW}openssl s_client might hang${NORMAL}\n" $N
    cant
else
gentestcert testsrv
ti="$td/test$N.datain"
to="$td/test$N.dataout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport udp4
CMD0="$TRACE $SOCAT $opts -U OPENSSL-DTLS-LISTEN:$PORT,pf=ip4,cert=testsrv.pem,verify=0 OPEN:$ti"
CMD1="$TRACE $SOCAT $opts -u OPENSSL-DTLS-CONNECT:$LOCALHOST:$PORT,pf=ip4,cafile=testsrv.crt CREAT:$to"
printf "test $F_n $TEST... " $N
i=0; while [ $i -lt $((2*8192)) ]; do printf "%9u %9u %9u %9u %9u %9u %9u %9u %9u %9u\n" $i $i $i $i $i $i $i $i $i $i; let i+=100; done >$ti
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waitudp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
relsleep 1
kill $pid0 2>/dev/null; wait
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
elif diff $ti $to >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    echo "diff:" >&2
    head -n 2 $tdiff >&2
    echo ... >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if the problem with overlapping internal parameters of sockets and
# openssl are fixed
NAME=OPENSSL_PARA_OVERLAP
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%ip4%*|*%tcp%*|*%tcp4%*|*%openssl%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test diverse of socket,openssl params"
# That bug had not many effects; the simplest to use is possible SIGSEGV on
# close when option accept-timeout with fractional seconds was applied
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! type openssl >/dev/null 2>&1; then
    $PRINTF "test $F_n $TEST... ${YELLOW}openssl executable not available${NORMAL}\n" $N
    cant
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
trc0="$td/test$N.rc0"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts OPENSSL-LISTEN:$PORT,pf=ip4,$REUSEADDR,accept-timeout=4.5,$SOCAT_EGD,cert=testsrv.crt,key=testsrv.key,verify=0 PIPE"
CMD1="$TRACE $SOCAT $opts /dev/null OPENSSL-CONNECT:$LOCALHOST:$PORT,pf=ip4,verify=0,$SOCAT_EGD"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" || echo $? >$trc0 &
pid0=$!
waittcp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
sleep 0.5
kill $pid0 2>/dev/null; wait
if [ $rc1 -ne 0 ]; then
    $PRINTF "$CANT\n"
    cant
elif [ ! -e $trc0 ]; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Bug fix, OpenSSL server could be crashed by client cert with IPv6 address in SubjectAltname
NAME=OPENSSL_CLIENT_IP6_CN
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%openssl%*|*%ip6%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: Test if OpenSSL server may be crashed by client cert with IPv6 address"
# Socat 1.7.4.1 had a bug that caused OpenSSL server to crash with SIGSEGV when
# it checked a client certificate containing IPv6 address in SubjectAltName and
# no openssl-commonname option was given
if ! eval $NUMCOND; then :;
elif ! testfeats openssl >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
    cant
elif ! testfeats tcp ip4 >/dev/null || ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}TCP/IPv4 not available${NORMAL}\n" $N
    cant
elif ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Cannot generate cert with IPv6 address${NORMAL}\n" $N
    cant
else
gentestcert testsrv
gentestaltcert testalt
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts -u OPENSSL-LISTEN:$PORT,pf=ip4,reuseaddr,cert=./testsrv.pem,cafile=./testalt.crt -"
CMD1="$TRACE $SOCAT $opts -u - OPENSSL-CONNECT:localhost:$PORT,pf=ip4,cafile=testsrv.crt,cert=testalt.pem,verify=0"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null >"${tf}0" 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
echo "$da" |$CMD1 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if [ $rc1 -eq 0 ] && echo "$da" |diff - "${tf}0" >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if unknown service specs are handled properly
NAME=BAD_SERVICE
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%tcp%*|*%socket%*|*%$NAME%*)
TEST="$NAME: test if unknown service specs are handled properly"
# Try to resolve an unspecified TCP service "
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT $opts - TCP:$LOCALHOST:zyxw"
printf "test $F_n $TEST... " $N
$CMD >/dev/null 2>"${te}" &
pid=$!
sleep 1
kill -9 $pid 2>/dev/null;
rc=$? 	# did process still exist?
if [ $rc -ne 0 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	echo "$CMD &" >&2
    fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD &" >&2
    cat "${te}" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if the user option with abstract UNIX domain socket is not applied to
# file "" (empty name)
NAME=ABSTRACT_USER
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%unix%*|*%abstract%*|*%listen%*|*%$NAME%*)
TEST="$NAME: Is the fs related user option on ABSTRACT socket applied to FD"
# Apply the user option to an abstract socket; check if this produces an error.
# No error should occur
if ! eval $NUMCOND; then :;
elif [ "$UNAME" != Linux ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}only on Linux${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT ABSTRACT-LISTEN:temp,accept-timeout=0.1,user=$USER FILE:/dev/null"
printf "test $F_n $TEST... " $N
$CMD >/dev/null 2>"${te}"
echo "$da" |$CMD >"${tf}1" 2>"${te}1"
rc=$?
if [ $rc -eq 0 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	echo "$CMD" >&2
    fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD" >&2
    cat "${te}" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if option -R does not "sniff" left-to-right traffic
NAME=SNIFF_RIGHT_TO_LEFT
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%$NAME%*)
TEST="$NAME: test if option -R does not "sniff" left-to-right traffic"
# Use option -R, check if left-to-right traffic is not in output file
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
ts="$td/test$N.sniffed"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT $opts -R $ts - /dev/null"
printf "test $F_n $TEST... " $N
echo "$da" |$CMD >"${tf}" 2>"${te}"
rc=$?
if [ ! -f "$ts" ]; then
    $PRINTF "$CANT\n"
    if [ "$VERBOSE" ]; then
	echo "$CMD" >&2
	cat "${te}" >&2
    fi
    cant
elif [ ! -s "$ts" ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	echo "$CMD" >&2
    fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD &" >&2
    cat "${te}" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Socats access to different types of file system entries using various kinds
# of addresses fails in a couple of useless combinations. These failures have
# to print an error message and exit with return code 1.
# Up to version 1.7.4.2 this desired behaviour was found for most combinations,
# however some fix in 1.7.4.3 degraded the overall result.
# This group of tests checks all known compinations.
while read entry method; do
if [ -z "$entry" ] || [[ "$entry" == \#* ]]; then continue; fi
NAME=$(toupper $method)_TO_$(toupper $entry)
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%unix%*|*%listen%*|*%$NAME%*)
#set -vx
TEST="$NAME: Failure handling on $method access to $entry"
# Create some kind of system entry and try to access it with some improper
# address. Check if Socat returns with rc 1 and prints an error message
if ! eval $NUMCOND; then :; else
ts="$td/test$N.socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"

printf "test $F_n $TEST... " $N
# create an invalid or non-matching UNIX socket
case "$entry" in
    missing)     pid0=; rm -f $ts ;;
    denied)      pid0=; rm -f $ts; touch $ts; chmod 000 $ts ;;
    directory)   pid0=; mkdir -p $ts ;;
    orphaned)    pid0= 	# the remainder of a UNIX socket in FS
		 SOCAT_MAIN_WAIT= $SOCAT $opts UNIX-LISTEN:$ts,unlink-close=0 /dev/null >${tf}0 2>${te}0 &
		 waitunixport $ts 1
		 SOCAT_MAIN_WAIT= $SOCAT $opts /dev/null UNIX-CONNECT:$ts >>${tf}0 2>>${te}0
		 ;;
    file)        pid0=; rm -f $ts; touch $ts ;;
    stream)      CMD0="$SOCAT $opts UNIX-LISTEN:$ts /dev/null"
		 SOCAT_MAIN_WAIT= $CMD0 >${tf}0 2>${te}0 &
		 pid0=$! ;;
    dgram)       CMD0="$SOCAT $opts -u UNIX-RECV:$ts /dev/null"
		 SOCAT_MAIN_WAIT= $CMD0 >${tf}0 2>${te}0 &
		 pid0=$! ;;
    seqpacket)   CMD0="$SOCAT $opts UNIX-LISTEN:$ts,socktype=$SOCK_SEQPACKET /dev/null"
		 SOCAT_MAIN_WAIT= $CMD0 >${tf}0 2>${te}0 &
		 pid0=$! ;;
esac
[ "$pid0" ] && waitunixport $ts 1
# try to access this socket
case "$method" in
    connect)   CMD1="$TRACE $SOCAT $opts -u - UNIX-CONNECT:$ts" ;;
    send)      CMD1="$TRACE $SOCAT $opts -u - UNIX-SEND:$ts" ;;
    sendto)    CMD1="$TRACE $SOCAT $opts -u - UNIX-SENDTO:$ts" ;;
    seqpacket) CMD1="$TRACE $SOCAT $opts -u - UNIX-CONNECT:$ts,socktype=$SOCK_SEQPACKET" ;;
    unix)      CMD1="$TRACE $SOCAT $opts -u - UNIX-CLIENT:$ts" ;;
    gopen)     CMD1="$TRACE $SOCAT $opts -u - GOPEN:$ts" ;;
esac
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
[ "$pid0" ] && { kill $pid0 2>/dev/null; wait; }
if [ $rc1 != 1 ]; then
    $PRINTF "$FAILED (bad return code $rc1)\n"
    if [ "$pid0" ]; then
	echo "$CMD0 &" >&2
	cat "${te}0" >&2
    fi
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
elif nerr=$(grep ' E ' "${te}1" |wc -l); test "$nerr" -ne 1; then
    $PRINTF "$FAILED ($nerr error message(s) instead of 1)\n"
    if [ "$pid0" ]; then
	echo "$CMD0 &" >&2
	cat "${te}0" >&2
    fi
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	if [ "$pid0" ]; then echo "$CMD0 &" >&2; fi
	echo "$CMD1" >&2
    fi
    ok
fi
set +vx
fi # NUMCOND
 ;;
esac
N=$((N+1))
done <<<"
missing     connect
denied      connect
directory   connect
orphaned    connect
file        connect
dgram       connect
seqpacket   connect
missing     send
denied      send
directory   send
orphaned    send
file        send
stream      send
seqpacket   send
missing     sendto
denied      sendto
directory   sendto
orphaned    sendto
file        sendto
stream      sendto
seqpacket   sendto
missing     seqpacket
denied      seqpacket
directory   seqpacket
orphaned    seqpacket
file        seqpacket
stream      seqpacket
dgram       seqpacket
missing     unix
denied      unix
directory   unix
file        unix
orphaned    unix
denied      gopen
directory   gopen
orphaned    gopen
"


# Test TCP with options connect-timeout and retry.
# Up to 1.7.4.3 this terminated immediately on connection refused
NAME=TCP_TIMEOUT_RETRY
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%tcp%*|*%socket%*|*%listen%*|*%retry%*|*%$NAME%*)
TEST="$NAME: TCP with options connect-timeout and retry"
# In background run a delayed echo server
# In foreground start TCP with connect-timeout and retry. On first attempt the
# server is not listening; when socat makes a second attempt that succeeds, the
# bug is absent and the test succeeded.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="sleep 1 && $TRACE $SOCAT $opts TCP4-L:$PORT,reuseaddr PIPE"
CMD1="$TRACE $SOCAT $opts - TCP4:$LOCALHOST:$PORT,connect-timeout=2,retry=1,interval=2"
printf "test $F_n $TEST... " $N
eval "$CMD0" >/dev/null 2>"${te}0" &
pid0=$!
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
elif echo "$da" |diff - "${tf}1" >$tdiff; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	echo "$CMD0 &" >&2
	echo "$CMD1" >&2
    fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &" >&2
    cat "${te}0" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if the rawer option works. Up to Socat 1.7.4.3, it failed because it
# cleared the CREAD flag.
NAME=RAWER
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%pty%*|*%$NAME%*)
TEST="$NAME: Test if the rawer option fails"
# Invoke Socat with a terminal address with option rawer. When it has no error
# the test succeeded.
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$SOCAT -lp outer /dev/null EXEC:\"$SOCAT\\ -lp\\ inner\\ -\\,rawer\\ PIPE\",pty"
printf "test $F_n $TEST... " $N
eval "$CMD0" >/dev/null 2>"${te}0"
rc0=$?
if [ $rc0 -eq 0 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	echo "$CMD0" >&2
    fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0" >&2
    cat "${te}0" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Up to 1.7.4.3 there was a bug with the lowport option:
# Active addresses UDP-SEND, UDP-SENDTO always bound to port 1 instead of
# 640..1023
NAME=UDP_LOWPORT
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%$NAME%*)
TEST="$NAME: UDP4-SEND with lowport"
# Run Socat with UDP4-SEND:...,lowport and full logging and check the
# parameters of bind() call. If port is in the range 640..1023 the test
# succeeded.
# This test does not require root because it just checks log of bind() but does
# not require success
# This test fails if WITH_SYCLS is turned off
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
#newport udp4 	# not needed in this test
CMD="$TRACE $SOCAT $opts -d -d -d -d /dev/null UDP4-SENDTO:$LOCALHOST:$PORT,lowport"
printf "test $F_n $TEST... " $N
$CMD >/dev/null 2>"${te}"
rc1=$?
LOWPORT=$(grep '[DE] bind(.*:' $te |sed 's/.*:\([0-9][0-9]*\)[}]*,.*/\1/' |head -n 1)
#echo "LOWPORT=\"$LOWPORT\"" >&2
#type socat >&2
#if  [[ $LOWPORT =~ [0-9][0-9]* ]] && [ "$LOWPORT" -ge 640 -a "$LOWPORT" -le 1023 ]; then
if re_match "$LOWPORT" '^[0-9][0-9]*' ]] && [ "$LOWPORT" -ge 640 -a "$LOWPORT" -le 1023 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    ok
elif $SOCAT -V |grep -q "undef WITH_SYCLS"; then
    $PRINTF "$CANT (no SYCLS)\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    cant
else
    $PRINTF "$FAILED\n"
    echo "$CMD"
    cat "${te}" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test if trailing garbage in integer type options gives error
NAME=MISSING_INTEGER
case "$TESTS" in
*%$N%*|*%functions%*|*%syntax%*|*%bugs%*|*%$NAME%*)
TEST="$NAME: Error on option that's missing integer value"
# Invoke Socat with pty and option ispeed=b19200.
# When socat terminates with error the test succeeded
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts - PTY,ispeed=b19200"
printf "test $F_n $TEST... " $N
$CMD0 </dev/null >/dev/null 2>"${te}0"
if grep -q "missing numerical value" "${te}0"; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test if trailing garbage in integer type options gives error
NAME=INTEGER_GARBAGE
case "$TESTS" in
*%$N%*|*%functions%*|*%syntax%*|*%bugs%*|*%$NAME%*)
TEST="$NAME: Error on trailing garbage"
# Invoke Socat with pty and option ispeed=b19200.
# When socat terminates with error the test succeeded
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts - PTY,ispeed=19200B"
printf "test $F_n $TEST... " $N
$CMD0 </dev/null >/dev/null 2>"${te}0"
if grep -q "trailing garbage" "${te}0"; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0" >&2; fi
    if [ "$debug" ]; then cat ${te} >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if Filan can print the target of symbolic links
NAME=FILANSYMLINK
case "$TESTS" in
*%$N%*|*%filan%*|*%$NAME%*)
TEST="$NAME: capability to display symlink target"
# Run Filan on a symbolic link
# When its output contains "LINKTARGET=<target>" the test succeeded
if ! eval $NUMCOND; then :; else
tf="$td/test$N.file"
tl="$td/test$N.symlink"
te="$td/test$N.stderr"
printf "test $F_n $TEST... " $N
touch "$tf"
ln -s "$tf" "$tl"
target=$($FILAN -f "$tl" 2>$te |tail -n 1 |sed 's/.*LINKTARGET=\([^ ]*\)/\1/')
if [ "$target" = "$tf" ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	echo "touch \"$tf\""
	echo "ln -s \"$tf\" \"$tl\""
	echo "$FILAN -f "$tl" 2>$te |tail -n 1 |sed 's/.*LINKTARGET=\([^ ]*\)/\1/'"
    fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "touch \"$tf\"" >&2
    echo "ln -s \"$tf\" \"$tl\"" >&2
    echo "$FILAN -f "$tl" 2>$te |tail -n 1 |sed 's/.*LINKTARGET=\([^ ]*\)/\1/'" >&2
    cat "$te"
    failed
fi
kill $spid 2>/dev/null
wait
fi ;; # NUMCOND
esac
N=$((N+1))


# Test preservation of packet boundaries from Socat to sub processes of
# various kind and back to Socat via socketpair with socket type datagram.
# (EXECSOCKETPAIRPACKETS SYSTEMSOCKETPAIRPACKETS)
for addr in exec system; do
  ADDR=$(echo $addr |tr a-z A-Z)
  NAME=${ADDR}SOCKETPAIRPACKETS
  case "$TESTS" in
    *%$N%*|*%functions%*|*%exec%*|*%socketpair%*|*%unix%*|*%dgram%*|*%packets%*|*%$NAME%*)
	TEST="$NAME: simple echo via $addr of cat with socketpair, keeping packet boundaries"
# Start a Socat process with a UNIX datagram socket on the left side and with
# a sub process connected via datagram socketpair that keeps packet boundaries
# (here: another Socat process in unidirectional mode).
# Pass two packets to the UNIX datagram socket; let Socat wait a little time
# before processing,
# so the packets are at the same time in the receive queue.
# The process that sends these packet uses a short packet size (-b),
# so the returned data is truncated in case the packets were merged.
# When the complete data is returned, the test succeeded.
	if ! eval $NUMCOND; then :; else
	    SAVEMICS=$MICROS
	    MICROS=500000
	    ts0="$td/test$N.sock0"
	    ts1="$td/test$N.sock1"
	    tf="$td/test$N.stdout"
	    te="$td/test$N.stderr"
	    tdiff="$td/test$N.diff"
	    da="test$N $(date) $RANDOM"
	    #CMD0="$TRACE $SOCAT $opts -lp server -T 2 UNIX-SENDTO:$ts1,bind=$ts0 $ADDR:\"$SOCAT -lp echoer -u - -\",pty,echo=0,pipes" 	# test the test
	    CMD0="$TRACE $SOCAT $opts -lp server -t $(reltime 1) -T $(reltime 2) UNIX-SENDTO:$ts1,bind=$ts0,null-eof $ADDR:\"$SOCAT -lp echoer -u - -\",socktype=$SOCK_DGRAM",shut-null
	    CMD1="$SOCAT $opts -lp client -b 24 -t $(reltime 2) -T $(reltime 3) - UNIX-SENDTO:$ts0,bind=$ts1",shut-null
	    printf "test $F_n $TEST... " $N
	    export SOCAT_TRANSFER_WAIT=1
	    eval "$CMD0" >/dev/null 2>"${te}0" &
	    pid0="$!"
	    unset SOCAT_TRANSFER_WAIT
	    waitunixport $ts0 1
	    { echo -n "${da:0:20}"; relsleep 1; echo "${da:20}"; } |$CMD1 >"${tf}1" 2>"${te}1"
	    rc1=$?
	    kill $pid0 2>/dev/null; wait
	    if [ "$rc1" -ne 0 ]; then
		$PRINTF "$FAILED (rc1=$rc1): $TRACE $SOCAT:\n"
		echo "$CMD0 &"
		cat "${te}0"
		echo "{ echo -n \"${da:0:20}\"; relsleep 1; echo \"${da:20}\"; } |$CMD1"
		cat "${te}1"
		failed
	    elif ! echo "$da" |diff - "${tf}1" >"$tdiff"; then
		$PRINTF "$FAILED (diff)\n"
		echo "$CMD0 &" >&2
		cat "${te}0" >&2
		echo "{ echo -n \"${da:0:20}\"; relsleep 1; echo \"${da:20}\"; } |$CMD1" >&2
		cat "${te}1" >&2
		echo "diff:" >&2
		cat $tdiff >&2
		failed
	    else
		$PRINTF "$OK\n"
		if [ "$VERBOSE" ]; then
		    echo "$CMD0 &" >&2
		    echo "{ echo -n \"${da:0:20}\"; relsleep 1; echo \"${da:20}\"; } |$CMD1" >&2
		fi
		ok
	    fi
	    MICROS=$SAVEMICS
	fi # NUMCOND
	;;
  esac
  N=$((N+1))
done  # for


# Test if a special quote based syntax error in dalan module does not raise
# SIGSEGV
NAME=DALAN_NO_SIGSEGV
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%dalan%*|*%listen%*|*%$NAME%*)
TEST="$NAME: Dalan syntax error does not raise SIGSEGV"
# Invoke Socat with an address that has this quote based syntax error.
# When exit code is 1 (due to syntax error) the test succeeded.
if ! eval $NUMCOND; then :
elif ! a=$(testfeats GOPEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $a not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs - GOPEN SOCKET-LISTEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $a not available in $SOCAT${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT $opts /dev/null SOCKET-LISTEN:1:1:'"/tmp/sock"'"
printf "test $F_n $TEST... " $N
$CMD >/dev/null 2>"${te}"
rc1=$?
if [ $rc1 -eq 1 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD"; fi
    if [ "$DEBUG" ];   then cat "${te}" >&2; fi
    ok
elif [ $rc1 -eq 139 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD"
    cat "${te}" >&2
    failed
else
    # Something unexpected happened
    $PRINTF "$CANT\n"
    echo "$CMD"
    cat "${te}" >&2
    cant
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if filan -s correctly displays TCP on appropriate FDs
# This feature was broken in version 1.7.4.4
NAME=FILAN_SHORT_TCP
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%filan%*|*%listen%*|*%$NAME%*)
TEST="$NAME: filan -s displays TCP etc"
# Establish a TCP connection using Socat server and client; on the server
# exec() filan -s using nofork option, so its output appears on the client.
# When the second word in the first line is "tcp" the test succeeded.
if ! eval $NUMCOND; then :
elif ! a=$(testfeats STDIO IP4 TCP LISTEN EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $a not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs STDIO TCP4 TCP4-LISTEN EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $a not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions so-reuseaddr nofork ) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -t $T4 TCP4-LISTEN:$PORT,reuseaddr EXEC:'$FILAN -s',nofork"
CMD1="$TRACE $SOCAT $opts -t $T8 - TCP4:localhost:$PORT"
printf "test $F_n $TEST... " $N
eval "$CMD0" >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1" </dev/null
rc1=$?
kill $pid0 2>/dev/null; wait
result="$(head -n 1 ${tf}1 |awk '{print($2);}')"
if [ $rc1 -eq 0 -a "$result" = tcp ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    if [ $rc1 -ne 0 ]; then
	echo "rc=$rc1" >&2
    else
	echo "result is \"$result\" instead of \"tcp\"" >&2
    fi
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if the settings of the terminal that Socat is invoked in are restored
# on termination.
# This failed on Open-Solaris family OSes up to 1.7.4.4
NAME=RESTORE_TTY
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%termios%*|*%tty%*|*%$NAME%*)
TEST="$NAME: Restoring of terminal settings"
# With an outer Socat command create a new pty and a bash in it.
# In this bash store the current terminal settings, then invoke a temporary
# inner Socat command that changes the term to raw mode and terminates.
# When the terminal settings afterwards are the same as before the call the
# test succeeded.
if ! eval $NUMCOND; then :
elif ! $(type stty >/dev/null 2>&1); then
    $PRINTF "test $F_n $TEST... ${YELLOW}stty not available${NORMAL}\n" $N
    cant
elif ! a=$(testfeats STDIO SYSTEM PTY GOPEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $a not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! a=$(testaddrs - STDIO SYSTEM GOPEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $a not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions cfmakeraw pty setsid ctty stderr) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
else
te="$td/test$N.stderr"
tx0="$td/test$N.stty0"
tx1="$td/test$N.stty1"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT $opts -lp outersocat - SYSTEM:\"stty\ >$tx0;\ $SOCAT\ -\,cfmakeraw\ /dev/nul\l >${te};\ stty\ >$tx1\",pty,setsid,ctty,stderr"
printf "test $F_n $TEST... " $N
eval "$CMD" >/dev/null 2>${te}.outer
rc=$?
if diff $tx0 $tx1 >$tdiff 2>&1; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD &"; fi
    if [ "$DEBUG" ];   then cat "${te}" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD"
    cat "${te}" >&2
    cat "${te}.outer" >&2
    cat $tdiff >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if EXEC'd program inherits only the stdio file descriptors
# thus there are no FD leaks from Socat to EXEC'd program
NAME=EXEC_FDS
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%filan%*|*%$NAME%*)
TEST="$NAME: Socat does not leak FDs to EXEC'd program"
# Run Socat with EXEC address, execute Filan to display its file descriptors
# Test succeeds when only FDs 0, 1, 2 are in use.
if ! eval $NUMCOND; then :;
elif ! a=$(testaddrs STDIO EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $a not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions stderr) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT $opts - EXEC:\"$FILAN -s\""
printf "test $F_n $TEST... " $N
eval "$CMD" >"${tf}" 2>"${te}"
# "door" is a special FD type on Solaris/SunOS
if [ "$(cat  "${tf}" |grep -v ' door ' |wc -l)" -eq 3 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD"; fi
    if [ "$DEBUG" ];   then cat "${te}" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD" >&2
    cat "${te}" >&2
    cat "${tf}" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test if Socat makes the sniffing file descriptos (-r, -R) CLOEXEC to not leak
# them to EXEC'd program
NAME=EXEC_SNIFF
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%filan%*|*%$NAME%*)
TEST="$NAME: Socat does not leak sniffing FDs"
# Run Socat sniffing both directions, with EXEC address,
# execute Filan to display its file descriptors
# Test succeeds when only FDs 0, 1, 2 are in use.
if ! eval $NUMCOND; then :;
elif ! a=$(testaddrs STDIO EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $a not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions stderr) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD="$TRACE $SOCAT $opts -r $td/test$N.-r -R $td/test$N.-R  - EXEC:\"$FILAN -s\""
printf "test $F_n $TEST... " $N
eval "$CMD" >"${tf}" 2>"${te}"
# "door" is a special FD type on Solaris/SunOS
if [ "$(cat  "${tf}" |grep -v ' door ' |wc -l)" -eq 3 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD"; fi
    if [ "$DEBUG" ];   then cat "${te}" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD" >&2
    cat "${te}" >&2
    cat "${tf}" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


while read KEYW FEAT RUNS ADDR IPPORT; do
if [ -z "$KEYW" ] || [[ "$KEYW" == \#* ]]; then continue; fi
PROTO=$KEYW
proto="$(tolower "$PROTO")"
feat="$(tolower "$FEAT")"
# test the fork option on really RECVFROM oriented sockets
NAME=${KEYW}_FORK
case "$TESTS" in
*%$N%*|*%functions%*|*%fork%*|*%$feat%*|*%$proto%*|*%socket%*|*%$NAME%*)
TEST="$NAME: ${KEYW}-RECVFROM with fork option"
# Start a RECVFROM process with fork option and SYSTEM address where clients
# data determines the sleep time; send a record with sleep before storing the
# data, then send a record with 0 sleep before storing data.
# When the second record is stored before the first one the test succeeded.
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats $FEAT STDIO SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs - STDIO SYSTEM $PROTO-RECVFROM $PROTO-SENDTO); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions fork ) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
elif ! runs$RUNS >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}$(toupper $RUNS) not available${NORMAL}\n" $N
    cant
else
case "X$IPPORT" in
    "XPORT")
    newport $proto
    tsl=$PORT 		# test socket listen address
    tsc="$ADDR:$PORT"	# test socket connect address
    ;;
    *)
    tsl="$(eval echo "$ADDR")"	# resolve $N
    tsc=$tsl
esac
#ts="$td/test$N.sock"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -t $(reltime 30) $PROTO-RECVFROM:$tsl,fork,so-rcvtimeo=1 SYSTEM:'read t x; sleep \$t; echo \\\"\$x\\\" >>'\"$tf\""
CMD1="$TRACE $SOCAT $opts -t $(reltime 30) - $PROTO-SENDTO:$tsc"
printf "test $F_n $TEST... " $N
eval $CMD0 </dev/null 2>"${te}0" &
pid0=$!
wait${proto}port $tsl 1
echo "$(reltime 20) $da 1" |$CMD1 >"${tf}1" 2>"${te}1" &
pid1=$!
relsleep 10
echo "$(reltime 0) $da 2" |$CMD1 >"${tf}2" 2>"${te}2" &
pid2=$!
relsleep 20
cpids="$(childpids $pid0 </dev/null)"
kill $pid1 $pid2 $cpids $pid0 2>/dev/null
wait 2>/dev/null
if $ECHO "$da 2\n$da 1" |diff -u - $tf >$tdiff; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    echo "diff:" >&2
    cat "$tdiff" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))
done <<<"
UDP4  UDP  ip4  127.0.0.1 PORT
UDP6  UDP  ip6  [::1]     PORT
UNIX  unix unix   $td/test\$N.server -
"


# Test if option -S turns off logging of SIGTERM
NAME=SIGTERM_NOLOG
case "$TESTS" in
*%$N%*|*%functions%*|*%signal%*|*%$NAME%*)
TEST="$NAME: Option -S can turn off logging of SIGTERM"
# Start Socat with option -S 0x0000, kill it with SIGTERM
# When no logging entry regarding this signal is there, the test succeeded
if ! eval $NUMCOND; then :;
elif ! $SOCAT -h | grep -e " -S\>" >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option -S not available${NORMAL}\n" $N
    cant
elif ! F=$(testfeats PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
CMD0="$TRACE $SOCAT $opts -S 0x0000 PIPE PIPE"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
relsleep 1 	# give process time to start
kill -TERM $pid0 2>/dev/null
wait 2>/dev/null
if ! grep "exiting on signal" ${te}0 >/dev/null; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$DEBUG" ];   then echo "kill -TERM <pid>" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "kill -TERM <pid>" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if option -S turns on logging of signal 31
NAME=SIG31_LOG
case "$TESTS" in
*%$N%*|*%functions%*|*%signal%*|*%$NAME%*)
TEST="$NAME: Option -S can turn on logging of signal 31"
# Start Socat with option -S 0x80000000, kill it with -31
# When a logging entry regarding this signal is there, the test succeeded
if ! eval $NUMCOND; then :;
elif ! $SOCAT -h | grep -e " -S\>" >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option -S not available${NORMAL}\n" $N
    cant
elif ! F=$(testfeats PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -S 0x80000000 PIPE PIPE"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
relsleep 1 	# give process time to start
kill -31 $pid0 2>/dev/null; wait
if grep "exiting on signal" ${te}0 >/dev/null; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$DEBUG" ];   then echo "kill -31 <pid>" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "kill -31 <pid>" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the http-version of the PROXY-CONNECT address
NAME=PROXY_HTTPVERSION
case "$TESTS" in
*%$N%*|*%functions%*|*%proxy%*|*%$NAME%*)
TEST="$NAME: PROXY-CONNECT with option http-version"
if ! eval $NUMCOND; then :;
elif ! $(type proxyecho.sh >/dev/null 2>&1); then
    $PRINTF "test $F_n $TEST... ${YELLOW}proxyecho.sh not available${NORMAL}\n" $N
    cant
elif ! F=$(testfeats IP4 TCP LISTEN EXEC STDIO PROXY); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs TCP4-LISTEN EXEC STDIO PROXY-CONNECT); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions so-reuseaddr crlf pf proxyport http-version) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
ts="$td/test$N.sh"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
CMD0="$TRACE $SOCAT $opts TCP4-L:$PORT,reuseaddr,crlf EXEC:\"/usr/bin/env bash proxyecho.sh -V 1.1\""
CMD1="$TRACE $SOCAT $opts - PROXY:$LOCALHOST:127.0.0.1:1000,pf=ip4,proxyport=$PORT,http-version=1.1"
printf "test $F_n $TEST... " $N
eval "$CMD0 2>\"${te}1\" &"
pid=$!	# background process id
waittcp4port $PORT 1
echo "$da" |$CMD1 >"$tf" 2>"${te}0"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "diff:" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$debug" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$debug" ];   then cat "${te}1" >&2; fi
    ok
fi
kill $pid 2>/dev/null
wait
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# Test the so-rcvtimeo address option with DTLS
NAME=RCVTIMEO_DTLS
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%udp%*|*%timeout%*|*%openssl%*|*%dtls%*|*%$NAME%*)
TEST="$NAME: test the so-rcvtimeo option with DTLS"
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO OPENSSL); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO DTLS); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions verify so-rcvtimeo) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
# We need a hanging connection attempt, guess an address for this
#HANGIP=0.0.0.1 	# some OSes refuse to end to this address
HANGIP=8.8.8.9 		# 2025 this hangs...
te1="$td/test$N.stderr1"
tk1="$td/test$N.kill1"
te2="$td/test$N.stderr2"
tk2="$td/test$N.kill2"
$PRINTF "test $F_n $TEST... " $N
# First, try to make socat hang and see if it can be killed
CMD1="$TRACE $SOCAT $opts - DTLS:$HANGIP:1,verify=0"
$CMD1 >"$te1" 2>$te1 </dev/null &
pid1=$!
relsleep 2
if ! kill -0 $pid1 2>"$tk1"; then
    $PRINTF "${YELLOW}does not hang${NORMAL}\n"
    if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    cant
    wait
else
    # DTLS restarts read() a few times
    while kill $pid1 2>/dev/null; do :; done
# Second, set so-rcvtimeo and see if Socat exits before kill
CMD2="$TRACE $SOCAT $opts - DTLS:$HANGIP:1,verify=0,so-rcvtimeo=$(reltime 1)"
$CMD2 >"$te1" 2>$te2 </dev/null &
pid2=$!
relsleep 8 	# in OpenSSL 1.1.1f DTLS takes two timeouts
sleep 0.02 	# in OpenSSL 3.0.13 SSL_CTX_clear_mode() needs e.g. 0.02s
kill $pid2 2>"$tk2"
prc2=$?
wait
if [ $prc2 -eq 0 ]; then
    $PRINTF "$FAILED (not timeout)\n"
    echo "$CMD2" >&2
    cat "$te2" >&2
    cat "$tk2" >&2
    failed
    while kill $pid2 2>/dev/null; do :; done
    wait
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD2 &"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi
wait
fi ;; # testfeats, NUMCOND
esac
N=$((N+1))


# Test the use of interesting variables in the sniffing file names
NAME=VARS_IN_SNIFFPATH
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%listen%*|*%fork%*|*%$NAME%*)
TEST="$NAME: sniff file names with variables"
# Start a server process with option fork that sniffs traffic and stores it in
# two files for each child process, using PID, timestamp, microseconds, and
# client IP
# Connect two times.
# For now we say that the test succeeded when 4 log files have been generated.
if ! eval $NUMCOND; then :;
elif ! A=$(testfeats IP4 TCP LISTEN PIPE STDIO); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs - TCP4 TCP4-LISTEN PIPE STDIO); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions so-reuseaddr fork) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts -T 1 -lp server0 -r \"$td/test$N.\\\$PROGNAME-\\\$TIMESTAMP.\\\$MICROS-\\\$SERVER0_PEERADDR-\\\$\\\$.in.log\" -R \"$td/test$N.\\\$PROGNAME-\\\$TIMESTAMP.\\\$MICROS-\\\$SERVER0_PEERADDR-\\\$\\\$.out.log\" TCP4-LISTEN:$PORT,so-reuseaddr,fork PIPE"
CMD1="$TRACE $SOCAT $opts - TCP4-CONNECT:$LOCALHOST:$PORT"
printf "test $F_n $TEST... " $N
eval "$CMD0" >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
echo "$da" |$CMD1 >"${tf}1a" 2>"${te}1a"
rc1a=$?
echo "$da" |$CMD1 >"${tf}1b" 2>"${te}1b"
rc1b=$?
kill $(childpids $pid0) $pid0 2>/dev/null
wait 2>/dev/null
if [ $rc1a != 0 -o $rc1b != 0 ]; then
    $PRINTF "$FAILED (client problem)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1a" >&2
    echo "$CMD1"
    cat "${te}1b" >&2
    failed
elif test $(ls -l $td/test$N.*.log |wc -l) -eq 4 &&
	test $(ls $td/test$N.*.log |head -n 1 |wc -c) -ge 56; then
    # Are the names correct?
    # Convert timestamps to epoch and compare
    # Are the contents correct?
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1b" >&2; fi
    ok
elif test -f $td/test$N.\$PROGNAME-\$TIMESTAMP.\$MICROS-\$SERVER0_PEERADDR-\$\$.in.log; then
    $PRINTF "$FAILED (vars not resolved)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1a" >&2
    echo "$CMD1"
    cat "${te}1b" >&2
    failed
else
    $PRINTF "$FAILED (unknown)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1a" >&2
    echo "$CMD1"
    cat "${te}1b" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test logging of statistics on Socat option --statistics
NAME=OPTION_STATISTICS
case "$TESTS" in
*%$N%*|*%functions%*|*%stats%*|*%system%*|*%stdio%*|*%$NAME%*)
TEST="$NAME: Socat option --statistics"
# Invoke Socat with option --statistics, transfer some date, and check the log
# file for the values
if ! eval $NUMCOND; then :;
elif ! $(type  >/dev/null 2>&1); then
    $PRINTF "test $F_n $TEST... ${YELLOW}tee not available${NORMAL}\n" $N
    cant
elif ! F=$(testfeats STATS STDIO SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions pty cfmakeraw) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts --statistics STDIO SYSTEM:'tee /dev/stdout',pty,cfmakeraw"
printf "test $F_n $TEST... " $N
echo "$da" |eval "$CMD0" >"${tf}0" 2>"${te}0"
rc0=$?
if [ $rc0 -ne 0 ]; then
    # The test could not run meaningfully
    $PRINTF "$CANT\n"
    if [ "$VERBOSE" ]; then echo "$CMD0"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    cant
elif [ $(grep STATISTICS "${te}0" |wc -l) -eq 2 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test logging of statistics on SIGUSR1
NAME=SIGUSR1_STATISTICS
case "$TESTS" in
*%$N%*|*%functions%*|*%signal%*|*%stats%*|*%system%*|*%stdio%*|*%$NAME%*)
TEST="$NAME: statistics on SIGUSR1"
# Invoke Socat without option --statistics, transfer some date, send signal
# USR1,and check the log file for the values
if ! eval $NUMCOND; then :;
elif ! $(type tee >/dev/null 2>&1); then
    $PRINTF "test $F_n $TEST... ${YELLOW}tee not available${NORMAL}\n" $N
    cant
elif ! $(type pkill >/dev/null 2>&1); then
    $PRINTF "test $F_n $TEST... ${YELLOW}pkill not available${NORMAL}\n" $N
    cant
elif ! F=$(testfeats STATS STDIO SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions pty cfmakeraw) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts STDIO SYSTEM:'tee /dev/stdout 2>/dev/null',pty,cfmakeraw"
TTY=$(tty |sed 's|/dev/||')
CMD1="pkill -USR1 -t $TTY socat"
printf "test $F_n $TEST... " $N
# On Fedora-41 pkill can be slow (eg.20ms)
{ echo "$da"; relsleep 20; } |eval "$CMD0" >"${tf}0" 2>"${te}0" &
pid0=$!
#date +'%Y-%m-%dT%H:%M:%S.%N' >>"${te}1"
relsleep 2
#date +'%Y-%m-%dT%H:%M:%S.%N' >>"${te}1"
$CMD1 2>"${te}1"
relsleep 2
#date +'%Y-%m-%dT%H:%M:%S.%N' >>"${te}1"
wait
pkill -t $TTY socat >>"${te}1"
if [ "$(grep STATISTICS "${te}0" |wc -l)" -eq 2 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
else
    $PRINTF "$FAILED (no stats)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the children-shutup option
NAME=CHILDREN_SHUTUP
case "$TESTS" in
*%$N%*|*%functions%*|*%exec%*|*%fork%*|*%socket%*|*%unix%*|*%$NAME%*)
TEST="$NAME: test the children-shutup option"
# Run a UNIX domain listening server with options fork and children-shutup, and
# that connects to a closed TCP4 port.
# Connect to the server and check if it logs the TCP4-CONNECT failure as warning.
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats UNIX LISTEN EXEC FILE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs UNIX-LISTEN TCP4 FILE UNIX-CONNECT); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions fork children-shutup) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
else
newport tcp4
ts="$td/test$N.sock"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts UNIX-LISTEN:$ts,fork,children-shutup TCP4:localhost:$PORT"
CMD1="$TRACE $SOCAT $opts -u FILE:/dev/null UNIX-CONNECT:$ts"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waitunixport $ts 1
{ $CMD1 2>"${te}1"; sleep 1; }
rc1=$?
kill $pid0 2>/dev/null; wait
relsleep 1 	# child process might need more time
if ! grep -q " E " ${te}0; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Socats INTERFACE address has to ignore outgoing packets if possible.
# On Linux is uses socket option PACKET_IGNORE_OUTGOING or it queries per
# packet the PACKET_OUTGOING flag of struct sockaddr_ll.sll_pkttype
NAME=INTERFACE_IGNOREOUTGOING
case "$TESTS" in
*%$N%*|*%functions%*|*%interface%*|*%tun%*|*%root%*|*%$NAME%*)
TEST="$NAME: INTERFACE ignores outgoing packets"
#idea: create a TUN interface and hook with INTERFACE.
# Send a packet out the interface, should not be seen by INTERFACE
if ! eval $NUMCOND; then :;
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}must be root${NORMAL}\n" $N
    cant
elif ! $(type ping >/dev/null 2>&1); then
    $PRINTF "test $F_n $TEST... ${YELLOW}ping not available${NORMAL}\n" $N
    cant
elif ! feat=$(testfeats TUN STDIO INTERFACE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs - TUN STDIO INTERFACE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions iff-up tun-type tun-name ) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
tl="$td/test$N.lock"
da="$(date) $RANDOM"
TUNNET=10.255.255
TUNNAME=tun9
CMD0="$TRACE $SOCAT $opts -L $tl TUN:$TUNNET.1/24,iff-up=1,tun-type=tun,tun-name=$TUNNAME -"
CMD1="$TRACE $SOCAT $opts -u INTERFACE:$TUNNAME -"
CMD2="ping -c 1 -w 1 -b $TUNNET.255"
printf "test $F_n $TEST... " $N
sleep 1 |$CMD0 2>"${te}0" >/dev/null &
pid0="$!"
#waitinterface "$TUNNAME"
relsleep 1
$CMD1 >"${tf}1" 2>"${te}1" &
pid1="$!"
relsleep 1
$CMD2 2>"${te}2" 1>&2
kill $pid1 2>/dev/null
relsleep 1
kill $pid0 2>/dev/null
wait
if [ $? -ne 0 ]; then
    $PRINTF "$FAILED: $TRACE $SOCAT:\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    cant
elif test -s "${tf}1"; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))


# Test if the SO_REUSEADDR socket option is applied automatically to TCP LISTEN
# type addresses.
NAME=TCP4_REUSEADDR
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%ip%*|*%ip4%*|*%tcp%*|*%tcp4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test if option reuseaddr's default is 1"
# Start a TCP4-LISTEN server, connect with a client, have the server shutdown
# the connection. Start the server on the same port again. If it starts the
# test succeeded.
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO PIPE IP4 TCP LISTEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO PIPE TCP4 TCP4-LISTEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions accept-timeout) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0a="$TRACE $SOCAT $opts -T 0.1 TCP4-LISTEN:$PORT PIPE"
CMD0b="$TRACE $SOCAT $opts TCP4-LISTEN:$PORT,accept-timeout=0.1 PIPE"
CMD1="$TRACE $SOCAT $opts STDIO TCP4-CONNECT:$LOCALHOST:$PORT"
printf "test $F_n $TEST... " $N
$CMD0a >/dev/null 2>"${te}0a" &
pid0=$!
waittcp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
$CMD0b >/dev/null 2>"${te}0b"
rc0b=$?
if [ $rc0b -eq 0 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0a &"; fi
    if [ "$DEBUG" ];   then cat "${te}0a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD0b"; fi
    if [ "$DEBUG" ];   then cat "${te}0b" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0a &"
    cat "${te}0a" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD0b"
    cat "${te}0b" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test if the SO_REUSEADDR socket option is applied automatically to OPENSSL LISTEN
# type addresses.
NAME=OPENSSL_6_REUSEADDR
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%ip%*|*%ip6%*|*%tcp%*|*%tcp6%*|*%listen%*|*%openssl%*|*%$NAME%*)
TEST="$NAME: test if option reuseaddr's default is 1 with SSL-L"
# Start an OPENSSL-LISTEN server using TCP on IPv6, connect with a client, have
# the server shutdown the connection. Start the server on the same port again.
# If it starts the test succeeded.
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats PIPE IP6 TCP OPENSSL LISTEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs PIPE OPENSSL-CONNECT OPENSSL-LISTEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions verify cert key) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv6 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
printf "test $F_n $TEST... " $N
newport tcp6
# Yup, it seems that with OpenSSL the side that begins with shutdown does NOT
# begin shutdown of the TCP connection
# therefore we let the client timeout
# result is not reliable (OK seen even without any SO_REUSEADDR)
CMD0a="$TRACE $SOCAT $opts -lp server1 -6 OPENSSL-LISTEN:$PORT,cert=testsrv.crt,key=testsrv.key,verify=0 PIPE"
CMD0b="$TRACE $SOCAT $opts -lp server2 -6 OPENSSL-LISTEN:$PORT,accept-timeout=.01,cert=testsrv.crt,key=testsrv.key,verify=0 PIPE"
CMD1="$TRACE $SOCAT $opts -lp client -6 -T 0.1 PIPE OPENSSL-CONNECT:$LOCALHOST6:$PORT,verify=0"
$CMD0a >/dev/null 2>"${te}0a" &
pid0=$!
waittcp6port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
$CMD0b >/dev/null 2>"${te}0b"
rc0b=$?
if [ $rc0b -eq 0 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0a &"; fi
    if [ "$DEBUG" ];   then cat "${te}0a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD0b"; fi
    if [ "$DEBUG" ];   then cat "${te}0b" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0a &"
    cat "${te}0a" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD0b"
    cat "${te}0b" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test if the so-reuseaddr= option prevents the SO_REUSEADDR socket option
NAME=REUSEADDR_NULL
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%ip%*|*%ip4%*|*%tcp%*|*%tcp4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: test option reuseaddr without value"
# Start a TCP4-LISTEN server with so-reuseaddr=, connect with a client, have
# the server shutdown the connection.
# Start the server on the same port again. If it fails with
# "Address already in use" the test succeeded.
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO PIPE IP4 TCP LISTEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO PIPE TCP4-CONNECT TCP4-LISTEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions so-reuseaddr accept-timeout) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0a="$TRACE $SOCAT $opts -T 0.1 TCP4-LISTEN:$PORT,so-reuseaddr= PIPE"
CMD0b="$TRACE $SOCAT $opts TCP4-LISTEN:$PORT,accept-timeout=0.1 PIPE"
CMD1="$TRACE $SOCAT $opts STDIO TCP4-CONNECT:$LOCALHOST:$PORT"
printf "test $F_n $TEST... " $N
$CMD0a >/dev/null 2>"${te}0a" &
pid0=$!
waittcp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
$CMD0b >/dev/null 2>"${te}0b"
rc0b=$?
if [ $rc0b -eq 1 ] && grep -q -e "Address already in use" -e "Address in use" "${te}0b"; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0a &"; fi
    if [ "$DEBUG" ];   then cat "${te}0a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD0b"; fi
    if [ "$DEBUG" ];   then cat "${te}0b" >&2; fi
    ok
#elif grep -q "accept: \(Connection\|Operation\) timed out" "${te}0b"; then
elif grep -q "accept: .* timed out" "${te}0b"; then
    # FreeBSD, Solaris do not seem to need SO_REUSEADDR with TCP at all
    $PRINTF "$CANT\n"
    if [ "$VERBOSE" ]; then echo "$CMD0a &"; fi
    if [ "$DEBUG" ];   then cat "${te}0a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD0b"; fi
    if [ "$DEBUG" ];   then cat "${te}0b" >&2; fi
    cant
else
    $PRINTF "$FAILED\n"
    echo "$CMD0a &"
    cat "${te}0a" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD0b"
    cat "${te}0b" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if Socats TCP4-client tries all addresses if necessary
NAME=TRY_ADDRS_4
case "$TESTS" in
*%$N%*|*%functions%*|*%tcp%*|*%tcp4%*|*%socket%*|*%internet%*|*%$NAME%*)
TEST="$NAME: try all available TCP4 addresses"
# Connect to a TCP4 port of a hostname that resolves to two addresses where at
# least on the first one the port is closed.
# server-4.dest-unreach.net has been configured for this purpose, it
# resolves to its public address and to 127.0.0.1; unfortunately
# forwarding nameservers need not keep order of A entries, so we need a port
# that is closed on both addresses.
# The test succeeded when the log shows that Socat tried to connect two times.
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats IP4 TCP GOPEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs TCP4-CONNECT GOPEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
elif [ -z "$INTERNET" ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}use test.sh option --internet${NORMAL}\n" $N
    cant
elif [ -z "$HAVEDNS" ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Broken DNS${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
while true; do
    newport tcp4
    OPEN=
    for addr in $ADDRS; do
	if $SOCAT /dev/null TCP4:$addr:$PORT 2>/dev/null; then
	    # port is open :-(
	    OPEN=1
	    break
	fi
    done
    if [ -z "$OPEN" ]; then
	break;
    fi
    newport tcp4
done
CMD="$TRACE $SOCAT $opts -d -d /dev/null TCP4:server-4.dest-unreach.net:$PORT"
printf "test $F_n $TEST... " $N
$CMD >/dev/null 2>"${te}"
rc=$?
if [ $(grep " N opening connection to .*AF=2 " ${te} |wc -l) -eq 2 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD"; fi
    if [ "$DEBUG" ];   then cat "${te}" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD"
    cat "${te}" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test if Socats TCP-client tries all addresses (IPv4+IPv6) if necessary
# Gives useful result only when getaddrinfo() to return both IPv4 and IPv6 addresses
# Therefore it appears useful to use AI-ADDRCONFIG on non-Linux systems
NAME=TRY_ADDRS_4_6
case "$TESTS" in
*%$N%*|*%functions%*|*%tcp%*|*%tcp4%*|*%tcp6%*|*%socket%*|*%internet%*|*%$NAME%*)
TEST="$NAME: for TCP try all available IPv4 and IPv6 addresses"
# Connect to a TCP port that is not open on localhost-4-6.dest-unreach.net,
# neither IPv4 nor IPv6
# Check the log if Socat tried both addresses
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats IP4 IP6 TCP); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs TCP-CONNECT GOPEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions ai-addrconfig) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
elif ! runsip6 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv6 not available or not routable${NORMAL}\n" $N
    cant
elif [ -z "$INTERNET" -a "$RES" != 'DEVTESTS' ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}use test.sh option --internet${NORMAL}\n" $N
    cant
elif [ -z "$HAVEDNS" ] && ! testfeats DEVTESTS >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Broken DNS${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
LOCALHOST_4_6=localhost-4-6.dest-unreach.net
if type nslookup >/dev/null 2>&1; then
    ADDRS=$(nslookup $LOCALHOST_4_6 |sed -n '/^$/,$ p' |grep ^Address |awk '{print($2);}')
elif type host >/dev/null 2>&1; then
    ADDRS=$(host $LOCALHOST_4_6 |sed 's/.*address //')
fi
# Specific config: on Ubuntu-12.04: getaddrinfo(...AI_ADDRCONFIG) does not
# resolve to IPv6 addresses even when there are link local IPv6 addresses
if test -f /etc/os-release &&
	grep -q '^NAME="Ubuntu"' /etc/os-release &&
	grep -q '^VERSION="12\.04' /etc/os-release; then
    AI_ADDRCONFIG="ai-addrconfig=0,"
elif [ $UNAME != 'Linux' ]; then
    AI_ADDRCONFIG="ai-addrconfig=0,"
fi
# Check if PORT is really closed on both addresses
while true; do
    OPEN=
    for addr in $ADDRS; do
	case $addr in
	    *.*) ;;
	    *:*) addr="[$addr]" ;
	esac
	if $SOCAT /dev/null TCP:$addr:$PORT,$AI_ADDRCONFIG 2>/dev/null; then
	    # port is open :-(
	    OPEN=1
	    break
	fi
    done
    if [ -z "$OPEN" ]; then
	break;
    fi
    newport tcp4
done
CMD="$TRACE $SOCAT $opts -d -d /dev/null TCP:$LOCALHOST_4_6:$PORT,$AI_ADDRCONFIG"
printf "test $F_n $TEST... " $N
$CMD >/dev/null 2>"${te}"
rc=$?
if [ $(grep " N opening connection to .*AF=[0-9]" ${te} |wc -l) -eq 2 ]; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD"; fi
    if [ "$DEBUG" ];   then cat "${te}" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD"
    cat "${te}" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the netns (net namespace) feature
NAME=NETNS
case "$TESTS" in
*%$N%*|*%functions%*|*%root%*|*%namespace%*|*%netns%*|*%socket%*|*%$NAME%*)
ns=socat-$$-test$N
TEST="$NAME: option netns (net namespace $ns)"
# Start a simple echo server with option netns on localhost of a net namespace;
# use a client process with option netns to send data to the net namespace
# net server and check the reply.
if ! eval $NUMCOND; then :;
elif [ "$UNAME" != Linux ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Only on Linux${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Must be root${NORMAL}\n" $N
    cant
elif ! $(type ip >/dev/null 2>&1); then
    $PRINTF "test $F_n $TEST... ${YELLOW}ip program not available${NORMAL}\n" $N
    cant
elif ! F=$(testfeats IP4 TCP LISTEN NAMESPACES); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO TCP-LISTEN TCP EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions netns) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="$TRACE $SOCAT $opts --experimental TCP4-LISTEN:$PORT,netns=$ns EXEC:'od -c'"
CMD1="$TRACE $SOCAT $opts --experimental - TCP4:127.0.0.1:$PORT,netns=$ns"
printf "test $F_n $TEST... " $N
ip netns del $ns 2>/dev/null 	# make sure it does not exist
ip netns add $ns
ip netns exec $ns  ip -4 addr add dev lo 127.0.0.1/8
ip netns exec $ns  ip link set lo up
eval "$CMD0" >/dev/null 2>"${te}0" &
pid0=$!
relsleep 1 	# if no matching wait*port function
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
ip netns del $ns
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED (client failed)\n"
    echo "ip netns del $ns"
    echo "ip netns add $ns"
    echo "ip netns exec $ns  ip -4 addr add dev lo 127.0.0.1/8"
    echo "ip netns exec $ns  ip link set lo up"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "ip netns del $ns"
    failed
elif echo "$da" |od -c |diff - ${tf}1 >"$tdiff"; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then
	echo "ip netns del $ns"
	echo "ip netns add $ns"
	echo "ip netns exec $ns  ip -4 addr add dev lo 127.0.0.1/8"
	echo "ip netns exec $ns  ip link set lo up"
    fi
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "ip netns del $ns"; fi
    ok
else
    $PRINTF "$FAILED (bad output)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    diff:
    cat "$tdiff"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test the netns (net namespace) feature with EXEC and reset
NAME=NETNS_EXEC
case "$TESTS" in
*%$N%*|*%functions%*|*%root%*|*%namespace%*|*%netns%*|*%socket%*|*%abstract%*|*%dgram%*|*%$NAME%*)
ns=socat-$$-test$N
TEST="$NAME: option netns with EXEC (net namespace $ns)"
# Start a simple server with option netns on localhost of a net namespace that
# stores data it receives;
# use a middle process that EXECs a socat client that connects to the server on
# the net namespace; then it listens on default namespace.
# With a third command line connect and send data to the middle process.
# When the data received by the server is correct the test succeeded.
if ! eval $NUMCOND; then :;
elif [ "$UNAME" != Linux ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Only on Linux${NORMAL}\n" $N
    cant
elif [ $(id -u) -ne 0 -a "$withroot" -eq 0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Must be root${NORMAL}\n" $N
    cant
elif ! $(type ip >/dev/null 2>&1); then
    $PRINTF "test $F_n $TEST... ${YELLOW}ip program not available${NORMAL}\n" $N
    cant
elif ! F=$(testfeats IP4 ABSTRACT_UNIXSOCKET UDP LISTEN NAMESPACES STDIO); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs ABSTRACT-RECV ABSTRACT-SENDTO CREATE EXEC UDP4-RECV STDIO UDP4); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $a not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions netns) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport udp4
CMD0="$TRACE $SOCAT $opts --experimental -u -T 1 ABSTRACT-RECV:test$N,netns=$ns CREAT:${tf}0"
CMD1="$TRACE $SOCAT $opts --experimental -U -T 1 EXEC:\"$SOCAT STDIO ABSTRACT-SENDTO\:test$N\",netns=$ns UDP4-RECV:$PORT"
CMD2="$TRACE $SOCAT $opts -u STDIO UDP4:127.0.0.1:$PORT"
printf "test $F_n $TEST... " $N
ip netns del $ns 2>/dev/null 	# make sure it does not exist
ip netns add $ns
#ip netns exec $ns  ip -4 addr add dev lo 127.0.0.1/8
#ip netns exec $ns  ip link set lo up
eval "$CMD0" >/dev/null 2>"${te}0" &
pid0=$!
relsleep 1 	# if no matching wait*port function
eval "$CMD1" 2>${te}1 &
pid1=$!
relsleep 1
echo "$da" |$CMD2 >"${tf}2" 2>"${te}2"
rc1=$?
kill $pid0 $pid1 2>/dev/null; wait
ip netns del $ns
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED (client failed)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
elif echo "$da" |diff - ${tf}0 >"$tdiff" 2>/dev/null; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
else
    $PRINTF "$FAILED (bad output)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    echo diff:
    cat "$tdiff"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=SOCKETPAIR_STREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%stdio%*|*%socketpair%*|*%$NAME%*)
TEST="$NAME: stdio and internal socketpair with stream"
if ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "STDIO SOCKETPAIR" \
		  "STDIO SOCKETPAIR" \
		  "" \
		  "" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    testecho "$N" "$NAME" "$TEST" "STDIO" "SOCKETPAIR" "$opts"
fi
esac
N=$((N+1))

NAME=SOCKETPAIR_DATAGRAM
case "$TESTS" in
*%$N%*|*%functions%*|*%stdio%*|*%socketpair%*|*%$NAME%*)
TEST="$NAME: stdio and internal socketpair with datagram"
if ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "STDIO SOCKETPAIR" \
		  "STDIO SOCKETPAIR" \
		  "" \
		  "" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    testecho "$N" "$NAME" "$TEST" "STDIO" "SOCKETPAIR,socktype=2" "$opts"
fi
esac
N=$((N+1))

NAME=SOCKETPAIR_SEQPACKET
case "$TESTS" in
*%$N%*|*%functions%*|*%stdio%*|*%socketpair%*|*%$NAME%*)
TEST="$NAME: stdio and internal socketpair with seqpacket"
if ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "STDIO SOCKETPAIR" \
		  "STDIO SOCKETPAIR" \
		  "" \
		  "" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    testecho "$N" "$NAME" "$TEST" "STDIO" "SOCKETPAIR,socktype=$SOCK_SEQPACKET" "$opts"
fi
esac
N=$((N+1))

# Test if SOCKETPAIR address with SOCK_DGRAM keeps packet boundaries
NAME=SOCKETPAIR_BOUNDARIES
case "$TESTS" in
*%$N%*|*%functions%*|*%socketpair%*|*%udp%*|*%udp4%*|*%ip4%*|*%dgram%*|*%$NAME%*)
TEST="$NAME: Internal socketpair keeps packet boundaries"
# Start a UDP4-DATAGRAM process that echoes data with datagram SOCKETPAIR;
# a client sends two packets with 24 and ~18 bytes using a UDP4-DATAGRAM. The
# client truncates packets to size 24, so when a large merged packet comes from
# server some data will be lost. If the original data is received, the test
# succeeded.
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO IP4 UDP SOCKETPAIR); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs - STDIO UDP4-DATAGRAM SOCKETPAIR); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions bind socktype ) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport udp; ts1p=$PORT
newport udp; ts2p=$PORT
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -T 0.2 UDP4-DATAGRAM:$LOCALHOST:$ts2p,bind=$LOCALHOST:$ts1p SOCKETPAIR,socktype=$SOCK_DGRAM"
CMD2="$TRACE $SOCAT $opts -b 24 -t 0.2 -T 0.3 - UDP4-DATAGRAM:$LOCALHOST:$ts1p,bind=$LOCALHOST:$ts2p"
printf "test $F_n $TEST... " $N
export SOCAT_TRANSFER_WAIT=0.2
$CMD1 2>"${te}1" &
pid1="$!"
unset SOCAT_TRANSFER_WAIT
waitudp4port $ts1p 1
{ echo -n "${da:0:20}"; relsleep 1; echo "${da:20}"; } |$CMD2 >>"$tf" 2>>"${te}2"
rc2="$?"
kill "$pid1" 2>/dev/null; wait;
if [ "$rc2" -ne 0 ]; then
    $PRINTF "$FAILED (rc2=$rc2): $TRACE $SOCAT:\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $PRINTF "$FAILED\n"
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    echo diff:
    cat "$tdiff"
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2 &"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the ACCEPT-FD address
NAME=ACCEPT_FD
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%systemd%*|*%accept%*|*%$NAME%*)
TEST="$NAME: ACCEPT-FD address"
# Start Socat with address ACCEPT-FD via systemd-socket-activate for echoing
# data.
# Connect with a client; the test succeeds when the client gets its data back.
if ! eval $NUMCOND; then :;
elif ! $(type systemd-socket-activate >/dev/null 2>&1); then
    $PRINTF "test $F_n $TEST... ${YELLOW}systemd-socket-activate not available${NORMAL}\n" $N
    cant
elif ! F=$(testfeats IP4 TCP LISTEN); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not available${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs ACCEPT-FD PIPE STDIO TCP4); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
CMD0="systemd-socket-activate -l $PORT --inetd $TRACE $SOCAT $opts ACCEPT-FD:0 PIPE"
CMD1="$TRACE $SOCAT $opts - TCP4:$LOCALHOST:$PORT"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
waittcp4port $PORT 1
echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if echo "$da" |diff "${tf}1" - >$tdiff; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    cat $tdiff >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the POSIX MQ feature with continuous READ and prioritization on Linux
NAME=POSIXMQ_READ_PRIO
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%posixmq%*|*%$NAME%*)
TEST="$NAME: POSIX-MQ with prio"
# Run a client/sender that creates a POSIX-MQ and sends a normal message and
# then a client/sender with a higher priority message.
# Run a passive/listening/receiving/reading process and check if it receives
# both messages and in the prioritized order
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds "" "" "" "POSIXMQ STDIO" "POSIXMQ-SEND POSIXMQ-READ STDIO" "mq-prio unlink-early unlink-close"); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
tq=/test$N
CMD0a="$TRACE $SOCAT $opts -u STDIO POSIXMQ-SEND:$tq,mq-prio=0,unlink-early"
CMD0b="$TRACE $SOCAT $opts -u STDIO POSIXMQ-SEND:$tq,mq-prio=1"
CMD1="$TRACE $SOCAT $opts -u POSIXMQ-READ:$tq,unlink-close STDIO"
printf "test $F_n $TEST... " $N
echo "$da 0" |$CMD0a 2>"${te}0a"
rc0a=$?
echo "$da 1" |$CMD0b 2>"${te}0b"
rc0b=$?
$CMD1 >"${tf}1" 2>"${te}1" &
pid1=$!
relsleep 1
kill $pid1; wait
if [ $rc0a -ne 0 -o $rc0b -ne 0 ]; then
    $PRINTF "$FAILED (rc0a=$rc0a, rc0b=$rc0b)\n"
    echo "$CMD0a"
    cat "${te}0a" >&2
    echo "$CMD0b"
    cat "${te}0b" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
elif $ECHO "$da 1\n$da 0" |diff - ${tf}1 >${tdiff}1; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0a"; fi
    if [ "$DEBUG" ];   then cat "${te}0a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD0b"; fi
    if [ "$DEBUG" ];   then cat "${te}0b" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
else
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD0a"
    cat "${te}0a" >&2
    echo "$CMD0b"
    cat "${te}0b" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    echo "difference:" >&2
    cat ${tdiff}1 >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test the POSIX MQ feature with RECV,fork
NAME=POSIXMQ_RECV_FORK
case "$TESTS" in
*%$N%*|*%functions%*|*%fork%*|*%socket%*|*%posixmq%*|*%$NAME%*)
TEST="$NAME: POSIX-MQ RECV with fork"
# Start a POSIX-MQ receiver with fork that creates a POSIX-MQ and stores its
# output.
# Run two clients/senders each with a message.
# Check if both messages are stored.
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds "" "" "" "POSIXMQ STDIO" "POSIXMQ-SEND POSIXMQ-READ STDIO" "fork unlink-early unlink-close"); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
tq=/test$N
CMD0="$TRACE $SOCAT $opts -u POSIXMQ-RECV:$tq,unlink-early,fork STDIO"
CMD1a="$TRACE $SOCAT $opts -u STDIO POSIXMQ-SEND:$tq"
CMD1b="$TRACE $SOCAT $opts -u STDIO POSIXMQ-SEND:$tq,unlink-close"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" >"${tf}0" &
pid0=$!
relsleep 1
echo "$da 0" |$CMD1a >/dev/null 2>"${te}1a"
rc1a=$?
echo "$da 1" |$CMD1b >/dev/null 2>"${te}1b"
rc1b=$?
relsleep 1
kill $pid0; wait
if [ $rc1a -ne 0 -o $rc1b -ne 0 ]; then
    $PRINTF "$FAILED (rc1a=$rc1a, rc1b=$rc1b)\n"
    echo "$CMD0"
    cat "${te}0" >&2
    echo "$CMD1a"
    cat "${te}1a" >&2
    echo "$CMD1b"
    cat "${te}1b" >&2
    failed
elif $ECHO "$da 0\n$da 1" |diff - ${tf}0 >${tdiff}0; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1a"; fi
    if [ "$DEBUG" ];   then cat "${te}1a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1b"; fi
    if [ "$DEBUG" ];   then cat "${te}1b" >&2; fi
    ok
else
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD0"
    cat "${te}0" >&2
    echo "$CMD1a"
    cat "${te}1a" >&2
    echo "$CMD1b"
    cat "${te}1b" >&2
    echo "difference:" >&2
    cat ${tdiff}0 >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test the POSIX MQ feature with RECV,fork,max-children
NAME=POSIXMQ_RECV_MAXCHILDREN
case "$TESTS" in
*%$N%*|*%functions%*|*%fork%*|*%maxchildren%*|*%socket%*|*%posixmq%*|*%$NAME%*)
TEST="$NAME: POSIX-MQ RECV with fork,max-children"
# Start a POSIX-MQ receiver with fork that creates a POSIX-MQ and stores its
# output via sub processes that sleep after writing.
# Run a client/sender that sends message 1;
# run a client/sender that sends message 2;
# run a client/sender that sends message 4, has to wait;
# write message 3 directly into output file;
# Check if the messages are stored in order of their numbers
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds "" "" "" "POSIXMQ STDIO SHELL" "POSIXMQ-SEND POSIXMQ-RECEIVE STDIO SHELL" "fork max-children unlink-early unlink-close"); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
tq=/test$N
CMD0="$TRACE $SOCAT $opts -u POSIXMQ-RECV:$tq,unlink-early,fork,max-children=2 SHELL:\"cat\ >>${tf}0;\ relsleep\ 5\""
CMD1a="$TRACE $SOCAT $opts -u STDIO POSIXMQ-SEND:$tq"
CMD1b="$TRACE $SOCAT $opts -u STDIO POSIXMQ-SEND:$tq,unlink-close"
printf "test $F_n $TEST... " $N
eval $CMD0 2>"${te}0" >"${tf}0" &
pid0=$!
relsleep 1
echo "$da 1" |$CMD1a >/dev/null 2>"${te}1a"
rc1a=$?
echo "$da 2" |$CMD1a >/dev/null 2>"${te}1b"
rc1b=$?
echo "$da 4" |$CMD1b >/dev/null 2>"${te}1c"
rc1c=$?
#sleep 0.5
relsleep 2
echo "$da 3" >>"${tf}0"
relsleep 5 	# as in SHELL
kill $(childpids $pid0) $pid0 2>/dev/null
wait 2>/dev/null
if [ $rc1a -ne 0 -o $rc1b -ne 0 ]; then
    $PRINTF "$FAILED (rc1a=$rc1a, rc1b=$rc1b, rc1c=$rc1c)\n"
    echo "$CMD0"
    cat "${te}0" >&2
    echo "$CMD1a"
    cat "${te}1a" >&2
    echo "$CMD1b"
    cat "${te}1b" >&2
    echo "$CMD1c"
    cat "${te}1c" >&2
    failed
elif $ECHO "$da 1\n$da 2\n$da 3\n$da 4" |diff - ${tf}0 >${tdiff}0; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1a"; fi
    if [ "$DEBUG" ];   then cat "${te}1a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1b"; fi
    if [ "$DEBUG" ];   then cat "${te}1b" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1c"; fi
    if [ "$DEBUG" ];   then cat "${te}1c" >&2; fi
    ok
else
    $PRINTF "$FAILED (diff)\n"
    echo "$CMD0"
    cat "${te}0" >&2
    echo "$CMD1a"
    cat "${te}1a" >&2
    echo "$CMD1b"
    cat "${te}1b" >&2
    echo "$CMD1c"
    cat "${te}1c" >&2
    echo "// diff:" >&2
    cat ${tdiff}0 >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test the POSIX MQ feature with SEND,fork,max-children
NAME=POSIXMQ_SEND_MAXCHILDREN
case "$TESTS" in
*%$N%*|*%functions%*|*%fork%*|*%maxchildren%*|*%socket%*|*%posixmq%*|*%$NAME%*)
TEST="$NAME: POSIX-MQ SEND with fork,max-children"
# Start a POSIX-MQ receiver that creates the MQ and transfers data from it
# to an output file
# Run a POSIX-MQ sender that forks two child shell processes that get data from
# a file queue with messages 1, 2, and 4, transfer it to the receiver and sleep
# afterwards to delay the third child by option max-children=2
# Afterwards write message 3 directly into output file; message 4 should be
# delayed due to max-children option
# Check if the messages are stored in order of their numbers.
# The data generator is implemented with just a directory containing files
# "1", "2", "4"
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds "" "" "" "POSIXMQ STDIO SHELL" "POSIXMQ-SEND POSIXMQ-READ STDIO SHELL" "fork max-children mq-prio unlink-early unlink-close"); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
tq="/test$N"
tQ="$td/test$N.q"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts -lp reader -u POSIXMQ-READ:$tq,unlink-early STDIO"
#CMD2="$TRACE $SOCAT $opts -lp worker -U POSIXMQ-SEND:$tq,fork,max-children=2,interval=$(relsecs 2) SHELL:'f=\$(ls -1 $tQ|head -n 1);\ test\ -f\ "$tQ/\$f"\ ||\ exit\ 1;\ {\ cat\ $tQ/\$f;\ rm\ $tQ/\$f;\ };\ sleep\ $(relsecs 5)'"
CMD2="$TRACE $SOCAT $opts -lp worker -U POSIXMQ-SEND:$tq,fork,max-children=2,interval=$(relsecs 2) SHELL:'shopt\ -s\ nullglob;\ f=\$(ls -1 $tQ|head -n 1);\ test\ -z\ "\$f"\ &&\ exit;\ {\ cat\ $tQ/\$f;\ rm\ $tQ/\$f;\ };\ sleep\ $(relsecs 5)'"
printf "test $F_n $TEST... " $N
# create data for the generator
mkdir -p $tQ
echo "$da 1" >$tQ/01
echo "$da 2" >$tQ/02
echo "$da 4" >$tQ/04
eval $CMD1 2>"${te}1" >>"${tf}1" &
pid1=$!
relsleep 1
eval $CMD2 2>"${te}2" &
pid2=$!
relsleep 4
echo "$da 3" >>"${tf}1"
relsleep 10
kill $(childpids -r $pid1) $pid1 $(childpids -r $pid2) $pid2 2>/dev/null
wait 2>/dev/null
# remove the MQ
$SOCAT -u /dev/null POSIXMQ-SEND:$tq,unlink-close 2>"${te}3b"
if $ECHO "$da 1\n$da 2\n$da 3\n$da 4" |diff - ${tf}1 >${tdiff}1; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD1"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2" >&2
    echo "// diff:" >&2
    cat ${tdiff}1 >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the sigint option with SHELL address
NAME=SHELL_SIGINT
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%shell%*|*%progcall%*|*%sigint%*|*%$NAME%*)
TEST="$NAME: sigint option with SHELL"
# Run Socat with an EXEC address invoking Socat, with option sigint
# Send the parent a SIGINT; when the child gets SIGINT too (vs.SIGTERM)
# the test succeeded
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO SHELL PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO SHELL PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions setsid sigint) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
#CMD0="$TRACE $SOCAT $opts -T 2 PIPE EXEC:\"socat\ -d\ -d\ -d\ -d\ -lu\ PIPE\ PIPE\",pty,setsid,sigint"
#CMD0="$TRACE $SOCAT $opts -T 2 PIPE EXEC:\"$CAT\",pty,setsid,sigint"
CMD0="$TRACE $SOCAT $opts -T 2 SOCKETPAIR EXEC:\"$CAT\",pty,setsid,sigint"
printf "test $F_n $TEST... " $N
eval $CMD0 >/dev/null 2>"${te}0" &
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
sleep 1
kill -INT $pid0
wait
if grep -q " W waitpid..: child .* exited with status 130" "${te}0" ||
   grep -q " W waitpid..: child .* exited on signal 2" "${te}0"; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the SHELL address with socketpair (default)
NAME=SHELL_SOCKETPAIR
case "$TESTS" in
*%$N%*|*%functions%*|*%shell%*|*%socketpair%*|*%$NAME%*)
TEST="$NAME: simple echo via SHELL of cat with socketpair"
# testecho ...
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO SHELL); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO SHELL); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
else
    testecho "$N" "$NAME" "$TEST" "" "SHELL:$CAT" "$opts" "$val_t"
fi
esac
N=$((N+1))

# Test the SHELL address with pipes
NAME=SHELL_PIPES
case "$TESTS" in
*%$N%*|*%functions%*|*%shell%*|*%pipe%*|*%$NAME%*)
TEST="$NAME: simple echo via SHELL of cat with pipes"
# testecho ...
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO SHELL SOCKETPAIR); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO SHELL PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
else
    testecho "$N" "$NAME" "$TEST" "" "SHELL:$CAT,pipes" "$opts" "$val_t"
fi
esac
N=$((N+1))

# Test the SHELL address with pty
NAME=SHELL_PTY
case "$TESTS" in
*%$N%*|*%functions%*|*%shell%*|*%pty%*|*%$NAME%*)
TEST="$NAME: simple echo via SHELL of cat with pty"
# testecho ...
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO SHELL PTY); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO SHELL PTY); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif [ "$SHELL" = /bin/ksh ]; then
    # on NetBSD-9.3 this test kills test.sh script...
    $PRINTF "test $F_n $TEST... ${YELLOW}/bin/ksh might kill test.sh${NORMAL}\n" $N
    cant
else
    testecho "$N" "$NAME" "$TEST" "" "SHELL:$CAT,pty,$PTYOPTS,$PTYOPTS2" "$opts" "$val_t"
fi
esac
N=$((N+1))

# Test the SHELL address halfclose with socketpair
NAME=SHELL_SOCKETPAIR_FLUSH
case "$TESTS" in
*%$N%*|*%functions%*|*%shell%*|*%socketpair%*|*%halfclose%*|*%$NAME%*)
TEST="$NAME: call  od -c  via SHELL using socketpair"
# testecho ...
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO SHELL); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO SHELL); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
else
    testod "$N" "$NAME" "$TEST" "" "SHELL:$OD_C" "$opts" "$val_t"
fi
esac
N=$((N+1))

# Test SHELL address halfclose with pipes
NAME=SHELL_PIPES
case "$TESTS" in
*%$N%*|*%functions%*|*%shell%*|*%pipe%*|*%halfclose%*|*%$NAME%*)
TEST="$NAME: call  od -c  via SHELL using pipes"
# testecho ...
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO SHELL SOCKETPAIR); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO SHELL PIPE); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
else
    testod "$N" "$NAME" "$TEST" "" "SHELL:$OD_C,pipes" "$opts" "$val_t"
fi
esac
N=$((N+1))


# Test the sigint option with SYSTEM address
NAME=SYSTEM_SIGINT
case "$TESTS" in
*%$N%*|*%functions%*|*%socket%*|*%progcall%*|*%system%*|*%sigint%*|*%$NAME%*)
TEST="$NAME: sigint option with SYSTEM"
# Run Socat with a SYSTEM address invoking Socat, with option sigint
# Send the parent a SIGINT; when the child gets SIGINT too (vs.SIGTERM)
# the test succeeded
# setsid is required so the initial SIGINT is not delivered to the sub process.
if ! eval $NUMCOND; then :;
elif [ "$UNAME" = "NetBSD" ]; then
    # On NetBSD-4.0 and NetBSD-9.3 this test hangs (signal has no effect)
    # (other versions not tried)
    $PRINTF "test $F_n $TEST... ${YELLOW}might hang on $UNAME${NORMAL}\n" $N
    cant
elif ! F=$(testfeats SYCLS STDIO SYSTEM SOCKETPAIR); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO SYSTEM SOCKETPAIR); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions setsid sigint) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
#CMD0="$TRACE $SOCAT $opts PIPE SYSTEM:\"$SOCAT\ -dddd\ -lf ${te}1\ PIPE\ PIPE\",setsid,sigint"
# Without -T process remains on OpenBSD-4, AIX, ?
CMD0="$TRACE $SOCAT $opts -T 2 SOCKETPAIR SYSTEM:\"$SOCAT\ -dddd\ -T\ 1\ -lf ${te}1\ PIPE\ PIPE\",setsid,sigint"
printf "test $F_n $TEST... " $N
eval $CMD0 >/dev/null 2>"${te}0" &
pid0=$!
sleep 1
#echo childpids:    $(childpids    $pid0)
#echo childpids -r: $(childpids -r $pid0)
kill -INT $(childpids -r $pid0) 2>/dev/null
wait 2>/dev/null
if grep -q " W waitpid..: child .* exited with status 130" "${te}0"; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    ok
else
    $PRINTF "${YELLOW}FAILED (shell does not propagate SIGINT?)${NORMAL}\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    cant
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the res-nsaddr (resolver, dns) option
NAME=RES_NSADDR
case "$TESTS" in
*%$N%*|*%functions%*|*%resolv%*|*%ip4%*|*%tcp4%*|*%socket%*|*%$NAME%*)
TEST="$NAME: test the res-nsaddr option"
# Start a supplementary Socat instance that will receive the DNS query.
# Run main Socat process, opening an IPv4 socket with option res-nsaddr
# directed to the aux process.
# When the supplementary Socat instance received the query the test succeeded.
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO IP4 UDP TCP); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO TCP4 UDP-RECVFROM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions res-nsaddr) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! runsip4 >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}IPv4 not available on host${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="$(echo test$N $(date) $RANDOM |tr ' :' '-')"
echo "$da" >"$td/test$N.da"
newport udp4
CMD0="$TRACE $SOCAT $opts -u UDP4-RECVFROM:$PORT -"
CMD1="$TRACE $SOCAT $opts - TCP4:$da:0,res-nsaddr=$LOCALHOST:$PORT"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" >"${tf}0" &
pid0=$!
waitudp4port $PORT 1
$CMD1 >"${tf}1" 2>"${te}1"
rc1=$?
kill $pid0 2>/dev/null; wait
if grep "$da" "${tf}0" >/dev/null; then
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
elif pgrep -u root nscd >/dev/null 2>&1; then
    $PRINTF "${YELLOW}FAILED (due to nscd?)${NORMAL}\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    cant
else
    $PRINTF "$FAILED (query not received)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1"
    cat "${te}1" >&2
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Some of the following tests need absolute path of Socat
case "$SOCAT" in
    /*) absSOCAT="$SOCAT" ;;
    */*) absSOCAT="$PWD/$SOCAT" ;;
    *) absSOCAT="$(type -p "$SOCAT")" ;;
esac
[ "$DEFS" ] && echo "absSOCAT=\"$absSOCAT\"" >&2

# Test the chdir option, in particular if chdir with the first address
# (CREATE) does not affect pwd of second address, i.e. original pwd is
# recovered
NAME=CHDIR_ON_CREATE
case "$TESTS" in
*%$N%*|*%functions%*|*%creat%*|*%system%*|*%chdir%*|*%$NAME%*)
TEST="$NAME: restore of pwd after CREAT with chdir option"
# Run Socat with first address CREAT with modified chdir,
# and second address SYSTEM (shell) with pwd command
# Check if the file is created with modified pwd but shell has original pwd
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats CREAT SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs - CREAT SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions chdir) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tc="test$N.creat"
    tdd="test$N.d"
    tdiff="$td/test$N.diff"
    tdebug="$td/test$N.debug"
    opwd=$(pwd)
    CMD0="$TRACE $absSOCAT $opts -U CREAT:$tc,chdir=$td SYSTEM:pwd"
    printf "test $F_n $TEST... " $N
    mkdir "$td/$tdd"
    pushd "$td/$tdd" >/dev/null
    $CMD0 >/dev/null 2>"${te}0"
    rc0=$?
    popd >/dev/null
    tpwd=$(find $td -name $tc -print); tpwd=${tpwd%/*}
    pwd2=$(cat $tpwd/$tc </dev/null)
    echo "Original pwd:  $opwd" >>$tdebug
    echo "Temporary pwd: $tpwd" >>$tdebug
    echo "Addr2 pwd:     $pwd2" >>$tdebug
    if [ "$rc0" -ne 0 ]; then
	$PRINTF "$FAILED\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	failed
    elif [ "$tpwd" != "$td" ]; then
	$PRINTF "$FAILED (chdir failed)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	failed
    elif ! echo "$pwd2" |diff "$td/$tc" - >$tdiff; then
	$PRINTF "$FAILED (bad pwd2)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test the chdir option, in particular if chdir with first address
# (SHELL) does not affect pwd of second address, i.e. original pwd is
# recovered
NAME=CHDIR_ON_SHELL
case "$TESTS" in
*%$N%*|*%functions%*|*%shell%*|*%system%*|*%chdir%*|*%$NAME%*)
TEST="$NAME: restore of pwd after SYSTEM with chdir option"
# Run Socat with first address SYSTEM:"cat >file" with chdir,
# and second address SYSTEM (shell) with pwd command.
# Check if the file is created with modified pwd but shell has original pwd
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats SHELL SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs SHELL SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions chdir) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tc="test$N.creat"
    tdd="test$N.d"
    tdiff="$td/test$N.diff"
    tdebug="$td/test$N.debug"
    opwd=$(pwd)
    CMD0="$TRACE $absSOCAT $opts SHELL:\"cat\ >$tc\",chdir=$td SYSTEM:pwd"
    printf "test $F_n $TEST... " $N
    mkdir "$td/$tdd"
    pushd "$td/$tdd" >/dev/null
    eval "$CMD0" >/dev/null 2>"${te}0"
    rc0=$?
    popd >/dev/null
    waitfile "$td/$tc"
    tpwd=$(find $td -name $tc -print); tpwd=${tpwd%/*}
    pwd2=$(cat $tpwd/$tc </dev/null)
    echo "Original pwd:  $opwd" >>$tdebug
    echo "Temporary pwd: $tpwd" >>$tdebug
    echo "Addr2 pwd:     $pwd2" >>$tdebug
    if [ "$rc0" -ne 0 ]; then
	$PRINTF "$FAILED (rc=$rc0)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	failed
    elif [ "$tpwd" != "$td" ]; then
	$PRINTF "$FAILED (chdir failed)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	failed
    elif ! echo "$pwd2" |diff "$td/$tc" - >$tdiff; then
	$PRINTF "$FAILED (bad pwd)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the modified umask option, in particular if umask with first address
# (CREATE) does not affect umask of second address, i.e. original umask is
# recovered
NAME=UMASK_ON_CREATE
case "$TESTS" in
*%$N%*|*%functions%*|*%creat%*|*%system%*|*%umask%*|*%$NAME%*)
TEST="$NAME: test restore after CREAT with umask option"
# Run Socat with first address CREAT with modified umask,
# and second address SYSTEM (shell) with umask command
# Check if the file is created with modified umask but shell has original umask
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats CREAT SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs CREAT SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions umask) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tc="$td/test$N.creat"
    tdiff="$td/test$N.diff"
    tdebug="$td/test$N.debug"
    oumask=$(umask)
    # Construct a temp umask differing from original umask
    case oumask in
	*066) tumask=0026 ;;
	*)    tumask=0066 ;;
    esac
    CMD0="$TRACE $SOCAT $opts -U CREAT:$tc,umask=$tumask SYSTEM:umask"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0"
    rc0=$?
    tperms=$(fileperms $tc)
    case $tperms in
	0*) ;;
	*) tperms=0$tperms ;;
    esac
    echo "Original umask:  $oumask" >>$tdebug
    echo "Temporary umask: $tumask" >>$tdebug
    echo "Created umask:   $tperms" >>$tdebug
    echo "Restored umask:  $(cat $tc)" >>$tdebug
    if [ "$rc0" -ne 0 ]; then
	$PRINTF "$FAILED\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	failed
    elif [ $((tumask + tperms - 0666)) -ne 0 ]; then
	$PRINTF "$FAILED (umask failed)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	failed
    elif ! [ "$oumask" -eq $(cat "$tc") ]; then
	$PRINTF "$FAILED (bad umask)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	cat "$tdebug" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test the modified umask option, in particular if umask with first address
# (SHELL) does not affect umask of second address, i.e. original umask is
# recovered
NAME=UMASK_ON_SYSTEM
case "$TESTS" in
*%$N%*|*%functions%*|*%shell%*|*%system%*|*%umask%*|*%socket%*|*%$NAME%*)
TEST="$NAME: test restore after SHELL with umask option"
# Run Socat with first address SHELL:"cat >file" with modified umask,
# and second address SYSTEM (shell) with umask command.
# Check if the file is created with modified umask but shell has original umask
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats SHELL SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs SHELL SYSTEM); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions umask) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tc="$td/test$N.creat"
    tdiff="$td/test$N.diff"
    tdebug="$td/test$N.debug"
    oumask=$(umask)
    # Construct a temp umask differing from original umask
    case oumask in
	*066) tumask=0026 ;;
	*)    tumask=0066 ;;
    esac
    CMD0="$TRACE $SOCAT $opts -U SHELL:\"cat\ >$tc\",umask=$tumask SYSTEM:\"umask; sleep 1\""
    printf "test $F_n $TEST... " $N
    eval "$CMD0" >/dev/null 2>"${te}0"
    rc0=$?
    tperms=$(fileperms $tc)
    case $tperms in
	0*) ;;
	*) tperms=0$tperms ;;
    esac
    echo "Original umask:  $oumask" >>$tdebug
    echo "Temporary umask: $tumask" >>$tdebug
    echo "Created umask:   $tperms" >>$tdebug
    echo "Restored umask:  $(cat $tc)" >>$tdebug
    if [ "$rc0" -ne 0 ]; then
	$PRINTF "$FAILED\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	failed
    elif [ $((tumask + tperms - 0666)) -ne 0 ]; then
	$PRINTF "$FAILED (umask failed)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	failed
    elif ! [ "$oumask" -eq $(cat "$tc") ]; then
	$PRINTF "$FAILED (bad umask)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	cat "$tdebug" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


while read _UNIX _SRV _CLI; do
    if [ -z "$_UNIX" ] || [[ "$_UNIX" == \#* ]]; then continue; fi
SRV=${_UNIX}-$_SRV
CLI=${_UNIX}-$_CLI
CLI_=$(echo $CLI |tr x- x_)
PROTO=${_UNIX}
proto=$(tolower $PROTO)

# Test the unix-bind-tempname option
NAME=${_UNIX}_${_SRV}_${_CLI}_BIND_TEMPNAME
case "$TESTS" in
*%$N%*|*%functions%*|*%$proto%*|*%socket%*|*%tempname%*|*%listen%*|*%fork%*|*%$NAME%*)
TEST="$NAME: Option unix-bind-tempname"
# Start a UNIX domain service with forking
# Start a TCP service with forking that relays to the UNIX domain service
# Open two concurrent client sessions to the TCP service.
# When both sessions work (in particular, when the UNIX domain service does not
# log "Transport endpoint is not connected" and the TCP service does not fail
# with "Address already in use"), the test succeeded.
if ! eval $NUMCOND; then :;
#elif [[ $CLI_ =~ ABSTRACT-* ]] && ! feat=$(testfeats abstract-unixsocket); then
elif re_match "$CLI_" 'ABSTRACT-*' && ! feat=$(testfeats abstract-unixsocket); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$feat not available${NORMAL}\n" $N
    cant
elif ! o=$(testoptions unix-bind-tempname) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
else
ts="$td/test$N.sock"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts -lp server $SRV:${ts}0,fork PIPE"
# Using this command would show the principal problem: UNIX (and ABSTRACT)
# datagram clients do not internally bind to a defined address and thus cannot
# receive replies. Applies to all(?) Linux, (some)FreeBSD, (some)Solaris, others
# not tried
#CMD1="$TRACE $SOCAT $opts -lp bind-tempname TCP4-LISTEN:$PORT,reuseaddr,fork $CLI:${ts}0"
# Attempt to bind the datagram client to some address works, but only for a
# single client; when multiple clients are forked they conflict
# The following command is the solution: option unix-bind-tempname generates
# random names (like tempnam(2)) for binding the datagram client socket;
# creating the XXXXXX file makes sure that the (non abstract) clients cannot
# erroneously bind there (part of the test)
CMD1="$TRACE $SOCAT $opts -lp bind-tempname TCP4-LISTEN:$PORT,reuseaddr,fork $CLI:${ts}0,bind=${ts}1"
touch ${ts}1.XXXXXX; CMD1="$TRACE $SOCAT $opts -lp tempname TCP4-LISTEN:$PORT,reuseaddr,fork $CLI:${ts}0,bind-tempname=${ts}1.XXXXXX"
CMD2="$TRACE $SOCAT $opts -lp client - TCP4-CONNECT:$LOCALHOST:$PORT"
printf "test $F_n $TEST... " $N
$CMD0 2>"${te}0" &
pid0=$!
wait${proto}port ${ts}0 1
$CMD1 2>"${te}1" &
pid1=$!
waittcp4port $PORT 1
{ echo "$da a"; relsleep 2; } |$CMD2 >"${tf}2a" 2>"${te}2a" &
pid2a=$!
relsleep 1
echo "$da b" |$CMD2 >"${tf}2b" 2>"${te}2b"
rc2b=$?
relsleep 1
kill $pid0 $pid1 $pid2a 2>/dev/null; wait
if [ $rc2b -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2 &"
    cat "${te}2a" >&2
    echo "$CMD2"
    cat "${te}2b" >&2
    failed
elif ! echo "$da a" |diff - ${tf}2a >${tdiff}2a; then
    $PRINTF "$FAILED (phase a)\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2"
    cat "${te}2a" >&2
    echo "diff a:" >&2
    cat ${tdiff}2a >&2
    failed
elif ! echo "$da b" |diff - ${tf}2b >${tdiff}2b; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "$CMD1 &"
    cat "${te}1" >&2
    echo "$CMD2 &"
    cat "${te}2a" >&2
    echo "$CMD2"
    cat "${te}2b" >&2
    echo "diff b:" >&2
    cat ${tdiff}2b >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD2"; fi
    if [ "$DEBUG" ];   then cat "${te}2b" >&2; fi
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

done <<<"
UNIX     LISTEN   CONNECT
UNIX     LISTEN   CLIENT
UNIX     RECVFROM CLIENT
UNIX     RECVFROM SENDTO
ABSTRACT LISTEN   CONNECT
ABSTRACT LISTEN   CLIENT
ABSTRACT RECVFROM CLIENT
ABSTRACT RECVFROM SENDTO
"

# Test if OS/libc is not prone to symlink attacks on UNIX bind()
NAME=TEMPNAME_SEC
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%unix%*|*%dgram%*|*%security%*|*%$NAME%*)
TEST="$NAME: test if a symlink attack works against bind()"
# Create a symlink .sock2 pointing to non-existing .sock3
# Start Socat with UNIX-SENDTO...,bind=.sock2
# When .sock3 exists the test failed
if ! eval $NUMCOND; then :; else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
ts1="$td/test$N.sock1"
ts2="$td/test$N.sock2"
ts3="$td/test$N.sock3"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0a="rm -f $ts3"
CMD0b="ln -s $ts3 $ts2"
CMD1="$TRACE $SOCAT $opts UNIX-SENDTO:$ts1,bind=$ts2 PIPE"
rc1=$?
printf "test $F_n $TEST... " $N
$CMD0a
$CMD0b
#echo; ls -l $ts2 $ts3
$CMD1 2>"${te}1" &
pid1=$!
waitunixport $ts1 1 1 2>/dev/null
#res="$(ls -l $ts3 2>/dev/null)"
kill $pid1 2>/dev/null
if [ -e $ts3 ]; then
    $PRINTF "$FAILED\n"
    echo "symlink target has been created" >&2
    echo "$CMD0a" >&2
    cat "${te}0a" >&2
    echo "$CMD0b" >&2
    cat "${te}0b" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
elif ! grep -q " E " ${te}1; then
    $PRINTF "$FAILED\n"
    echo "Socat did not fail"
    echo "$CMD0a" >&2
    cat "${te}0a" >&2
    echo "$CMD0b" >&2
    cat "${te}0b" >&2
    echo "$CMD1" >&2
    cat "${te}1" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0a"; fi
    if [ "$DEBUG" ];   then cat "${te}0a" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD0b"; fi
    if [ "$DEBUG" ];   then cat "${te}0b" >&2; fi
    if [ "$VERBOSE" ]; then echo "$CMD1"; fi
    if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the new f-setpipe-sz option on a STDIN pipe
NAME=STDIN_F_SETPIPE_SZ
case "$TESTS" in
*%$N%*|*%functions%*|*%filan%*|*%dual%*|*%stdio%*|*%exec%*|*%pipe%*|*%f-setpipe-sz%*|*%$NAME%*)
TEST="$NAME: f-setpipe-sz on STDIN"
# Start Socat in a shell pipe and have it calling Filan via EXEC and nofork
# Check Filan output if pipe size of its input pipe is modified.
if ! eval $NUMCOND; then :;
elif ! $(type true >/dev/null 2>&1); then
    $PRINTF "test $F_n $TEST... ${YELLOW}true not available${NORMAL}\n" $N
    cant
elif ! F=$(testfeats STDIO EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions f-setpipe-sz nofork) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
newport tcp4
# Find the default pipe size
PIPESZ="$(echo |$FILAN -n 0 |grep "0:" |head -n 1 |sed 's/.*F_GETPIPE_SZ=\([0-9][0-9]*\).*/\1/')"
PIPESZ2=$((2*PIPESZ))
CMD0="$TRACE $SOCAT $opts STDIN,f-setpipe-sz=$PIPESZ2!!STDOUT EXEC:$FILAN,nofork"
printf "test $F_n $TEST... " $N
true |$CMD0 >"${tf}" 2>"${te}0"
rc0=$?
PIPESZ2b="$(cat "$tf" |grep "0:" |head -n 1 |sed 's/.*F_GETPIPE_SZ=\([0-9][0-9]*\).*/\1/')"
if [ "$rc0" -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0" >&2
    failed
elif ! diff <(echo $PIPESZ2) <(echo $PIPESZ2b) >$tdiff; then
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0" >&2
    echo "diff:" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test the new f-setpipe-sz option on EXEC with pipes
NAME=EXEC_F_SETPIPE_SZ
case "$TESTS" in
*%$N%*|*%functions%*|*%filan%*|*%stdio%*|*%exec%*|*%pipe%*|*%f-setpipe-sz%*|*%$NAME%*)
TEST="$NAME: f-setpipe-sz on EXEC with pipes"
# Start Socat calling Filan via EXEC and pipes and f-setpipe-sz
# Check Filan output if pipe size of both pipes is modified.
if ! eval $NUMCOND; then :;
elif ! F=$(testfeats STDIO EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Feature $F not configured in $SOCAT${NORMAL}\n" $N
    cant
elif ! A=$(testaddrs STDIO EXEC); then
    $PRINTF "test $F_n $TEST... ${YELLOW}Address $A not available in $SOCAT${NORMAL}\n" $N
    cant
elif ! o=$(testoptions pipes f-setpipe-sz) >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option $o not available in $SOCAT${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
# Find the default pipe size
PIPESZ="$(echo |$FILAN -n 0 |grep "0:" |head -n 1 |sed 's/.*F_GETPIPE_SZ=\([0-9][0-9]*\).*/\1/')"
PIPESZ2=$((2*PIPESZ))
CMD0="$TRACE $SOCAT $opts STDIO EXEC:$FILAN,pipes,f-setpipe-sz=$PIPESZ2"
printf "test $F_n $TEST... " $N
$CMD0 >"$tf" 2>"${te}0"
rc0=$?
PIPESZ2b="$(cat "$tf" |grep "0:" |head -n 1 |sed 's/.*F_GETPIPE_SZ=\([0-9][0-9]*\).*/\1/')"
if [ "$rc0" -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0"
    cat "${te}0" >&2
    failed
elif ! diff <(echo $PIPESZ2) <(echo $PIPESZ2b) >$tdiff; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0" >&2
    echo "diff:" >&2
    cat "$tdiff" >&2
    failed
else
    $PRINTF "$OK\n"
    if [ "$VERBOSE" ]; then echo "$CMD0"; fi
    if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
    ok
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


NAME=DCCP4
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%dccp%*|*%listen%*|*%$NAME%*)
TEST="$NAME: DCCP over IPv4"
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds "" "" "" \
			 "IP4 DCCP LISTEN STDIO PIPE" \
			 "DCCP4-LISTEN PIPE STDIN STDOUT DCCP4" \
			 "so-reuseaddr" \
			 "dccp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
newport dccp4; tsl=$PORT
ts="127.0.0.1:$tsl"
da="test$N $(date) $RANDOM"
CMD1="$TRACE $SOCAT $opts DCCP4-LISTEN:$tsl,$REUSEADDR PIPE"
CMD2="$TRACE $SOCAT $opts STDIN!!STDOUT DCCP4:$ts"
printf "test $F_n $TEST... " $N
$CMD1 >"$tf" 2>"${te}1" &
pid1=$!
waittcp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
if [ $? -ne 0 ]; then
	$PRINTF "$FAILED\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
	$PRINTF "$FAILED\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
fi
kill $pid1 2>/dev/null
wait
fi ;; # NUMCOND, checkconds
esac
N=$((N+1))


NAME=UDPLITE4STREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%udplite%*|*%$NAME%*)
TEST="$NAME: echo via connection to UDP-Lite IPv4 socket"
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 UDPLITE LISTEN STDIO PIPE" \
		  "UDPLITE4-LISTEN PIPE STDIO UDPLITE4" \
		  "so-reuseaddr" \
		  "udplite4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    tsl=$PORT
    ts="$LOCALHOST:$tsl"
    da="test$N $(date) $RANDOM"
    CMD1="$TRACE $SOCAT $opts UDPLITE4-LISTEN:$tsl,$REUSEADDR PIPE"
    CMD2="$TRACE $SOCAT $opts - UDPLITE4:$ts"
    printf "test $F_n $TEST... " $N
    $CMD1 >"$tf" 2>"${te}1" &
    pid1=$!
    waitudplite4port $tsl 1
    echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
    rc2=$?
    kill $pid1 2>/dev/null; wait
    if [ $rc2 -ne 0 ]; then
	$PRINTF "$FAILED: $TRACE $SOCAT:\n"
	echo "$CMD1 &"
	cat "${te}1" >&2
	echo "$CMD2"
	cat "${te}2" >&2
	failed
    elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD1 &" >&2
	cat "${te}1"
	echo "$CMD2" >&2
	cat "${te}2" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD2"; fi
	if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
	ok
    fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=UDPLITE4STREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%udplite%*|*%$NAME%*)
TEST="$NAME: echo via connection to UDP-Lite IPv4 socket"
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 UDPLITE LISTEN STDIO PIPE" \
		  "UDPLITE4-LISTEN PIPE STDIO UDPLITE4" \
		  "so-reuseaddr" \
		  "udplite4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    tsl=$PORT
    ts="$LOCALHOST:$tsl"
    da="test$N $(date) $RANDOM"
    CMD1="$TRACE $SOCAT $opts UDPLITE4-LISTEN:$tsl,$REUSEADDR PIPE"
    CMD2="$TRACE $SOCAT $opts - UDPLITE4:$ts"
    printf "test $F_n $TEST... " $N
    $CMD1 >"$tf" 2>"${te}1" &
    pid1=$!
    waitudplite4port $tsl 1
    echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
    rc2=$?
    kill $pid1 2>/dev/null; wait
    if [ $rc2 -ne 0 ]; then
	$PRINTF "$FAILED: $TRACE $SOCAT:\n"
	echo "$CMD1 &"
	cat "${te}1" >&2
	echo "$CMD2"
	cat "${te}2" >&2
	failed
    elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD1 &" >&2
	cat "${te}1"
	echo "$CMD2" >&2
	cat "${te}2" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD2"; fi
	if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
	ok
    fi
fi ;; # NUMCOND
esac
N=$((N+1))


NAME=UDPLITE6STREAM
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%ipapp%*|*%udplite%*|*%$NAME%*)
TEST="$NAME: echo via connection to UDP-Lite IPv6 socket"
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP6 UDPLITE LISTEN STDIO PIPE" \
		  "UDPLITE6-LISTEN PIPE STDIO UDPLITE6" \
		  "so-reuseaddr" \
		  "udplite6" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    tsl=$PORT
    ts="$LOCALHOST6:$tsl"
    da="test$N $(date) $RANDOM"
    CMD1="$TRACE $SOCAT $opts UDPLITE6-LISTEN:$tsl,$REUSEADDR PIPE"
    CMD2="$TRACE $SOCAT $opts - UDPLITE6:$ts"
    printf "test $F_n $TEST... " $N
    $CMD1 >"$tf" 2>"${te}1" &
    pid1=$!
    waitudplite6port $tsl 1
    echo "$da" |$CMD2 >>"$tf" 2>>"${te}2"
    rc2=$?
    kill $pid1 2>/dev/null; wait
    if [ $rc2 -ne 0 ]; then
	$PRINTF "$FAILED: $TRACE $SOCAT:\n"
	echo "$CMD1 &"
	cat "${te}1" >&2
	echo "$CMD2"
	cat "${te}2" >&2
	failed
    elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD1 &" >&2
	cat "${te}1"
	echo "$CMD2" >&2
	cat "${te}2" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD2"; fi
	if [ "$DEBUG" ];   then cat "${te}2" >&2; fi
	ok
    fi
fi ;; # NUMCOND
esac
N=$((N+1))


# test: setting of environment variables that describe a stream socket
# connection: SOCAT_SOCKADDR, SOCAT_PEERADDR; and SOCAT_SOCKPORT,
# SOCAT_PEERPORT when applicable
while read KEYW FEAT SEL TEST_SOCKADDR TEST_PEERADDR PORTMETHOD; do
if [ -z "$KEYW" ] || [[ "$KEYW" == \#* ]]; then continue; fi
#
protov="$(echo "$KEYW" |tr A-Z a-z)"
proto="${protov%%[0-9]}"
NAME=${KEYW}LISTENENV
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%ipapp%*|*%$SEL%*|*%$proto%*|*%$protov%*|*%envvar%*|*%listen%*|*%$NAME%*)
TEST="$NAME: $KEYW-LISTEN sets environment variables with socket addresses"
# have a server accepting a connection and invoking some shell code. The shell
# code extracts and prints the SOCAT related environment vars.
# outside code then checks if the environment contains the variables correctly
# describing the peer and local sockets.
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "$FEAT $(echo $SEL |tr a-z A-Z) STDIO SYSTEM" \
		  "$KEYW-LISTEN SYSTEM STDIO $KEYW-CONNECT" \
		  "$REUSEADDR bind" \
		  "$protov" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
TEST_SOCKADDR="$(echo "$TEST_SOCKADDR" |sed "s/\$N/$N/g")"	# actual vars
tsa="$TEST_SOCKADDR"	# test server address
if [ "$PORTMETHOD" = PORT ]; then
    newport $proto; tsp="$PORT"; 	# test server port
    tsa1="$tsp"; tsa2="$tsa"; tsa="$tsa:$tsp"	# tsa2 used for server bind=
    TEST_SOCKPORT=$tsp
else
    tsa1="$tsa"; tsa2=				# tsa1 used for addr parameter
fi
TEST_PEERADDR="$(echo "$TEST_PEERADDR" |sed "s/\$N/$N/g")"	# actual vars
tca="$TEST_PEERADDR"	# test client address
if [ "$PORTMETHOD" = PORT ]; then
    newport $proto; tcp="$PORT"; 	# test client port
    tca="$tca:$tcp"
    TEST_PEERPORT=$tcp
fi
#CMD0="$TRACE $SOCAT $opts -u $KEYW-LISTEN:$tsa1 SYSTEM:\"export -p\""
CMD0="$TRACE $SOCAT $opts -u -lpsocat $KEYW-LISTEN:$tsa1,$REUSEADDR SYSTEM:\"echo SOCAT_SOCKADDR=\\\$SOCAT_SOCKADDR; echo SOCAT_PEERADDR=\\\$SOCAT_PEERADDR; echo SOCAT_SOCKPORT=\\\$SOCAT_SOCKPORT; echo SOCAT_PEERPORT=\\\$SOCAT_PEERPORT; sleep 1\""
CMD1="$TRACE $SOCAT $opts -u - $KEYW-CONNECT:$tsa,bind=$tca"
printf "test $F_n $TEST... " $N
eval "$CMD0 2>\"${te}0\" >\"$tf\" &"
pid0=$!
wait${protov}port $tsa1 1
{ echo; sleep 0.1; } |$CMD1 2>"${te}1"
rc1=$?
waitfile "$tf" 2
kill $pid0 2>/dev/null; wait
#set -vx
if [ $rc1 != 0 ]; then
    $PRINTF "$NO_RESULT (client failed):\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1"
    cat "${te}1"
    cant
elif [ "$(grep SOCAT_SOCKADDR "${tf}" |sed -e 's/^[^=]*=//' |sed -e "s/[\"']//g")" = "$TEST_SOCKADDR" -a \
    "$(grep SOCAT_PEERADDR "${tf}" |sed -e 's/^[^=]*=//' -e "s/[\"']//g")" = "$TEST_PEERADDR" -a \
    \( "$PORTMETHOD" = ',' -o "$(grep SOCAT_SOCKPORT "${tf}" |sed -e 's/^[^=]*=//' |sed -e 's/"//g')" = "$TEST_SOCKPORT" \) -a \
    \( "$PORTMETHOD" = ',' -o "$(grep SOCAT_PEERPORT "${tf}" |sed -e 's/^[^=]*=//' |sed -e 's/"//g')" = "$TEST_PEERPORT" \) \
    ]; then
    $PRINTF "$OK\n"
    if [ "$debug" ]; then
	echo "$CMD0 &"
	cat "${te}0"
	echo "$CMD1"
	cat "${te}1"
    fi
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    cat "${te}0"
    echo "$CMD1"
    cat "${te}1"
    echo -e "SOCAT_SOCKADDR=$TEST_SOCKADDR\nSOCAT_PEERADDR=$TEST_PEERADDR\nSOCAT_SOCKPORT=$TEST_SOCKPORT\nSOCAT_PEERPORT=$TEST_PEERPORT" |
    diff - "${tf}"
    failed
fi
fi # NUMCOND, feats
 ;;
esac
N=$((N+1))
#set +xv
#
done <<<"
UDPLITE4 IP4  udplite 127.0.0.1                                 $SECONDADDR                               PORT
UDPLITE6 IP6  udplite [0000:0000:0000:0000:0000:0000:0000:0001] [0000:0000:0000:0000:0000:0000:0000:0001] PORT
"


# test the max-children option on pseudo connected sockets
while read KEYW FEAT SEL ADDR IPPORT SHUT; do
if [ -z "$KEYW" ] || [[ "$KEYW" == \#* ]]; then continue; fi
RUNS=$(tolower $KEYW)
PROTO=$KEYW
proto="$(tolower "$PROTO")"
# test the max-children option on pseudo connected sockets
NAME=${KEYW}_L_MAXCHILDREN
case "$TESTS" in
*%$N%*|*%functions%*|*%fork%*|*%maxchildren%*|*%$SEL%*|*%socket%*|*%listen%*|*%$NAME%*)
TEST="$NAME: max-children option with $PROTO-LISTEN"
# start a listen process with max-children=1; connect with a client, let it
# send data and then sleep; connect with second client that wants to send
# data immediately, but keep first client active until server terminates.
#If max-children is working correctly only the first data should
# arrive.
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "$FEAT IP${KEYW##*[A-Z]} FILE STDIO" \
		  "FILE $PROTO-LISTEN STDIO $KEYW-CONNECT" \
		  "$REUSEADDR o-trunc o-creat o-append fork max-children $SHUT" \
		  "$RUNS" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
case "X$IPPORT" in
    "XPORT")
    newport $proto
    tsl=$PORT 		# test socket listen address
    tsc="$ADDR:$PORT"	# test socket connect address
    ;;
    *)
    tsl="$(eval echo "$ADDR")"	# resolve $N
    tsc=$tsl
esac
#ts="$td/test$N.sock"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
# on some Linux distributions it hangs, thus -T option here
CMD0="$TRACE $SOCAT $opts -U -T 4 FILE:$tf,o-trunc,o-creat,o-append $PROTO-LISTEN:$tsl,$REUSEADDR,fork,max-children=1"
CMD1="$TRACE $SOCAT $opts -u - $PROTO-CONNECT:$tsc,$SHUT"
printf "test $F_n $TEST... " $N
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
wait${proto}port $tsl 1
(echo "$da 1"; relsleep 3) |$CMD1 >"${tf}1" 2>"${te}1" &
pid1=$!
relsleep 1
echo "$da 2" |$CMD1 >"${tf}2" 2>"${te}2" &
pid2=$!
relsleep 1
cpids="$(childpids $pid0)"
kill $pid1 $pid2 $cpids $pid0 2>/dev/null; wait
if echo -e "$da 1" |diff - $tf >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "(echo \"$da 1\"; sleep 2) |$CMD1"
    echo "echo \"$da 2\" |$CMD1"
    cat "${te}0"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
    failed
fi
fi # NUMCOND
 ;;
esac
N=$((N+1))
done <<<"
UDPLITE4  UDPLITE  udplite  127.0.0.1 PORT shut-null
UDPLITE6  UDPLITE  udplite  [::1]     PORT shut-null
"


# Test the procan controlling terminal output
NAME=PROCAN_CTTY
case "$TESTS" in
*%$N%*|*%functions%*|*%procan%*|*%$NAME%*)
TEST="$NAME: test procan controlling terminal output"
# Run procan and compare its controlling terminal output with tty (oops)"
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "tty" \
		  "" \
		  "" \
		  "" \
		  "" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    CMD0="$TRACE $PROCAN"
    printf "test $F_n $TEST... " $N
    $CMD0 >"${tf}0" 2>"${te}0"
    rc0=$?
    if [ "$rc0" -ne 0 ]; then
	$PRINTF "$FAILED\n"
	echo "$CMD0"
	cat "${te}0" >&2
	failed
    elif ! tty |diff - <(cat ${tf}0 |grep "controlling terminal" |grep -v -e '"/dev/tty"' -e none |head -n 1 |sed -e 's/controlling terminal by .*:[[:space:]]*//' -e 's/"//g') >$tdiff; then
	$PRINTF "$FAILED\n"
	echo "$CMD0"
	cat "${te}0" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the socat-chain.sh script with SOCKS4 over UNIX-socket
NAME=SOCAT_CHAIN_SOCKS4
case "$TESTS" in
*%$N%*|*%functions%*|*%scripts%*|*%socat-chain%*|*%listen%*|*%fork%*|*%ip4%*|*%tcp4%*|*%unix%*|*%socks4%*|*%socket%*|*%$NAME%*)
TEST="$NAME: test socat-chain.sh with SOCKS4 over UNIX-socket"
# Run a socks4 server on UNIX-listen
# Connect with socat-chain.sh; check if data transfer is correct
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 TCP LISTEN STDIO UNIX SOCKS4" \
		  "TCP4-LISTEN PIPE STDIN STDOUT TCP4 UNIX UNIX-LISTEN" \
		  "so-reuseaddr" \
		  "tcp4 unix" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    ts="$td/test$N.sock"
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    CMD0="$TRACE $SOCAT $opts UNIX-LISTEN:$ts,reuseaddr EXEC:./socks4echo.sh"
    CMD1="$TRACE ./socat-chain.sh $opts - SOCKS4::32.98.76.54:32109,socksuser=nobody UNIX:$ts"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    waitunixport $ts 1
    #relsleep 1 	# if no matching wait*port function
    echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif ! echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test the socat-chain.sh script by driving SSL over serial
NAME=SOCAT_CHAIN_SSL_PTY
case "$TESTS" in
*%$N%*|*%functions%*|*%scripts%*|*%socat-chain%*|*%listen%*|*%fork%*|*%ip4%*|*%tcp4%*|*%openssl%*|*%unix%*|*%socket%*|*%pty%*|*%$NAME%*)
TEST="$NAME: test socat-chain.sh with SSL over PTY"
# Run a socat-chain.sh instance with SSL listening behind a PTY;
# open the PTY with socat-chain.sh using SSL;
# check if data transfer is correct
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 TCP LISTEN OPENSSL STDIO PTY" \
		  "TCP4-LISTEN SOCKETPAIR STDIN STDOUT TCP4 SSL SSL-L" \
		  "so-reuseaddr" \
		  "tcp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
elif re_match "$BASH_VERSION" '^[1-3]\.'; then
    $PRINTF "test $F_n $TEST... ${YELLOW}requires bash 4 or higher${NORMAL}\n" $N
    cant
else
    gentestcert testsrv
    tp="$td/test$N.pty"
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    CMD0="$TRACE ./socat-chain.sh $opts PTY,link=$tp SSL-L,cert=testsrv.pem,verify=0 SOCKETPAIR"
    CMD1="$TRACE ./socat-chain.sh $opts - SSL,cafile=testsrv.crt,commonname=localhost $tp,cfmakeraw"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    waittcp4port $tp
    # NetBSD-9 seems to need massive delay
    { echo "$da"; relsleep 100; } |$CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null
    wait 2>/dev/null
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif ! echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the socat-mux.sh script
# Requires lo/lo0 to have broadcast address 127.255.255.255
NAME=SOCAT_MUX
case "$TESTS" in
*%$N%*|*%functions%*|*%script%*|*%socat-mux%*|*%socket%*|*%udp%*|*%broadcast%*|*%$NAME%*)
TEST="$NAME: test the socat-mux.sh script"
# Start a simple TCP server
# Start socat-mux.sh to connect to this server
# Connect with two clients to mux, send different data records from both.
# Check if both clients received both records in order.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 TCP LISTEN STDIO UNIX" \
		  "TCP4-LISTEN PIPE STDIN STDOUT TCP4 UNIX UNIX-LISTEN" \
		  "so-reuseaddr" \
		  "tcp4 unix" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    newport tcp4
    PORT0=$PORT
    newport tcp4
    PORT1=$PORT
    CMD0="$TRACE $SOCAT $opts -lp server TCP-LISTEN:$PORT0 PIPE"
    CMD1="./socat-mux.sh $opts TCP-LISTEN:$PORT1 TCP-CONNECT:$LOCALHOST:$PORT0"
    CMD2="$TRACE $SOCAT $opts -lp client STDIO TCP:$LOCALHOST:$PORT1"
    da_a="test$N $(date) $RANDOM"
    da_b="test$N $(date) $RANDOM"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    waittcp4port $PORT0 1
    $CMD1 >/dev/null 2>"${te}1" &
    pid1=$!
    waittcp4port $PORT1 1
    { relsleep 10; echo "$da_a"; relsleep 20; } </dev/null |$CMD2 >"${tf}2a" 2>"${te}2a" &
    pid2a=$!
    { relsleep 20; echo "$da_b"; relsleep 10; } |$CMD2 >"${tf}2b" 2>"${te}2b"
    rc2b=$?
    kill $pid0 $(childpids $pid1) $pid1 2>/dev/null
    wait 2>/dev/null
    kill $pid0 2>/dev/null; wait
    if [ "$rc2b" -ne 0 ]; then
	$PRINTF "$FAILED (rc2b=$rc2b)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1 &"
	cat "${te}1" >&2
	echo "{ relsleep 10; echo \"\$da_a\"; relsleep 20; } |$CMD2 &"
	cat "${te}2a" >&2
	echo "{ relsleep 20; echo \"\$da_b\"; relsleep 10; } |$CMD2"
	cat "${te}2b" >&2
	failed
    elif ! $ECHO "$da_a\n$da_b" |diff - "${tf}2a" >${tdiff}_a; then
	$PRINTF "$FAILED (diff a)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1 &"
	cat "${te}1" >&2
	echo "{ relsleep 10; echo \"\$da_a\"; relsleep 20; } |$CMD2 &"
	cat "${te}2a" >&2
	echo "{ relsleep 20; echo \"\$da_b\"; relsleep 10; } |$CMD2"
	cat "${te}2b" >&2
	echo "// diff a:" >&2
	cat "${tdiff}_a" >&2
	failed
    elif ! $ECHO "$da_a\n$da_b" |diff - "${tf}2b" >${tdiff}_b; then
	$PRINTF "$FAILED (diff b)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1 &"
	cat "${te}1" >&2
	echo "{ relsleep 10; echo \"\$da_a\"; relsleep 20; } |$CMD2 &"
	cat "${te}2a" >&2
	echo "{ relsleep 20; echo \"\$da_b\"; relsleep 10; } |$CMD2"
	cat "${te}2b" >&2
	echo "// diff b:" >&2
	cat "${tdiff}_b" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	if [ "$VERBOSE" ]; then echo "{ relsleep 10; echo \"\$da_a\"; relsleep 20; } |$CMD2 &"; fi
	if [ "$DEBUG" ];   then cat "${te}2a" >&2; fi
	if [ "$VERBOSE" ]; then echo "{ relsleep 20; echo \"\$da_b\"; relsleep 10; } |$CMD2"; fi
	if [ "$DEBUG" ];   then cat "${te}2b" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test the socat-broker.sh script
# Requires lo/lo0 to have broadcast address 127.255.255.255
NAME=SOCAT_BROKER
case "$TESTS" in
*%$N%*|*%functions%*|*%script%*|*%socat-broker%*|*%socket%*|*%udp%*|*%broadcast%*|*%$NAME%*)
TEST="$NAME: test the socat-broker.sh script"
# Start a socat-broker.sh instance
# Connect with two clients, send different data records from both.
# Check if both client received both records in order.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 UDP TCP LISTEN STDIO" \
		  "TCP4-LISTEN TCP4-CONNECT STDIO UDP-DATAGRAM" \
		  "so-reuseaddr" \
		  "udp4 tcp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    newport tcp4
    CMD0="$TRACE ./socat-broker.sh $OPTS TCP4-LISTEN:$PORT"
    CMD1="$TRACE $SOCAT $OPTS - TCP:$LOCALHOST:$PORT"
    da_a="test$N $(date) $RANDOM"
    da_b="test$N $(date) $RANDOM"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    waittcp4port $PORT 1
    { relsleep 10; echo "$da_a"; relsleep 20; } </dev/null |$CMD1 >"${tf}1a" 2>"${te}1a" &
    pid1a=$!
    { relsleep 20; echo "$da_b"; relsleep 10; } |$CMD1 >"${tf}1b" 2>"${te}1b"
    rc1b=$?
    kill $(childpids $pid0) $pid0 $pid1a 2>/dev/null
    wait 2>/dev/null
    #kill $pid0 2>/dev/null; wait
    if [ "$rc1b" -ne 0 ]; then
	$PRINTF "$FAILED (rc1b=$rc1b)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "{ relsleep 10; echo \"\$da_a\"; relsleep 20; } |$CMD1"
	cat "${te}1a" >&2
	echo "{ relsleep 20; echo \"\$da_b\"; relsleep 10; } |$CMD1"
	cat "${te}1b" >&2
	failed
    elif ! $ECHO "$da_a\n$da_b" |diff - "${tf}1a" >${tdiff}_a; then
	$PRINTF "$FAILED (diff a)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "{ relsleep 10; echo \"\$da_a\"; relsleep 20; } |$CMD1"
	cat "${te}1a" >&2
	echo "{ relsleep 20; echo \"\$da_b\"; relsleep 10; } |$CMD1"
	cat "${te}1b" >&2
	echo "// diff a:" >&2
	cat "${tdiff}_a" >&2
	failed
    elif ! $ECHO "$da_a\n$da_b" |diff - "${tf}1b" >${tdiff}_b; then
	$PRINTF "$FAILED (diff b)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "{ relsleep 10; echo \"\$da_a\"; relsleep 20; } |$CMD1"
	cat "${te}1a" >&2
	echo "{ relsleep 20; echo \"\$da_b\"; relsleep 10; } |$CMD1"
	cat "${te}1b" >&2
	echo "// diff b:" >&2
	cat "${tdiff}_b" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "{ relsleep 10; echo \"\$da_a\"; relsleep 20; } |$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1a" >&2; fi
	if [ "$VERBOSE" ]; then echo "{ relsleep 20; echo \"\$da_b\"; relsleep 10.; } |$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1b" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Socat 1.8.0.0 with addresses of type RECVFROM and option fork entered a
# loop that was only stopped by FD exhaustion cause by FD leak, when the
# second address failed to connect/open in the child process
NAME=RECVFROM_FORK_LOOP
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%ip4%*|*%udp%*|*%udp4%*|*%fork%*|*%socket%*|*%$NAME%*)
TEST="$NAME: Bug on RECVFROM with fork and child failure"
# Start a Socat process that uses UDP4-RECFROM with fork options, and in the
# second address opens a file in a non existent directory.
# Send a UDP4-packet to the receiver.
# When only one child process is forked off, thus when only one appropriate
# error message is in the log file, the test succeeded.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 UDP STDIO FILE" \
		  "UDP4-RECVFROM OPEN STDIO UDP4-SEND" \
		  "fork" \
		  "udp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    newport udp4
    CMD0="$TRACE $SOCAT $opts UDP4-RECVFROM:$PORT,fork OPEN:$td/nonexistent/file"
    CMD1="$TRACE $SOCAT $opts - UDP4-SENDTO:$LOCALHOST4:$PORT"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    waitudp4port $PORT 1
    echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$CANT (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "echo \$da\" |$CMD1"
	cat "${te}1" >&2
	cant
    elif [ $(grep -c " E open(" "${te}0") -eq 0 ]; then
	$PRINTF "$CANT (no error)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "echo \$da\" |$CMD1"
	cat "${te}1" >&2
	cant
    elif [ $(grep -c " E open(" "${te}0") -ge 2 ]; then
	$PRINTF "$FAILED (this bug)\n"
	echo "$CMD0 &"
	head -n 2 "${te}0" >&2
	echo "echo \$da\" |$CMD1"
	cat "${te}1" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Socat 1.8.0.0 with addresses of type RECVFROM and option fork had a file
# descriport leak that could lead to FD exhaustion.
NAME=RECVFROM_FORK_LEAK
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%ip4%*|*%udp%*|*%udp4%*|*%fork%*|*%socket%*|*%$NAME%*)
TEST="$NAME: FD leak on RECVFROM with fork"
# Start a Socat process that uses UDP4-RECFROM with fork option.
# Send two UDP4-packets to the receiver.
# Check the server logs: when the socketpair calls on the second packet returns
# the same FDs as the first call, the test succeeded.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 UDP FILE STDIO" \
		  "UDP4-RECVFROM FILE STDIO UDP4-SENDTO" \
		  "fork" \
		  "udp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    newport udp4
    CMD0="$TRACE $SOCAT $opts -ddd UDP4-RECVFROM:$PORT,fork FILE:/dev/null"
    CMD1="$TRACE $SOCAT $opts - UDP4-SEND:$LOCALHOST4:$PORT"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    waitudp4port $PORT 1
    echo "$da a" |$CMD1 >"${tf}1a" 2>"${te}1a"
    echo "$da b" |$CMD1 >"${tf}1b" 2>"${te}1b"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$CANT (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "echo \$da a\" |$CMD1"
	cat "${te}1a" >&2
	echo "echo \$da b\" |$CMD1"
	cat "${te}1b" >&2
	cant
    elif [ $(grep -c " I socketpair(" "${te}0") -ne 2 ]; then
	$PRINTF "$CANT (not 2 socketpair())\n"
	echo "$CMD0 &"
	#cat "${te}0" >&2
	echo "echo \$da a\" |$CMD1"
	cat "${te}1a" >&2
	echo "echo \$da b\" |$CMD1"
	cat "${te}1b" >&2
	cant
    elif ! diff <(grep " I socketpair(" "${te}0" |head -n 1 |sed 's/.*\( I socketpair.*\)/\1/') <(grep " I socketpair(" "${te}0" |tail -n 1 |sed 's/.*\( I socketpair.*\)/\1/') >/dev/null 2>&1; then
	$PRINTF "$FAILED (this bug)\n"
	echo "$CMD0 &"
	grep " I socketpair(" "${te}0" >&2
	echo "echo \$da a\" |$CMD1"
	cat "${te}1a" >&2
	echo "echo \$da b\" |$CMD1"
	cat "${te}1b" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "echo \$da a\" |$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1a" >&2; fi
	if [ "$VERBOSE" ]; then echo "echo \$da b\" |$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1b" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test for a bug (up to 1.8.0.0) with IP-SENDTO and option pf (protocol-family)
# with protocol name (vs.numeric)
NAME=IP_SENDTO_PF
case "$TESTS" in
*%$N%*|*%functions%*|*%root%*|*%bugs%*|*%socket%*|*%ip4%*|*%rawip%*|*%gopen%*|*%$NAME%*)
TEST="$NAME: test IP-SENDTO with option pf with protocol name"
# Invoke Socat with address IP-SENDTO with option pf=ip4
# When this works the test succeeded; when an error (in particular:
# E retropts_int(): trailing garbage in numerical arg of option "protocol-family")
# occurs, the test fails
if ! eval $NUMCOND; then :
# Remove unneeded checks, adapt lists of the remaining ones
elif ! cond=$(checkconds \
		  "" \
		  "root" \
		  "" \
		  "IP4 RAWIP GOPEN" \
		  "GOPEN IP-SENDTO" \
		  "pf" \
		  "ip4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    CMD0="$TRACE $SOCAT $opts -u /dev/null IP-SENDTO:127.0.0.1:254,pf=ip4"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0"
    rc0=$?
    if [ "$rc0" -ne 0 ]; then
	$PRINTF "$FAILED (rc0=$rc0)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# DEVTESTS IPv4/IPv6 resolver tests: just manually:

# Prepare:
#socat TCP4-LISTEN:12345,reuseaddr,fork PIPE
# These must succeed:
#echo AAAA |socat - TCP4:localhost-4.dest-unreach.net:12345
#echo AAAA |socat - TCP4:localhost-4-6.dest-unreach.net:12345
#echo AAAA |socat - TCP4:localhost-6-4.dest-unreach.net:12345

# Prepare:
#socat TCP6-LISTEN:12345,reuseaddr,fork PIPE
# These must succeed:
#echo AAAA |socat - TCP6:localhost-6.dest-unreach.net:12345
#echo AAAA |socat - TCP6:localhost-6-4.dest-unreach.net:12345
#echo AAAA |socat - TCP6:localhost-4-6.dest-unreach.net:12345

# These must fail with No address associated with hostname
#socat - TCP4:localhost-6.dest-unreach.net:12345
#socat - TCP6:localhost-4.dest-unreach.net:12345


# Is option -0 available?
opt0=
if SOCAT_MAIN_WAIT= $SOCAT -h |grep -e '[[:space:]]-0[[:space:]]' >/dev/null; then
    opt0=-0
fi

# Test if Socat TCP-L without special options and env accepts IPv4 connections.
# This is traditional behaviour, but version 1.8.0.0 did this only on Linux.
NAME=LISTEN_4
case "$TESTS" in
*%$N%*|*%functions%*|*%ip4%*|*%tcp4%*|*%listen%*|*%socket%*|*%$NAME%*)
TEST="$NAME: TCP-L with -0 accepts IPv4"
# Start a listener with TCP-L, check if TCP4-CONNECT succeeds
if ! eval $NUMCOND; then :
elif [ -z "$opt0" -a $SOCAT_VERSION != 1.8.0.0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option -0 not available${NORMAL}\n" $N
    cant
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 TCP LISTEN FILE" \
		  "TCP-LISTEN TCP4-CONNECT FILE" \
		  "" \
		  "tcp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    opt0=
    if SOCAT_MAIN_WAIT= $SOCAT -h |grep -e '[[:space:]]-0[[:space:]]' >/dev/null; then
	opt0=-0
    fi
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    newport tcp4
    CMD0="$TRACE $SOCAT $opts -u $opt0 TCP-LISTEN:$PORT FILE:/dev/null"
    CMD1="$TRACE $SOCAT $opts -u FILE:/dev/null TCP4-CONNECT:$LOCALHOST4:$PORT"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    relsleep 10
    $CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if [ "$rc1" -ne 0 ]; then
      case "$UNAME" in
      FreeBSD|NetBSD|OpenBSD)
	$PRINTF "${GREEN}FAILED${NORMAL} (by design not on BSD)\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok ;;
      Linux)
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed ;;
      *)
	  $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
	  cant ;;
      esac
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test if Socat TCP-L without special options and env accepts IPv6 connections.
# This is a nice-to-have behaviour, it might only work on Linux.
NAME=LISTEN_6
case "$TESTS" in
*%$N%*|*%functions%*|*%ip6%*|*%tcp%*|*%listen%*|*%socket%*|*%$NAME%*)
TEST="$NAME: TCP-L with -0 accepts IPv6"
# Start a listener with TCP-L, check if TCP6-CONNECT succeeds
if ! eval $NUMCOND; then :
elif [ -z "$opt0" -a $SOCAT_VERSION != 1.8.0.0 ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Option -0 not available${NORMAL}\n" $N
    cant
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP6 TCP LISTEN FILE" \
		  "TCP-LISTEN TCP6-CONNECT FILE" \
		  "ai-addrconfig" \
		  "tcp6" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    newport tcp6
    CMD0="$TRACE $SOCAT $opts -u $opt0 TCP-LISTEN:$PORT,ai-addrconfig=0 FILE:/dev/null"
    CMD1="$TRACE $SOCAT $opts -u FILE:/dev/null TCP6-CONNECT:$LOCALHOST6:$PORT"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    relsleep 10
    $CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

BIN_TIMEOUT=
if type timeout >/dev/null 2>&1; then
    BIN_TIMEOUT=timeout
fi

# Test regression in 1.8.0.0 of passive IP addresses without explicit IP version
# with options range and bind using IPv4 addresses
while read ADDR protov IPPORT ACCEPT_TIMEOUT option _; do
if [ -z "$ADDR" ] || [[ "$ADDR" == \#* ]]; then continue; fi
[ "$ACCEPT_TIMEOUT" = "." ] && ACCEPT_TIMEOUT=""
FEATS=
case "$ADDR" in *-LISTEN|*-L) FEATS=LISTEN ;; esac
ADDR_="$(echo $ADDR |tr - _)"
PROTO="${ADDR%%-*}"
proto=$(tolower $PROTO)
OPTION=$(toupper $option)
FEATS="$FEATS $PROTO"
case "$PROTO" in OPENSSL*|SSL*) PROTO=TCP ;; esac
OPTKW="${OPTION%%=**}"
#
NAME="$(echo "V1800_${ADDR_}_${OPTION%%=*}" |sed 's/:[.0-8]*//')"
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%ip4%*|*%$protov%*|*$proto%*|*%socket%*|*%$NAME%*)
TEST="$NAME: test regression of $ADDR with IPv4 $OPTKW"
# Start a command with the given address and use bind or range with IPv4
# address, terminate immediately. When no error occurs the test succeeded.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "$([ $IPPORT = PROTO ] && echo root)" \
		  "" \
		  "$FEATS IP4 PIPE" \
		  "$ADDR PIPE" \
		  "${option%%=*}" \
		  "$(tolower $PROTO)4 $(tolower $PROTO)6" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    case X$IPPORT in
	XPORT)  newport $(tolower $PROTO); _PORT=$PORT ;;
	XPROTO) #echo "IPPROTO=\"$IPPROTO\""
		_PORT=$IPPROTO ;;
    esac
    CMD0="$TRACE $SOCAT $opts ${ADDR}:$_PORT,$option,$ACCEPT_TIMEOUT PIPE"
    printf "test $F_n $TEST... " $N
    if [ -z "$ACCEPT_TIMEOUT" ] && [ -z "$BIN_TIMEOUT" ]; then
	$PRINTF "$CANT (would block)\n"
	cant
    else
      if [ "$BIN_TIMEOUT" ]; then
	$BIN_TIMEOUT 0.1 $CMD0 >/dev/null 2>"${te}0"
      else
	$CMD0 >/dev/null 2>"${te}0"
      fi
    rc0=$?
    # rc0=124 is SIGALRM from timeout, is success
    if [ "$rc0" -ne 0 -a "$rc0" -ne 124 ]; then
	$PRINTF "$FAILED (rc0=$rc0)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
    fi
    fi 	# not would block
fi # NUMCOND
 ;;
esac
N=$((N+1))
done <<<"
TCP-LISTEN               tcp4     PORT  accept-timeout=0.001 range=localhost:255.255.255.255
TCP-LISTEN               tcp4     PORT  accept-timeout=0.001 bind=127.0.0.1  # works
SCTP-LISTEN              sctp4    PORT  accept-timeout=0.001 range=localhost:255.255.255.255
SCTP-LISTEN              sctp4    PORT  accept-timeout=0.001 bind=127.0.0.1  # works
DCCP-LISTEN              dccp4    PORT  accept-timeout=0.001 range=localhost:255.255.255.255
DCCP-LISTEN              dccp4    PORT  accept-timeout=0.001 bind=127.0.0.1  # works
OPENSSL-LISTEN           tcp4     PORT  accept-timeout=0.001 range=localhost:255.255.255.255
OPENSSL-LISTEN           tcp4     PORT  accept-timeout=0.001 bind=127.0.0.1  # works
UDP-LISTEN               udp4     PORT  .                    range=127.0.0.1/8
UDP-LISTEN               udp4     PORT  .                    bind=127.0.0.1
UDP-RECVFROM             udp4     PORT  .                    range=127.0.0.1/8
UDP-RECVFROM             udp4     PORT  .                    bind=127.0.0.1
UDPLITE-LISTEN           udplite4 PORT  .                    range=127.0.0.1/8
UDPLITE-LISTEN           udplite4 PORT  .                    bind=127.0.0.1
UDPLITE-RECVFROM         udplite4 PORT  .                    range=127.0.0.1/8
UDPLITE-RECVFROM         udplite4 PORT  .                    bind=127.0.0.1
UDP-DATAGRAM:1.2.3.4     udp4     PORT  .                    range=127.0.0.1/8
UDP-DATAGRAM:1.2.3.4     udp4     PORT  .                    bind=127.0.0.1
UDPLITE-DATAGRAM:1.2.3.4 udplite4 PORT  .                    range=127.0.0.1/8
UDPLITE-DATAGRAM:1.2.3.4 udplite4 PORT  .                    bind=127.0.0.1
IP-DATAGRAM:1.2.3.4      ip4      PROTO .                    range=127.0.0.1/8
IP-DATAGRAM:1.2.3.4      ip4      PROTO .                    bind=127.0.0.1
"


# Test if datagram SENDTO to a server name that resolves to IPv6 first and IPv4
# as second address, binding to an IPv4 address, uses IPv4
# This failed in Socat 1.8.0.0
while read ADDR protov IPPORT _; do
if [ -z "$ADDR" ] || [[ "$ADDR" == \#* ]]; then continue; fi
FEATS=
ADDR_="$(echo $ADDR |tr - _)" 	# UDP_SENDTO
PROTO="${ADDR%%-*}" 		# UDP
proto=$(tolower $PROTO) 	# udp
FEATS="$FEATS $PROTO"
NAME="$(echo "V1800_${ADDR_}_RESOLV_6_4" |sed 's/:[.0-8]*//')"
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%ip4%*|*%$protov%*|*%$proto%*|*%socket%*|*%$NAME%*)
TEST="$NAME: test regression of $ADDR with IPv6,4 and binding to IPv4"
# Start a SENDTO command to (internal) test name localhost-6-4.dest-unreach.net
# and bind to an IPv4 address, and terminate immediately.
# When no error occurs the test succeeded.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "$([ $IPPORT = PROTO ] && echo root)" \
		  "" \
		  "$FEATS DEVTESTS IP4" \
		  "$ADDR GOPEN" \
		  "bind" \
		  "$protov" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    case X$IPPORT in
	XPORT)  newport $(tolower $PROTO); _PORT=$PORT ;;
	XPROTO) #echo "IPPROTO=\"$IPPROTO\""
		_PORT=$IPPROTO ;;
    esac
    CMD0="$TRACE $SOCAT $opts -u /dev/null $ADDR:localhost-6-4.dest-unreach.net:$_PORT,bind=127.0.0.1"
    printf "test $F_n $TEST... " $N
    $CMD0 2>"${te}0" </dev/null
    rc0=$?
    if [ "$rc0" -ne 0 ]; then
	$PRINTF "$FAILED (rc0=$rc0)\n"
	echo "$CMD0"
	cat "${te}0" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))
done <<<"
UDP-SENDTO               udp4     PORT
UDPLITE-SENDTO           udplite4 PORT
IP-SENDTO                ip4      PROTO
"

# Test if CONNECT to a server name that resolves to IPv6 first and IPv4
# as second address, when binding to an IPv4 address, uses IPv4
# This failed in Socat 1.8.0.0
while read ADDR protov IPPORT _; do
if [ -z "$ADDR" ] || [[ "$ADDR" == \#* ]]; then continue; fi
FEATS=
ADDR_="$(echo $ADDR |tr - _)" 	# TCP_CONNECT
PROTO="${ADDR%%[-:]*}" 		# TCP
proto=$(tolower $PROTO) 	# tcp
FEATS="$FEATS $PROTO"
NAME="$(echo "V1800_${ADDR_}_CONNECT_6_4" |sed 's/:[.0-8]*//')"
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%ip4%*|*%$protov%*|*%$proto%*|*%socket%*|*%$NAME%*)
TEST="$NAME: test regression of $ADDR with IPv6,4 and binding to IPv4"
# Run an appropriate server address in background.
# Start a CONNECT command to (internal) test name localhost-6-4.dest-unreach.net
# and bind to an IPv4 address, connect, terminate immediately.
# When no error occurs the test succeeded.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "$FEATS DEVTESTS IP4" \
		  "$ADDR GOPEN" \
		  "bind" \
		  "$protov" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    case X$IPPORT in
	XPORT)  newport $(tolower $PROTO); _PORT=$PORT ;;
	XPROTO) #echo "IPPROTO=\"$IPPROTO\""
		_PORT=$IPPROTO ;;
    esac
    CMD0="$TRACE $SOCAT $opts ${ADDR%%-*}-LISTEN:$_PORT,pf=ip4 PIPE"
    CMD1="$TRACE $SOCAT $opts /dev/null $ADDR:localhost-6-4.dest-unreach.net:$_PORT,bind=127.0.0.1"
    printf "test $F_n $TEST... " $N
    $CMD0 2>"${te}0" </dev/null &
    pid0=$!
    wait${protov}port $PORT 1
    $CMD1 2>"${te}1" </dev/null
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))
done <<<"
TCP-CONNECT                tcp4      PORT
SCTP-CONNECT               sctp4     PORT
DCCP-CONNECT               dccp4     PORT
#PENSSL                    tcp4      PORT
#OCKS4:127.0.0.1           tcp4      PORT
#OCKS4A:127.0.0.1          tcp4      PORT
#OCKS5:127.0.0.1:1080      tcp4      PORT
#ROXY::127.0.0.1           tcp4      PORT
"


# Above tests introduced before or with 1.8.0.1
#==============================================================================
# Below test introduced with 1.8.0.2

# Test the readline.sh file overwrite vulnerability
NAME=READLINE_SH_OVERWRITE
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%readline%*|*%security%*|*%$NAME%*)
TEST="$NAME: Test the readline.sh file overwrite vulnerability"
# Create a symlink /tmp/$USER/stderr2 pointing to a temporary file,
# run readline.sh
# When the temporary file is kept the test succeeded
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "readline.sh" \
		  "" \
		  "" \
		  "" \
		  "" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.file"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    echo "$da" >"$tf"
    ln -sf "$tf" /tmp/$USER/stderr2
    CMD0="readline.sh cat"
    printf "test $F_n $TEST... " $N
    $CMD0 </dev/null >/dev/null 2>"${te}0"
    rc0=$?
#    if [ "$rc0" -ne 0 ]; then
#	$PRINTF "$CANT (rc0=$rc0)\n"
#	echo "$CMD0"
#	cat "${te}0" >&2
#	cant
#    elif ! echo "$da" |diff - "$tf" >$tdiff; then
    if ! echo "$da" |diff - "$tf" >$tdiff; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Above test introduced with 1.8.0.2
#==============================================================================
# Below tests introduced with 1.8.0.3 (or later)

# Test the SOCKS5-CONNECT and SOCKS5-LISTEN addresses with IPv4
for SUFFIX in CONNECT LISTEN; do

suffix=$(tolower $SUFFIX)
if [ "$SUFFIX" = LISTEN ]; then
    test=listen
    LISTEN=LISTEN
    listen=listen
else
    test=dont
    LISTEN=
    listen=
fi
NAME=SOCKS5${SUFFIX}_TCP4
case "$TESTS" in
*%$N%*|*%functions%*|*%socks%*|*%socks5%*|*%tcp%*|*%tcp4%*|*%ip4%*|*%$test%*|*%$NAME%*)
TEST="$NAME: SOCKS5-$SUFFIX over TCP/IPv4"
if ! eval $NUMCOND; then :;
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "od ./socks5server-echo.sh" \
		  "SOCKS5 IP4 TCP $LISTEN STDIO" \
		  "TCP4-LISTEN EXEC STDIN SOCKS5-$SUFFIX" \
		  "so-reuseaddr readbytes" \
		  "tcp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"; da="$da$($ECHO '\r')"
    newport tcp4 	# provide free port number in $PORT
    CMD0="$TRACE $SOCAT $opts TCP4-LISTEN:$PORT,$REUSEADDR EXEC:\"./socks5server-echo.sh\""
    CMD1="$TRACE $SOCAT $opts STDIO SOCKS5-$SUFFIX:$LOCALHOST:127.0.0.1:80,pf=ip4,socksport=$PORT"
    printf "test $F_n $TEST... " $N
    eval "$CMD0 2>\"${te}0\" &"
    pid0=$!	# background process id
    waittcp4port $PORT 1
    echo "$da" |$CMD1 >${tf}1 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null
    wait
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif ! echo "$da" |diff - "${tf}1" >"$tdiff"; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1 &"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
fi ;; # NUMCOND, feats
esac
N=$((N+1))

done 	# CONNECT LISTEN


# Test UDP-LISTEN with bind to IPv4 address; this failed with Socat version
# 1.8.0.0
NAME=UDP_LISTEN_BIND4
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%ip4%*|*%udp%*|*%udp4%*|*%listen%*|*%$NAME%*)
TEST="$NAME: Test UDP-LISTEN with bind to IPv4 addr"
# Start a listener with UDP-LISTEN and bind to 127.0.0.1; when it starts
# without error and even processes data the test succeeded
if ! eval $NUMCOND; then :
# Remove unneeded checks, adapt lists of the remaining ones
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 UDP LISTEN STDIO PIPE" \
		  "UDP-LISTEN PIPE STDIO UDP" \
		  "bind" \
		  "udp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    newport udp4
    CMD0="$TRACE $SOCAT $opts UDP-LISTEN:$PORT,bind=$LOCALHOST4 PIPE"
    CMD1="$TRACE $SOCAT $opts - UDP-CONNECT:$LOCALHOST4:$PORT"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    waitudp4port $PORT 1
    echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif ! echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


# Test for useful error message on UNIX-L with bind option
NAME=UNIX_L_BIND
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%listen%*|*%unix%*|*%bind%*|*%$NAME%*)
TEST="$NAME: Test if UNIX-L with bind does not fail INTERNAL"
# Invoke Socat with a UNIX-LISTEN address with bind option.
# When there is no INTERNAL error the test succeeded.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "UNIX LISTEN PIPE" \
		  "UNIX-LISTEN PIPE" \
		  "bind,accept-timeout" \
		  "" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    ts="$td/test$N.sock"
    tb="$td/test$N.bind"
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    CMD0="$TRACE $SOCAT $opts UNIX-LISTEN:$ts,accept-timeout=0.001,bind=$tb PIPE"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0"
    rc0=$?
    if [ "$rc0" -eq 0 ]; then
	$PRINTF "$CANT (rc0=$rc0)\n"
	echo "$CMD0"
	cat "${te}0" >&2
	cant
    elif grep " E .* INTERNAL " "${te}0" >/dev/null; then
	$PRINTF "$FAILED (INTERNAL)\n"
	echo "$CMD0"
	cat "${te}0" >&2
	failed
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	ok
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


while read ADDR proto CSUFF CPARMS COPTS SADDR SOPTS PIPE; do
if [ -z "$ADDR" ] || [[ "$ADDR" == \#* ]]; then continue; fi

if [ "X$CSUFF" != "X-" ]; then
    CADDR=$ADDR-$CSUFF
else
    CADDR=$ADDR
fi
CNAME=$(echo $CADDR |tr - _)
PROTO=$(toupper $proto)
FEAT=$ADDR
addr=$(tolower $ADDR)
runs=$proto
case "$CPARMS" in
    PORT) newport $proto; CPARMS=$PORT ;;
    *'$PORT'*) newport $proto; CPARMS=$(eval echo "$CPARMS") ;;
esac
#echo "PORT=$PORT CPARMS=$CPARMS" >&2
case "X$COPTS" in
    X-) COPTS= ;;
    *'$PORT'*) newport $proto; COPTS=$(eval echo "$COPTS") ;;
esac
case "X$SOPTS" in
    X-) SOPTS= ;;
esac

# Test if bind on *-CONNECT selects the matching IP version
NAME=${CNAME}_BIND_6_4
case "$TESTS" in
*%$N%*|*%functions%*|*%$addr%*|*%$proto%*|*%${proto}4%*|*%${proto}6%*|*%ip4%*|*%ip6%*|*%listen%*|*%bind%*|*%socket%*|*%$NAME%*)
TEST="$NAME: $ADDR bind chooses matching IPv"
# Have an IPv4 listener
# Host name localhost-4-6.dest-unreach.net resolves to both 127.0.0.1 and [::1],
# consequently; with option -6 we have Socat try IPv6 first, and on failure try
# IPv4
# Start Socat TCP-CONNECT with -6 and binding and connecting to this host name;
# Up to version 1.8.0.0 Socat only tries IPv6 and fails
# With version 1.8.0.1 Socat first connects using IPv6, and due to ECONNREFUSED
# tries to connect using IPv4 but still binds to IPv6 which fails with
# EAFNOSUPPORT "Address family not supported by protocol";
# With 1.8.0.3 the connection attempt with IPv4 correctly binds to IPv4 and
# succeeds
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "$FEAT IP4 IP6 TCP LISTEN STDIO PIPE" \
		  "$CADDR $SADDR STDIO PIPE" \
		  "bind pf" \
		  "${runs}4 ${runs}6" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
elif ! SOCAT_MAIN_WAIT= $SOCAT -h |grep -e '[[:space:]]-6[[:space:]]' >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}no option -0${NORMAL}\n" $N
    cant
elif [ -z "$INTERNET" -a "$RES" != 'DEVTESTS' ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}use test.sh option --internet${NORMAL}\n" $N
    cant
elif [ -z "$HAVEDNS" ] && ! testfeats DEVTESTS >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Broken DNS${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    CMD0="$TRACE $SOCAT $opts $SADDR:$PORT,$SOPTS,pf=2 $PIPE"
    CMD1="$TRACE $SOCAT $opts -6 STDIO $CADDR:localhost-4-6.dest-unreach.net:$CPARMS,bind=localhost-4-6.dest-unreach.net,$COPTS"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    wait${proto}4port $PORT 1
    { echo "$da"; relsleep 10; } |$CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    elif [ "$rc1" -ne 0 ] && grep "Address family not supported by protocol" "${te}1" >/dev/null; then
	$PRINTF "$FAILED (EAFNOSUPPORT)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif [ "$rc1" -ne 0 ]; then
	$PRINTF "$CANT (unexpected error)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	cant
    elif ! echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$CANT (unexpected problem)\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	cant
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

done <<<"
TCP     tcp  CONNECT PORT        -                           TCP-L  -                                         PIPE
SCTP    sctp CONNECT PORT        -                           SCTP-L -                                         PIPE
DCCP    dccp CONNECT PORT        -                           DCCP-L -                                         PIPE
OPENSSL tcp  CONNECT PORT        cafile=testsrv.pem,verify=0 SSL-L  cert=testsrv.pem,key=testsrv.key,verify=0 PIPE
SOCKS4  tcp  -       32.98.76.54:32109 socksport=\$PORT,socksuser=nobody  TCP-L  -             EXEC:./socks4echo.sh
SOCKS5  tcp  CONNECT \$PORT:127.0.0.1:80  -                               TCP-L  -      EXEC:./socks5server-echo.sh
PROXY   tcp  CONNECT 127.0.0.1:80        proxyport=\$PORT,crlf            TCP-L  crlf          EXEC:./proxyecho.sh
"


# Test if TCP-CONNECT with host name resolving to IPv6 first and IPv4 second
# (due to option -6) chooses IPv4 when bind option is specific.
# This works only since version 1.8.0.3
NAME=TCP_BIND_4
case "$TESTS" in
*%$N%*|*%functions%*|*%internet%*|*%tcp4%*|*%tcp6%*|*%ip4%*|*%ip6%*|*%listen%*|*%socket%*|*%$NAME%*)
TEST="$NAME: TCP-CONNECT chooses IPv4 from bind"
# Start a TCP4 listener with echo function
# Start Socat TCP-CONNECT with host name resolving to IPv6 first and IPv4
# second, and bind to IPv4 explicitly.
# When connection and data transfer work the test succeeded.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 IP6 TCP LISTEN STDIO PIPE" \
		  "TCP-CONNECT TCP4-LISTEN STDIO PIPE" \
		  "" \
		  "tcp4 tcp6" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
elif ! SOCAT_MAIN_WAIT= $SOCAT -h |grep -e '[[:space:]]-6[[:space:]]' >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}no option -0${NORMAL}\n" $N
    cant
elif [ -z "$INTERNET" -a "$RES" != 'DEVTESTS' ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}use test.sh option --internet${NORMAL}\n" $N
    cant
elif [ -z "$HAVEDNS" ] && ! testfeats DEVTESTS >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Broken DNS${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    newport tcp4
    CMD0="$TRACE $SOCAT $opts TCP4-LISTEN:$PORT PIPE"
    CMD1="$TRACE $SOCAT $opts -6 - TCP-CONNECT:localhost-4-6.dest-unreach.net:$PORT,bind=127.0.0.1"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    waittcp4port $PORT 1
    echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    elif [ "$rc1" -ne 0 ] && grep "Address family for hostname not supported" "${te}1" >/dev/null; then
	$PRINTF "$FAILED (EAFNOSUPPORT)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif [ "$rc1" -ne 0 ]; then
	$PRINTF "$CANT (unexpected error)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	cant
    elif ! echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$CANT (unexpected problem)\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	cant
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

# Test if TCP-CONNECT with host name resolving to IPv4 first and IPv6 second
# (due to option -4) chooses IPv6 when bind option is specific.
# This works only since version 1.8.0.3
NAME=TCP_BIND_6
case "$TESTS" in
*%$N%*|*%functions%*|*%internet%*|*%tcp4%*|*%tcp6%*|*%ip4%*|*%ip6%*|*%listen%*|*%socket%*|*%$NAME%*)
TEST="$NAME: TCP-CONNECT chooses IPv6 from bind"
# Start a TCP6 listener with echo function
# Start Socat TCP-CONNECT with host name resolving to IPv4 first and IPv6
# second, and bind to IPv6 explicitly.
# When connection and data transfer work the test succeeded.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "IP4 IP6 TCP LISTEN STDIO PIPE" \
		  "TCP-CONNECT TCP4-LISTEN STDIO PIPE" \
		  "" \
		  "tcp4 tcp6" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
elif ! SOCAT_MAIN_WAIT= $SOCAT -h |grep -e '[[:space:]]-4[[:space:]]' >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}no option -0${NORMAL}\n" $N
    cant
elif [ -z "$INTERNET" -a "$RES" != 'DEVTESTS' ]; then
    $PRINTF "test $F_n $TEST... ${YELLOW}use test.sh option --internet${NORMAL}\n" $N
    cant
elif [ -z "$HAVEDNS" ] && ! testfeats DEVTESTS >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}Broken DNS${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    newport tcp4
    CMD0="$TRACE $SOCAT $opts TCP6-LISTEN:$PORT PIPE"
    CMD1="$TRACE $SOCAT $opt -4 - TCP-CONNECT:localhost-4-6.dest-unreach.net:$PORT,bind=[::1]"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    waittcp4port $PORT 1
    echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    elif [ "$rc1" -ne 0 ] && grep "Address family for hostname not supported" "${te}1" >/dev/null; then
	$PRINTF "$FAILED (EAFNOSUPPORT)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif [ "$rc1" -ne 0 ]; then
	$PRINTF "$CANT (unexpected error)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	cant
    elif ! echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$CANT (unexpected problem)\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	cant
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))


while read ADDR proto CSUFF CPARMS COPTS SADDR SOPTS PIPE; do
if [ -z "$ADDR" ] || [[ "$ADDR" == \#* ]]; then continue; fi

if [ "X$CSUFF" != "X-" ]; then
    CADDR=$ADDR-$CSUFF
else
    CADDR=$ADDR
fi
CNAME=$(echo $CADDR |tr - _)
PROTO=$(toupper $proto)
FEAT=$ADDR
addr=$(tolower $ADDR)
runs=$proto
case "$CPARMS" in
    PORT) newport $proto; CPARMS=$PORT ;;
    *'$PORT'*) newport $proto; CPARMS=$(eval echo "$CPARMS") ;;
esac
#echo "PORT=$PORT CPARMS=$CPARMS" >&2
case "X$COPTS" in
    X-) COPTS= ;;
    *'$PORT'*) newport $proto; COPTS=$(eval echo "$COPTS") ;;
esac
case "X$SOPTS" in
    X-) SOPTS= ;;
esac

# Test the retry option with *-CONNECT addresses
NAME=${CNAME}_RETRY
case "$TESTS" in
*%$N%*|*%functions%*|*%$addr%*|*%$proto%*|*%${proto}4%*|*%ip4%*|*%listen%*|*%socket%*|*%retry%*|*%$NAME%*)
TEST="$NAME: $ADDR can retry"
# Have an IPv4 listener with delay
# Start a connector whose first attempt must fail; check if the second attempt
# succeeds.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "$FEAT IP4 TCP LISTEN STDIO" \
		  "$CADDR $SADDR STDIO PIPE" \
		  "pf retry interval" \
		  "${runs}4 ${runs}6" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
elif ! SOCAT_MAIN_WAIT= $SOCAT -h |grep -e '[[:space:]]-6[[:space:]]' >/dev/null; then
    $PRINTF "test $F_n $TEST... ${YELLOW}no option -0${NORMAL}\n" $N
    cant
else
#    newport $proto
#echo "PORT=$PORT CPARMS=$CPARMS" >&2
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    CMD0="relsleep 5; $TRACE $SOCAT $opts $SADDR:$PORT,$SOPTS,pf=2 $PIPE"
    CMD1="$TRACE $SOCAT $opts -4 STDIO $CADDR:$LOCALHOST:$CPARMS,retry=1,interval=$(relsecs 10),$COPTS"
    printf "test $F_n $TEST... " $N
#date +%Y/%m/%d" "%H:%M:%S.%N
    eval "$CMD0" >/dev/null 2>"${te}0" &
    pid0=$!
    { echo "$da"; relsleep 15; } |$CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    elif [ "$rc1" -ne 0 ] && grep "Address family not supported by protocol" "${te}1" >/dev/null; then
	$PRINTF "$FAILED (EAFNOSUPPORT)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif [ "$rc1" -ne 0 ]; then
	$PRINTF "$CANT (unexpected error)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	cant
    elif ! echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    else
	$PRINTF "$CANT (unexpected problem)\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	cant
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

done <<<"
TCP     tcp  CONNECT PORT        -                           TCP-L  -                                         PIPE
SCTP    sctp CONNECT PORT        -                           SCTP-L -                                         PIPE
DCCP    dccp CONNECT PORT        -                           DCCP-L -                                         PIPE
OPENSSL tcp  CONNECT PORT        cafile=testsrv.pem,verify=0 SSL-L  cert=testsrv.pem,key=testsrv.key,verify=0 PIPE
SOCKS4  tcp  -       32.98.76.54:32109 socksport=\$PORT,socksuser=nobody  TCP-L  -             EXEC:./socks4echo.sh
SOCKS5  tcp  CONNECT \$PORT:127.0.0.1:80  -                               TCP-L  -      EXEC:./socks5server-echo.sh
PROXY   tcp  CONNECT 127.0.0.1:80        proxyport=\$PORT,crlf            TCP-L  crlf          EXEC:./proxyecho.sh
"

#------------------------------------------------------------------------------

while read ADDR proto CSUFF CPARMS COPTS SADDR SOPTS PIPE SLOW; do
if [ -z "$ADDR" ] || [[ "$ADDR" == \#* ]]; then continue; fi

if [ "X$CSUFF" != "X-" ]; then
    CADDR=$ADDR-$CSUFF
else
    CADDR=$ADDR
fi
CNAME=$(echo $CADDR |tr - _)
PROTO=$(toupper $proto)
FEAT=$ADDR
addr=$(tolower $ADDR)
runs=$proto
case "$CPARMS" in
    PORT) newport $proto; CPARMS=$PORT ;;
    *'$PORT'*) newport $proto; CPARMS=$(eval echo "$CPARMS") ;;
esac
#echo "PORT=$PORT CPARMS=$CPARMS" >&2
case "X$COPTS" in
    X-) COPTS= ;;
    *'$PORT'*) newport $proto; COPTS=$(eval echo "$COPTS") ;;
esac
case "X$SOPTS" in
    X-) SOPTS= ;;
esac

# Test the fork and max-children options with CONNECT addresses
NAME=${CNAME}_MAXCHILDREN
case "$TESTS" in
*%$N%*|*%functions%*|*%$addr%*|*%$proto%*|*%${proto}4%*|*%ip4%*|*%listen%*|*%socket%*|*%fork%*|*%maxchildren%*|*%$NAME%*)
TEST="$NAME: $ADDR with fork,max-children"
# Start a reader process that transfers received data to an output file;
# run a sending client that forks at most 2 parallel child processes that
# transfer data from a simple directory queue to the reader but afterwards
# hang some time to prevent more child process.
# After the first two transfers write the third record directly to the file;
# a little later the Socat mechanism puts a 4th record.
# When the 4 records in the output file have the expected order the test
# succeeded.
if ! eval $NUMCOND; then :
elif ! cond=$(checkconds \
		  "" \
		  "" \
		  "" \
		  "$FEAT IP4 TCP LISTEN STDIO PIPE" \
		  "$CADDR $SADDR STDIO PIPE" \
		  "pf" \
		  "${runs}4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
#    newport $proto
#echo "PORT=$PORT CPARMS=$CPARMS" >&2
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    tQ="$td/test$N.q"
    ext=q.y.f. 	# some unusual extension to prevent from deleting wrong file
    da="test$N $(date) $RANDOM"
    CMD0="$TRACE $SOCAT $opts -lp reader $SADDR:$PORT,$SOPTS,pf=2,reuseaddr,fork $PIPE"
    CMD1="$TRACE $SOCAT $opts -4 $CADDR:$LOCALHOST:$CPARMS,fork,max-children=2,interval=$(relsecs $((2*SLOW))),$COPTS SHELL:'shopt\ -s\ nullglob;\ F=\$(ls -1 $tQ|grep .$ext\\\$|head -n 1);\ test\ \"\$F\"\ ||\ exit;\ cat\ $tQ/\$F;\ mv\ -i\ $tQ/\$F\ $tQ/.\$F;\ sleep\ $(relsecs $((5*SLOW)) )'!!-"
    printf "test $F_n $TEST... " $N
    # create data for the generator
    mkdir -p $tQ
    echo "$da 1" >$tQ/01.$ext
    echo "$da 2" >$tQ/02.$ext
    echo "$da 4" >$tQ/04.$ext
    eval "$CMD0" 2>"${te}0" &
    pid0=$!
    relsleep $((1*SLOW))
    eval $CMD1 2>"${te}1" >>"${tf}0" &
    pid1=$!
    relsleep $((4*SLOW))
#date +%Y/%m/%d" "%H:%M:%S.%N
    echo "$da 3" >>"${tf}0"
    relsleep $((4*SLOW))
    kill $(childpids -r $pid0) $pid0 $(childpids -r $pid1) $pid1 2>/dev/null
    wait 2>/dev/null
    if ! test -e "${tf}0" || ! test -s "${tf}0"; then
	$PRINTF "$FAILED\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif $ECHO "$da 1\n$da 2\n$da 3\n$da 4" |diff - ${tf}0 >$tdiff; then
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    else
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    fi
fi # NUMCOND
 ;;
esac
N=$((N+1))

done <<<"
TCP     tcp  CONNECT PORT         -                                TCP-L  -                           PIPE 1
SCTP    sctp CONNECT PORT         -                                SCTP-L -                           PIPE 1
DCCP    dccp CONNECT PORT         -                                DCCP-L -                           PIPE 1
OPENSSL tcp  CONNECT PORT cafile=testsrv.pem,verify=0 SSL-L cert=testsrv.pem,key=testsrv.key,verify=0 PIPE 7
SOCKS4  tcp  - 32.98.76.54:32109 socksport=\$PORT,socksuser=nobody TCP-L  -           EXEC:./socks4echo.sh 6
SOCKS5  tcp  CONNECT \$PORT:127.0.0.1:80  -                        TCP-L  -    EXEC:./socks5server-echo.sh 5
PROXY   tcp  CONNECT 127.0.0.1:80 proxyport=\$PORT,crlf            TCP-L  crlf         EXEC:./proxyecho.sh 4
"
# detto IP6


# test combined 4_6/6_4 with retry and fork


# end of common tests

##################################################################################
#=================================================================================
# Here come tests that might affect your systems integrity. Put normal tests
# before this paragraph.
# Tests must be explicitly selected by roottough or name (not number)

NAME=PTYGROUPLATE
case "$TESTS" in
*%roottough%*|*%$NAME%*)
TEST="$NAME: pty with group-late works on pty"
# up to socat 1.7.1.1 address pty changed the ownership of /dev/ptmx instead of
# the pty with options user-late, group-late, or perm-late.
# here we check for correct behaviour.
# ATTENTION: in case of failure of this test the
# group of /dev/ptmx might be changed!
if ! eval $NUMCOND; then :; else
# save current /dev/ptmx properties
F=
for f in /dev/ptmx /dev/ptc; do
    if [ -e $f ]; then
	F=$(echo "$f" |tr / ..)
	ls -l $f >"$td/test$N.$F.ls-l"
	break
    fi
done
printf "test $F_n $TEST... " $N
if [ -z "$F" ]; then
    echo -e "${YELLOW}no /dev/ptmx or /dev/ptc${NORMAL}"
else
GROUP=daemon
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tl="$td/test$N.pty"
tdiff="$td/test$N.diff"
da="test$N $(date) $RANDOM"
CMD0="$TRACE $SOCAT $opts pty,link=$tl,group-late=$GROUP,escape=0x1a PIPE"
CMD1="$TRACE $SOCAT $opts - $tl,raw,echo=0"
$CMD0 >/dev/null 2>"${te}0" &
pid0=$!
(echo "$da"; relsleep 1; echo -e "\x1a") |$CMD1 >"${tf}1" 2>"${te}1" >"$tf"
rc1=$?
kill $pid0 2>/dev/null; wait
if [ $rc1 -ne 0 ]; then
    $PRINTF "$FAILED\n"
    echo "$CMD0 &"
    echo "$CMD1"
    cat "${te}0"
    cat "${te}1"
    failed
elif echo "$da" |diff - "$tf" >$tdiff; then
    $PRINTF "$OK\n"
    ok
else
    $PRINTF "$FAILED\n"
    cat "$tdiff"
    failed
fi
if ! ls -l $f |diff "$td/test$N.$F.ls-l" -; then
    $PRINTF "${RED}this test changed properties of $f!${NORMAL}\n"
fi
fi # no /dev/ptmx
fi # NUMCOND
 ;;
esac
N=$((N+1))


echo "Used temp directory $TD - you might want to remove it after analysis"
echo "Summary: $((N-1)) tests, $((numOK+numFAIL+numCANT)) selected; $numOK ok, $numFAIL failed, $numCANT could not be performed"

set -- $listCANT; while [ "$1" ]; do echo "$1"; shift; done >"$td/cannot.lst"
ln -sf "$td/cannot.lst" .
set -- $listOK;   while [ "$1" ]; do echo "$1"; shift; done >"$td/success.lst"
ln -sf "$td/success.lst" .
set -- $listFAIL; while [ "$1" ]; do echo "$1"; shift; done >"$td/failed.lst"
ln -sf "$td/failed.lst" .
#sort -n <(cat "$td/success.lst" |while read x; do echo "$x OK"; done) <(cat "$td/cannot.lst" |while read x; do echo "$x CANT"; done) <(cat "$td/failed.lst" |while read x; do echo "$x FAILED"; done) >"$td/result.txt"
#ln -sf "$td/result.txt" .

ln -sf "$td/results.txt" .

if [ "$numCANT" -gt 0 ]; then
    echo "CANT: $listCANT"
fi
if [ "$numFAIL" -gt 0 ]; then
    echo "FAILED: $listFAIL"
fi

if [ -z "$OPT_EXPECT_FAIL" ]; then
    [ "$numFAIL" -eq 0 ]
    exit 	# with rc from above statement
fi

if [ "$OPT_EXPECT_FAIL" ]; then
    diff  <(set -- $(echo "$EXPECT_FAIL" |tr ',' ' '); while [ "$1" ]; do echo "$1"; shift; done) "$td/failed.lst" >"$td/failed.diff"
    ln -sf "$td/failed.diff" .
    #grep "^"
    grep "^> " "$td/failed.diff" |awk '{print($2);}' >"$td/failed.unexp"
    ln -sf "$td/failed.unexp" .
    echo "FAILED unexpected: $(cat "$td/failed.unexp" |xargs echo)"
    grep "^< " "$td/failed.diff" |awk '{print($2);}' >"$td/ok.unexp"
    ln -sf "$td/ok.unexp" .
    echo "OK unexpected: $(cat "$td/ok.unexp" |xargs echo)"
else
    touch "$td/failed.diff"
fi
#listFAIL=$(cat "$td/failed.lst" |xargs echo)
#numFAIL="$(wc -l "$td/failed.lst" |awk '{print($1);}')"

! test -s "$td/failed.unexp"
exit

#==============================================================================

rm -f testsrv.* testcli.* testsrvdsa* testsrvfips* testclifips*

# end

# too dangerous - run as root and having a shell problem, it might purge your
# file systems
#rm -r "$td"

# sometimes subprocesses hang; we want to see this
wait

exit

#==============================================================================
# test template

# Give a description of what is tested (a bugfix, a new feature...)
NAME=SHORT_UNIQUE_TESTNAME
case "$TESTS" in
*%$N%*|*%functions%*|*%bugs%*|*%socket%*|*%$NAME%*)
#*%internet%*|*%root%*|*%listen%*|*%fork%*|*%ip4%*|*%tcp4%*|*%bug%*|...
TEST="$NAME: give a one line description of test"
# Describe how the test is performed, and what's the success criteria
if ! eval $NUMCOND; then :
# Remove unneeded checks, adapt lists of the remaining ones
elif ! cond=$(checkconds \
		  "Linux FreeBSD" \
		  "root" \
		  "nslookup" \
		  "IP4 TCP LISTEN STDIO PIPE" \
		  "TCP4-LISTEN PIPE STDIN STDOUT TCP4" \
		  "so-reuseaddr" \
		  "tcp4" ); then
    $PRINTF "test $F_n $TEST... ${YELLOW}$cond${NORMAL}\n" $N
    cant
else
    tf="$td/test$N.stdout"
    te="$td/test$N.stderr"
    tdiff="$td/test$N.diff"
    da="test$N $(date) $RANDOM"
    newport tcp4 	# or whatever proto, or drop this line
    CMD0="$TRACE $SOCAT $opts server-address PIPE"
    CMD1="$TRACE $SOCAT $opts - client-address"
    printf "test $F_n $TEST... " $N
    $CMD0 >/dev/null 2>"${te}0" &
    pid0=$!
    wait<something>port $PORT 1
    #relsleep 1 	# if no matching wait*port function
    echo "$da" |$CMD1 >"${tf}1" 2>"${te}1"
    rc1=$?
    kill $pid0 2>/dev/null; wait
    if [ "$rc1" -ne 0 ]; then
	$PRINTF "$FAILED (rc1=$rc1)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	failed
    elif ! echo "$da" |diff - "${tf}1" >$tdiff; then
	$PRINTF "$FAILED (diff)\n"
	echo "$CMD0 &"
	cat "${te}0" >&2
	echo "$CMD1"
	cat "${te}1" >&2
	echo "// diff:" >&2
	cat "$tdiff" >&2
	failed
    elif [ ??? ]; then
	# The test could not run meaningfully
	$PRINTF "$CANT\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	cant
    else
	$PRINTF "$OK\n"
	if [ "$VERBOSE" ]; then echo "$CMD0 &"; fi
	if [ "$DEBUG" ];   then cat "${te}0" >&2; fi
	if [ "$VERBOSE" ]; then echo "$CMD1"; fi
	if [ "$DEBUG" ];   then cat "${te}1" >&2; fi
	ok
    fi
    result
fi # NUMCOND
 ;;
esac
N=$((N+1))
