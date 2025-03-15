#! /usr/bin/env bash
# source: readline.sh
# Copyright Gerhard Rieger and contributors (see file CHANGES)
# Published under the GNU General Public License V.2, see file COPYING

# this is an attempt for a socat based readline wrapper
# usage: readline.sh [options] <program>

withhistfile=1

STDERR=
while true; do
    case "X$1" in
	X-lf?*) STDERR="${1:3}" ;;
	X-lf) shift; STDERR="$1" ;;
	X-nh|X-nohist*) withhistfile= ;;
	*) break;;
    esac
    shift
done

PROGRAM="$@"
if [ "$withhistfile" ]; then
    HISTFILE="$HOME/.$1_history"
    HISTOPT=",history=$HISTFILE"
else
    HISTOPT=
fi
#
#

#if test -w .; then
if [ -z "$STDERR" ] && find . -maxdepth 0 -user $USER ! -perm /022 -print |grep ^ >/dev/null; then
    # When cwd is owned by $USER and it is neither group nor world writable
    STDERR=./socat-readline.${1##*/}.log
    rm -f $STDERR
    echo "$0: logs go to $STDERR" >&2
elif [ -z "$STDERR" ]; then
    echo "$0: insecure working directory, no logs are written" >&2
    STDERR=/dev/null
else
    echo "$0: logs go to $STDERR" >&2
fi

exec socat -d READLINE"$HISTOPT",noecho='[Pp]assword:' EXEC:"$PROGRAM",sigint,pty,setsid,ctty,raw,echo=0,stderr 2>$STDERR

