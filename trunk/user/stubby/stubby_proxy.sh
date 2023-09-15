#!/bin/sh

func_start()
{
### Configure Stubby. DNS-over-TLS (DoT).

if [ ! -f "/etc/storage/stubby/stubby.yml" ]; then

	mkdir /etc/storage/stubby

cat << EOF > /etc/storage/stubby/stubby.yml
resolution_type: GETDNS_RESOLUTION_STUB
round_robin_upstreams: 1
appdata_dir: "/var/lib/stubby"
# tls_authentication: GETDNS_AUTHENTICATION_REQUIRED
tls_query_padding_blocksize: 128
edns_client_subnet_private: 1
idle_timeout: 9000
listen_addresses:
  - 127.0.0.1@65053
  - 0::1@65053
dns_transport_list:
  - GETDNS_TRANSPORT_TLS
upstream_recursive_servers:
  - address_data: 1.1.1.1
    tls_auth_name: "cloudflare-dns.com"
  - address_data: 1.0.0.1
    tls_auth_name: "cloudflare-dns.com"
  - address_data: 8.8.8.8
    tls_auth_name: "dns.google"
  - address_data: 8.8.4.4
    tls_auth_name: "dns.google"
EOF

	chmod -R u=rwX,go=rX /etc/storage/stubby

	nvram set wan_dnsenable_x=0
	nvram set wan_dns1_x=127.0.0.1
	nvram set wan_dns2_x=
	nvram set wan_dns3_x=
	nvram commit
	mtd_storage.sh save
	echo 'Done!'
fi

if [ -f "/var/run/stubby_proxy.pid" ]; then
		logger -t stubby "Stubby is running."
	else
		/usr/sbin/stubby_start.sh 
###		/usr/sbin/stubby -g -l /tmp/stubby.log
		touch /var/run/stubby_proxy.pid
		logger -t stubby "Running."
fi
}


func_stop()
{
if [ -f "/var/run/stubby_proxy.pid" ]; then
		killall -SIGHUP stubby
		logger -t stubby "Shutdown."
		rm /var/run/stubby_proxy.pid

	else
		logger -t stubby "Stubby is stoping."
fi
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
