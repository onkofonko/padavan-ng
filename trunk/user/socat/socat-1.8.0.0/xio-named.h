/* source: xio-named.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_named_h_included
#define __xio_named_h_included 1

extern const struct optdesc opt_group_early;
extern const struct optdesc opt_perm_early;
extern const struct optdesc opt_user_early;
/*0 extern const struct optdesc opt_cleanup; */
/*0 extern const struct optdesc opt_force; */
extern const struct optdesc opt_unlink;
extern const struct optdesc opt_unlink_early;
extern const struct optdesc opt_unlink_late;
extern const struct optdesc opt_unlink_close;

extern int
   applyopts_named(const char *filename, struct opt *opts, unsigned int phase);
extern int _xioopen_named_early(int argc, const char *argv[], xiofile_t *xfd, groups_t groups, bool *exists, struct opt *opts, const char *syntax);
extern int _xioopen_open(const char *path, int rw, struct opt *opts);

extern int xio_unlink(const char *pathname, int level);

#endif /* !defined(__xio_named_h_included) */
