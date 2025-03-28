/* source: xio-openssl.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the implementation of the openssl addresses */

#include "xiosysincludes.h"
#if WITH_OPENSSL	/* make this address configure dependent */
#include <openssl/conf.h>
#include <openssl/x509v3.h>

#include "xioopen.h"

#include "xio-fd.h"
#include "xio-ip.h"
#include "xio-socket.h"	/* _xioopen_connect() */
#include "xio-listen.h"
#include "xio-udp.h"
#include "xio-ipapp.h"
#include "xio-ip6.h"

#include "xio-openssl.h"

/* the openssl library requires a file descriptor for external communications.
   so our best effort is to provide any possible kind of un*x file descriptor
   (not only tcp, but also pipes, stdin, files...)
   for tcp we want to provide support for socks and proxy.
   read and write functions must use the openssl crypt versions.
*/

/* Linux: "man 3 ssl" */

/* generate a simple openssl server for testing:
   1) generate a private key
   openssl genrsa -out server.key 1024
   2) generate a self signed cert
   openssl req -new -key server.key -x509 -days 3653 -out server.crt
      enter fields...
   3) generate the pem file
   cat server.key server.crt >server.pem
   openssl s_server  (listens on 4433/tcp)
 */

/* static declaration of ssl's open function */
static int xioopen_openssl_connect(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *fd, const struct addrdesc *addrdesc);

/* static declaration of ssl's open function */
static int xioopen_openssl_listen(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *fd, const struct addrdesc *addrdesc);

static int openssl_SSL_ERROR_SSL(int level, const char *funcname);
static int openssl_handle_peer_certificate(struct single *sfd,
					   const char *peername,
					   bool opt_ver,
					   int level);
static int xioSSL_set_fd(struct single *sfd, int level);
static int xioSSL_connect(struct single *sfd, const char *opt_commonname, bool opt_ver, int level);
static int openssl_delete_cert_info(void);


/* description record for ssl connect */
const struct addrdesc xioaddr_openssl = {
   "OPENSSL",	/* keyword for selecting this address type in xioopen calls
		   (canonical or main name) */
   3,		/* data flow directions this address supports on API layer:
		   1..read, 2..write, 3..both */
   xioopen_openssl_connect,	/* a function pointer used to "open" these addresses.*/
   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_TCP|GROUP_CHILD|GROUP_OPENSSL|GROUP_RETRY,	/* bitwise OR of address groups this address belongs to.
		   You might have to specify a new group in xioopts.h */
   0,		/* an integer passed to xioopen_openssl; makes it possible to
		   use the xioopen_openssl_connect function for slightly different
		   address types. */
   0,		/* like previous argument */
   0		/* like previous arguments, but pointer type.
		   No trailing comma or semicolon! */
   HELP(":<host>:<port>")	/* a text displayed from xio help function.
			   No trailing comma or semicolon!
			   only generates this text if WITH_HELP is != 0 */
} ;

#if WITH_LISTEN
/* description record for ssl listen */
const struct addrdesc xioaddr_openssl_listen = {
   "OPENSSL-LISTEN",	/* keyword for selecting this address type in xioopen calls
		   (canonical or main name) */
   3,		/* data flow directions this address supports on API layer:
		   1..read, 2..write, 3..both */
   xioopen_openssl_listen,	/* a function pointer used to "open" these addresses.*/
   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_TCP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE|GROUP_OPENSSL|GROUP_RETRY,	/* bitwise OR of address groups this address belongs to.
		   You might have to specify a new group in xioopts.h */
   0,		/* an integer passed to xioopen_openssl_listen; makes it possible to
		   use the xioopen_openssl_listen function for slightly different
		   address types. */
   0,		/* like previous argument */
   0		/* like previous arguments, but pointer type.
		   No trailing comma or semicolon! */
   HELP(":<port>")	/* a text displayed from xio help function.
			   No trailing comma or semicolon!
			   only generates this text if WITH_HELP is != 0 */
} ;
#endif /* WITH_LISTEN */

const struct addrdesc xioaddr_openssl_dtls_client = { "OPENSSL-DTLS-CLIENT", 3, xioopen_openssl_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_CHILD|GROUP_OPENSSL|GROUP_RETRY, 1, 0, 0  HELP(":<host>:<port>") } ;
#if WITH_LISTEN
const struct addrdesc xioaddr_openssl_dtls_server = { "OPENSSL-DTLS-SERVER", 3, xioopen_openssl_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE|GROUP_OPENSSL|GROUP_RETRY, 1, 0, 0  HELP(":<port>") } ;
#endif

/* both client and server */
const struct optdesc opt_openssl_cipherlist = { "openssl-cipherlist", "ciphers", OPT_OPENSSL_CIPHERLIST, GROUP_OPENSSL, PH_SPEC, TYPE_STRING, OFUNC_SPEC };
#if WITH_OPENSSL_METHOD
const struct optdesc opt_openssl_method     = { "openssl-method",     "method",  OPT_OPENSSL_METHOD,     GROUP_OPENSSL, PH_SPEC, TYPE_STRING, OFUNC_SPEC };
#endif
#if HAVE_SSL_CTX_set_min_proto_version || defined(SSL_CTX_set_min_proto_version)
const struct optdesc opt_openssl_min_proto_version = { "openssl-min-proto-version", "min-version", OPT_OPENSSL_MIN_PROTO_VERSION, GROUP_OPENSSL, PH_OFFSET, TYPE_STRING, OFUNC_OFFSET, XIO_OFFSETOF(para.openssl.min_proto_version) };
#endif
#if HAVE_SSL_CTX_set_max_proto_version || defined(SSL_CTX_set_max_proto_version)
const struct optdesc opt_openssl_max_proto_version = { "openssl-max-proto-version", "max-version", OPT_OPENSSL_MAX_PROTO_VERSION, GROUP_OPENSSL, PH_OFFSET, TYPE_STRING, OFUNC_OFFSET, XIO_OFFSETOF(para.openssl.max_proto_version) };
#endif
const struct optdesc opt_openssl_verify     = { "openssl-verify",     "verify",  OPT_OPENSSL_VERIFY,     GROUP_OPENSSL, PH_SPEC, TYPE_BOOL,   OFUNC_SPEC };
const struct optdesc opt_openssl_certificate = { "openssl-certificate", "cert",  OPT_OPENSSL_CERTIFICATE, GROUP_OPENSSL, PH_SPEC, TYPE_FILENAME, OFUNC_SPEC };
const struct optdesc opt_openssl_key         = { "openssl-key",         "key",   OPT_OPENSSL_KEY,         GROUP_OPENSSL, PH_SPEC, TYPE_FILENAME, OFUNC_SPEC };
const struct optdesc opt_openssl_dhparam     = { "openssl-dhparam",     "dh",    OPT_OPENSSL_DHPARAM,     GROUP_OPENSSL, PH_SPEC, TYPE_FILENAME, OFUNC_SPEC };
const struct optdesc opt_openssl_cafile      = { "openssl-cafile",     "cafile", OPT_OPENSSL_CAFILE,      GROUP_OPENSSL, PH_SPEC, TYPE_FILENAME, OFUNC_SPEC };
const struct optdesc opt_openssl_capath      = { "openssl-capath",     "capath", OPT_OPENSSL_CAPATH,      GROUP_OPENSSL, PH_SPEC, TYPE_FILENAME, OFUNC_SPEC };
const struct optdesc opt_openssl_egd         = { "openssl-egd",        "egd",    OPT_OPENSSL_EGD,         GROUP_OPENSSL, PH_SPEC, TYPE_FILENAME, OFUNC_SPEC };
#if HAVE_SSL_CTX_set_tlsext_max_fragment_length || defined(SSL_CTX_set_tlsext_max_fragment_length)
const struct optdesc opt_openssl_maxfraglen  = { "openssl-maxfraglen",  "maxfraglen",  OPT_OPENSSL_MAXFRAGLEN,  GROUP_OPENSSL, PH_SPEC, TYPE_INT, OFUNC_SPEC };
#endif
#if HAVE_SSL_CTX_set_max_send_fragment || defined(SSL_CTX_set_max_send_fragment)
const struct optdesc opt_openssl_maxsendfrag = { "openssl-maxsendfrag", "maxsendfrag", OPT_OPENSSL_MAXSENDFRAG, GROUP_OPENSSL, PH_SPEC, TYPE_INT, OFUNC_SPEC };
#endif
const struct optdesc opt_openssl_pseudo      = { "openssl-pseudo",     "pseudo", OPT_OPENSSL_PSEUDO,      GROUP_OPENSSL, PH_SPEC, TYPE_BOOL,     OFUNC_SPEC };
#if OPENSSL_VERSION_NUMBER >= 0x00908000L && !defined(OPENSSL_NO_COMP)
const struct optdesc opt_openssl_compress    = { "openssl-compress",   "compress", OPT_OPENSSL_COMPRESS,  GROUP_OPENSSL, PH_SPEC, TYPE_STRING,   OFUNC_SPEC };
#endif
#if WITH_FIPS
const struct optdesc opt_openssl_fips        = { "openssl-fips",       "fips",   OPT_OPENSSL_FIPS,        GROUP_OPENSSL, PH_SPEC, TYPE_BOOL,     OFUNC_SPEC };
#endif
const struct optdesc opt_openssl_commonname  = { "openssl-commonname", "cn",     OPT_OPENSSL_COMMONNAME,  GROUP_OPENSSL, PH_SPEC, TYPE_STRING,   OFUNC_SPEC };
#if defined(HAVE_SSL_set_tlsext_host_name) || defined(SSL_set_tlsext_host_name)
const struct optdesc opt_openssl_no_sni      = { "openssl-no-sni",    "nosni",   OPT_OPENSSL_NO_SNI,      GROUP_OPENSSL, PH_SPEC, TYPE_BOOL,     OFUNC_SPEC };
const struct optdesc opt_openssl_snihost     = { "openssl-snihost",   "snihost", OPT_OPENSSL_SNIHOST,     GROUP_OPENSSL, PH_SPEC, TYPE_STRING,   OFUNC_SPEC };
#endif

/* If FIPS is compiled in, we need to track if the user asked for FIPS mode.
 * On forks, the FIPS mode must be reset by a disable, then enable since
 * FIPS tracks the process ID that initializes things.
 * If FIPS is not compiled in, no tracking variable is needed
 * and we make the reset code compile out.  This keeps the
 * rest of the code below free of FIPS related #ifs
 */
#if WITH_FIPS
static bool xio_openssl_fips = false;
int xio_reset_fips_mode(void) {
   if (xio_openssl_fips) {
      if(!sycFIPS_mode_set(0) || !sycFIPS_mode_set(1)) {
	 ERR_load_crypto_strings();
	 ERR_print_errors(BIO_new_fp(stderr,BIO_NOCLOSE));
	 Error("Failed to reset OpenSSL FIPS mode");
	 xio_openssl_fips = false;
         return -1;
      }
   }
   return 0;
}
#else
#define xio_reset_fips_mode() 0
#endif

static void openssl_conn_loginfo(SSL *ssl) {
   const char *string;
   SSL_SESSION *session;

   string = SSL_get_cipher_version(ssl);
   Notice1("SSL proto version used: %s", string);
   xiosetenv("OPENSSL_PROTO_VERSION", string, 1, NULL);

   string = SSL_get_cipher(ssl);
   Notice1("SSL connection using %s", string);
   xiosetenv("OPENSSL_CIPHER", string, 1, NULL);

#if OPENSSL_VERSION_NUMBER >= 0x00908000L && !defined(OPENSSL_NO_COMP)
   {
      const COMP_METHOD *comp, *expansion;

      comp = sycSSL_get_current_compression(ssl);
      expansion = sycSSL_get_current_expansion(ssl);

      Notice1("SSL connection compression \"%s\"",
              comp?sycSSL_COMP_get_name(comp):"none");
      Notice1("SSL connection expansion \"%s\"",
              expansion?sycSSL_COMP_get_name(expansion):"none");
   }
#endif
   session = SSL_get_session(ssl);
   if (session == NULL) {
      Warn1("SSL_get_session(%p) failed", ssl);
      return;
   }
#if HAVE_SSL_CTX_set_tlsext_max_fragment_length || defined(SSL_CTX_set_tlsext_max_fragment_length)
   {
      uint8_t fragcod;
      int fraglen = -1;
      fragcod = SSL_SESSION_get_max_fragment_length(session);
      switch (fragcod) {
      case TLSEXT_max_fragment_length_DISABLED: fraglen =  0; break;
      case TLSEXT_max_fragment_length_512:  fraglen =  512; break;
      case TLSEXT_max_fragment_length_1024: fraglen = 1024; break;
      case TLSEXT_max_fragment_length_2048: fraglen = 2048; break;
      case TLSEXT_max_fragment_length_4096: fraglen = 4096; break;
      default: Warn1("SSL_SESSION_get_max_fragment_length(): unknown code %u",
		    fragcod);
	 break;
      }
      if (fraglen > 0) {
	 Info1("OpenSSL: max fragment length is %d", fraglen);
      }
   }
#endif
}

/* the open function for OpenSSL client */
static int xioopen_openssl_connect(
	int argc,
	const char *argv[],	/* the arguments in the address string */
	struct opt *opts,
	int xioflags,		/* is the open meant for reading (0),
				   writing (1), or both (2) ? */
	xiofile_t *xxfd,	/* a xio file descriptor structure,
				   already allocated */
	const struct addrdesc *addrdesc)	/* the above descriptor */
{
   struct single *sfd = &xxfd->stream;
   struct opt *opts0 = NULL;
   const char *hostname, *portname;
   int protogrp = addrdesc->arg1;
   int pf = PF_UNSPEC;
   bool use_dtls = (protogrp != 0);
   int socktype = SOCK_STREAM;
   int ipproto = IPPROTO_TCP;
   bool dofork = false;
   int maxchildren = 0;
   struct addrinfo **bindarr = NULL;
   struct addrinfo **themarr = NULL;
   uint16_t bindport = 0;
   bool needbind = false;
   bool lowport = false;
   int level = E_ERROR;
   SSL_CTX* ctx;
   bool opt_ver = true;	/* verify peer certificate */
   char *opt_cert = NULL;	/* file name of client certificate */
   const char *opt_commonname = NULL;	/* for checking peer certificate */
   bool        opt_no_sni = false;
   const char *opt_snihost = NULL;	/* for SNI host */
   int result;

   if (!(xioflags & XIO_MAYCONVERT)) {
      Error1("%s: address with data processing not allowed here", argv[0]);
      return STAT_NORETRY;
   }
   sfd->flags |= XIO_DOESCONVERT;

   if (argc != 3) {
      xio_syntax(argv[0], 2, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }
   hostname = argv[1];
   portname = argv[2];
   if (hostname[0] == '\0') {
      /* We catch this explicitly because empty commonname (peername) disables
	 commonName check of peer certificate */
      Error1("%s: empty host name", argv[0]);
      return STAT_NORETRY;
   }

   retropt_string(opts, OPT_OPENSSL_CERTIFICATE, &opt_cert);
   retropt_string(opts, OPT_OPENSSL_COMMONNAME, (char **)&opt_commonname);
#if defined(HAVE_SSL_set_tlsext_host_name) || defined(SSL_set_tlsext_host_name)
   retropt_bool(opts, OPT_OPENSSL_NO_SNI, &opt_no_sni);
   retropt_string(opts, OPT_OPENSSL_SNIHOST, (char **)&opt_snihost);
#endif

   if (opt_commonname == NULL) {
      opt_commonname = strdup(hostname);
      if (opt_commonname == NULL) {
	 Error1("strdup("F_Zu"): out of memory", strlen(hostname)+1);
      }
   }

#if defined(HAVE_SSL_set_tlsext_host_name) || defined(SSL_set_tlsext_host_name)
   if (opt_snihost != NULL) {
      if (check_ipaddr(opt_snihost) == 0) {
	 Warn1("specified SNI host \"%s\" is an IP address", opt_snihost);
      }
   } else if (check_ipaddr(opt_commonname) != 0) {
      opt_snihost = strdup(opt_commonname);
      if (opt_snihost == NULL) {
	 Error1("strdup("F_Zu"): out of memory", strlen(opt_commonname)+1);
      }
   }
#endif

   result =
      _xioopen_openssl_prepare(opts, sfd, false, &opt_ver, opt_cert, &ctx, (bool *)&use_dtls);
   if (result != STAT_OK)  return STAT_NORETRY;

   if (use_dtls) {
      socktype = SOCK_DGRAM;
      ipproto = IPPROTO_UDP;
   }

   /* Apply and retrieve some options */
   result = _xioopen_ipapp_init(sfd, xioflags, opts,
				&dofork, &maxchildren,
				&pf, &socktype, &ipproto);
   if (result != STAT_OK)
      return result;

   opts0 = opts; 	/* save remaining options for each loop */
   opts = NULL;

   Notice2("opening OpenSSL connection to %s:%s", hostname, portname);

   do {	/* loop over retries (failed connect and SSL handshake attempts) and/or forks */
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
      case STAT_NORETRY:
	 if (bindarr != NULL)  xiofreeaddrinfo(bindarr);
	 xiofreeaddrinfo(themarr);
	 freeopts(opts);
	 freeopts(opts0);
	 return result;
      }

      Notice2("opening connection to server %s:%s", hostname, portname);
      result =
	 _xioopen_ipapp_connect(sfd, hostname, opts, themarr,
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
      default:
	 Error4("%s:%s:%s: %s", argv[0], hostname, portname,
		_errno?strerror(_errno):"(See above)");
	 freeopts(opts0);
	 freeopts(opts);
	 return result;
      }

      result = _xioopen_openssl_connect(sfd, opt_ver, opt_commonname,
			opt_no_sni, opt_snihost, ctx, level);
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
	    Close(sfd->fd);
	    continue;
	 }
#endif /* WITH_RETRY */
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
	    level = E_WARN;
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
	 sycSSL_free(sfd->para.openssl.ssl);
	 sfd->para.openssl.ssl = NULL;
	 /* with and without retry */
	 Nanosleep(&sfd->intervall, NULL);
	 while (maxchildren > 0 && num_child >= maxchildren) {
	    Info1("all %d allowed children are active, waiting", maxchildren);
	    Nanosleep(&sfd->intervall, NULL);
	 }
	 freeopts(opts);
	 continue;	/* with next socket() bind() connect() */
      }
#endif /* WITH_RETRY */
      break;

   } while (true);	/* drop out on success */

   openssl_conn_loginfo(sfd->para.openssl.ssl);

   free((void *)opt_commonname);
   free((void *)opt_snihost);

   Notice2("successfully connected to SSL server %s:%s", hostname, portname);

   result = _xio_openlate(sfd, opts);
   freeopts(opts);
   freeopts(opts0);
   return result;
}


/* this function is typically called within the OpenSSL client fork/retry loop.
   sfd must be of type DATA_OPENSSL, and its fd must be set with a valid file
   descriptor. this function then performs all SSL related step to make a valid
   SSL connection from an FD and a CTX. */
int _xioopen_openssl_connect(struct single *sfd,
			     bool opt_ver,
			     const char *opt_commonname,
			     bool no_sni,
			     const char *snihost,
			     SSL_CTX *ctx,
			     int level) {
   SSL *ssl;
   unsigned long err;
   int result;

   /* create a SSL object */
   if ((ssl = sycSSL_new(ctx)) == NULL) {
      if (ERR_peek_error() == 0)  Msg(level, "SSL_new() failed");
      while (err = ERR_get_error()) {
	 Msg1(level, "SSL_new(): %s", ERR_error_string(err, NULL));
      }
      /*Error("SSL_new()");*/
      return STAT_RETRYLATER;
   }
   sfd->para.openssl.ssl = ssl;

   result = xioSSL_set_fd(sfd, level);
   if (result != STAT_OK) {
      sycSSL_free(sfd->para.openssl.ssl);
      sfd->para.openssl.ssl = NULL;
      return result;
   }

#if defined(HAVE_SSL_set_tlsext_host_name) || defined(SSL_set_tlsext_host_name)
   if (!no_sni) {
      if (snihost == NULL || strlen(snihost) == 0) {
	 Warn("refusing to set empty SNI host name");
      } else if (!SSL_set_tlsext_host_name(ssl, snihost)) {
	 Error1("Failed to set SNI host \"%s\"", snihost);
	 sycSSL_free(sfd->para.openssl.ssl);
	 sfd->para.openssl.ssl = NULL;
	 return STAT_NORETRY;
      }
   }
#endif

   result = xioSSL_connect(sfd, opt_commonname, opt_ver, level);
   if (result != STAT_OK) {
      sycSSL_free(sfd->para.openssl.ssl);
      sfd->para.openssl.ssl = NULL;
      return result;
   }

   result = openssl_handle_peer_certificate(sfd, opt_commonname,
					    opt_ver, level);
   if (result != STAT_OK) {
      sycSSL_free(sfd->para.openssl.ssl);
      sfd->para.openssl.ssl = NULL;
      return result;
   }

   return STAT_OK;
}


#if WITH_LISTEN

static int xioopen_openssl_listen(
	int argc,
	const char *argv[],	/* the arguments in the address string */
	struct opt *opts,
	int xioflags,		/* is the open meant for reading (0),
				   writing (1), or both (2) ? */
	xiofile_t *xxfd,	/* a xio file descriptor structure,
				   already allocated */
	const struct addrdesc *addrdesc)	/* the above descriptor */
{
   struct single *sfd = &xxfd->stream;
   const char *portname;
   int protogrp = addrdesc->arg1;
   struct opt *opts0 = NULL;
   union sockaddr_union us_sa, *us = &us_sa;
   socklen_t uslen = sizeof(us_sa);
   int pf = PF_UNSPEC;
   bool use_dtls = (protogrp != 0);
   int socktype = SOCK_STREAM;
   int ipproto = IPPROTO_TCP;
   /*! lowport? */
   int level;
   SSL_CTX* ctx;
   bool opt_ver = true;	/* verify peer certificate - changed with 1.6.0 */
   char *opt_cert = NULL;	/* file name of server certificate */
   const char *opt_commonname = NULL;	/* for checking peer certificate */
   int result;

   if (!(xioflags & XIO_MAYCONVERT)) {
      Error("address with data processing not allowed here");
      return STAT_NORETRY;
   }
   sfd->flags |= XIO_DOESCONVERT;

   if (argc != 2) {
      xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   xioinit_ip(&pf, xioparms.default_ip);
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

   portname = argv[1];

   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;
   if (applyopts_single(sfd, opts, PH_INIT) < 0)  return -1;
   applyopts(sfd, -1, opts, PH_INIT);

   retropt_string(opts, OPT_OPENSSL_CERTIFICATE, &opt_cert);
   if (opt_cert == NULL) {
      Warn("no certificate given; consider option \"cert\"");
   }

   retropt_string(opts, OPT_OPENSSL_COMMONNAME, (char **)&opt_commonname);

   applyopts(sfd, -1, opts, PH_EARLY);

   result =
      _xioopen_openssl_prepare(opts, sfd, true, &opt_ver, opt_cert, &ctx, &use_dtls);
   if (result != STAT_OK)  return STAT_NORETRY;

   if (use_dtls) {
      socktype = SOCK_DGRAM;
      ipproto = IPPROTO_UDP;
   }
   retropt_int(opts, OPT_SO_TYPE,      &socktype);
   retropt_int(opts, OPT_SO_PROTOTYPE, &ipproto);

   if (_xioopen_ipapp_listen_prepare(opts, &opts0, portname, &pf, ipproto,
				     sfd->para.socket.ip.ai_flags,
				     us, &uslen, socktype)
       != STAT_OK) {
      return STAT_NORETRY;
   }
   if (pf == 0)
      pf = us->soa.sa_family;

   sfd->dtype = XIODATA_OPENSSL;

   while (true) {	/* loop over failed attempts */

#if WITH_RETRY
      if (sfd->forever || sfd->retry) {
	 level = E_INFO;
      } else
#endif /* WITH_RETRY */
	 level = E_ERROR;

      /* this can fork() for us; it only returns on error or on
	 successful establishment of connection */
      if (ipproto == IPPROTO_TCP
#ifdef IPPROTO_DCCP
	  || ipproto == IPPROTO_DCCP
#endif
	  ) {
	 result = _xioopen_listen(sfd, xioflags,
			       (struct sockaddr *)us, uslen,
			       opts, pf, socktype, ipproto,
#if WITH_RETRY
			       (sfd->retry||sfd->forever)?E_INFO:E_ERROR
#else
			       E_ERROR
#endif /* WITH_RETRY */
			       );
#if WITH_UDP
      } else {
	 result = _xioopen_ipdgram_listen(sfd, xioflags,
		us, uslen, opts, pf, socktype, ipproto);
#endif /* WITH_UDP */
      }
	 /*! not sure if we should try again on retry/forever */
      switch (result) {
      case STAT_OK: break;
#if WITH_RETRY
      case STAT_RETRYLATER:
      case STAT_RETRYNOW:
	 if (sfd->forever || sfd->retry) {
	    dropopts(opts, PH_ALL); opts = copyopts(opts0, GROUP_ALL);
	    if (result == STAT_RETRYLATER) {
	       Nanosleep(&sfd->intervall, NULL);
	    }
	    dropopts(opts, PH_ALL); opts = copyopts(opts0, GROUP_ALL);
	    --sfd->retry;
	    continue;
	 }
	 return STAT_NORETRY;
#endif /* WITH_RETRY */
      default:
	 return result;
      }

      result = _xioopen_openssl_listen(sfd, opt_ver, opt_commonname, ctx, level);
      switch (result) {
      case STAT_OK: break;
#if WITH_RETRY
      case STAT_RETRYLATER:
      case STAT_RETRYNOW:
	 if (sfd->forever || sfd->retry) {
	    dropopts(opts, PH_ALL); opts = copyopts(opts0, GROUP_ALL);
	    if (result == STAT_RETRYLATER) {
	       Nanosleep(&sfd->intervall, NULL);
	    }
	    dropopts(opts, PH_ALL); opts = copyopts(opts0, GROUP_ALL);
	    --sfd->retry;
	    continue;
	 }
	 return STAT_NORETRY;
#endif /* WITH_RETRY */
      default:
	 return result;
      }

      openssl_conn_loginfo(sfd->para.openssl.ssl);
      break;

   }	/* drop out on success */

   /* fill in the fd structure */

   return STAT_OK;
}


int _xioopen_openssl_listen(struct single *sfd,
			     bool opt_ver,
			    const char *opt_commonname,
			     SSL_CTX *ctx,
			     int level) {
   char error_string[256];
   unsigned long err;
   int errint, ret;

   /* create an SSL object */
   if ((sfd->para.openssl.ssl = sycSSL_new(ctx)) == NULL) {
      if (ERR_peek_error() == 0)  Msg(level, "SSL_new() failed");
      while (err = ERR_get_error()) {
	 Msg1(level, "SSL_new(): %s", ERR_error_string(err, NULL));
      }
      /*Error("SSL_new()");*/
      return STAT_NORETRY;
   }

   /* assign the network connection to the SSL object */
   if (sycSSL_set_fd(sfd->para.openssl.ssl, sfd->fd) <= 0) {
      if (ERR_peek_error() == 0) Msg(level, "SSL_set_fd() failed");
      while (err = ERR_get_error()) {
	 Msg2(level, "SSL_set_fd(, %d): %s",
	      sfd->fd, ERR_error_string(err, NULL));
      }
   }

#if WITH_DEBUG
   {
      int i = 0;
      const char *ciphers = NULL;
      Debug("available ciphers:");
      do {
	 ciphers = SSL_get_cipher_list(sfd->para.openssl.ssl, i);
	 if (ciphers == NULL)  break;
	 Debug2("CIPHERS pri=%d: %s", i, ciphers);
	 ++i;
      } while (1);
   }
#endif /* WITH_DEBUG */

   /* connect via SSL by performing handshake */
   if ((ret = sycSSL_accept(sfd->para.openssl.ssl)) <= 0) {
      /*if (ERR_peek_error() == 0) Msg(level, "SSL_accept() failed");*/
      errint = SSL_get_error(sfd->para.openssl.ssl, ret);
      switch (errint) {
      case SSL_ERROR_NONE:
	 Msg(level, "ok"); break;
      case SSL_ERROR_ZERO_RETURN:
	 Msg(level, "connection closed (wrong version number?)"); break;
      case SSL_ERROR_WANT_READ: case SSL_ERROR_WANT_WRITE:
      case SSL_ERROR_WANT_CONNECT:
      case SSL_ERROR_WANT_X509_LOOKUP:
	 Msg(level, "nonblocking operation did not complete"); break;	/*!*/
      case SSL_ERROR_SYSCALL:
	 if (ERR_peek_error() == 0) {
	    if (ret == 0) {
	       Msg(level, "SSL_accept(): socket closed by peer");
	    } else if (ret == -1) {
	       Msg1(level, "SSL_accept(): %s", strerror(errno));
	    }
	 } else {
	    Msg(level, "I/O error");	/*!*/
	    while (err = ERR_get_error()) {
	       ERR_error_string_n(err, error_string, sizeof(error_string));
	       Msg4(level, "SSL_accept(): %s / %s / %s / %s", error_string,
		    ERR_lib_error_string(err), error_string,
		    ERR_reason_error_string(err));
	    }
	    /* Msg1(level, "SSL_accept(): %s", ERR_error_string(e, buf));*/
	 }
	 break;
      case SSL_ERROR_SSL:
	 /*ERR_print_errors_fp(stderr);*/
	 openssl_SSL_ERROR_SSL(level, "SSL_accept");
	 break;
      default:
	 Msg(level, "unknown error");
      }

      return STAT_RETRYLATER;
   }

   if (openssl_handle_peer_certificate(sfd, opt_commonname, opt_ver, E_ERROR/*!*/) < 0) {
      return STAT_NORETRY;
   }

   return STAT_OK;
}

#endif /* WITH_LISTEN */


#if OPENSSL_VERSION_NUMBER >= 0x00908000L
/* In OpenSSL 0.9.7 compression methods could be added using
 * SSL_COMP_add_compression_method(3), but the implementation is not compatible
 * with the standard (RFC3749).
 */
static int openssl_setup_compression(SSL_CTX *ctx, char *method)
{
   STACK_OF(SSL_COMP)* comp_methods;

   assert(method);

   /* Getting the stack of compression methods has the intended side-effect of
    * initializing the SSL library's compression part.
    */
   comp_methods = SSL_COMP_get_compression_methods();
   if (!comp_methods) {
      Info("OpenSSL built without compression support");
      return STAT_OK;
   }

   if (strcasecmp(method, "auto") == 0) {
      Info("Using default OpenSSL compression");
      return STAT_OK;
   }

   if (strcasecmp(method, "none") == 0) {
      /* Disable compression */
#ifdef SSL_OP_NO_COMPRESSION
      Info("Disabling OpenSSL compression");
      SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);
#else
      /* SSL_OP_NO_COMPRESSION was only introduced in OpenSSL 0.9.9 (released
       * as 1.0.0). Removing all compression methods is a work-around for
       * earlier versions of OpenSSL, but it affects all SSL connections.
       */
      Info("Disabling OpenSSL compression globally");
      sk_SSL_COMP_zero(comp_methods);
#endif
      return STAT_OK;
   }

   /* zlib compression in OpenSSL before version 0.9.8e-beta1 uses the libc's
    * default malloc/free instead of the ones passed to OpenSSL. Should socat
    * ever use custom malloc/free functions for OpenSSL, this must be taken
    * into consideration. See OpenSSL bug #1468.
    */

   Error1("openssl-compress=\"%s\": unknown compression method", method);
   return STAT_NORETRY;
}
#endif


#if HAVE_CTX_SSL_set_min_proto_version || defined(SSL_CTX_set_min_proto_version) || \
   HAVE_SSL_CTX_set_max_proto_version || defined(SSL_CTX_set_max_proto_version)
#define XIO_OPENSSL_VERSIONGROUP_TLS 1
#define XIO_OPENSSL_VERSIONGROUP_DTLS 2

static struct wordent _xio_openssl_versions[] = {
#ifdef DTLS1_VERSION
   { "DTLS1",		(void *)DTLS1_VERSION },
   { "DTLS1.0",		(void *)DTLS1_VERSION },
#endif
#ifdef DTLS1_2_VERSION
   { "DTLS1.2",		(void *)DTLS1_2_VERSION },
#endif
#ifdef DTLS1_VERSION
   { "DTLSv1",		(void *)DTLS1_VERSION },
   { "DTLSv1.0",	(void *)DTLS1_VERSION },
#endif
#ifdef DTLS1_2_VERSION
   { "DTLSv1.2",	(void *)DTLS1_2_VERSION },
#endif
#ifdef SSL2_VERSION
   { "SSL2",		(void *)SSL2_VERSION },
#endif
#ifdef SSL3_VERSION
   { "SSL3",		(void *)SSL3_VERSION },
#endif
#ifdef SSL2_VERSION
   { "SSLv2",		(void *)SSL2_VERSION },
#endif
#ifdef SSL3_VERSION
   { "SSLv3",		(void *)SSL3_VERSION },
#endif
#ifdef TLS1_VERSION
   { "TLS1",		(void *)TLS1_VERSION },
   { "TLS1.0",		(void *)TLS1_VERSION },
#endif
#ifdef TLS1_1_VERSION
   { "TLS1.1",		(void *)TLS1_1_VERSION },
#endif
#ifdef TLS1_2_VERSION
   { "TLS1.2",		(void *)TLS1_2_VERSION },
#endif
#ifdef TLS1_3_VERSION
   { "TLS1.3",		(void *)TLS1_3_VERSION },
#endif
#ifdef TLS1_VERSION
   { "TLSv1",		(void *)TLS1_VERSION },
   { "TLSv1.0",		(void *)TLS1_VERSION },
#endif
#ifdef TLS1_1_VERSION
   { "TLSv1.1",		(void *)TLS1_1_VERSION },
#endif
#ifdef TLS1_2_VERSION
   { "TLSv1.2",		(void *)TLS1_2_VERSION },
#endif
#ifdef TLS1_3_VERSION
   { "TLSv1.3",		(void *)TLS1_3_VERSION },
#endif
} ;

static int _xio_openssl_parse_version(const char *verstring, int vergroups) {
   int sslver;
   const struct wordent *we;
   we = keyw(_xio_openssl_versions, verstring,
	     sizeof(_xio_openssl_versions)/sizeof(struct wordent));
   if (we == 0) {
      Error1("Unknown SSL/TLS version \"%s\"", verstring);
      return -1;
   }
   sslver = (size_t)we->desc;
   switch (sslver) {
#ifdef SSL2_VERSION
   case SSL2_VERSION:
#endif
#ifdef SSL3_VERSION
   case SSL3_VERSION:
#endif
#ifdef TLS1_VERSION
   case TLS1_VERSION:
#endif
#ifdef TLS1_1_VERSION
   case TLS1_1_VERSION:
#endif
#ifdef TLS1_2_VERSION
   case TLS1_2_VERSION:
#endif
#ifdef TLS1_3_VERSION
   case TLS1_3_VERSION:
#endif
      if (!(vergroups & XIO_OPENSSL_VERSIONGROUP_TLS)) {
	 Error1("Wrong type of TLS/DTLS version \"%s\"", verstring);
	 return -1;
      }
#ifdef DTLS1_VERSION
   case DTLS1_VERSION:
#endif
#ifdef DTLS1_2_VERSION
   case DTLS1_2_VERSION:
#endif
      if (!(vergroups & XIO_OPENSSL_VERSIONGROUP_DTLS)) {
	 Error1("Wrong type of TLS/DTLS version \"%s\"", verstring);
	 return -1;
      }
      break;
   }
   return sslver;
}
#endif /* defined(SSL_CTX_set_min_proto_version) || defined(SSL_CTX_set_max_proto_version) */


int
   _xioopen_openssl_prepare(struct opt *opts,
			    struct single *sfd,/* a xio file descriptor
						  structure, already allocated
					       */
			    bool server,	/* SSL client: false */
			    bool *opt_ver,
			    const char *opt_cert,
			    SSL_CTX **ctxp,
			    bool *use_dtls)	/* checked,overwritten with true if DTLS-method */
{
   SSL_CTX *ctx;
   bool opt_fips = false;
   const SSL_METHOD *method = NULL;
   char *me_str = NULL;	/* method string */
   char *ci_str = "HIGH:-NULL:-PSK:-aNULL";	/* cipher string */
   char *opt_key  = NULL;	/* file name of client private key */
   char *opt_dhparam = NULL;	/* file name of DH params */
   char *opt_cafile = NULL;	/* certificate authority file */
   char *opt_capath = NULL;	/* certificate authority directory */
   char *opt_egd = NULL;	/* entropy gathering daemon socket path */
#if OPENSSL_VERSION_NUMBER >= 0x00908000L
   char *opt_compress = NULL;	/* compression method */
#endif
   bool opt_pseudo = false;	/* use pseudo entropy if nothing else */
   unsigned long err;
   int result;

   sfd->dtype = XIODATA_OPENSSL;

   retropt_bool(opts, OPT_OPENSSL_FIPS, &opt_fips);
   retropt_string(opts, OPT_OPENSSL_METHOD, &me_str);
   retropt_string(opts, OPT_OPENSSL_CIPHERLIST, &ci_str);
   retropt_bool(opts, OPT_OPENSSL_VERIFY, opt_ver);
   retropt_string(opts, OPT_OPENSSL_CAFILE, &opt_cafile);
   retropt_string(opts, OPT_OPENSSL_CAPATH, &opt_capath);
   retropt_string(opts, OPT_OPENSSL_KEY, &opt_key);
   retropt_string(opts, OPT_OPENSSL_DHPARAM, &opt_dhparam);
   retropt_string(opts, OPT_OPENSSL_EGD, &opt_egd);
   retropt_bool(opts,OPT_OPENSSL_PSEUDO, &opt_pseudo);
#if OPENSSL_VERSION_NUMBER >= 0x00908000L
   retropt_string(opts, OPT_OPENSSL_COMPRESS, &opt_compress);
#endif
#if WITH_FIPS
   if (opt_fips) {
      if (!sycFIPS_mode_set(1)) {
	 ERR_load_crypto_strings();
	 ERR_print_errors(BIO_new_fp(stderr,BIO_NOCLOSE));
	 Error("Failed to set FIPS mode");
      } else {
	 xio_openssl_fips = true;
      }
   }
#endif

   openssl_delete_cert_info();

   /* OpenSSL preparation */
#if defined(HAVE_OPENSSL_INIT_SSL) && defined(HAVE_OPENSSL_INIT_new)
   {
      uint64_t opts = 0;
#if defined(OPENSSL_INIT_SETTINGS)
      OPENSSL_INIT_SETTINGS *settings;
#else
      void *settings;
#endif
      settings = OPENSSL_INIT_new();
#ifdef OPENSSL_INIT_NO_ATEXIT
      opts |= OPENSSL_INIT_NO_ATEXIT;
#endif
      sycOPENSSL_init_ssl(opts, settings);
   }
#else
#  if defined(HAVE_SSL_library_init)
   sycSSL_library_init();
#  endif
   OpenSSL_add_all_algorithms();
   OpenSSL_add_all_ciphers();
   OpenSSL_add_all_digests();
   sycSSL_load_error_strings();
#endif /* defined(HAVE_OPENSSL_INIT_SSL) && defined(HAVE OPENSSL_INIT_new) */

   /*! actions_to_seed_PRNG();*/

   if (!server) {
      if (me_str != NULL) {
	 if (false) {
	    ;	/* for canonical reasons */
#if HAVE_SSLv2_client_method
	 } else if (!strcasecmp(me_str, "SSL2")) {
	    method = sycSSLv2_client_method();
#endif
#if HAVE_SSLv3_client_method
	 } else if (!strcasecmp(me_str, "SSL3")) {
	    method = sycSSLv3_client_method();
#endif
#if HAVE_SSLv23_client_method
	 } else if (!strcasecmp(me_str, "SSL23")) {
	    method = sycSSLv23_client_method();
#endif
#if HAVE_TLSv1_client_method
	 } else if (!strcasecmp(me_str, "TLS1") || !strcasecmp(me_str, "TLS1.0")) {
	    method = sycTLSv1_client_method();
#endif
#if HAVE_TLSv1_1_client_method
	 } else if (!strcasecmp(me_str, "TLS1.1")) {
	    method = sycTLSv1_1_client_method();
#endif
#if HAVE_TLSv1_2_client_method
	 } else if (!strcasecmp(me_str, "TLS1.2")) {
	    method = sycTLSv1_2_client_method();
#endif
#if HAVE_DTLSv1_client_method
	 } else if (!strcasecmp(me_str, "DTLS1") || !strcasecmp(me_str, "DTLS1.0")) {
	    method = sycDTLSv1_client_method();
	    *use_dtls = true;
#endif
#if HAVE_DTLSv1_2_client_method
	 } else if (!strcasecmp(me_str, "DTLS1.2")) {
	    method = sycDTLSv1_2_client_method();
	 *use_dtls = true;
#endif
	 } else {
	    Error1("openssl-method=\"%s\": method unknown or not provided by library", me_str);
	 }
      } else if (!*use_dtls) {
#if   HAVE_TLS_client_method
	 method = sycTLS_client_method();
#elif HAVE_SSLv23_client_method
	 method = sycSSLv23_client_method();
#elif HAVE_TLSv1_2_client_method
	 method = sycTLSv1_2_client_method();
#elif HAVE_TLSv1_1_client_method
	 method = sycTLSv1_1_client_method();
#elif HAVE_TLSv1_client_method
	 method = sycTLSv1_client_method();
#elif HAVE_SSLv3_client_method
	 method = sycSSLv3_client_method();
#elif HAVE_SSLv2_client_method
	 method = sycSSLv2_client_method();
#else
#        error "OpenSSL does not seem to provide SSL/TLS client methods"
#endif
      } else {
#if   HAVE_DTLS_client_method
	 method = sycDTLS_client_method();
#elif HAVE_DTLSv1_2_client_method
	 method = sycDTLSv1_2_client_method();
#elif HAVE_DTLSv1_client_method
	 method = sycDTLSv1_client_method();
#else
#        warning "OpenSSL does not seem to provide DTLS client methods"
#endif
	 *use_dtls = true;
      }
   } else /* server */ {
      if (me_str != 0) {
	 if (false) {
	    ;	/* for canonical reasons */
#if HAVE_SSLv2_server_method
	 } else if (!strcasecmp(me_str, "SSL2")) {
	    method = sycSSLv2_server_method();
#endif
#if HAVE_SSLv3_server_method
	 } else if (!strcasecmp(me_str, "SSL3")) {
	    method = sycSSLv3_server_method();
#endif
#if HAVE_SSLv23_server_method
	 } else if (!strcasecmp(me_str, "SSL23")) {
	    method = sycSSLv23_server_method();
#endif
#if HAVE_TLSv1_server_method
	 } else if (!strcasecmp(me_str, "TLS1") || !strcasecmp(me_str, "TLS1.0")) {
	    method = sycTLSv1_server_method();
#endif
#if HAVE_TLSv1_1_server_method
	 } else if (!strcasecmp(me_str, "TLS1.1")) {
	    method = sycTLSv1_1_server_method();
#endif
#if HAVE_TLSv1_2_server_method
	 } else if (!strcasecmp(me_str, "TLS1.2")) {
	    method = sycTLSv1_2_server_method();
#endif
#if HAVE_DTLSv1_server_method
	 } else if (!strcasecmp(me_str, "DTLS1") || !strcasecmp(me_str, "DTLS1.0")) {
	    method = sycDTLSv1_server_method();
	    *use_dtls = true;
#endif
#if HAVE_DTLSv1_2_server_method
	 } else if (!strcasecmp(me_str, "DTLS1.2")) {
	    method = sycDTLSv1_2_server_method();
	 *use_dtls = true;
#endif
	 } else {
	    Error1("openssl-method=\"%s\": method unknown or not provided by library", me_str);
	 }
      } else if (!*use_dtls) {
#if   HAVE_TLS_server_method
	 method = sycTLS_server_method();
#elif HAVE_SSLv23_server_method
	 method = sycSSLv23_server_method();
#elif HAVE_TLSv1_2_server_method
	 method = sycTLSv1_2_server_method();
#elif HAVE_TLSv1_1_server_method
	 method = sycTLSv1_1_server_method();
#elif HAVE_TLSv1_server_method
	 method = sycTLSv1_server_method();
#elif HAVE_SSLv3_server_method
	 method = sycSSLv3_server_method();
#elif HAVE_SSLv2_server_method
	 method = sycSSLv2_server_method();
#else
#        error "OpenSSL does not seem to provide SSL/TLS server methods"
#endif
      } else {
#if   HAVE_DTLS_server_method
	 method = sycDTLS_server_method();
#elif HAVE_DTLSv1_2_server_method
	 method = sycDTLSv1_2_server_method();
#elif HAVE_DTLSv1_server_method
	 method = sycDTLSv1_server_method();
#else
#        warning "OpenSSL does not seem to provide DTLS server methods"
#endif
	 *use_dtls = true;
      }
   }

   if (method == NULL) {
      Error("no OpenSSL method available");
      return STAT_NORETRY;
   }

   if (opt_egd) {
#if !defined(OPENSSL_NO_EGD) && HAVE_RAND_egd
      sycRAND_egd(opt_egd);
#else
      Debug("RAND_egd() is not available by OpenSSL");
#endif
   }

   if (opt_pseudo) {
      long int randdata;
      /* initialize libc random from actual microseconds */
      struct timeval tv;
      struct timezone tz;
      tz.tz_minuteswest = 0;
      tz.tz_dsttime = 0;
      if ((result = Gettimeofday(&tv, &tz)) < 0) {
	 Warn2("gettimeofday(%p, {0,0}): %s", &tv, strerror(errno));
      }
      srandom(tv.tv_sec*1000000+tv.tv_usec);

      while (!RAND_status()) {
	 randdata = random();
	 Debug2("RAND_seed(0x{%lx}, "F_Zu")",
		randdata, sizeof(randdata));
	 RAND_seed(&randdata, sizeof(randdata));
      }
   }

   if ((ctx = sycSSL_CTX_new(method)) == NULL) {
      if (ERR_peek_error() == 0) Error("SSL_CTX_new()");
      while (err = ERR_get_error()) {
	 Error1("SSL_CTX_new(): %s", ERR_error_string(err, NULL));
      }

      /*ERR_clear_error;*/
      return STAT_RETRYLATER;
   }
   sfd->para.openssl.ctx = ctx;
   *ctxp = ctx;

#if HAVE_SSL_CTX_set_min_proto_version || defined(SSL_CTX_set_min_proto_version)
   if (sfd->para.openssl.min_proto_version != NULL) {
      int sslver, rc;
      sslver = _xio_openssl_parse_version(sfd->para.openssl.min_proto_version,
					  XIO_OPENSSL_VERSIONGROUP_TLS|XIO_OPENSSL_VERSIONGROUP_DTLS);
      if (sslver < 0)
	 return STAT_NORETRY;
      if ((rc = SSL_CTX_set_min_proto_version(ctx, sslver)) <= 0) {
	 Debug1("version: %ld", SSL_CTX_get_min_proto_version(ctx));
	 Error3("_xioopen_openssl_prepare(): SSL_CTX_set_min_proto_version(\"%s\"->%d): failed (%d)",
		sfd->para.openssl.min_proto_version, sslver, rc);
	 return STAT_NORETRY;
      }
	 Debug1("version: %ld", SSL_CTX_get_min_proto_version(ctx));
   }
#endif /* HAVE_SSL_set_min_proto_version || defined(SSL_set_min_proto_version) */
#if HAVE_SSL_CTX_set_max_proto_version || defined(SSL_CTX_set_max_proto_version)
   if (sfd->para.openssl.max_proto_version != NULL) {
      int sslver;
      sslver = _xio_openssl_parse_version(sfd->para.openssl.max_proto_version,
					  XIO_OPENSSL_VERSIONGROUP_TLS|XIO_OPENSSL_VERSIONGROUP_DTLS);
      if (sslver < 0)
	 return STAT_NORETRY;
      if (SSL_CTX_set_max_proto_version(ctx, sslver) <= 0) {
	 Error2("_xioopen_openssl_prepare(): SSL_CTX_set_max_proto_version(\"%s\"->%d): failed",
		sfd->para.openssl.max_proto_version, sslver);
	 return STAT_NORETRY;
      }
   }
#endif /* HAVE_SSL_set_max_proto_version || defined(SSL_set_max_proto_version) */

   {
      static unsigned char dh2048_p[] = {
	 0x00,0xdc,0x21,0x64,0x56,0xbd,0x9c,0xb2,0xac,0xbe,0xc9,0x98,0xef,0x95,0x3e,
	 0x26,0xfa,0xb5,0x57,0xbc,0xd9,0xe6,0x75,0xc0,0x43,0xa2,0x1c,0x7a,0x85,0xdf,
	 0x34,0xab,0x57,0xa8,0xf6,0xbc,0xf6,0x84,0x7d,0x05,0x69,0x04,0x83,0x4c,0xd5,
	 0x56,0xd3,0x85,0x09,0x0a,0x08,0xff,0xb5,0x37,0xa1,0xa3,0x8a,0x37,0x04,0x46,
	 0xd2,0x93,0x31,0x96,0xf4,0xe4,0x0d,0x9f,0xbd,0x3e,0x7f,0x9e,0x4d,0xaf,0x08,
	 0xe2,0xe8,0x03,0x94,0x73,0xc4,0xdc,0x06,0x87,0xbb,0x6d,0xae,0x66,0x2d,0x18,
	 0x1f,0xd8,0x47,0x06,0x5c,0xcf,0x8a,0xb5,0x00,0x51,0x57,0x9b,0xea,0x1e,0xd8,
	 0xdb,0x8e,0x3c,0x1f,0xd3,0x2f,0xba,0x1f,0x5f,0x3d,0x15,0xc1,0x3b,0x2c,0x82,
	 0x42,0xc8,0x8c,0x87,0x79,0x5b,0x38,0x86,0x3a,0xeb,0xfd,0x81,0xa9,0xba,0xf7,
	 0x26,0x5b,0x93,0xc5,0x3e,0x03,0x30,0x4b,0x00,0x5c,0xb6,0x23,0x3e,0xea,0x94,
	 0xc3,0xb4,0x71,0xc7,0x6e,0x64,0x3b,0xf8,0x92,0x65,0xad,0x60,0x6c,0xd4,0x7b,
	 0xa9,0x67,0x26,0x04,0xa8,0x0a,0xb2,0x06,0xeb,0xe0,0x7d,0x90,0xdd,0xdd,0xf5,
	 0xcf,0xb4,0x11,0x7c,0xab,0xc1,0xa3,0x84,0xbe,0x27,0x77,0xc7,0xde,0x20,0x57,
	 0x66,0x47,0xa7,0x35,0xfe,0x0d,0x6a,0x1c,0x52,0xb8,0x58,0xbf,0x26,0x33,0x81,
	 0x5e,0xb7,0xa9,0xc0,0xee,0x58,0x11,0x74,0x86,0x19,0x08,0x89,0x1c,0x37,0x0d,
	 0x52,0x47,0x70,0x75,0x8b,0xa8,0x8b,0x30,0x11,0x71,0x36,0x62,0xf0,0x73,0x41,
	 0xee,0x34,0x9d,0x0a,0x2b,0x67,0x4e,0x6a,0xa3,0xe2,0x99,0x92,0x1b,0xf5,0x32,
	 0x73,0x63
      };
      static unsigned char dh2048_g[] = {
	 0x02,
      };
      DH *dh;
      BIGNUM *p = NULL, *g = NULL;
      unsigned long err;

      dh = DH_new();
      p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
      g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
      if (!dh || !p || !g) {
         if (dh)
            DH_free(dh);
         if (p)
            BN_free(p);
         if (g)
            BN_free(g);
         while (err = ERR_get_error()) {
            Warn1("dh2048 setup(): %s",
                  ERR_error_string(err, NULL));
         }
         Error("dh2048 setup failed");
         goto cont_out;
      }
#if HAVE_DH_set0_pqg
      if (!DH_set0_pqg(dh, p, NULL, g)) {
	      DH_free(dh);
	      BN_free(p);
	      BN_free(g);
	      goto cont_out;
      }
#else
      dh->p = p;
      dh->g = g;
#endif /* HAVE_DH_set0_pqg */
      if (sycSSL_CTX_set_tmp_dh(ctx, dh) <= 0) {
         while (err = ERR_get_error()) {
            Warn3("SSL_CTX_set_tmp_dh(%p, %p): %s", ctx, dh,
                  ERR_error_string(err, NULL));
         }
         Error2("SSL_CTX_set_tmp_dh(%p, %p) failed", ctx, dh);
      }
      /* p & g are freed by DH_free() once attached */
      DH_free(dh);
cont_out:
      ;
   }

#if HAVE_TYPE_EC_KEY	/* not on Openindiana 5.11 */
   {
      /* see http://openssl.6102.n7.nabble.com/Problem-with-cipher-suite-ECDHE-ECDSA-AES256-SHA384-td42229.html */
      int	 nid;
      EC_KEY *ecdh;

#if 0
      nid = OBJ_sn2nid(ECDHE_CURVE);
      if (nid == NID_undef) {
	 Error("openssl: failed to set ECDHE parameters");
	 return -1;
      }
#endif
      nid = NID_X9_62_prime256v1;
      ecdh = EC_KEY_new_by_curve_name(nid);
      if (NULL == ecdh) {
	 Error("openssl: failed to set ECDHE parameters");
	 return -1;
      }

      SSL_CTX_set_tmp_ecdh(ctx, ecdh);
   }
#endif /* HAVE_TYPE_EC_KEY */

#if OPENSSL_VERSION_NUMBER >= 0x00908000L
   if (opt_compress) {
      int result;
      result = openssl_setup_compression(ctx, opt_compress);
      if (result != STAT_OK) {
	return result;
      }
   }
#endif

#if defined(HAVE_SSL_CTX_clear_mode) || defined(SSL_CTX_clear_mode)
   /* It seems that OpenSSL-1.1.1 presets the mode differently.
      Without correction socat might hang in SSL_read() */
   {
      long mode = 0;
      mode = SSL_CTX_get_mode(ctx);
      if (mode & SSL_MODE_AUTO_RETRY) {
	 Info("SSL_CTX mode has SSL_MODE_AUTO_RETRY set. Correcting..");
	 Debug1("SSL_CTX_clear_mode(%p, SSL_MODE_AUTO_RETRY)", ctx);
	 SSL_CTX_clear_mode(ctx, SSL_MODE_AUTO_RETRY);
      }
   }
#endif /* defined(HAVE_SSL_CTX_clear_mode) || defined(SSL_CTX_clear_mode) */

   if (opt_cafile != NULL || opt_capath != NULL) {
      if (sycSSL_CTX_load_verify_locations(ctx, opt_cafile, opt_capath) != 1) {
	 int result;

	 if ((result =
	      openssl_SSL_ERROR_SSL(E_ERROR, "SSL_CTX_load_verify_locations"))
	     != STAT_OK) {
	    /*! free ctx */
	    return STAT_RETRYLATER;
	 }
      }
#ifdef HAVE_SSL_CTX_set_default_verify_paths
   } else {
      SSL_CTX_set_default_verify_paths(ctx);
#endif
   }

   /* set pre openssl-connect options */
   /* SSL_CIPHERS */
   if (ci_str != NULL) {
      if (sycSSL_CTX_set_cipher_list(ctx, ci_str) <= 0) {
	 if (ERR_peek_error() == 0)
	    Error1("SSL_set_cipher_list(, \"%s\") failed", ci_str);
	 while (err = ERR_get_error()) {
	    Error2("SSL_set_cipher_list(, \"%s\"): %s",
		   ci_str, ERR_error_string(err, NULL));
	 }
	 /*Error("SSL_new()");*/
	 return STAT_RETRYLATER;
      }
   }

   if (opt_cert) {
      BIO *bio;
      DH *dh;

      if (sycSSL_CTX_use_certificate_chain_file(ctx, opt_cert) <= 0) {
	 /*! trace functions */
	 /*0 ERR_print_errors_fp(stderr);*/
	 if (ERR_peek_error() == 0)
	    Error2("SSL_CTX_use_certificate_file(%p, \"%s\", SSL_FILETYPE_PEM) failed",
		 ctx, opt_cert);
	 while (err = ERR_get_error()) {
	    Error1("SSL_CTX_use_certificate_file(): %s",
		   ERR_error_string(err, NULL));
	 }
	 return STAT_RETRYLATER;
      }

      if (sycSSL_CTX_use_PrivateKey_file(ctx, opt_key?opt_key:opt_cert, SSL_FILETYPE_PEM) <= 0) {
	 /*ERR_print_errors_fp(stderr);*/
	 openssl_SSL_ERROR_SSL(E_ERROR/*!*/, "SSL_CTX_use_PrivateKey_file");
	 return STAT_RETRYLATER;
      }

      if (opt_dhparam == NULL) {
	 opt_dhparam = (char *)opt_cert;
      }
      if ((bio = sycBIO_new_file(opt_dhparam, "r")) == NULL) {
	 Warn2("BIO_new_file(\"%s\", \"r\"): %s",
	       opt_dhparam, strerror(errno));
      } else {
	 if ((dh = sycPEM_read_bio_DHparams(bio, NULL, NULL, NULL)) == NULL) {
	    Info1("PEM_read_bio_DHparams(%p, NULL, NULL, NULL): error", bio);
	 } else {
	    BIO_free(bio);
	    if (sycSSL_CTX_set_tmp_dh(ctx, dh) <= 0) {
	       while (err = ERR_get_error()) {
		  Warn3("SSL_CTX_set_tmp_dh(%p, %p): %s", ctx, dh,
			ERR_error_string(err, NULL));
	       }
	       Error2("SSL_CTX_set_tmp_dh(%p, %p): error", ctx, dh);
	    }
	 }
      }
   }

   if (*opt_ver) {
      sycSSL_CTX_set_verify(ctx,
			    SSL_VERIFY_PEER| SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
			    NULL);
      if (first_child) {
	 /* The first forked off process, print the warning only once */
	 Warn("OpenSSL: Warning: this implementation does not check CRLs");
      }
   } else {
      sycSSL_CTX_set_verify(ctx,
			    SSL_VERIFY_NONE,
			    NULL);
   }

#if HAVE_SSL_CTX_set_tlsext_max_fragment_length || defined(SSL_CTX_set_tlsext_max_fragment_length)
   {
      /* set client max fragment length negotiation (512, 1024, 2048, or 4096) */

      int opt_maxfraglen = -1;

      retropt_int(opts, OPT_OPENSSL_MAXFRAGLEN, &opt_maxfraglen);

      if (!server) {
         /* on client connection, ask the server not to send us packets bigger than our inbound buffer */
         uint8_t mfl_code = TLSEXT_max_fragment_length_DISABLED;
         if (opt_maxfraglen == -1) {
            /* max frag length is not specified, leave DISABLED */
         } else if (opt_maxfraglen == 512) {
            mfl_code = TLSEXT_max_fragment_length_512;
         } else if (opt_maxfraglen == 1024) {
            mfl_code = TLSEXT_max_fragment_length_1024;
         } else if (opt_maxfraglen == 2048) {
            mfl_code = TLSEXT_max_fragment_length_2048;
         } else if (opt_maxfraglen == 4096) {
            mfl_code = TLSEXT_max_fragment_length_4096;
         } else {
            Error1("openssl: maxfraglen %d is not one of 512, 1024, 2048, or 4096", opt_maxfraglen);
            return STAT_NORETRY;
         }

         sycSSL_CTX_set_tlsext_max_fragment_length(ctx, mfl_code);
      } else {
         if (opt_maxfraglen != -1) {
            Error("openssl: maxfraglen option not applicable to a server");
            return STAT_NORETRY;
         }
      }
   }
#endif

#if HAVE_SSL_CTX_set_max_send_fragment || defined(SSL_CTX_set_max_send_fragment)
   {
      /* limit the maximum size of sent packets */
      const int maxsendfrag_min = 512; /* per OpenSSL documentation */
      int opt_maxsendfrag = SSL3_RT_MAX_PLAIN_LENGTH;

      retropt_int(opts, OPT_OPENSSL_MAXSENDFRAG, &opt_maxsendfrag);

      if (opt_maxsendfrag < maxsendfrag_min || opt_maxsendfrag > SSL3_RT_MAX_PLAIN_LENGTH) {
         Error2("openssl: maxsendfrag %d out of range 512 - %d", maxsendfrag_min,
            SSL3_RT_MAX_PLAIN_LENGTH);
         return STAT_NORETRY;
      }

      sycSSL_CTX_set_max_send_fragment(ctx, opt_maxsendfrag);
   }
#endif

   return STAT_OK;
}


/* analyses an OpenSSL error condition, prints the appropriate messages with
   severity 'level' and returns one of STAT_OK, STAT_RETRYLATER, or
   STAT_NORETRY */
static int openssl_SSL_ERROR_SSL(int level, const char *funcname) {
   unsigned long e;
   char buf[120];	/* this value demanded by "man ERR_error_string" */
   int stat = STAT_OK;

   while (e = ERR_get_error()) {
      Debug1("ERR_get_error(): %lx", e);
      if
	 (
#if defined(OPENSSL_IS_BORINGSSL)
	  0  /* BoringSSL's RNG always succeeds. */
#elif defined(HAVE_RAND_status)
	  ERR_GET_LIB(e) == ERR_LIB_RAND && RAND_status() != 1
#else
	  e == ((ERR_LIB_RAND<<24)|
#if defined(RAND_F_RAND_BYTES)
		(RAND_F_RAND_BYTES<<12)|
#else
		(RAND_F_SSLEAY_RAND_BYTES<<12)|
#endif
		(RAND_R_PRNG_NOT_SEEDED)) /*0x24064064*/
#endif
	  )
      {
	 Error("too few entropy; use options \"egd\" or \"pseudo\"");
	 stat = STAT_NORETRY;
      } else {
	 Msg2(level, "%s(): %s", funcname, ERR_error_string(e, buf));
	 stat =  level==E_ERROR ? STAT_NORETRY : STAT_RETRYLATER;
      }
   }
   return stat;
}

static const char *openssl_verify_messages[] = {
   /*  0 */ "ok",
   /*  1 */ NULL,
   /*  2 */ "unable to get issuer certificate",
   /*  3 */ "unable to get certificate CRL",
   /*  4 */ "unable to decrypt certificate's signature",
   /*  5 */ "unable to decrypt CRL's signature",
   /*  6 */ "unable to decode issuer public key",
   /*  7 */ "certificate signature failure",
   /*  8 */ "CRL signature failure",
   /*  9 */ "certificate is not yet valid",
   /* 10 */ "certificate has expired",
   /* 11 */ "CRL is not yet valid",
   /* 12 */ "CRL has expired",
   /* 13 */ "format error in certificate's notBefore field",
   /* 14 */ "format error in certificate's notAfter field",
   /* 15 */ "format error in CRL's lastUpdate field",
   /* 16 */ "format error in CRL's nextUpdate field",
   /* 17 */ "out of memory",
   /* 18 */ "self signed certificate",
   /* 19 */ "self signed certificate in certificate chain",
   /* 20 */ "unable to get local issuer certificate",
   /* 21 */ "unable to verify the first certificate",
   /* 22 */ "certificate chain too long",
   /* 23 */ "certificate revoked",
   /* 24 */ "invalid CA certificate",
   /* 25 */ "path length constraint exceeded",
   /* 26 */ "unsupported certificate purpose",
   /* 27 */ "certificate not trusted",
   /* 28 */ "certificate rejected",
   /* 29 */ "subject issuer mismatch",
   /* 30 */ "authority and subject key identifier mismatch",
   /* 31 */ "authority and issuer serial number mismatch",
   /* 32 */ "key usage does not include certificate signing",
   /* 33 */ NULL,
   /* 34 */ NULL,
   /* 35 */ NULL,
   /* 36 */ NULL,
   /* 37 */ NULL,
   /* 38 */ NULL,
   /* 39 */ NULL,
   /* 40 */ NULL,
   /* 41 */ NULL,
   /* 42 */ NULL,
   /* 43 */ NULL,
   /* 44 */ NULL,
   /* 45 */ NULL,
   /* 46 */ NULL,
   /* 47 */ NULL,
   /* 48 */ NULL,
   /* 49 */ NULL,
   /* 50 */ "application verification failure",
} ;


/* delete all environment variables whose name begins with SOCAT_OPENSSL_
   resp. <progname>_OPENSSL_ */
static int openssl_delete_cert_info(void) {
#  define XIO_ENVNAMELEN 256
   const char *progname;
   char envprefix[XIO_ENVNAMELEN];
   char envname[XIO_ENVNAMELEN];
   size_t i, l;
   const char **entry;

   progname = diag_get_string('p');
   envprefix[0] = '\0'; strncat(envprefix, progname, XIO_ENVNAMELEN-1);
   l = strlen(envprefix);
   for (i = 0; i < l; ++i)  envprefix[i] = toupper((unsigned char)envprefix[i]);
   strncat(envprefix+l, "_OPENSSL_", XIO_ENVNAMELEN-l-1);

#if HAVE_VAR_ENVIRON
   entry = (const char **)environ;
   while (*entry != NULL) {
      if (!strncmp(*entry, envprefix, strlen(envprefix))) {
	 const char *eq = strchr(*entry, '=');
	 if (eq == NULL)  eq = *entry + strlen(*entry);
	 envname[0] = '\0'; strncat(envname, *entry, eq-*entry);
#if HAVE_UNSETENV
	 Unsetenv(envname);
#endif
      } else {
	 ++entry;
      }
   }
#endif /* HAVE_VAR_ENVIRON */
   return 0;
}

/* read in the "name" information (from field "issuer" or "subject") and
   create environment variable with complete info, eg:
   SOCAT_OPENSSL_X509_SUBJECT */
static int openssl_setenv_cert_name(const char *field, X509_NAME *name) {
   BIO *bio = BIO_new(BIO_s_mem());
   char *buf = NULL, *str;
   size_t len;
   X509_NAME_print_ex(bio, name, 0, XN_FLAG_ONELINE&~ASN1_STRFLGS_ESC_MSB);	/* rc not documented */
   len = BIO_get_mem_data (bio, &buf);
   if ((str = Malloc(len+1)) == NULL) {
      BIO_free(bio);
      return -1;
   }
   memcpy(str, buf, len);
   str[len] = '\0';
   Info2("SSL peer cert %s: \"%s\"", field, str);
   xiosetenv2("OPENSSL_X509", field, str, 1, NULL);
   free(str);
   BIO_free(bio);
   return 0;
}

/* read in the "name" information (from field "issuer" or "subject") and
   create environment variables with the fields, eg:
   SOCAT_OPENSSL_X509_COMMONNAME
*/
static int openssl_setenv_cert_fields(const char *field, X509_NAME *name) {
   int n, i;
   n = X509_NAME_entry_count(name);
   /* extract fields of cert name */
   for (i = 0; i < n; ++i) {
      X509_NAME_ENTRY *entry;
      ASN1_OBJECT *obj;
      ASN1_STRING *data;
      const unsigned char *text;
      int nid;
      entry = X509_NAME_get_entry(name, i);
      obj  = X509_NAME_ENTRY_get_object(entry);
      data = X509_NAME_ENTRY_get_data(entry);
      nid  = OBJ_obj2nid(obj);
#if HAVE_ASN1_STRING_get0_data
      text = ASN1_STRING_get0_data(data);
#else
      text = ASN1_STRING_data(data);
#endif
      Debug3("SSL peer cert %s entry: %s=\"%s\"", (field[0]?field:"subject"), OBJ_nid2ln(nid), text);
      if (field != NULL && field[0] != '\0') {
         xiosetenv3("OPENSSL_X509", field, OBJ_nid2ln(nid), (const char *)text, 2, " // ");
      } else {
         xiosetenv2("OPENSSL_X509", OBJ_nid2ln(nid), (const char *)text, 2, " // ");
      }
   }
   return 0;
}

/* compares the peername used/provided by the client to cn as extracted from
   the peer certificate.
   supports wildcard cn like *.domain which matches domain and
   host.domain
   returns true on match */
static bool openssl_check_name(const char *nametype, const char *cn, const char *peername) {
   const char *dotp;
   if (peername == NULL) {
      Info2("%s \"%s\": no peername", nametype, cn);
      return false;
   } else if (peername[0] == '\0') {
      Info2("%s \"%s\": matched by empty peername", nametype, cn);
      return true;
   }
   if (! (cn[0] == '*' && cn[1] == '.')) {
      /* normal server name - this is simple */
      if (strcmp(cn, peername) == 0) {
	 Debug3("%s \"%s\" matches peername \"%s\"", nametype, cn, peername);
	 return true;
      } else {
	 Info3("%s \"%s\" does not match peername \"%s\"", nametype, cn, peername);
	 return false;
      }
   }
   /* wildcard cert */
   Debug2("%s \"%s\" is a wildcard name", nametype, cn);
   /* case: just the base domain */
   if (strcmp(cn+2, peername) == 0) {
      Debug3("wildcard %s \"%s\" matches base domain \"%s\"", nametype, cn, peername);
      return true;
   }
   /* case: subdomain; only one level! */
   dotp = strchr(peername, '.');
   if (dotp == NULL) {
      Info2("peername \"%s\" is not a subdomain, thus is not matched by wildcard commonName \"%s\"",
	    peername, cn);
      return false;
   }
   if (strcmp(cn+1, dotp) != 0) {
      Info3("%s \"%s\" does not match subdomain peername \"%s\"", nametype, cn, peername);
      return false;
   }
   Debug3("%s \"%s\" matches subdomain peername \"%s\"", nametype, cn, peername);
   return true;
}

/* retrieves the commonName field and compares it to the peername
   returns true on match, false otherwise */
static bool openssl_check_peername(X509_NAME *name, const char *peername) {
   int ind = -1;
   X509_NAME_ENTRY *entry;
   ASN1_STRING *data;
   const unsigned char *text;
   ind = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
   if (ind < 0) {
      Info("no COMMONNAME field in peer certificate");
      return false;
   }
   entry = X509_NAME_get_entry(name, ind);
   data = X509_NAME_ENTRY_get_data(entry);
#if HAVE_ASN1_STRING_get0_data
   text = ASN1_STRING_get0_data(data);
#else
   text = ASN1_STRING_data(data);
#endif
   return openssl_check_name("commonName", (const char *)text, peername);
}

/* retrieves certificate provided by peer, sets env vars containing
   certificates field values, and checks peername if provided by
   calling function */
/* parts of this code were copied from Gene Spaffords C/C++ Secure Programming at Etutorials.org:
   http://etutorials.org/Programming/secure+programming/Chapter+10.+Public+Key+Infrastructure/10.8+Adding+Hostname+Checking+to+Certificate+Verification/
   The code examples in this tutorial do not seem to have explicit license restrictions.
*/
/* peername is, with OpenSSL client, the server name, or the value of option
   commonname if provided;
   With OpenSSL server, it is the value of option commonname */
static int openssl_handle_peer_certificate(struct single *sfd,
					   const char *peername,
					   bool opt_ver, int level) {
   X509 *peer_cert;
   X509_NAME *subjectname, *issuername;
   /*ASN1_TIME not_before, not_after;*/
   int extcount, i, ok = 0;
   int status;

   if ((peer_cert = SSL_get_peer_certificate(sfd->para.openssl.ssl)) == NULL) {
      if (opt_ver) {
	 Msg(level, "no peer certificate");
	 status = STAT_RETRYLATER;
      } else {
	 Notice("no peer certificate and no check");
	 status = STAT_OK;
      }
      return status;
   }

   /* verify peer certificate (trust, signature, validity dates) */
   if (opt_ver) {
      long verify_result;
      if ((verify_result = sycSSL_get_verify_result(sfd->para.openssl.ssl)) != X509_V_OK) {
	 const char *message = NULL;
	 if (verify_result >= 0 &&
	     (size_t)verify_result <
	     sizeof(openssl_verify_messages)/sizeof(char*)) {
	    message = openssl_verify_messages[verify_result];
	 }
	 if (message) {
	    Msg1(level, "%s", message);
	 } else {
	    Msg1(level, "rejected peer certificate with error %ld", verify_result);
	 }
	 status = STAT_RETRYLATER;
	 X509_free(peer_cert);
	 return STAT_RETRYLATER;
      }
      Info("peer certificate is trusted");
   }

   /* set env vars from cert's subject and issuer values */
   if ((subjectname = X509_get_subject_name(peer_cert)) != NULL) {
      openssl_setenv_cert_name("subject", subjectname);
      openssl_setenv_cert_fields("", subjectname);
      /*! I'd like to provide dates too; see
	 http://markmail.org/message/yi4vspp7aeu3xwtu#query:+page:1+mid:jhnl4wklif3pgzqf+state:results */
   }
   if ((issuername = X509_get_issuer_name(peer_cert)) != NULL) {
      openssl_setenv_cert_name("issuer", issuername);
   }

   if (!opt_ver) {
      Notice("option openssl-verify disabled, no check of certificate");
      X509_free(peer_cert);
      return STAT_OK;
   }

   /* check peername against cert's subjectAltName DNS entries */
   /* this code is based on example from Gerhard Gappmeier in
      http://openssl.6102.n7.nabble.com/How-to-extract-subjectAltName-td17236.html
      and the GEN_IPADD from
      http://openssl.6102.n7.nabble.com/reading-IP-addresses-from-Subject-Alternate-Name-extension-td29245.html
   */
   if ((extcount = X509_get_ext_count(peer_cert)) > 0) {
      for (i = 0;  !ok && i < extcount;  ++i) {
	 const char            *extstr;
	 X509_EXTENSION        *ext;
	 const X509V3_EXT_METHOD     *meth;
	 ext = X509_get_ext(peer_cert, i);
	 extstr = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));
	 if (!strcasecmp(extstr, "subjectAltName")) {
	    void *names;
	    if (!(meth = X509V3_EXT_get(ext))) break;
	    names = X509_get_ext_d2i(peer_cert, NID_subject_alt_name, NULL, NULL);
	    if (names) {
	       int numalts;
	       int i;

	       /* get amount of alternatives, RFC2459 claims there MUST be at least one, but we don't depend on it... */
	       numalts = sk_GENERAL_NAME_num ( names );
	       /* loop through all alternatives */
	       for (i = 0; i < numalts; ++i) {
		  /* get a handle to alternative name number i */
		  const GENERAL_NAME *pName = sk_GENERAL_NAME_value (names, i);
		  unsigned char *pBuffer;
		  switch (pName->type) {

		  case GEN_DNS:
		     ASN1_STRING_to_UTF8(&pBuffer, pName->d.ia5);
		     xiosetenv("OPENSSL_X509V3_SUBJECTALTNAME_DNS", (char *)pBuffer, 2, " // ");
		     if (peername != NULL &&
			 openssl_check_name("subjectAltName", (char *)pBuffer, /*const char*/peername)) {
			ok = 1;
		     }
		     OPENSSL_free(pBuffer);
		     break;

		  case GEN_IPADD:
		     {
			/* binary address format */
			const unsigned char *data = pName->d.iPAddress->data;
			size_t len = pName->d.iPAddress->length;
			char aBuffer[INET6_ADDRSTRLEN]; 	/* canonical peername */
			struct in6_addr ip6bin;

			switch (len) {
			case 4: /* IPv4 */
			   snprintf(aBuffer, sizeof(aBuffer), "%u.%u.%u.%u", data[0], data[1], data[2], data[3]);
			   if (peername != NULL &&
			       openssl_check_name("subjectAltName", aBuffer, /*const char*/peername)) {
			      ok = 1;
			   }
			   break;
#if WITH_IP6
			case 16: /* IPv6 */
			   inet_ntop(AF_INET6, data, aBuffer, sizeof(aBuffer));
			   if (peername != NULL) {
			      xioip6_pton(peername, &ip6bin, sfd->para.socket.ip.ai_flags);
			      if (memcmp(data, &ip6bin, sizeof(ip6bin)) == 0) {
			         Debug2("subjectAltName \"%s\" matches peername \"%s\"",
					aBuffer, peername);
			         ok = 1;
			      } else {
			         Info2("subjectAltName \"%s\" does not match peername \"%s\"",
				       aBuffer, peername);
			      }
			   }
			   break;
#endif
			}
			xiosetenv("OPENSSL_X509V3_SUBJECTALTNAME_IPADD", (char *)aBuffer, 2, " // ");
		     }
		     break;
		  default: Warn3("Unknown subject type %d (GEN_DNS=%d, GEN_IPADD=%d",
				 pName->type, GEN_DNS, GEN_IPADD);
		     continue;
		  }
		  if (ok)  { break; }
	       }
	    }
	 }
      }
   }

   if (ok) {
      Notice("trusting certificate, commonName matches");
      X509_free(peer_cert);
      return STAT_OK;
   }

   if (peername == NULL || peername[0] == '\0') {
      Notice("trusting certificate, no check of commonName");
      X509_free(peer_cert);
      return STAT_OK;
   }

   /* here: all envs set; opt_ver, cert verified, no subjAltName match -> check subject CN */
   if (!openssl_check_peername(/*X509_NAME*/subjectname, /*const char*/peername)) {
      Error1("certificate is valid but its commonName does not match hostname \"%s\"",
	     peername);
      status = STAT_NORETRY;
   } else {
      Notice("trusting certificate, commonName matches");
      status = STAT_OK;
   }
   X509_free(peer_cert);
   return status;
}

static int xioSSL_set_fd(struct single *sfd, int level) {
   unsigned long err;

   /* assign a network connection to the SSL object */
   if (sycSSL_set_fd(sfd->para.openssl.ssl, sfd->fd) <= 0) {
      Msg(level, "SSL_set_fd() failed");
      while (err = ERR_get_error()) {
	 Msg2(level, "SSL_set_fd(, %d): %s",
	      sfd->fd, ERR_error_string(err, NULL));
      }
      return STAT_RETRYLATER;
   }
   return STAT_OK;
}


/* ...
   in case of an error condition, this function check forever and retry
   options and ev. sleeps an interval. It returns NORETRY when the caller
   should not retry for any reason. */
static int xioSSL_connect(struct single *sfd, const char *opt_commonname,
			  bool opt_ver, int level) {
   sigset_t masksigs, oldsigs;
   char error_string[256];
   int errint, status, _errno, ret;
   unsigned long err;

   sigemptyset(&masksigs);
   sigaddset(&masksigs, SIGCHLD);
   sigaddset(&masksigs, SIGUSR1);
   Sigprocmask(SIG_BLOCK, &masksigs, &oldsigs);
   /* connect via SSL by performing handshake */
   ret = sycSSL_connect(sfd->para.openssl.ssl);
   _errno = errno;
   Sigprocmask(SIG_SETMASK, &oldsigs, NULL);
   if (ret <= 0) {
      /*if (ERR_peek_error() == 0) Msg(level, "SSL_connect() failed");*/
      errint = SSL_get_error(sfd->para.openssl.ssl, ret);
      switch (errint) {
      case SSL_ERROR_NONE:
	 /* this is not an error, but I dare not continue for security reasons*/
	 Msg(level, "ok");
	 status = STAT_RETRYLATER;
      case SSL_ERROR_ZERO_RETURN:
	 Msg(level, "connection closed (wrong version number?)");
	 status = STAT_RETRYLATER;
	 break;
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
      case SSL_ERROR_WANT_CONNECT:
      case SSL_ERROR_WANT_X509_LOOKUP:
	 Msg(level, "nonblocking operation did not complete");
	 status = STAT_RETRYLATER;
	 break;	/*!*/
      case SSL_ERROR_SYSCALL:
	 if (ERR_peek_error() == 0) {
	    if (ret == 0) {
	       Msg(level, "SSL_connect(): socket closed by peer");
	    } else if (ret == -1) {
	       Msg1(level, "SSL_connect(): %s", strerror(_errno));
	    }
	 } else {
	    Msg(level, "I/O error");	/*!*/
	    while (err = ERR_get_error()) {
	       ERR_error_string_n(err, error_string, sizeof(error_string));
	       Msg4(level, "SSL_connect(): %s / %s / %s / %s", error_string,
		    ERR_lib_error_string(err), error_string,
		    ERR_reason_error_string(err));
	    }
	 }
	 status = STAT_RETRYLATER;
	 break;
      case SSL_ERROR_SSL:
	 status = openssl_SSL_ERROR_SSL(level, "SSL_connect");
	 if (openssl_handle_peer_certificate(sfd, opt_commonname, opt_ver, level/*!*/) < 0) {
	    return STAT_RETRYLATER;
	 }
	 break;
      default:
	 Msg(level, "unknown error");
	 status = STAT_RETRYLATER;
	 break;
      }
      return status;
   }
   return STAT_OK;
}

/* on result < 0: errno is set (at least to EIO) */
ssize_t xioread_openssl(struct single *pipe, void *buff, size_t bufsiz) {
   unsigned long err;
   char error_string[256];
   int _errno = EIO;	/* if we have no better idea about nature of error */
   int errint, ret;

   ret = sycSSL_read(pipe->para.openssl.ssl, buff, bufsiz);
   if (ret < 0) {
      errint = SSL_get_error(pipe->para.openssl.ssl, ret);
      switch (errint) {
      case SSL_ERROR_NONE:
	 /* this is not an error, but I dare not continue for security reasons*/
	 Error("ok");
	 break;
      case SSL_ERROR_ZERO_RETURN:
	 Error("connection closed by peer");
	 break;
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
      case SSL_ERROR_WANT_CONNECT:
      case SSL_ERROR_WANT_X509_LOOKUP:
	 Info("nonblocking operation did not complete");
	 errno = EAGAIN;
	 return -1;
      case SSL_ERROR_SYSCALL:
	 if (ERR_peek_error() == 0) {
	    if (ret == 0) {
	       Error("SSL_read(): socket closed by peer");
	    } else if (ret == -1) {
	       _errno = errno;
	       Error1("SSL_read(): %s", strerror(errno));
	    }
	 } else {
	    Error("I/O error");	/*!*/
	    while (err = ERR_get_error()) {
	       ERR_error_string_n(err, error_string, sizeof(error_string));
	       Error4("SSL_read(): %s / %s / %s / %s", error_string,
		      ERR_lib_error_string(err), error_string,
		      ERR_reason_error_string(err));
	    }
	 }
	 break;
      case SSL_ERROR_SSL:
	 openssl_SSL_ERROR_SSL(E_ERROR, "SSL_read");
	 break;
      default:
	 Error("unknown error");
	 break;
      }
      errno = _errno;
      return -1;
   }
   return ret;
}

ssize_t xiopending_openssl(struct single *pipe) {
   int bytes = sycSSL_pending(pipe->para.openssl.ssl);
   return bytes;
}

/* on result < 0: errno is set (at least to EIO) */
ssize_t xiowrite_openssl(struct single *pipe, const void *buff, size_t bufsiz) {
   unsigned long err;
   char error_string[256];
   int _errno = EIO;	/* if we have no better idea about nature of error */
   int errint, ret;

   ret = sycSSL_write(pipe->para.openssl.ssl, buff, bufsiz);
   if (ret < 0) {
      errint = SSL_get_error(pipe->para.openssl.ssl, ret);
      switch (errint) {
      case SSL_ERROR_NONE:
	 /* this is not an error, but I dare not continue for security reasons*/
	 Error("ok");
      case SSL_ERROR_ZERO_RETURN:
	 Error("connection closed by peer");
	 break;
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
      case SSL_ERROR_WANT_CONNECT:
      case SSL_ERROR_WANT_X509_LOOKUP:
	 Error("nonblocking operation did not complete");
	 break;	/*!*/
      case SSL_ERROR_SYSCALL:
	 if (ERR_peek_error() == 0) {
	    if (ret == 0) {
	       Error("SSL_write(): socket closed by peer");
	    } else if (ret == -1) {
	       _errno = errno;
	       Error1("SSL_write(): %s", strerror(errno));
	    }
	 } else {
	    Error("I/O error");	/*!*/
	    while (err = ERR_get_error()) {
	       ERR_error_string_n(err, error_string, sizeof(error_string));
	       Error4("SSL_write(): %s / %s / %s / %s", error_string,
		      ERR_lib_error_string(err), error_string,
		      ERR_reason_error_string(err));
	    }
	 }
	 break;
      case SSL_ERROR_SSL:
	 openssl_SSL_ERROR_SSL(E_ERROR, "SSL_write");
	 break;
      default:
	 Error("unknown error");
	 break;
      }
      errno = _errno;
      return -1;
   }
   return ret;
}

int xioshutdown_openssl(struct single *sfd, int how)
{
   int rc;

   if ((rc = sycSSL_shutdown(sfd->para.openssl.ssl)) < 0) {
      Warn1("xioshutdown_openssl(): SSL_shutdown() -> %d", rc);
   }
   if (sfd->tag == XIO_TAG_WRONLY) {
      char buff[1];
      /* give peer time to read all data before closing socket */
      xioread_openssl(sfd, buff, 1);
   }
   return 0;
}

#endif /* WITH_OPENSSL */
