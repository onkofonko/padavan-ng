/* source: xiohelp.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for the help function */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xiohelp.h"

#if WITH_HELP

/* keep consistent with xioopts.h:enum e_types ! */
static const char *optiontypenames[] = {
	"CONST",	"BIN",		"BOOL",		"BYTE",
	"INT",		"INT/NULL",	"LONG",		"STRING",	"PTRDIFF",
	"SHORT",	"SIZE_T",	"SOCKADDR",	"UNSIGNED-INT",
	"UNSIGNED-LONG","UNSIGNED-SHORT","MODE_T",	"GID_T",
	"UID_T",	"INT[3]",	"STRUCT-TIMEVAL", "STRUCT-TIMESPEC",
	"DOUBLE",	"STRING-NULL",	"LONG-LONG",	"OFF_T",
	"OFF64_T",	"INT:INT",	"INT:INTP",	"INT:BIN",
	"INT:STRING",	"INT:INT:INT",	"INT:INT:BIN",	"INT:INT:STRING",
	"INT:INT:GENERIC",
	"IP4NAME",	"IP4SOCK",
#if HAVE_STRUCT_LINGER
			"STRUCT-LINGER",
#endif
#if HAVE_STRUCT_IP_MREQN
					"STRUCT-IP_MREQN",
#elif HAVE_STRUCT_IP_MREQ
					"STRUCT-IP_MREQ",
#endif
#if HAVE_STRUCT_IP_MREQ_SOURCE
	"IP-MREQ-SOURCE",
#endif
	"GENERIC",
} ;


/* keep consistent with xioopts.h:#define GROUP_* ! */
static const char *addressgroupnames[] = {
	"FD",		"FIFO",		"CHR",		"BLK",
	"REG",		"SOCKET",	"READLINE",	"undef",
	"NAMED",	"OPEN",		"EXEC",		"FORK",
	"LISTEN",	"SHELL",	"CHILD",	"RETRY",
	"TERMIOS",	"RANGE",	"PTY",		"PARENT",
	"UNIX",		"IP4",		"IP6",		"INTERFACE",
	"UDP",		"TCP",		"SOCKS",	"OPENSSL",
	"PROCESS",	"APPL",		"HTTP",		"undef",
	"POSIXMQ",	"SCTP",		"DCCP",		"UDPLITE"
} ;

/* keep consistent with xioopts.h:enum ephase ! */
static char *optionphasenames[] = {
	"ALL",		"OFFSET",	"INIT",		"EARLY",
	"PREOPEN",	"OPEN",		"PASTOPEN",
	"PRESOCKET",	"SOCKET",	"PASTSOCKET",
	"PREBIGEN",	"BIGEN",	"PASTBIGEN",
	"FD",
	"PREBIND",	"BIND",		"PASTBIND",
	"PRELISTEN",	"LISTEN",	"PASTLISTEN",
	"PRECONNECT",	"CONNECT",	"PASTCONNECT",
	"PREACCEPT",	"ACCEPT",	"PASTACCEPT",
	"CONNECTED",
	"PREFORK",	"FORK",		"PASTFORK",
	"LATE",		"LATE2",
	"PREEXEC",	"EXEC",		"PASTEXEC",
	"SPECIFIC",
	NULL
} ;


/* print a line about a single option */
static int xiohelp_option(FILE *of, const struct optname *on, const char *name) {
   int chars;
   int i, j;
   groups_t groups;
   bool occurred;

   chars = fprintf(of, "      %s", name);
   i = (16 - chars + 7) / 8;
   for (; i > 0; --i) { fputc('\t', of); }
   fputc('\t', of);

   fputs("groups=", of);
   groups = on->desc->group;
   occurred = false;
   chars = 7;
   if (groups == 0)
      fputs("(all)", of);
   for (j = 0; j < 8*sizeof(groups_t); ++j) {
      if (groups & 1) {
	 if (occurred) {
	    fputc(',', of);
	    ++chars;
	 }
	 fputs(addressgroupnames[j], of);
	 chars += strlen(addressgroupnames[j]);
	 occurred = true;
      }
      groups >>= 1;
   }
   i = (24 - chars + 7) / 8;
   for (; i > 0; --i) { fputc('\t', of); }

   chars = fprintf(of, "phase=%s", optionphasenames[on->desc->phase]);
   i = (24 - chars + 7) / 8;
   for (; i > 0; --i) { fputc('\t', of); }

   fprintf(of, "type=%s", optiontypenames[on->desc->type]);
   fputc('\n', of);
   return 0;
}

const char *xiohelp_opttypename(enum e_types typnum)
{
	if (typnum < 0 || typnum >= TYPE_OVERFLOW) {
		Warn2("%s(): invalid type number %d", __func__, typnum);
		return "<invalid>";
	}
	return optiontypenames[typnum];
}

int xioopenhelp(FILE *of,
	       int level	/* 0..only addresses, 1..and options */
	       ) {
   const struct addrname *an;
   const struct optname *on;
   int i, j;
   groups_t groups;
   bool occurred;

   fputs("   bi-address:  /* is an address that may act both as data sync and source */\n", of);
   fputs("      <single-address>\n", of);
   fputs("      <single-address>!!<single-address>\n", of);
   fputs("   single-address:\n", of);
   fputs("      <address-head>[,<opts>]\n", of);
   fputs("   address-head:\n", of);
   an = &addressnames[0];
   i = 0;
   while (addressnames[i].name) {
      if (!strcmp(an->name, an->desc->defname)) {
	 int chars, i;

         /* it is a canonical address name */
	 chars = fprintf(of, "      %s", an->name);
	 if (an->desc->syntax) {
	    fputs(an->desc->syntax, of);
	    chars += strlen(an->desc->syntax);
	 }
	 i = (40 - chars + 7) / 8;
	 for (; i > 0; --i) { fputc('\t', of); }
	 fputs("\tgroups=", of);
	 groups = an->desc->groups;
	 occurred = false;
	 for (j = 0; j < sizeof(groups_t)*8; ++j) {
	    if (groups & 1) {
	       if (occurred) { fputc(',', of); }
	       fprintf(of, "%s", addressgroupnames[j]);
	       occurred = true;
	    }
	    groups >>= 1;
	 }
	 fputc('\n', of);
      } else if (level == 2) {
	 int chars, i;

         chars = fprintf(of, "      %s", an->name);
	 i = (40 - chars + 7) / 8;
	 for (; i > 0; --i) { fputc('\t', of); }

         fprintf(of, "\tis an alias name for %s\n", an->desc->defname);
      }
      ++an; ++i;
   }
   if (level == 2) {
      fputs("         <num> is a short form for fd:<num>\n", of);
      fputs("         <filename> is a short form for gopen:<filename>\n", of);
   }

   if (level <= 0)  return 0;

   fputs("   opts:\n", of);
   fputs("      <opt>{,<opts>}:\n", of);
   fputs("   opt:\n", of);
   on = optionnames;
   while (on->name != NULL) {
      if (on->desc->nickname != NULL
	  && !strcmp(on->name, on->desc->nickname)) {
	 if (level == 2) {
	    int chars, i;

	    chars = fprintf(of, "      %s", on->name);
	    i = (16 - chars + 7) / 8;
	    for (; i > 0; --i) { fputc('\t', of); }

	    fprintf(of, "\tis an alias for %s\n", on->desc->defname);
	 } else {
	    xiohelp_option(of, on, on->name);
	 }
      } else if (on->desc->nickname == NULL &&
		 !strcmp(on->name, on->desc->defname)) {
	 xiohelp_option(of, on, on->name);
      } else if (level == 2) {
	 if (!strcmp(on->name, on->desc->defname)) {
	    xiohelp_option(of, on, on->name);
	 } else {
	    int chars, i;

	    chars = fprintf(of, "      %s", on->name);
	    i = (16 - chars + 7) / 8;
	    for (; i > 0; --i) { fputc('\t', of); }

	    fprintf(of, "\tis an alias for %s\n", on->desc->defname);
	 }
      }
      ++on;
   }
   fflush(of);
   return 0;
}

/* This function may be used by address handling code to log syntax error */
int xiohelp_syntax(
	const char *addr,
	int expectnum,
	int isnum,
	const char *syntax)
{
	Error5("%s: wrong number of parameters (%d instead of %d): usage: %s%s",
	       addr, isnum, expectnum, addr, syntax);
	return -1;
}

#endif /* WITH_HELP */
