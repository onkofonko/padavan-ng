#!/bin/sh

func_start()
{
### Create Configure DoH proxy. DNS-over-HTTPS (DoH).
if [ ! -f "/etc/storage/doh_proxy.sh" ]; then
	logger -t doh_proxy "Create file doh_proxy.sh."
	cp /usr/sbin/doh_proxy.sh /etc/storage/doh_proxy.sh

	echo -e '\n''### Config DOH_proxy ###' >> /etc/storage/dnsmasq/dnsmasq.conf
	echo -e '\n''no-resolv''\n''proxy-dnssec' >> /etc/storage/dnsmasq/dnsmasq.conf
	echo 'server=127.0.0.1#65054' >> /etc/storage/dnsmasq/dnsmasq.conf
	echo 'server=...1#65054' >> /etc/storage/dnsmasq/dnsmasq.conf
	echo 'server=127.0.0.1#65055' >> /etc/storage/dnsmasq/dnsmasq.conf
	echo 'server=...1#65055' >> /etc/storage/dnsmasq/dnsmasq.conf
	echo 'server=127.0.0.1#65056' >> /etc/storage/dnsmasq/dnsmasq.conf
	echo 'server=...1#65056' >> /etc/storage/dnsmasq/dnsmasq.conf

	[ -n "`nvram get ntp_server0`" ] && \
	echo "server=/`nvram get ntp_server0`/1.1.1.1" >> /etc/storage/dnsmasq/dnsmasq.conf

	[ -n "`nvram get ntp_server1`" ] && \
	echo "server=/`nvram get ntp_server1`/1.1.1.1" >> /etc/storage/dnsmasq/dnsmasq.conf
	echo -e '\n''all-servers' >> /etc/storage/dnsmasq/dnsmasq.conf

	nvram set wan_dnsenable_x=0
	nvram set wan_dns1_x=127.0.0.1
	nvram set wan_dns2_x=
	nvram set wan_dns3_x=
	nvram commit

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
