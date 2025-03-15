/* source: procan-cdefs.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* a function that prints compile time parameters */
/* the set of parameters is only a small subset of the available defines and
   will be extended on demand */


#include "xiosysincludes.h"
#include "mytypes.h"
#include "compat.h"
#include "error.h"

#include "procan.h"

int procan_cdefs(FILE *outfile) {
   /* System constants */
#ifdef __KERNEL__
   fprintf(outfile, "__KERNEL__                = \"%s\"\n", __KERNEL__);
#endif
#ifdef __GLIBC__
   fprintf(outfile, "__GLIBC__                 = %d\n", __GLIBC__);
#endif
   /* Basic C/system constants */
#ifdef FD_SETSIZE
   fprintf(outfile, "#define FD_SETSIZE  %u\n", FD_SETSIZE);
#endif
#ifdef NFDBITS
   fprintf(outfile, "#define NFDBITS     %d\n", (int)NFDBITS);
#endif
#ifdef O_RDONLY
   fprintf(outfile, "#define O_RDONLY    %u\n", O_RDONLY);
#endif
#ifdef O_WRONLY
   fprintf(outfile, "#define O_WRONLY    %u\n", O_WRONLY);
#endif
#ifdef O_RDWR
   fprintf(outfile, "#define O_RDWR      %u\n", O_RDWR);
#endif
#ifdef O_CREAT
   fprintf(outfile, "#define O_CREAT     0x%06x  /* 0%08o */\n", O_CREAT, O_CREAT);
#endif
#ifdef O_EXCL
   fprintf(outfile, "#define O_EXCL      0x%06x  /* 0%08o */\n", O_EXCL, O_EXCL);
#endif
#ifdef O_NOCTTY
   fprintf(outfile, "#define O_NOCTTY    0x%06x  /* 0%08o */\n", O_NOCTTY, O_NOCTTY);
#endif
#ifdef O_TRUNC
   fprintf(outfile, "#define O_TRUNC     0x%06x  /* 0%08o */\n", O_TRUNC, O_TRUNC);
#endif
#ifdef O_APPEND
   fprintf(outfile, "#define O_APPEND    0x%06x  /* 0%08o */\n", O_APPEND, O_APPEND);
#endif
#ifdef O_NONBLOCK
   fprintf(outfile, "#define O_NONBLOCK  0x%06x  /* 0%08o */\n", O_NONBLOCK, O_NONBLOCK);
#endif
#ifdef O_NDELAY
   fprintf(outfile, "#define O_NDELAY    0x%06x  /* 0%08o */\n", O_NDELAY, O_NDELAY);
#endif
#ifdef O_SYNC
   fprintf(outfile, "#define O_SYNC      0x%06x  /* 0%08o */\n", O_SYNC, O_SYNC);
#endif
#ifdef O_FSYNC
   fprintf(outfile, "#define O_FSYNC     0x%06x  /* 0%08o */\n", O_FSYNC, O_FSYNC);
#endif
#ifdef O_LARGEFILE
   fprintf(outfile, "#define O_LARGEFILE 0x%06x  /* 0%08o */\n", O_LARGEFILE, O_LARGEFILE);
#endif
#ifdef O_DIRECTORY
   fprintf(outfile, "#define O_DIRECTORY 0x%06x  /* 0%08o */\n", O_DIRECTORY, O_DIRECTORY);
#endif
#ifdef O_NOFOLLOW
   fprintf(outfile, "#define O_NOFOLLOW  0x%06x  /* 0%08o */\n", O_NOFOLLOW, O_NOFOLLOW);
#endif
#ifdef O_CLOEXEC
   fprintf(outfile, "#define O_CLOEXEC   0x%06x  /* 0%08o */\n", O_CLOEXEC, O_CLOEXEC);
#endif
#ifdef O_DIRECT
   fprintf(outfile, "#define O_DIRECT    0x%06x  /* 0%08o */\n", O_DIRECT, O_DIRECT);
#endif
#ifdef O_NOATIME
   fprintf(outfile, "#define O_NOATIME   0x%06x  /* 0%08o */\n", O_NOATIME, O_NOATIME);
#endif
#ifdef O_PATH
   fprintf(outfile, "#define O_PATH      0x%06x  /* 0%08o */\n", O_PATH, O_PATH);
#endif
#ifdef O_DSYNC
   fprintf(outfile, "#define O_DSYNC     0x%06x  /* 0%08o */\n", O_SYNC, O_SYNC);
#endif
#ifdef O_TMPFILE
   fprintf(outfile, "#define O_TMPFILE   0x%06x  /* 0%08o */\n", O_TMPFILE, O_TMPFILE);
#endif
#ifdef SHUT_RD
   fprintf(outfile, "#define SHUT_RD     %u\n", SHUT_RD);
#endif
#ifdef SHUT_WR
   fprintf(outfile, "#define SHUT_WR     %u\n", SHUT_WR);
#endif
#ifdef SHUT_RDWR
   fprintf(outfile, "#define SHUT_RDWR   %u\n", SHUT_RDWR);
#endif

   /* Compile time controls */
#ifdef _FILE_OFFSET_BITS
   fprintf(outfile, "#define _FILE_OFFSET_BITS %u\n", _FILE_OFFSET_BITS);
#endif
#ifdef _LARGE_FILES
   fprintf(outfile, "#define _LARGE_FILES %u\n", _LARGE_FILES);
#endif

   /* termios constants */
#ifdef CRDLY
   fprintf(outfile, "#define CRDLY       0x%08x  /* 0%011o */\n", CRDLY, CRDLY);
#endif
#ifdef CR0
   fprintf(outfile, "#define CR0         0x%08x  /* 0%011o */\n", CR0, CR0);
#endif
#ifdef CR1
   fprintf(outfile, "#define CR1         0x%08x  /* 0%011o */\n", CR1, CR1);
#endif
#ifdef CR2
   fprintf(outfile, "#define CR2         0x%08x  /* 0%011o */\n", CR2, CR2);
#endif
#ifdef CR3
   fprintf(outfile, "#define CR3         0x%08x  /* 0%011o */\n", CR3, CR3);
#endif
#ifdef TABDLY
   fprintf(outfile, "#define TABDLY      0x%08x  /* 0%011o */\n", TABDLY, TABDLY);
#endif
#ifdef TAB0
   fprintf(outfile, "#define TAB0        0x%08x  /* 0%011o */\n", TAB0, TAB0);
#endif
#ifdef TAB1
   fprintf(outfile, "#define TAB1        0x%08x  /* 0%011o */\n", TAB1, TAB1);
#endif
#ifdef TAB2
   fprintf(outfile, "#define TAB2        0x%08x  /* 0%011o */\n", TAB2, TAB2);
#endif
#ifdef TAB3
   fprintf(outfile, "#define TAB3        0x%08x  /* 0%011o */\n", TAB3, TAB3);
#endif
#ifdef CSIZE
   fprintf(outfile, "#define CSIZE       0x%08x  /* 0%011o */\n", CSIZE, CSIZE);
#endif
#ifdef TIOCEXCL
   fprintf(outfile, "#define TIOCEXCL    0x%lx\n", (unsigned long)TIOCEXCL);
#endif

   /* stdio constants */
#ifdef FOPEN_MAX
   fprintf(outfile, "#define FOPEN_MAX %u\n", FOPEN_MAX);
#endif

   /* socket constants */
#ifdef PF_UNSPEC
   fprintf(outfile, "#define PF_UNSPEC %d\n", PF_UNSPEC);
#endif
#ifdef PF_UNIX
   fprintf(outfile, "#define PF_UNIX %d\n", PF_UNIX);
#elif defined(PF_LOCAL)
   fprintf(outfile, "#define PF_LOCAL %d\n", PF_LOCAL);
#endif
#ifdef PF_INET
   fprintf(outfile, "#define PF_INET %d\n", PF_INET);
#endif
#ifdef PF_INET6
   fprintf(outfile, "#define PF_INET6 %d\n", PF_INET6);
#endif
#ifdef PF_APPLETALK
   fprintf(outfile, "#define PF_APPLETALK %d\n", PF_APPLETALK);
#endif
#ifdef PF_PACKET
   fprintf(outfile, "#define PF_PACKET %d\n", PF_PACKET);
#endif
#ifdef PF_VSOCK
   fprintf(outfile, "#define PF_VSOCK %d\n", PF_VSOCK);
#endif
#ifdef SOCK_STREAM
   fprintf(outfile, "#define SOCK_STREAM %d\n", SOCK_STREAM);
#endif
#ifdef SOCK_DGRAM
   fprintf(outfile, "#define SOCK_DGRAM %d\n", SOCK_DGRAM);
#endif
#ifdef SOCK_RAW
   fprintf(outfile, "#define SOCK_RAW %d\n", SOCK_RAW);
#endif
#ifdef SOCK_SEQPACKET
   fprintf(outfile, "#define SOCK_SEQPACKET %d\n", SOCK_SEQPACKET);
#endif
#ifdef SOCK_PACKET
   fprintf(outfile, "#define SOCK_PACKET %d\n", SOCK_PACKET);
#endif
#ifdef IPPROTO_IP
   fprintf(outfile, "#define IPPROTO_IP %d\n", IPPROTO_IP);
#endif
#ifdef IPPROTO_TCP
   fprintf(outfile, "#define IPPROTO_TCP %d\n", IPPROTO_TCP);
#endif
#ifdef IPPROTO_UDP
   fprintf(outfile, "#define IPPROTO_UDP %d\n", IPPROTO_UDP);
#endif
#ifdef IPPROTO_DCCP
   fprintf(outfile, "#define IPPROTO_DCCP %d\n", IPPROTO_DCCP);
#endif
#ifdef IPPROTO_SCTP
   fprintf(outfile, "#define IPPROTO_SCTP %d\n", IPPROTO_SCTP);
#endif
#ifdef IPPROTO_UDPLITE
   fprintf(outfile, "#define IPPROTO_UDPLITE %d\n", IPPROTO_UDPLITE);
#endif
#ifdef IPPROTO_RAW
   fprintf(outfile, "#define IPPROTO_RAW %d\n", IPPROTO_RAW);
#endif
#ifdef SOL_SOCKET
   fprintf(outfile, "#define SOL_SOCKET 0x%x\n", SOL_SOCKET);
#endif
#ifdef SOL_PACKET
   fprintf(outfile, "#define SOL_PACKET 0x%x\n", SOL_PACKET);
#endif
#ifdef SOL_IP
   fprintf(outfile, "#define SOL_IP 0x%x\n", SOL_IP);
#endif
#ifdef SOL_IPV6
   fprintf(outfile, "#define SOL_IPV6 0x%x\n", SOL_IPV6);
#endif
#ifdef SOL_TCP
   fprintf(outfile, "#define SOL_TCP 0x%x\n", SOL_TCP);
#endif
#ifdef SOL_UDP
   fprintf(outfile, "#define SOL_UDP 0x%x\n", SOL_UDP);
#endif
#ifdef SOL_SCTP
   fprintf(outfile, "#define SOL_SCTP 0x%x\n", SOL_SCTP);
#endif
#ifdef SOL_DCCP
   fprintf(outfile, "#define SOL_DCCP 0x%x\n", SOL_DCCP);
#endif
#ifdef SO_PROTOCOL
   fprintf(outfile, "#define SO_PROTOCOL %d\n", SO_PROTOCOL);
#endif
#ifdef SO_PROTOTYPE
   fprintf(outfile, "#define SO_PROTOTYPE %d\n", SO_PROTOTYPE);
#endif
#ifdef SO_REUSEADDR
   fprintf(outfile, "#define SO_REUSEADDR %d\n", SO_REUSEADDR);
#endif
#ifdef TCP_MAXSEG
   fprintf(outfile, "#define TCP_MAXSEG %d\n",   TCP_MAXSEG);
#endif
#ifdef AI_PASSIVE
   fprintf(outfile, "#define AI_PASSIVE     0x%02x\n", AI_PASSIVE);
#endif
#ifdef AI_CANONNAME
   fprintf(outfile, "#define AI_CANONNAME   0x%02x\n", AI_CANONNAME);
#endif
#ifdef AI_NUMERICHOST
   fprintf(outfile, "#define AI_NUMERICHOST 0x%02x\n", AI_NUMERICHOST);
#endif
#ifdef AI_V4MAPPED
   fprintf(outfile, "#define AI_V4MAPPED    0x%02x\n", AI_V4MAPPED);
#endif
#ifdef AI_ALL
   fprintf(outfile, "#define AI_ALL         0x%02x\n", AI_ALL);
#endif
#ifdef AI_ADDRCONFIG
   fprintf(outfile, "#define AI_ADDRCONFIG  0x%02x\n", AI_ADDRCONFIG);
#endif
#ifdef EAI_BADFLAGS
   fprintf(outfile, "#define EAI_BADFLAGS    %d\n", EAI_BADFLAGS);
#endif
#ifdef EAI_NONAME
   fprintf(outfile, "#define EAI_NONAME      %d\n", EAI_NONAME);
#endif
#ifdef EAI_AGAIN
   fprintf(outfile, "#define EAI_AGAIN       %d\n", EAI_AGAIN);
#endif
#ifdef EAI_FAIL
   fprintf(outfile, "#define EAI_FAIL        %d\n", EAI_FAIL);
#endif
#ifdef EAI_FAMILY
   fprintf(outfile, "#define EAI_FAMILY      %d\n", EAI_FAMILY);
#endif
#ifdef EAI_SOCKTYPE
   fprintf(outfile, "#define EAI_SOCKTYPE    %d\n", EAI_SOCKTYPE);
#endif
#ifdef EAI_SERVICE
   fprintf(outfile, "#define EAI_SERVICE     %d\n", EAI_SERVICE);
#endif
#ifdef EAI_MEMORY
   fprintf(outfile, "#define EAI_MEMORY      %d\n", EAI_MEMORY);
#endif
#ifdef EAI_SYSTEM
   fprintf(outfile, "#define EAI_SYSTEM      %d\n", EAI_SYSTEM);
#endif
#ifdef EAI_OVERFLOW
   fprintf(outfile, "#define EAI_OVERFLOW    %d\n", EAI_OVERFLOW);
#endif
#ifdef EAI_NODATA
   fprintf(outfile, "#define EAI_NODATA      %d\n", EAI_NODATA);
#endif
#ifdef EAI_ADDRFAMILY
   fprintf(outfile, "#define EAI_ADDRFAMILY  %d\n", EAI_ADDRFAMILY);
#endif
#ifdef EAI_INPROGRESS
   fprintf(outfile, "#define EAI_INPROGRESS  %d\n", EAI_INPROGRESS);
#endif
#ifdef EAI_CANCELED
   fprintf(outfile, "#define EAI_CANCELED    %d\n", EAI_CANCELED);
#endif
#ifdef EAI_NOTCANCELED
   fprintf(outfile, "#define EAI_NOTCANCELED %d\n", EAI_NOTCANCELED);
#endif
#ifdef EAI_ALLDONE
   fprintf(outfile, "#define EAI_ALLDONE     %d\n", EAI_ALLDONE);
#endif
#ifdef EAI_INTR
   fprintf(outfile, "#define EAI_INTR        %d\n", EAI_INTR);
#endif
#ifdef EAI_IDN_ENCODE
   fprintf(outfile, "#define EAI_IDN_ENCODE  %d\n", EAI_IDN_ENCODE);
#endif

   return 0;
}
