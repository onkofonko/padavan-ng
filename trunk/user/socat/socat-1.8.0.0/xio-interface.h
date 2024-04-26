/* source: xio-interface.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_interface_h_included
#define __xio_interface_h_included 1

extern const struct addrdesc xioaddr_interface;

extern const struct optdesc opt_interface_addr;
extern const struct optdesc opt_interface_netmask;
extern const struct optdesc opt_iff_up;
extern const struct optdesc opt_iff_broadcast;
extern const struct optdesc opt_iff_debug;
extern const struct optdesc opt_iff_loopback;
extern const struct optdesc opt_iff_pointopoint;
extern const struct optdesc opt_iff_notrailers;
extern const struct optdesc opt_iff_running;
extern const struct optdesc opt_iff_noarp;
extern const struct optdesc opt_iff_promisc;
extern const struct optdesc opt_iff_allmulti;
extern const struct optdesc opt_iff_master;
extern const struct optdesc opt_iff_slave;
extern const struct optdesc opt_iff_multicast;
extern const struct optdesc opt_iff_portsel;
extern const struct optdesc opt_iff_automedia;
/*extern const struct optdesc opt_iff_dynamic;*/
extern const struct optdesc opt_retrieve_vlan;

extern int xiolog_ancillary_packet(struct single *sfd, struct cmsghdr *cmsg, int *num, char *typbuff, int typlen, char *nambuff, int namlen, char *envbuff, int envlen, char *valbuff, int vallen);

extern int _interface_retrieve_vlan(struct single *fd, struct opt *opts);
extern int _xiointerface_get_iff(int sockfd, const char *name, short *save_iff);
extern int _xiointerface_set_iff(int sockfd, const char *name, 	short new_iff);
extern int _xiointerface_apply_iff(int sockfd, const char *name, short iff_opts[2]);
extern int _interface_setsockopt_auxdata(int fd, int auxdata);

#endif /* !defined(__xio_interface_h_included) */
