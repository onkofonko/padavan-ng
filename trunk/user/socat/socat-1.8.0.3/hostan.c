/* source: hostan.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* the subroutine hostan makes a "HOST ANalysis". It gathers information
   about the host environment it is running in without modifying its state
   (almost).
 */

#include "xiosysincludes.h"
#include "mytypes.h"
#include "compat.h"
#include "error.h"
#include "sycls.h"
#include "sysutils.h"
#include "filan.h"

#include "hostan.h"


static int iffan(FILE *outfile);
static int vsockan(FILE *outfile);


int hostan(FILE *outfile) {
   fprintf(outfile, "\nC TYPE SIZES\n");
   fprintf(outfile, "sizeof(char)      = %u\n", (unsigned int)sizeof(char));
   fprintf(outfile, "sizeof(short)     = %u\n", (unsigned int)sizeof(short));
   fprintf(outfile, "sizeof(int)       = %u\n", (unsigned int)sizeof(int));
   fprintf(outfile, "sizeof(long)      = %u\n", (unsigned int)sizeof(long));
#if HAVE_TYPE_LONGLONG
   fprintf(outfile, "sizeof(long long) = %u\n", (unsigned int)sizeof(long long));
#endif
   fprintf(outfile, "sizeof(size_t)    = %u\n", (unsigned int)sizeof(size_t));

   if ((char)-1 > 0)
      fprintf(outfile, "typedef unsigned char       char;\n");
   else
      fprintf(outfile, "typedef signed char         char;\n");

#  if HAVE_BASIC_SIZE_T==2
   fprintf(outfile, "typedef unsigned short      size_t;     /* sizeof(size_t) = %u */\n", (unsigned int)sizeof(size_t));
#elif HAVE_BASIC_SIZE_T==4
   fprintf(outfile, "typedef unsigned int        size_t;     /* sizeof(size_t) = %u */\n", (unsigned int)sizeof(size_t));
#elif HAVE_BASIC_SIZE_T==6
   fprintf(outfile, "typedef unsigned long       size_t;     /* sizeof(size_t) = %u */\n", (unsigned int)sizeof(size_t));
#elif HAVE_BASIC_SIZE_T==8
   fprintf(outfile, "typedef unsigned long long  size_t;     /* sizeof(size_t) = %u */\n", (unsigned int)sizeof(size_t));
#endif

#  if HAVE_BASIC_MODE_T==1
   fprintf(outfile, "typedef          short      mode_t;     /* sizeof(mode_t) = %u */\n", (unsigned int)sizeof(mode_t));
#elif HAVE_BASIC_MODE_T==2
   fprintf(outfile, "typedef unsigned short      mode_t;     /* sizeof(mode_t) = %u */\n", (unsigned int)sizeof(mode_t));
#elif HAVE_BASIC_MODE_T==3
   fprintf(outfile, "typedef          int        mode_t;     /* sizeof(mode_t) = %u */\n", (unsigned int)sizeof(mode_t));
#elif HAVE_BASIC_MODE_T==4
   fprintf(outfile, "typedef unsigned int        mode_t;     /* sizeof(mode_t) = %u */\n", (unsigned int)sizeof(mode_t));
#elif HAVE_BASIC_MODE_T==5
   fprintf(outfile, "typedef          long       mode_t;     /* sizeof(mode_t) = %u */\n", (unsigned int)sizeof(mode_t));
#elif HAVE_BASIC_MODE_T==6
   fprintf(outfile, "typedef unsigned long       mode_t;     /* sizeof(mode_t) = %u */\n", (unsigned int)sizeof(mode_t));
#elif HAVE_BASIC_MODE_T==7
   fprintf(outfile, "typedef          long long  mode_t;     /* sizeof(mode_t) = %u */\n", (unsigned int)sizeof(mode_t));
#elif HAVE_BASIC_MODE_T==8
   fprintf(outfile, "typedef unsigned long long  mode_t;     /* sizeof(mode_t) = %u */\n", (unsigned int)sizeof(mode_t));
#endif

#  if HAVE_BASIC_PID_T==1
   fprintf(outfile, "typedef          short      pid_t;      /* sizeof(pid_t) = %u */\n", (unsigned int)sizeof(pid_t));
#elif HAVE_BASIC_PID_T==2
   fprintf(outfile, "typedef unsigned short      pid_t;      /* sizeof(pid_t) = %u */\n", (unsigned int)sizeof(pid_t));
#elif HAVE_BASIC_PID_T==3
   fprintf(outfile, "typedef          int        pid_t;      /* sizeof(pid_t) = %u */\n", (unsigned int)sizeof(pid_t));
#elif HAVE_BASIC_PID_T==4
   fprintf(outfile, "typedef unsigned int        pid_t;      /* sizeof(pid_t) = %u */\n", (unsigned int)sizeof(pid_t));
#elif HAVE_BASIC_PID_T==5
   fprintf(outfile, "typedef          long       pid_t;      /* sizeof(pid_t) = %u */\n", (unsigned int)sizeof(pid_t));
#elif HAVE_BASIC_PID_T==6
   fprintf(outfile, "typedef unsigned long       pid_t;      /* sizeof(pid_t) = %u */\n", (unsigned int)sizeof(pid_t));
#elif HAVE_BASIC_PID_T==7
   fprintf(outfile, "typedef          long long  pid_t;      /* sizeof(pid_t) = %u */\n", (unsigned int)sizeof(pid_t));
#elif HAVE_BASIC_PID_T==8
   fprintf(outfile, "typedef unsigned long long  pid_t;      /* sizeof(pid_t) = %u */\n", (unsigned int)sizeof(pid_t));
#endif

#  if HAVE_BASIC_UID_T==1
   fprintf(outfile, "typedef          short      uid_t;      /* sizeof(uid_t) = %u */\n", (unsigned int)sizeof(uid_t));
#elif HAVE_BASIC_UID_T==2
   fprintf(outfile, "typedef unsigned short      uid_t;      /* sizeof(uid_t) = %u */\n", (unsigned int)sizeof(uid_t));
#elif HAVE_BASIC_UID_T==3
   fprintf(outfile, "typedef          int        uid_t;      /* sizeof(uid_t) = %u */\n", (unsigned int)sizeof(uid_t));
#elif HAVE_BASIC_UID_T==4
   fprintf(outfile, "typedef unsigned int        uid_t;      /* sizeof(uid_t) = %u */\n", (unsigned int)sizeof(uid_t));
#elif HAVE_BASIC_UID_T==5
   fprintf(outfile, "typedef          long       uid_t;      /* sizeof(uid_t) = %u */\n", (unsigned int)sizeof(uid_t));
#elif HAVE_BASIC_UID_T==6
   fprintf(outfile, "typedef unsigned long       uid_t;      /* sizeof(uid_t) = %u */\n", (unsigned int)sizeof(uid_t));
#elif HAVE_BASIC_UID_T==7
   fprintf(outfile, "typedef          long long  uid_t;      /* sizeof(uid_t) = %u */\n", (unsigned int)sizeof(uid_t));
#elif HAVE_BASIC_UID_T==8
   fprintf(outfile, "typedef unsigned long long  uid_t;      /* sizeof(uid_t) = %u */\n", (unsigned int)sizeof(uid_t));
#endif

#  if HAVE_BASIC_GID_T==1
   fprintf(outfile, "typedef          short      gid_t;      /* sizeof(gid_t) = %u */\n", (unsigned int)sizeof(gid_t));
#elif HAVE_BASIC_GID_T==2
   fprintf(outfile, "typedef unsigned short      gid_t;      /* sizeof(gid_t) = %u */\n", (unsigned int)sizeof(gid_t));
#elif HAVE_BASIC_GID_T==3
   fprintf(outfile, "typedef          int        gid_t;      /* sizeof(gid_t) = %u */\n", (unsigned int)sizeof(gid_t));
#elif HAVE_BASIC_GID_T==4
   fprintf(outfile, "typedef unsigned int        gid_t;      /* sizeof(gid_t) = %u */\n", (unsigned int)sizeof(gid_t));
#elif HAVE_BASIC_GID_T==5
   fprintf(outfile, "typedef          long       gid_t;      /* sizeof(gid_t) = %u */\n", (unsigned int)sizeof(gid_t));
#elif HAVE_BASIC_GID_T==6
   fprintf(outfile, "typedef unsigned long       gid_t;      /* sizeof(gid_t) = %u */\n", (unsigned int)sizeof(gid_t));
#elif HAVE_BASIC_GID_T==7
   fprintf(outfile, "typedef          long long  gid_t;      /* sizeof(gid_t) = %u */\n", (unsigned int)sizeof(gid_t));
#elif HAVE_BASIC_GID_T==8
   fprintf(outfile, "typedef unsigned long long  gid_t;      /* sizeof(gid_t) = %u */\n", (unsigned int)sizeof(gid_t));
#endif

#  if HAVE_BASIC_TIME_T==1
   fprintf(outfile, "typedef          short      time_t;     /* sizeof(time_t) = %u */\n", (unsigned int)sizeof(time_t));
#elif HAVE_BASIC_TIME_T==2
   fprintf(outfile, "typedef unsigned short      time_t;     /* sizeof(time_t) = %u */\n", (unsigned int)sizeof(time_t));
#elif HAVE_BASIC_TIME_T==3
   fprintf(outfile, "typedef          int        time_t;     /* sizeof(time_t) = %u */\n", (unsigned int)sizeof(time_t));
#elif HAVE_BASIC_TIME_T==4
   fprintf(outfile, "typedef unsigned int        time_t;     /* sizeof(time_t) = %u */\n", (unsigned int)sizeof(time_t));
#elif HAVE_BASIC_TIME_T==5
   fprintf(outfile, "typedef          long       time_t;     /* sizeof(time_t) = %u */\n", (unsigned int)sizeof(time_t));
#elif HAVE_BASIC_TIME_T==6
   fprintf(outfile, "typedef unsigned long       time_t;     /* sizeof(time_t) = %u */\n", (unsigned int)sizeof(time_t));
#elif HAVE_BASIC_TIME_T==7
   fprintf(outfile, "typedef          long long  time_t;     /* sizeof(time_t) = %u */\n", (unsigned int)sizeof(time_t));
#elif HAVE_BASIC_TIME_T==8
   fprintf(outfile, "typedef unsigned long long  time_t;     /* sizeof(time_t) = %u */\n", (unsigned int)sizeof(time_t));
#endif

#  if HAVE_BASIC_SOCKLEN_T==1
   fprintf(outfile, "typedef          short      socklen_t;  /* sizeof(socklen_t) = %u */\n", (unsigned int)sizeof(socklen_t));
#elif HAVE_BASIC_SOCKLEN_T==2
   fprintf(outfile, "typedef unsigned short      socklen_t;  /* sizeof(socklen_t) = %u */\n", (unsigned int)sizeof(socklen_t));
#elif HAVE_BASIC_SOCKLEN_T==3
   fprintf(outfile, "typedef          int        socklen_t;  /* sizeof(socklen_t) = %u */\n", (unsigned int)sizeof(socklen_t));
#elif HAVE_BASIC_SOCKLEN_T==4
   fprintf(outfile, "typedef unsigned int        socklen_t;  /* sizeof(socklen_t) = %u */\n", (unsigned int)sizeof(socklen_t));
#elif HAVE_BASIC_SOCKLEN_T==5
   fprintf(outfile, "typedef          long       socklen_t;  /* sizeof(socklen_t) = %u */\n", (unsigned int)sizeof(socklen_t));
#elif HAVE_BASIC_SOCKLEN_T==6
   fprintf(outfile, "typedef unsigned long       socklen_t;  /* sizeof(socklen_t) = %u */\n", (unsigned int)sizeof(socklen_t));
#elif HAVE_BASIC_SOCKLEN_T==7
   fprintf(outfile, "typedef          long long  socklen_t;  /* sizeof(socklen_t) = %u */\n", (unsigned int)sizeof(socklen_t));
#elif HAVE_BASIC_SOCKLEN_T==8
   fprintf(outfile, "typedef unsigned long long  socklen_t;  /* sizeof(socklen_t) = %u */\n", (unsigned int)sizeof(socklen_t));
#endif

#  if HAVE_BASIC_OFF_T==1
   fprintf(outfile, "typedef          short      off_t;      /* sizeof(off_t) = %u */\n", (unsigned int)sizeof(off_t));
#elif HAVE_BASIC_OFF_T==2
   fprintf(outfile, "typedef unsigned short      off_t;      /* sizeof(off_t) = %u */\n", (unsigned int)sizeof(off_t));
#elif HAVE_BASIC_OFF_T==3
   fprintf(outfile, "typedef          int        off_t;      /* sizeof(off_t) = %u */\n", (unsigned int)sizeof(off_t));
#elif HAVE_BASIC_OFF_T==4
   fprintf(outfile, "typedef unsigned int        off_t;      /* sizeof(off_t) = %u */\n", (unsigned int)sizeof(off_t));
#elif HAVE_BASIC_OFF_T==5
   fprintf(outfile, "typedef          long       off_t;      /* sizeof(off_t) = %u */\n", (unsigned int)sizeof(off_t));
#elif HAVE_BASIC_OFF_T==6
   fprintf(outfile, "typedef unsigned long       off_t;      /* sizeof(off_t) = %u */\n", (unsigned int)sizeof(off_t));
#elif HAVE_BASIC_OFF_T==7
   fprintf(outfile, "typedef          long long  off_t;      /* sizeof(off_t) = %u */\n", (unsigned int)sizeof(off_t));
#elif HAVE_BASIC_OFF_T==8
   fprintf(outfile, "typedef unsigned long long  off_t;      /* sizeof(off_t) = %u */\n", (unsigned int)sizeof(off_t));
#endif

#if HAVE_TYPE_OFF64 && defined(HAVE_BASIC_OFF64_T) && HAVE_BASIC_OFF64_T
#  if HAVE_BASIC_OFF64_T==1
   fprintf(outfile, "typedef          short      off64_t;    /* sizeof(off64_t) = %u */\n", (unsigned int)sizeof(off64_t));
#elif HAVE_BASIC_OFF64_T==2
   fprintf(outfile, "typedef unsigned short      off64_t;    /* sizeof(off64_t) = %u */\n", (unsigned int)sizeof(off64_t));
#elif HAVE_BASIC_OFF64_T==3
   fprintf(outfile, "typedef          int        off64_t;    /* sizeof(off64_t) = %u */\n", (unsigned int)sizeof(off64_t));
#elif HAVE_BASIC_OFF64_T==4
   fprintf(outfile, "typedef unsigned int        off64_t;    /* sizeof(off64_t) = %u */\n", (unsigned int)sizeof(off64_t));
#elif HAVE_BASIC_OFF64_T==5
   fprintf(outfile, "typedef          long       off64_t;    /* sizeof(off64_t) = %u */\n", (unsigned int)sizeof(off64_t));
#elif HAVE_BASIC_OFF64_T==6
   fprintf(outfile, "typedef unsigned long       off64_t;    /* sizeof(off64_t) = %u */\n", (unsigned int)sizeof(off64_t));
#elif HAVE_BASIC_OFF64_T==7
   fprintf(outfile, "typedef          long long  off64_t;    /* sizeof(off64_t) = %u */\n", (unsigned int)sizeof(off64_t));
#elif HAVE_BASIC_OFF64_T==8
   fprintf(outfile, "typedef unsigned long long  off64_t;    /* sizeof(off64_t) = %u */\n", (unsigned int)sizeof(off64_t));
#endif
#endif /* defined(HAVE_BASIC_OFF64_T) && HAVE_BASIC_OFF64_T */

#  if HAVE_BASIC_DEV_T==1
   fprintf(outfile, "typedef          short      dev_t;      /* sizeof(dev_t) = %u */\n", (unsigned int)sizeof(dev_t));
#elif HAVE_BASIC_DEV_T==2
   fprintf(outfile, "typedef unsigned short      dev_t;      /* sizeof(dev_t) = %u */\n", (unsigned int)sizeof(dev_t));
#elif HAVE_BASIC_DEV_T==3
   fprintf(outfile, "typedef          int        dev_t;      /* sizeof(dev_t) = %u */\n", (unsigned int)sizeof(dev_t));
#elif HAVE_BASIC_DEV_T==4
   fprintf(outfile, "typedef unsigned int        dev_t;      /* sizeof(dev_t) = %u */\n", (unsigned int)sizeof(dev_t));
#elif HAVE_BASIC_DEV_T==5
   fprintf(outfile, "typedef          long       dev_t;      /* sizeof(dev_t) = %u */\n", (unsigned int)sizeof(dev_t));
#elif HAVE_BASIC_DEV_T==6
   fprintf(outfile, "typedef unsigned long       dev_t;      /* sizeof(dev_t) = %u */\n", (unsigned int)sizeof(dev_t));
#elif HAVE_BASIC_DEV_T==7
   fprintf(outfile, "typedef          long long  dev_t;      /* sizeof(dev_t) = %u */\n", (unsigned int)sizeof(dev_t));
#elif HAVE_BASIC_DEV_T==8
   fprintf(outfile, "typedef unsigned long long  dev_t;      /* sizeof(dev_t) = %u */\n", (unsigned int)sizeof(dev_t));
#endif

   {
      struct stat x;
#  if HAVE_TYPEOF_ST_INO==1
   fprintf(outfile, "typedef          short      ino_t;      /* sizeof(ino_t) = %u */\n", (unsigned int)sizeof(x.st_ino));
#elif HAVE_TYPEOF_ST_INO==2
   fprintf(outfile, "typedef unsigned short      ino_t;      /* sizeof(ino_t) = %u */\n", (unsigned int)sizeof(x.st_ino));
#elif HAVE_TYPEOF_ST_INO==3
   fprintf(outfile, "typedef          int        ino_t;      /* sizeof(ino_t) = %u */\n", (unsigned int)sizeof(x.st_ino));
#elif HAVE_TYPEOF_ST_INO==4
   fprintf(outfile, "typedef unsigned int        ino_t;      /* sizeof(ino_t) = %u */\n", (unsigned int)sizeof(x.st_ino));
#elif HAVE_TYPEOF_ST_INO==5
   fprintf(outfile, "typedef          long       ino_t;      /* sizeof(ino_t) = %u */\n", (unsigned int)sizeof(x.st_ino));
#elif HAVE_TYPEOF_ST_INO==6
   fprintf(outfile, "typedef unsigned long       ino_t;      /* sizeof(ino_t) = %u */\n", (unsigned int)sizeof(x.st_ino));
#elif HAVE_TYPEOF_ST_INO==7
   fprintf(outfile, "typedef          long long  ino_t;      /* sizeof(ino_t) = %u */\n", (unsigned int)sizeof(x.st_ino));
#elif HAVE_TYPEOF_ST_INO==8
   fprintf(outfile, "typedef unsigned long long  ino_t;      /* sizeof(ino_t) = %u */\n", (unsigned int)sizeof(x.st_ino));
#endif
   }
   {
      unsigned short x = 0x100;
      if (x == ntohs(0x100)) {
	 fprintf(outfile, "#define __BYTE_ORDER __BIG_ENDIAN\t/* Motorola ea.*/\n");
      } else {
	 fprintf(outfile, "#define __BYTE_ORDER __LITTLE_ENDIAN\t/* Intel ea.*/\n");
      }
   }

#include <sys/time.h>	/* select(); OpenBSD: struct timespec */
   fprintf(outfile, "sizeof(struct timespec)      = %u\n", (unsigned int)sizeof(struct timespec));

   fprintf(outfile, "\n");
   fprintf(outfile, "/* Socat types */\n");
   fprintf(outfile, "sizeof(struct diag_dgram)      = %u\n", (unsigned int)sizeof(struct diag_dgram));
   fprintf(outfile, "((struct diag_dgram *)0)->op-((struct diag_dgram *)0)     = %u\n", (unsigned int)((char *)(&((struct diag_dgram *)0)->op)-(char *)((struct diag_dgram *)0)));
   fprintf(outfile, "((struct diag_dgram *)0)->now-((struct diag_dgram *)0)     = %u\n", (unsigned int)((char *)(&((struct diag_dgram *)0)->now)-(char *)((struct diag_dgram *)0)));
   fprintf(outfile, "((struct diag_dgram *)0)->exitcode-((struct diag_dgram *)0)     = %u\n", (unsigned int)((char *)(&((struct diag_dgram *)0)->exitcode)-(char *)((struct diag_dgram *)0)));
   fprintf(outfile, "((struct diag_dgram *)0)->text-((struct diag_dgram *)0)     = %u\n", (unsigned int)((((struct diag_dgram *)0)->text)-(char *)((struct diag_dgram *)0)));
#if _WITH_SOCKET && (_WITH_IP4 || _WITH_IP6)
   fprintf(outfile, "\nIP INTERFACES\n");
   iffan(outfile);
#endif
#if WITH_VSOCK
   vsockan(outfile);
#endif
   return 0;
}

#if _WITH_SOCKET && (_WITH_IP4 || _WITH_IP6)
static int iffan(FILE *outfile) {
   /* Linux: man 7 netdevice */
   /* FreeBSD, NetBSD: man 4 networking */
   /* Solaris: man 7 if_tcp */

/* currently we support Linux and a little FreeBSD */
#ifdef SIOCGIFCONF	/* not Solaris */

#define IFBUFSIZ 32*sizeof(struct ifreq) /*1024*/
   int s;
   unsigned char buff[IFBUFSIZ];
   struct ifconf ic;
   int i;

   if ((s = Socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
      Warn1("socket(PF_INET, SOCK_DGRAM, IPPROTO_IP): %s", strerror(errno));
      return -1;
   }

   for (i=0; i < IFBUFSIZ; ++i) {
      buff[i] = 255;
   }
   ic.ifc_len = sizeof(buff);
   ic.ifc_ifcu.ifcu_buf = (caddr_t)buff;
   if (Ioctl(s, SIOCGIFCONF, &ic) < 0) {
      Warn3("ioctl(%d, SIOCGIFCONF, %p): %s", s, &ic, strerror(errno));
      return -1;
   }

   for (i = 0; i < ic.ifc_len; i += sizeof(struct ifreq)) {
      struct ifreq *ifp = (struct ifreq *)((caddr_t)ic.ifc_req + i);
#if 0 || defined(SIOCGIFINDEX)	/* not NetBSD, OpenBSD */
      struct ifreq ifr;
#endif

#if 0 || defined(SIOCGIFINDEX)	/* not NetBSD, OpenBSD */
      strcpy(ifr.ifr_name, ifp->ifr_name);
      if (Ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
	 Warn3("ioctl(%d, SIOCGIFINDEX, {\"%s\"}): %s",
		s, ifr.ifr_name, strerror(errno));
	 return 1;
      }
#if HAVE_STRUCT_IFREQ_IFR_INDEX
      fprintf(outfile, "%2d: %s\n", ifr.ifr_index, ifp->ifr_name);
#elif HAVE_STRUCT_IFREQ_IFR_IFINDEX
      fprintf(outfile, "%2d: %s\n", ifr.ifr_ifindex, ifp->ifr_name);
#endif /* HAVE_STRUCT_IFREQ_IFR_INDEX */
#else /* !defined(SIOCGIFINDEX) */
      fprintf(outfile, "%2d: %s\n", i/(int)sizeof(struct ifreq), ifp->ifr_name);
#endif /* defined(SIOCGIFINDEX) */
   }
   Close(s);
#endif /* defined(SIOCGIFCONF) */
   return 0;
}
#endif /* _WITH_SOCKET */


#if WITH_VSOCK
static int vsockan(FILE *outfile) {
	unsigned int cid;
	int vsock;
	if (Getuid() != 0) {
	   return 1;
	}
	if ((vsock = Open("/dev/vsock", O_RDONLY, 0)) < 0 ) {
		Warn1("open(\"/dev/vsock\", ...): %s", strerror(errno));
		return -1;
	} else if (Ioctl(vsock, IOCTL_VM_SOCKETS_GET_LOCAL_CID, &cid) < 0) {
		Warn2("ioctl(%d, IOCTL_VM_SOCKETS_GET_LOCAL_CID, ...): %s",
		      vsock, strerror(errno));
		return -1;
	} else {
		Notice1("VSOCK CID=%u", cid);
		fprintf(outfile, "\nVSOCK_CID        = %u\n", cid);
	}
	if (vsock >= 0) {
		Close(vsock);
	}
	return 0;
}
#endif /* WITH_VSOCK */
