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
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>

#include "rc.h"
#include "switch.h"

#if BOARD_RAM_SIZE < 32
#define SHRINK_TX_QUEUE_LEN (300)
#elif BOARD_RAM_SIZE < 64
#define SHRINK_TX_QUEUE_LEN (600)
#endif

#define SIGNAL_SIGUSR1 SIGUSR1
#define HTTPD_PROCESS_NAME "httpd"
#define POLLING_INTERVAL_MS 100
#define TIMEOUT_MS 1000

// Define constants for magic numbers and strings
#define ENABLED 1
#define DISABLED 0
#define IPADDR_LOOPBACK "127.0.0.1"
#define IPMASK_LOOPBACK "255.0.0.0"
#define IPADDR_ANY "0.0.0.0"
#define PATH_HOSTNAME "/etc/hostname"
#define PATH_HOSTS "/etc/hosts"
#define PATH_RESOLV_CONF "/etc/resolv.conf"
#define PIDFILE_UDHCPC_LAN "/var/run/udhcpc_lan.pid"
#define LOGSRC_DHCP_CLIENT "DHCP LAN Client"
#define NVKEY_LAN_GATEWAY_TEMP "lan_gateway_t"
#define NVKEY_MULTICAST_ROUTER_ENABLE "mr_enable_x"
#define RATE_MAX 1024

// Define constants for NVRAM keys
#define NVKEY_LAN_IPADDR "lan_ipaddr"
#define NVKEY_LAN_NETMASK "lan_netmask"
#define NVKEY_LAN_HWADDR "lan_hwaddr"
#define NVKEY_ETHER_LINK_WAN "ether_link_wan"
#define NVKEY_VLAN_FILTER "vlan_filter"
#define NVKEY_VLAN_VID_CPU "vlan_vid_cpu"
#define NVKEY_LAN_DNS1 "lan_dns1"
#define NVKEY_LAN_PROTO_X "lan_proto_x"
#define NVKEY_LOG_IPADDR "log_ipaddr"
#define NVKEY_LAN_STP "lan_stp"

// Define constants for VLAN IDs and parameters
#define VLANID_LAN_DEFAULT 1
#define VLANID_GUEST_DEFAULT 2
#define VLAN_PRIO_DEFAULT -1

// Define constants for radio modes
#define RADIO_MODE_AP 1
#define RADIO_MODE_AP_WDS 3

// Define constants for MII parameters
#define MII_SYNC_DISABLED 0
#define MII_BRIDGE_ENABLED 1
#define MII_MAX_FW_UPLOAD 10

// Define constants for IGMP/Bridge parameters
#define IGMP_STATIC_PORT_NONE -1
#define IGMP_MULTICAST_ROUTER_DISABLED 0
#define IGMP_MULTICAST_ROUTER_ENABLED 1
#define IGMP_MULTICAST_ROUTER_PATH 2

// Define constants for VLAN priority mask
#define VLAN_PRIO_MASK 0x07

// Define constants for DHCP retry/timeout
#define DHCPC_DEFAULT_RETRIES 4
#define DHCPC_DEFAULT_TIMEOUT 4

// Define constants for service count in stop_lan
#define STOP_LAN_AP_SVC_COUNT 3

// Define constants for bridge forward delay
#define BR_FD_STP_ACTIVE 15

// Define constants for interface names
#define IFNAME_WAN_VLAN "eth2.1"

// Define constants for MII mode
#define MII_MODE_AP "ap"

// Define constants for module parameters
#define HWNAT_MODULE_PARAMS "ttl_regen=0"

// Define constants for logging messages
#define LOGMSG_HW_NAT_ROUTING "Hardware NAT/Routing"
#define LOGMSG_HW_NAT_ENABLED "Enabled, L2 bridge offload"

// Define enums for modes
typedef enum {
    MCAST_ROUTER_DISABLED = 0,
    MCAST_ROUTER_ENABLED = 1,
    MCAST_ROUTER_PATH = 2
} McastRouterMode;

typedef enum {
    SWITCH_LINK_AUTO = 0,
    SWITCH_LINK_FORCE = 1
} SwitchLinkMode;

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

static int set_txqueuelen(const char *ifname, int txqueuelen) {
    struct ifreq ifr;
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_qlen = txqueuelen;

    if (ioctl(sockfd, SIOCSIFTXQLEN, &ifr) < 0) {
        perror("ioctl");
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}

pid_t get_pid_by_name(const char *name) {
    DIR *proc_dir;
    struct dirent *entry;
    char cmdline_path[256], cmdline[1024];
    FILE *fp;
    pid_t pid = -1;

    proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("Failed to open /proc");
        return -1;
    }

    while ((entry = readdir(proc_dir)) != NULL) {
        if (!isdigit(entry->d_name[0])) {
            continue;
        }

        snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%s/cmdline", entry->d_name);
        fp = fopen(cmdline_path, "r");
        if (!fp) {
            continue;
        }

        size_t bytes_read = fread(cmdline, 1, sizeof(cmdline) - 1, fp);
        fclose(fp);

        if (bytes_read > 0) {
            cmdline[bytes_read] = '\0'; // Ensure null termination
            char *first_arg = cmdline;
            char *base_name = strrchr(first_arg, '/');
            base_name = base_name ? base_name + 1 : first_arg;

            if (strcmp(base_name, name) == 0) {
                pid = (pid_t)atoi(entry->d_name);
                break;
            }
        }
    }

    closedir(proc_dir);
    return pid;
}

static int wait_for_interface(const char *ifname, int timeout_ms) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        if (is_interface_up(ifname)) {
            return 0; // Interface is ready
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed_ms >= timeout_ms) {
            return -1; // Timeout
        }

        usleep(POLLING_INTERVAL_MS * 1000); // Poll every POLLING_INTERVAL_MS
    }
}

in_addr_t get_lan_ip4(void)
{
	return get_interface_addr4(IFNAME_BR);
}

int has_lan_ip4(void)
{
	if (get_lan_ip4() != INADDR_ANY)
		return 1;

	return 0;
}

int has_lan_gw4(void)
{
	if (is_valid_ipv4(nvram_safe_get("lan_gateway_t")))
		return 1;

	return 0;
}

int add_static_lan_routes(char *lan_ifname)
{
	return control_static_routes(SR_PREFIX_LAN, lan_ifname, 1);
}

int del_static_lan_routes(char *lan_ifname)
{
	return control_static_routes(SR_PREFIX_LAN, lan_ifname, 0);
}

void init_loopback(void) {
    /* Bring up loopback interface */
    ifconfig("lo", ENABLED, IPADDR_LOOPBACK, IPMASK_LOOPBACK);

    /* Add to routing table */
    route_add("lo", 0, IPADDR_LOOPBACK, IPADDR_ANY, IPMASK_LOOPBACK);
}

void init_bridge(int is_ap_mode) {
    int rt_radio_on = get_enabled_radio_rt();
#if !defined(USE_RT3352_MII)
    int rt_mode_x = get_mode_radio_rt();
#endif
#if BOARD_HAS_5G_RADIO
    int wl_radio_on = get_enabled_radio_wl();
    int wl_mode_x = get_mode_radio_wl();
#endif
    char *lan_hwaddr = nvram_safe_get("lan_hwaddr");

    if (!lan_hwaddr || strlen(lan_hwaddr) == 0) {
        logmessage(LOGNAME, "Error: Failed to retrieve LAN hardware address");
        return;
    }

    if (!is_ap_mode) {
        switch_config_vlan(1);
    } else {
        phy_bridge_mode(SWAPI_WAN_BRIDGE_DISABLE_WAN, SWAPI_WAN_BWAN_ISOLATION_NONE);
    }

#if BOARD_RAM_SIZE < 64
    set_txqueuelen(IFNAME_MAC, SHRINK_TX_QUEUE_LEN);
#endif
    set_interface_hwaddr(IFNAME_MAC, lan_hwaddr);
    ifconfig(IFNAME_MAC, IFUP, NULL, NULL);

    switch_config_base();
    switch_config_storm();
    switch_config_link();

    phy_ports_power(1);

#if defined(USE_SINGLE_MAC)
    if (!is_ap_mode) {
        create_vlan_iface(IFNAME_MAC, VLANID_LAN_DEFAULT, VLAN_PRIO_DEFAULT, VLAN_PRIO_DEFAULT, lan_hwaddr, ENABLED);
        create_vlan_iface(IFNAME_MAC, VLANID_GUEST_DEFAULT, VLAN_PRIO_DEFAULT, VLAN_PRIO_DEFAULT, NULL, DISABLED);
    }
#if defined(AP_MODE_LAN_TAGGED)
    else {
        create_vlan_iface(IFNAME_MAC, VLANID_LAN_DEFAULT, VLAN_PRIO_DEFAULT, VLAN_PRIO_DEFAULT, lan_hwaddr, ENABLED);
    }
#endif
#endif

#if defined(USE_RT3352_MII)
    create_vlan_iface(IFNAME_MAC, INIC_GUEST_VLAN_VID, VLAN_PRIO_DEFAULT, VLAN_PRIO_DEFAULT, lan_hwaddr, ENABLED);
#endif

#if BOARD_2G_IN_SOC
#if !defined(USE_RT3352_MII)
    if (!rt_radio_on || (rt_mode_x == RADIO_MODE_AP || rt_mode_x == RADIO_MODE_AP_WDS)) {
        gen_ralink_config_2g(ENABLED);
        ifconfig(IFNAME_2G_MAIN, IFUP, NULL, NULL);
    }
#endif
#if BOARD_HAS_5G_RADIO
    if (!wl_radio_on || (wl_mode_x == RADIO_MODE_AP || wl_mode_x == RADIO_MODE_AP_WDS)) {
        gen_ralink_config_5g(ENABLED);
        ifconfig(IFNAME_5G_MAIN, IFUP, NULL, NULL);
    }
#endif
#else
#if BOARD_HAS_5G_RADIO
    if (!wl_radio_on || (wl_mode_x == RADIO_MODE_AP || wl_mode_x == RADIO_MODE_AP_WDS)) {
        gen_ralink_config_5g(ENABLED);
        ifconfig(IFNAME_5G_MAIN, IFUP, NULL, NULL);
    }
#endif
#if !defined(USE_RT3352_MII)
    if (!rt_radio_on || (rt_mode_x == RADIO_MODE_AP || rt_mode_x == RADIO_MODE_AP_WDS)) {
        gen_ralink_config_2g(ENABLED);
        ifconfig(IFNAME_2G_MAIN, IFUP, NULL, NULL);
    }
#endif
#endif

    br_add_del_bridge(IFNAME_BR, ENABLED);
    br_set_stp(IFNAME_BR, DISABLED);
    br_set_fd(IFNAME_BR, 2);
    set_interface_hwaddr(IFNAME_BR, lan_hwaddr);

    if (!is_ap_mode) {
#if defined(USE_GMAC2_TO_GPHY) || defined(USE_GMAC2_TO_GSW)
        if (get_wan_bridge_mode() != SWAPI_WAN_BRIDGE_DISABLE) {
            create_vlan_iface(IFNAME_MAC, VLANID_LAN_DEFAULT, VLAN_PRIO_DEFAULT, VLAN_PRIO_DEFAULT, lan_hwaddr, ENABLED);
            br_add_del_if(IFNAME_BR, IFNAME_WAN_VLAN, ENABLED);
        } else
#endif
        br_add_del_if(IFNAME_BR, IFNAME_LAN, ENABLED);
    } else {
#if defined(AP_MODE_LAN_TAGGED)
        br_add_del_if(IFNAME_BR, IFNAME_LAN, ENABLED);
#else
        br_add_del_if(IFNAME_BR, IFNAME_MAC, ENABLED);
#endif
#if defined(USE_GMAC2_TO_GPHY) || defined(USE_GMAC2_TO_GSW)
        ifconfig(IFNAME_MAC2, IFUP, NULL, NULL);
        br_add_del_if(IFNAME_BR, IFNAME_MAC2, ENABLED);
#if defined(USE_HW_NAT)
        module_smart_load("hw_nat", HWNAT_MODULE_PARAMS);
        logmessage(LOGNAME, "%s: %s", LOGMSG_HW_NAT_ROUTING, LOGMSG_HW_NAT_ENABLED);
#endif
#endif
    }

#if defined(USE_RT3352_MII)
    {
        char inic_param[80];
        snprintf(inic_param, sizeof(inic_param), "miimaster=%s mode=%s syncmiimac=%d bridge=%d max_fw_upload=%d",
                 IFNAME_MAC, MII_MODE_AP, MII_SYNC_DISABLED, MII_BRIDGE_ENABLED, MII_MAX_FW_UPLOAD);
        module_smart_load("iNIC_mii", inic_param);
    }
#endif

#if BOARD_2G_IN_SOC
    start_wifi_ap_rt(rt_radio_on);
    start_wifi_wds_rt(rt_radio_on);
    start_wifi_apcli_rt(rt_radio_on);
#if BOARD_HAS_5G_RADIO
    start_wifi_ap_wl(wl_radio_on);
    start_wifi_wds_wl(wl_radio_on);
    start_wifi_apcli_wl(wl_radio_on);
#endif
#else
#if BOARD_HAS_5G_RADIO
    start_wifi_ap_wl(wl_radio_on);
    start_wifi_wds_wl(wl_radio_on);
    start_wifi_apcli_wl(wl_radio_on);
#endif
    start_wifi_ap_rt(rt_radio_on);
    start_wifi_wds_rt(rt_radio_on);
    start_wifi_apcli_rt(rt_radio_on);
#endif

#if defined(BOARD_GPIO_LED_SW2G)
    if (rt_radio_on)
        LED_CONTROL(BOARD_GPIO_LED_SW2G, LED_ON);
#endif
#if defined(BOARD_GPIO_LED_SW5G) && BOARD_HAS_5G_RADIO
    if (wl_radio_on)
        LED_CONTROL(BOARD_GPIO_LED_SW5G, LED_ON);
#endif

    if (wait_for_interface(IFNAME_BR, TIMEOUT_MS) < 0) {
        logmessage(LOGNAME, "Timeout waiting for %s to be ready", IFNAME_BR);
    }

#if BOARD_RAM_SIZE < 64
    set_txqueuelen(IFNAME_2G_MAIN, SHRINK_TX_QUEUE_LEN);
    set_txqueuelen(IFNAME_2G_GUEST, SHRINK_TX_QUEUE_LEN);
    set_txqueuelen(IFNAME_2G_APCLI, SHRINK_TX_QUEUE_LEN);
    set_txqueuelen(IFNAME_2G_WDS0, SHRINK_TX_QUEUE_LEN);
    set_txqueuelen(IFNAME_2G_WDS1, SHRINK_TX_QUEUE_LEN);
    set_txqueuelen(IFNAME_2G_WDS2, SHRINK_TX_QUEUE_LEN);
    set_txqueuelen(IFNAME_2G_WDS3, SHRINK_TX_QUEUE_LEN);
#endif

    ifconfig(IFNAME_BR, IFUP, NULL, NULL);

#if BOARD_HAS_5G_RADIO
    if (!wl_radio_on || (wl_mode_x == RADIO_MODE_AP || wl_mode_x == RADIO_MODE_AP_WDS)) {
        if (wait_for_interface(IFNAME_5G_MAIN, 500) < 0) {
            logmessage(LOGNAME, "Timeout waiting for %s to be ready", IFNAME_5G_MAIN);
        }
        ifconfig(IFNAME_5G_MAIN, DISABLED, NULL, NULL);
        gen_ralink_config_5g(DISABLED);
    }

    if (wl_radio_on)
        update_vga_clamp_wl(ENABLED);
#endif

#if !defined(USE_RT3352_MII)
    if (!rt_radio_on || (rt_mode_x == RADIO_MODE_AP || rt_mode_x == RADIO_MODE_AP_WDS)) {
        if (wait_for_interface(IFNAME_2G_MAIN, 500) < 0) {
            logmessage(LOGNAME, "Timeout waiting for %s to be ready", IFNAME_2G_MAIN);
        }
        ifconfig(IFNAME_2G_MAIN, DISABLED, NULL, NULL);
        gen_ralink_config_2g(DISABLED);
    }

    if (rt_radio_on)
        update_vga_clamp_rt(ENABLED);
#endif

    restart_guest_lan_isolation();

    nvram_set_int_temp("reload_svc_wl", DISABLED);
    nvram_set_int_temp("reload_svc_rt", DISABLED);
}

void config_bridge(int is_ap_mode)
{
	const char *wired_ifname;
	int multicast_router, multicast_querier, igmp_static_port;
	int igmp_snoop = nvram_get_int("ether_igmp");
	int wired_m2u = nvram_get_int("ether_m2u");

	if (!is_ap_mode)
	{
		igmp_static_port = IGMP_STATIC_PORT_NONE;
		if (nvram_match(NVKEY_MULTICAST_ROUTER_ENABLE, "1"))
		{
			multicast_router = IGMP_MULTICAST_ROUTER_PATH;  // bridge is mcast router path (br0 <--igmpproxy--> eth3)
			multicast_querier = IGMP_MULTICAST_ROUTER_DISABLED; // bridge is not needed internal mcast querier (igmpproxy is mcast querier)
		}
		else
		{
			multicast_router = IGMP_MULTICAST_ROUTER_ENABLED;  // bridge may be mcast router path
			multicast_querier = IGMP_MULTICAST_ROUTER_ENABLED; // bridge is needed internal mcast querier (for eth2-ra0-rai0 snooping work)
		}
		wired_ifname = IFNAME_LAN;
#if defined(USE_GMAC2_TO_GPHY) || defined(USE_GMAC2_TO_GSW)
		if (get_wan_bridge_mode() != SWAPI_WAN_BRIDGE_DISABLE)
			wired_ifname = IFNAME_WAN_VLAN;
#endif
	}
	else
	{
		igmp_static_port = nvram_get_int("ether_uport");
		multicast_router = IGMP_MULTICAST_ROUTER_DISABLED;  // bridge is not mcast router path
		multicast_querier = IGMP_MULTICAST_ROUTER_ENABLED; // bridge is needed internal mcast querier (for eth2-ra0-rai0 snooping work)
#if defined(AP_MODE_LAN_TAGGED)
		wired_ifname = IFNAME_LAN;
#else
		wired_ifname = IFNAME_MAC;
#endif
	}

	br_set_param_int(IFNAME_BR, "multicast_router", multicast_router);
	br_set_param_int(IFNAME_BR, "multicast_querier", multicast_querier);

	/* allow use bridge IP address as IGMP/MLD query source IP (avoid cisco issue) */
	br_set_param_int(IFNAME_BR, "multicast_query_use_ifaddr", ENABLED);

	br_set_param_int(IFNAME_BR, "multicast_snooping", (igmp_snoop) ? ENABLED : DISABLED);

	brport_set_m2u(wired_ifname, (igmp_snoop && wired_m2u == 1) ? ENABLED : DISABLED);

	phy_igmp_static_port(igmp_static_port);
	phy_igmp_snooping((igmp_snoop && wired_m2u == 2) ? ENABLED : DISABLED);
}

void switch_config_link(void)
{
	int i, i_flow_mode, i_link_mode;
	char nvram_param[20];

	// WAN
	i_link_mode = nvram_safe_get_int(NVKEY_ETHER_LINK_WAN, SWAPI_LINK_SPEED_MODE_AUTO,
									 SWAPI_LINK_SPEED_MODE_AUTO, SWAPI_LINK_SPEED_MODE_FORCE_POWER_OFF);
	i_flow_mode = nvram_safe_get_int("ether_flow_wan", SWAPI_LINK_FLOW_CONTROL_TX_RX,
									 SWAPI_LINK_FLOW_CONTROL_TX_RX, SWAPI_LINK_FLOW_CONTROL_DISABLE);
	phy_set_link_port(SWAPI_PORT_WAN, i_link_mode, i_flow_mode);

	for (i = 0; i < BOARD_NUM_ETH_EPHY - 1; i++)
	{
		snprintf(nvram_param, sizeof(nvram_param), "ether_link_lan%d", i + 1);
		i_link_mode = nvram_safe_get_int(nvram_param, SWAPI_LINK_SPEED_MODE_AUTO,
										 SWAPI_LINK_SPEED_MODE_AUTO, SWAPI_LINK_SPEED_MODE_FORCE_POWER_OFF);
		snprintf(nvram_param, sizeof(nvram_param), "ether_flow_lan%d", i + 1);
		i_flow_mode = nvram_safe_get_int(nvram_param, SWAPI_LINK_FLOW_CONTROL_TX_RX,
										 SWAPI_LINK_FLOW_CONTROL_TX_RX, SWAPI_LINK_FLOW_CONTROL_DISABLE);
		phy_set_link_port(SWAPI_PORT_LAN1 + i, i_link_mode, i_flow_mode);
	}
}

void switch_config_base(void)
{
	update_ether_leds();

	phy_jumbo_frames(nvram_get_int("ether_jumbo"));
	phy_green_ethernet(nvram_get_int("ether_green"));
	phy_eee_lpi(nvram_get_int("ether_eee"));
}

void switch_config_storm(void)
{
	int controlrate_unknown_unicast;
	int controlrate_unknown_multicast;
	int controlrate_multicast;
	int controlrate_broadcast;

	/* unknown unicast storm control */
	controlrate_unknown_unicast = nvram_get_int("controlrate_unknown_unicast");
	if (controlrate_unknown_unicast <= 0 || controlrate_unknown_unicast > RATE_MAX)
		controlrate_unknown_unicast = RATE_MAX;

	/* unknown multicast storm control */
	controlrate_unknown_multicast = nvram_get_int("controlrate_unknown_multicast");
	if (controlrate_unknown_multicast <= 0 || controlrate_unknown_multicast > RATE_MAX)
		controlrate_unknown_multicast = RATE_MAX;

	/* multicast storm control */
	controlrate_multicast = nvram_get_int("controlrate_multicast");
	if (controlrate_multicast <= 0 || controlrate_multicast > RATE_MAX)
		controlrate_multicast = RATE_MAX;

	/* broadcast storm control */
	controlrate_broadcast = nvram_get_int("controlrate_broadcast");
	if (controlrate_broadcast <= 0 || controlrate_broadcast > RATE_MAX)
		controlrate_broadcast = RATE_MAX;

	phy_storm_unicast_unknown(controlrate_unknown_unicast);
	phy_storm_multicast_unknown(controlrate_unknown_multicast);
	phy_storm_multicast(controlrate_multicast);
	phy_storm_broadcast(controlrate_broadcast);
}

void switch_config_vlan(int first_call)
{
	int bridge_mode, bwan_isolation, is_vlan_filter;
	int vlan_vid[SWAPI_VLAN_RULE_NUM] = {0};
	int vlan_pri[SWAPI_VLAN_RULE_NUM] = {0};
	int vlan_tag[SWAPI_VLAN_RULE_NUM] = {0};
	unsigned int vrule;

	bridge_mode = get_wan_bridge_mode();
	bwan_isolation = get_wan_bridge_iso_mode(bridge_mode);

	is_vlan_filter = (nvram_match(NVKEY_VLAN_FILTER, "1")) ? ENABLED : DISABLED;
	if (is_vlan_filter)
	{
#if defined(USE_MTK_ESW)
		/* MT7620 and MT7628 ESW not support port matrix + security */
		if (bwan_isolation == SWAPI_WAN_BWAN_ISOLATION_BETWEEN)
			bwan_isolation = SWAPI_WAN_BWAN_ISOLATION_NONE;
#endif
		vlan_vid[SWAPI_VLAN_RULE_WAN_INET] = nvram_get_int(NVKEY_VLAN_VID_CPU);
		vlan_vid[SWAPI_VLAN_RULE_WAN_IPTV] = nvram_get_int("vlan_vid_iptv");
		vlan_vid[SWAPI_VLAN_RULE_WAN_LAN1] = nvram_get_int("vlan_vid_lan1");
		vlan_vid[SWAPI_VLAN_RULE_WAN_LAN2] = nvram_get_int("vlan_vid_lan2");
		vlan_vid[SWAPI_VLAN_RULE_WAN_LAN3] = nvram_get_int("vlan_vid_lan3");
		vlan_vid[SWAPI_VLAN_RULE_WAN_LAN4] = nvram_get_int("vlan_vid_lan4");

		vlan_pri[SWAPI_VLAN_RULE_WAN_INET] = nvram_get_int("vlan_pri_cpu") & VLAN_PRIO_MASK;
		vlan_pri[SWAPI_VLAN_RULE_WAN_IPTV] = nvram_get_int("vlan_pri_iptv") & VLAN_PRIO_MASK;
		vlan_pri[SWAPI_VLAN_RULE_WAN_LAN1] = nvram_get_int("vlan_pri_lan1") & VLAN_PRIO_MASK;
		vlan_pri[SWAPI_VLAN_RULE_WAN_LAN2] = nvram_get_int("vlan_pri_lan2") & VLAN_PRIO_MASK;
		vlan_pri[SWAPI_VLAN_RULE_WAN_LAN3] = nvram_get_int("vlan_pri_lan3") & VLAN_PRIO_MASK;
		vlan_pri[SWAPI_VLAN_RULE_WAN_LAN4] = nvram_get_int("vlan_pri_lan4") & VLAN_PRIO_MASK;

		vlan_tag[SWAPI_VLAN_RULE_WAN_INET] = DISABLED;
		vlan_tag[SWAPI_VLAN_RULE_WAN_IPTV] = DISABLED;
		vlan_tag[SWAPI_VLAN_RULE_WAN_LAN1] = nvram_get_int("vlan_tag_lan1");
		vlan_tag[SWAPI_VLAN_RULE_WAN_LAN2] = nvram_get_int("vlan_tag_lan2");
		vlan_tag[SWAPI_VLAN_RULE_WAN_LAN3] = nvram_get_int("vlan_tag_lan3");
		vlan_tag[SWAPI_VLAN_RULE_WAN_LAN4] = nvram_get_int("vlan_tag_lan4");

		if (is_vlan_vid_valid(vlan_vid[SWAPI_VLAN_RULE_WAN_INET]))
			vlan_tag[SWAPI_VLAN_RULE_WAN_INET] = ENABLED;
		else
			vlan_vid[SWAPI_VLAN_RULE_WAN_INET] = DISABLED;

		if (is_vlan_vid_valid(vlan_vid[SWAPI_VLAN_RULE_WAN_IPTV]))
			vlan_tag[SWAPI_VLAN_RULE_WAN_IPTV] = ENABLED;
		else
			vlan_vid[SWAPI_VLAN_RULE_WAN_IPTV] = DISABLED;
	}

	/* set vlan rule before change bridge mode! */
	for (vrule = 0; vrule < SWAPI_VLAN_RULE_NUM; vrule++)
		phy_vlan_rule_set(vrule, vlan_vid[vrule], vlan_pri[vrule], vlan_tag[vrule]);

	phy_bridge_mode(bridge_mode, bwan_isolation);

#if defined(USE_RT3352_MII)
	if (!first_call)
	{
		// clear isolation iNIC port from all LAN ports
		if (is_interface_up(IFNAME_INIC_MAIN) && get_mlme_radio_rt())
			phy_isolate_inic(DISABLED);
	}
#endif
}

void restart_switch_config_vlan(void)
{
#if !defined(USE_GMAC2_TO_GPHY) && !defined(USE_GMAC2_TO_GSW)
	int pvid_wan = phy_vlan_pvid_wan_get();
#endif

	if (get_ap_mode())
		return;

	notify_reset_detect_link();
	switch_config_vlan(DISABLED);

#if !defined(USE_GMAC2_TO_GPHY) && !defined(USE_GMAC2_TO_GSW)
	if (phy_vlan_pvid_wan_get() != pvid_wan)
#endif
		full_restart_wan();
}

int is_vlan_vid_valid(int vlan_vid)
{
	if (vlan_vid == VLANID_GUEST_DEFAULT)
		return ENABLED;
	return (vlan_vid >= MIN_EXT_VLAN_VID && vlan_vid < 4095) ? ENABLED : DISABLED;
}

void update_ether_leds(void)
{
#if (BOARD_NUM_ETH_LEDS > 1)
	int led0 = nvram_get_int("ether_led0");
	int led1 = nvram_get_int("ether_led1");

	if (!nvram_get_int("led_ether_t"))
	{
		led0 = SWAPI_LED_OFF;
		led1 = SWAPI_LED_OFF;
	}
#if BOARD_ETH_LED_SWAP
	phy_led_mode_green(led1);
	phy_led_mode_yellow(led0);
#else
	phy_led_mode_green(led0);
	phy_led_mode_yellow(led1);
#endif
#elif (BOARD_NUM_ETH_LEDS == 1)
	int led0 = nvram_get_int("ether_led0");

	if (!nvram_get_int("led_ether_t"))
		led0 = SWAPI_LED_OFF;
#if BOARD_ETH_LED_SWAP
	phy_led_mode_yellow(led0);
#else
	phy_led_mode_green(led0);
#endif
#endif
}

void reset_lan_temp(void)
{
	if (nvram_match(NVKEY_LAN_IPADDR, ""))
	{
		nvram_set(NVKEY_LAN_IPADDR, DEF_LAN_ADDR);
		nvram_set(NVKEY_LAN_NETMASK, DEF_LAN_MASK);
	}
	else if (nvram_match(NVKEY_LAN_NETMASK, ""))
	{
		nvram_set(NVKEY_LAN_NETMASK, DEF_LAN_MASK);
	}

	nvram_set_temp("lan_ipaddr_t", nvram_safe_get(NVKEY_LAN_IPADDR));
	nvram_set_temp("lan_netmask_t", nvram_safe_get(NVKEY_LAN_NETMASK));
	nvram_set_temp("lan_gateway_t", "");
	nvram_set_temp("lan_domain_t", "");
	nvram_set_temp("lan_dns_t", "");
}

void reset_lan_vars(void)
{
	nvram_set(NVKEY_LAN_HWADDR, nvram_safe_get("il0macaddr"));
}

static void
create_hosts_lan(const char *lan_ipaddr, const char *lan_dname)
{
	FILE *fp;
	char *lan_hname = get_our_hostname();

	sethostname(lan_hname, strlen(lan_hname));
	setdomainname(lan_dname, strlen(lan_dname));

	fp = fopen(PATH_HOSTNAME, "w+");
	if (fp)
	{
		fprintf(fp, "%s\n", lan_hname);
		fclose(fp);
	}

	fp = fopen(PATH_HOSTS, "w+");
	if (fp)
	{
		fprintf(fp, "%s %s %s\n", IPADDR_LOOPBACK, "localhost.localdomain", "localhost");
		if (strlen(lan_dname) > 0)
			fprintf(fp, "%s %s.%s %s\n", lan_ipaddr, lan_hname, lan_dname, lan_hname);
		else
			fprintf(fp, "%s %s\n", lan_ipaddr, lan_hname);
		fclose(fp);
	}
}

void update_hosts_ap(void)
{
	create_hosts_lan(nvram_safe_get("lan_ipaddr_t"), nvram_safe_get("lan_domain_t"));
}

void start_lan(int is_ap_mode, int do_wait)
{
	char *lan_ipaddr;
	char *lan_netmsk;
	char *lan_ifname = IFNAME_BR;

	lan_ipaddr = nvram_safe_get(NVKEY_LAN_IPADDR);
	lan_netmsk = nvram_safe_get(NVKEY_LAN_NETMASK);

	/* bring up and configure LAN interface */
	ifconfig(lan_ifname, IFUP, lan_ipaddr, lan_netmsk);

	/*
	 * Configure DHCP connection. The DHCP client will run
	 * 'udhcpc bound'/'udhcpc deconfig' upon finishing IP address
	 * renew and release.
	 */
	if (is_ap_mode)
	{
		char *lan_dname = nvram_safe_get("lan_domain");

		create_hosts_lan(lan_ipaddr, lan_dname);

		if (nvram_match(NVKEY_LAN_PROTO_X, "1"))
		{

			symlink("/sbin/rc", SCRIPT_UDHCPC_LAN);

			/* early fill XXX_t fields */
			update_lan_status(DISABLED);

			/* wait PHY ports link ready */
			if (do_wait)
				sleep(ENABLED);

			/* di wakeup after 60 secs */
			notify_run_detect_internet(60);

			/* start dhcp daemon */
			start_udhcpc_lan(lan_ifname);
		}
		else
		{

			/* manual config lan gateway and dns */
			lan_up_manual(lan_ifname, lan_dname);

			/* di wakeup after 2 secs */
			notify_run_detect_internet(2);
		}
	}
	else
	{

		/* install lan specific static routes */
		add_static_lan_routes(lan_ifname);

		/* fill XXX_t fields */
		update_lan_status(DISABLED);
	}

#if defined(USE_IPV6)
	if (get_ipv6_type() != IPV6_DISABLED)
		reload_lan_addr6();
#endif

	config_bridge(is_ap_mode);
}

void stop_lan(int is_ap_mode)
{
	char *svcs[] = {"udhcpc", "detect_wan", NULL};

	if (is_ap_mode)
	{
		notify_pause_detect_internet();

		kill_services(svcs, STOP_LAN_AP_SVC_COUNT, ENABLED);
	}
	else
	{
		char *lan_ip = nvram_safe_get(NVKEY_LAN_GATEWAY_TEMP);

		/* flush conntrack table (only old LAN IP records) */
		if (is_valid_ipv4(lan_ip))
			flush_conntrack_table(lan_ip);

		/* Remove static routes */
		clear_if_route4(IFNAME_BR);
	}

#if defined(USE_IPV6)
	clear_lan_addr6();
#endif

	/* Bring down LAN interface */
	ifconfig(IFNAME_BR, DISABLED, NULL, NULL);
}

void full_restart_lan(void) {
    int is_wan_err = DISABLED, is_lan_stp = DISABLED;
    int is_ap_mode = get_ap_mode();
    int log_remote = nvram_invmatch(NVKEY_LOG_IPADDR, "");

    if (!is_ap_mode) {
        is_wan_err = get_wan_unit_value_int(DISABLED, "err");
        is_lan_stp = nvram_get_int(NVKEY_LAN_STP);
    }

    /* stop logger if remote */
    if (log_remote)
        stop_logger();

    stop_lltd();
    stop_infosvr();
    stop_networkmap();
    stop_upnp();
    stop_vpn_server();
    stop_dns_dhcpd();
    stop_lan(is_ap_mode);

    reset_lan_vars();

    if (!is_ap_mode) {
        br_set_stp(IFNAME_BR, DISABLED);
        br_set_fd(IFNAME_BR, 2);
    }

    /* down and up all LAN ports link */
    phy_ports_lan_power(DISABLED);
    if (wait_for_interface(IFNAME_BR, TIMEOUT_MS) < 0) {
        logmessage(LOGNAME, "Timeout waiting for LAN ports to power down");
    }
    phy_ports_lan_power(ENABLED);

    start_lan(is_ap_mode, ENABLED);

    /* start logger if remote */
    if (log_remote)
        start_logger(DISABLED);

#if defined(APP_SMBD)
    /* update SMB fastpath owner address */
    config_smb_fastpath(ENABLED);
#endif

    /* restart dns relay and dhcp server */
    start_dns_dhcpd(is_ap_mode);

    if (!is_ap_mode) {
        if (is_lan_stp) {
            br_set_stp(IFNAME_BR, ENABLED);
            br_set_fd(IFNAME_BR, BR_FD_STP_ACTIVE);
        }

        if (is_wan_err) {
            full_restart_wan();
            start_vpn_server();
        } else {
            /* restart vpn server, firewall and miniupnpd */
            restart_vpn_server();
        }
    }

    /* restart igmpproxy, udpxy, xupnpd */
    if (!is_wan_err)
        restart_iptv(is_ap_mode);

#if defined(APP_NFSD)
    /* reload NFS server exports */
    reload_nfsd();
#endif

#if defined(APP_SMBD) || defined(APP_NMBD)
    reload_nmbd();
#endif

    start_infosvr();
    start_lltd();

    /* start ARP network scanner */
    start_networkmap(ENABLED);

    /* force httpd logout */
    pid_t pid = get_pid_by_name(HTTPD_PROCESS_NAME);
    if (pid > 0) {
        if (kill(pid, SIGNAL_SIGUSR1) < 0) {
            perror("Failed to send SIGUSR1 to httpd");
        }
    } else {
        fprintf(stderr, "httpd process not found\n");
    }
}

void lan_up_manual(char *lan_ifname, char *lan_dname)
{
	FILE *fp;
	int lock;
	int dns_count = DISABLED;
	char *dns_ip, *gateway_ip;

	gateway_ip = nvram_safe_get("lan_gateway");

	/* Set default route to gateway if specified */
	if (is_valid_ipv4(gateway_ip))
		route_add(lan_ifname, DISABLED, IPADDR_ANY, gateway_ip, IPADDR_ANY);

	lock = file_lock("resolv");

	/* Open resolv.conf */
	fp = fopen(PATH_RESOLV_CONF, "w+");
	if (fp)
	{
		if (strlen(lan_dname) > 0)
			fprintf(fp, "domain %s\n", lan_dname);

		dns_ip = nvram_safe_get(NVKEY_LAN_DNS1);
		if (is_valid_ipv4(dns_ip))
		{
			fprintf(fp, "nameserver %s\n", dns_ip);
			dns_count++;
		}

		dns_ip = nvram_safe_get("lan_dns2");
		if (is_valid_ipv4(dns_ip))
		{
			fprintf(fp, "nameserver %s\n", dns_ip);
			dns_count++;
		}

		if (!dns_count && is_valid_ipv4(gateway_ip))
			fprintf(fp, "nameserver %s\n", gateway_ip);

		fclose(fp);
	}

	file_unlock(lock);

	/* sync time */
	notify_watchdog_time();

	/* fill XXX_t fields */
	update_lan_status(DISABLED);
}

static void
lan_up_auto(char *lan_ifname, char *lan_gateway, char *lan_dname)
{
	FILE *fp;
	int dns_count = DISABLED;
	char word[100], *next, *dns_ip;

	/* Set default route to gateway if specified */
	if (is_valid_ipv4(lan_gateway))
		route_add(lan_ifname, DISABLED, IPADDR_ANY, lan_gateway, IPADDR_ANY);

	/* Open resolv.conf */
	fp = fopen(PATH_RESOLV_CONF, "w+");
	if (fp)
	{
		if (strlen(lan_dname) > 0)
			fprintf(fp, "domain %s\n", lan_dname);

		if (nvram_get_int("lan_dns_x") == DISABLED)
		{
			dns_ip = nvram_safe_get(NVKEY_LAN_DNS1);
			if (is_valid_ipv4(dns_ip))
			{
				fprintf(fp, "nameserver %s\n", dns_ip);
				dns_count++;
			}

			dns_ip = nvram_safe_get("lan_dns2");
			if (is_valid_ipv4(dns_ip))
			{
				fprintf(fp, "nameserver %s\n", dns_ip);
				dns_count++;
			}
		}
		else
		{
			foreach (word, nvram_safe_get("lan_dns_t"), next)
			{
				if (is_valid_ipv4(word))
				{
					fprintf(fp, "nameserver %s\n", word);
					dns_count++;
				}
			}
		}

		if (!dns_count && is_valid_ipv4(lan_gateway))
			fprintf(fp, "nameserver %s\n", lan_gateway);

		fclose(fp);
	}

	/* sync time */
	notify_watchdog_time();

	/* fill XXX_t fields */
	update_lan_status(ENABLED);

#if defined(APP_SMBD)
	/* update SMB fastpath owner address */
	config_smb_fastpath(ENABLED);
#endif

	/* di wakeup after 2 secs */
	notify_run_detect_internet(2);
}

static void
lan_down_auto(char *lan_ifname)
{
	FILE *fp;
	char *lan_gateway = nvram_safe_get(NVKEY_LAN_GATEWAY_TEMP);

	notify_pause_detect_internet();

	/* Remove default route to gateway if specified */
	if (is_valid_ipv4(lan_gateway))
		route_del(lan_ifname, DISABLED, IPADDR_ANY, lan_gateway, IPADDR_ANY);

	/* Clear resolv.conf */
	fp = fopen(PATH_RESOLV_CONF, "w+");
	if (fp)
		fclose(fp);

	/* fill XXX_t fields */
	update_lan_status(DISABLED);

#if defined(APP_SMBD)
	/* update SMB fastpath owner address */
	config_smb_fastpath(ENABLED);
#endif
}

void update_lan_status(int is_auto)
{
	if (!is_auto)
	{
		nvram_set_temp("lan_ipaddr_t", nvram_safe_get(NVKEY_LAN_IPADDR));
		nvram_set_temp("lan_netmask_t", nvram_safe_get(NVKEY_LAN_NETMASK));
		nvram_set_temp("lan_domain_t", nvram_safe_get("lan_domain"));

		if (!get_ap_mode())
		{
			if (is_dhcpd_enabled(DISABLED))
			{
				if (nvram_invmatch("dhcp_gateway_x", ""))
					nvram_set_temp("lan_gateway_t", nvram_safe_get("dhcp_gateway_x"));
				else
					nvram_set_temp("lan_gateway_t", nvram_safe_get(NVKEY_LAN_IPADDR));
			}
			else
				nvram_set_temp("lan_gateway_t", nvram_safe_get(NVKEY_LAN_IPADDR));
		}
		else
			nvram_set_temp("lan_gateway_t", nvram_safe_get("lan_gateway"));
	}
}

static int
udhcpc_lan_deconfig(char *lan_ifname)
{
	ifconfig(lan_ifname, IFUP,
			 nvram_safe_get(NVKEY_LAN_IPADDR),
			 nvram_safe_get(NVKEY_LAN_NETMASK));

	lan_down_auto(lan_ifname);

	logmessage(LOGSRC_DHCP_CLIENT, "%s: lease is lost", "deconfig");

	return DISABLED;
}

static int udhcpc_lan_bound(char *lan_ifname, int is_renew, char *udhcpc_lan_state) {
    char *value;
    char tmp[100], prefix[16];
    int is_changed = DISABLED, ip_changed = DISABLED, lease_dur = DISABLED;

    snprintf(prefix, sizeof(prefix), "lan_");

    char ipaddr_param[100], netmask_param[100], gateway_param[100], dns_param[100], domain_param[100];

    snprintf(ipaddr_param, sizeof(ipaddr_param), "%s%s", prefix, "ipaddr_t");
    snprintf(netmask_param, sizeof(netmask_param), "%s%s", prefix, "netmask_t");
    snprintf(gateway_param, sizeof(gateway_param), "%s%s", prefix, "gateway_t");
    snprintf(dns_param, sizeof(dns_param), "%s%s", prefix, "dns_t");
    snprintf(domain_param, sizeof(domain_param), "%s%s", prefix, "domain_t");

    if ((value = getenv("ip"))) {
        is_changed |= nvram_invmatch(ipaddr_param, value);
        ip_changed |= is_changed;
        nvram_set_temp(ipaddr_param, value);
    }
    if ((value = getenv("subnet"))) {
        is_changed |= nvram_invmatch(netmask_param, value);
        nvram_set_temp(netmask_param, value);
    } else {
        is_changed |= ENABLED;
    }
    if ((value = getenv("router"))) {
        is_changed |= nvram_invmatch(gateway_param, value);
        nvram_set_temp(gateway_param, value);
    }
    if ((value = getenv("dns"))) {
        is_changed |= nvram_invmatch(dns_param, value);
        nvram_set_temp(dns_param, value);
    }
    if ((value = getenv("domain"))) {
        is_changed |= nvram_invmatch(domain_param, value);
        nvram_set_temp(domain_param, value);
    }
    if ((value = getenv("wins"))) {
        snprintf(tmp, sizeof(tmp), "%s%s", prefix, "wins_t");
        nvram_set_temp(tmp, value);
    }
    if ((value = getenv("lease"))) {
        lease_dur = atoi(value);
        snprintf(tmp, sizeof(tmp), "%s%s", prefix, "lease_t");
        nvram_set_temp(tmp, value);
    }

    if (is_changed || !is_renew) {
        char *lan_ipaddr = nvram_safe_get(ipaddr_param);
        char *lan_ipmask = nvram_safe_get(netmask_param);
        char *lan_gateway = nvram_safe_get(gateway_param);
        char *lan_domain = nvram_safe_get(domain_param);

        if (ip_changed)
            ifconfig(lan_ifname, IFUP, IPADDR_ANY, NULL);

        ifconfig(lan_ifname, IFUP, lan_ipaddr, lan_ipmask);

        create_hosts_lan(lan_ipaddr, lan_domain);

        lan_up_auto(lan_ifname, lan_gateway, lan_domain);

        logmessage(LOGSRC_DHCP_CLIENT, "%s, IP: %s/%s, GW: %s, lease time: %d",
                   udhcpc_lan_state, lan_ipaddr, lan_ipmask, lan_gateway, lease_dur);
    }

    if (!is_renew)
        restart_networkmap();

    return DISABLED;
}

static int
udhcpc_lan_leasefail(char *lan_ifname)
{
	return DISABLED;
}

static int
udhcpc_lan_noack(char *lan_ifname)
{
	logmessage(LOGSRC_DHCP_CLIENT, "Received NAK for %s", lan_ifname);
	return DISABLED;
}

int udhcpc_lan_main(int argc, char **argv)
{
	int ret = DISABLED;
	char *lan_ifname;
	char udhcpc_lan_state[16] = {0};

	if (argc < 2 || !argv[1])
		return EINVAL;

	lan_ifname = safe_getenv("interface");
	snprintf(udhcpc_lan_state, sizeof(udhcpc_lan_state), "%s", argv[1]);

	umask(0000);

	if (!strcmp(argv[1], "deconfig"))
		ret = udhcpc_lan_deconfig(lan_ifname);
	else if (!strcmp(argv[1], "bound"))
		ret = udhcpc_lan_bound(lan_ifname, DISABLED, udhcpc_lan_state);
	else if (!strcmp(argv[1], "renew"))
		ret = udhcpc_lan_bound(lan_ifname, ENABLED, udhcpc_lan_state);
	else if (!strcmp(argv[1], "leasefail"))
		ret = udhcpc_lan_leasefail(lan_ifname);
	else if (!strcmp(argv[1], "nak"))
		ret = udhcpc_lan_noack(lan_ifname);

	return ret;
}

int start_udhcpc_lan(char *lan_ifname) {
    static char lan_mergedhostname[80];
    const char *our_hostname = get_our_hostname();
    char *dhcp_argv[] = {
        "/sbin/udhcpc",
        "-i", lan_ifname,
        "-s", SCRIPT_UDHCPC_LAN,
        "-p", PIDFILE_UDHCPC_LAN,
        "-t", STRINGIFY(DHCPC_DEFAULT_RETRIES),
        "-T", STRINGIFY(DHCPC_DEFAULT_TIMEOUT),
        "-d", // Background after run (new patch for udhcpc)
        NULL, // Placeholder for "-x"
        NULL, // Placeholder for hostname argument
        NULL  // Terminator
    };
    int index = 7;
    if (our_hostname && our_hostname[0] != '\0') {
        snprintf(lan_mergedhostname, sizeof(lan_mergedhostname), "hostname:%s", our_hostname);
        dhcp_argv[index++] = "-x";
        dhcp_argv[index++] = lan_mergedhostname; // Use the static buffer
    }

    logmessage(LOGSRC_DHCP_CLIENT, "starting on %s ...", lan_ifname);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // Child process
        execv(dhcp_argv[0], dhcp_argv);
        perror("execv");
        _exit(EXIT_FAILURE);
    }

    // Parent process continues immediately without waitpid
    return DISABLED;
}

int stop_udhcpc_lan()
{
	return kill_pidfile(PIDFILE_UDHCPC_LAN);
}
