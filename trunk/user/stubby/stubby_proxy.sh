#!/bin/sh

func_start()
{
### Configure Stubby. DNS-over-TLS (DoT).

if [ ! -f "/etc/storage/stubby/stubby.yml" ]; then

	mkdir /etc/storage/stubby

cat << EOF > /etc/storage/stubby/stubby.yml
####### STUBBY YAML CONFIG FILE ######
resolution_type: GETDNS_RESOLUTION_STUB
dns_transport_list:
  - GETDNS_TRANSPORT_TLS
tls_authentication: GETDNS_AUTHENTICATION_REQUIRED
tls_query_padding_blocksize: 128
edns_client_subnet_private : 1
round_robin_upstreams: 1
idle_timeout: 10000
listen_addresses:
  - 127.0.0.1@65054
  - 0::1@65054
####### DNSSEC SETTINGS ######
#dnssec_return_status: GETDNS_EXTENSION_TRUE
#dnssec_return_only_secure: GETDNS_EXTENSION_TRUE
#trust_anchors_backoff_time: 2500
#appdata_dir: "/var/lib/stubby"
#######  UPSTREAMS  ######
upstream_recursive_servers:
####### IPv4 addresses ######
## Cloudflare
  - address_data: 1.1.1.1
    tls_auth_name: "cloudflare-dns.com"
  - address_data: 1.0.0.1
    tls_auth_name: "cloudflare-dns.com"
## Google
  - address_data: 8.8.8.8
    tls_auth_name: "dns.google"
  - address_data: 8.8.4.4
    tls_auth_name: "dns.google"
EOF

	chmod -R u=rwX,go=rX /etc/storage/stubby

	mtd_storage.sh save
	echo 'Done!'
fi

if [ -f "/var/run/stubby_proxy.pid" ]; then
		logger -t stubby "Stubby is running."
	else
		/usr/bin/stubby_start.sh 
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
