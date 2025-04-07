/* source: xio-udplite.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for handling UDPLITE addresses */

#include "xiosysincludes.h"

#if WITH_UDPLITE && (WITH_IP4 || WITH_IP6)

#include "xioopen.h"
#include "xio-socket.h"
#include "xio-ip4.h"
#include "xio-ip6.h"
#include "xio-ip.h"
#include "xio-ipapp.h"
#include "xio-tcpwrap.h"

#include "xio-udp.h"
#include "xio-udplite.h"


/* due to bug in Ubuntu-18.04 */
#ifndef UDPLITE_SEND_CSCOV
#  define UDPLITE_SEND_CSCOV 10
#endif
#ifndef UDPLITE_RECV_CSCOV
#  define UDPLITE_RECV_CSCOV 11
#endif


const struct addrdesc xioaddr_udplite_connect   = { "UDPLITE-CONNECT",   1+XIO_RDWR,   xioopen_ipapp_connect,  GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE, SOCK_DGRAM, IPPROTO_UDPLITE, PF_UNSPEC HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_udplite_listen    = { "UDPLITE-LISTEN",    1+XIO_RDWR,   xioopen_ipdgram_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE, PF_UNSPEC, IPPROTO_UDPLITE, PF_UNSPEC HELP(":<port>") };
#endif /* WITH_LISTEN */
const struct addrdesc xioaddr_udplite_sendto    = { "UDPLITE-SENDTO",    1+XIO_RDWR,   xioopen_udp_sendto,     GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE, PF_UNSPEC, SOCK_DGRAM, IPPROTO_UDPLITE HELP(":<host>:<port>") };
const struct addrdesc xioaddr_udplite_recvfrom  = { "UDPLITE-RECVFROM",  1+XIO_RDWR,   xioopen_udp_recvfrom,   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_CHILD|GROUP_RANGE, PF_UNSPEC, SOCK_DGRAM, IPPROTO_UDPLITE HELP(":<port>") };
const struct addrdesc xioaddr_udplite_recv      = { "UDPLITE-RECV",      1+XIO_RDONLY, xioopen_udp_recv,       GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_RANGE,             PF_UNSPEC, SOCK_DGRAM, IPPROTO_UDPLITE  HELP(":<port>") };
const struct addrdesc xioaddr_udplite_datagram  = { "UDPLITE-DATAGRAM",  1+XIO_RDWR,   xioopen_udp_datagram,   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_RANGE, PF_UNSPEC, SOCK_DGRAM, IPPROTO_UDPLITE HELP(":<host>:<port>") };

#if WITH_IP4
const struct addrdesc xioaddr_udplite4_connect  = { "UDPLITE4-CONNECT",  1+XIO_RDWR,   xioopen_ipapp_connect,  GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_IP_UDPLITE, SOCK_DGRAM, IPPROTO_UDPLITE, PF_INET HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_udplite4_listen   = { "UDPLITE4-LISTEN",   1+XIO_RDWR,   xioopen_ipdgram_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE, PF_INET, IPPROTO_UDPLITE, PF_INET HELP(":<port>") };
#endif /* WITH_LISTEN */
const struct addrdesc xioaddr_udplite4_sendto   = { "UDPLITE4-SENDTO",   1+XIO_RDWR,   xioopen_udp_sendto,     GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_IP_UDPLITE, PF_INET, SOCK_DGRAM, IPPROTO_UDPLITE  HELP(":<host>:<port>") };
const struct addrdesc xioaddr_udplite4_datagram = { "UDPLITE4-DATAGRAM", 1+XIO_RDWR,   xioopen_udp_datagram,   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_RANGE, PF_INET, SOCK_DGRAM, IPPROTO_UDPLITE HELP(":<remote-address>:<port>") };
const struct addrdesc xioaddr_udplite4_recvfrom = { "UDPLITE4-RECVFROM", 1+XIO_RDWR,   xioopen_udp_recvfrom,   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_CHILD|GROUP_RANGE, PF_INET, SOCK_DGRAM, IPPROTO_UDPLITE  HELP(":<host>:<port>") };
const struct addrdesc xioaddr_udplite4_recv     = { "UDPLITE4-RECV",     1+XIO_RDONLY, xioopen_udp_recv,       GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_RANGE,             PF_INET, SOCK_DGRAM, IPPROTO_UDPLITE  HELP(":<port>") };
#endif /* WITH_IP4 */

#if WITH_IP6
const struct addrdesc xioaddr_udplite6_connect  = { "UDPLITE6-CONNECT",  1+XIO_RDWR,   xioopen_ipapp_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE, SOCK_DGRAM, IPPROTO_UDPLITE, PF_INET6 HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_udplite6_listen   = { "UDPLITE6-LISTEN",   1+XIO_RDWR,   xioopen_ipdgram_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE, PF_INET6, IPPROTO_UDPLITE, 0 HELP(":<port>") };
#endif /* WITH_LISTEN */
const struct addrdesc xioaddr_udplite6_sendto   = { "UDPLITE6-SENDTO",   1+XIO_RDWR,   xioopen_udp_sendto, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE, PF_INET6, SOCK_DGRAM, IPPROTO_UDPLITE HELP(":<host>:<port>") };
const struct addrdesc xioaddr_udplite6_datagram = { "UDPLITE6-DATAGRAM", 1+XIO_RDWR,   xioopen_udp_datagram,GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_RANGE, PF_INET6, SOCK_DGRAM, IPPROTO_UDPLITE HELP(":<host>:<port>") };
const struct addrdesc xioaddr_udplite6_recvfrom = { "UDPLITE6-RECVFROM", 1+XIO_RDWR,   xioopen_udp_recvfrom, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_CHILD|GROUP_RANGE, PF_INET6, SOCK_DGRAM, IPPROTO_UDPLITE  HELP(":<port>") };
const struct addrdesc xioaddr_udplite6_recv     = { "UDPLITE6-RECV",     1+XIO_RDONLY, xioopen_udp_recv,     GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_IP_UDPLITE|GROUP_RANGE,             PF_INET6, SOCK_DGRAM, IPPROTO_UDPLITE  HELP(":<port>") };
#endif /* WITH_IP6 */

const struct optdesc xioopt_udplite_send_cscov = { "udplite-send-cscov", NULL, OPT_UDPLITE_SEND_CSCOV, GROUP_IP_UDPLITE, PH_FD, TYPE_INT, OFUNC_SOCKOPT, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV};
const struct optdesc xioopt_udplite_recv_cscov = { "udplite-recv-cscov", NULL, OPT_UDPLITE_RECV_CSCOV, GROUP_IP_UDPLITE, PH_FD, TYPE_INT, OFUNC_SOCKOPT, IPPROTO_UDPLITE, UDPLITE_RECV_CSCOV};

#endif /* WITH_UDPLITE && (WITH_IP4 || WITH_IP6) */
