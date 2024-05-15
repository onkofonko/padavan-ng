#!/bin/sh

func_start()
{
if [ -f "/var/run/doh_proxy.pid" ]; then
		logger -t doh_proxy "DoH proxy is running."
else

DOH_S1=$(nvram get doh_server1)
DOH_S2=$(nvram get doh_server2)
DOH_S3=$(nvram get doh_server3)
DOH_S4=$(nvram get doh_server4)
DOH_O1=$(nvram get doh_opt2_1)
DOH_O2=$(nvram get doh_opt2_2)
DOH_O3=$(nvram get doh_opt2_3)
DOH_O4=$(nvram get doh_opt2_4)

if [ "$DOH_S1" = "" ]; then
  logger -t doh_proxy "Not Server1"
else
   if [ "$DOH_O1" = "" ]; then
/usr/sbin/doh_proxy -a 127.0.0.1 -p 65055 -r $DOH_S1 $(nvram get doh_opt1_1)
   else
/usr/sbin/doh_proxy -a 127.0.0.1 -p 65055 -b $DOH_O1 -r $DOH_S1 $(nvram get doh_opt1_1)
   fi
logger -t doh_proxy "Start resolving to $DOH_S1 : 65055."
fi
if [ "$DOH_S2" = "" ]; then
  logger -t doh_proxy "Not Server2"
else
   if [ "$DOH_O2" = "" ]; then
/usr/sbin/doh_proxy -a 127.0.0.1 -p 65056 -r $DOH_S2 $(nvram get doh_opt1_2)
   else
/usr/sbin/doh_proxy -a 127.0.0.1 -p 65056 -b $DOH_O2 -r $DOH_S2 $(nvram get doh_opt1_2)
   fi
logger -t doh_proxy "Start resolving to $DOH_S2 : 65056."
fi
if [ "$DOH_S3" = "" ]; then
  logger -t doh_proxy2 "Not Server3"
else
   if [ "$DOH_O3" = "" ]; then
/usr/sbin/doh_proxy2 -a 127.0.0.1 -p 65057 -r $DOH_S3 $(nvram get doh_opt1_3)
   else
/usr/sbin/doh_proxy2 -a 127.0.0.1 -p 65057 -b $DOH_O3 -r $DOH_S3 $(nvram get doh_opt1_3)
   fi
logger -t doh_proxy2 "Start resolving to $DOH_S3 : 65057."
fi
if [ "$DOH_S4" = "" ]; then
  logger -t doh_proxy2 "Not Server4"
else
   if [ "$DOH_O4" = "" ]; then
/usr/sbin/doh_proxy2 -a 127.0.0.1 -p 65058 -r $DOH_S4 $(nvram get doh_opt1_4)
   else
/usr/sbin/doh_proxy2 -a 127.0.0.1 -p 65058 -b $DOH_O4 -r $DOH_S4 $(nvram get doh_opt1_4)
   fi
logger -t doh_proxy2 "Start resolving to $DOH_S4 : 65058."
fi

touch /var/run/doh_proxy.pid
sync && echo 3 > /proc/sys/vm/drop_caches
fi
}

func_stop()
{
if [ -f "/var/run/doh_proxy.pid" ]; then
	killall -SIGHUP doh_proxy
	killall -SIGHUP doh_proxy2
	logger -t doh_proxy "Shutdown."
	rm /var/run/doh_proxy.pid
else
	logger -t doh_proxy "DoH proxy is stoping."
fi
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
	echo "Usage: $0 {start|stop|restart}"
	exit 1
	;;
esac

exit 0
