#!/bin/sh

resolver=$(nvram get dnscrypt_resolver)
localipaddr=$(nvram get dnscrypt_ipaddr)
localport=$(nvram get dnscrypt_port)
options=$(nvram get dnscrypt_options)
log_type=""

func_message()
{
	if [ "$log_type" = "logger" ]; then
		logger -t "dnscrypt-proxy" "$1"
	else
    	echo $([ ! -z $2 ] && echo "$2 ")"$1" || :;
	fi
}

func_status ()
{
	[ "`pidof dnscrypt-proxy`" ]
}

func_start()
{
	# Start dnscrypt-proxy
	func_message "dnscrypt-proxy running..." "-n"

	/usr/bin/logger -t DNSCrypt-proxy listening on $localipaddr:$localport.
	/usr/sbin/dnscrypt-proxy -R $resolver -a $localipaddr:$localport -u dnscrypt -d $options
	sleep 1
	if [ "$log_type" = "logger" ]; then
    	func_status && func_message "start done" || func_message "start failed"
	else
    	func_status && func_message " done" || func_message " failed"
    fi
}

func_stop()
{
	func_message "dnscrypt-proxy stoping..." "-n"
	killall -q dnscrypt-proxy
	sleep 1
	if [ "$log_type" = "logger" ]; then
   	 	func_status && func_message "stop failed" || func_message "stop done"
	else
   	 	func_status && func_message " failed" || func_message " done"
	fi
}

[ -n $2 ] && [ "$2" = "-l" ] && log_type="logger"

case "$1" in
start)
	func_status && func_message "dnscrypt-proxy already running" || func_start
	;;
stop)
	func_status && func_stop || func_message "dnscrypt-proxy is not running"
	;;
restart)
	func_status && func_stop
	sleep 3
	func_start
	;;
status)
	func_status && func_message "dnscrypt-proxy already running" || func_message "dnscrypt-proxy is not running"
	;;
*)
	echo "Usage: $0 {start|stop|restart|status}"
	exit 1
	;;
esac

exit 0
