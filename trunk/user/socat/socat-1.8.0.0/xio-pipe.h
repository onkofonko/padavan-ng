/* source: xio-pipe.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_pipe_h_included
#define __xio_pipe_h_included 1

extern const struct addrdesc xioaddr_pipe;

extern const struct optdesc opt_f_setpipe_sz;

extern int xio_chk_pipesz(int fd);

#endif /* !defined(__xio_pipe_h_included) */
