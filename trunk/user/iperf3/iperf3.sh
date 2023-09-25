#!/bin/sh

func_start(){
	iperf3 -s -D
	logger -st "iperf3" "start"
}

func_stop(){
	killall -q iperf3
	logger -st "iperf3" "stop"
}

case "$1" in
start)
	func_start
	;;
stop)
	func_stop
	;;
restart)
	func_stop
	func_start
	;;
*)
	echo "Usage: $0 { start | stop | restart }"
	exit 1
	;;
esac
