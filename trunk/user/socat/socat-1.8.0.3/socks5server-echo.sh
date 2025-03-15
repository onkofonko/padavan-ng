#! /usr/bin/env bash
# Source: socks5connect-echo.sh

# Copyright Gerhard Rieger and contributors (see file CHANGES)
# Published under the GNU General Public License V.2, see file COPYING

# Performs primitive simulation of a socks5 server with echo function via stdio.
# Accepts and answers SOCKS5 CONNECT request without authentication to
# 8.8.8.8:80, however is does not connect there but just echoes data.
# It is required for test.sh
# For TCP, use this script as:
# socat TCP-L:1080,reuseaddr EXEC:"socks5connect-echo.sh"

#set -vx

if [ "$SOCAT" ]; then
    :
elif type socat >/dev/null 2>&1; then
    SOCAT=socat
else
    SOCAT=./socat
fi

case `uname` in
HP-UX|OSF1)
    CAT="$SOCAT -u STDIN STDOUT"
    ;;
*)
    CAT=cat
    ;;
esac

A="7f000001"
P="0050"

# Read and parse SOCKS5 greeting
read _ v b c _ <<<"$($SOCAT -u -,readbytes=3 - |od -t x1)"
#echo "$v $b $c" >&2
if [ "$v" != 05 ]; then echo "$0: Packet1: expected version x05, got \"$v\"" >&2; exit 1; fi
if [ "$b" != 01 ]; then echo "$0: Packet1: expected 01 auth methods, got \"$b\"" >&2; exit 1; fi
if [ "$c" != 00 ]; then echo "$0: Packet1: expected auth method 00, got \"$c\"" >&2; exit 1; fi
# Send answer
echo -en "\x05\x00"

# Read and parse SOCKS5 connect request
read _ v b c d a1 a2 a3 a4 p1 p2 _ <<<"$($SOCAT -u -,readbytes=10 - |od -t x1)"
#echo "$v $b $c $d $a1 $a2 $a3 $a4 $p1 $p2" >&2
a="$a1$a2$a3$a4"
p="$p1$p2"
if [ "$v" != 05 ];   then echo "$0: Packet2: expected version x05, got \"$v\"" >&2; exit 1; fi
if [ "$b" != 01 ] && [ "$b" != 02 ];   then echo "$0: Packet2: expected connect request 01 or bind request 02, got \"$b\"" >&2; exit 1; fi
if [ "$c" != 00 ];   then echo "$0: Packet2: expected reserved 00, got \"$c\"" >&2; exit 1; fi
if [ "$d" != 01 ];   then echo "$0: Packet2: expected address type 01, got \"$d\"" >&2; exit 1; fi
if [ "$a" != "$A" ]; then echo "$0: Packet2: expected address $A, got \"$a\"" >&2; exit 1; fi
if [ "$p" != "$P" ]; then echo "$0: Packet2: expected port $P, got \"$p\"" >&2; exit 1; fi
if [ "$z" != "" ];   then echo "$0: Packet2: trailing data \"$z\"" >&2; exit 1; fi
# Send answer
echo -en "\x05\x00\x00\x01\x10\x00\x1f\x64\x1f\x64"

# Bind/listen/passive mode
if [ "$b" == 02 ]; then
    sleep 1 	# pretend to be waiting for connection
    echo -en "\x05\x00\x00\x01\x10\xff\x1f\x64\x23\x28"
fi

# perform echo function
$CAT
