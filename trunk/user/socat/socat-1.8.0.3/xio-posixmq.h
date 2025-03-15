/* Source: xio-posixmq.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_posixmq_h_included
#define __xio_posixmq_h_included 1

extern const struct addrdesc xioaddr_posixmq_bidir;
extern const struct addrdesc xioaddr_posixmq_read;
extern const struct addrdesc xioaddr_posixmq_receive;
extern const struct addrdesc xioaddr_posixmq_send;
extern const struct addrdesc xioaddr_posixmq_write;

extern const struct optdesc opt_posixmq_maxmsg;
extern const struct optdesc opt_posixmq_msgsize;
extern const struct optdesc opt_posixmq_priority;
extern const struct optdesc opt_posixmq_flush;

extern ssize_t xioread_posixmq(struct single *file, void *buff, size_t bufsiz);
extern ssize_t xiopending_posixmq(struct single *pipe);
extern ssize_t xiowrite_posixmq(struct single *file, const void *buff, size_t bufsiz);
extern ssize_t xioclose_posixmq(struct single *sfd);

#endif /* !defined(__xio_posixmq_h_included) */
