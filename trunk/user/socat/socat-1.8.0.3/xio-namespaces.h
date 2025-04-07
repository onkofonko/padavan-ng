/* Source: xio-namespaces.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_namespaces_h_included
#define __xio_namespaces_h_included 1

#if WITH_NAMESPACES

extern const struct optdesc opt_set_netns;
extern const struct optdesc opt_reset_netns;

extern int xio_set_namespace(const char *nstype, const char *nsname);
extern int xio_apply_namespace(struct opt *opts);
extern int xio_reset_namespace(int saved_netfd);

#endif /* WITH_NAMESPACES */

#endif /* __xio_namespaces_h_included */
