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

#include "rc.h"

#if defined (USE_IPV6)

void reset_lan6_vars(void)
{
	struct in6_addr addr6;
	char addr6s[INET6_ADDRSTRLEN] = {0};
	char *lan_addr6;
	int lan_size6;

	if (is_lan_addr6_static() == 1)
	{
		lan_addr6 = nvram_safe_get("ip6_lan_addr");
		lan_size6 = nvram_get_int("ip6_lan_size");
		if (*lan_addr6) {
			memset(&addr6, 0, sizeof(addr6));
			// Check return value of ipv6_from_string
			if (ipv6_from_string(lan_addr6, &addr6) >= 0) {
				char addr_part[INET6_ADDRSTRLEN]; // Temporary buffer for the address part
				// Check return value of inet_ntop before using addr_part
				if (inet_ntop(AF_INET6, &addr6, addr_part, sizeof(addr_part)) != NULL) {
					if (lan_size6 < 48 || lan_size6 > 64)
						lan_size6 = 64;
					// Use snprintf with the temporary buffer
					snprintf(addr6s, sizeof(addr6s), "%s/%d", addr_part, lan_size6);
				} else {
					syslog(LOG_ERR, "inet_ntop failed for static IPv6 address: %s", lan_addr6);
					addr6s[0] = '\0'; // Ensure addr6s is empty on error
				}
			} else {
				// Handle error: log or clear addr6s if needed
				syslog(LOG_ERR, "Invalid static IPv6 address: %s", lan_addr6);
				addr6s[0] = '\0'; // Ensure addr6s is empty on error
			}
		}
	}

	nvram_set_temp("lan_addr6", addr6s);
}

int is_lan_radv_on(void)
{
	int ipv6_type = get_ipv6_type();

	if (ipv6_type == IPV6_DISABLED)
		return -1;

	if (nvram_invmatch("ip6_lan_radv", "0"))
		return 1;

	return 0;
}

int is_lan_addr6_static(void)
{
	int ipv6_type = get_ipv6_type();

	if (ipv6_type == IPV6_DISABLED)
		return -1;

	if (ipv6_type == IPV6_6TO4 ||
	    ipv6_type == IPV6_6RD)
		return 0;

	if (nvram_match("ip6_lan_auto", "0") || 
	    ipv6_type == IPV6_NATIVE_STATIC || 
	    ipv6_type == IPV6_6IN4)
		return 1;
	
	return 0;
}

int get_lan_dhcp6s_mode(void)
{
	if (get_ipv6_type() == IPV6_DISABLED)
		return -1;

	return nvram_get_int("ip6_lan_dhcp");
}

int get_lan_dhcp6s_irt(void)
{
	int irt = 600;			// 10 min (IRT_MINIMUM=600)

	if (is_lan_addr6_static() == 1)
		irt = 1800;		// 30 min

	return irt;
}

int get_lan_dhcp6s_prefix_size(void)
{
	int lan_size6 = 64;

#if 0
	/* dnsmasq support only prefix len 64 for construct case */
	if (is_lan_addr6_static() == 1)
		lan_size6 = nvram_safe_get_int("ip6_lan_size", 64, 48, 64);
#endif

	return lan_size6;
}

int store_lan_addr6(char *lan_addr6_new)
{
	char *lan_addr6_old;

	if (!lan_addr6_new)
		return 0;

	if (!(*lan_addr6_new))
		return 0;

	lan_addr6_old = nvram_safe_get("lan_addr6");
	if (strcmp(lan_addr6_new, lan_addr6_old) != 0) {
		nvram_set_temp("lan_addr6", lan_addr6_new);
		return 1;
	}

	return 0;
}

void reload_lan_addr6(void)
{
	char *lan_addr6;
	const char *p;
	int valid = 1;
	int ret; // Variable to store doSystem return value

	clear_if_addr6(IFNAME_BR);
	lan_addr6 = nvram_safe_get("lan_addr6");
	if (*lan_addr6) {
		// Validate lan_addr6 content
		for (p = lan_addr6; *p; p++) {
			if (!isalnum(*p) && *p != ':' && *p != '/') {
				valid = 0;
				break;
			}
		}

		if (valid) {
			// Check return value of doSystem
			ret = doSystem("ip -6 addr add %s dev %s", lan_addr6, IFNAME_BR);
			if (ret != 0) {
				syslog(LOG_ERR, "Failed to add IPv6 address %s to %s (error: %d)", lan_addr6, IFNAME_BR, ret);
			}
		} else {
			syslog(LOG_ERR, "Invalid characters detected in lan_addr6: %s", lan_addr6);
		}
	}
}

void clear_lan_addr6(void)
{
	clear_if_addr6(IFNAME_BR);
}

char *get_lan_addr6_host(char *p_addr6s)
{
	char *tmp = p_addr6s;
	char *lan_addr6 = nvram_safe_get("lan_addr6");

	if (*lan_addr6) {
		snprintf(p_addr6s, INET6_ADDRSTRLEN, "%s", lan_addr6);
		strsep(&tmp, "/");
		return p_addr6s;
	}

	return NULL;
}

char *get_lan_addr6_prefix(char *p_addr6s)
{
	/* force prefix len to 64 for SLAAC */
	struct in6_addr addr6;
	char *lan_addr6 = nvram_safe_get("lan_addr6");
	size_t len; // Use size_t for length
	char addr_part[INET6_ADDRSTRLEN]; // Temporary buffer for the address part

	if (*lan_addr6) {
		// Check return value of ipv6_from_string
		if (ipv6_from_string(lan_addr6, &addr6) < 0) {
			syslog(LOG_ERR, "Invalid IPv6 address string in lan_addr6: %s", lan_addr6);
			return NULL;
		}

		ipv6_to_net(&addr6, 64);
		// Check return value of inet_ntop
		if (inet_ntop(AF_INET6, &addr6, addr_part, sizeof(addr_part)) == NULL) {
			syslog(LOG_ERR, "inet_ntop failed for lan_addr6 prefix calculation");
			return NULL;
		}

		// Use snprintf with the temporary buffer
		snprintf(p_addr6s, INET6_ADDRSTRLEN, "%s/%d", addr_part, 64);

		return p_addr6s;
	}

	return NULL;
}

#endif
