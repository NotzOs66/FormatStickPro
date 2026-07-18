#ifndef CONFIG_H
#define CONFIG_H

#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_FCNTL_H 1
#define HAVE_ERRNO_H 1
#define HAVE_LIMITS_H 1
#define HAVE_CTYPE_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDARG_H 1
#define HAVE_TIME_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_LINUX_FD_H 1
#define HAVE_LINUX_HDREG_H 1
#define HAVE_GETOPT_H 1
#define HAVE_ENDIAN_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_SYS_SYSMACROS_H 1
#define HAVE_STRINGS_H 1
#define HAVE_FFS 1
#define HAVE_LOCALE_H 1

#define PACKAGE_NAME "ntfs-3g"
#define PACKAGE_VERSION "2026.2.25"
#define VERSION PACKAGE_VERSION

/* Android/Linux specific */
#define HAVE_LINUX_FS_H 1
#define HAVE_MNTENT_H 1

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifndef LC_ALL
#define LC_ALL 6
#endif

#endif
