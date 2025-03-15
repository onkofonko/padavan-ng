/* source: xio-socks.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of socks4 type */

#include "xiosysincludes.h"

#include "xioopen.h"
#include "xio-ascii.h"
#include "xio-socket.h"
#include "xio-ip.h"
#include "xio-ipapp.h"

#define SOCKSPORT "1080"

#if WITH_SOCKS4 || WITH_SOCKS4A

#include "xio-socks.h"


enum {
   SOCKS_CD_GRANTED = 90,
   SOCKS_CD_FAILED,
   SOCKS_CD_NOIDENT,
   SOCKS_CD_IDENTFAILED
} ;

#define BUFF_LEN (SIZEOF_STRUCT_SOCKS4+512)

static int xioopen_socks4_connect(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *fd, const struct addrdesc *addrdesc);

const struct optdesc opt_socksport = { "socksport", NULL, OPT_SOCKSPORT, GROUP_IP_SOCKS, PH_LATE, TYPE_STRING, OFUNC_SPEC };
const struct optdesc opt_socksuser = { "socksuser", NULL, OPT_SOCKSUSER, GROUP_IP_SOCKS, PH_LATE, TYPE_NAME, OFUNC_SPEC };

const struct addrdesc xioaddr_socks4_connect = { "SOCKS4", 3, xioopen_socks4_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_TCP|GROUP_IP_SOCKS|GROUP_CHILD|GROUP_RETRY, 0, 0, 0 HELP(":<socks-server>:<host>:<port>") };

const struct addrdesc xioaddr_socks4a_connect = { "SOCKS4A", 3, xioopen_socks4_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_TCP|GROUP_IP_SOCKS|GROUP_CHILD|GROUP_RETRY, 1, 0, 0 HELP(":<socks-server>:<host>:<port>") };

static int xioopen_socks4_connect(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   /* we expect the form: host:host:port */
   struct single *sfd = &xxfd->stream;
   int socks4a = addrdesc->arg1;
   struct opt *opts0 = NULL;
   const char *sockdname; char *socksport;
   const char *targetname, *targetport;
   int pf = PF_UNSPEC;
   int ipproto = IPPROTO_TCP;
   bool dofork = false;
   int maxchildren = 0;
   struct addrinfo **bindarr = NULL;
   struct addrinfo **themarr = NULL;
   uint16_t bindport = 0;
   bool needbind = false;
   bool lowport = false;
   unsigned char buff[BUFF_LEN];
   struct socks4 *sockhead = (struct socks4 *)buff;
   size_t buflen = sizeof(buff);
   int socktype = SOCK_STREAM;
   int level;
   int result;

   if (argc != 4) {
      xio_syntax(argv[0], 3, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }
   sockdname = argv[1];
   targetname = argv[2];
   targetport = argv[3];

   /* Apply and retrieve some options */
   result = _xioopen_ipapp_init(sfd, xioflags, opts,
			        &dofork, &maxchildren,
			        &pf, &socktype, &ipproto);
   if (result != STAT_OK)
      return result;

   result = _xioopen_socks4_init(targetport, opts, &socksport, sockhead,
				 &buflen);
   if (result != STAT_OK)
      return result;

   opts0 = opts; 	/* save remaining options for each loop */
   opts = NULL;

   Notice5("opening connection to %s:%u via socks4 server %s:%s as user \"%s\"",
	   targetname, ntohs(sockhead->port),
	   sockdname, socksport, sockhead->userid);

   do {	/* loop over retries (failed connect and socks-request attempts)
	   and/or forks */
      int _errno;

#if WITH_RETRY
      if (sfd->forever || sfd->retry) {
	 level = E_NOTICE;
      } else
#endif /* WITH_RETRY */
	 level = E_WARN;

      opts = copyopts(opts0, GROUP_ALL);

      result =
	 _xioopen_ipapp_prepare(&opts, opts0, sockdname, socksport,
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

      /* we try to resolve the target address _before_ connecting to the socks
	 server: this may avoid unnecessary connects and timeouts */
      result =
	 _xioopen_socks4_prepare(sfd, targetname, socks4a, sockhead,
				 (ssize_t *)&buflen, level);
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
      default:
	 if (bindarr != NULL)  xiofreeaddrinfo(bindarr);
	 xiofreeaddrinfo(themarr);
	 freeopts(opts);
	 freeopts(opts0);
	 return result;
      }

      Notice2("opening connection to sockd %s:%s", sockdname, socksport);
      result =
	 _xioopen_ipapp_connect(sfd, sockdname, opts, themarr,
				needbind, bindarr, bindport, lowport, level);
      _errno = errno;
      if (bindarr != NULL)  xiofreeaddrinfo(bindarr);
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
	 errno = _errno;
	 Error4("%s:%s:...,socksport=%s: %s", argv[0], sockdname, socksport,
		_errno?strerror(_errno):"(See above)");
	 freeopts(opts0);
	 freeopts(opts);
	 return result;
      }

      result = _xioopen_socks4_connect(sfd, sockhead, buflen, level);
      switch (result) {
      case STAT_OK: break;
#if WITH_RETRY
      case STAT_RETRYLATER:
      case STAT_RETRYNOW:
	 if (sfd->forever || sfd->retry--) {
	    if (result == STAT_RETRYLATER)
	       Nanosleep(&sfd->intervall, NULL);
	    freeopts(opts);
	    continue;
	 }
#endif /* WITH_RETRY */
	 /* FALLTHROUGH */
      default:
	 freeopts(opts);
	 freeopts(opts0);
	 return result;
      }

      if (dofork) {
	 xiosetchilddied();	/* set SIGCHLD handler */
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
	       freeopts(opts);
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
	 continue;
      } else
#endif /* WITH_RETRY */
      {
	 break;
      }

   } while (true);	/* end of complete open loop - drop out on success */
   /* only "active" process breaks (master without fork, or child) */

   Notice4("successfully connected to %s:%s via sockd %s:%s",
	   targetname, targetport, sockdname, socksport);

   result = _xio_openlate(sfd, opts);
   freeopts(opts);
   freeopts(opts0);
   return result;
}

#endif /* WITH_SOCKS4 || WITH_SOCKS4A */
#if WITH_SOCKS4 || WITH_SOCKS4A || WITH_SOCKS5

int _xioopen_opt_socksport(
	struct opt *opts,
	char **socksport)
{
   struct servent *se;

   if (retropt_string(opts, OPT_SOCKSPORT, socksport) < 0 &&
       *socksport == NULL) {
      if ((se = getservbyname("socks", "tcp")) != NULL) {
	 Debug1("\"socks/tcp\" resolves to %u", ntohs(se->s_port));
	 if ((*socksport = Malloc(6)) == NULL) {
	    return STAT_NORETRY;
	 }
	 sprintf(*socksport, "%u", ntohs(se->s_port));
      } else {
	 Debug1("cannot resolve service \"socks/tcp\", using %s", SOCKSPORT);
	 if ((*socksport = strdup(SOCKSPORT)) == NULL) {
	    return STAT_NORETRY;
	 }
      }
   }
   return 0;
}

#endif /* WITH_SOCKS4 || WITH_SOCKS4A || WITH_SOCKS5 */
#if WITH_SOCKS4 || WITH_SOCKS4A

int _xioopen_socks4_init(
	const char *targetport,
	struct opt *opts,
	char **socksport,
	struct socks4 *sockhead,
	size_t *headlen)
{
   char *userid;

   /* generate socks header - points to final target */
   sockhead->version = 4;
   sockhead->action  = 1;
   sockhead->port    = parseport(targetport, IPPROTO_TCP);	/* network byte
								   order */
   if (_xioopen_opt_socksport(opts, socksport) < 0) {
      return STAT_NORETRY;
   }

   if (retropt_string(opts, OPT_SOCKSUSER, &userid) < 0) {
      if ((userid = getenv("LOGNAME")) == NULL) {
	 if ((userid = getenv("USER")) == NULL) {
	    userid = "anonymous";
	 }
      }
   }
   sockhead->userid[0] = '\0'; strncat(sockhead->userid, userid, *headlen-SIZEOF_STRUCT_SOCKS4-1);
   *headlen = SIZEOF_STRUCT_SOCKS4+strlen(userid)+1;
   return STAT_OK;
}


/* called within retry/fork loop, before connect() */
int _xioopen_socks4_prepare(
	struct single *sfd,
	const char *hostname,	/* socks target host */
	int socks4a,
	struct socks4 *sockhead,
	ssize_t *headlen,	/* get available space, return used length*/
	int level)
{
   int result;

   if (!socks4a) {
      union sockaddr_union sau;
      socklen_t saulen = sizeof(sau);

      if ((result = xioresolve(hostname, NULL,
			       PF_INET, SOCK_STREAM, IPPROTO_TCP,
			       &sau, &saulen,
			       sfd->para.socket.ip.ai_flags))
	  != STAT_OK) {
	 return result;	/*! STAT_RETRY? */
      }
      memcpy(&sockhead->dest, &sau.ip4.sin_addr, 4);
   }
#if WITH_SOCKS4A
   else {
      /*! noresolve */
      sockhead->dest = htonl(0x00000001);	/* three bytes zero */
   }
#endif /* WITH_SOCKS4A */
#if WITH_SOCKS4A
   if (socks4a) {
      /* SOCKS4A requires us to append the host name to resolve
         after the user name's trailing 0 byte.  */
      char* insert_position = (char*) sockhead + *headlen;

      insert_position[0] = '\0'; strncat(insert_position, hostname, BUFF_LEN-*headlen-1);
      ((char *)sockhead)[BUFF_LEN-1] = 0;
      *headlen += strlen(hostname) + 1;
      if (*headlen > BUFF_LEN) {
	 *headlen = BUFF_LEN;
      }
   }
#endif /* WITH_SOCKS4A */
   return STAT_OK;
}


/* perform socks4 client dialog on existing FD.
   Called within fork/retry loop, after connect() */
int _xioopen_socks4_connect(struct single *sfd,
			    struct socks4 *sockhead,
			    size_t headlen,
			    int level) {
   ssize_t bytes;
   int result;
   unsigned char buff[SIZEOF_STRUCT_SOCKS4];
   struct socks4 *replyhead = (struct socks4 *)buff;
   char *destdomname = NULL;

   /* send socks header (target addr+port, +auth) */
#if WITH_MSGLEVEL <= E_INFO
   if (ntohl(sockhead->dest) <= 0x000000ff) {
      destdomname = strchr(sockhead->userid, '\0')+1;
   }
   Info11("sending socks4%s request VN=%d DC=%d DSTPORT=%d DSTIP=%d.%d.%d.%d USERID=%s%s%s",
	  destdomname?"a":"",
	  sockhead->version, sockhead->action, ntohs(sockhead->port),
	  ((unsigned char *)&sockhead->dest)[0],
	  ((unsigned char *)&sockhead->dest)[1],
	  ((unsigned char *)&sockhead->dest)[2],
	  ((unsigned char *)&sockhead->dest)[3],
	  sockhead->userid,
	  destdomname?" DESTNAME=":"",
	  destdomname?destdomname:"");
#endif /* WITH_MSGLEVEL <= E_INFO */
#if WITH_MSGLEVEL <= E_DEBUG
   {
      char *msgbuff;
      if ((msgbuff = Malloc(3*headlen)) != NULL) {
	 xiohexdump((const unsigned char *)sockhead, headlen, msgbuff);
	 Debug1("sending socks4(a) request data %s", msgbuff);
      }
   }
#endif /* WITH_MSGLEVEL <= E_DEBUG */
   if (writefull(sfd->fd, sockhead, headlen, NULL) < 0) {
      Msg4(level, "write(%d, %p, "F_Zu"): %s",
	   sfd->fd, sockhead, headlen, strerror(errno));
      if (Close(sfd->fd) < 0) {
	 Info2("close(%d): %s", sfd->fd, strerror(errno));
      }
      return STAT_RETRYLATER;	/* retry complete open cycle */
   }

   bytes = 0;
   Info("waiting for socks reply");
   while (bytes >= 0) {	/* loop over answer chunks until complete or error */
      /* receive socks answer */
      do {
	 result = Read(sfd->fd, buff+bytes, SIZEOF_STRUCT_SOCKS4-bytes);
      } while (result < 0 && errno == EINTR);
      if (result < 0) {
	 Msg4(level, "read(%d, %p, "F_Zu"): %s",
	      sfd->fd, buff+bytes, SIZEOF_STRUCT_SOCKS4-bytes,
	      strerror(errno));
	 if (Close(sfd->fd) < 0) {
	    Info2("close(%d): %s", sfd->fd, strerror(errno));
	 }
      }
      if (result == 0) {
	 Msg(level, "read(): EOF during read of socks reply, peer might not be a socks4 server");
	 if (Close(sfd->fd) < 0) {
	    Info2("close(%d): %s", sfd->fd, strerror(errno));
	 }
	 return STAT_RETRYLATER;
      }
#if WITH_MSGLEVEL <= E_DEBUG
      {
	 char msgbuff[3*SIZEOF_STRUCT_SOCKS4];
	 * xiohexdump((const unsigned char *)replyhead+bytes, result, msgbuff)
	    = '\0';
	 Debug2("received socks4 reply data (offset "F_Zd"): %s", bytes, msgbuff);
      }
#endif /* WITH_MSGLEVEL <= E_DEBUG */
      bytes += result;
      if (bytes == SIZEOF_STRUCT_SOCKS4) {
	 Debug1("received all "F_Zd" bytes", bytes);
	 break;
      }
      Debug2("received %d bytes, waiting for "F_Zu" more bytes",
	     result, SIZEOF_STRUCT_SOCKS4-bytes);
   }
   if (result <= 0) {	/* we had a problem while reading socks answer */
      return STAT_RETRYLATER;	/* retry complete open cycle */
   }

   Info7("received socks reply VN=%u CD=%u DSTPORT=%u DSTIP=%u.%u.%u.%u",
	 replyhead->version, replyhead->action, ntohs(replyhead->port),
	 ((uint8_t *)&replyhead->dest)[0],
	 ((uint8_t *)&replyhead->dest)[1],
	 ((uint8_t *)&replyhead->dest)[2],
	 ((uint8_t *)&replyhead->dest)[3]);
   if (replyhead->version != 0) {
      Warn1("socks: reply code version is not 0 (%d)",
	    replyhead->version);
   }

   switch (replyhead->action) {
   case SOCKS_CD_GRANTED:
      /* Notice("socks: connect request succeeded"); */
      Notice("successfully connected via socks4");
      break;

   case SOCKS_CD_FAILED:
      Msg(level, "socks: connect request rejected or failed");
      return STAT_RETRYLATER;

   case SOCKS_CD_NOIDENT:
      Msg(level, "socks: ident refused by client");
      return STAT_RETRYLATER;

   case SOCKS_CD_IDENTFAILED:
      Msg(level, "socks: ident failed");
      return STAT_RETRYLATER;

   default:
      Msg1(level, "socks: undefined status %u", replyhead->action);
   }

   return STAT_OK;
}
#endif /* WITH_SOCKS4 || WITH_SOCKS4A */

