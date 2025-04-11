/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/sockios.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <netinet/ip_icmp.h>

#include "rc.h"
#include "switch.h"

// Define constants for buffer sizes, timeouts, and other magic numbers
#define BUFFER_SIZE_64 64
#define BUFFER_SIZE_24 24
#define BUFFER_SIZE_16 16
#define BUFFER_SIZE_32 32
#define BUFFER_SIZE_8 8
#define BUFFER_SIZE_80 80
#define BUFFER_SIZE_100 100
#define BUFFER_SIZE_256 256
#define TIMEOUT_5_SECONDS 5
#define TIMEOUT_3_SECONDS 3
#define TIMEOUT_2_SECONDS 2
#define TIMEOUT_10_SECONDS 10
#define TIMEOUT_60_SECONDS 60
#define VLAN_PRIORITY_DEFAULT 0
#define VLAN_PRIORITY_MAX 7
#define VLAN_ID_MIN 0
#define VLAN_ID_MAX 4095
#define METRIC_DEFAULT 10
#define METRIC_HIGH 1
#define METRIC_LOW 2
#define IPV4_NETMASK_DEFAULT "255.255.255.255"
#define IPV4_ADDRESS_ANY "0.0.0.0"
#define IPV4_GOOGLE_DNS "8.8.8.8"

static int send_icmp_ping(const char *ip_address) {
    int sockfd;
    struct sockaddr_in addr;
    struct icmphdr icmp_hdr;
    char buffer[64];

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip_address, &addr.sin_addr);

    memset(&icmp_hdr, 0, sizeof(icmp_hdr));
    icmp_hdr.type = ICMP_ECHO;
    icmp_hdr.un.echo.id = getpid();
    icmp_hdr.un.echo.sequence = 1;
    icmp_hdr.checksum = 0; // Kernel will calculate checksum

    memcpy(buffer, &icmp_hdr, sizeof(icmp_hdr));

    if (sendto(sockfd, buffer, sizeof(icmp_hdr), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("sendto");
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}

static int kill_process_by_pidfile(const char *pidfile, int signal) {
    FILE *fp = fopen(pidfile, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    pid_t pid;
    if (fscanf(fp, "%d", &pid) != 1) {
        fclose(fp);
        fprintf(stderr, "Failed to read PID from %s\n", pidfile);
        return -1;
    }
    fclose(fp);

    if (kill(pid, signal) < 0) {
        perror("kill");
        return -1;
    }

    return 0;
}

static int detect_wan(void) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execlp("detect_wan", "detect_wan", NULL);
        _exit(EXIT_FAILURE); // If execlp fails
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } else {
        // Fork failed
        logmessage("net_wan", "Failed to fork for detect_wan: %s", strerror(errno));
        return -1;
    }
}

static int configure_interface(const char *ifname, const char *ip, const char *netmask, const char *broadcast, int flags) {
    int sockfd;
    struct ifreq ifr;
    struct sockaddr_in *addr;

    // Open a socket for ioctl calls
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        logmessage("net_wan", "Failed to open socket: %s", strerror(errno));
        return -1;
    }

    // Set interface name
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

    // Set IP address
    if (ip) {
        addr = (struct sockaddr_in *)&ifr.ifr_addr;
        addr->sin_family = AF_INET;
        if (inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
            logmessage("net_wan", "Invalid IP address: %s", ip);
            close(sockfd);
            return -1;
        }
        if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
            logmessage("net_wan", "Failed to set IP address: %s", strerror(errno));
            close(sockfd);
            return -1;
        }
    }

    // Set netmask
    if (netmask) {
        addr = (struct sockaddr_in *)&ifr.ifr_netmask;
        addr->sin_family = AF_INET;
        if (inet_pton(AF_INET, netmask, &addr->sin_addr) <= 0) {
            logmessage("net_wan", "Invalid netmask: %s", netmask);
            close(sockfd);
            return -1;
        }
        if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
            logmessage("net_wan", "Failed to set netmask: %s", strerror(errno));
            close(sockfd);
            return -1;
        }
    }

    // Set broadcast address
    if (broadcast) {
        addr = (struct sockaddr_in *)&ifr.ifr_broadaddr;
        addr->sin_family = AF_INET;
        if (inet_pton(AF_INET, broadcast, &addr->sin_addr) <= 0) {
            logmessage("net_wan", "Invalid broadcast address: %s", broadcast);
            close(sockfd);
            return -1;
        }
        if (ioctl(sockfd, SIOCSIFBRDADDR, &ifr) < 0) {
            logmessage("net_wan", "Failed to set broadcast address: %s", strerror(errno));
            close(sockfd);
            return -1;
        }
    }

    // Set interface flags
    ifr.ifr_flags = flags;
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        logmessage("net_wan", "Failed to set interface flags: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}

static int bring_up_interface(const char *ifname, const char *ip, const char *netmask) {
    return configure_interface(ifname, ip, netmask, NULL, IFF_UP | IFF_RUNNING);
}

static int bring_down_interface(const char *ifname) {
    return configure_interface(ifname, NULL, NULL, NULL, 0);
}

char *
get_wan_unit_value(int unit, const char *param_name)
{
	char wanN_param[BUFFER_SIZE_64];

	snprintf(wanN_param, sizeof(wanN_param), "wan%d_%s", unit, param_name);
	return nvram_safe_get(wanN_param);
}

int get_wan_unit_value_int(int unit, const char *param_name)
{
	char wanN_param[BUFFER_SIZE_64];

	snprintf(wanN_param, sizeof(wanN_param), "wan%d_%s", unit, param_name);
	return nvram_get_int(wanN_param);
}

void set_wan_unit_value(int unit, const char *param_name, const char *value)
{
	char wanN_param[BUFFER_SIZE_64];

	snprintf(wanN_param, sizeof(wanN_param), "wan%d_%s", unit, param_name);
	nvram_set_temp(wanN_param, value);
}

void set_wan_unit_value_int(int unit, const char *param_name, int value)
{
	char wanN_param[BUFFER_SIZE_64];

	snprintf(wanN_param, sizeof(wanN_param), "wan%d_%s", unit, param_name);
	nvram_set_int_temp(wanN_param, value);
}

void set_wan_unit_param(int unit, const char *param_name)
{
	char wan_param[BUFFER_SIZE_64], wanN_param[BUFFER_SIZE_64];

	snprintf(wan_param, sizeof(wan_param), "wan_%s", param_name);
	snprintf(wanN_param, sizeof(wanN_param), "wan%d_%s", unit, param_name);

	nvram_set_temp(wanN_param, nvram_safe_get(wan_param));
}

static void
control_wan_led_isp_state(int is_wan_up, int is_modem_unit)
{
#if defined(BOARD_GPIO_LED_WAN)
	int front_led_wan = nvram_get_int("front_led_wan");

	if (front_led_wan == 2)
	{
		int has_link = 0;
		if (is_wan_up)
		{
			has_link = 1;
			if (!get_wan_wisp_active(&has_link) && !is_modem_unit)
				has_link = get_wan_ether_link_cached();
		}
		LED_CONTROL(BOARD_GPIO_LED_WAN, (is_wan_up && has_link) ? LED_ON : LED_OFF);
	}
	else if (front_led_wan == 3)
	{
		if (!is_wan_up)
			LED_CONTROL(BOARD_GPIO_LED_WAN, LED_OFF);
	}
#endif
}

static void
clear_wan_state(void)
{
	set_wan_unit_value_int(0, "err", 0);
	set_wan_unit_value_int(0, "uptime", 0);
	set_wan_unit_value_int(0, "dltime", 0);
	set_wan_unit_value_int(0, "bytes_rx", 0);
	set_wan_unit_value_int(0, "bytes_tx", 0);
	nvram_set_int_temp("l2tp_wan_t", 0);
	nvram_set_temp("vpnc_dns_t", "");
	nvram_set_temp("vpnc_dom_t", "");
}

void reset_man_vars(void)
{
	nvram_set_temp("wanx_ipaddr", "");
	nvram_set_temp("wanx_netmask", "");
	nvram_set_temp("wanx_gateway", "");
	nvram_set_temp("wanx_dns", "");
	nvram_set_temp("wanx_lease", "");
	nvram_set_temp("wanx_domain", "");
	nvram_set_temp("wanx_mtu", "");
	nvram_set_temp("wanx_routes", "");
	nvram_set_temp("wanx_routes_ms", "");
	nvram_set_temp("wanx_routes_rfc", "");
	nvram_set_int_temp("wanx_err", 0);

	nvram_set_temp("manv_ipaddr", "");
	nvram_set_temp("manv_netmask", "");
	nvram_set_temp("manv_gateway", "");
	nvram_set_temp("manv_mtu", "");
	nvram_set_temp("manv_routes", "");
	nvram_set_temp("manv_routes_ms", "");
	nvram_set_temp("manv_routes_rfc", "");
	nvram_set_int_temp("manv_err", 0);
}

void reset_wan_vars(void)
{
	int unit, wan_proto;
	char mac_buf[BUFFER_SIZE_24], *man_addr, *man_mask, *man_gate, *man_mtu;

	unit = 0;

	clear_wan_state();
	reset_man_vars();

	set_wan_unit_param(unit, "proto");
	set_wan_unit_param(unit, "dnsenable_x");
	set_wan_unit_param(unit, "vci");
	set_wan_unit_param(unit, "hostname");
	set_wan_unit_param(unit, "auth_mode");
	set_wan_unit_param(unit, "auth_user");
	set_wan_unit_param(unit, "auth_pass");

	set_wan_unit_value(unit, "dns", "");
	set_wan_unit_value(unit, "wins", "");
	set_wan_unit_value(unit, "lease", "");
	set_wan_unit_value(unit, "domain", "");
	set_wan_unit_value(unit, "routes", "");
	set_wan_unit_value(unit, "routes_ms", "");
	set_wan_unit_value(unit, "routes_rfc", "");
	set_wan_unit_value_int(unit, "err", 0);

	wan_proto = get_wan_proto(unit);

	if (wan_proto == IPV4_WAN_PROTO_IPOE_STATIC)
	{
		set_wan_unit_param(unit, "ipaddr");
		set_wan_unit_param(unit, "netmask");
		set_wan_unit_param(unit, "gateway");
		set_wan_unit_param(unit, "mtu");
	}
	else
	{
		set_wan_unit_value(unit, "ipaddr", IPV4_ADDRESS_ANY);
		set_wan_unit_value(unit, "netmask", IPV4_ADDRESS_ANY);
		set_wan_unit_value(unit, "gateway", IPV4_ADDRESS_ANY);
		set_wan_unit_value(unit, "mtu", "");
	}

	man_addr = IPV4_ADDRESS_ANY;
	man_mask = IPV4_ADDRESS_ANY;
	man_gate = IPV4_ADDRESS_ANY;
	man_mtu = "";

	int is_dhcp_client = nvram_match("x_DHCPClient", "0");
	if (is_dhcp_client)
	{
		if (wan_proto == IPV4_WAN_PROTO_PPTP ||
			wan_proto == IPV4_WAN_PROTO_L2TP ||
			(wan_proto == IPV4_WAN_PROTO_PPPOE && nvram_match("wan_pppoe_man", "1")))
		{
			man_addr = nvram_safe_get("wan_ipaddr");
			man_mask = nvram_safe_get("wan_netmask");
			man_gate = nvram_safe_get("wan_gateway");
			man_mtu = nvram_safe_get("wan_mtu");
		}
	}

	set_wan_unit_value(unit, "man_ipaddr", man_addr);
	set_wan_unit_value(unit, "man_netmask", man_mask);
	set_wan_unit_value(unit, "man_gateway", man_gate);
	set_wan_unit_value(unit, "man_mtu", man_mtu);

	if (wan_proto == IPV4_WAN_PROTO_PPPOE ||
		wan_proto == IPV4_WAN_PROTO_PPTP ||
		wan_proto == IPV4_WAN_PROTO_L2TP)
	{
		nvram_set_temp("wanx_ipaddr", man_addr);
		if (is_valid_ipv4(man_addr) && is_valid_ipv4(man_mask))
			nvram_set_temp("wanx_netmask", man_mask);

		if (is_valid_ipv4(man_gate))
			nvram_set_temp("wanx_gateway", man_gate);

		set_wan_unit_param(unit, "pppoe_username");
		set_wan_unit_param(unit, "pppoe_passwd");

		if (wan_proto == IPV4_WAN_PROTO_PPPOE)
			set_wan_unit_param(unit, "pppoe_idletime");
		else
			set_wan_unit_value_int(unit, "pppoe_idletime", 0);

		set_wan_unit_param(unit, "pppoe_txonly_x");
		set_wan_unit_param(unit, "pppoe_service");
		set_wan_unit_param(unit, "pppoe_ac");
		set_wan_unit_param(unit, "pppoe_man");
		set_wan_unit_param(unit, "pppoe_mtu");
		set_wan_unit_param(unit, "pppoe_mru");
		set_wan_unit_param(unit, "pptp_mtu");
		set_wan_unit_param(unit, "pptp_mru");
		set_wan_unit_param(unit, "l2tp_mtu");
		set_wan_unit_param(unit, "l2tp_mru");

		set_wan_unit_param(unit, "ppp_peer");
		set_wan_unit_param(unit, "ppp_auth");
		set_wan_unit_param(unit, "ppp_mppe");
		set_wan_unit_param(unit, "ppp_lcp");
		set_wan_unit_param(unit, "ppp_alcp");
		set_wan_unit_param(unit, "ppp_pppd");
	}

	mac_buf[0] = 0;
	mac_conv("wan_hwaddr_x", -1, mac_buf);
	if (strlen(mac_buf) == 17)
		nvram_set("wan_hwaddr", mac_buf);
	else
		nvram_set("wan_hwaddr", nvram_safe_get("il1macaddr"));

	set_wan_unit_param(unit, "hwaddr");

#if defined(USE_IPV6)
	reset_wan6_vars();
#endif
}

int get_vlan_vid_wan(void) {
    int vlan_vid;
    int is_vlan_filter;

    // Cache nvram_match result
    is_vlan_filter = nvram_match("vlan_filter", "1") ? 1 : 0;
    if (is_vlan_filter) {
        vlan_vid = nvram_get_int("vlan_vid_cpu");
    } else {
        vlan_vid = VLAN_ID_MIN;
    }

    if (!is_vlan_vid_valid(vlan_vid)) {
        vlan_vid = phy_vlan_pvid_wan_get();
    }

    return vlan_vid;
}

int get_wan_bridge_mode(void) {
    int bridge_mode;

    // Cache nvram_get_int result
    bridge_mode = nvram_get_int("wan_stb_x");

    if (bridge_mode < 0 || bridge_mode > 7) {
        bridge_mode = SWAPI_WAN_BRIDGE_DISABLE;
    }

    return bridge_mode;
}

int get_wan_bridge_iso_mode(int bridge_mode) {
    int bwan_isolation;

    // Cache nvram_get_int result
    bwan_isolation = nvram_get_int("wan_stb_iso");

    if (bwan_isolation < 0 || bwan_isolation > 2 || bridge_mode == SWAPI_WAN_BRIDGE_DISABLE) {
        bwan_isolation = SWAPI_WAN_BWAN_ISOLATION_NONE;
    }

    return bwan_isolation;
}

#if defined(USE_GMAC2_TO_GPHY) || defined(USE_GMAC2_TO_GSW)

#define BRPREFIX_WAN "brwan"

static void
config_soft_bridge_lan(void)
{
	char lan_ifname[BUFFER_SIZE_24];
	const char *wired_ifname;
	int igmp_snoop = nvram_get_int("ether_igmp");
	int wired_m2u = nvram_get_int("ether_m2u");

	snprintf(lan_ifname, sizeof(lan_ifname), "%s.%d", IFNAME_MAC, 1);

	if (get_wan_bridge_mode() != SWAPI_WAN_BRIDGE_DISABLE)
	{
		create_vlan_iface(IFNAME_MAC, 1, -1, -1, nvram_safe_get("lan_hwaddr"), 1);
#if defined(USE_IPV6)
		control_if_ipv6(lan_ifname, 0);
#endif
		br_add_del_if(IFNAME_BR, IFNAME_MAC, 0);
		br_add_del_if(IFNAME_BR, lan_ifname, 1);
		wired_ifname = lan_ifname;
	}
	else
	{
		br_add_del_if(IFNAME_BR, lan_ifname, 0);
		br_add_del_if(IFNAME_BR, IFNAME_MAC, 1);
		remove_vlan_iface(lan_ifname);
		wired_ifname = IFNAME_MAC;
	}

	brport_set_m2u(wired_ifname, (igmp_snoop && wired_m2u == 1) ? 1 : 0);
}

static void
disassembly_bridges_wan(void)
{
	int i;
	char br_ifname[BUFFER_SIZE_16];

	/* disassembly all wan bridges brwan5..brwan0 */
	for (i = 5; i >= 0; i--)
	{
		snprintf(br_ifname, sizeof(br_ifname), "%s%d", BRPREFIX_WAN, i);
		if (!is_interface_exist(br_ifname))
			continue;
		bring_down_interface(br_ifname);
		br_del_all_ifs(br_ifname, 1, 1);
		br_add_del_bridge(br_ifname, 0);
	}
}

static void
config_soft_bridge_wan(int br_id, int vid, int prio, const char *wan_ifname, const char *wan_hwaddr)
{
	char br_ifname[BUFFER_SIZE_16], bwan_ifname[BUFFER_SIZE_24];

	snprintf(br_ifname, sizeof(br_ifname), "%s%d", BRPREFIX_WAN, br_id);

	if (!is_interface_exist(br_ifname))
	{
		br_add_del_bridge(br_ifname, 1);
		br_set_stp(br_ifname, 0);
		br_set_fd(br_ifname, 2);
		br_set_param_int(br_ifname, "multicast_snooping", 0);
	}
	else
	{
		bring_down_interface(br_ifname);
	}

	set_interface_hwaddr(br_ifname, wan_hwaddr);
	set_interface_mtu(br_ifname, 1500);

	snprintf(bwan_ifname, sizeof(bwan_ifname), "%s.%d", IFNAME_MAC, vid);
	create_vlan_iface(IFNAME_MAC, vid, prio, -1, wan_hwaddr, 1);

#if defined(USE_IPV6)
	control_if_ipv6(wan_ifname, 0);
	control_if_ipv6(bwan_ifname, 0);
#endif
	br_add_del_if(br_ifname, wan_ifname, 1);
	br_add_del_if(br_ifname, bwan_ifname, 1);

	if (br_id < 2)
	{
		/* set isolation for INET and IPTV bridges */
		if (nvram_get_int("wan_stb_iso") == SWAPI_WAN_BWAN_ISOLATION_BETWEEN)
		{
			brport_set_param_int(wan_ifname, "isolate_mode", 1);
			brport_set_param_int(bwan_ifname, "isolate_mode", 1);
		}
	}
	else
	{
#if defined(USE_IPV6)
		control_if_ipv6(br_ifname, 0);
#endif
	}

	bring_up_interface(br_ifname, IPV4_ADDRESS_ANY, NULL);
}

static void
config_soft_bridges_other(int vinet_vid, int viptv_vid, int pvid_wan, const char *wan_hwaddr, int bridge_mode)
{
	int i, br_id, port_vid[4], port_pri[4] = {0};
	char port_ifname[BUFFER_SIZE_24];

	for (i = 0; i < 4; i++)
		port_vid[i] = -1;

	switch (bridge_mode)
	{
	case SWAPI_WAN_BRIDGE_LAN1:
	case SWAPI_WAN_BRIDGE_LAN1_LAN2:
	case SWAPI_WAN_BRIDGE_LAN1_LAN2_LAN3:
		port_vid[0] = nvram_get_int("vlan_vid_lan1");
		port_pri[0] = nvram_get_int("vlan_pri_lan1") & VLAN_PRIORITY_MAX;
		break;
	}

	switch (bridge_mode)
	{
	case SWAPI_WAN_BRIDGE_LAN2:
	case SWAPI_WAN_BRIDGE_LAN1_LAN2:
	case SWAPI_WAN_BRIDGE_LAN1_LAN2_LAN3:
		port_vid[1] = nvram_get_int("vlan_vid_lan2");
		port_pri[1] = nvram_get_int("vlan_pri_lan2") & VLAN_PRIORITY_MAX;
		break;
	}

	switch (bridge_mode)
	{
	case SWAPI_WAN_BRIDGE_LAN3:
	case SWAPI_WAN_BRIDGE_LAN3_LAN4:
	case SWAPI_WAN_BRIDGE_LAN1_LAN2_LAN3:
		port_vid[2] = nvram_get_int("vlan_vid_lan3");
		port_pri[2] = nvram_get_int("vlan_pri_lan3") & VLAN_PRIORITY_MAX;
		break;
	}

	switch (bridge_mode)
	{
	case SWAPI_WAN_BRIDGE_LAN4:
	case SWAPI_WAN_BRIDGE_LAN3_LAN4:
		port_vid[3] = nvram_get_int("vlan_vid_lan4");
		port_pri[3] = nvram_get_int("vlan_pri_lan4") & VLAN_PRIORITY_MAX;
		break;
	}

	for (i = 0; i < 4; i++)
	{
		if (port_vid[i] < VLAN_ID_MIN)
			continue;
		if (!is_vlan_vid_valid(port_vid[i]))
			port_vid[i] = pvid_wan;
		if (port_vid[i] == vinet_vid || port_vid[i] == viptv_vid)
			port_vid[i] = VLAN_ID_MIN;
	}

	br_id = 2;
	for (i = 0; i < 4; i++)
	{
		if (port_vid[i] < 2)
			continue;
		snprintf(port_ifname, sizeof(port_ifname), "%s.%d", IFNAME_MAC2, port_vid[i]);
		create_vlan_iface(IFNAME_MAC2, port_vid[i], port_pri[i], 1500, wan_hwaddr, 1);
		config_soft_bridge_wan(br_id, port_vid[i], port_pri[i], port_ifname, wan_hwaddr);
		br_id++;
	}
}
#endif

static void
config_vinet_wan(void) {
    int vlan_vid[2], vlan_pri, pvid_wan;
    int is_vlan_filter, is_vlan_ifname;
    char vinet_ifname[BUFFER_SIZE_24];
    const char *vinet_iflast, *wan_hwaddr;
#if defined(USE_SINGLE_MAC)
    const char *ifname_wan_cpu = IFNAME_MAC;
#else
    const char *ifname_wan_cpu = IFNAME_MAC2;
#endif
#if defined(USE_GMAC2_TO_GPHY) || defined(USE_GMAC2_TO_GSW)
    int bridge_mode = get_wan_bridge_mode();
#endif

    // Cache NVRAM values
    is_vlan_filter = nvram_match("vlan_filter", "1") ? 1 : 0;
    wan_hwaddr = nvram_safe_get("wan_hwaddr");

    if (is_vlan_filter) {
        vlan_vid[0] = nvram_get_int("vlan_vid_cpu");
        vlan_vid[1] = nvram_get_int("vlan_vid_iptv");
        vlan_pri = nvram_get_int("vlan_pri_cpu") & VLAN_PRIORITY_MAX;
    } else {
        vlan_vid[0] = VLAN_ID_MIN;
        vlan_vid[1] = VLAN_ID_MIN;
        vlan_pri = VLAN_PRIORITY_DEFAULT;
    }

    pvid_wan = phy_vlan_pvid_wan_get();

    if (!is_vlan_vid_valid(vlan_vid[0])) {
        vlan_vid[0] = pvid_wan;
        vlan_pri = VLAN_PRIORITY_DEFAULT;
    }

    if (!is_vlan_vid_valid(vlan_vid[1])) {
        vlan_vid[1] = pvid_wan;
    }

    /* update VLAN VID for raeth (HW_VLAN_TX) */
    hw_vlan_tx_map(6, vlan_vid[0]);
    hw_vlan_tx_map(7, vlan_vid[1]);

    is_vlan_ifname = 1;
    snprintf(vinet_ifname, sizeof(vinet_ifname), "%s.%d", ifname_wan_cpu, vlan_vid[0]);
#if !defined(USE_SINGLE_MAC)
#if !defined(USE_GMAC2_TO_GPHY) && !defined(USE_GMAC2_TO_GSW)
    /* case1: CPU Internet tagged: n, CPU IPTV tagged: n */
    /* case2: CPU Internet tagged: y, CPU IPTV tagged: y (common VID) */
    if (vlan_vid[0] == vlan_vid[1])
#else
    /* case: no VLAN filter */
    if (!is_vlan_filter)
#endif
    {
        is_vlan_ifname = 0;
        snprintf(vinet_ifname, sizeof(vinet_ifname), "%s", IFNAME_MAC2);
    }

    /* always prepare eth3 interface */
    bring_down_interface(IFNAME_MAC2);
    set_interface_hwaddr(IFNAME_MAC2, wan_hwaddr);
    set_interface_mtu(IFNAME_MAC2, 1500);
    bring_up_interface(IFNAME_MAC2, IPV4_ADDRESS_ANY, NULL);
#endif

    /* remove previous VLAN interface */
    vinet_iflast = get_wan_unit_value(0, "viflast");
    if (*vinet_iflast && strcmp(vinet_iflast, vinet_ifname)) {
        remove_vlan_iface(vinet_iflast);
    }

    /* create VLAN interface for INET */
    if (is_vlan_ifname) {
#if defined(USE_IPV6)
        control_if_ipv6(ifname_wan_cpu, 0);
#endif
        create_vlan_iface(ifname_wan_cpu, vlan_vid[0], vlan_pri, 1500, wan_hwaddr, 1);
    }

    set_wan_unit_value(0, "viflast", (is_vlan_ifname) ? vinet_ifname : "");

#if defined(USE_GMAC2_TO_GPHY) || defined(USE_GMAC2_TO_GSW)
    if (bridge_mode != SWAPI_WAN_BRIDGE_DISABLE && is_vlan_filter) {
        config_soft_bridges_other(vlan_vid[0], vlan_vid[1], pvid_wan, wan_hwaddr, bridge_mode);
    }
    if (bridge_mode != SWAPI_WAN_BRIDGE_DISABLE) {
        config_soft_bridge_wan(0, vlan_vid[0], vlan_pri, vinet_ifname, wan_hwaddr);
        snprintf(vinet_ifname, sizeof(vinet_ifname), "%s%d", BRPREFIX_WAN, 0);
    }
#endif

#if defined(USE_IPV6)
    if (get_ipv6_type() != IPV6_DISABLED) {
        control_if_ipv6(vinet_ifname, 1);
    }
#endif
    set_wan_unit_value(0, "ifname", vinet_ifname);
}

static void
config_apcli_wisp(void)
{
	int unit = 0;
	char *man_ifname;
	char *wisp_ifname = get_apcli_wisp_ifname();

	if (!wisp_ifname)
		return;

	man_ifname = get_man_ifname(unit);
	if (strlen(man_ifname) > 0 && !is_man_wisp(man_ifname))
	{
		bring_up_interface(man_ifname, IPV4_ADDRESS_ANY, NULL);
#if defined(USE_SINGLE_MAC)
		bring_down_interface(man_ifname);
#endif
	}

#if defined(USE_IPV6)
	if (get_ipv6_type() != IPV6_DISABLED)
		control_if_ipv6(wisp_ifname, 1);
#endif
	set_wan_unit_value(0, "ifname", wisp_ifname);
}

static int
wait_apcli_connected(const char *ifname, int wait_sec)
{
    int elapsed_time = 0;

    while (elapsed_time < wait_sec) {
        if (get_apcli_connected(ifname)) {
            return 1;
        }

        // Use a non-blocking delay mechanism
        struct timespec req = {0, 500000000}; // 500ms
        nanosleep(&req, NULL);
        elapsed_time += 0.5;
    }

    return 0;
}

static void
launch_viptv_wan(void)
{
	int vlan_vid[2], vlan_pri, pvid_wan;
	int is_vlan_filter, viptv_mode;
	char viptv_ifname[BUFFER_SIZE_24], *viptv_addr, *viptv_mask, *viptv_gate;
	const char *viptv_iflast, *vinet_iflast, *wan_hwaddr;
#if defined(USE_SINGLE_MAC)
	const char *ifname_wan_cpu = IFNAME_MAC;
#else
	const char *ifname_wan_cpu = IFNAME_MAC2;
#endif

	is_vlan_filter = (nvram_match("vlan_filter", "1")) ? 1 : 0;
	if (is_vlan_filter)
	{
		vlan_vid[0] = nvram_get_int("vlan_vid_cpu");
		vlan_vid[1] = nvram_get_int("vlan_vid_iptv");
		vlan_pri = nvram_get_int("vlan_pri_iptv") & VLAN_PRIORITY_MAX;
	}
	else
	{
		vlan_vid[0] = VLAN_ID_MIN;
		vlan_vid[1] = VLAN_ID_MIN;
		vlan_pri = VLAN_PRIORITY_DEFAULT;
	}

	pvid_wan = phy_vlan_pvid_wan_get();

	if (!is_vlan_vid_valid(vlan_vid[0]))
		vlan_vid[0] = pvid_wan;

	if (!is_vlan_vid_valid(vlan_vid[1]))
	{
		vlan_vid[1] = pvid_wan;
		vlan_pri = VLAN_PRIORITY_DEFAULT;
	}

	vinet_iflast = get_wan_unit_value(0, "viflast");
	viptv_iflast = nvram_safe_get("viptv_ifname");

	if (vlan_vid[1] == vlan_vid[0])
	{
		/* case1: CPU Internet tagged: n, CPU IPTV tagged: n */
		/* case2: CPU Internet tagged: y, CPU IPTV tagged: y (common VID) */

		if (*viptv_iflast && strcmp(viptv_iflast, vinet_iflast))
			remove_vlan_iface(viptv_iflast);

		viptv_ifname[0] = 0;
		nvram_set_temp("viptv_ifname", viptv_ifname);

		return;
	}

	/* case3: CPU Internet tagged: y, CPU IPTV tagged: y (different VID) */
	/* case4: CPU Internet tagged: n, CPU IPTV tagged: y */
	/* case5: CPU Internet tagged: y, CPU IPTV tagged: n */

	/* VLAN interface for IPTV */
	snprintf(viptv_ifname, sizeof(viptv_ifname), "%s.%d", ifname_wan_cpu, vlan_vid[1]);

	/* remove previous VLAN interface */
	if (*viptv_iflast && strcmp(viptv_iflast, vinet_iflast) && strcmp(viptv_iflast, viptv_ifname))
		remove_vlan_iface(viptv_iflast);

	wan_hwaddr = nvram_safe_get("wan_hwaddr");

	/* create VLAN interface for IPTV */
	create_vlan_iface(ifname_wan_cpu, vlan_vid[1], vlan_pri, 1500, wan_hwaddr, 1);

	nvram_set_temp("viptv_ifname", viptv_ifname);

#if defined(USE_GMAC2_TO_GPHY) || defined(USE_GMAC2_TO_GSW)
	if (get_wan_bridge_mode() != SWAPI_WAN_BRIDGE_DISABLE)
	{
		config_soft_bridge_wan(1, vlan_vid[1], vlan_pri, viptv_ifname, wan_hwaddr);
		snprintf(viptv_ifname, sizeof(viptv_ifname), "%s%d", BRPREFIX_WAN, 1);
	}
#endif

	/* disable rp_filter */
	set_interface_conf_int("ipv4", viptv_ifname, "rp_filter", 0);

	viptv_mode = nvram_get_int("viptv_mode");
	viptv_addr = nvram_safe_get("viptv_ipaddr");
	viptv_mask = nvram_safe_get("viptv_netmask");

	if (viptv_mode == 2 && is_valid_ipv4(viptv_addr) && is_valid_ipv4(viptv_mask))
	{
		char tmp[BUFFER_SIZE_32], prefix[BUFFER_SIZE_8];

		strncpy(prefix, "manv_", sizeof(prefix) - 1);
		prefix[sizeof(prefix) - 1] = '\0';
		nvram_set_temp(strcat_r(prefix, "ipaddr", tmp), viptv_addr);
		nvram_set_temp(strcat_r(prefix, "netmask", tmp), viptv_mask);
		bring_up_interface(viptv_ifname, viptv_addr, viptv_mask);

		/* default route via default gateway (metric 10) */
		viptv_gate = nvram_safe_get("viptv_gateway");
		if (is_valid_ipv4(viptv_gate))
		{
			nvram_set_temp(strcat_r(prefix, "gateway", tmp), viptv_gate);
			route_add(viptv_ifname, METRIC_DEFAULT, IPV4_ADDRESS_ANY, viptv_gate, IPV4_ADDRESS_ANY);
		}

		logmessage(LOGNAME, "%s %s (%s)", "IPTV MAN", "up", viptv_ifname);

		start_igmpproxy(viptv_ifname);
	}
	else if (viptv_mode == 1)
	{
		start_zcip_viptv(viptv_ifname);
	}
	else
	{
		start_udhcpc_viptv(viptv_ifname);
	}
}

static void
launch_wanx(char *man_ifname, int unit, int wait_dhcpc, int use_zcip)
{
	int man_mtu = get_wan_unit_value_int(unit, "man_mtu");
	char *man_addr = get_wan_unit_value(unit, "man_ipaddr");
	char *man_mask = get_wan_unit_value(unit, "man_netmask");

	if (!is_valid_ipv4(man_addr))
		man_addr = IPV4_ADDRESS_ANY;

	if (!is_valid_ipv4(man_addr) || !is_valid_ipv4(man_mask))
		man_mask = NULL;

	/* add a bit delay for AP-Client ready */
	if (is_man_wisp(man_ifname))
		wait_apcli_connected(man_ifname, TIMEOUT_3_SECONDS);

	if (use_zcip || !is_valid_ipv4(man_addr))
	{
		/* PPPoE connection not needed WAN physical address first, skip wait DHCP lease */
		/* PPTP and L2TP needed WAN physical first for create VPN tunnel, wait DHCP lease */
		if (!use_zcip)
		{
			start_udhcpc_wan(man_ifname, unit, wait_dhcpc);

			/* add delay 2s after eth3 up: gethostbyname delay issue (L2TP/PPTP) */
			if (wait_dhcpc)
				sleep(TIMEOUT_2_SECONDS);
		}
		else
			start_zcip_wan(man_ifname);
	}
	else
	{
		int man_err = 0;
		char *lan_addr = nvram_safe_get("lan_ipaddr");
		char *lan_mask = nvram_safe_get("lan_netmask");

		if (man_mtu >= 1300 && man_mtu < 1500)
			set_interface_mtu(man_ifname, man_mtu);

		if (is_same_subnet2(man_addr, lan_addr, man_mask, lan_mask))
		{
			man_err = 1;

			logmessage(LOGNAME, "%s (%s) ERROR! Static IP [%s/%s] in LAN subnet [%s/%s]!",
					   "MAN", man_ifname, man_addr, man_mask, lan_addr, lan_mask);
		}
		else
		{
			bring_up_interface(man_ifname, man_addr, man_mask);
			man_up(man_ifname, unit, 1);
		}

		nvram_set_int_temp("wanx_err", man_err);
	}

#if defined(USE_IPV6)
	if (is_wan_ipv6_type_sit() == 0 && !is_wan_ipv6_if_ppp())
		wan6_up(man_ifname, unit);
#endif
}

static void
wait_ppp_up(char *ppp_ifname, int unit)
{
	int timeout, sockfd;
	struct ifreq ifr;
	char *ppp_gate;

	/* Wait for pppX to be created */
	timeout = TIMEOUT_5_SECONDS;
	while (bring_up_interface(ppp_ifname, NULL, NULL) && timeout--)
		sleep(1);

	/* Retrieve IP info */
	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return;

	/* Store temporary IP address */
	ppp_gate = "";
	strncpy(ifr.ifr_name, ppp_ifname, IFNAMSIZ);
	if (ioctl(sockfd, SIOCGIFADDR, &ifr) == 0)
	{
		set_wan_unit_value(unit, "ipaddr", inet_ntoa(sin_addr(&ifr.ifr_addr)));
		set_wan_unit_value(unit, "netmask", IPV4_NETMASK_DEFAULT);

		/* Store temporary P-t-P address */
		if (ioctl(sockfd, SIOCGIFDSTADDR, &ifr) == 0)
		{
			ppp_gate = inet_ntoa(sin_addr(&ifr.ifr_dstaddr));
			set_wan_unit_value(unit, "gateway", ppp_gate);
		}
	}
	close(sockfd);

	/* Preset routes so that traffic can be sent to proper pppx even before the link is brought up. */
	preset_wan_ppp_routes(ppp_ifname, unit);

	/* Stimulate link up */
	if (is_valid_ipv4(ppp_gate))
	{
		if (send_icmp_ping(ppp_gate) < 0) {
			logmessage("wait_ppp_up", "Failed to send ICMP ping to %s", ppp_gate);
		}
	}
}

static void
create_cb_links(void)
{
	mkdir("/tmp/ppp", 0777);
	mkdir(PPP_PEERS_DIR, 0777);

	symlink("/sbin/rc", "/tmp/ppp/ip-up");
	symlink("/sbin/rc", "/tmp/ppp/ip-down");
	symlink("/sbin/rc", SCRIPT_UDHCPC_WAN);
	symlink("/sbin/rc", SCRIPT_UDHCPC_VIPTV);
	symlink("/sbin/rc", SCRIPT_ZCIP_WAN);
	symlink("/sbin/rc", SCRIPT_ZCIP_VIPTV);
	symlink("/sbin/rc", SCRIPT_WPACLI_WAN);
#if defined(USE_IPV6)
	symlink("/sbin/rc", "/tmp/ppp/ipv6-up");
	symlink("/sbin/rc", "/tmp/ppp/ipv6-down");
	symlink("/sbin/rc", SCRIPT_DHCP6C_WAN);
#endif
}

static void
remove_cb_links(void)
{
	/* Remove dynamically created links */
	unlink(SCRIPT_ZCIP_WAN);
	unlink(SCRIPT_ZCIP_VIPTV);
	unlink(SCRIPT_UDHCPC_WAN);
	unlink(SCRIPT_UDHCPC_VIPTV);
	unlink(SCRIPT_WPACLI_WAN);
	unlink("/tmp/ppp/ip-up");
	unlink("/tmp/ppp/ip-down");
#if defined(USE_IPV6)
	unlink(SCRIPT_DHCP6C_WAN);
	unlink("/tmp/ppp/ipv6-up");
	unlink("/tmp/ppp/ipv6-down");
#endif
}

void start_wan(void)
{
	int unit, wan_proto;
	char *wan_ifname;

	/* Create links */
	create_cb_links();

	config_vinet_wan();
	launch_viptv_wan();
	config_apcli_wisp();

	reload_nat_modules();

	update_resolvconf(1, 0);

	set_nf_conntrack();
	set_tcp_syncookies();
	set_igmp_mld_version();

	/* di wakeup after 60 secs */
	notify_run_detect_internet(TIMEOUT_60_SECONDS);

	/* Start each configured and enabled wan connection and its undelying i/f */
	for (unit = 0; unit < 1; unit++)
	{
		wan_proto = get_wan_proto(unit);
		if (wan_proto < 0)
			continue;

		/* make sure the connection exists and is enabled */
		wan_ifname = get_man_ifname(unit);
		if (strlen(wan_ifname) < 1)
			continue;

		/* bring up physical WAN interface */
		bring_up_interface(wan_ifname, IPV4_ADDRESS_ANY, IPV4_NETMASK_DEFAULT);

		/* perform ApCli reconnect */
		reconnect_apcli(wan_ifname, 0);

#if defined(USE_USB_SUPPORT)
		if (get_usb_modem_wan(unit))
		{
			if (nvram_get_int("modem_type") == 3)
			{
				launch_wan_usbnet(unit);
			}
			else
			{
				char *ppp_ifname = IFNAME_RAS;

				check_upnp_wanif_changed(ppp_ifname);
				set_wan_unit_value(unit, "proto_t", "RAS Modem");
				set_wan_unit_value(unit, "ifname_t", ppp_ifname);

				if (wan_proto == IPV4_WAN_PROTO_PPPOE ||
					wan_proto == IPV4_WAN_PROTO_PPTP ||
					wan_proto == IPV4_WAN_PROTO_L2TP)
				{
					int i_pppoe, i_pppoe_man;

					i_pppoe = (wan_proto == IPV4_WAN_PROTO_PPPOE) ? 1 : 0;
					i_pppoe_man = get_wan_unit_value_int(unit, "pppoe_man");
					if (!i_pppoe || i_pppoe_man == 1)
						launch_wanx(wan_ifname, unit, 0, 0);
					else if (i_pppoe && i_pppoe_man == 2)
						launch_wanx(wan_ifname, unit, 0, 1);
				}

				/* re-build iptables rules (first stage w/o WAN IP) */
				start_firewall_ex();

				launch_wan_modem_ras(unit);
			}
		}
		else
#endif
			if (wan_proto == IPV4_WAN_PROTO_PPPOE ||
				wan_proto == IPV4_WAN_PROTO_PPTP ||
				wan_proto == IPV4_WAN_PROTO_L2TP)
		{
			char *proto_desc, *ppp_ifname = IFNAME_PPP;
			int i_pppoe, i_pppoe_man, i_demand, wan_auth_mode;

			if (wan_proto == IPV4_WAN_PROTO_PPPOE)
				proto_desc = "PPPoE";
			else if (wan_proto == IPV4_WAN_PROTO_PPTP)
				proto_desc = "PPTP";
			else
				proto_desc = "L2TP";

			check_upnp_wanif_changed(ppp_ifname);
			set_wan_unit_value(unit, "proto_t", proto_desc);
			set_wan_unit_value(unit, "ifname_t", ppp_ifname);

			i_pppoe = (wan_proto == IPV4_WAN_PROTO_PPPOE) ? 1 : 0;
			i_pppoe_man = get_wan_unit_value_int(unit, "pppoe_man");

			/* Start eapol authenticator */
			wan_auth_mode = get_wan_unit_value_int(unit, "auth_mode");
			if (!i_pppoe && wan_auth_mode > 1)
				start_auth_eapol(wan_ifname, unit, wan_auth_mode - 2);

			if (!i_pppoe || i_pppoe_man == 1)
				launch_wanx(wan_ifname, unit, !i_pppoe, 0);
			else if (i_pppoe && i_pppoe_man == 2)
				launch_wanx(wan_ifname, unit, 0, 1);

			/* re-build iptables rules (first stage w/o WAN IP) */
			start_firewall_ex();

			/* update demand option */
			i_demand = get_wan_unit_value_int(unit, "pppoe_idletime");
			if (!i_pppoe || i_demand < 0)
				i_demand = 0;

			if (i_demand)
				i_demand = 1;

			set_wan_unit_value_int(unit, "pppoe_demand", i_demand);

			/* launch ppp client daemon */
			launch_wan_pppd(unit, wan_proto);

			/* Pretend that the WAN interface is up */
			if (i_demand)
			{
				wait_ppp_up(ppp_ifname, unit);
			}
		}
		else if (wan_proto == IPV4_WAN_PROTO_IPOE_DHCP ||
				 wan_proto == IPV4_WAN_PROTO_IPOE_STATIC)
		{
			int wan_auth_mode, wan_err = 0;

			/* Configure DHCP connection. */
			check_upnp_wanif_changed(wan_ifname);
			set_wan_unit_value(unit, "proto_t", "IPoE");
			set_wan_unit_value(unit, "ifname_t", wan_ifname);

			/* add a bit delay for AP-Client ready */
			if (is_man_wisp(wan_ifname))
				wait_apcli_connected(wan_ifname, TIMEOUT_3_SECONDS);

			/* Assign static IP address to i/f */
			if (wan_proto == IPV4_WAN_PROTO_IPOE_STATIC)
			{
				int wan_mtu = get_wan_unit_value_int(unit, "mtu");
				char *wan_addr = get_wan_unit_value(unit, "ipaddr");
				char *wan_mask = get_wan_unit_value(unit, "netmask");
				char *lan_addr = nvram_safe_get("lan_ipaddr");
				char *lan_mask = nvram_safe_get("lan_netmask");

				if (wan_mtu >= 1300 && wan_mtu < 1500)
					set_interface_mtu(wan_ifname, wan_mtu);

				if (is_same_subnet2(wan_addr, lan_addr, wan_mask, lan_mask))
				{
					wan_err = 1;

					logmessage(LOGNAME, "%s (%s) ERROR! Static IP [%s/%s] in LAN subnet [%s/%s]!",
							   "WAN", wan_ifname, wan_addr, wan_mask, lan_addr, lan_mask);
				}
				else
				{
					bring_up_interface(wan_ifname, wan_addr, wan_mask);
				}

				set_wan_unit_value_int(unit, "err", wan_err);
			}

			/* re-build iptables rules (final stage for IPoE static) */
			start_firewall_ex();

			/* Start eapol authenticator */
			wan_auth_mode = get_wan_unit_value_int(unit, "auth_mode");
			if (wan_auth_mode > 1)
				start_auth_eapol(wan_ifname, unit, wan_auth_mode - 2);

			/* We are done configuration */
			if (wan_proto == IPV4_WAN_PROTO_IPOE_STATIC)
			{
				if (!wan_err)
				{
					wan_up(wan_ifname, unit, 1);
					/* update UPnP forwards from lease file (after wan up) */
					update_upnp();
				}
			}
			else
				start_udhcpc_wan(wan_ifname, unit, 0);
#if defined(USE_IPV6)
			if (is_wan_ipv6_type_sit() == 0)
				wan6_up(wan_ifname, unit);
#endif
		}
	}

	set_passthrough_pppoe(1);
}

static void
stop_wan_ppp(void)
{
	// stop services only for pppX interface
	char *svcs_ppp[] = {
		"ip-up",
		"ip-down",
#if defined(USE_IPV6)
		"ipv6-up",
		"ipv6-down",
#endif
		"l2tpd",
		"xl2tpd",
		"pppd",
		NULL};

	nvram_set_int_temp("deferred_wanup_t", 0);

	notify_pause_detect_internet();

	stop_vpn_client();
	kill_services(svcs_ppp, 6, 1);

	clear_wan_state();

	control_wan_led_isp_state(0, 0);
}

void stop_wan(void)
{
	int unit, wan_proto;
	char *man_ifname;
	char *svcs_ppp[] = {
		"ip-up",
		"ip-down",
#if defined(USE_IPV6)
		"ipv6-up",
		"ipv6-down",
#endif
		"l2tpd",
		"xl2tpd",
		"pppd",
		NULL};
	char *svcs_wan[] = {
		"ntpd",
		"inadyn",
		"igmpproxy",
		"xupnpd",
		"udpxy",
		"udhcpc",
		"zcip",
		"detect_wan",
		NULL};

	unit = 0;

	nvram_set_int_temp("deferred_wanup_t", 0);

	wan_proto = get_wan_proto(unit);
	man_ifname = get_man_ifname(unit);

	notify_pause_detect_internet();

	stop_vpn_client();

#if defined(USE_IPV6)
	if (is_wan_ipv6_type_sit() == 0)
		wan6_down(man_ifname, unit);
#endif

	kill_services(svcs_ppp, 6, 1);

	if (kill_process_by_pidfile("/var/run/udhcpc0.pid", SIGUSR2) < 0) {
		logmessage("stop_wan", "Failed to send SIGUSR2 to udhcpc");
	}

	stop_auth_eapol();
	stop_auth_kabinet();
	set_passthrough_pppoe(0);

	kill_services(svcs_wan, 3, 1);

	if (wan_proto == IPV4_WAN_PROTO_IPOE_STATIC &&
		strcmp(man_ifname, get_wan_unit_value(unit, "ifname_t")) == 0)
		wan_down(man_ifname, unit, 1);

	/* Bring down WAN interfaces */
	bring_down_interface(man_ifname);
	if (strcmp(man_ifname, IFNAME_WAN) != 0)
		bring_down_interface(IFNAME_WAN);

#if defined(USE_USB_SUPPORT)
	/* Bring down usbnet interface */
	stop_wan_usbnet();
#endif

	/* Remove dynamically created links */
	remove_cb_links();

	clear_wan_state();

	control_wan_led_isp_state(0, 0);
}

static int
get_wan_unit(char *wan_ifname)
{
	int unit;

	for (unit = 0; unit < 1; unit++)
	{
		if (strcmp(wan_ifname, get_wan_unit_value(unit, "ifname_t")) == 0)
			return unit;
		if (strcmp(wan_ifname, get_man_ifname(unit)) == 0)
			return unit;
	}

	/* fallback to unit 0 */
	return 0;
}

static int
is_ifunit_man(char *wan_ifname, int unit)
{
	int wan_proto = get_wan_proto(unit);

	if (!is_usbnet_interface(wan_ifname) &&
		(wan_proto == IPV4_WAN_PROTO_PPPOE ||
		 wan_proto == IPV4_WAN_PROTO_PPTP ||
		 wan_proto == IPV4_WAN_PROTO_L2TP))
		return 1;

	return 0;
}

static int
is_ifunit_modem(char *wan_ifname, int unit)
{
	char *ifname_temp;

	if (!get_usb_modem_wan(unit))
		return 0;

	if (ppp_ifindex(wan_ifname) >= RAS_PPP_UNIT)
		return 1;

	ifname_temp = get_wan_unit_value(unit, "ifname_t");
	if (is_usbnet_interface(ifname_temp) && strcmp(wan_ifname, ifname_temp) == 0)
		return 2;

	return 0;
}

static void
add_man_gateway_routes(char *man_ifname, int unit, int metric)
{
	int is_man_dns_auto, is_modem_active, wan_proto;
	char *next, *man_gate, *man_addr, *man_mask, *man_dns;
	char word[BUFFER_SIZE_100];

	man_gate = nvram_safe_get("wanx_gateway");
	if (!is_valid_ipv4(man_gate))
		return;

	wan_proto = get_wan_proto(unit);
	is_modem_active = get_usb_modem_wan(unit);
	is_man_dns_auto = (get_wan_unit_value_int(unit, "dnsenable_x") > 0) ? 1 : 0;

	man_addr = nvram_safe_get("wanx_ipaddr");
	man_mask = nvram_safe_get("wanx_netmask");
	man_dns = nvram_safe_get("wanx_dns");

	/* if the gateway is out of the subnet */
	if (!is_same_subnet(man_gate, man_addr, man_mask))
		route_add(man_ifname, metric, man_gate, NULL, IPV4_NETMASK_DEFAULT);

	/* default route via default gateway */
	route_add(man_ifname, metric, IPV4_ADDRESS_ANY, man_gate, IPV4_ADDRESS_ANY);

	/* ... and to dns servers as well for demand ppp to work */
	if (is_man_dns_auto && !is_modem_active && wan_proto != IPV4_WAN_PROTO_PPPOE)
	{
		foreach (word, man_dns, next)
		{
			if (is_valid_ipv4(word) && inet_addr(word) != inet_addr(man_gate) &&
				!is_same_subnet(word, man_addr, man_mask))
				route_add(man_ifname, metric, word, man_gate, IPV4_NETMASK_DEFAULT);
		}
	}
}

void man_up(char *man_ifname, int unit, int is_static)
{
	logmessage(LOGNAME, "%s %s (%s)", "MAN", "up", man_ifname);

	/* setup static wan routes via physical device */
	add_static_man_routes(man_ifname);

	if (!is_static)
	{
		/* and routes supplied via DHCP */
		add_dhcp_routes_by_prefix("wanx_", man_ifname, 0);
	}

	/* and default route (metric 2) */
	add_man_gateway_routes(man_ifname, unit, METRIC_LOW);

	if (!is_static)
	{
		/* update resolv.conf content */
		update_resolvconf(0, 0);

		/* re-start firewall */
		notify_rc("restart_firewall_wan");
	}

	/* start multicast router */
	start_igmpproxy(man_ifname);
}

void man_down(char *man_ifname, int unit)
{
	logmessage(LOGNAME, "%s %s (%s)", "MAN", "down", man_ifname);

	/* for update dnsmasq.servers */
	nvram_set_temp("wanx_domain", "");

	/* stop multicast router */
	stop_igmpproxy(man_ifname);
}

void wan_up(char *wan_ifname, int unit, int is_static)
{
	char wan_cnt[BUFFER_SIZE_32], *wan_addr, *wan_mask, *wan_gate;
	const char *script_postw = SCRIPT_POST_WAN;
	int wan_proto, modem_unit_id;

	logmessage(LOGNAME, "%s %s (%s)", "WAN", "up", wan_ifname);

	wan_proto = get_wan_proto(unit);
	modem_unit_id = is_ifunit_modem(wan_ifname, unit);

	snprintf(wan_cnt, sizeof(wan_cnt), "%ld", uptime());
	set_wan_unit_value(unit, "uptime", wan_cnt);

	if (modem_unit_id || wan_proto != IPV4_WAN_PROTO_IPOE_STATIC)
	{
		snprintf(wan_cnt, sizeof(wan_cnt), "%llu", get_ifstats_bytes_rx(wan_ifname));
		set_wan_unit_value(unit, "bytes_rx", wan_cnt);

		snprintf(wan_cnt, sizeof(wan_cnt), "%llu", get_ifstats_bytes_tx(wan_ifname));
		set_wan_unit_value(unit, "bytes_tx", wan_cnt);
	}

	wan_addr = get_wan_unit_value(unit, "ipaddr");
	wan_mask = get_wan_unit_value(unit, "netmask");
	wan_gate = get_wan_unit_value(unit, "gateway");

	if (!is_valid_ipv4(wan_gate))
		wan_gate = NULL;

	/* Set default route to gateway if specified (for NDIS or IPoE) */
	if (wan_gate && ppp_ifindex(wan_ifname) < 0)
	{
		/* if the gateway is out of the subnet */
		if (!is_same_subnet(wan_gate, wan_addr, wan_mask))
			route_add(wan_ifname, METRIC_HIGH, wan_gate, NULL, IPV4_NETMASK_DEFAULT);
	}

	/* default route via default gateway */
	if (wan_gate)
		route_add(wan_ifname, METRIC_HIGH, IPV4_ADDRESS_ANY, wan_gate, IPV4_ADDRESS_ANY);

#if 0
	/* hack: avoid routing cycles, when both peer and server has the same IP (for PPTP or L2TP) */
	if (!modem_unit_id && (wan_proto == IPV4_WAN_PROTO_PPTP || wan_proto == IPV4_WAN_PROTO_L2TP)) {
		/* delete gateway route as it's no longer needed */
		if (wan_gate)
			route_del(wan_ifname, 0, wan_gate, "0.0.0.0", "255.255.255.255");
	}
#endif

	/* Install interface dependent static routes */
	add_static_wan_routes(wan_ifname);

	/* Add static MAN routes (for IPoE) */
	if (!modem_unit_id && (wan_proto == IPV4_WAN_PROTO_IPOE_STATIC || wan_proto == IPV4_WAN_PROTO_IPOE_DHCP))
	{
		nvram_set_temp("wanx_gateway", (wan_gate) ? wan_gate : IPV4_ADDRESS_ANY);
		add_static_man_routes(wan_ifname);
	}

	/* Add dynamic routes supplied via DHCP (for NDIS or IPoE Auto) */
	if ((modem_unit_id == 2) || (!modem_unit_id && wan_proto == IPV4_WAN_PROTO_IPOE_DHCP))
	{
		char prefix[BUFFER_SIZE_16];
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		add_dhcp_routes_by_prefix(prefix, wan_ifname, 0);
	}

#if defined(USE_IPV6)
	if (is_wan_ipv6_type_sit() == 1)
		wan6_up(wan_ifname, unit);
#endif

	/* update resolv.conf content */
	update_resolvconf(0, 0);

	if (!is_static)
	{
		/* re-start firewall */
		notify_rc("restart_firewall_wan");
	}

	/* Start kabinet authenticator (for IPoE) */
	if (!modem_unit_id && (wan_proto == IPV4_WAN_PROTO_IPOE_STATIC || wan_proto == IPV4_WAN_PROTO_IPOE_DHCP))
	{
		if (get_wan_unit_value_int(unit, "auth_mode") == 1)
			start_auth_kabinet();
	}

	/* start multicast router (for NDIS or IPoE) */
	if (ppp_ifindex(wan_ifname) < 0)
		start_igmpproxy(wan_ifname);

	/* notify DDNS client */
	notify_ddns_update();

	/* notify watchdog for NTP update */
	notify_watchdog_time();

	/* start gateway ARP checker (for IPoE Auto) */
	if (!modem_unit_id && wan_proto == IPV4_WAN_PROTO_IPOE_DHCP)
	{
		if (nvram_match("gw_arp_ping", "1") && !pids("detect_wan"))
			detect_wan();
	}

	if (wan_gate)
		control_wan_led_isp_state(1, modem_unit_id);

	/* di wakeup after 2 secs */
	notify_run_detect_internet(TIMEOUT_2_SECONDS);

	/* call custom user script */
	if (check_if_file_exist(script_postw)) {
		pid_t pid = fork();
		if (pid == 0) {
			execlp("/bin/sh", "sh", "-c", script_postw, "up", wan_ifname, wan_addr, NULL);
			_exit(EXIT_FAILURE);
		} else if (pid < 0) {
			logmessage("wan_up", "Failed to fork: %s", strerror(errno));
		}
	}

	/* deferred start static VPN client and add delay 10s */
	sleep(TIMEOUT_10_SECONDS);
	notify_rc("restart_vpn_client");
}

void wan_down(char *wan_ifname, int unit, int is_static)
{
	char *wan_addr, *wan_gate;
	const char *script_postw = SCRIPT_POST_WAN;
	int wan_proto, modem_unit_id;

	logmessage(LOGNAME, "%s %s (%s)", "WAN", "down", wan_ifname);

	notify_pause_detect_internet();

	/* deferred stop static VPN client (prevent rebuild resolv.conf) */
	nvram_set_temp("vpnc_dns_t", "");
	if (!is_static)
		notify_rc("stop_vpn_client");

#if defined(USE_IPV6)
	if (is_wan_ipv6_type_sit() == 1)
		wan6_down(wan_ifname, unit);
#endif

	wan_proto = get_wan_proto(unit);
	modem_unit_id = is_ifunit_modem(wan_ifname, unit);

	/* Stop multicast router (for NDIS or IPoE) */
	if (ppp_ifindex(wan_ifname) < 0)
		stop_igmpproxy(wan_ifname);

	/* Stop kabinet authenticator (for IPoE) */
	if (!modem_unit_id && (wan_proto == IPV4_WAN_PROTO_IPOE_STATIC || wan_proto == IPV4_WAN_PROTO_IPOE_DHCP))
	{
		if (get_wan_unit_value_int(unit, "auth_mode") == 1)
			stop_auth_kabinet();
	}

	/* Remove default route to gateway if specified */
	wan_gate = get_wan_unit_value(unit, "gateway");
	if (is_valid_ipv4(wan_gate))
		route_del(wan_ifname, METRIC_HIGH, IPV4_ADDRESS_ANY, wan_gate, IPV4_ADDRESS_ANY);

	/* Remove interface dependent static routes */
	del_static_wan_routes(wan_ifname);

	/* leave as is if no dns servers left for demand to work */
	if (wan_proto == IPV4_WAN_PROTO_IPOE_STATIC || wan_proto == IPV4_WAN_PROTO_IPOE_DHCP ||
		strlen(nvram_safe_get("wanx_dns")) > 6)
		set_wan_unit_value(unit, "dns", "");

	/* update resolv.conf content */
	update_resolvconf(0, 0);

	if (!modem_unit_id && wan_proto == IPV4_WAN_PROTO_IPOE_STATIC)
		bring_up_interface(wan_ifname, IPV4_ADDRESS_ANY, NULL);

	/* flush conntrack table (only old WAN IP records) */
	wan_addr = get_wan_unit_value(unit, "ipaddr");
	if (is_valid_ipv4(wan_addr))
		flush_conntrack_table(wan_addr);
	else
		wan_addr = IPV4_ADDRESS_ANY;

	control_wan_led_isp_state(0, 0);

	set_wan_unit_value_int(unit, "uptime", 0);
	set_wan_unit_value_int(unit, "dltime", 0);
	set_wan_unit_value_int(unit, "bytes_rx", 0);
	set_wan_unit_value_int(unit, "bytes_tx", 0);

	if (check_if_file_exist(script_postw)) {
		pid_t pid = fork();
		if (pid == 0) {
			execlp("/bin/sh", "sh", "-c", script_postw, "down", wan_ifname, wan_addr, NULL);
			_exit(EXIT_FAILURE);
		} else if (pid < 0) {
			logmessage("wan_down", "Failed to fork: %s", strerror(errno));
		}
	}
}

void full_restart_wan(void)
{
	if (get_ap_mode())
		return;

	stop_wan();

	clear_if_route4(get_man_ifname(0));
	clear_if_route4(IFNAME_BR);
	flush_route_caches();

	phy_clear_mac_table();

	update_router_mode();

	reset_wan_vars();

	add_static_lan_routes(IFNAME_BR);

	select_usb_modem_to_wan();

	notify_reset_detect_link();
	switch_config_vlan(0);

#if defined(USE_GMAC2_TO_GPHY) || defined(USE_GMAC2_TO_GSW)
	disassembly_bridges_wan();
	config_soft_bridge_lan();
#endif

	start_wan();

	/* restore L2TP VPN server after L2TP WAN client closed */
	if (nvram_match("l2tp_srv_t", "1"))
		safe_start_xl2tpd();
}

void try_wan_reconnect(int try_use_modem, long pause_in_seconds)
{
	if (get_ap_mode())
		return;

	stop_wan();
	reset_wan_vars();

	if (pause_in_seconds > 0)
	{
		long deferred_up_time = uptime() + pause_in_seconds;

		if (deferred_up_time == 0)
			deferred_up_time++;
		nvram_set_int_temp("deferred_wanup_t", deferred_up_time);

		/* check watchdog started */
		start_watchdog();

		logmessage(LOGNAME, "WAN up delay: %lds.", pause_in_seconds);

		return;
	}

	if (try_use_modem)
		select_usb_modem_to_wan();

	notify_reset_detect_link();

	start_wan();

	/* restore L2TP VPN server after L2TP WAN client closed */
	if (nvram_match("l2tp_srv_t", "1"))
		safe_start_xl2tpd();
}

void deferred_wan_connect(void)
{
	/* check wan already started */
	if (check_if_file_exist(SCRIPT_UDHCPC_WAN))
		return;

	nvram_set_int_temp("deferred_wanup_t", 0);

	select_usb_modem_to_wan();

	notify_reset_detect_link();

	start_wan();

	/* restore L2TP VPN server after L2TP WAN client closed */
	if (nvram_match("l2tp_srv_t", "1"))
		safe_start_xl2tpd();
}

void manual_wan_disconnect(void)
{
	int wan_proto;

	logmessage(LOGNAME, "Perform WAN %s %s", "manual", "disconnect");

	if (get_ap_mode())
		return;

	wan_proto = get_wan_proto(0);
	if (get_usb_modem_wan(0))
	{
		if (nvram_get_int("modem_type") == 3)
			stop_wan();
		else
			stop_wan_ppp();
	}
	else if (wan_proto == IPV4_WAN_PROTO_PPPOE ||
			 wan_proto == IPV4_WAN_PROTO_PPTP ||
			 wan_proto == IPV4_WAN_PROTO_L2TP)
	{
		/* pptp, l2tp, pppoe */
		stop_wan_ppp();
	}
	else
	{
		/* dhcp, static */
		stop_wan();
	}
}

void manual_wan_reconnect(void)
{
	logmessage(LOGNAME, "Perform WAN %s %s", "manual", "reconnect");

	try_wan_reconnect(1, 0);
}

void manual_wisp_reassoc(void)
{
	char *wisp_ifname = get_apcli_wisp_ifname();

	if (!wisp_ifname)
		return;

	logmessage(LOGNAME, "Perform WISP %s %s", "manual", "reassoc");

	reconnect_apcli(wisp_ifname, 1);
}

void auto_wan_reconnect(void)
{
	logmessage(LOGNAME, "Perform WAN %s %s", "auto", "reconnect");

	try_wan_reconnect(1, 0);
}

void auto_wan_reconnect_pause(void)
{
	logmessage(LOGNAME, "Perform WAN %s %s", "auto", "reconnect");

	try_wan_reconnect(1, nvram_get_int("di_recon_pause"));
}

void notify_on_wan_ether_link_restored(void)
{
	int unit = 0;
	int wan_proto = get_wan_proto(unit);

	if (wan_proto == IPV4_WAN_PROTO_IPOE_STATIC || nvram_match("x_DHCPClient", "0"))
		return;

	if (is_usbnet_interface(get_wan_unit_value(unit, "ifname_t")))
		return;

	if (get_wan_wisp_active(NULL))
		return;

	if (renew_udhcpc_wan(unit) == 0)
	{
		logmessage(LOGNAME, "force WAN DHCP client renew...");
	}
}

void notify_on_internet_state_changed(int has_internet, long elapsed)
{
	const char *script_inet = SCRIPT_INTERNET_STATE;

	if (!has_internet && !get_ap_mode())
	{
		int fail_action = nvram_safe_get_int("di_lost_action", 0, 0, 3);
		switch (fail_action)
		{
		case 1:
			logmessage(LOGNAME, "Perform router auto-reboot on %s event", "Internet lost");
			notify_rc(RCN_RESTART_REBOOT);
			return;
		case 2:
			notify_rc("auto_wan_reconnect_pause");
			break;
#if defined(USE_USB_SUPPORT)
		case 3:
			if (get_modem_devnum())
			{
				nvram_set_int("modem_prio", (get_usb_modem_wan(0)) ? 0 : 1);
				notify_rc("auto_wan_reconnect");
			}
			break;
#endif
		}
	}

	if (check_if_file_exist(script_inet)) {
		pid_t pid = fork();
		if (pid == 0) {
			char elapsed_str[32], has_internet_str[32];
			snprintf(elapsed_str, sizeof(elapsed_str), "%ld", elapsed);
			snprintf(has_internet_str, sizeof(has_internet_str), "%d", has_internet);
			execlp("/bin/sh", "sh", "-c", script_inet, has_internet_str, elapsed_str, NULL);
			_exit(EXIT_FAILURE);
		} else if (pid < 0) {
			logmessage("notify_on_internet_state_changed", "Failed to fork: %s", strerror(errno));
		}
	}
}

int update_resolvconf(int is_first_run, int do_not_notify) {
    FILE *fp;
    static char word[BUFFER_SIZE_256]; // Reduced size and made static
    char *next, *wan_dns;
    const char *google_dns = IPV4_GOOGLE_DNS;
    const char *resolv_temp = "/etc/resolv.tmp";
    int i, i_pdns, i_total_dns = 0;
    int lock, dns_static, resolv_changed = 0;

    if (!is_first_run)
        fill_dnsmasq_servers();

    lock = file_lock("resolv");

    i_pdns = nvram_get_int("vpnc_pdns");
    dns_static = get_wan_dns_static();

    fp = fopen((is_first_run) ? DNS_RESOLV_CONF : resolv_temp, "w+");
    if (fp) {
        fprintf(fp, "nameserver %s\n", "127.0.0.1");

        if (!is_first_run && i_pdns > 0) {
            wan_dns = nvram_safe_get("vpnc_dns_t");
            if (strlen(wan_dns) > 6) {
                foreach (word, wan_dns, next) {
                    if (is_valid_ipv4(word)) {
                        fprintf(fp, "nameserver %s\n", word);
                        i_total_dns++;
                    }
                }
            }
        }

        if (i_pdns != 2 || i_total_dns < 1) {
            if (dns_static) {
                char dns_name_x[BUFFER_SIZE_16];

                if (is_first_run)
                    resolv_changed = 1;

                for (i = 1; i <= 3; i++) {
                    sprintf(dns_name_x, "wan_dns%d_x", i);
                    wan_dns = nvram_safe_get(dns_name_x);
                    if (is_valid_ipv4(wan_dns)) {
                        fprintf(fp, "nameserver %s\n", wan_dns);
                        i_total_dns++;
                    }
                }
            } else if (!is_first_run) {
                wan_dns = get_wan_unit_value(0, "dns");
                if (strlen(wan_dns) < 7)
                    wan_dns = nvram_safe_get("wanx_dns");

                foreach (word, wan_dns, next) {
                    if (is_valid_ipv4(word)) {
                        fprintf(fp, "nameserver %s\n", word);
                        i_total_dns++;
                    }
                }
            }
        }

        if (i_total_dns < 1)
            fprintf(fp, "nameserver %s\n", google_dns);

#if defined(USE_IPV6)
        wan_dns = get_wan_unit_value(0, "dns6");
        foreach (word, wan_dns, next) {
            if (strlen(word) > 0) {
                char dns6s[INET6_ADDRSTRLEN] = {0};
                if (ipv6_compact(word, dns6s, 0) == 0) {
                    fprintf(fp, "nameserver %s\n", dns6s);
                    if (is_first_run)
                        resolv_changed = 1;
                }
            }
        }
#endif
        fclose(fp);
    }

    if (!is_first_run) {
        if (compare_text_files(DNS_RESOLV_CONF, resolv_temp) != 0) {
            rename(resolv_temp, DNS_RESOLV_CONF);
            resolv_changed = 1;
        }
        unlink(resolv_temp);
    }

    file_unlock(lock);

    if (!resolv_changed && get_usb_modem_wan(0))
        resolv_changed = 1;

    if (resolv_changed && !do_not_notify)
        restart_dns();

    return 0;
}

int update_hosts_router(const char *lan_ipaddr)
{
	FILE *fp;
	char *lan_hname, *lan_dname;

	lan_hname = get_our_hostname();
	lan_dname = nvram_safe_get("lan_domain");

	sethostname(lan_hname, strlen(lan_hname));
	setdomainname(lan_dname, strlen(lan_dname));

	fp = fopen("/etc/hostname", "w+");
	if (fp)
	{
		fprintf(fp, "%s\n", lan_hname);
		fclose(fp);
	}

	fp = fopen("/etc/hosts", "w+");
	if (fp)
	{
		fprintf(fp, "%s %s %s\n", "127.0.0.1", "localhost.localdomain", "localhost");
		if (strlen(lan_dname) > 0)
			fprintf(fp, "%s %s.%s %s\n", lan_ipaddr, lan_hname, lan_dname, lan_hname);
		else
			fprintf(fp, "%s %s\n", lan_ipaddr, lan_hname);
		fprintf(fp, "%s %s\n", lan_ipaddr, "my.router");

		/* load static DHCP list */
		load_user_config(fp, "/tmp", "hosts.static", NULL);

#if defined(USE_IPV6)
		if (get_ipv6_type() != IPV6_DISABLED)
		{
			fprintf(fp, "%s %s %s\n", "::1", "localhost.localdomain", "localhost");
			char addr6s[INET6_ADDRSTRLEN];
			char *lan_addr6_host = get_lan_addr6_host(addr6s);
			if (lan_addr6_host)
			{
				if (strlen(lan_dname) > 0)
					fprintf(fp, "%s %s.%s %s\n", lan_addr6_host, lan_hname, lan_dname, lan_hname);
				else
					fprintf(fp, "%s %s\n", lan_addr6_host, lan_hname);
				fprintf(fp, "%s %s\n", lan_addr6_host, "my.router");
			}
		}
#endif
		fclose(fp);
	}

	return 0;
}

void add_dhcp_routes(char *rt_orig, char *rt_rfc_orig, char *rt_ms_orig, char *ifname, int metric) {
    char *routes_copy = NULL; // Initialize to NULL
    char *tmp;
    struct in_addr mask;
    char *ipaddr, *gateway;
    char netmask_buf[INET_ADDRSTRLEN]; // Buffer for inet_ntoa result

    if (!nvram_match("dr_enable_x", "1")) {
        return;
    }

    // --- Process standard routes ('routes' option) ---
    if (rt_orig && *rt_orig) {
        routes_copy = strdup(rt_orig); // Create a mutable copy
        if (!routes_copy) {
            logmessage("add_dhcp_routes", "Failed to allocate memory for standard routes copy");
            // No return here, try processing classless routes anyway
        } else {
            tmp = routes_copy; // Process the copy
            // Format is expected to be: ip/mask gateway ip/mask gateway ...
            // However, the original logic seemed to expect ip/gateway pairs?
            // Let's assume the original loop logic was parsing "ip/bits gateway" or similar,
            // but the simple route option usually is just "ip gateway".
            // Re-implementing based on the likely intended parsing of space-separated "ip gateway" pairs:
            while ((ipaddr = strsep(&tmp, " ")) != NULL) { // Get IP address
                gateway = strsep(&tmp, " "); // Get Gateway
                if (gateway == NULL) {
                    logmessage("add_dhcp_routes", "Malformed standard route entry (missing gateway?) for: %s", ipaddr);
                    break; // Stop processing this malformed string
                }
                if (is_valid_ipv4(ipaddr) && is_valid_ipv4(gateway)) {
                    // Standard routes usually imply host routes unless mask is given
                    // Using default netmask here as per original logic
                    route_add(ifname, metric, ipaddr, gateway, IPV4_NETMASK_DEFAULT);
                } else {
                     logmessage("add_dhcp_routes", "Invalid IP/Gateway in standard route: %s / %s", ipaddr, gateway);
                }
            }
            free(routes_copy); // Free the allocated memory
            routes_copy = NULL; // Reset pointer after free
        }
    }

    // --- Process RFC3442 or MS Classless Static Routes ---
    // Select the source string (prefer RFC over MS)
    char *classless_routes_orig = rt_rfc_orig && *rt_rfc_orig ? rt_rfc_orig : rt_ms_orig;

    if (classless_routes_orig && *classless_routes_orig) {
        routes_copy = strdup(classless_routes_orig); // Create a mutable copy
        if (!routes_copy) {
            logmessage("add_dhcp_routes", "Failed to allocate memory for classless routes copy");
            return; // Cannot proceed without memory
        } else {
            tmp = routes_copy; // Process the copy
            // Format is expected to be: ip/bits gateway ip/bits gateway ...
            while (tmp && *tmp) { // Ensure we don't process empty strings
                char *dest_and_mask = strsep(&tmp, " "); // Get "ip/bits" part
                gateway = strsep(&tmp, " ");             // Get "gateway" part

                if (dest_and_mask == NULL || gateway == NULL) {
                     logmessage("add_dhcp_routes", "Malformed classless route entry: %s", routes_copy + (tmp - routes_copy) ); // Log remaining part
                     break; // Stop processing malformed string
                }

                // Separate ip from bits
                char *slash = strchr(dest_and_mask, '/');
                long bits = -1; // Initialize bits to invalid
                char *endptr = NULL;

                if (slash) {
                    *slash = '\0'; // Null-terminate the IP address part
                    ipaddr = dest_and_mask;
                    char *bits_str = slash + 1;
                    if (bits_str && *bits_str) {
                         errno = 0; // Reset errno before strtol
                         bits = strtol(bits_str, &endptr, 10);
                         // Check for conversion errors
                         if (errno != 0 || endptr == bits_str || *endptr != '\0' || bits < 0 || bits > 32) {
                              logmessage("add_dhcp_routes", "Invalid CIDR bits in classless route: %s (original: %s/%s)", bits_str, ipaddr, bits_str);
                              bits = -1; // Mark as invalid
                         }
                    }
                } else {
                    // If no slash, treat as host route? Or error? Assuming error based on format.
                    ipaddr = dest_and_mask;
                    logmessage("add_dhcp_routes", "Missing CIDR mask in classless route entry: %s", ipaddr);
                    bits = -1; // Mark as invalid
                }

                // Check validity and add route
                if (bits >= 0 && is_valid_ipv4(ipaddr) && is_valid_ipv4(gateway)) {
                    mask.s_addr = (bits == 0) ? 0 : htonl(0xFFFFFFFF << (32 - bits)); // Calculate netmask
                    snprintf(netmask_buf, sizeof(netmask_buf), "%s", inet_ntoa(mask));
                    route_add(ifname, metric, ipaddr, gateway, netmask_buf);
                } else {
                     logmessage("add_dhcp_routes", "Skipping invalid classless route entry: %s/%ld via %s", ipaddr ? ipaddr : "(null)", bits, gateway);
                }
            }
            free(routes_copy); // Free the allocated memory
            routes_copy = NULL; // Reset pointer after free
        }
    }
}

void add_dhcp_routes_by_prefix(char *prefix, char *ifname, int metric)
{
	char buf[BUFFER_SIZE_32];
	char *rt, *rt_rfc, *rt_ms;

	rt = nvram_safe_get(strcat_r(prefix, "routes", buf));
	rt_rfc = nvram_safe_get(strcat_r(prefix, "routes_rfc", buf));
	rt_ms = nvram_safe_get(strcat_r(prefix, "routes_ms", buf));

	add_dhcp_routes(rt, rt_rfc, rt_ms, ifname, metric);
}

int add_static_wan_routes(char *wan_ifname)
{
	return control_static_routes(SR_PREFIX_WAN, wan_ifname, 1);
}

int del_static_wan_routes(char *wan_ifname)
{
	return control_static_routes(SR_PREFIX_WAN, wan_ifname, 0);
}

int add_static_man_routes(char *man_ifname)
{
	return control_static_routes(SR_PREFIX_MAN, man_ifname, 1);
}

int del_static_man_routes(char *man_ifname)
{
	return control_static_routes(SR_PREFIX_MAN, man_ifname, 0);
}

void select_usb_modem_to_wan(void)
{
	int modem_devnum = 0;

#if defined(USE_USB_SUPPORT)
	/* check modem enabled and ready */
	modem_devnum = get_modem_devnum();
	if (modem_devnum)
	{
		int modem_prio = nvram_get_int("modem_prio");
		if (modem_prio < 1)
		{
			modem_devnum = 0;
		}
		else if (modem_prio == 2)
		{
			if (!get_apcli_wisp_ifname())
			{
				int has_link = get_wan_ether_link_direct(0);
				if (has_link < 0)
					has_link = 0;

				if (has_link)
					modem_devnum = 0;
			}
			else
				modem_devnum = 0;
		}
	}
#endif
	set_usb_modem_dev_wan(0, modem_devnum);
}

int get_wan_ether_link_direct(int is_ap_mode)
{
	int ret = 0, wan_src_phy = SWAPI_PORT_WAN;
	unsigned int phy_link = 0;

	if (!is_ap_mode)
		wan_src_phy = nvram_get_int("wan_src_phy");

	if (wan_src_phy < 0)
		wan_src_phy = SWAPI_PORT_WAN;

	ret = phy_status_port_link(wan_src_phy, &phy_link);
	if (ret != 0)
		return -1;

	return (phy_link) ? 1 : 0;
}

int get_wan_dns_static(void)
{
	int unit = 0;

	if (get_usb_modem_wan(unit))
		return nvram_match("modem_dnsa", "0"); // dynamic or static dns for ppp2/weth0

	if (get_wan_proto(unit) == IPV4_WAN_PROTO_IPOE_STATIC)
		return 1;

	if (get_wan_unit_value_int(unit, "dnsenable_x") < 1)
		return 1;

	return 0;
}

int get_wan_wisp_active(int *p_has_link)
{
	int unit = 0;
	char *man_ifname;

	man_ifname = get_man_ifname(unit);
	if (!is_man_wisp(man_ifname))
		return 0;

	if (p_has_link)
		*p_has_link = get_apcli_connected(man_ifname);

	return 1;
}

void get_wan_ifname(char wan_ifname[BUFFER_SIZE_16])
{
	int unit = 0;
	char *ifname = get_man_ifname(unit);
	char *ifname_temp = get_wan_unit_value(unit, "ifname_t");

	if (get_usb_modem_wan(unit))
	{
		if (nvram_get_int("modem_type") == 3)
		{
			if (is_usbnet_interface(ifname_temp))
				ifname = ifname_temp;
		}
		else
		{
			if (ppp_ifindex(ifname_temp) >= RAS_PPP_UNIT)
				ifname = ifname_temp;
			else
				ifname = IFNAME_RAS;
		}
	}
	else
	{
		int wan_proto = get_wan_proto(unit);
		if (wan_proto == IPV4_WAN_PROTO_PPPOE ||
			wan_proto == IPV4_WAN_PROTO_PPTP ||
			wan_proto == IPV4_WAN_PROTO_L2TP)
		{
			if (ppp_ifindex(ifname_temp) >= WAN_PPP_UNIT)
				ifname = ifname_temp;
			else
				ifname = IFNAME_PPP;
		}
	}

	strncpy(wan_ifname, ifname, sizeof(wan_ifname) - 1);
	wan_ifname[sizeof(wan_ifname) - 1] = '\0';
}

in_addr_t
get_wan_ip4(int only_broadband_wan)
{
	int unit = 0;
	char *ifname, *ifname_temp;

	if (get_ap_mode())
		return INADDR_ANY;

	ifname = get_man_ifname(unit);
	ifname_temp = get_wan_unit_value(unit, "ifname_t");

	if (!only_broadband_wan && get_usb_modem_wan(unit))
	{
		ifname = ifname_temp;
	}
	else
	{
		int wan_proto = get_wan_proto(unit);
		if (wan_proto == IPV4_WAN_PROTO_PPPOE ||
			wan_proto == IPV4_WAN_PROTO_PPTP ||
			wan_proto == IPV4_WAN_PROTO_L2TP)
		{
			if (ppp_ifindex(ifname_temp) >= 0)
				ifname = ifname_temp;
			else
				ifname = IFNAME_PPP;
		}
	}

	return get_interface_addr4(ifname);
}

int has_wan_ip4(int only_broadband_wan)
{
	if (get_wan_ip4(only_broadband_wan) != INADDR_ANY)
		return 1;

	return 0;
}

int has_wan_gw4(void)
{
	int unit = 0;

	if (is_valid_ipv4(get_wan_unit_value(unit, "gateway")))
		return 1;

	return 0;
}

static int
udhcpc_deconfig(char *wan_ifname, int is_zcip)
{
	int unit, wan_proto, is_man;
	char log_prefix[BUFFER_SIZE_32], *udhcpc_state;

	unit = get_wan_unit(wan_ifname);
	wan_proto = get_wan_proto(unit);
	is_man = is_ifunit_man(wan_ifname, unit);

	snprintf(log_prefix, sizeof(log_prefix), "%s %s Client", (is_zcip) ? "ZeroConf" : "DHCP", (is_man) ? "MAN" : "WAN");
	udhcpc_state = "deconfig";

	if (is_man && (wan_proto == IPV4_WAN_PROTO_PPTP ||
				   wan_proto == IPV4_WAN_PROTO_L2TP))
	{
		/* fix hang-up issue */
		//		logmessage(log_prefix, "skipping resetting IP address to 0.0.0.0");
		;
	}
	else
	{
		bring_up_interface(wan_ifname, IPV4_ADDRESS_ANY, NULL);

		if (is_man)
		{
			char *man_addr = nvram_safe_get("wanx_ipaddr");

			/* flush conntrack table (only old WAN IP records) */
			if (is_valid_ipv4(man_addr))
				flush_conntrack_table(man_addr);

			nvram_set_temp("wanx_ipaddr", IPV4_ADDRESS_ANY);
		}
	}

	logmessage(log_prefix, "%s (%s): lease is lost",
			   udhcpc_state, wan_ifname);

	if (is_man)
		man_down(wan_ifname, unit);
	else
		wan_down(wan_ifname, unit, 0);

	return 0;
}

static int
udhcpc_bound(char *wan_ifname, int is_renew_mode)
{
	char *param, *value, *lan_ip, *lan_nm, *wan_ip, *wan_nm, *wan_gw;
	char tmp[BUFFER_SIZE_100], prefix[BUFFER_SIZE_16], route[BUFFER_SIZE_32], log_prefix[BUFFER_SIZE_32], *udhcpc_state;
	int unit, is_man, is_changed, i_err, has_gateway, lease_dur, dhcp_mtu;

	unit = get_wan_unit(wan_ifname);
	is_man = is_ifunit_man(wan_ifname, unit);

	if (is_man)
	{
		strncpy(prefix, "wanx_", sizeof(prefix) - 1);
		prefix[sizeof(prefix) - 1] = '\0';
	}
	else
	{
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		snprintf(tmp, sizeof(tmp), "%ld", uptime());
		set_wan_unit_value(unit, "dltime", tmp);
	}

	snprintf(log_prefix, sizeof(log_prefix), "%s %s Client", "DHCP", (is_man) ? "MAN" : "WAN");
	udhcpc_state = (is_renew_mode) ? "renew" : "bound";

	i_err = 0;
	lease_dur = 0;
	is_changed = 0;
	has_gateway = 0;
	dhcp_mtu = 0;

	if ((value = getenv("ip")))
	{
		param = strcat_r(prefix, "ipaddr", tmp);
		is_changed = nvram_invmatch(param, trim_r(value));
		nvram_set_temp(param, trim_r(value));
	}
	if ((value = getenv("subnet")))
		nvram_set_temp(strcat_r(prefix, "netmask", tmp), trim_r(value));
	if ((value = getenv("router")))
	{
		has_gateway = 1;
		nvram_set_temp(strcat_r(prefix, "gateway", tmp), trim_r(value));
	}
	if ((value = getenv("dns")))
		nvram_set_temp(strcat_r(prefix, "dns", tmp), trim_r(value));
	if ((value = getenv("wins")))
		nvram_set_temp(strcat_r(prefix, "wins", tmp), trim_r(value));
	if ((value = getenv("mtu")))
	{
		dhcp_mtu = atoi(trim_r(value));
		nvram_set_int_temp(strcat_r(prefix, "mtu", tmp), dhcp_mtu);
	}

	nvram_set_temp(strcat_r(prefix, "routes", tmp), safe_getenv("routes"));
	nvram_set_temp(strcat_r(prefix, "routes_ms", tmp), safe_getenv("msstaticroutes"));
	nvram_set_temp(strcat_r(prefix, "routes_rfc", tmp), safe_getenv("staticroutes"));

	if ((value = getenv("domain")))
		nvram_set_temp(strcat_r(prefix, "domain", tmp), trim_r(value));
	if ((value = getenv("lease")))
	{
		lease_dur = atoi(trim_r(value));
		nvram_set_int_temp(strcat_r(prefix, "lease", tmp), lease_dur);
	}

#if defined(USE_IPV6)
	if ((value = getenv("ip6rd")))
		store_ip6rd_from_dhcp(value, prefix);
#endif

	if (!has_gateway)
	{
		foreach (route, nvram_safe_get(strcat_r(prefix, "routes_rfc", tmp)), value)
		{
			if (has_gateway)
			{
				nvram_set_temp(strcat_r(prefix, "gateway", tmp), route);
				break;
			}
			else
				has_gateway = !strcmp(route, "0.0.0.0/0");
		}
	}

	lan_ip = nvram_safe_get("lan_ipaddr");
	lan_nm = nvram_safe_get("lan_netmask");
	wan_ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
	wan_nm = nvram_safe_get(strcat_r(prefix, "netmask", tmp));

	if (is_same_subnet2(wan_ip, lan_ip, wan_nm, lan_nm))
	{
		if (!is_renew_mode)
			bring_up_interface(wan_ifname, IPV4_ADDRESS_ANY, NULL);

		i_err = 1;

		logmessage(log_prefix,
				   "%s (%s). ERROR! Obtained IP [%s/%s] in LAN subnet [%s/%s]. Need change LAN subnet!",
				   udhcpc_state, wan_ifname, wan_ip, wan_nm, lan_ip, lan_nm);
	}
	else
	{
		if (is_changed)
			bring_up_interface(wan_ifname, IPV4_ADDRESS_ANY, NULL);

		if (!is_renew_mode && (dhcp_mtu >= 1300 && dhcp_mtu < 1500))
			set_interface_mtu(wan_ifname, dhcp_mtu);

		bring_up_interface(wan_ifname, wan_ip, wan_nm);

		wan_gw = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		logmessage(log_prefix,
				   "%s (%s), IP: %s, GW: %s, lease time: %d",
				   udhcpc_state, wan_ifname, wan_ip, wan_gw, lease_dur);

		if (is_man)
			man_up(wan_ifname, unit, 0);
		else
			wan_up(wan_ifname, unit, 0);
	}

	nvram_set_int_temp(strcat_r(prefix, "err", tmp), i_err);

	return 0;
}

static int
udhcpc_viptv_bound(char *man_ifname, int is_renew_mode)
{
	char *value, *lan_ip, *lan_nm, *ip, *nm, *gw, *rt, *rt_ms, *rt_rfc;
	char tmp[BUFFER_SIZE_100], prefix[BUFFER_SIZE_16], log_prefix[BUFFER_SIZE_32], *udhcpc_state;
	int lease_dur, is_changed, i_err, dhcp_mtu;

	strncpy(prefix, "manv_", sizeof(prefix) - 1);
	prefix[sizeof(prefix) - 1] = '\0';
	snprintf(log_prefix, sizeof(log_prefix), "%s %s Client", "DHCP", "IPTV");
	udhcpc_state = (is_renew_mode) ? "renew" : "bound";

	i_err = 0;
	lease_dur = 0;
	is_changed = 0;
	dhcp_mtu = 0;

	ip = IPV4_ADDRESS_ANY;
	nm = "255.255.0.0";
	gw = "";
	rt = "";
	rt_ms = "";
	rt_rfc = "";

	if ((value = getenv("ip")))
		ip = trim_r(value);

	is_changed = nvram_invmatch(strcat_r(prefix, "ipaddr", tmp), ip);
	nvram_set_temp(strcat_r(prefix, "ipaddr", tmp), ip);

	if ((value = getenv("subnet")))
		nm = trim_r(value);

	nvram_set_temp(strcat_r(prefix, "netmask", tmp), nm);

	if ((value = getenv("router")))
		gw = trim_r(value);

	nvram_set_temp(strcat_r(prefix, "gateway", tmp), gw);

	if ((value = getenv("mtu")))
	{
		dhcp_mtu = atoi(trim_r(value));
		nvram_set_int_temp(strcat_r(prefix, "mtu", tmp), dhcp_mtu);
	}

	if ((value = getenv("routes")))
		rt = trim_r(value);

	if ((value = getenv("msstaticroutes")))
		rt_ms = trim_r(value);

	if ((value = getenv("staticroutes")))
		rt_rfc = trim_r(value);

	nvram_set_temp(strcat_r(prefix, "routes", tmp), rt);
	nvram_set_temp(strcat_r(prefix, "routes_ms", tmp), rt_ms);
	nvram_set_temp(strcat_r(prefix, "routes_rfc", tmp), rt_rfc);

	if ((value = getenv("lease")))
		lease_dur = atoi(value);

	lan_ip = nvram_safe_get("lan_ipaddr");
	lan_nm = nvram_safe_get("lan_netmask");

	if (is_same_subnet2(ip, lan_ip, nm, lan_nm))
	{
		if (!is_renew_mode)
			bring_up_interface(man_ifname, IPV4_ADDRESS_ANY, NULL);

		i_err = 1;

		logmessage(log_prefix,
				   "%s (%s). ERROR! Obtained IP [%s/%s] in LAN subnet [%s/%s]. Need change LAN subnet!",
				   udhcpc_state, man_ifname, ip, nm, lan_ip, lan_nm);
	}
	else
	{
		if (is_changed)
			bring_up_interface(man_ifname, IPV4_ADDRESS_ANY, NULL);

		if (!is_renew_mode && (dhcp_mtu >= 1300 && dhcp_mtu < 1500))
			set_interface_mtu(man_ifname, dhcp_mtu);

		bring_up_interface(man_ifname, ip, nm);

		logmessage(log_prefix,
				   "%s (%s), IP: %s, GW: %s, lease time: %d",
				   udhcpc_state, man_ifname, ip, gw, lease_dur);

		/* and routes supplied via DHCP */
		if (*rt || *rt_rfc || *rt_ms)
			add_dhcp_routes(rt, rt_rfc, rt_ms, man_ifname, 0);

		/* default route via default gateway (metric 10) */
		if (is_valid_ipv4(gw))
			route_add(man_ifname, METRIC_DEFAULT, IPV4_ADDRESS_ANY, gw, IPV4_ADDRESS_ANY);

		start_igmpproxy(man_ifname);
	}

	nvram_set_int_temp(strcat_r(prefix, "err", tmp), i_err);

	return 0;
}

static int
zcip_bound(char *man_ifname)
{
	char *value, *ip;
	char tmp[BUFFER_SIZE_100], prefix[BUFFER_SIZE_16];
	int unit;
	char *zeroconf_mask = "255.255.0.0";

	unit = get_wan_unit(man_ifname);

	strncpy(prefix, "wanx_", sizeof(prefix) - 1);
	prefix[sizeof(prefix) - 1] = '\0';

	bring_up_interface(man_ifname, IPV4_ADDRESS_ANY, NULL);

	nvram_set_temp(strcat_r(prefix, "netmask", tmp), zeroconf_mask);
	nvram_set_temp(strcat_r(prefix, "gateway", tmp), "");
	nvram_set_temp(strcat_r(prefix, "dns", tmp), "");

	if ((value = getenv("ip")))
	{
		ip = trim_r(value);
		nvram_set_temp(strcat_r(prefix, "ipaddr", tmp), ip);
		bring_up_interface(man_ifname, ip, zeroconf_mask);

		man_up(man_ifname, unit, 0);
	}

	return 0;
}

static int
zcip_viptv_bound(char *man_ifname)
{
	char *value, *ip;
	char tmp[BUFFER_SIZE_100], prefix[BUFFER_SIZE_16];
	char *zeroconf_mask = "255.255.0.0";

	strncpy(prefix, "manv_", sizeof(prefix) - 1);
	prefix[sizeof(prefix) - 1] = '\0';

	bring_up_interface(man_ifname, IPV4_ADDRESS_ANY, NULL);

	nvram_set_temp(strcat_r(prefix, "netmask", tmp), zeroconf_mask);
	nvram_set_temp(strcat_r(prefix, "gateway", tmp), "");

	if ((value = getenv("ip")))
	{
		ip = trim_r(value);
		nvram_set_temp(strcat_r(prefix, "ipaddr", tmp), ip);
		bring_up_interface(man_ifname, ip, zeroconf_mask);

		start_igmpproxy(man_ifname);
	}

	return 0;
}

static int
udhcpc_renew(char *wan_ifname)
{
	char *param, *value;
	char tmp[BUFFER_SIZE_100], prefix[BUFFER_SIZE_16], log_prefix[BUFFER_SIZE_32], *udhcpc_state;
	int unit, is_man, is_changed;

	unit = get_wan_unit(wan_ifname);
	is_man = is_ifunit_man(wan_ifname, unit);

	if (is_man)
	{
		strncpy(prefix, "wanx_", sizeof(prefix) - 1);
		prefix[sizeof(prefix) - 1] = '\0';
	}
	else
	{
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		snprintf(tmp, sizeof(tmp), "%ld", uptime());
		set_wan_unit_value(unit, "dltime", tmp);
	}

	snprintf(log_prefix, sizeof(log_prefix), "%s %s Client", "DHCP", (is_man) ? "MAN" : "WAN");
	udhcpc_state = "renew";

	is_changed = 0;

	if (!(value = getenv("subnet")) || nvram_invmatch(strcat_r(prefix, "netmask", tmp), trim_r(value)))
		return udhcpc_bound(wan_ifname, 1);
	if ((value = getenv("router")) && nvram_invmatch(strcat_r(prefix, "gateway", tmp), trim_r(value)))
		return udhcpc_bound(wan_ifname, 1);
	if ((value = getenv("ip")) && nvram_invmatch(strcat_r(prefix, "ipaddr", tmp), trim_r(value)))
		return udhcpc_bound(wan_ifname, 1);

	if ((value = getenv("dns")))
	{
		param = strcat_r(prefix, "dns", tmp);
		if (nvram_invmatch(param, trim_r(value)))
		{
			nvram_set_temp(param, trim_r(value));
			is_changed = 1;
		}
	}

	if ((value = getenv("domain")))
		nvram_set_temp(strcat_r(prefix, "domain", tmp), trim_r(value));
	if ((value = getenv("wins")))
		nvram_set_temp(strcat_r(prefix, "wins", tmp), trim_r(value));
	if ((value = getenv("lease")))
		nvram_set_temp(strcat_r(prefix, "lease", tmp), trim_r(value));

	if (is_changed)
	{
		logmessage(log_prefix, "%s (%s), new DNS: %s",
				   udhcpc_state, wan_ifname,
				   nvram_safe_get(strcat_r(prefix, "dns", tmp)));

		update_resolvconf(0, 0);
	}

	return 0;
}

static int
udhcpc_viptv_renew(char *man_ifname)
{
	char *value;
	char tmp[BUFFER_SIZE_100], prefix[BUFFER_SIZE_16];

	strncpy(prefix, "manv_", sizeof(prefix) - 1);
	prefix[sizeof(prefix) - 1] = '\0';

	if ((value = getenv("subnet")) && nvram_invmatch(strcat_r(prefix, "netmask", tmp), trim_r(value)))
		return udhcpc_viptv_bound(man_ifname, 1);
	if ((value = getenv("router")) && nvram_invmatch(strcat_r(prefix, "gateway", tmp), trim_r(value)))
		return udhcpc_viptv_bound(man_ifname, 1);
	if ((value = getenv("ip")) && nvram_invmatch(strcat_r(prefix, "ipaddr", tmp), trim_r(value)))
		return udhcpc_viptv_bound(man_ifname, 1);

	return 0;
}

static int
udhcpc_leasefail(char *wan_ifname)
{
	/* DHCP failed for INET */
	return 0;
}

static int
udhcpc_viptv_leasefail(char *man_ifname)
{
	/* DHCP failed for IPTV */
	return 0;
}

static int
udhcpc_noack(char *wan_ifname)
{
	int unit, is_man;
	char log_prefix[BUFFER_SIZE_32];

	unit = get_wan_unit(wan_ifname);
	is_man = is_ifunit_man(wan_ifname, unit);

	snprintf(log_prefix, sizeof(log_prefix), "%s %s Client", "DHCP", (is_man) ? "MAN" : "WAN");
	logmessage(log_prefix, "Received NAK for %s", wan_ifname);
	return 0;
}

static int
udhcpc_viptv_noack(char *man_ifname)
{
	char log_prefix[BUFFER_SIZE_32];

	snprintf(log_prefix, sizeof(log_prefix), "%s %s Client", "DHCP", "IPTV");
	logmessage(log_prefix, "Received NAK for %s", man_ifname);
	return 0;
}

int udhcpc_main(int argc, char **argv)
{
	int ret = 0;
	char *wan_ifname = safe_getenv("interface");

	if (argc < 2 || !argv[1] || strlen(wan_ifname) < 1)
		return EINVAL;

	umask(0000);

	if (!strcmp(argv[1], "deconfig"))
		ret = udhcpc_deconfig(wan_ifname, 0);
	else if (!strcmp(argv[1], "bound"))
		ret = udhcpc_bound(wan_ifname, 0);
	else if (!strcmp(argv[1], "renew"))
		ret = udhcpc_renew(wan_ifname);
	else if (!strcmp(argv[1], "leasefail"))
		ret = udhcpc_leasefail(wan_ifname);
	else if (!strcmp(argv[1], "nak"))
		ret = udhcpc_noack(wan_ifname);

	return ret;
}

int udhcpc_viptv_main(int argc, char **argv)
{
	int ret = 0;
	char *man_ifname = safe_getenv("interface");

	if (argc < 2 || !argv[1] || strlen(man_ifname) < 1)
		return EINVAL;

	umask(0000);

	if (!strcmp(argv[1], "deconfig"))
		ret = 0;
	else if (!strcmp(argv[1], "bound"))
		ret = udhcpc_viptv_bound(man_ifname, 0);
	else if (!strcmp(argv[1], "renew"))
		ret = udhcpc_viptv_renew(man_ifname);
	else if (!strcmp(argv[1], "leasefail"))
		ret = udhcpc_viptv_leasefail(man_ifname);
	else if (!strcmp(argv[1], "nak"))
		ret = udhcpc_viptv_noack(man_ifname);

	return ret;
}

int zcip_main(int argc, char **argv)
{
	int ret = 0;
	char *man_ifname = safe_getenv("interface");

	if (argc < 2 || !argv[1] || strlen(man_ifname) < 1)
		return EINVAL;

	umask(0000);

	if (!strcmp(argv[1], "deconfig"))
		ret = udhcpc_deconfig(man_ifname, 1);
	else if (!strcmp(argv[1], "config"))
		ret = zcip_bound(man_ifname);

	return ret;
}

int zcip_viptv_main(int argc, char **argv)
{
	int ret = 0;
	char *man_ifname = safe_getenv("interface");

	if (argc < 2 || !argv[1] || strlen(man_ifname) < 1)
		return EINVAL;

	umask(0000);

	if (!strcmp(argv[1], "deconfig"))
		ret = 0;
	else if (!strcmp(argv[1], "config"))
		ret = zcip_viptv_bound(man_ifname);

	return ret;
}

int start_udhcpc_wan(char *wan_ifname, int unit, int wait_lease)
{
	int index, is_man;
	char log_prefix[BUFFER_SIZE_32], pidfile[BUFFER_SIZE_32] = {0};
	char *wan_hostname, *wan_vci;
	static char wan_mergedhostname[BUFFER_SIZE_80];
	char *dhcp_argv[] = {
		"/sbin/udhcpc",
		"-i", wan_ifname,
		"-s", SCRIPT_UDHCPC_WAN,
		"-p", pidfile,
		"-t4",
		"-T4",
		NULL,
		NULL, NULL, /* -x hostname:hostname */
		NULL, NULL, /* -V vendorclass	*/
		NULL,		/* -O mtu		*/
		NULL,		/* -O routes		*/
		NULL,		/* -O staticroutes	*/
		NULL,		/* -O msstaticroutes	*/
#if defined(USE_IPV6)
		NULL, /* -O 6rd		*/
		NULL, /* -O comcast6rd	*/
#endif
		NULL};

	index = 9; /* first NULL index	*/

	snprintf(pidfile, sizeof(pidfile), "/var/run/udhcpc%d.pid", unit);

	if (is_man_wisp(wan_ifname) || is_usbnet_interface(wan_ifname))
		dhcp_argv[7] = "-t6";

	if (wait_lease)
		dhcp_argv[index++] = "-b"; /* Background if lease is not obtained (timeout 4*4 sec) */
	else
		dhcp_argv[index++] = "-d"; /* Background after run (new patch for udhcpc) */

	wan_hostname = get_wan_unit_value(unit, "hostname");
	if (wan_hostname[0] != '\0')
	{
		const char *sane_hostname = sanity_hostname(wan_hostname);
		if (sane_hostname && sane_hostname[0] != '\0')
		{
			snprintf(wan_mergedhostname, sizeof(wan_mergedhostname), "hostname:%s", sane_hostname);
			dhcp_argv[index++] = "-x";
			dhcp_argv[index++] = wan_mergedhostname;
		}
	}

	wan_vci = get_wan_unit_value(unit, "vci");
	if (strlen(wan_vci) > 0)
	{
		dhcp_argv[index++] = "-V";
		dhcp_argv[index++] = wan_vci;
	}

	dhcp_argv[index++] = "-O26"; /* "mtu" */

	if (nvram_match("dr_enable_x", "1"))
	{
		dhcp_argv[index++] = "-O33";  /* "routes" */
		dhcp_argv[index++] = "-O121"; /* "staticroutes" */
		dhcp_argv[index++] = "-O249"; /* "msstaticroutes" */
	}

#if defined(USE_IPV6)
	if (get_ipv6_type() == IPV6_6RD)
	{
		dhcp_argv[index++] = "-O212"; /* "6rd" */
		dhcp_argv[index++] = "-O150"; /* "comcast6rd" */
	}
#endif

	is_man = is_ifunit_man(wan_ifname, unit);
	snprintf(log_prefix, sizeof(log_prefix), "%s %s Client", "DHCP", (is_man) ? "MAN" : "WAN");
	logmessage(log_prefix, "starting on %s ...", wan_ifname);

	return _eval(dhcp_argv, NULL, 0, NULL);
}

int start_udhcpc_viptv(char *man_ifname)
{
	int index;
	char log_prefix[BUFFER_SIZE_32], pidfile[BUFFER_SIZE_32] = {0};
	char *dhcp_argv[] = {
		"/sbin/udhcpc",
		"-i", man_ifname,
		"-s", SCRIPT_UDHCPC_VIPTV,
		"-p", pidfile,
		"-t4",
		"-T4",
		"-d", /* Background after run (new patch for udhcpc) */
		NULL, /* -O mtu		*/
		NULL, /* -O routes		*/
		NULL, /* -O staticroutes	*/
		NULL, /* -O msstaticroutes	*/
		NULL};

	index = 10; /* first NULL index	*/

	snprintf(pidfile, sizeof(pidfile), "/var/run/udhcpc_viptv.pid");

	dhcp_argv[index++] = "-O26"; /* "mtu" */

	if (nvram_match("dr_enable_x", "1"))
	{
		dhcp_argv[index++] = "-O33";  /* "routes" */
		dhcp_argv[index++] = "-O121"; /* "staticroutes" */
		dhcp_argv[index++] = "-O249"; /* "msstaticroutes" */
	}

	snprintf(log_prefix, sizeof(log_prefix), "%s %s Client", "DHCP", "IPTV");
	logmessage(log_prefix, "starting on %s ...", man_ifname);

	return _eval(dhcp_argv, NULL, 0, NULL);
}

static int start_zcip(const char *ifname, const char *script) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execlp("/sbin/zcip", "/sbin/zcip", ifname, script, NULL);
        _exit(EXIT_FAILURE); // If execlp fails
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } else {
        // Fork failed
        logmessage("net_wan", "Failed to fork for zcip: %s", strerror(errno));
        return -1;
    }
}

int start_zcip_wan(char *wan_ifname)
{
	return start_zcip(wan_ifname, SCRIPT_ZCIP_WAN);
}

int start_zcip_viptv(char *man_ifname)
{
	char log_prefix[BUFFER_SIZE_32];

	snprintf(log_prefix, sizeof(log_prefix), "%s %s Client", "ZeroConf", "IPTV");
	logmessage(log_prefix, "starting on %s ...", man_ifname);

	return start_zcip(man_ifname, SCRIPT_ZCIP_VIPTV);
}

int renew_udhcpc_wan(int unit)
{
	char pidfile[BUFFER_SIZE_32];

	snprintf(pidfile, sizeof(pidfile), "/var/run/udhcpc%d.pid", unit);
	return kill_pidfile_s(pidfile, SIGUSR1);
}

int release_udhcpc_wan(int unit)
{
	char pidfile[BUFFER_SIZE_32];

	snprintf(pidfile, sizeof(pidfile), "/var/run/udhcpc%d.pid", unit);
	return kill_pidfile_s(pidfile, SIGUSR2);
}

int stop_udhcpc_wan(int unit)
{
	char pidfile[BUFFER_SIZE_32];

	snprintf(pidfile, sizeof(pidfile), "/var/run/udhcpc%d.pid", unit);
	return kill_pidfile(pidfile);
}

int stop_udhcpc_viptv(void)
{
	char pidfile[BUFFER_SIZE_32];

	snprintf(pidfile, sizeof(pidfile), "/var/run/udhcpc_viptv.pid");
	return kill_pidfile(pidfile);
}

