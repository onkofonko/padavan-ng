/* source: socat.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the main source, including command line option parsing, general
   control, and the data shuffler */

#include "config.h"
#include "xioconfig.h"	/* what features are enabled */

#include "sysincludes.h"

#include "mytypes.h"
#include "compat.h"
#include "error.h"

#include "sycls.h"
#include "sysutils.h"
#include "dalan.h"
#include "filan.h"
#include "xio.h"
#include "xioopts.h"
#include "xiolockfile.h"

#include "xio-pipe.h"


/* command line options */
struct socat_opts {
   bool verbose;
   bool verbhex;
   struct timeval pollintv;	/* with ignoreeof, reread after seconds */
   struct timeval closwait;	/* after close of x, die after seconds */
   struct timeval total_timeout;/* when nothing happens, die after seconds */
   bool debug;
   bool strictopts;	/* stop on errors in address options */
   char logopt;		/* y..syslog; s..stderr; f..file; m..mixed */
   bool lefttoright;	/* first addr ro, second addr wo */
   bool righttoleft;	/* first addr wo, second addr ro */
   xiolock_t lock;	/* a lock file */
   unsigned long log_sigs;	/* signals to be caught just for logging */
   bool statistics; 	/* log statistics on exit */
} socat_opts = {
   false,	/* verbose */
   false,	/* verbhex */
   {1,0},	/* pollintv */
   {0,500000},	/* closwait */
   {0,1000000},	/* total_timeout (this invalid default means no timeout)*/
   0,		/* debug */
   0,		/* strictopts */
   's',		/* logopt */
   false,	/* lefttoright */
   false,	/* righttoleft */
   { NULL, 0 },	/* lock */
   1<<SIGHUP | 1<<SIGINT | 1<<SIGQUIT | 1<<SIGILL | 1<<SIGABRT | 1<<SIGBUS | 1<<SIGFPE | 1<<SIGSEGV | 1<<SIGTERM, 	/* log_sigs */
   false	/* statistics */
};

void socat_usage(FILE *fd);
void socat_opt_hint(FILE *fd, char a, char b);
void socat_version(FILE *fd);
int socat(const char *address1, const char *address2);
int _socat(void);
int cv_newline(unsigned char *buff, ssize_t *bytes, int lineterm1, int lineterm2);
void socat_signal(int sig);
void socat_signal_logstats(int sig);
static int socat_sigchild(struct single *file);

void lftocrlf(char **in, ssize_t *len, size_t bufsiz);
void crlftolf(char **in, ssize_t *len, size_t bufsiz);

static int socat_lock(void);
static void socat_unlock(void);
static int socat_newchild(void);
static void socat_print_stats(void);

static const char socatversion[] =
#include "./VERSION"
      ;
static const char timestamp[] = BUILD_DATE;

const char copyright_socat[] = "socat by Gerhard Rieger and contributors - see www.dest-unreach.org";
#if WITH_OPENSSL
const char copyright_openssl[] = "This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/)";
const char copyright_ssleay[] = "This product includes software written by Tim Hudson (tjh@cryptsoft.com)";
#endif

bool havelock;

int main(int argc, const char *argv[]) {
   const char **arg1, *a;
   char *mainwaitstring;
   char buff[10];
   double rto;
   int i, argc0, result;
   bool isdash = false;
   int msglevel = 0;
   struct utsname ubuf;
   int lockrc;

   if (mainwaitstring = getenv("SOCAT_MAIN_WAIT")) {
       sleep(atoi(mainwaitstring));
   }
   diag_set('p', strchr(argv[0], '/') ? strrchr(argv[0], '/')+1 : argv[0]);

   /* we must init before applying options because env settings have lower
      priority and are to be overridden by options */
   if (xioinitialize() != 0) {
      Exit(1);
   }

   xiosetopt('p', "!!");
   xiosetopt('o', ":");

   argc0 = argc;	/* save for later use */
   arg1 = argv+1;  --argc;
   while (arg1[0] && (arg1[0][0] == '-')) {
      switch (arg1[0][1]) {
      case 'V':  if (arg1[0][2])  { socat_usage(stderr); Exit(1); }
	 socat_version(stdout); Exit(0);
#if WITH_HELP
      case '?':
      case 'h':
	 socat_usage(stdout);
	 xioopenhelp(stdout, (arg1[0][2]=='?'||arg1[0][2]=='h') ? (arg1[0][3]=='?'||arg1[0][3]=='h') ? 2 : 1 : 0);
	 Exit(0);
#endif /* WITH_HELP */
      case 'd':
	 a = *arg1+2;
	 switch (*a) {
	 case 'd':
	    break;
	 case '-': case '0': case '1': case '2': case '3': case '4':
	    {
	       char *endptr;
	       msglevel = strtol(a, &endptr, 0);
	       if (endptr == a || *endptr) {
		  Error2("Invalid (trailing) character(s) \"%c\" in \"%s\"option", *a, *arg1);
	       }
	       diag_set_int('d', 4-msglevel);
	    }
	    break;
	 case '\0':
	    ++msglevel;
	    diag_set_int('d', 4-msglevel);
	    break;
	 default: socat_usage(stderr);
	 }
	 if (*a != 'd')  break;
	 ++msglevel;
	 while (*a)  {
	    if (*a == 'd') {
	       ++msglevel;
	       diag_set_int('d', 4-msglevel);
	    } else {
	       socat_usage(stderr);
	       Exit(1);
	    }
	    ++a;
	 }
	 break;
#if WITH_FILAN
      case 'D':  if (arg1[0][2])  { socat_opt_hint(stderr, arg1[0][1], arg1[0][2]); Exit(1); }
	 socat_opts.debug = true; break;
#endif
      case 'l':
	 switch (arg1[0][2]) {
	 case 'm': /* mixed mode: stderr, then switch to syslog; + facility */
	    diag_set('s', NULL);
	    xiosetopt('l', "m");
	    socat_opts.logopt = arg1[0][2];
	    xiosetopt('y', &arg1[0][3]);
	    break;
	 case 'y': /* syslog + facility */
	    diag_set(arg1[0][2], &arg1[0][3]);
	    break;
	 case 'f': /* to file, +filename */
	 case 'p': /* artificial program name */
	    if (arg1[0][3]) {
	       diag_set(arg1[0][2], &arg1[0][3]);
	    } else if (arg1[1]) {
	       diag_set(arg1[0][2], arg1[1]);
	       ++arg1, --argc;
	    } else {
	       Error1("option -l%c requires an argument; use option \"-h\" for help", arg1[0][2]);
	    }
	    break;
	 case 's': /* stderr */
	    diag_set(arg1[0][2], NULL);
	    break;
	 case 'u':
	    diag_set('u', NULL);
	    break;
	 case 'h':
	    diag_set_int('h', true);
	    break;
	 default:
	    Error1("unknown log option \"%s\"; use option \"-h\" for help", arg1[0]);
	    break;
	 }
	 break;
      case 'v':  if (arg1[0][2])  { socat_opt_hint(stderr, arg1[0][1], arg1[0][2]); Exit(1); }
	 socat_opts.verbose = true; break;
      case 'x':  if (arg1[0][2])  { socat_opt_hint(stderr, arg1[0][1], arg1[0][2]); Exit(1); }
	 socat_opts.verbhex = true; break;
      case 'r': if (arg1[0][2]) {
	    a = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((a = *arg1) == NULL) {
	       Error("option -r requires an argument; use option \"-h\" for help");
	       break;
	    }
	 }
	 xiosetopt('r', a);
	 break;
      case 'R': if (arg1[0][2]) {
	    a = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((a = *arg1) == NULL) {
	       Error("option -R requires an argument; use option \"-h\" for help");
	       break;
	    }
	 }
	 xiosetopt('R', a);
	 break;
      case 'b': if (arg1[0][2]) {
	    a = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((a = *arg1) == NULL) {
	       Error("option -b requires an argument; use option \"-h\" for help");
	       Exit(1);
	    }
	 }
	 xioparms.bufsiz = Strtoul(a, (char **)&a, 0, "-b");
	 break;
      case 's':  if (arg1[0][2])  { socat_opt_hint(stderr, arg1[0][1], arg1[0][2]); Exit(1); }
	 diag_set_int('e', E_FATAL); break;
      case 'S': 	/* do not catch signals */
	 if (arg1[0][2]) {
	    a = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((a = *arg1) == NULL) {
	       Error("option -S requires an argument; use option \"-h\" for help");
	       Exit(1);
	    }
	 }
	 socat_opts.log_sigs = Strtoul(a, (char **)&a, 0, "-S");
	 break;
      case 't': if (arg1[0][2]) {
	    a = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((a = *arg1) == NULL) {
	       Error("option -t requires an argument; use option \"-h\" for help");
	       Exit(1);
	    }
	 }
	 rto = Strtod(a, (char **)&a, "-t");
	 socat_opts.closwait.tv_sec = rto;
	 socat_opts.closwait.tv_usec =
	    (rto-socat_opts.closwait.tv_sec) * 1000000;
	 break;
      case 'T':  if (arg1[0][2]) {
	    a = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((a = *arg1) == NULL) {
	       Error("option -T requires an argument; use option \"-h\" for help");
	       Exit(1);
	    }
	 }
	 rto = Strtod(a, (char **)&a, "-T");
	 if (rto < 0) {
	    socat_opts.total_timeout.tv_sec = 0; 	/* infinite */
	    socat_opts.total_timeout.tv_usec = 1000000;	/* by invalid */
	 } else {
	    socat_opts.total_timeout.tv_sec = rto;
	    socat_opts.total_timeout.tv_usec =
	       (rto-socat_opts.total_timeout.tv_sec) * 1000000;
	 }
	 xioparms.total_timeout.tv_sec  = socat_opts.total_timeout.tv_sec;
	 xioparms.total_timeout.tv_usec = socat_opts.total_timeout.tv_usec;
	 break;
      case 'u':  if (arg1[0][2])  { socat_opt_hint(stderr, arg1[0][1], arg1[0][2]); Exit(1); }
	 socat_opts.lefttoright = true; break;
      case 'U':  if (arg1[0][2])  { socat_opt_hint(stderr, arg1[0][1], arg1[0][2]); Exit(1); }
	 socat_opts.righttoleft = true; break;
      case 'g':  if (arg1[0][2])  { socat_opt_hint(stderr, arg1[0][1], arg1[0][2]); Exit(1); }
	 xioopts_ignoregroups = true; break;
      case 'L': if (socat_opts.lock.lockfile)
	     Error("only one -L and -W option allowed");
	 if (arg1[0][2]) {
	    socat_opts.lock.lockfile = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((socat_opts.lock.lockfile = *arg1) == NULL) {
	       Error("option -L requires an argument; use option \"-h\" for help");
	       Exit(1);
	    }
	 }
	 break;
      case 'W': if (socat_opts.lock.lockfile) {
	    Error("only one -L and -W option allowed");
	 }
	 if (arg1[0][2]) {
	    socat_opts.lock.lockfile = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((socat_opts.lock.lockfile = *arg1) == NULL) {
	       Error("option -W requires an argument; use option \"-h\" for help");
	       Exit(1);
	    }
	 }
	 socat_opts.lock.waitlock = true;
	 socat_opts.lock.intervall.tv_sec  = 1;
	 socat_opts.lock.intervall.tv_nsec = 0;
	 break;
#if WITH_IP4 || WITH_IP6
      case '0':
#if WITH_IP4
      case '4':
#endif
#if WITH_IP6
      case '6':
#endif
	 if (arg1[0][2])  { socat_opt_hint(stderr, arg1[0][1], arg1[0][2]); Exit(1); }
	 xioparms.default_ip = arg1[0][1];
	 xioparms.preferred_ip = arg1[0][1];
	 break;
#endif /* WITH_IP4 || WITH_IP6 */
      case '-':
	 if (!strcmp("experimental", &arg1[0][2])) {
	    xioparms.experimental = true;
	 } else if (!strcmp("statistics", &arg1[0][2])) {
	    socat_opts.statistics = true;
	 } else {
	    Error1("unknown option \"%s\"; use option \"-h\" for help", arg1[0]);
	 }
	 break;
      case '\0':
      case ',':
      case ':':
	 isdash = true;
	 break;	/* this "-" is a variation of STDIO */
      default:
	 xioinqopt('p', buff, sizeof(buff)); 	/* fetch pipe separator char */
	 if (arg1[0][1] == buff[0]) {
	    isdash = true;
	    break;
	 }
	 Error1("unknown option \"%s\"; use option \"-h\" for help", arg1[0]);
	 Exit(1);
      }
      if (isdash) {
	 /* the leading "-" is a form of the first address */
	 break;
      }
      ++arg1; --argc;
   }
   if (argc != 2) {
      Error1("exactly 2 addresses required (there are %d); use option \"-h\" for help", argc);
      Exit(1);
   }
   if (socat_opts.lefttoright && socat_opts.righttoleft) {
      Error("-U and -u must not be combined");
   }

   xioinitialize2();
   Info(copyright_socat);
#if WITH_OPENSSL
   Info(copyright_openssl);
   Info(copyright_ssleay);
#endif
   Debug2("socat version %s on %s", socatversion, timestamp);
   xiosetenv("VERSION", socatversion, 1, NULL);	/* SOCAT_VERSION */
   uname(&ubuf);	/* ! here we circumvent internal tracing (Uname) */
   Debug4("running on %s version %s, release %s, machine %s\n",
	   ubuf.sysname, ubuf.version, ubuf.release, ubuf.machine);

#if WITH_MSGLEVEL <= E_DEBUG
   for (i = 0; i < argc0; ++i) {
      Debug2("argv[%d]: \"%s\"", i, argv[i]);
   }
#endif /* WITH_MSGLEVEL <= E_DEBUG */

   {
#if HAVE_SIGACTION
      struct sigaction act;
#endif
      int i, m;

      sigfillset(&act.sa_mask); 	/* while in sighandler block all signals */
      act.sa_flags = 0;
      act.sa_handler = socat_signal;
      /* not sure which signals should be caught and print a message */
      for (i = 0, m = 1; i < 8*sizeof(unsigned long); ++i, m <<= 1) {
	 if (socat_opts.log_sigs & m) {
#if HAVE_SIGACTION
	    Sigaction(i,  &act, NULL);
#else
	    Signal(i, socat_signal);
#endif
	 }
      }

#if WITH_STATS
#if HAVE_SIGACTION
      act.sa_handler = socat_signal_logstats;
      Sigaction(SIGUSR1, &act, NULL);
#else
      Signal(SIGUSR1, socat_signal_logstats);
#endif
#endif /* WITH_STATS */
   }
   Signal(SIGPIPE, SIG_IGN);

   /* set xio hooks */
   xiohook_newchild = &socat_newchild;

   if (lockrc = socat_lock()) {
      /* =0: goon; >0: locked; <0: error, printed in sub */
      if (lockrc > 0)
	 Error1("could not obtain lock \"%s\"", socat_opts.lock.lockfile);
      Exit(1);
   }

   Atexit(socat_unlock);
#if WITH_STATS
   if (socat_opts.statistics) {
      Atexit(socat_print_stats);
   }
#endif /* WITH_STATS */

   /* Display important info, values may be set by:
      ./configure --enable-default-ipv=0|4|6
      env SOCAT_PREFERRED_RESOLVE_IP, SOCAT_DEFAULT_LISTEN_IP
      options -0 -4 -6  */
   Info1("default listen IP version is %c", xioparms.default_ip);
   Info1("preferred resolve IP version is %c", xioparms.preferred_ip);

   result = socat(arg1[0], arg1[1]);
   if (result == EXIT_SUCCESS && engine_result != EXIT_SUCCESS) {
      result = engine_result; 	/* a signal handler reports failure */
   }
   Notice1("exiting with status %d", result);
   Exit(result);
   return 0;	/* not reached, just for gcc -Wall */
}


void socat_usage(FILE *fd) {
   fputs(copyright_socat, fd); fputc('\n', fd);
   fputs("Usage:\n", fd);
   fputs("socat [options] <bi-address> <bi-address>\n", fd);
   fputs("   options (general command line options):\n", fd);
   fputs("      -V     print version and feature information to stdout, and exit\n", fd);
#if WITH_HELP
   fputs("      -h|-?  print a help text describing command line options and addresses\n", fd);
   fputs("      -hh    like -h, plus a list of all common address option names\n", fd);
   fputs("      -hhh   like -hh, plus a list of all available address option names\n", fd);
#endif /* WITH_HELP */
   fputs("      -d[ddd]        increase verbosity (use up to 4 times; 2 are recommended)\n", fd);
   fputs("      -d0|1|2|3|4    set verbosity level (0: Errors; 4 all including Debug)\n", fd);
#if WITH_FILAN
   fputs("      -D     analyze file descriptors before loop\n", fd);
#endif
   fputs("      --experimental enable experimental features\n", fd);
   fputs("      --statistics   output transfer statistics on exit\n", fd);
   fputs("      -ly[facility]  log to syslog, using facility (default is daemon)\n", fd);
   fputs("      -lf<logfile>   log to file\n", fd);
   fputs("      -ls            log to stderr (default if no other log)\n", fd);
   fputs("      -lm[facility]  mixed log mode (stderr during initialization, then syslog)\n", fd);
   fputs("      -lp<progname>  set the program name used for logging and vars\n", fd);
   fputs("      -lu            use microseconds for logging timestamps\n", fd);
   fputs("      -lh            add hostname to log messages\n", fd);
   fputs("      -v     verbose text dump of data traffic\n", fd);
   fputs("      -x     verbose hexadecimal dump of data traffic\n", fd);
   fputs("      -r <file>      raw dump of data flowing from left to right\n", fd);
   fputs("      -R <file>      raw dump of data flowing from right to left\n", fd);
   fputs("      -b<size_t>     set data buffer size (8192)\n", fd);
   fputs("      -s     sloppy (continue on error)\n", fd);
   fputs("      -S<sigmask>    log these signals, override default\n", fd);
   fputs("      -t<timeout>    wait seconds before closing second channel\n", fd);
   fputs("      -T<timeout>    total inactivity timeout in seconds\n", fd);
   fputs("      -u     unidirectional mode (left to right)\n", fd);
   fputs("      -U     unidirectional mode (right to left)\n", fd);
   fputs("      -g     do not check option groups\n", fd);
   fputs("      -L <lockfile>  try to obtain lock, or fail\n", fd);
   fputs("      -W <lockfile>  try to obtain lock, or wait\n", fd);
#if WITH_IP4 || WITH_IP6
   fputs("      -0     do not prefer an IP version\n", fd);
#endif
#if WITH_IP4
   fputs("      -4     prefer IPv4 if version is not explicitly specified\n", fd);
#endif
#if WITH_IP6
   fputs("      -6     prefer IPv6 if version is not explicitly specified\n", fd);
#endif
}

void socat_opt_hint(FILE *fd, char a, char b) {
   fprintf(fd, "Do not merge single character options, i.e. use \"-%c -%c\" instead of \"-%c%c\"\n",
	   a, b, a, b);
}


void socat_version(FILE *fd) {
   struct utsname ubuf;

   fputs(copyright_socat, fd); fputc('\n', fd);
   fprintf(fd, "socat version %s on %s\n", socatversion, timestamp);
   Uname(&ubuf);
   fprintf(fd, "   running on %s version %s, release %s, machine %s\n",
	   ubuf.sysname, ubuf.version, ubuf.release, ubuf.machine);
   fputs("features:\n", fd);
#ifdef WITH_HELP
   fprintf(fd, "  #define WITH_HELP %d\n", WITH_HELP);
#else
   fputs("  #undef WITH_HELP\n", fd);
#endif
#ifdef WITH_STATS
   fprintf(fd, "  #define WITH_STATS %d\n", WITH_STATS);
#else
   fputs("  #undef WITH_STATS\n", fd);
#endif
#ifdef WITH_STDIO
   fprintf(fd, "  #define WITH_STDIO %d\n", WITH_STDIO);
#else
   fputs("  #undef WITH_STDIO\n", fd);
#endif
#ifdef WITH_FDNUM
   fprintf(fd, "  #define WITH_FDNUM %d\n", WITH_FDNUM);
#else
   fputs("  #undef WITH_FDNUM\n", fd);
#endif
#ifdef WITH_FILE
   fprintf(fd, "  #define WITH_FILE %d\n", WITH_FILE);
#else
   fputs("  #undef WITH_FILE\n", fd);
#endif
#ifdef WITH_CREAT
   fprintf(fd, "  #define WITH_CREAT %d\n", WITH_CREAT);
#else
   fputs("  #undef WITH_CREAT\n", fd);
#endif
#ifdef WITH_GOPEN
   fprintf(fd, "  #define WITH_GOPEN %d\n", WITH_GOPEN);
#else
   fputs("  #undef WITH_GOPEN\n", fd);
#endif
#ifdef WITH_TERMIOS
   fprintf(fd, "  #define WITH_TERMIOS %d\n", WITH_TERMIOS);
#else
   fputs("  #undef WITH_TERMIOS\n", fd);
#endif
#ifdef WITH_PIPE
   fprintf(fd, "  #define WITH_PIPE %d\n", WITH_PIPE);
#else
   fputs("  #undef WITH_PIPE\n", fd);
#endif
#ifdef WITH_SOCKETPAIR
   fprintf(fd, "  #define WITH_SOCKETPAIR %d\n", WITH_SOCKETPAIR);
#else
   fputs("  #undef WITH_SOCKETPAIR\n", fd);
#endif
#ifdef WITH_UNIX
   fprintf(fd, "  #define WITH_UNIX %d\n", WITH_UNIX);
#else
   fputs("  #undef WITH_UNIX\n", fd);
#endif /* WITH_UNIX */
#ifdef WITH_ABSTRACT_UNIXSOCKET
   fprintf(fd, "  #define WITH_ABSTRACT_UNIXSOCKET %d\n", WITH_ABSTRACT_UNIXSOCKET);
#else
   fputs("  #undef WITH_ABSTRACT_UNIXSOCKET\n", fd);
#endif /* WITH_ABSTRACT_UNIXSOCKET */
#ifdef WITH_IP4
   fprintf(fd, "  #define WITH_IP4 %d\n", WITH_IP4);
#else
   fputs("  #undef WITH_IP4\n", fd);
#endif
#ifdef WITH_IP6
   fprintf(fd, "  #define WITH_IP6 %d\n", WITH_IP6);
#else
   fputs("  #undef WITH_IP6\n", fd);
#endif
#ifdef WITH_RAWIP
   fprintf(fd, "  #define WITH_RAWIP %d\n", WITH_RAWIP);
#else
   fputs("  #undef WITH_RAWIP\n", fd);
#endif
#ifdef WITH_GENERICSOCKET
   fprintf(fd, "  #define WITH_GENERICSOCKET %d\n", WITH_GENERICSOCKET);
#else
   fputs("  #undef WITH_GENERICSOCKET\n", fd);
#endif
#ifdef WITH_INTERFACE
   fprintf(fd, "  #define WITH_INTERFACE %d\n", WITH_INTERFACE);
#else
   fputs("  #undef WITH_INTERFACE\n", fd);
#endif
#ifdef WITH_TCP
   fprintf(fd, "  #define WITH_TCP %d\n", WITH_TCP);
#else
   fputs("  #undef WITH_TCP\n", fd);
#endif
#ifdef WITH_UDP
   fprintf(fd, "  #define WITH_UDP %d\n", WITH_UDP);
#else
   fputs("  #undef WITH_UDP\n", fd);
#endif
#ifdef WITH_SCTP
   fprintf(fd, "  #define WITH_SCTP %d\n", WITH_SCTP);
#else
   fputs("  #undef WITH_SCTP\n", fd);
#endif
#ifdef WITH_DCCP
   fprintf(fd, "  #define WITH_DCCP %d\n", WITH_DCCP);
#else
   fputs("  #undef WITH_DCCP\n", fd);
#endif
#ifdef WITH_UDPLITE
   fprintf(fd, "  #define WITH_UDPLITE %d\n", WITH_UDPLITE);
#else
   fputs("  #undef WITH_UDPLITE\n", fd);
#endif
#ifdef WITH_LISTEN
   fprintf(fd, "  #define WITH_LISTEN %d\n", WITH_LISTEN);
#else
   fputs("  #undef WITH_LISTEN\n", fd);
#endif
#ifdef WITH_POSIXMQ
   fprintf(fd, "  #define WITH_POSIXMQ %d\n", WITH_POSIXMQ);
#else
   fputs("  #undef WITH_POSIXMQ\n", fd);
#endif
#ifdef WITH_SOCKS4
   fprintf(fd, "  #define WITH_SOCKS4 %d\n", WITH_SOCKS4);
#else
   fputs("  #undef WITH_SOCKS4\n", fd);
#endif
#ifdef WITH_SOCKS4A
   fprintf(fd, "  #define WITH_SOCKS4A %d\n", WITH_SOCKS4A);
#else
   fputs("  #undef WITH_SOCKS4A\n", fd);
#endif
#ifdef WITH_SOCKS5
   fprintf(fd, "  #define WITH_SOCKS5 %d\n", WITH_SOCKS5);
#else
   fputs("  #undef WITH_SOCKS5\n", fd);
#endif
#ifdef WITH_VSOCK
   fprintf(fd, "  #define WITH_VSOCK %d\n", WITH_VSOCK);
#else
   fputs("  #undef WITH_VSOCK\n", fd);
#endif
#ifdef WITH_NAMESPACES
   fprintf(fd, "  #define WITH_NAMESPACES %d\n", WITH_NAMESPACES);
#else
   fputs("  #undef WITH_NAMESPACES\n", fd);
#endif
#ifdef WITH_PROXY
   fprintf(fd, "  #define WITH_PROXY %d\n", WITH_PROXY);
#else
   fputs("  #undef WITH_PROXY\n", fd);
#endif
#ifdef WITH_SYSTEM
   fprintf(fd, "  #define WITH_SYSTEM %d\n", WITH_SYSTEM);
#else
   fputs("  #undef WITH_SYSTEM\n", fd);
#endif
#ifdef WITH_SHELL
   fprintf(fd, "  #define WITH_SHELL %d\n", WITH_SHELL);
#else
   fputs("  #undef WITH_SHELL\n", fd);
#endif
#ifdef WITH_EXEC
   fprintf(fd, "  #define WITH_EXEC %d\n", WITH_EXEC);
#else
   fputs("  #undef WITH_EXEC\n", fd);
#endif
#ifdef WITH_READLINE
   fprintf(fd, "  #define WITH_READLINE %d\n", WITH_READLINE);
#else
   fputs("  #undef WITH_READLINE\n", fd);
#endif
#ifdef WITH_TUN
   fprintf(fd, "  #define WITH_TUN %d\n", WITH_TUN);
#else
   fputs("  #undef WITH_TUN\n", fd);
#endif
#ifdef WITH_PTY
   fprintf(fd, "  #define WITH_PTY %d\n", WITH_PTY);
#else
   fputs("  #undef WITH_PTY\n", fd);
#endif
#ifdef WITH_OPENSSL
   fprintf(fd, "  #define WITH_OPENSSL %d\n", WITH_OPENSSL);
#else
   fputs("  #undef WITH_OPENSSL\n", fd);
#endif
#ifdef WITH_FIPS
   fprintf(fd, "  #define WITH_FIPS %d\n", WITH_FIPS);
#else
   fputs("  #undef WITH_FIPS\n", fd);
#endif
#ifdef WITH_LIBWRAP
   fprintf(fd, "  #define WITH_LIBWRAP %d\n", WITH_LIBWRAP);
#else
   fputs("  #undef WITH_LIBWRAP\n", fd);
#endif
#ifdef WITH_SYCLS
   fprintf(fd, "  #define WITH_SYCLS %d\n", WITH_SYCLS);
#else
   fputs("  #undef WITH_SYCLS\n", fd);
#endif
#ifdef WITH_FILAN
   fprintf(fd, "  #define WITH_FILAN %d\n", WITH_FILAN);
#else
   fputs("  #undef WITH_FILAN\n", fd);
#endif
#ifdef WITH_RETRY
   fprintf(fd, "  #define WITH_RETRY %d\n", WITH_RETRY);
#else
   fputs("  #undef WITH_RETRY\n", fd);
#endif
#ifdef WITH_DEVTESTS
   fprintf(fd, "  #define WITH_DEVTESTS %d\n", WITH_DEVTESTS);
#else
   fputs("  #undef WITH_DEVTESTS\n", fd);
#endif
#ifdef WITH_MSGLEVEL
   fprintf(fd, "  #define WITH_MSGLEVEL %d /*%s*/\n", WITH_MSGLEVEL,
	   &"debug\0\0\0info\0\0\0\0notice\0\0warn\0\0\0\0error\0\0\0fatal\0\0\0"[WITH_MSGLEVEL<<3]);
#else
   fputs("  #undef WITH_MSGLEVEL\n", fd);
#endif
#ifdef WITH_DEFAULT_IPV
#  if WITH_DEFAULT_IPV
   fprintf(fd, "  #define WITH_DEFAULT_IPV %c\n", WITH_DEFAULT_IPV);
#  else
   fprintf(fd, "  #define WITH_DEFAULT_IPV '\\0'\n");
#  endif
#else
   fputs("  #undef WITH_DEFAULT_IPV\n", fd);
#endif
}


xiofile_t *sock1, *sock2;
int closing = 0;	/* 0..no eof yet, 1..first eof just occurred,
			   2..counting down closing timeout */
int sniffleft = -1; 		/* -1 or an FD for teeing data arriving on xfd1 */
int sniffright = -1; 	/* -1 or an FD for teeing data arriving on xfd2 */

/* call this function when the common command line options are parsed, and the
   addresses are extracted (but not resolved). */
int socat(const char *address1, const char *address2) {
   int mayexec;

   if (socat_opts.lefttoright) {
      if ((sock1 = xioopen(address1, XIO_RDONLY|XIO_MAYFORK|XIO_MAYCHILD|XIO_MAYCONVERT)) == NULL) {
	 return -1;
      }
      xiosetsigchild(sock1, socat_sigchild);
   } else if (socat_opts.righttoleft) {
      if ((sock1 = xioopen(address1, XIO_WRONLY|XIO_MAYFORK|XIO_MAYCHILD|XIO_MAYCONVERT)) == NULL) {
	 return -1;
      }
      xiosetsigchild(sock1, socat_sigchild);
   } else {
      if ((sock1 = xioopen(address1, XIO_RDWR|XIO_MAYFORK|XIO_MAYCHILD|XIO_MAYCONVERT)) == NULL) {
	 return -1;
      }
      xiosetsigchild(sock1, socat_sigchild);
   }
#if 1	/*! */
   if (XIO_READABLE(sock1) &&
       (XIO_RDSTREAM(sock1)->howtoend == END_KILL ||
	XIO_RDSTREAM(sock1)->howtoend == END_CLOSE_KILL ||
	XIO_RDSTREAM(sock1)->howtoend == END_SHUTDOWN_KILL)) {
      int i;
      for (i = 0; i < NUMUNKNOWN; ++i) {
	 if (XIO_RDSTREAM(sock1)->para.exec.pid == diedunknown[i]) {
	    /* Child has already died... but it might have put regular data into
	       the communication channel, so continue */
	    Info2("child "F_pid" has already died with status %d",
		  XIO_RDSTREAM(sock1)->para.exec.pid, statunknown[i]);
	    ++num_child; 	/* it was counted as anonymous child, undo */
	    if (statunknown[i] != 0) {
	       return 1;
	    }
	    diedunknown[i] = 0;
	    XIO_RDSTREAM(sock1)->para.exec.pid = 0;
	    /* return STAT_RETRYLATER; */
	 }
      }
   }
#endif

   mayexec = (sock1->common.flags&XIO_DOESCONVERT ? 0 : XIO_MAYEXEC);
   if (XIO_WRITABLE(sock1)) {
      if (XIO_READABLE(sock1)) {
	 if ((sock2 = xioopen(address2, XIO_RDWR|XIO_MAYFORK|XIO_MAYCHILD|mayexec|XIO_MAYCONVERT)) == NULL) {
	    return -1;
	 }
	 xiosetsigchild(sock2, socat_sigchild);
      } else {
	 if ((sock2 = xioopen(address2, XIO_RDONLY|XIO_MAYFORK|XIO_MAYCHILD|mayexec|XIO_MAYCONVERT)) == NULL) {
	    return -1;
	 }
	 xiosetsigchild(sock2, socat_sigchild);
      }
   } else {	/* assuming sock1 is readable */
      if ((sock2 = xioopen(address2, XIO_WRONLY|XIO_MAYFORK|XIO_MAYCHILD|mayexec|XIO_MAYCONVERT)) == NULL) {
	 return -1;
      }
      xiosetsigchild(sock2, socat_sigchild);
   }
#if 1	/*! */
   if (XIO_READABLE(sock2) &&
       (XIO_RDSTREAM(sock2)->howtoend == END_KILL ||
	XIO_RDSTREAM(sock2)->howtoend == END_CLOSE_KILL ||
	XIO_RDSTREAM(sock2)->howtoend == END_SHUTDOWN_KILL)) {
      int i;
      for (i = 0; i < NUMUNKNOWN; ++i) {
	 if (XIO_RDSTREAM(sock2)->para.exec.pid == diedunknown[i]) {
	    /* Child has already died... but it might have put regular data into
	       the communication channel, so continue */
	    Info2("child "F_pid" has already died with status %d",
		  XIO_RDSTREAM(sock2)->para.exec.pid, statunknown[i]);
	    if (statunknown[i] != 0) {
	       return 1;
	    }
	    diedunknown[i] = 0;
	    XIO_RDSTREAM(sock2)->para.exec.pid = 0;
	    /* return STAT_RETRYLATER; */
	 }
      }
   }
#endif

   Info("resolved and opened all sock addresses");
   return _socat();	/* nsocks, sockets are visible outside function */
}

/* checks if this is a connection to a child process, and if so, sees if the
   child already died, leaving some data for us.
   returns <0 if an error occurred;
   returns 0 if no child or not yet died or died without data (sets eof);
   returns >0 if child died and left data
*/
int childleftdata(xiofile_t *xfd) {
   struct pollfd in;
   int retval;

   /* have to check if a child process died before, but left read data */
   if (XIO_READABLE(xfd) &&
       (XIO_RDSTREAM(xfd)->howtoend == END_KILL ||
	XIO_RDSTREAM(xfd)->howtoend == END_CLOSE_KILL ||
	XIO_RDSTREAM(xfd)->howtoend == END_SHUTDOWN_KILL) &&
       XIO_RDSTREAM(xfd)->para.exec.pid == 0) {
      struct timeval timeout = { 0, 0 };

      if (XIO_RDSTREAM(xfd)->eof >= 2 && !XIO_RDSTREAM(xfd)->ignoreeof)
	 return 0;

      in.fd = XIO_GETRDFD(xfd);
      in.events = POLLIN/*|POLLRDBAND*/;
      in.revents = 0;
      do {
	 int _errno;
	 retval = xiopoll(&in, 1, &timeout);
	 _errno = errno; diag_flush(); errno = _errno;	/* just in case it's not debug level and Msg() not been called */
      } while (retval < 0 && errno == EINTR);

      if (retval < 0) {
	 Error5("xiopoll({%d,0%o}, 1, {"F_tv_sec"."F_tv_usec"}): %s",
		in.fd, in.events, timeout.tv_sec, timeout.tv_usec,
		strerror(errno));
	 return -1;
      }
      if (retval == 0) {
	 Info("terminated child did not leave data for us");
	 XIO_RDSTREAM(xfd)->eof = 2;
	 xfd->stream.eof = 2;
	 closing = MAX(closing, 1);
      }
   }
   return 0;
}

int xiotransfer(xiofile_t *inpipe, xiofile_t *outpipe,
		unsigned char *buff, size_t bufsiz, bool righttoleft);

bool mayrd1;		/* sock1 has read data or eof, according to poll() */
bool mayrd2;		/* sock2 has read data or eof, according to poll() */
bool maywr1;		/* sock1 can be written to, according to poll() */
bool maywr2;		/* sock2 can be written to, according to poll() */

/* here we come when the sockets are opened (in the meaning of C language),
   and their options are set/applied
   returns -1 on error or 0 on success */
int _socat(void) {
   char *transferwaitstring;
   struct pollfd fds[4],
       *fd1in  = &fds[0],
       *fd1out = &fds[1],
       *fd2in  = &fds[2],
       *fd2out = &fds[3];
   int retval;
   unsigned char *buff;
   ssize_t bytes1, bytes2;
   int polling = 0;	/* handling ignoreeof */
   int wasaction = 1;	/* last poll was active, do NOT sleep before next */
   struct timeval total_timeout;	/* the actual total timeout timer */

   {
      /* Open sniff file(s) */
      char name[PATH_MAX];
      struct timeval tv = { 0 }; 	/* 'cache' to have same time in both */

      if (xioinqopt('r', name, sizeof(name)) == 0) {
	 if (sniffleft >= 0)  Close(sniffleft);
	 sniffleft = xio_opensnifffile(name, &tv);
	 if (sniffleft < 0) {
	    Error2("option -r \"%s\": %s", name, strerror(errno));
         }
      }

      if (xioinqopt('R', name, sizeof(name)) == 0) {
	 if (sniffright >= 0)  Close(sniffright);
	 sniffright = xio_opensnifffile(name, &tv);
	 if (sniffright < 0) {
	    Error2("option -R \"%s\": %s", name, strerror(errno));
         }
      }
   }

#if WITH_FILAN
   if (socat_opts.debug) {
      int fdi, fdo;
      int msglevel, exitlevel;

      msglevel = diag_get_int('D');	/* save current message level */
      diag_set_int('D', E_ERROR);	/* only print errors and fatals in filan */
      exitlevel = diag_get_int('e');	/* save current exit level */
      diag_set_int('e', E_FATAL);	/* only exit on fatals */

      fdi = XIO_GETRDFD(sock1);
      fdo = XIO_GETWRFD(sock1);
      filan_fd(fdi, stderr);
      if (fdo != fdi) {
	 filan_fd(fdo, stderr);
      }

      fdi = XIO_GETRDFD(sock2);
      fdo = XIO_GETWRFD(sock2);
      filan_fd(fdi, stderr);
      if (fdo != fdi) {
	 filan_fd(fdo, stderr);
      }

      diag_set_int('e', exitlevel);	/* restore old exit level */
      diag_set_int('D', msglevel);	/* restore old message level */
   }
#endif /* WITH_FILAN */

   /* when converting nl to crnl, size might double */
   if (xioparms.bufsiz > (SIZE_MAX-1)/2) {
      Error2("buffer size option (-b) to big - "F_Zu" (max is "F_Zu")", xioparms.bufsiz, (SIZE_MAX-1)/2);
      xioparms.bufsiz = (SIZE_MAX-1)/2;
   }

#if HAVE_PROTOTYPE_LIB_posix_memalign
   /* Operations on files with flag O_DIRECT might need buffer alignment.
      Without this, eg.read() fails with "Invalid argument" */
   {
      int _errno;
      if ((_errno = Posix_memalign((void **)&buff, getpagesize(), 2*xioparms.bufsiz+1)) != 0) {
	 Error1("posix_memalign(): %s", strerror(_errno));
	 return -1;
      }
   }
#else /* !HAVE_PROTOTYPE_LIB_posix_memalign */
   buff = Malloc(2*xioparms.bufsiz+1);
   if (buff == NULL)  return -1;
#endif /* !HAVE_PROTOTYPE_LIB_posix_memalign */

   if (socat_opts.logopt == 'm' && xioinqopt('l', NULL, 0) == 'm') {
      Info("switching to syslog");
      diag_set('y', xioparms.syslogfac);
      xiosetopt('l', "\0");
   }
   total_timeout = socat_opts.total_timeout;

   if (transferwaitstring = getenv("SOCAT_TRANSFER_WAIT")) {
      Info1("before starting data transfer loop: sleeping %ds (env:SOCAT_TRANSFER_WAIT)", atoi(transferwaitstring));
      sleep(atoi(transferwaitstring));
   }
   Notice4("starting data transfer loop with FDs [%d,%d] and [%d,%d]",
	   XIO_GETRDFD(sock1), XIO_GETWRFD(sock1),
	   XIO_GETRDFD(sock2), XIO_GETWRFD(sock2));
   while (XIO_RDSTREAM(sock1)->eof <= 1 ||
	  XIO_RDSTREAM(sock2)->eof <= 1) {
      struct timeval timeout, *to = NULL;

      Debug6("data loop: sock1->eof=%d, sock2->eof=%d, closing=%d, wasaction=%d, total_to={"F_tv_sec"."F_tv_usec"}",
	     XIO_RDSTREAM(sock1)->eof, XIO_RDSTREAM(sock2)->eof,
	     closing, wasaction,
	     total_timeout.tv_sec, total_timeout.tv_usec);

      /* for ignoreeof */
      if (polling) {
	 if (!wasaction) {
	    if (socat_opts.total_timeout.tv_usec < 1000000) {
	       if (total_timeout.tv_usec < socat_opts.pollintv.tv_usec) {
		  total_timeout.tv_usec += 1000000;
		  total_timeout.tv_sec  -= 1;
	       }
	       total_timeout.tv_sec  -= socat_opts.pollintv.tv_sec;
	       total_timeout.tv_usec -= socat_opts.pollintv.tv_usec;
	       if (total_timeout.tv_sec < 0 ||
		   total_timeout.tv_sec == 0 && total_timeout.tv_usec < 0) {
		  Notice("inactivity timeout triggered");
		  free(buff);
		  return 0;
	       }
	    }

	 } else {
	    wasaction = 0;
	 }
      }

      if (polling) {
	 /* there is a ignoreeof poll timeout, use it */
	 timeout = socat_opts.pollintv;
	 to = &timeout;
      } else if (socat_opts.total_timeout.tv_usec < 1000000) {
	 /* there might occur a total inactivity timeout */
	 timeout = socat_opts.total_timeout;
	 to = &timeout;
      } else {
	 to = NULL;
      }

      if (closing>=1) {
	 /* first eof already occurred, start end timer */
	 timeout = socat_opts.pollintv;
	 to = &timeout;
	 closing = 2;
      }

      /* frame 1: set the poll parameters and loop over poll() EINTR) */
      do {	/* loop over poll() EINTR */
	 int _errno;

	 childleftdata(sock1);
	 childleftdata(sock2);

	 if (closing>=1) {
	    /* first eof already occurred, start end timer */
	    timeout = socat_opts.closwait;
	    to = &timeout;
	    closing = 2;
	 }

	 /* use the ignoreeof timeout if appropriate */
	 if (polling) {
	    if (closing == 0 ||
		(socat_opts.pollintv.tv_sec < timeout.tv_sec) ||
		((socat_opts.pollintv.tv_sec == timeout.tv_sec) &&
		 socat_opts.pollintv.tv_usec < timeout.tv_usec)) {
	       timeout = socat_opts.pollintv;
	    }
	 }

	 /* now the fds will be assigned */
	 if (XIO_READABLE(sock1) &&
	     !(XIO_RDSTREAM(sock1)->eof > 1 && !XIO_RDSTREAM(sock1)->ignoreeof) &&
	     !socat_opts.righttoleft) {
	    if (!mayrd1 && !(XIO_RDSTREAM(sock1)->eof > 1)) {
		fd1in->fd = XIO_GETRDFD(sock1);
		fd1in->events = POLLIN;
	    } else {
		fd1in->fd = -1;
	    }
	    if (!maywr2) {
		fd2out->fd = XIO_GETWRFD(sock2);
		fd2out->events = POLLOUT;
	    } else {
		fd2out->fd = -1;
	    }
	 } else {
	     fd1in->fd = -1;
	     fd2out->fd = -1;
	 }
	 if (XIO_READABLE(sock2) &&
	     !(XIO_RDSTREAM(sock2)->eof > 1 && !XIO_RDSTREAM(sock2)->ignoreeof) &&
	     !socat_opts.lefttoright) {
	    if (!mayrd2 && !(XIO_RDSTREAM(sock2)->eof > 1)) {
		fd2in->fd = XIO_GETRDFD(sock2);
		fd2in->events = POLLIN;
	    } else {
		fd2in->fd = -1;
	    }
	    if (!maywr1) {
		fd1out->fd = XIO_GETWRFD(sock1);
		fd1out->events = POLLOUT;
	    } else {
		fd1out->fd = -1;
	    }
	 } else {
	     fd1out->fd = -1;
	     fd2in->fd = -1;
	 }
	 /* frame 0: innermost part of the transfer loop: check FD status */
	 retval = xiopoll(fds, 4, to);
	 if (retval >= 0 || errno != EINTR) {
	    break;
	 }
	 _errno = errno;
	 Info1("poll(): %s", strerror(errno));
	 errno = _errno;
      } while (true);

      /* attention:
	 when an exec'd process sends data and terminates, it is unpredictable
	 whether the data or the sigchild arrives first.
	 */

      if (retval < 0) {
	 Error11("xiopoll({%d,0%o}{%d,0%o}{%d,0%o}{%d,0%o}, 4, {"F_tv_sec"."F_tv_usec"}): %s",
		 fds[0].fd, fds[0].events, fds[1].fd, fds[1].events,
		 fds[2].fd, fds[2].events, fds[3].fd, fds[3].events,
		 timeout.tv_sec, timeout.tv_usec, strerror(errno));
		  free(buff);
	    return -1;
      } else if (retval == 0) {
	 Info2("poll timed out (no data within %ld.%06ld seconds)",
	       closing>=1?socat_opts.closwait.tv_sec:socat_opts.total_timeout.tv_sec,
	       closing>=1?socat_opts.closwait.tv_usec:socat_opts.total_timeout.tv_usec);
	 if (polling && !wasaction) {
	    /* there was a ignoreeof poll timeout, use it */
	    polling = 0;	/*%%%*/
	    if (XIO_RDSTREAM(sock1)->ignoreeof) {
	       mayrd1 = 0;
	    }
	    if (XIO_RDSTREAM(sock2)->ignoreeof) {
	       mayrd2 = 0;
	    }
	 } else if (polling && wasaction) {
	    wasaction = 0;

	 } else if (socat_opts.total_timeout.tv_usec < 1000000) {
	    /* there was a total inactivity timeout */
	    Notice("inactivity timeout triggered");
		  free(buff);
	    return 0;
	 }

	 if (closing) {
	    break;
	 }
	 /* one possibility to come here is ignoreeof on some fd, but no EOF
	    and no data on any descriptor - this is no indication for end! */
	 continue;
      }

      if (XIO_READABLE(sock1) && XIO_GETRDFD(sock1) >= 0 &&
	  (fd1in->revents /*&(POLLIN|POLLHUP|POLLERR)*/)) {
	 if (fd1in->revents & POLLNVAL) {
	    /* this is what we find on Mac OS X when poll()'ing on a device or
	       named pipe. a read() might imm. return with 0 bytes, resulting
	       in a loop? */
	    Error1("poll(...[%d]: invalid request", fd1in->fd);
		  free(buff);
	    return -1;
	 }
	 mayrd1 = true;
      }
      if (XIO_READABLE(sock2) && XIO_GETRDFD(sock2) >= 0 &&
	  (fd2in->revents)) {
	 if (fd2in->revents & POLLNVAL) {
	    Error1("poll(...[%d]: invalid request", fd2in->fd);
		  free(buff);
	    return -1;
	 }
	 mayrd2 = true;
      }
      if (XIO_GETWRFD(sock1) >= 0 && fd1out->fd >= 0 && fd1out->revents) {
	 if (fd1out->revents & POLLNVAL) {
	    Error1("poll(...[%d]: invalid request", fd1out->fd);
		  free(buff);
	    return -1;
	 }
	 maywr1 = true;
      }
      if (XIO_GETWRFD(sock2) >= 0 && fd2out->fd >= 0 && fd2out->revents) {
	 if (fd2out->revents & POLLNVAL) {
	    Error1("poll(...[%d]: invalid request", fd2out->fd);
		  free(buff);
	    return -1;
	 }
	 maywr2 = true;
      }

      if (mayrd1 && maywr2) {
	 mayrd1 = false;
	 if ((bytes1 = xiotransfer(sock1, sock2, buff, xioparms.bufsiz, false))
	     < 0) {
	    if (errno != EAGAIN) {
	       closing = MAX(closing, 1);
	       Notice("socket 1 to socket 2 is in error");
	       if (socat_opts.lefttoright) {
		  break;
	       }
	    }
	 } else if (bytes1 > 0) {
	    maywr2 = false;
	    total_timeout = socat_opts.total_timeout;
	    wasaction = 1;
	    /* is more data available that has already passed poll()? */
	    mayrd1 = (xiopending(sock1) > 0);
	    if (XIO_RDSTREAM(sock1)->readbytes != 0 &&
		XIO_RDSTREAM(sock1)->actbytes == 0) {
	       /* avoid idle when all readbytes already there */
	       mayrd1 = true;
	    }
	    /* escape char occurred? */
	    if (XIO_RDSTREAM(sock1)->actescape) {
	       bytes1 = 0;	/* indicate EOF */
	    }
	 }
	 /* (bytes1 == 0)  handled later */
      } else {
	 bytes1 = -1;
      }

      if (mayrd2 && maywr1) {
	 mayrd2 = false;
	 if ((bytes2 = xiotransfer(sock2, sock1, buff, xioparms.bufsiz, true))
	     < 0) {
	    if (errno != EAGAIN) {
	       closing = MAX(closing, 1);
	       Notice("socket 2 to socket 1 is in error");
	       if (socat_opts.righttoleft) {
		  break;
	       }
	    }
	 } else if (bytes2 > 0) {
	    maywr1 = false;
	    total_timeout = socat_opts.total_timeout;
	    wasaction = 1;
	    /* is more data available that has already passed poll()? */
	    mayrd2 = (xiopending(sock2) > 0);
	    if (XIO_RDSTREAM(sock2)->readbytes != 0 &&
		XIO_RDSTREAM(sock2)->actbytes == 0) {
	       /* avoid idle when all readbytes already there */
	       mayrd2 = true;
	    }
	    /* escape char occurred? */
	    if (XIO_RDSTREAM(sock2)->actescape) {
	       bytes2 = 0;	/* indicate EOF */
	    }
	 }
	 /* (bytes2 == 0)  handled later */
      } else {
	 bytes2 = -1;
      }

      /* NOW handle EOFs */

      /*0 Debug4("bytes1=F_Zd, XIO_RDSTREAM(sock1)->eof=%d, XIO_RDSTREAM(sock1)->ignoreeof=%d, closing=%d",
	     bytes1, XIO_RDSTREAM(sock1)->eof, XIO_RDSTREAM(sock1)->ignoreeof,
	     closing);*/
      if (bytes1 == 0 || XIO_RDSTREAM(sock1)->eof >= 2) {
	 if (XIO_RDSTREAM(sock1)->ignoreeof &&
	     !XIO_RDSTREAM(sock1)->actescape && !closing) {
	    Debug1("socket 1 (fd %d) is at EOF, ignoring",
		   XIO_RDSTREAM(sock1)->fd);	/*! */
	    mayrd1 = true;
	    polling = 1;	/* do not hook this eof fd to poll for pollintv*/
	 } else if (XIO_RDSTREAM(sock1)->eof <= 2) {
	    Notice1("socket 1 (fd %d) is at EOF", XIO_GETRDFD(sock1));
	    xioshutdown(sock2, SHUT_WR);
	    XIO_RDSTREAM(sock1)->eof = 3;
	    XIO_RDSTREAM(sock1)->ignoreeof = false;
	 }
      } else if (polling && XIO_RDSTREAM(sock1)->ignoreeof) {
	 polling = 0;
      }
      if (XIO_RDSTREAM(sock1)->eof >= 2) {
	 if (socat_opts.lefttoright) {
	    break;
	 }
	 closing = 1;
      }

      if (bytes2 == 0 || XIO_RDSTREAM(sock2)->eof >= 2) {
	 if (XIO_RDSTREAM(sock2)->ignoreeof &&
	     !XIO_RDSTREAM(sock2)->actescape && !closing) {
	    Debug1("socket 2 (fd %d) is at EOF, ignoring",
		   XIO_RDSTREAM(sock2)->fd);
	    mayrd2 = true;
	    polling = 1;	/* do not hook this eof fd to poll for pollintv*/
	 } else if (XIO_RDSTREAM(sock2)->eof <= 2) {
	    Notice1("socket 2 (fd %d) is at EOF", XIO_GETRDFD(sock2));
	    xioshutdown(sock1, SHUT_WR);
	    XIO_RDSTREAM(sock2)->eof = 3;
	    XIO_RDSTREAM(sock2)->ignoreeof = false;
	 }
      } else if (polling && XIO_RDSTREAM(sock2)->ignoreeof) {
	 polling = 0;
      }
      if (XIO_RDSTREAM(sock2)->eof >= 2) {
	 if (socat_opts.righttoleft) {
	    break;
	 }
	 closing = 1;
      }
   }

   /* close everything that's still open */
   xioclose(sock1);
   xioclose(sock2);

   free(buff);
   return 0;
}


#define MAXTIMESTAMPLEN 128
/* prints the timestamp to the buffer and terminates it with '\0'. This buffer
   should be at least MAXTIMESTAMPLEN bytes long.
   returns 0 on success or -1 if an error occurred */
int gettimestamp(char *timestamp) {
#if HAVE_CLOCK_GETTIME
   struct timespec now;
#elif HAVE_PROTOTYPE_LIB_gettimeofday
   struct timeval now;
#endif /* !HAVE_PROTOTYPE_LIB_gettimeofday */
   time_t nowt;
   int result;

#if HAVE_CLOCK_GETTIME
   result = clock_gettime(CLOCK_REALTIME, &now);
   if (result < 0) {
      return result;
   }
   nowt = now.tv_sec;
#elif HAVE_PROTOTYPE_LIB_gettimeofday
   result = Gettimeofday(&now, NULL);
   if (result < 0) {
      return result;
   }
   nowt = now.tv_sec;
#else
   nowt = time(NULL);
   if (nowt == (time_t)-1) {
      return -1;
   }
#endif
#if HAVE_STRFTIME
   strftime(timestamp, 20, "%Y/%m/%d %H:%M:%S", localtime(&nowt));
#if HAVE_CLOCK_GETTIME
   sprintf(timestamp+19, "."F_tv_nsec" ", now.tv_nsec/1000);
#elif HAVE_PROTOTYPE_LIB_gettimeofday
   sprintf(timestamp+19, "."F_tv_usec" ", now.tv_usec);
#else
   strncpy(&timestamp[bytes++], " ", 2);
#endif
#else
   strcpy(timestamp, ctime(&nowt));
#endif
   return 0;
}

static const char *prefixltor = "> ";
static const char *prefixrtol = "< ";
static unsigned long numltor;
static unsigned long numrtol;
/* print block header (during verbose or hex dump)
   returns 0 on success or -1 if an error occurred */
static int
   xioprintblockheader(FILE *file, size_t bytes, bool righttoleft) {
   char timestamp[MAXTIMESTAMPLEN];
   char buff[128+MAXTIMESTAMPLEN];
   if (gettimestamp(timestamp) < 0) {
      return -1;
   }
   if (righttoleft) {
      sprintf(buff, "%s%s length="F_Zu" from=%lu to=%lu\n",
	      prefixrtol, timestamp, bytes, numrtol, numrtol+bytes-1);
      numrtol+=bytes;
   } else {
      sprintf(buff, "%s%s length="F_Zu" from=%lu to=%lu\n",
	      prefixltor, timestamp, bytes, numltor, numltor+bytes-1);
      numltor+=bytes;
   }
   fputs(buff, file);
   return 0;
}


/* inpipe is suspected to have read data available; read at most bufsiz bytes
   and transfer them to outpipe. Perform required data conversions.
   buff must be a malloc()'ed storage and might be realloc()'ed in this
   function if more space is required after conversions.
   Returns the number of bytes written, or 0 on EOF or <0 if an
   error occurred or when data was read but none written due to conversions
   (with EAGAIN). EAGAIN also occurs when reading from a nonblocking FD where
   the file has a mandatory lock.
   If 0 bytes were read (EOF), it does NOT shutdown or close a channel, and it
   does NOT write a zero bytes block.
   */
/* inpipe, outpipe must be single descriptors (not dual!) */
int xiotransfer(xiofile_t *inpipe, xiofile_t *outpipe,
		unsigned char *buff, size_t bufsiz, bool righttoleft) {
   ssize_t bytes, writt = 0;
   ssize_t sniffed;

	 bytes = xioread(inpipe, buff, bufsiz);
	 if (bytes < 0) {
	    if (errno != EAGAIN)
	       XIO_RDSTREAM(inpipe)->eof = 2;
	    /*xioshutdown(inpipe, SHUT_RD);*/
	    return -1;
	 }
	 if (bytes == 0 && XIO_RDSTREAM(inpipe)->ignoreeof && !closing) {
	    ;
	 } else if (bytes == 0) {
	    XIO_RDSTREAM(inpipe)->eof = 2;
	    closing = MAX(closing, 1);
	 }

	 if (bytes > 0) {
#if WITH_STATS
	    ++XIO_RDSTREAM(inpipe)->blocks_read;
	    XIO_RDSTREAM(inpipe)->bytes_read += bytes;
#endif
	    /* handle escape char */
	    if (XIO_RDSTREAM(inpipe)->escape != -1) {
	       /* check input data for escape char */
	       unsigned char *ptr = buff;
	       size_t ctr = 0;
	       while (ctr < bytes) {
		  if (*ptr == XIO_RDSTREAM(inpipe)->escape) {
		     /* found: set flag, truncate input data */
		     XIO_RDSTREAM(inpipe)->actescape = true;
		     bytes = ctr;
		     Info("escape char found in input");
		     break;
		  }
		  ++ptr; ++ctr;
	       }
	       if (ctr != bytes) {
		  XIO_RDSTREAM(inpipe)->eof = 2;
	       }
	    }
	 }

	    if (bytes > 0) {

	    if (XIO_RDSTREAM(inpipe)->lineterm !=
		XIO_WRSTREAM(outpipe)->lineterm) {
	       cv_newline(buff, &bytes,
			  XIO_RDSTREAM(inpipe)->lineterm,
			  XIO_WRSTREAM(outpipe)->lineterm);
	    }
	    if (bytes == 0) {
	       errno = EAGAIN;  return -1;
	    }

	    if (!righttoleft && sniffleft >= 0) {
	       if ((sniffed = Write(sniffleft, buff, bytes)) < bytes) {
		  if (sniffed < 0)
		     Warn3("-r: write(%d, buff, "F_Zu"): %s",
			   sniffleft, bytes, strerror(errno));
		  else if (sniffed < bytes)
		     Warn3("-r: write(%d, buff, "F_Zu") -> "F_Zd,
			   sniffleft, bytes, sniffed);
	       }
	    } else if (righttoleft && sniffright >= 0) {
	       if ((sniffed = Write(sniffright, buff, bytes)) < bytes) {
		  if (sniffed < 0)
		     Warn3("-R: write(%d, buff, "F_Zu"): %s",
			   sniffright, bytes, strerror(errno));
		  else if (sniffed < bytes)
		     Warn3("-R: write(%d, buff, "F_Zu") -> "F_Zd,
			   sniffright, bytes, sniffed);
	       }
	    }

	    if (socat_opts.verbose && socat_opts.verbhex) {
	       /* Hack-o-rama */
	       size_t i = 0;
	       size_t j;
	       size_t N = 16;
	       const unsigned char *end, *s, *t;
	       s = buff;
	       end = buff+bytes;
	       xioprintblockheader(stderr, bytes, righttoleft);
	       while (s < end) {
		  /*! prefix? */
		  j = Min(N, (size_t)(end-s));

		  /* print hex */
		  t = s;
		  i = 0;
		  while (i < j) {
		     int c = *t++;
		     fprintf(stderr, " %02x", c);
		     ++i;
		     if (c == '\n')  break;
		  }

		  /* fill hex column */
		  while (i < N) {
		     fputs("   ", stderr);
		     ++i;
		  }
		  fputs("  ", stderr);

		  /* print acsii */
		  t = s;
		  i = 0;
		  while (i < j) {
		     int c = *t++;
		     if (c == '\n') {
			fputc('.', stderr);
			break;
		     }
		     if (!isprint(c))
			c = '.';
		     fputc(c, stderr);
		     ++i;
		  }

		  fputc('\n', stderr);
		  s = t;
	       }
	       fputs("--\n", stderr);
	    } else if (socat_opts.verbose) {
	       size_t i = 0;
	       xioprintblockheader(stderr, bytes, righttoleft);
	       while (i < (size_t)bytes) {
		  int c = buff[i];
		  if (i > 0 && buff[i-1] == '\n')
		     /*! prefix? */;
		  switch (c) {
		  case '\a' : fputs("\\a", stderr); break;
		  case '\b' : fputs("\\b", stderr); break;
		  case '\t' : fputs("\t", stderr); break;
		  case '\n' : fputs("\n", stderr); break;
		  case '\v' : fputs("\\v", stderr); break;
		  case '\f' : fputs("\\f", stderr); break;
		  case '\r' : fputs("\\r", stderr); break;
		  case '\\' : fputs("\\\\", stderr); break;
		  default:
		     if (!isprint(c))
			c = '.';
		     fputc(c, stderr);
		     break;
		  }
		  ++i;
	       }
	    } else if (socat_opts.verbhex) {
	       int i;
	       /* print prefix */
	       xioprintblockheader(stderr, bytes, righttoleft);
	       for (i = 0; i < bytes; ++i) {
		  fprintf(stderr, " %02x", buff[i]);
	       }
	       fputc('\n', stderr);
	    }

	    writt = xiowrite(outpipe, buff, bytes);
	    if (writt < 0) {
	       /* EAGAIN when nonblocking but a mandatory lock is on file.
		  the problem with EAGAIN is that the read cannot be repeated,
		  so we need to buffer the data and try to write it later
		  again. not yet implemented, sorry. */
#if 0
	       if (errno == EPIPE) {
		  return 0;	/* can no longer write; handle like EOF */
	       }
#endif
	       return -1;
	    } else {
	       Info3("transferred "F_Zu" bytes from %d to %d",
		     writt, XIO_GETRDFD(inpipe), XIO_GETWRFD(outpipe));
#if WITH_STATS
	       ++XIO_WRSTREAM(outpipe)->blocks_written;
	       XIO_WRSTREAM(outpipe)->bytes_written += writt;
#endif
	    }
	 }
   return writt;
}

#define CR '\r'
#define LF '\n'


/* converts the newline characters (or character sequences) from the one
   specified in lineterm1 to that of lineterm2. Possible values are
   LINETERM_CR, LINETERM_CRNL, LINETERM_RAW.
   bytes specifies the number of bytes input and output */
int cv_newline(unsigned char *buff, ssize_t *bytes,
	       int lineterm1, int lineterm2) {
   /* must perform newline changes */
   if (lineterm1 <= LINETERM_CR && lineterm2 <= LINETERM_CR) {
      /* no change in data length */
      unsigned char from, to,  *p, *z;
      if (lineterm1 == LINETERM_RAW) {
	 from = '\n'; to = '\r';
      } else {
	 from = '\r'; to = '\n';
      }
      z = buff + *bytes;
      p = buff;
      while (p < z) {
	 if (*p == from)  *p = to;
	 ++p;
      }

   } else if (lineterm1 == LINETERM_CRNL) {
      /* buffer might become shorter */
      unsigned char to,  *s, *t, *z;
      if (lineterm2 == LINETERM_RAW) {
	 to = '\n';
      } else {
	 to = '\r';
      }
      z = buff + *bytes;
      s = t = buff;
      while (s < z) {
	 if (*s == '\r') {
	    ++s;
	    continue;
	 }
	 if (*s == '\n') {
	    *t++ = to; ++s;
	 } else {
	    *t++ = *s++;
	 }
      }
      *bytes = t - buff;
   } else {
      /* buffer becomes longer (up to double length), must alloc another space */
      static unsigned char *buf2;	/*! not threadsafe */
      unsigned char from;  unsigned char *s, *t, *z;

      if (lineterm1 == LINETERM_RAW) {
	 from = '\n';
      } else {
	 from = '\r';
      }
      if (buf2 == NULL) {
	 if ((buf2 = Malloc(xioparms.bufsiz)) == NULL) {
	    return -1;
	 }
      }
      memcpy(buf2, buff, *bytes);
      s = buf2;  t = buff;  z = buf2 + *bytes;
      while (s < z) {
	 if (*s == from) {
	    *t++ = '\r'; *t++ = '\n';
	    ++s;
	    continue;
	 } else {
	    *t++ = *s++;
	 }
      }
      *bytes = t - buff;;
   }
   return 0;
}

void socat_signal(int signum) {
   int _errno;
   _errno = errno;
   diag_in_handler = 1;
   Notice1("socat_signal(): handling signal %d", signum);
   switch (signum) {
   default:
      diag_immediate_exit = 1;
   case SIGQUIT:
   case SIGPIPE:
      diag_set_int('x', 128+signum);	/* in case Error exits for us */
      Error1("exiting on signal %d", signum);
      diag_set_int('x', 0);	/* in case Error did not exit */
      break;
   case SIGTERM:
      Warn1("exiting on signal %d", signum); break;
   case SIGHUP:
   case SIGINT:
      Notice1("exiting on signal %d", signum); break;
   }
   Notice1("socat_signal(): finishing signal %d", signum);
   diag_exit(128+signum);	/* internal cleanup + _exit() */
   diag_in_handler = 0;
   errno = _errno;
}

/* this is the callback when the child of an address died */
static int socat_sigchild(struct single *file) {
   if (file->ignoreeof && !closing) {
      ;
   } else {
      file->eof = MAX(file->eof, 1);
      closing = 1;
   }
   return 0;
}

static int socat_lock(void) {
   int lockrc;

#if 1
   if ((lockrc = xiolock(&socat_opts.lock)) < 0) {
      return -1;
   }
   if (lockrc == 0) {
      havelock = true;
   }
   return lockrc;
#else
   if (socat_opts.lock.lockfile) {
      if ((lockrc = xiolock(socat_opts.lock.lockfile)) < 0) {
	 /*Error1("error with lockfile \"%s\"", socat_opts.lock.lockfile);*/
	 return -1;
      }
      if (lockrc) {
	 return 1;
      }
      havelock = true;
      /*0 Info1("obtained lock \"%s\"", socat_opts.lock.lockfile);*/
   }

   if (socat_opts.lock.waitlock) {
      if (xiowaitlock(socat_opts.lock.waitlock, socat_opts.lock.intervall)) {
	 /*Error1("error with lockfile \"%s\"", socat_opts.lock.lockfile);*/
	 return -1;
      } else {
	 havelock = true;
	 /*0 Info1("obtained lock \"%s\"", socat_opts.lock.waitlock);*/
      }
   }
   return 0;
#endif
}

static void socat_unlock(void) {
   if (!havelock)  return;
   if (socat_opts.lock.lockfile) {
      if (Unlink(socat_opts.lock.lockfile) < 0) {
	 if (!diag_in_handler) {
	    Warn2("unlink(\"%s\"): %s",
	          socat_opts.lock.lockfile, strerror(errno));
	 } else {
	    Warn1("unlink(\"%s\"): "F_strerror,
	          socat_opts.lock.lockfile);
	 }
      } else {
	 Info1("released lock \"%s\"", socat_opts.lock.lockfile);
      }
   }
}

/* this is a callback function that may be called by the newchild hook of xio
 */
static int socat_newchild(void) {
   havelock = false;
   return 0;
}


#if WITH_STATS
void socat_signal_logstats(int signum) {
   diag_in_handler = 1;
   Notice1("socat_signal_logstats(): handling signal %d", signum);
   socat_print_stats();
   Notice1("socat_signal_logstats(): finishing signal %d", signum);
   diag_in_handler = 0;
}
#endif /* WITH_STATS */

#if WITH_STATS
static void socat_print_stats(void)
{
	const char ltorf0[] = "STATISTICS: left to right: %%%ullu packets(s), %%%ullu byte(s)";
	const char rtolf0[] = "STATISTICS: right to left: %%%ullu packets(s), %%%ullu byte(s)";
	char ltorf1[sizeof(ltorf0)];	/* final printf format with lengths of number */
	char rtolf1[sizeof(rtolf0)];	/* final printf format with lengths of number */
	unsigned int blocksd = 1, bytesd = 1;	/* number of digits in output */
	struct single *sock1w, *sock2w;
	int savelevel;

	if (sock1 == NULL || sock2 == NULL) {
		Warn("transfer engine not yet started, statistics not available");
		return;
	}
	if ((sock1->tag & ~XIO_TAG_CLOSED) == XIO_TAG_DUAL) {
		sock1w = sock1->dual.stream[1];
	} else {
		sock1w = &sock1->stream;
	}
	if ((sock2->tag & ~XIO_TAG_CLOSED) == XIO_TAG_DUAL) {
		sock2w = sock2->dual.stream[1];
	} else {
		sock2w = &sock2->stream;
	}
	if (!socat_opts.righttoleft && !socat_opts.righttoleft) {
		/* Both directions - format output */
		unsigned long long int maxblocks =
			Max(sock1w->blocks_written, sock2w->blocks_written);
		unsigned long long int maxbytes =
			Max(sock1w->bytes_written,  sock2w->bytes_written);
		/* Calculate number of digits */
		while (maxblocks >= 10) { ++blocksd; maxblocks /= 10; }
		while (maxbytes  >= 10) { ++bytesd;  maxbytes  /= 10; }
	}
	snprintf(ltorf1, sizeof(ltorf1), ltorf0, blocksd, bytesd);
	snprintf(rtolf1, sizeof(rtolf1), rtolf0, blocksd, bytesd);
	/* Statistics are E_INFO level; make sure they are printed anyway */
	savelevel = diag_get_int('d');
	diag_set_int('d', E_INFO);
	Warn("statistics are experimental");
	if (!socat_opts.righttoleft) {
		Info2(ltorf1, sock2w->blocks_written, sock2w->bytes_written);
	}
	if (!socat_opts.lefttoright) {
		Info2(rtolf1, sock1w->blocks_written, sock1w->bytes_written);
	}
	diag_set_int('d', savelevel);
	return;
}
#endif /* WITH_STATs */
