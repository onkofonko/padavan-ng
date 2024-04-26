/* source: xiohelp.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xiohelp_h_included
#define __xiohelp_h_included 1

extern const char *xiohelp_opttypename(enum e_types typnum);

extern int xioopenhelp(FILE *of,
	       int level	/* 0..only addresses, 1..and options */
	       );

extern int xiohelp_syntax(const char *addr, int expectnum, int isnum, const char *syntax);

#endif /* !defined(__xiohelp_h_included) */
