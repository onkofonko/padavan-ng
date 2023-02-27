#!/bin/sh
#nvram set ntp_ready=0

logger -t "autostart" "checking if the router is connected to the internet..."
count=0
while :
do
	ping -c 1 -W 1 -q 1.1.1.1 1>/dev/null 2>&1
	if [ "$?" == "0" ]; then
		break
	fi
	sleep 5
	ping -c 1 -W 1 -q one.one.one.one 1>/dev/null 2>&1
	if [ "$?" == "0" ]; then
		break
	fi
	sleep 5
	count=$((count+1))
	if [ $count -gt 18 ]; then
		break
	fi
done
