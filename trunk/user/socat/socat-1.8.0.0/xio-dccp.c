/* source: xio-dccp.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for DCCP related functions and options */

#include "xiosysincludes.h"

#if WITH_DCCP

#include "xioopen.h"
#include "xio-listen.h"
#include "xio-ip4.h"
#include "xio-ipapp.h"
#include "xio-dccp.h"

/****** DCCP addresses ******/

#if WITH_IP4 || WITH_IP6
const struct addrdesc xioaddr_dccp_connect = { "DCCP-CONNECT", 1+XIO_RDWR, xioopen_ipapp_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_DCCP|GROUP_CHILD|GROUP_RETRY, SOCK_DCCP, IPPROTO_DCCP, PF_UNSPEC HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_dccp_listen  = { "DCCP-LISTEN",  1+XIO_RDWR, xioopen_ipapp_listen,  GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_DCCP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE|GROUP_RETRY, SOCK_DCCP, IPPROTO_DCCP, PF_UNSPEC HELP(":<port>") };
#endif
#endif

#if WITH_IP4
const struct addrdesc xioaddr_dccp4_connect = { "DCCP4-CONNECT", 1+XIO_RDWR, xioopen_ipapp_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_DCCP|GROUP_CHILD|GROUP_RETRY, SOCK_DCCP, IPPROTO_DCCP, PF_INET HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_dccp4_listen  = { "DCCP4-LISTEN", 1+XIO_RDWR, xioopen_ipapp_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_DCCP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE|GROUP_RETRY, SOCK_DCCP, IPPROTO_DCCP, PF_INET HELP(":<port>") };
#endif
#endif /* WITH_IP4 */

#if WITH_IP6
const struct addrdesc xioaddr_dccp6_connect = { "DCCP6-CONNECT", 1+XIO_RDWR, xioopen_ipapp_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_DCCP|GROUP_CHILD|GROUP_RETRY, SOCK_DCCP, IPPROTO_DCCP, PF_INET6 HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_dccp6_listen  = { "DCCP6-LISTEN", 1+XIO_RDWR, xioopen_ipapp_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_DCCP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE|GROUP_RETRY, SOCK_DCCP, IPPROTO_DCCP, PF_INET6 HELP(":<port>") };
#endif
#endif /* WITH_IP6 */

/****** DCCP address options ******/

#if defined(SOL_DCCP) && defined(DCCP_SOCKOPT_QPOLICY_ID)
const struct optdesc xioopt_dccp_set_ccid = { "dccp-set-ccid",   "ccid", OPT_DCCP_SET_CCID, GROUP_IP_DCCP, PH_PASTSOCKET, TYPE_BYTE,	OFUNC_SOCKOPT, SOL_DCCP, DCCP_SOCKOPT_CCID };
#endif

#endif /* WITH_DCCP */
