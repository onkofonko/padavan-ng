/* source: xioparam.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for xio options handling */

#include "xiosysincludes.h"
#include "xioopen.h"

/*#include "xioparam.h" are all in xio.h */

/* options that can be applied to this module */
xioparms_t xioparms = {
   false,	/* strictopts */
   "!!",	/* pipesep */
   ":",		/* paramsep */
   ",",		/* optionsep */
   ':',		/* ip4portsep */
   ':',		/* ip6portsep */
   '\0',	/* logopt */
   NULL,	/* syslogfac */
   WITH_DEFAULT_IPV, 	/* default_ip */
   WITH_DEFAULT_IPV, 	/* preferred_ip */
   false, 	/* experimental */
   NULL, 	/* sniffleft_name */
   NULL, 	/* sniffright_name */
   8192 	/* bufsiz */
} ;


/* allow application to set xioopen options */
int xiosetopt(char what, const char *arg) {
   switch (what) {
   case 's': xioparms.strictopts = true; break;
   case 'p': if ((xioparms.pipesep = strdup(arg)) == NULL) {
	 Error1("strdup("F_Zu"): out of memory", strlen(arg)+1);
         return -1;
      }
      break;
   case 'o': xioparms.ip4portsep = arg[0];
      if (arg[1] != '\0') {
	 Error2("xiosetopt('%c', \"%s\"): port separator must be single character",
		what, arg);
	 return -1;
      }
      break;
   case 'l': xioparms.logopt = *arg; break;
   case 'y': xioparms.syslogfac = arg; break;
   case 'r': xioparms.sniffleft_name = arg; break;
   case 'R': xioparms.sniffright_name = arg; break;
   default:
      Error2("xiosetopt('%c', \"%s\"): unknown option",
	     what, arg?arg:"NULL");
      return -1;
   }
   return 0;
}


int xioinqopt(char what, char *arg, size_t n) {
   switch (what) {
   case 's': return xioparms.strictopts;
   case 'p':
      arg[0] = '\0'; strncat(arg, xioparms.pipesep, n-1);
      return 0;
   case 'o': return xioparms.ip4portsep;
   case 'l': return xioparms.logopt;
   case 'r':
      if (xioparms.sniffleft_name == NULL) {
	 return 1;
      }
      if (n < strlen(xioparms.sniffleft_name)+1) {
	 return -1;
      }
      arg[0] = '\0';
      strncat(arg, xioparms.sniffleft_name, n-1);
      return 0;
   case 'R':
      if (xioparms.sniffright_name == NULL) {
	 return 1;
      }
      if (n < strlen(xioparms.sniffright_name)+1) {
	 return -1;
      }
      arg[0] = '\0';
      strncat(arg, xioparms.sniffright_name, n-1);
      return 0;
   default:
      Error3("xioinqopt('%c', \"%s\", "F_Zu"): unknown option",
	     what, arg, n);
      return -1;
   }
   return 0;
}
