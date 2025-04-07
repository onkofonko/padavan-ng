/* source: xio-ipapp.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for TCP and UDP related options */

#include "xiosysincludes.h"

#if WITH_TCP || WITH_UDP || WITH_SCTP || WITH_DCCP || WITH_UDPLITE

#include "xioopen.h"
#include "xio-socket.h"
#include "xio-ip.h"
#include "xio-listen.h"
#include "xio-ip6.h"
#include "xio-ipapp.h"

const struct optdesc opt_sourceport = { "sourceport", "sp",       OPT_SOURCEPORT,  GROUP_IPAPP,     PH_LATE,TYPE_2BYTE,	OFUNC_SPEC };
/*const struct optdesc opt_port = { "port",  NULL,    OPT_PORT,        GROUP_IPAPP, PH_BIND,    TYPE_USHORT,	OFUNC_SPEC };*/
const struct optdesc opt_lowport = { "lowport", NULL, OPT_LOWPORT, GROUP_IPAPP, PH_LATE, TYPE_BOOL, OFUNC_SPEC };


#if _WITH_IP4 || _WITH_IP6
/* we expect the form "host:port" */
int xioopen_ipapp_connect(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xxfd->stream;
   struct opt *opts0 = NULL;
   const char *hostname = argv[1], *portname = argv[2];
   int pf = addrdesc->arg3;
   int socktype = addrdesc->arg1;
   int ipproto = addrdesc->arg2;
   bool dofork = false;
   int maxchildren = 0;
   struct addrinfo **bindarr = NULL;
   struct addrinfo **themarr = NULL;
   uint16_t bindport = 0;
   bool needbind = false;
   bool lowport = false;
   int level = E_ERROR;
   int result;

   if (argc != 3) {
      xio_syntax(argv[0], 2, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   /* Apply and retrieve some options */
   result = _xioopen_ipapp_init(sfd, xioflags, opts,
				&dofork, &maxchildren,
				&pf, &socktype, &ipproto);
   if (result != STAT_OK)
      return result;

   opts0 = opts; 	/* save remaining options for each loop */
   opts = NULL;

   Notice2("opening connection to %s:%s", hostname, portname);

   do {	/* loop over retries and/or forks */
      int _errno;

#if WITH_RETRY
      if (sfd->forever || sfd->retry) {
	 level = E_NOTICE;
      } else
#endif /* WITH_RETRY */
	 level = E_WARN;

      opts = copyopts(opts0, GROUP_ALL);

      result =
	 _xioopen_ipapp_prepare(&opts, opts0, hostname, portname,
				pf, socktype, ipproto,
				sfd->para.socket.ip.ai_flags,
				&themarr, &bindarr, &bindport, &needbind, &lowport);
      switch (result) {
      case STAT_OK: break;
#if WITH_RETRY
      case STAT_RETRYLATER:
      case STAT_RETRYNOW:
	 if (sfd->forever || sfd->retry--) {
	    if (result == STAT_RETRYLATER)
	       Nanosleep(&sfd->intervall, NULL);
	    if (bindarr != NULL)  xiofreeaddrinfo(bindarr);
	    xiofreeaddrinfo(themarr);
	    freeopts(opts);
	    continue;
	 }
#endif /* WITH_RETRY */
	 /* FALLTHROUGH */
      case STAT_NORETRY:
	 if (bindarr != NULL)  xiofreeaddrinfo(bindarr);
	 xiofreeaddrinfo(themarr);
	 freeopts(opts);
	 freeopts(opts0);
	 return result;
      }

      result =
	 _xioopen_ipapp_connect(sfd, hostname, opts, themarr,
				needbind, bindarr, bindport, lowport, level);
      _errno = errno;
      if (bindarr != NULL)
	 xiofreeaddrinfo(bindarr);
      xiofreeaddrinfo(themarr);
      switch (result) {
      case STAT_OK: break;
#if WITH_RETRY
      case STAT_RETRYLATER:
      case STAT_RETRYNOW:
	 if (sfd->forever || sfd->retry--) {
	    if (result == STAT_RETRYLATER) {
	       Nanosleep(&sfd->intervall, NULL);
	    }
	    freeopts(opts);
	    continue;
	 }
#endif /* WITH_RETRY */
	 /* FALLTHROUGH */
      default:
	 Error4("%s:%s:%s: %s", argv[0], argv[1], argv[2],
		_errno?strerror(_errno):"(See above)");
	 freeopts(opts);
	 freeopts(opts0);
	 return result;
      }

#if WITH_RETRY
      if (dofork) {
	 pid_t pid;
	 int level = E_ERROR;
	 if (sfd->forever || sfd->retry) {
	    level = E_WARN;	/* most users won't expect a problem here,
				   so Notice is too weak */
	 }
	 while ((pid = xio_fork(false, level, sfd->shutup)) < 0) {
	    if (sfd->forever || sfd->retry--) {
	       Nanosleep(&sfd->intervall, NULL);
	       continue;
	    }
	    freeopts(opts);
	    freeopts(opts0);
	    return STAT_RETRYLATER;
	 }

	 if (pid == 0) {	/* child process */
	    sfd->forever = false;
	    sfd->retry = 0;
	    break;
	 }

	 /* parent process */
	 Close(sfd->fd);
	 /* with and without retry */
	 Nanosleep(&sfd->intervall, NULL);
	 while (maxchildren > 0 && num_child >= maxchildren) {
	    Info1("all %d allowed children are active, waiting", maxchildren);
	    Nanosleep(&sfd->intervall, NULL);
	 }
	 freeopts(opts);
	 continue;	/* with next socket() bind() connect() */
      } else
#endif /* WITH_RETRY */
      {
	 break;
      }
   } while (true); 	/* end of loop over retries and/or forks */
   /* only "active" process breaks (master without fork, or child) */

   Notice2("successfully connected to %s:%s", hostname, portname);

   result = _xio_openlate(sfd, opts);
   freeopts(opts);
   freeopts(opts0);
   return result;
}


/* This function performs static initializations for addresses like TCP-CONNECT
   before start of the outer loop:
   it retrieves some options
   returns STAT_OK on success or some other value on failure;
   applies and consumes the following options:
   PH_INIT, OPT_FORK, OPT_MAX_CHILDREN, OPT_PROTOCOL_FAMILY, OPT_SO_TYPE,
   OPT_SO_PROTOTYPE
*/
int _xioopen_ipapp_init(
	struct single *sfd,
	int xioflags,
	struct opt *opts,
	bool *dofork,
	int *maxchildren,
	int *pf,
	int *socktype,
	int *ipproto)
{
   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;

   if (applyopts_single(sfd, opts, PH_INIT) < 0)
      return -1;
   if (applyopts(sfd, -1, opts, PH_INIT) < 0)
      return -1;

   retropt_bool(opts, OPT_FORK, dofork);
   if (dofork) {
      if (!(xioflags & XIO_MAYFORK)) {
	 Error1("%s: option fork not allowed here", sfd->addr->defname);
	 return STAT_NORETRY;
      }
      sfd->flags |= XIO_DOESFORK;
   }

   retropt_int(opts, OPT_MAX_CHILDREN, maxchildren);
   if (! dofork && maxchildren) {
      Error1("%s: option max-children not allowed without option fork", sfd->addr->defname);
      return STAT_NORETRY;
   }

   retropt_socket_pf(opts, pf);
   retropt_int(opts, OPT_SO_TYPE, socktype);
   retropt_int(opts, OPT_SO_PROTOTYPE, ipproto);

   if (dofork) {
      xiosetchilddied();	/* set SIGCHLD handler */
   }

   if (xioparms.logopt == 'm') {
      Info("starting connect loop, switching to syslog");
      diag_set('y', xioparms.syslogfac);
      xioparms.logopt = 'y';
   } else {
      Info("starting connect loop");
   }

   return STAT_OK;
}


/* This function performs preparations for addresses like TCP-CONNECT
   at the beginning of the outer (retry/fork) loop:
   it evaluates some options and performs name resolution of both server
   (target, "them") address and bind ("us") address.
   It is intended to be invoked before the connect loop starts;
   returns STAT_OK on success or some other value on failure;
   applies and consumes the following options:
   PH_EARLY
   OPT_BIND, OPT_SOURCEPORT, OPT_LOWPORT
   returns STAT_OK, STAT_RETRYLATER, or STAT_NORETRY (+errno)
*/
int _xioopen_ipapp_prepare(
	struct opt **opts,
	struct opt *opts0,
	const char *hostname,
	const char *portname,
	int pf,
	int socktype,
	int protocol,
	const int ai_flags[2],
	struct addrinfo ***themarr, 	/* always from getaddrinfo(); xiofreeaddrinfo()! */
	struct addrinfo ***bindarr, 	/* on bind from getaddrinfo(); xiofreeaddrinfo()! */
	uint16_t *bindport, 		/* for bind without address */
	bool *needbind,
	bool *lowport)
{
   uint16_t port;
   int rc;

   *opts = copyopts(opts0, GROUP_ALL);

   if (hostname != NULL || portname != NULL) {
      rc = xiogetaddrinfo(hostname, portname, pf, socktype, protocol,
			  themarr, ai_flags);
      if (rc == EAI_AGAIN) {
	 Warn4("_xioopen_ipapp_prepare(node=\"%s\", service=\"%s\", pf=%d, ...): %s",
	       hostname?hostname:"NULL", portname?portname:"NULL",
	       pf, gai_strerror(rc));
	 errno = EAGAIN;
	 return STAT_RETRYLATER;
      } else if (rc != 0) {
	 Error4("_xioopen_ipapp_prepare(node=\"%s\", service=\"%s\", pf=%d, ...): %s",
		hostname?hostname:"NULL", portname?portname:"NULL",
		pf, (rc == EAI_SYSTEM)?strerror(errno):gai_strerror(rc));
	 errno = 0; 	/* unspecified */
	 return STAT_NORETRY;	/*! STAT_RETRYLATER? */
      }
   }

   applyopts(NULL, -1, *opts, PH_EARLY);

   /* 3 means: IP address AND port accepted */
   if (retropt_bind_ip(*opts, pf, socktype, protocol, bindarr, 3, ai_flags)
       != STAT_NOACTION) {
      *needbind = true;
   }
   if (retropt_2bytes(*opts, OPT_SOURCEPORT, &port) >= 0) {
      if (*bindarr) {
	 struct addrinfo **bindp;
	 bindp = *bindarr;
	 switch ((*bindp)->ai_family) {
#if WITH_IP4
	 case PF_INET:  ((struct sockaddr_in *)(*bindp)->ai_addr)->sin_port = htons(port); break;
#endif /* WITH_IP4 */
#if WITH_IP6
	 case PF_INET6: ((struct sockaddr_in6 *)(*bindp)->ai_addr)->sin6_port = htons(port); break;
#endif /* WITH_IP6 */
	 default:
	    Error("unsupported protocol family");
	    errno = EPROTONOSUPPORT;
	    return STAT_NORETRY;
	 }
      } else {
	 *bindport = port;
      }
      *needbind = true;
   }

   retropt_bool(*opts, OPT_LOWPORT, lowport);

   return STAT_OK;
}

#endif /* _WITH_IP4 || _WITH_IP6 */


/* Tries to connect to the addresses in themarr, for each one it tries to bind
   to the addresses in bindarr.
   Ends on success or when all attempts failed.
   Returns STAT_OK on success, or STAT_RETRYLATER (+errno) on failure. */
int _xioopen_ipapp_connect(struct single *sfd,
			   const char *hostname,
			   struct opt *opts,
			   struct addrinfo **themarr,
			   bool needbind,
			   struct addrinfo **bindarr,
			   uint16_t bindport,
			   bool lowport,
			   int level)
{
   struct addrinfo **themp;
   struct addrinfo **bindp;
   union sockaddr_union bindaddr = {0};
   union sockaddr_union *bindaddrp = NULL;
   socklen_t bindlen = 0;
   char infobuff[256];
   int _errno;
   int result = STAT_OK;

   --level;

   /* Loop over server addresses (themarr) */
   themp = themarr;
   while (*themp != NULL) {
      Notice1("opening connection to %s",
	      sockaddr_info((*themp)->ai_addr, (*themp)->ai_addrlen,
			    infobuff, sizeof(infobuff)));

      if (*(themp+1) == NULL) {
	 ++level; 	/* last attempt */
      }

      /* Loop over array (list) of bind addresses */
      if (needbind && bindarr != NULL) {
	 /* Bind by hostname, use resolvers results list */
	 bindp = bindarr;
	 while (*bindp != NULL) {
	    if ((*bindp)->ai_family == (*themp)->ai_family)
	       break;
	    ++bindp;
	 }
	 if (*bindp == NULL) {
	    Warn3("%s: No bind address with matching address family (%d) of %s available",
		  sfd->addr->defname, (*themp)->ai_family, hostname);
	    ++themp;
	    if ((*themp) == NULL) {
	       result = STAT_RETRYLATER;
	    }
	    _errno = ENOPROTOOPT;
	    continue;
	 }
	 bindaddrp = (union sockaddr_union *)(*bindp)->ai_addr;
	 bindlen  = (*bindp)->ai_addrlen;
      } else if (needbind && bindport) {
	 /* Bind by sourceport option */
	 switch ((*themp)->ai_family) {
#if WITH_IP4
	 case PF_INET:
	    bindaddr.ip4.sin_family = (*themp)->ai_family;
	    bindaddr.ip4.sin_port = htons(bindport);
	    bindaddrp = &bindaddr;
	    bindlen = sizeof(bindaddr.ip4);
	    break;
#endif
#if WITH_IP6
	 case PF_INET6:
	    bindaddr.ip6.sin6_family = (*themp)->ai_family;
	    bindaddr.ip6.sin6_port = htons(bindport);
	    bindaddrp = &bindaddr;
	    bindlen = sizeof(bindaddr.ip6);
	    break;
#endif
	 }
      }

      result =
	 _xioopen_connect(sfd,
			  bindaddrp, bindlen,
			  (*themp)->ai_addr, (*themp)->ai_addrlen,
			  opts, /*pf?pf:*/(*themp)->ai_family, (*themp)->ai_socktype, (*themp)->ai_protocol,
			  lowport, level);
      if (result == STAT_OK)
	 break;
      _errno = errno;
      ++themp;
      if (*themp == NULL)
	 result = STAT_RETRYLATER;
   } 	/* end of loop over target addresses */

   if (result != STAT_OK)
      errno = _errno;
   return result;
}


#if WITH_TCP && WITH_LISTEN
/*
   applies and consumes the following options:
   OPT_PROTOCOL_FAMILY, OPT_BIND
 */
int _xioopen_ipapp_listen_prepare(
	struct opt *opts,
	struct opt **opts0,
	const char *portname,
	int *pf,
	int ipproto,
	const int ai_flags[2],
	union sockaddr_union *us,
	socklen_t *uslen,
	int socktype)
{
   char *bindname = NULL;
   int ai_flags2[2];
   int result;

   retropt_socket_pf(opts, pf);

   retropt_string(opts, OPT_BIND, &bindname);

   /* Set AI_PASSIVE, except when it is explicitly disabled */
   ai_flags2[0] = ai_flags[0];
   ai_flags2[1] = ai_flags[1];
   if (!(ai_flags2[1] & AI_PASSIVE))
      ai_flags2[0] |= AI_PASSIVE;

   result =
	xioresolve(bindname, portname, *pf, socktype, ipproto,
		   us, uslen, ai_flags2);
   if (result != STAT_OK) {
      /*! STAT_RETRY? */
      return result;
   }
   *opts0 = copyopts(opts, GROUP_ALL);
   return STAT_OK;
}


/* we expect the form: port */
/* currently only used for TCP4 */
int xioopen_ipapp_listen(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xfd->stream;
   struct opt *opts0 = NULL;
   int socktype = addrdesc->arg1;
   int ipproto = addrdesc->arg2;
   int pf = addrdesc->arg3;
   union sockaddr_union us_sa, *us = &us_sa;
   socklen_t uslen = sizeof(us_sa);
   int result;

   if (argc != 2) {
      xio_syntax(argv[0], 2, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   xioinit_ip(&pf, xioparms.default_ip);
   if (pf == PF_UNSPEC) {
#if WITH_IP4 && WITH_IP6
      switch (xioparms.default_ip) {
      case '4': pf = PF_INET; break;
      case '6': pf = PF_INET6; break;
      default: break;		/* includes \0 */
      }
#elif WITH_IP6
      pf = PF_INET6;
#else
      pf = PF_INET;
#endif
   }

   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;

   if (applyopts_single(sfd, opts, PH_INIT) < 0)  return -1;
   applyopts(sfd, -1, opts, PH_INIT);
   applyopts(sfd, -1, opts, PH_EARLY);

   if (_xioopen_ipapp_listen_prepare(opts, &opts0, argv[1], &pf, ipproto,
				     sfd->para.socket.ip.ai_flags,
				     us, &uslen, socktype)
       != STAT_OK) {
      return STAT_NORETRY;
   }

   if ((result =
	xioopen_listen(sfd, xioflags,
		       (struct sockaddr *)us, uslen,
		       opts, opts0, pf, socktype, ipproto))
       != 0)
      return result;
   return 0;
}
#endif /* WITH_TCP && WITH_LISTEN */

#endif /* WITH_TCP || WITH_UDP || WITH_SCTP || WITH_DCCP || WITH_UDPLITE */
