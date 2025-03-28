/* source: sysutils.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* translate socket addresses into human readable form */

#include "config.h"
#include "xioconfig.h"

#include "sysincludes.h"

#include "compat.h"	/* socklen_t */
#include "mytypes.h"
#include "error.h"
#include "sycls.h"
#include "utils.h"
#include "sysutils.h"

#if _WITH_INTERFACE
const int one = 1;
#endif

/* Substitute for Write():
   Try to write all bytes before returning; this handles EINTR,
   EAGAIN/EWOULDBLOCK, and partial write situations. The drawback is that this
   function might block even with O_NONBLOCK option.
   Returns <0 on unhandled error, errno valid
   Will only return <0 or bytes
*/
/* Assuming your pipe size is 65536 - find out with:
   filan -i 1 |grep F_GETPIPE_SZ |sed 's|.*\(F_GETPIPE_SZ=[1-9][0-9]*\).*|\1|'
   Then we can test partial write with something like:
   socat -d4 -lu -b 262144 -u /dev/zero,readbytes=262144 -,o-nonblock |{ sleep 3; wc -c; }
*/
ssize_t writefull(
	int fd,
	const void *buff,
	size_t bytes,
	const struct timeval *tmo0) {
   size_t writt = 0;
   ssize_t chk;
   struct pollfd pfd;
   struct timeval tmo = { 0 };
   int rc;

   while (1) {
      chk = Write(fd, (const char *)buff + writt, bytes - writt);
      if (chk < 0) {
	 switch (errno) {
	 case EINTR:
	 case EAGAIN:
#if EAGAIN != EWOULDBLOCK
	 case EWOULDBLOCK:
#endif
	    Warn4("write(%d, %p, "F_Zu"): %s", fd, (const char *)buff+writt, bytes-writt, strerror(errno));
	    pfd.fd      = fd;
	    pfd.events  = POLLOUT;
	    pfd.revents = 0;
	    if (tmo0 != NULL) {
	       tmo.tv_sec  = tmo0->tv_sec;
	       tmo.tv_usec = tmo0->tv_usec;
	    }
	    rc = xiopoll(&pfd, 1, (tmo.tv_sec!=0 || tmo.tv_usec!=0) ? &tmo : NULL);
	    if (rc == 0) {
	       Notice("inactivity timeout triggered");
	       errno = ETIMEDOUT;
	       return -1;
	    }
	    continue;
	 default:
	    return -1;
	 }
      } else if (chk == bytes) {
	 /* First attempt, complete write */
	 return chk;
      } else if (writt+chk < bytes) {
	 Warn4("write(%d, %p, "F_Zu"): only wrote "F_Zu" bytes, trying to continue (meanwhile, other direction is blocked)",
	       fd, (const char *)buff+writt, bytes-writt, chk);
	 writt += chk;
      } else if (writt == 0) {
	 /* First attempt, write complete - no extra message */
	 return chk;
      } else { 	/* write completed */
	 break;
      }
   }
   Notice3("write(%d, %p, "F_Zu") completed", fd, (const char *)buff, bytes);
   return writt;
}

#if WITH_UNIX
void socket_un_init(struct sockaddr_un *sa) {
#if HAVE_STRUCT_SOCKADDR_SALEN
   sa->sun_len         = sizeof(struct sockaddr_un);
#endif
   sa->sun_family      = AF_UNIX;
   memset(sa->sun_path, '\0', sizeof(sa->sun_path));
}
#endif /* WITH_UNIX */

#if WITH_IP4
void socket_in_init(struct sockaddr_in *sa) {
#if HAVE_STRUCT_SOCKADDR_SALEN
   sa->sin_len         = sizeof(struct sockaddr_in);
#endif
   sa->sin_family      = AF_INET;
   sa->sin_port        = 0;
   sa->sin_addr.s_addr = 0;
   sa->sin_zero[0]     = 0;
   sa->sin_zero[1]     = 0;
   sa->sin_zero[2]     = 0;
   sa->sin_zero[3]     = 0;
   sa->sin_zero[4]     = 0;
   sa->sin_zero[5]     = 0;
   sa->sin_zero[6]     = 0;
   sa->sin_zero[7]     = 0;
}
#endif /* WITH_IP4 */

#if WITH_IP6
void socket_in6_init(struct sockaddr_in6 *sa) {
#if HAVE_STRUCT_SOCKADDR_SALEN
   sa->sin6_len        = sizeof(struct sockaddr_in6);
#endif
   sa->sin6_family     = AF_INET6;
   sa->sin6_port       = 0;
   sa->sin6_flowinfo   = 0;
#if HAVE_IP6_SOCKADDR==0
   sa->sin6_addr.s6_addr[0] = 0;
   sa->sin6_addr.s6_addr[1] = 0;
   sa->sin6_addr.s6_addr[2] = 0;
   sa->sin6_addr.s6_addr[3] = 0;
   sa->sin6_addr.s6_addr[4] = 0;
   sa->sin6_addr.s6_addr[5] = 0;
   sa->sin6_addr.s6_addr[6] = 0;
   sa->sin6_addr.s6_addr[7] = 0;
   sa->sin6_addr.s6_addr[8] = 0;
   sa->sin6_addr.s6_addr[9] = 0;
   sa->sin6_addr.s6_addr[10] = 0;
   sa->sin6_addr.s6_addr[11] = 0;
   sa->sin6_addr.s6_addr[12] = 0;
   sa->sin6_addr.s6_addr[13] = 0;
   sa->sin6_addr.s6_addr[14] = 0;
   sa->sin6_addr.s6_addr[15] = 0;
#elif HAVE_IP6_SOCKADDR==1
   sa->sin6_addr.u6_addr.u6_addr32[0] = 0;
   sa->sin6_addr.u6_addr.u6_addr32[1] = 0;
   sa->sin6_addr.u6_addr.u6_addr32[2] = 0;
   sa->sin6_addr.u6_addr.u6_addr32[3] = 0;
#elif HAVE_IP6_SOCKADDR==2
   sa->sin6_addr.u6_addr32[0] = 0;
   sa->sin6_addr.u6_addr32[1] = 0;
   sa->sin6_addr.u6_addr32[2] = 0;
   sa->sin6_addr.u6_addr32[3] = 0;
#elif HAVE_IP6_SOCKADDR==3
   sa->sin6_addr.in6_u.u6_addr32[0] = 0;
   sa->sin6_addr.in6_u.u6_addr32[1] = 0;
   sa->sin6_addr.in6_u.u6_addr32[2] = 0;
   sa->sin6_addr.in6_u.u6_addr32[3] = 0;
#elif HAVE_IP6_SOCKADDR==4
   sa->sin6_addr._S6_un._S6_u32[0] = 0;
   sa->sin6_addr._S6_un._S6_u32[1] = 0;
   sa->sin6_addr._S6_un._S6_u32[2] = 0;
   sa->sin6_addr._S6_un._S6_u32[3] = 0;
#elif HAVE_IP6_SOCKADDR==5
   sa->sin6_addr.__u6_addr.__u6_addr32[0] = 0;
   sa->sin6_addr.__u6_addr.__u6_addr32[1] = 0;
   sa->sin6_addr.__u6_addr.__u6_addr32[2] = 0;
   sa->sin6_addr.__u6_addr.__u6_addr32[3] = 0;
#endif
}
#endif /* WITH_IP6 */


#if _WITH_SOCKET
/* initializes the socket address of the specified address family. Returns the
   length of the specific socket address, or 0 on error. */
socklen_t socket_init(int af, union sockaddr_union *sa) {
   switch (af) {
   case AF_UNSPEC: memset(sa, 0, sizeof(*sa)); return sizeof(*sa);
#if WITH_UNIX
   case AF_UNIX:   socket_un_init(&sa->un);   return sizeof(sa->un);
#endif
#if WITH_IP4
   case AF_INET:   socket_in_init(&sa->ip4);  return sizeof(sa->ip4);
#endif
#if WITH_IP6
   case AF_INET6:  socket_in6_init(&sa->ip6); return sizeof(sa->ip6);
#endif
   default: Info1("socket_init(): unknown address family %d", af);
      memset(sa, 0, sizeof(union sockaddr_union));
      sa->soa.sa_family = af;
      return 0;
   }
}
#endif /* _WITH_SOCKET */

#if WITH_UNIX
#define XIOUNIXSOCKOVERHEAD (sizeof(struct sockaddr_un)-sizeof(((struct sockaddr_un*)0)->sun_path))
#endif

#if _WITH_SOCKET
/* writes a textual human readable representation of the sockaddr contents to buff and returns a pointer to buff
   writes at most blen bytes to buff including the terminating \0 byte
 */
char *sockaddr_info(const struct sockaddr *sa, socklen_t salen, char *buff, size_t blen) {
   union sockaddr_union *sau = (union sockaddr_union *)sa;
   char *lbuff = buff;
   char *cp = lbuff;
   int n;

#if HAVE_STRUCT_SOCKADDR_SALEN
   n = xio_snprintf(cp, blen, "LEN=%d ", sau->soa.sa_len);
   if (n < 0 || n >= blen) {
      Warn1("sockaddr_info(): buffer too short ("F_Zu")", blen);
      *buff = '\0';
      return buff;
   }
   cp += n,  blen -= n;
#endif
   n = xio_snprintf(cp, blen, "AF=%d ", sau->soa.sa_family);
   if (n < 0 || n >= blen) {
      Warn1("sockaddr_info(): buffer too short ("F_Zu")", blen);
      *buff = '\0';
      return buff;
   }
   cp += n,  blen -= n;

   switch (sau->soa.sa_family) {
#if WITH_UNIX
   case 0:
   case AF_UNIX:
      *cp++ = '"';  --blen;
      sockaddr_unix_info(&sau->un, salen, cp, blen-1);
      blen -= strlen(cp);
      cp = strchr(cp, '\0');
      *cp++ = '"';  --blen;
      if (blen > 0)
	 *cp = '\0';
      break;
#endif
#if WITH_IP4
   case AF_INET: sockaddr_inet4_info(&sau->ip4, cp, blen);
      break;
#endif
#if WITH_IP6
   case AF_INET6: sockaddr_inet6_info(&sau->ip6, cp, blen);
      break;
#endif
#if WITH_VSOCK
   case AF_VSOCK: sockaddr_vm_info(&sau->vm, cp, blen);
      break;
#endif
   default:
      n = xio_snprintf(cp, blen, "AF=%d ", sa->sa_family);
      if (n < 0 || n >= blen) {
	 Warn1("sockaddr_info(): buffer too short ("F_Zu")", blen);
	 *buff = '\0';
	 return buff;
      }
      cp += n,  blen -= n;
      n = xio_snprintf(cp, blen,
		    "0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		    ((unsigned char *)sau->soa.sa_data)[0],
		    ((unsigned char *)sau->soa.sa_data)[1],
		    ((unsigned char *)sau->soa.sa_data)[2],
		    ((unsigned char *)sau->soa.sa_data)[3],
		    ((unsigned char *)sau->soa.sa_data)[4],
		    ((unsigned char *)sau->soa.sa_data)[5],
		    ((unsigned char *)sau->soa.sa_data)[6],
		    ((unsigned char *)sau->soa.sa_data)[7],
		    ((unsigned char *)sau->soa.sa_data)[8],
		    ((unsigned char *)sau->soa.sa_data)[9],
		    ((unsigned char *)sau->soa.sa_data)[10],
		    ((unsigned char *)sau->soa.sa_data)[11],
		    ((unsigned char *)sau->soa.sa_data)[12],
		   ((unsigned char *)sau->soa.sa_data)[13]);
      if (n < 0 || n >= blen) {
	 Warn("sockaddr_info(): buffer too short");
	 *buff = '\0';
	 return buff;
      }
   }
   return lbuff;
}
#endif /* _WITH_SOCKET */


#if WITH_UNIX
char *sockaddr_unix_info(const struct sockaddr_un *sa, socklen_t salen, char *buff, size_t blen) {
   char ubuff[5*UNIX_PATH_MAX+3];
   char *nextc;

#if WITH_ABSTRACT_UNIXSOCKET
   if (salen > XIOUNIXSOCKOVERHEAD &&
       sa->sun_path[0] == '\0') {
      nextc =
	 sanitize_string(sa->sun_path, salen-XIOUNIXSOCKOVERHEAD,
			 ubuff, XIOSAN_DEFAULT_BACKSLASH_OCT_3);
   } else
#endif /* WITH_ABSTRACT_UNIXSOCKET */
   {
      if (salen <= XIOUNIXSOCKOVERHEAD) {
	 nextc = sanitize_string ("<anon>", MIN(UNIX_PATH_MAX, strlen("<anon>")),
				  ubuff, XIOSAN_DEFAULT_BACKSLASH_OCT_3);
      } else {
	 nextc = sanitize_string(sa->sun_path,
				 MIN(UNIX_PATH_MAX, strlen(sa->sun_path)),
				 ubuff, XIOSAN_DEFAULT_BACKSLASH_OCT_3);
      }
   }
   *nextc = '\0';
   buff[0] = '\0'; strncat(buff, ubuff, blen-1);
   return buff;
}
#endif /* WITH_UNIX */

#if WITH_IP4
/* addr in host byte order! */
char *inet4addr_info(uint32_t addr, char *buff, size_t blen) {
   if (xio_snprintf(buff, blen, "%u.%u.%u.%u",
		(unsigned int)(addr >> 24), (unsigned int)((addr >> 16) & 0xff),
		(unsigned int)((addr >> 8) & 0xff), (unsigned int)(addr & 0xff)) >= blen) {
      Warn("inet4addr_info(): buffer too short");
      buff[blen-1] = '\0';
   }
   return buff;
}
#endif /* WITH_IP4 */

#if WITH_IP4
char *sockaddr_inet4_info(const struct sockaddr_in *sa, char *buff, size_t blen) {
   if (xio_snprintf(buff, blen, "%u.%u.%u.%u:%hu",
		((unsigned char *)&sa->sin_addr.s_addr)[0],
		((unsigned char *)&sa->sin_addr.s_addr)[1],
		((unsigned char *)&sa->sin_addr.s_addr)[2],
		((unsigned char *)&sa->sin_addr.s_addr)[3],
		htons(sa->sin_port)) >= blen) {
      Warn("sockaddr_inet4_info(): buffer too short");
      buff[blen-1] = '\0';
   }
   return buff;
}
#endif /* WITH_IP4 */

#if WITH_VSOCK
char *sockaddr_vm_info(const struct sockaddr_vm *sa, char *buff, size_t blen) {
   if (xio_snprintf(buff, blen, "cid:%u port:%u", sa->svm_cid, sa->svm_port) >= blen) {
      Warn("sockaddr_vm_info(): buffer too short");
      buff[blen-1] = '\0';
   }
   return buff;
}

int sockaddr_vm_parse(struct sockaddr_vm *sa, const char *cid_str,
                      const char *port_str)
{
   char *garbage = NULL;
   if (!cid_str) {
      sa->svm_cid = VMADDR_CID_ANY;
   } else {
      sa->svm_cid = strtoul(cid_str, &garbage, 0);
      if (*garbage != '\0') {
         Error1("sockaddr_vm - garbage in cid: \"%s\"", garbage);
         return -EINVAL;
      }
   }

   if (!port_str) {
      sa->svm_port = VMADDR_PORT_ANY;
   } else {
      sa->svm_port = strtoul(port_str, &garbage, 0);
      if (*garbage != '\0') {
         Error1("sockaddr_vm - garbage in port: \"%s\"", garbage);
         return -EINVAL;
      }
   }

   return 0;
}
#endif /* WITH_VSOCK */

#if !HAVE_INET_NTOP
/* http://www.opengroup.org/onlinepubs/000095399/functions/inet_ntop.html */
const char *inet_ntop(int pf, const void *binaddr,
		      char *addrtext, socklen_t textlen) {
   size_t retlen;
   switch (pf) {
   case PF_INET:
      if ((retlen =
	   xio_snprintf(addrtext, textlen, "%u.%u.%u.%u",
		    ((unsigned char *)binaddr)[0],
		    ((unsigned char *)binaddr)[1],
		    ((unsigned char *)binaddr)[2],
		    ((unsigned char *)binaddr)[3]))
	  >= textlen) {
	 errno = ENOSPC; return NULL;
      }
      break;
#if WITH_IP6
   case PF_INET6:
      if ((retlen =
	   xio_snprintf(addrtext, textlen, "%x:%x:%x:%x:%x:%x:%x:%x",
		    ntohs(((uint16_t *)binaddr)[0]),
		    ntohs(((uint16_t *)binaddr)[1]),
		    ntohs(((uint16_t *)binaddr)[2]),
		    ntohs(((uint16_t *)binaddr)[3]),
		    ntohs(((uint16_t *)binaddr)[4]),
		    ntohs(((uint16_t *)binaddr)[5]),
		    ntohs(((uint16_t *)binaddr)[6]),
		    ntohs(((uint16_t *)binaddr)[7])
		    ))
	  >= textlen) {
	 errno = ENOSPC; return NULL;
      }
      break;
#endif /* WITH_IP6 */
   default:
      errno = EAFNOSUPPORT;
      return NULL;
   }
   addrtext[retlen] = '\0';
   return addrtext;
}
#endif /* !HAVE_INET_NTOP */

#if WITH_IP6
/* convert the IP6 socket address to human readable form. buff should be at
   least 50 chars long. output includes the port number */
char *sockaddr_inet6_info(const struct sockaddr_in6 *sa, char *buff, size_t blen) {
   if (xio_snprintf(buff, blen, "[%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x]:%hu",
#if HAVE_IP6_SOCKADDR==0
		(sa->sin6_addr.s6_addr[0]<<8)+
		sa->sin6_addr.s6_addr[1],
		(sa->sin6_addr.s6_addr[2]<<8)+
		sa->sin6_addr.s6_addr[3],
		(sa->sin6_addr.s6_addr[4]<<8)+
		sa->sin6_addr.s6_addr[5],
		(sa->sin6_addr.s6_addr[6]<<8)+
		sa->sin6_addr.s6_addr[7],
		(sa->sin6_addr.s6_addr[8]<<8)+
		sa->sin6_addr.s6_addr[9],
		(sa->sin6_addr.s6_addr[10]<<8)+
		sa->sin6_addr.s6_addr[11],
		(sa->sin6_addr.s6_addr[12]<<8)+
		sa->sin6_addr.s6_addr[13],
		(sa->sin6_addr.s6_addr[14]<<8)+
		sa->sin6_addr.s6_addr[15],
#elif HAVE_IP6_SOCKADDR==1
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[7]),
#elif HAVE_IP6_SOCKADDR==2
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[7]),
#elif HAVE_IP6_SOCKADDR==3
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[7]),
#elif HAVE_IP6_SOCKADDR==4
		(sa->sin6_addr._S6_un._S6_u8[0]<<8)|(sa->sin6_addr._S6_un._S6_u8[1]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[2]<<8)|(sa->sin6_addr._S6_un._S6_u8[3]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[4]<<8)|(sa->sin6_addr._S6_un._S6_u8[5]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[6]<<8)|(sa->sin6_addr._S6_un._S6_u8[7]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[8]<<8)|(sa->sin6_addr._S6_un._S6_u8[9]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[10]<<8)|(sa->sin6_addr._S6_un._S6_u8[11]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[12]<<8)|(sa->sin6_addr._S6_un._S6_u8[13]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[14]<<8)|(sa->sin6_addr._S6_un._S6_u8[15]&0xff),
#elif HAVE_IP6_SOCKADDR==5
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[7]),
#endif
		ntohs(sa->sin6_port)) >= blen) {
      Warn("sockaddr_inet6_info(): buffer too short");
   }
   return buff;
}
#endif /* WITH_IP6 */


#if WITH_IP4
/* Checks if the given string is an IPv4 address.
   Returns 0 when it is an address.
   Returns 1 when it is not an address.
   Returns -1 when an error occurred (bad regex - internal) */
int check_ip4addr(
	const char *address)
{
#if HAVE_REGEX_H
	regex_t preg;
	if (regcomp(&preg, "^(25[0-5]|(2[0-4]|1[0-9]|[1-9]?)[0-9])\\.(25[0-5]|(2[0-4]|1[0-9]|[1-9]?)[0-9])\\.(25[0-5]|(2[0-4]|1[0-9]|[1-9]?)[0-9])\\.(25[0-5]|(2[0-4]|1[0-9]|[1-9]?)[0-9])$", REG_EXTENDED|REG_NOSUB)
	    != 0) {
		return -1; 	/* do not handle, just state that no match */
	}
	if (regexec(&preg, address, 0, NULL, 0) == 0) {
		return 0;
	}
#endif /* HAVE_REGEX_H */
	/* Fallback when no regexec() compiled in */
	return 1;
}
#endif /* WITH_IP4 */

#if WITH_IP6
/* Checks if the given string is an IPv6 address.
   Currently a hack, just checks a few criteria.
   Returns 0 when it is an address.
   Returns 1 when it is not an address.
   Returns -1 when an error occurred (bad regex - internal) */
int check_ip6addr(
	const char *address)
{
#if HAVE_REGEX_H
	regex_t preg;
	if (regcomp(&preg, "^\\[[0-9a-fA-F:]*\\]$", REG_EXTENDED|REG_NOSUB)
	    != 0) {
		return -1; 	/* do not handle, just state that no match */
	}
	if (regexec(&preg, address, 0, NULL, 0) == 0) {
		return 0;
	}
#endif /* HAVE_REGEX_H */
	/* Fallback when no regexec() compiled in */
	return 1;
}
#endif /* WITH_IP6 */

/* Checks if the given string is an IPv6 address.
   Currently a hack, just checks a few criteria.
   Returns 0 when it is an address.
   Returns 1 when it is not an address or on error. */
int check_ipaddr(
	const char *address)
{
	int res4, res6;
#if WITH_IP4
	if ((res4 = check_ip4addr(address)) == 0) {
		return 0;
	}
#endif
#if WITH_IP6
	if ((res6 = check_ip6addr(address)) == 0) {
		return 0;
	}
#endif
	return 1;
}


#if HAVE_GETGROUPLIST || (defined(HAVE_SETGRENT) && defined(HAVE_GETGRENT) && defined(HAVE_ENDGRENT))
/* fills the list with the supplementary group ids of user.
   caller passes size of list in ngroups, function returns number of groups in
   ngroups.
   function returns 0 if 0 or more groups were found, or 1 if the list is too
   short. */
int getusergroups(const char *user, gid_t *list, int *ngroups) {
#if HAVE_GETGROUPLIST
   /* we prefer getgrouplist because it may be much faster with many groups, but it is not standard */
   gid_t grp, twogrps[2];
   int two = 2;
   /* getgrouplist requires to pass an extra group id, typically the users primary group, that is then added to the supplementary group list. We don't want such an additional group in the result, but there is not "unspecified" gid value available. Thus we try to find an arbitrary supplementary group id that we then pass in a second call to getgrouplist. */
   grp = 0;
   Getgrouplist(user, grp, twogrps, &two);
   if (two == 1) {
      /* either user has just this supp group, or none; we try another id */
      grp = 1; two = 2;
      Getgrouplist(user, grp, twogrps, &two);
      if (two == 1) {
	 /* user has no supp group */
	 *ngroups = 0;
	 return 0;
      }
      /* user has just the first tried group */
      *ngroups = 1; list[0] = grp;
      return 0;
   }
   /* find the first supp group that is not our grp, and use its id */
   if (twogrps[0] == grp) {
      grp = twogrps[1];
   } else {
      grp = twogrps[0];
   }
   if (Getgrouplist(user, grp, list, ngroups) < 0) {
      return 1;
   }
   return 0;

#elif defined(HAVE_SETGRENT) && defined(HAVE_GETGRENT) && defined(HAVE_ENDGRENT)
   /* this is standard (POSIX) but may be slow */

   struct group *grp;
   int i = 0;

   setgrent();
   while (grp = getgrent()) {
      char **gusr = grp->gr_mem;
      while (*gusr) {
	 if (!strcmp(*gusr, user)) {
	    if (i == *ngroups)
	       return 1;
	    list[i++] = grp->gr_gid;
	    break;
	 }
	 ++gusr;
      }
   }
   endgrent();
   *ngroups = i;
   return 0;
#endif /* HAVE_SETGRENT... */
}
#endif

#if !HAVE_HSTRERROR
const char *hstrerror(int err) {
   static const char *h_messages[] = {
      "success",
      "authoritative answer not found",
      "non-authoritative, host not found, or serverfail",
      "Host name lookup failure",	/* "non recoverable error" */
      "valid name, no data record of requested type" };

   assert(HOST_NOT_FOUND==1);
   assert(TRY_AGAIN==2);
   assert(NO_RECOVERY==3);
   assert(NO_DATA==4);
   if ((err < 0) || err > sizeof(h_messages)/sizeof(const char *)) {
      return "";
   }
   return h_messages[err];
}
#endif /* !HAVE_HSTRERROR */


/* this function behaves like poll(). It tries to do so even when the poll()
   system call is not available. */
/* note: glibc 5.4 does not know nfds_t */
int xiopoll(struct pollfd fds[], unsigned long nfds, struct timeval *timeout) {
   int i, n = 0;
   int result = 0;

   while (true) { /* should be if (), but we want to break */
      fd_set readfds;
      fd_set writefds;
      fd_set exceptfds;

      FD_ZERO(&readfds);  FD_ZERO(&writefds);  FD_ZERO(&exceptfds);
      for (i = 0; i < nfds; ++i) {
	 fds[i].revents = 0;
	 if (fds[i].fd < 0)  { continue; }
	 if (fds[i].fd > FD_SETSIZE)  { break; /* use poll */ }
	 if (fds[i].events & POLLIN)  {
	    FD_SET(fds[i].fd, &readfds);  n = MAX(n, fds[i].fd); }
	 if (fds[i].events & POLLOUT) {
	    FD_SET(fds[i].fd, &writefds); n = MAX(n, fds[i].fd); }
      }
      if (i < nfds)  { break; /* use poll */ }

      result = Select(n+1, &readfds, &writefds, &exceptfds, timeout);
      if (result < 0)  { return result; }
      for (i = 0; i < nfds; ++i) {
	 if (fds[i].fd < 0)  { continue; }
	 if ((fds[i].events & POLLIN)  && FD_ISSET(fds[i].fd, &readfds))  {
	    fds[i].revents |= POLLIN;  ++result;
	 }
	 if ((fds[i].events & POLLOUT) && FD_ISSET(fds[i].fd, &writefds)) {
	    fds[i].revents |= POLLOUT; ++result;
	 }
      }
      return result;
   }
   {
#if HAVE_POLL
      int ms = 0;
      if (timeout == NULL) {
	 ms = -1;
      } else {
	 ms = 1000*timeout->tv_sec + timeout->tv_usec/1000;
      }
      /*! timeout */
      return Poll(fds, nfds, ms);
#else /* HAVE_POLL */
      Error("poll() not available");
      return -1;
#endif /* !HAVE_POLL */
   }
}


#if WITH_TCP || WITH_UDP || WITH_SCTP || WITH_DCCP || WITH_UDPLITE
/* returns port in network byte order;
   ipproto==IPPROTO_UDP resolves as UDP service, every other value resolves as
   TCP */
int parseport(const char *portname, int ipproto) {
   struct servent *se;
   char *extra;
   int result;

   if (isdigit(portname[0]&0xff)) {
      result = htons(strtoul(portname, &extra, 0));
      if (*extra != '\0') {
	 Error3("parseport(\"%s\", %d): extra trailing data \"%s\"",
		portname, ipproto, extra);
      }
      return result;
   }

   if ((se = getservbyname(portname, ipproto==IPPROTO_UDP?"udp":"tcp")) == NULL) {
      Error2("cannot resolve service \"%s/%d\"", portname, ipproto);
      return 0;
   }

   return se->s_port;
}
#endif /* WITH_TCP || WITH_UDP || WITH_SCTP || WITH_DCCP || WITH_UDPLITE */


#if WITH_IP4 || WITH_IP6 || _WITH_INTERFACE
/* check the systems interfaces for ifname and return its index
   or -1 if no interface with this name was found
   The system calls require an arbitrary socket; the calling program may
   provide one in anysock to avoid creation of a dummy socket. anysock must be
   <0 if it does not specify a socket fd.
 */
int ifindexbyname(const char *ifname, int anysock) {
   /* Linux: man 7 netdevice */
   /* FreeBSD: man 4 networking */
   /* Solaris: man 7 if_tcp */

#if defined(HAVE_STRUCT_IFREQ) && defined(SIOCGIFCONF) && defined(SIOCGIFINDEX)
   /* currently we support Linux, FreeBSD; not Solaris */

#define IFBUFSIZ 32*sizeof(struct ifreq) /*1024*/
   int s;
   struct ifreq ifr;

   if (ifname[0] == '\0') {
      return -1;
   }
   if (anysock >= 0) {
      s = anysock;
   } else  if ((s = Socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
      Error1("socket(PF_INET, SOCK_DGRAM, 0): %s", strerror(errno));
      return -1;
   }

   strncpy(ifr.ifr_name, ifname, IFNAMSIZ);	/* ok */
   if (Ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
      Info3("ioctl(%d, SIOCGIFINDEX, {\"%s\"}): %s",
	     s, ifr.ifr_name, strerror(errno));
      Close(s);
      return -1;
   }
   Close(s);
#if HAVE_STRUCT_IFREQ_IFR_INDEX
   Info3("ioctl(%d, SIOCGIFINDEX, {\"%s\"}) -> { %d }",
         s, ifname, ifr.ifr_index);
   return ifr.ifr_index;
#elif HAVE_STRUCT_IFREQ_IFR_IFINDEX
   Info3("ioctl(%d, SIOCGIFINDEX, {\"%s\"}) -> { %d }",
         s, ifname, ifr.ifr_ifindex);
   return ifr.ifr_ifindex;
#endif /* HAVE_STRUCT_IFREQ_IFR_IFINDEX */

#else /* !defined(HAVE_ STRUCT_IFREQ) && defined(SIOCGIFCONF) && defined(SIOCGIFINDEX) */
   return -1;
#endif /* !defined(HAVE_ STRUCT_IFREQ) && defined(SIOCGIFCONF) && defined(SIOCGIFINDEX) */
}
#endif /* WITH_IP4 || WITH_IP6 || _WITH_INTERFACE */


#if WITH_IP4 || WITH_IP6 || _WITH_INTERFACE
/* like ifindexbyname(), but also allows the index number as input - in this
   case it does not lookup the index.
   writes the resulting index to *ifindex and returns 0,
   or returns -1 on error */
int ifindex(const char *ifname, unsigned int *ifindex, int anysock) {
   char *endptr;
   long int val;

   if (ifname[0] == '\0') {
      return -1;
   }
   val = strtol(ifname, &endptr, 0);
   if (endptr[0] == '\0') {
      *ifindex = val;
      return 0;
   }

   if ((val = ifindexbyname(ifname, anysock)) < 0) {
      return -1;
   }
   *ifindex = val;
   return 0;
}
#endif /* WITH_IP4 || WITH_IP6 || _WITH_INTERFACE */


int _xiosetenv(const char *envname, const char *value, int overwrite, const char *sep) {
   char *oldval;
   char *newval;
   if (overwrite >= 2 && (oldval = getenv(envname)) != NULL) {
      size_t newlen = strlen(oldval)+strlen(sep)+strlen(value)+1;
      if ((newval = Malloc(newlen+1)) == NULL) {
	 return -1;
      }
      snprintf(newval, newlen+1, "%s%s%s", oldval, sep, value);
   } else {
      newval = (char *)value;
   }
   if (Setenv(envname, newval, overwrite) < 0) {
      Warn3("setenv(\"%s\", \"%s\", 1): %s",
	    envname, value, strerror(errno));
#if HAVE_UNSETENV
      Unsetenv(envname);      /* dont want to have a wrong value */
#endif
      return -1;
   }
   return 0;
}

/* constructs an environment variable whose name is built from socats uppercase
   program name, and underscore and varname;
   if the variable of this name already exists arg overwrite determines:
   0: keep old value
   1: overwrite with new value
   2: append to old value, separated by *sep
   returns 0 on success or <0 if an error occurred. */
int xiosetenv(const char *varname, const char *value, int overwrite, const char *sep) {
#  define XIO_ENVNAMELEN 256
   const char *progname;
   char envname[XIO_ENVNAMELEN];
   size_t i, l;

   progname = diag_get_string('p');
   envname[0] = '\0'; strncat(envname, progname, XIO_ENVNAMELEN-1);
   l = strlen(envname);
   for (i = 0; i < l; ++i)  envname[i] = toupper((unsigned char)envname[i]);
   strncat(envname+l, "_", XIO_ENVNAMELEN-l-1);
   l += 1;
   strncat(envname+l, varname, XIO_ENVNAMELEN-l-1);
   return _xiosetenv(envname, value, overwrite, sep);
#  undef XIO_ENVNAMELEN
}

int xiosetenv2(const char *varname, const char *varname2, const char *value,
	       int overwrite, const char *sep) {
#  define XIO_ENVNAMELEN 256
   const char *progname;
   char envname[XIO_ENVNAMELEN];
   size_t i, l;

   progname = diag_get_string('p');
   envname[0] = '\0'; strncat(envname, progname, XIO_ENVNAMELEN-1);
   l = strlen(progname);
   strncat(envname+l, "_", XIO_ENVNAMELEN-l-1);
   l += 1;
   strncat(envname+l, varname, XIO_ENVNAMELEN-l-1);
   l += strlen(envname+l);
   strncat(envname+l, "_", XIO_ENVNAMELEN-l-1);
   l += 1;
   strncat(envname+l, varname2, XIO_ENVNAMELEN-l-1);
   l += strlen(envname+l);
   for (i = 0; i < l; ++i)  envname[i] = toupper((unsigned char)envname[i]);
   return _xiosetenv(envname, value, overwrite, sep);
#  undef XIO_ENVNAMELEN
}

int xiosetenv3(const char *varname, const char *varname2, const char *varname3,
	       const char *value,
	       int overwrite, const char *sep) {
#  define XIO_ENVNAMELEN 256
   const char *progname;
   char envname[XIO_ENVNAMELEN];
   size_t i, l;

   progname = diag_get_string('p');
   envname[0] = '\0'; strncat(envname, progname, XIO_ENVNAMELEN-1);
   l = strlen(progname);
   strncat(envname+l, "_", XIO_ENVNAMELEN-l-1);
   l += 1;
   strncat(envname+l, varname, XIO_ENVNAMELEN-l-1);
   l += strlen(envname+l);
   strncat(envname+l, "_", XIO_ENVNAMELEN-l-1);
   l += 1;
   strncat(envname+l, varname2, XIO_ENVNAMELEN-l-1);
   l += strlen(envname+l);
   strncat(envname+l, "_", XIO_ENVNAMELEN-l-1);
   l += 1;
   strncat(envname+l, varname3, XIO_ENVNAMELEN-l-1);
   l += strlen(envname+l);
   for (i = 0; i < l; ++i)  envname[i] = toupper((unsigned char)envname[i]);
   return _xiosetenv(envname, value, overwrite, sep);
#  undef XIO_ENVNAMELEN
}


/* like xiosetenv(), but uses an unsigned long value */
int xiosetenvulong(const char *varname, unsigned long value, int overwrite) {
#  define XIO_LONGLEN 21	/* should suffice for 64bit longs with \0 */
   char envbuff[XIO_LONGLEN];

   snprintf(envbuff, XIO_LONGLEN, "%lu", value);
   return xiosetenv(varname, envbuff, overwrite, NULL);
#  undef XIO_LONGLEN
}

/* like xiosetenv(), but uses an unsigned short value */
int xiosetenvushort(const char *varname, unsigned short value, int overwrite) {
#  define XIO_SHORTLEN 11      /* should suffice for 32bit shorts with \0 */
   char envbuff[XIO_SHORTLEN];

   snprintf(envbuff, XIO_SHORTLEN, "%hu", value);
   return xiosetenv(varname, envbuff, overwrite, NULL);
#  undef XIO_SHORTLEN
}


unsigned long int Strtoul(const char *nptr, char **endptr, int base, const char *txt) {
   unsigned long res;

   res = strtoul(nptr, endptr, base);
   if (nptr == *endptr) {
      Error1("parseopts(): missing numerical value of option \"%s\"", txt);
   }
   if (**endptr != '\0') {
      Error1("parseopts(): trailing garbage in numerical arg of option \"%s\"", txt);
   }
   return res;
}

#if HAVE_STRTOLL
long long int Strtoll(const char *nptr, char **endptr, int base, const char *txt) {
   long long int res;

   res = strtoul(nptr, endptr, base);
   if (nptr == *endptr) {
      Error1("parseopts(): missing numerical value of option \"%s\"", txt);
   }
   if (**endptr != '\0') {
      Error1("parseopts(): trailing garbage in numerical arg of option \"%s\"", txt);
   }
   return res;
}
#endif /* HAVE_STRTOLL */

double Strtod(const char *nptr, char **endptr, const char *txt) {
   double res;

   res = strtod(nptr, endptr);
   if (nptr == *endptr) {
      Error1("parseopts(): missing numerical value of option \"%s\"", txt);
   }
   if (**endptr != '\0') {
      Error1("parseopts(): trailing garbage in numerical arg of option \"%s\"", txt);
   }
   return res;
}


/* This function gets a string with possible references to environment
   variables and expands them. Variables must be prefixed with '$' and their
   names consist only of A-Z, a-z, 0-9, and '_'.
   The name may be enclosed with { }.
   To pass a literal "$" us "\$".
   There are special variables supported whose values do not comr from
   environment:
   $$ expands to the process's pid
   $PROGNAME expands to the executables basename or the value of option -lp
   $TIMESTAMP expands to the actual time in format %Y%m%dT%H%M%S
   $MICROS expands to the actual microseconds (decimal)
   The dst output is a copy of src but with the variables expanded.
   Returns 0 on success;
   returns -1 when the output buffer was too short (overflow);
   returns 1 on syntax error.
*/
int expandenv(
	char *dst, 		/* prealloc'd output buff, will be \0 termd */
	const char *src, 	/* input string to generate expansion from */
	size_t n, 		/* length of dst */
	struct timeval *tv) 	/* in/out timestamp for sync */
{
	char c; 		/* char currently being lex'd/parsed */
	bool esc = false;	/* just got '\' */
	bool bra = false; 	/* within ${ } */
	char *nam = NULL; 	/* points to temp.allocated rw copy of src */
	const char *e;		/* pointer to isolated var name in tmp[] */
	size_t s=0, d=0; 	/* counters in src, dst */
	size_t v; 		/* variable name begin */
	bool ofl = false;	/* dst overflow, output truncated */
	char tmp[18];		/* buffer for timestamp, micros */

	while (c = src[s++]) {
		if (esc) {
			if (c == '\0') {
				if (d+2 > n)  { ofl = true; break; }
				dst[d++] = '\\';
				dst[d++] = c;
				break;
			}
			if (c != '$') {
				if (d+3 > n)  { ofl = true; break; }
				dst[d++] = '\\';
			} else {
				if (d+2 > n)  { ofl = true; break; }
			}
			dst[d++] = c;
			esc = false;
			continue;
		}
		if (c == '\0') {
			dst[d++] = c;
			break;
		}
		if (c == '\\') {
			esc = true;
			continue;
		}
		if (c != '$') {
			if (d+2 > n)  { ofl = true; break; }
			dst[d++] = c;
			continue;
		}

		/* c == '$': Expecting env var to expand */
		c = src[s];
		if (c == '\0') {
			if (d+2 > n)  { ofl = true; break; }
			++s;
			dst[d++] = '$';
			break;
		}
		if (c == '$') {
			int wr;
			/* Special case: pid */
			++s;
			wr = snprintf(&dst[d], n-d, F_pid, getpid());
			if (wr >= n-d || wr < 0)  { ofl = true; break; }
			d += wr;
			continue;
		}
		if (c == '{') {
			++s;
			bra = true;
		}
		if (!isalpha(c) && c != '_') {
			/* Special case no var name, just keep '$' */
			if (d+3 > n)  { ofl = true; break; }
			++s;
			dst[d++] = '$';
			dst[d++] = c;
			continue;
		}
		v = 0; 		/* seems we found valid variable name */
		if (nam == NULL) {
			nam = strdup(src+s);
			if (nam == NULL) {
				errno = ENOMEM;
				return -1;
			}
		}
		while (c = src[s]) {
			if (!isalnum(c) && c != '_') {
				break;
			}
			nam[v++] = c;
			++s;
		}
		if (bra && c != '}') {
			return 1;
		}

		nam[v] = '\0';
		/* Var name is complete */
		/* Check hardcoded var names */
		if (strcmp(nam, "PROGNAME") == 0) {
			e = diag_get_string('p');
		} else if (strcmp(nam, "TIMESTAMP") == 0) {
			if (tv->tv_sec == 0) {
				gettimeofday(tv, NULL);
			}
			struct tm tm;
			localtime_r(&tv->tv_sec, &tm);
			strftime(tmp, sizeof(tmp), "%Y%m%dT%H%M%S", &tm);
			e = tmp;
		} else if (strcmp(nam, "MICROS") == 0) {
			if (tv->tv_sec == 0) {
				gettimeofday(tv, NULL);
			}
			sprintf(tmp, F_tv_usec, tv->tv_usec);
			e = tmp;
		} else {
			e = getenv(nam);
		}
		if (e == NULL) {
			/* Var not found, skip it */
			continue;
		}
		/* Var found, copy it to output buffer */
		if (d+strlen(e)+1 > n)  { ofl = true; break; }
		strcpy(&dst[d], e);
		d += strlen(&dst[d]);
		continue;
	}

	if (nam != NULL) {
		free(nam);
	}

	if (ofl) {
		dst[d] = '\0';
		return -1;
	}
	dst[d] = '\0';
	if (src[s-1] != '\0') {
		errno = EINVAL;
		return -1;
	}
	return 0;
}

int xio_opensnifffile(
	const char *a,
	struct timeval *tv)
{
	char path[PATH_MAX];
	int flags;
	int rc;
	int fd;

	rc = expandenv(path, a, sizeof(path), tv);
	if (rc < 0) {
		Error2("expandenv(source=\"%s\", n="F_Zu"): Out of memory", a, sizeof(path));
		errno = ENOMEM;
		return -1;
	} else if (rc > 0) {
		Error1("expandenv(source=\"%s\"): Syntax error", a);
		errno = EINVAL;
		return -1;
	}
	flags = O_CREAT|O_WRONLY|O_APPEND|
#ifdef O_LARGEFILE
		O_LARGEFILE|
#endif
#ifdef O_CLOEXEC
		O_CLOEXEC|
#endif
		O_NONBLOCK;
	if ((fd = Open(path, flags, 0664)) < 0) {
		if (errno == ENXIO) {
			/* try to open pipe rdwr */
			if ((fd = Open(path, flags, 0664)) < 0) {
				return -1;
			}
		}
	}
#ifdef O_CLOEXEC
	if (Fcntl_l(fd, F_SETFD, FD_CLOEXEC) < 0) {
		Warn2("fcntl(%d, F_SETFD, FD_CLOEXEC): %s", fd, strerror(errno));
	}
#endif
	return fd;
}
