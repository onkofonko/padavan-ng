/* source: xio-sctp.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for SCTP related functions and options */

#include "xiosysincludes.h"

#if WITH_SCTP

#include "xioopen.h"
#include "xio-listen.h"
#include "xio-ip4.h"
#include "xio-ipapp.h"
#include "xio-sctp.h"

/****** SCTP addresses ******/

#if WITH_IP4 || WITH_IP6
const struct addrdesc xioaddr_sctp_connect = { "SCTP-CONNECT", 1+XIO_RDWR, xioopen_ipapp_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_SCTP|GROUP_CHILD|GROUP_RETRY, SOCK_STREAM, IPPROTO_SCTP, PF_UNSPEC HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_sctp_listen  = { "SCTP-LISTEN",  1+XIO_RDWR, xioopen_ipapp_listen,  GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_SCTP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE|GROUP_RETRY, SOCK_STREAM, IPPROTO_SCTP, PF_UNSPEC HELP(":<port>") };
#endif
#endif

#if WITH_IP4
const struct addrdesc xioaddr_sctp4_connect = { "SCTP4-CONNECT", 1+XIO_RDWR, xioopen_ipapp_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_SCTP|GROUP_CHILD|GROUP_RETRY, SOCK_STREAM, IPPROTO_SCTP, PF_INET HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_sctp4_listen  = { "SCTP4-LISTEN", 1+XIO_RDWR, xioopen_ipapp_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_SCTP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE|GROUP_RETRY, SOCK_STREAM, IPPROTO_SCTP, PF_INET HELP(":<port>") };
#endif
#endif /* WITH_IP4 */

#if WITH_IP6
const struct addrdesc xioaddr_sctp6_connect = { "SCTP6-CONNECT", 1+XIO_RDWR, xioopen_ipapp_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_SCTP|GROUP_CHILD|GROUP_RETRY, SOCK_STREAM, IPPROTO_SCTP, PF_INET6 HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_sctp6_listen  = { "SCTP6-LISTEN", 1+XIO_RDWR, xioopen_ipapp_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_SCTP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE|GROUP_RETRY, SOCK_STREAM, IPPROTO_SCTP, PF_INET6 HELP(":<port>") };
#endif
#endif /* WITH_IP6 */

/****** SCTP address options ******/

#ifdef SCTP_NODELAY
const struct optdesc opt_sctp_nodelay = { "sctp-nodelay",   "nodelay", OPT_SCTP_NODELAY, GROUP_IP_SCTP, PH_PASTSOCKET, TYPE_INT,	OFUNC_SOCKOPT, SOL_SCTP, SCTP_NODELAY };
#endif
#ifdef SCTP_MAXSEG
const struct optdesc opt_sctp_maxseg  = { "sctp-maxseg",    "mss",  OPT_SCTP_MAXSEG,  GROUP_IP_SCTP, PH_PASTSOCKET,TYPE_INT, OFUNC_SOCKOPT, SOL_SCTP, SCTP_MAXSEG };
const struct optdesc opt_sctp_maxseg_late={"sctp-maxseg-late","mss-late",OPT_SCTP_MAXSEG_LATE,GROUP_IP_SCTP,PH_CONNECTED,TYPE_INT,OFUNC_SOCKOPT, SOL_SCTP, SCTP_MAXSEG};
#endif

#endif /* WITH_SCTP */
