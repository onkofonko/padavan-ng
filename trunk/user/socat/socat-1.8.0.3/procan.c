/* source: procan.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* the subroutine procan makes a "PROCess ANalysis". It gathers information
   about the process environment it is running in without modifying its state
   (almost).
 */

#include "xiosysincludes.h"
#include "mytypes.h"
#include "compat.h"
#include "error.h"
#include "sycls.h"
#include "sysutils.h"
#include "sched.h"
#include "filan.h"

#include <sys/resource.h> 	/* RLIMIT_CPU ... */
#include <dirent.h> 		/* opendir() readdir() closedir() */

#include "procan.h"


/* Search dir recursively for matching device file.
   Returns 0 on success;
   returns -1 when it failed to find the device file. */
int find_devpath(
	char *dirname,
	unsigned int major,
	unsigned int minor,
	char *devname)
{
   DIR *dirp;
   struct dirent *dirent;
   char devpath[PATH_MAX];
   int rc;

   /* Pass 1: search dir flatly for this device entry */
   dirp = opendir(dirname);
   if (dirp == NULL) {
      Warn2("failed to open dir \"%s\": %s", dirname, strerror(errno));
      return -1;
   }
   while ((errno = 0) || (dirent = readdir(dirp))) {
      struct stat statbuf;

#if HAVE_DIRENT_D_TYPE
      if (dirent->d_type != DT_CHR && dirent->d_type != DT_UNKNOWN)
	 continue;
#endif
      snprintf(devpath, PATH_MAX, "%s/%s", dirname, dirent->d_name);
      if (Stat(devpath, &statbuf) < 0) {
	 Warn2("failed to stat entry \"%s\": %s", devpath, strerror(errno));
	 continue;
      }
      if ((statbuf.st_mode & S_IFMT) != S_IFCHR)
	 continue;
      if ((statbuf.st_rdev >> 8) == major &&
	  (statbuf.st_rdev & 0xff) == minor) {
	 strcpy(devname, devpath);
	 return 0;
      }
      continue;
   }
   closedir(dirp);
   if (errno != 0) {
      Warn2("failed to read dir \"%s\": %s", dirname, strerror(errno));
      snprintf(devname, PATH_MAX, "device %u, %u", major, minor);
   }

   /* Pass 2: search sub dirs */
   dirp = opendir(dirname);
   if (dirp == NULL) {
      Warn2("failed to open dir \"%s\": %s", dirname, strerror(errno));
      return -1;
   }
   while ((errno = 0) || (dirent = readdir(dirp))) {
      char dirpath[PATH_MAX];
#if HAVE_DIRENT_D_TYPE
      if (dirent->d_type != DT_DIR)
	 continue;
#else /* Solaris */
      {
	 struct stat statbuf;
	 if (Stat(dirent->d_name, &statbuf) < 0)
	    continue;
	 if ((statbuf.st_mode & S_IFMT) != S_IFDIR)
	    continue;
      }
#endif
      if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
	 continue;
      snprintf(dirpath, PATH_MAX, "%s/%s", dirname, dirent->d_name);
      rc = find_devpath(dirpath, major, minor, devname);
      if (rc == 0) {
	 return 0;
      }
   }
   closedir(dirp);
   if (dirent == NULL) {
      return 1;
   }
   return 0;
}

   /* Tries to determine the name of the controlling terminal.
   Returns 0 on success, the name in cttyname;
   returns 1 when only the device numbers are in cttyname;
   returns -1 when it failed to determine ctty. */
static int controlling_term(
	FILE *outfile)
{
   char cttypath[PATH_MAX+1];
   int rc;

   { /* On Linux this just gives "/dev/tty" */
      char s[L_ctermid+1];
      fprintf(outfile, "controlling terminal by ctermid():           \"%s\"\n", ctermid(s));
   }

   { /* Check if there is a controlling terminal */
      int fd;

      if ((fd = Open("/dev/tty", O_NOCTTY, 0)) >= 0)
	 /* On Linux this just gives "/dev/tty" */
	 fprintf(outfile, "controlling terminal by /dev/tty, ttyname(): \"%s\"\n", Ttyname(fd));
      else
	 fprintf(outfile, "controlling terminal by /dev/tty, ttyname(): (none)\n");
   }

#if HAVE_PROC_DIR
   do { /* Linux: derive ctty from info in /proc */
      const char procpath[] = "/proc/self/stat";
      FILE *procstat;
      unsigned int dev;
      int n = 0;
      unsigned int maj, min;

      /* Linux: get device ids from /proc */
      if ((procstat = fopen(procpath, "r")) == NULL) {
	 Warn1("failed to open \"%s\" for process info", procpath);
	 rc = -1;
	 break;
      }
      n = fscanf(procstat, "%*s %*s %*s %*s %*s %*s %u", &dev);
      if (n != 1) {
	 Warn1("failed to read ctty info from \"%s\"", procpath);
	 rc = -1;
	 break;
      }
      maj = (dev>>8)&0xff;
      min = ((dev>>12)&0xfff00)|(dev&0xff);
      rc = find_devpath("/dev" /* _PATH_DEV has trailing "/" */, maj, min, cttypath);
      if (rc < 0) {
	 snprintf(cttypath, PATH_MAX, "device %u, %u", maj, min);
	 rc = 1;
	 break;
      }
      rc = 0;
   } while (false);
#else /* !HAVE_PROC_DIR */
   rc = -1;
#endif /* !HAVE_PROC_DIR */
   if (rc >= 0)
      fprintf(outfile, "controlling terminal by /proc/<pid>/:        \"%s\"\n", cttypath);
   else
      fprintf(outfile, "controlling terminal by /proc/<pid>/:        (none)\n");

   return 0;
}


int procan(FILE *outfile) {

   /*filan(0, outfile);*/

   fprintf(outfile, "process id = "F_pid"\n", Getpid());
   fprintf(outfile, "process parent id = "F_pid"\n", Getppid());
   controlling_term(outfile);
   fprintf(outfile, "process group id = "F_pid"\n", Getpgrp());
#if HAVE_GETSID
   fprintf(outfile, "process session id = "F_pid"\n", Getsid(0));
#endif
   fprintf(outfile, "process group id if fg process / stdin = "F_pid"\n", Tcgetpgrp(0));
   fprintf(outfile, "process group id if fg process / stdout = "F_pid"\n", Tcgetpgrp(1));
   fprintf(outfile, "process group id if fg process / stderr = "F_pid"\n", Tcgetpgrp(2));

   /* process owner, groups */
#if HAVE_GETRESUID
   {
      uid_t ruid, euid, suid;
      getresuid(&ruid, &euid, &suid);
      fprintf(outfile, "user id  = "F_uid"\n", ruid);
      fprintf(outfile, "effective user id  = "F_uid"\n", euid);
      fprintf(outfile, "saved set-user id  = "F_uid"\n", suid);
   }
#else /* !HAVE_GETRESUID */
   fprintf(outfile, "user id  = "F_uid"\n", Getuid());
   fprintf(outfile, "effective user id  = "F_uid"\n", Geteuid());
#endif /* !HAVE_GETRESUID */
#if HAVE_GETRESGID
   {
      gid_t rgid, egid, sgid;
      getresgid(&rgid, &egid, &sgid);
      fprintf(outfile, "group id = "F_gid"\n", rgid);
      fprintf(outfile, "effective group id = "F_gid"\n", egid);
      fprintf(outfile, "saved set-group id = "F_gid"\n", sgid);
   }
#else /* !HAVE_GETRESGID */
   fprintf(outfile, "group id = "F_gid"\n", Getgid());
   fprintf(outfile, "effective group id = "F_gid"\n", Getegid());
#endif /* !HAVE_GETRESGID */

   /* Simple process features */
   fprintf(outfile, "\n");
   {
      mode_t mask;
#if LATER
      char procpath[PATH_MAX];
      sprintf(procpath, "/proc/"F_pid"/status", Getpid());
      if (Stat()) {

      } else
#endif
      {
	 mask = Umask(0066);
	 Umask(mask);
      }
      fprintf(outfile, "umask = "F_mode"\n", mask);
   }

   {
      struct rlimit rlim;

      fprintf(outfile, "\n/* Resource limits */\n");
      fprintf(outfile, "resource                                 current                 maximum\n");
      if (getrlimit(RLIMIT_CPU, &rlim) < 0) {
	 Warn2("getrlimit(RLIMIT_CPU, %p): %s", &rlim, strerror(errno));
      } else {
	 fprintf(outfile,
		 "cpu time (seconds)      %24"F_rlim_max"%24"F_rlim_max"\n",
		 rlim.rlim_cur, rlim.rlim_max);
      }
      if (getrlimit(RLIMIT_FSIZE, &rlim) < 0) {
	 Warn2("getrlimit(RLIMIT_FSIZE, %p): %s", &rlim, strerror(errno));
      } else {
	 fprintf(outfile,
		 "file size (blocks)      %24"F_rlim_max"%24"F_rlim_max"\n",
		 rlim.rlim_cur, rlim.rlim_max);
      }
      if (getrlimit(RLIMIT_DATA, &rlim) < 0) {
	 Warn2("getrlimit(RLIMIT_DATA, %p): %s", &rlim, strerror(errno));
      } else {
	 fprintf(outfile,
		 "data seg size (kbytes)  %24"F_rlim_max"%24"F_rlim_max"\n",
		 rlim.rlim_cur, rlim.rlim_max);
      }
      if (getrlimit(RLIMIT_STACK, &rlim) < 0) {
	 Warn2("getrlimit(RLIMIT_STACK, %p): %s", &rlim, strerror(errno));
      } else {
	 fprintf(outfile,
		 "stack size (blocks)     %24"F_rlim_max"%24"F_rlim_max"\n",
		 rlim.rlim_cur, rlim.rlim_max);
      }
      if (getrlimit(RLIMIT_CORE, &rlim) < 0) {
	 Warn2("getrlimit(RLIMIT_CORE, %p): %s", &rlim, strerror(errno));
      } else {
	 fprintf(outfile,
		 "core file size (blocks) %24"F_rlim_max"%24"F_rlim_max"\n",
		 rlim.rlim_cur, rlim.rlim_max);
      }
#ifdef RLIMIT_RSS	/* Linux, AIX; not Cygwin */
      if (getrlimit(RLIMIT_RSS, &rlim) < 0) {
	 Warn2("getrlimit(RLIMIT_RSS, %p): %s", &rlim, strerror(errno));
      } else {
	 fprintf(outfile,
		 "max resident set size   %24"F_rlim_max"%24"F_rlim_max"\n",
		 rlim.rlim_cur, rlim.rlim_max);
      }
#endif
#ifdef RLIMIT_NPROC	/* Linux, not AIX, Cygwin */
      if (getrlimit(RLIMIT_NPROC, &rlim) < 0) {
	 Warn2("getrlimit(RLIMIT_NPROC, %p): %s", &rlim, strerror(errno));
      } else {
	 fprintf(outfile,
		 "max user processes      %24"F_rlim_max"%24"F_rlim_max"\n",
		 rlim.rlim_cur, rlim.rlim_max);
      }
#endif
#ifdef RLIMIT_NOFILE	/* not AIX 4.1 */
      if (getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
	 Warn2("getrlimit(RLIMIT_NOFILE, %p): %s", &rlim, strerror(errno));
      } else {
	 fprintf(outfile,
		 "open files              %24"F_rlim_max"%24"F_rlim_max"\n",
		 rlim.rlim_cur, rlim.rlim_max);
      }
#endif
#ifdef RLIMIT_MEMLOCK	/* Linux, not AIX, Cygwin */
      if (getrlimit(RLIMIT_MEMLOCK, &rlim) < 0) {
	 Warn2("getrlimit(RLIMIT_MEMLOCK, %p): %s", &rlim, strerror(errno));
      } else {
	 fprintf(outfile,
		 "max locked-in-memory\n  address space         %24"F_rlim_max"%24"F_rlim_max"\n",
		 rlim.rlim_cur, rlim.rlim_max);
      }
#endif
#ifdef RLIMIT_AS
      if (getrlimit(RLIMIT_AS, &rlim) < 0) {
	 Warn2("getrlimit(RLIMIT_AS, %p): %s", &rlim, strerror(errno));
      } else {
	 fprintf(outfile,
		 "virtual memory (kbytes) %24"F_rlim_max"%24"F_rlim_max"\n",
		 rlim.rlim_cur, rlim.rlim_max);
      }
#endif
      fputc('\n', outfile);

   }

#ifdef CC
   fprintf(outfile, "// CC:                      "CC"\n");
#endif
#ifdef __STDC_VERSION__
   fprintf(outfile, "#define __STDC_VERSION__    %ld\n", __STDC_VERSION__);
#endif
#ifdef SIZE_MAX
   fprintf(outfile, "SIZE_MAX                  = "F_Zu" /* maximum value of size_t */\n", SIZE_MAX);
#endif
#ifdef P_tmpdir
   fprintf(outfile, "P_tmpdir                  = \"%s\"\n", P_tmpdir);
#endif
#ifdef L_tmpnam
   fprintf(outfile, "L_tmpnam                  = %u\n", L_tmpnam);
#endif
#ifdef TMP_MAX
   fprintf(outfile, "TMP_MAX                   = %d\n", TMP_MAX);
#endif
#ifdef FD_SETSIZE
   fprintf(outfile, "FD_SETSIZE                = %d /* maximum number of FDs for select() */\n", FD_SETSIZE);
#endif
#ifdef PIPE_BUF
   fprintf(outfile, "PIPE_BUF                  = %-24d\n", PIPE_BUF);
#endif

   /* Name spaces */
   {
      char path[PATH_MAX];
      char link[PATH_MAX];
      snprintf(path, sizeof(path)-1, "/proc/"F_pid"/ns/net", getpid());
      if (readlink(path, link, sizeof(link)-1) >= 0) {
	 fprintf(outfile, "Network namespace: %s", link);
      }
   }

   /* file descriptors */

   /* what was this for?? */
   /*Sleep(1);*/
   return 0;
}
