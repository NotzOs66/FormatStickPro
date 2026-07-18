#define VERSION "7.0.1"
#define PACKAGE "xfsprogs"
#define HAVE_FSTAT64 1
#define HAVE_LSEEK64 1
#define HAVE_LLSEEK 1
#define HAVE_STRSET 1
#define HAVE_MEMALIGN 1
#define HAVE_GETPAGESIZE 1
#define HAVE_MNTENT_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_SYSMACROS_H 1
#define HAVE_LINUX_BLKZONED_H 1
#define HAVE_LIBURCU 1
#define HAVE_LIBURCU_ATOMIC64 1
#define HAVE_LIBINIH 1
#define HAVE_LIBBLKID 1
#define HAVE_LIBUUID 1

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
