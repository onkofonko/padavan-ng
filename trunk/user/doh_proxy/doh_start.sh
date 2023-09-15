#!/bin/sh

func_start()
{
### Create Configure DoH proxy. DNS-over-HTTPS (DoH).
if [ ! -f "/etc/storage/doh_proxy.sh" ]; then
	logger -t doh_proxy "Create file doh_proxy.sh."
	cp /usr/sbin/doh_proxy.sh /etc/storage/doh_proxy.sh

	mtd_storage.sh save

	echo 'Done!'

fi
	/etc/storage/doh_proxy.sh start
}


func_stop()
{
	/etc/storage/doh_proxy.sh stop
}

case "$1" in
start)
	func_start
	;;
stop)
	func_stop
	;;
*)
	echo "Usage: $0 {start|stop}"
	exit 1
	;;
esac

exit 0
