/* source: xio-ipapp.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for TCP and UDP related options */

#include "xiosysincludes.h"

#if WITH_TCP || WITH_UDP

#include "xioopen.h"
#include "xio-socket.h"
#include "xio-ip.h"
#include "xio-listen.h"
#include "xio-ip6.h"
#include "xio-ipapp.h"

const struct optdesc opt_sourceport = { "sourceport", "sp",       OPT_SOURCEPORT,  GROUP_IPAPP,     PH_LATE,TYPE_2BYTE,	OFUNC_SPEC };
/*const struct optdesc opt_port = { "port",  NULL,    OPT_PORT,        GROUP_IPAPP, PH_BIND,    TYPE_USHORT,	OFUNC_SPEC };*/
const struct optdesc opt_lowport = { "lowport", NULL, OPT_LOWPORT, GROUP_IPAPP, PH_LATE, TYPE_BOOL, OFUNC_SPEC };

#if WITH_IP4
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
   int socktype = addrdesc->arg1;
   int ipproto = addrdesc->arg2;
   int pf = addrdesc->arg3;
   const char *hostname = argv[1], *portname = argv[2];
   bool dofork = false;
   int maxchildren = 0;
   union sockaddr_union us_sa,  *us = &us_sa;
   socklen_t uslen = sizeof(us_sa);
   struct addrinfo *themlist, *themp;
   char infobuff[256];
   bool needbind = false;
   bool lowport = false;
   int level;
   struct addrinfo **ai_sorted;
   int i;
   int result;

   if (argc != 3) {
      xio_syntax(argv[0], 2, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   xioinit_ip(&pf, xioparms.default_ip);
   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;

   if (applyopts_single(sfd, opts, PH_INIT) < 0)
      return -1;
   applyopts(sfd, -1, opts, PH_INIT);

   retropt_bool(opts, OPT_FORK, &dofork);
   if (dofork) {
      if (!(xioflags & XIO_MAYFORK)) {
	 Error("option fork not allowed here");
	 return STAT_NORETRY;
      }
      sfd->flags |= XIO_DOESFORK;
   }

   retropt_int(opts, OPT_MAX_CHILDREN, &maxchildren);

   if (! dofork && maxchildren) {
       Error("option max-children not allowed without option fork");
       return STAT_NORETRY;
   }

   if (_xioopen_ipapp_prepare(opts, &opts0, hostname, portname, &pf, ipproto,
			      sfd->para.socket.ip.ai_flags,
			      &themlist, us, &uslen, &needbind, &lowport,
			      socktype) != STAT_OK) {
      return STAT_NORETRY;
   }

   if (dofork) {
      xiosetchilddied();	/* set SIGCHLD handler */
   }

   if (xioparms.logopt == 'm') {
      Info("starting connect loop, switching to syslog");
      diag_set('y', xioparms.syslogfac);  xioparms.logopt = 'y';
   } else {
      Info("starting connect loop");
   }

   /* Count addrinfo entries */
   themp = themlist;
   i = 0;
   while (themp != NULL) {
      ++i;
      themp = themp->ai_next;
   }
   ai_sorted = Calloc((i+1), sizeof(struct addrinfo *));
   if (ai_sorted == NULL)
      return STAT_RETRYLATER;
   /* Generate a list of addresses sorted by preferred ip version */
   _xio_sort_ip_addresses(themlist, ai_sorted);

   do {	/* loop over retries, and forks */

      /* Loop over themlist - no, over ai_sorted */
      result = STAT_RETRYLATER;
      i = 0;
      themp = ai_sorted[i++];
      while (themp != NULL) {
	 Notice1("opening connection to %s",
		 sockaddr_info(themp->ai_addr, themp->ai_addrlen,
			       infobuff, sizeof(infobuff)));

#if WITH_RETRY
	 if (sfd->forever || sfd->retry || ai_sorted[i] != NULL) {
	    level = E_INFO;
         } else
#endif /* WITH_RETRY */
	    level = E_ERROR;

       result =
	 _xioopen_connect(sfd,
			  needbind?us:NULL, uslen,
			  themp->ai_addr, themp->ai_addrlen,
			  opts, pf?pf:themp->ai_family, socktype, ipproto,
			  lowport, level);
       if (result == STAT_OK)
	  break;
       themp = ai_sorted[i++];
       if (themp == NULL) {
	  result = STAT_RETRYLATER;
       }
      }
      switch (result) {
      case STAT_OK: break;
#if WITH_RETRY
      case STAT_RETRYLATER:
      case STAT_RETRYNOW:
	 if (sfd->forever || sfd->retry) {
	    --sfd->retry;
	    if (result == STAT_RETRYLATER) {
	       Nanosleep(&sfd->intervall, NULL);
	    }
	    dropopts(opts, PH_ALL); free(opts); opts = copyopts(opts0, GROUP_ALL);
	    continue;
	 }
#endif /* WITH_RETRY */
      default:
	 free(ai_sorted);
	 free(opts0);free(opts);
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
	    if (sfd->forever || --sfd->retry) {
	       Nanosleep(&sfd->intervall, NULL); continue;
	    }
	    free(ai_sorted);
	    free(opts0);
	    return STAT_RETRYLATER;
	 }

	 if (pid == 0) {	/* child process */
	    sfd->forever = false;  sfd->retry = 0;
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
	 dropopts(opts, PH_ALL); free(opts); opts = copyopts(opts0, GROUP_ALL);
	 continue;	/* with next socket() bind() connect() */
      } else
#endif /* WITH_RETRY */
      {
	 break;
      }
   } while (true);
   /* only "active" process breaks (master without fork, or child) */
   free(ai_sorted);
   xiofreeaddrinfo(themlist);

   if ((result = _xio_openlate(sfd, opts)) < 0) {
	   free(opts0);free(opts);
      return result;
   }
   free(opts0); free(opts);
   return 0;
}


/* returns STAT_OK on success or some other value on failure
   applies and consumes the following options:
   PH_EARLY
   OPT_PROTOCOL_FAMILY, OPT_BIND, OPT_SOURCEPORT, OPT_LOWPORT
 */
int
   _xioopen_ipapp_prepare(
	   struct opt *opts,
	   struct opt **opts0,
	   const char *hostname,
	   const char *portname,
	   int *pf,
	   int protocol,
	   const int ai_flags[2],
	   struct addrinfo **themlist,
	   union sockaddr_union *us,
	   socklen_t *uslen,
	   bool *needbind,
	   bool *lowport,
	   int socktype) {
   uint16_t port;
   int result;

   retropt_socket_pf(opts, pf);

   if (hostname != NULL || portname != NULL) {
    if ((result =
	xiogetaddrinfo(hostname, portname,
		       *pf, socktype, protocol,
		       themlist, ai_flags))
       != STAT_OK) {
      return STAT_NORETRY;	/*! STAT_RETRYLATER? */
    }
   }

   applyopts(NULL, -1, opts, PH_EARLY);

   /* 3 means: IP address AND port accepted */
   if (retropt_bind(opts, (*pf!=PF_UNSPEC)?*pf:(*themlist)->ai_family,
		    socktype, protocol, (struct sockaddr *)us, uslen, 3,
		    ai_flags)
       != STAT_NOACTION) {
      *needbind = true;
   } else {
      switch ((*pf!=PF_UNSPEC)?*pf:(*themlist)->ai_family) {
#if WITH_IP4
      case PF_INET:  socket_in_init(&us->ip4);  *uslen = sizeof(us->ip4); break;
#endif /* WITH_IP4 */
#if WITH_IP6
      case PF_INET6: socket_in6_init(&us->ip6); *uslen = sizeof(us->ip6); break;
#endif /* WITH_IP6 */
      default: Error("unsupported protocol family");
      }
   }

   if (retropt_2bytes(opts, OPT_SOURCEPORT, &port) >= 0) {
      switch ((*pf!=PF_UNSPEC)?*pf:(*themlist)->ai_family) {
#if WITH_IP4
      case PF_INET:  us->ip4.sin_port = htons(port); break;
#endif /* WITH_IP4 */
#if WITH_IP6
      case PF_INET6: us->ip6.sin6_port = htons(port); break;
#endif /* WITH_IP6 */
      default: Error("unsupported protocol family");
      }
      *needbind = true;
   }

   retropt_bool(opts, OPT_LOWPORT, lowport);

   *opts0 = copyopts(opts, GROUP_ALL);

   return STAT_OK;
}
#endif /* WITH_IP4 */


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

   /* Set AI_PASSIVE, except when it is explicitely disabled */
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
#endif /* WITH_IP4 && WITH_TCP && WITH_LISTEN */


/* Sort the records of an addrinfo list themp (as returned by getaddrinfo),
   return the sorted list in the array ai_sorted (takes at most n entries
   including the terminating NULL)
   Returns 0 on success. */
int _xio_sort_ip_addresses(
	struct addrinfo *themlist,
	struct addrinfo **ai_sorted)
{
	struct addrinfo *themp;
	int i;
	int ipv[3];
	int ipi = 0;

	/* Make a simple array of IP version preferences */
	switch (xioparms.preferred_ip) {
	case '0':
		ipv[0] = PF_UNSPEC;
		ipv[1] = -1;
		break;
	case '4':
		ipv[0] = PF_INET;
		ipv[1] = PF_INET6;
		ipv[2] = -1;
		break;
	case '6':
		ipv[0] = PF_INET6;
		ipv[1] = PF_INET;
		ipv[2] = -1;
		break;
	default:
		Error("INTERNAL: undefined preferred_ip value");
		return -1;
	}

	/* Create the sorted list */
	ipi = 0;
	i = 0;
	while (ipv[ipi] >= 0) {
		themp = themlist;
		while (themp != NULL) {
			if (ipv[ipi] == PF_UNSPEC) {
				ai_sorted[i] = themp;
				++i;
			} else if (ipv[ipi] == themp->ai_family) {
				ai_sorted[i] = themp;
				++i;
			}
			themp = themp->ai_next;
		}
		++ipi;
	}
	ai_sorted[i] = NULL;
	return 0;
}

#endif /* WITH_TCP || WITH_UDP */
