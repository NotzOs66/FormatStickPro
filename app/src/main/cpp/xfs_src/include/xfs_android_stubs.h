#ifndef _XFS_ANDROID_STUBS_H
#define _XFS_ANDROID_STUBS_H

#include "config.h"

// 1. System headers FIRST
#include <setjmp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <locale.h>
#include <mntent.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

// 2. Global state and basic stubs
extern jmp_buf g_xfs_jmp;
extern void xfs_msg(const char *fmt, ...);

#ifndef XFS_BRIDGE_INTERNAL
#undef exit
#define exit(code) longjmp(g_xfs_jmp, (code) == 0 ? -1 : (code))
#endif

#undef printf
#define printf(fmt, ...) xfs_msg(fmt, ##__VA_ARGS__)
#undef fprintf
#define fprintf(stream, fmt, ...) xfs_msg(fmt, ##__VA_ARGS__)
#undef perror
#define perror(s) xfs_msg("%s: %s", s, strerror(errno))

// 3. Locale stubs - avoid redefinition warnings
#undef textdomain
#define textdomain(a) ((void)0)
#undef bindtextdomain
#define bindtextdomain(a, b) ((void)0)
#undef gettext
#define gettext(a) (a)
#undef setlocale
#define setlocale(a, b) ((char*)"C")

static inline int is_usb_dev(const char *path) {
    if (!path) return 0;
    return strstr(path, "usb_device") != NULL;
}

// 4. SCSI bridge helpers
extern int64_t usb_scsi_read_internal(void* buf, int64_t count);
extern int64_t usb_scsi_write_internal(const void* buf, int64_t count);
extern int64_t usb_scsi_pread_internal(void* buf, int64_t count, int64_t offset);
extern int64_t usb_scsi_pwrite_internal(const void* buf, int64_t count, int64_t offset);
extern int64_t usb_scsi_lseek_internal(int64_t offset, int whence);
extern int usb_scsi_sync(void);
extern uint64_t g_usb_scsi_part_sectors;

#ifndef XFS_BRIDGE_INTERNAL
#define MAGIC_USB_FD (-200)

// 5. Hooks
#undef open
#define open(path, flags, ...) (is_usb_dev(path) ? MAGIC_USB_FD : (open)(path, flags, ##__VA_ARGS__))
#undef open64
#define open64 open

#undef pread
#define pread(fd, buf, count, offset) ((fd) == MAGIC_USB_FD ? usb_scsi_pread_internal(buf, count, offset) : (pread)(fd, buf, (size_t)(count), (off_t)(offset)))
#undef pread64
#define pread64 pread

#undef read
#define read(fd, buf, count) ((fd) == MAGIC_USB_FD ? usb_scsi_read_internal(buf, count) : (read)(fd, buf, count))

#undef pwrite
#define pwrite(fd, buf, count, offset) ((fd) == MAGIC_USB_FD ? usb_scsi_pwrite_internal(buf, count, offset) : (pwrite)(fd, buf, (size_t)(count), (off_t)(offset)))
#undef pwrite64
#define pwrite64 pwrite

#undef write
#define write(fd, buf, count) ((fd) == MAGIC_USB_FD ? usb_scsi_write_internal(buf, count) : (write)(fd, buf, count))

static inline ssize_t xfs_writev_hook(int fd, const struct iovec *iov, int iovcnt) {
    int64_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        int64_t res = usb_scsi_write_internal(iov[i].iov_base, iov[i].iov_len);
        if (res < 0) return -1;
        total += res;
    }
    return (ssize_t)total;
}

#undef writev
#define writev(fd, iov, iovcnt) ((fd) == MAGIC_USB_FD ? xfs_writev_hook(fd, iov, iovcnt) : (writev)(fd, iov, iovcnt))

static inline ssize_t xfs_pwritev_hook(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
    int64_t total = 0;
    off_t cur_offset = offset;
    for (int i = 0; i < iovcnt; i++) {
        int64_t res = usb_scsi_pwrite_internal(iov[i].iov_base, iov[i].iov_len, cur_offset);
        if (res < 0) return -1;
        total += res;
        cur_offset += (off_t)res;
    }
    return (ssize_t)total;
}

#undef pwritev
#define pwritev(fd, iov, iovcnt, offset) ((fd) == MAGIC_USB_FD ? xfs_pwritev_hook(fd, iov, iovcnt, offset) : (pwritev)(fd, iov, iovcnt, offset))
#undef pwritev2
#define pwritev2(fd, iov, iovcnt, offset, flags) pwritev(fd, iov, iovcnt, offset)

static inline ssize_t xfs_preadv_hook(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
    int64_t total = 0;
    off_t cur_offset = offset;
    for (int i = 0; i < iovcnt; i++) {
        int64_t res = usb_scsi_pread_internal(iov[i].iov_base, iov[i].iov_len, cur_offset);
        if (res < 0) return -1;
        total += res;
        cur_offset += (off_t)res;
    }
    return (ssize_t)total;
}

#undef preadv
#define preadv(fd, iov, iovcnt, offset) ((fd) == MAGIC_USB_FD ? xfs_preadv_hook(fd, iov, iovcnt, offset) : (preadv)(fd, iov, iovcnt, offset))
#undef preadv2
#define preadv2(fd, iov, iovcnt, offset, flags) preadv(fd, iov, iovcnt, offset)

#undef fallocate
#define fallocate(fd, mode, offset, len) ((fd) == MAGIC_USB_FD ? (errno = EOPNOTSUPP, -1) : (fallocate)(fd, mode, offset, len))
#undef fallocate64
#define fallocate64 fallocate

static inline int xfs_fstat_hook(int fd, struct stat *buf) {
    if (!buf) return -1;
    memset(buf, 0, sizeof(struct stat));
    buf->st_mode = S_IFREG | 0644;
    buf->st_size = (off_t)g_usb_scsi_part_sectors * 512;
    buf->st_blksize = 512;
    buf->st_blocks = (blkcnt_t)g_usb_scsi_part_sectors;
    return 0;
}

#undef fstat
#define fstat(fd, buf) ((fd) == MAGIC_USB_FD ? xfs_fstat_hook(fd, buf) : (fstat)(fd, buf))
#undef fstat64
#define fstat64 fstat

#undef fsync
#define fsync(fd) ((fd) == MAGIC_USB_FD ? usb_scsi_sync() : (fsync)(fd))
#undef fdatasync
#define fdatasync(fd) ((fd) == MAGIC_USB_FD ? usb_scsi_sync() : (fdatasync)(fd))

#undef close
#define close(fd) ((fd) == MAGIC_USB_FD ? 0 : (close)(fd))

#undef lseek
#define lseek(fd, offset, whence) ((fd) == MAGIC_USB_FD ? usb_scsi_lseek_internal(offset, whence) : (lseek)(fd, offset, whence))
#undef lseek64
#define lseek64 lseek

#endif

// blkid stubs for topology.c
typedef void* blkid_probe;
typedef void* blkid_topology;
static inline blkid_probe blkid_new_probe_from_filename(const char *filename) { return NULL; }
static inline void blkid_free_probe(blkid_probe pr) {}
static inline int blkid_probe_enable_partitions(blkid_probe pr, int enable) { return -1; }
static inline int blkid_do_fullprobe(blkid_probe pr) { return -1; }
static inline int blkid_probe_lookup_value(blkid_probe pr, const char *name, const char **data, unsigned int *len) { return -1; }
static inline blkid_topology blkid_probe_get_topology(blkid_probe pr) { return NULL; }
static inline unsigned long blkid_topology_get_logical_sector_size(blkid_topology tp) { return 512; }
static inline unsigned long blkid_topology_get_physical_sector_size(blkid_topology tp) { return 512; }
static inline unsigned long blkid_topology_get_minimum_io_size(blkid_topology tp) { return 0; }
static inline unsigned long blkid_topology_get_optimal_io_size(blkid_topology tp) { return 0; }
static inline unsigned long blkid_topology_get_alignment_offset(blkid_topology tp) { return 0; }

#define XFS_NOT_DQATTACHED(mp, ip) (0)
static inline void rcu_init(void) {}

static inline char *hasmntopt(const struct mntent *mnt, const char *opt)
{
    char *s = mnt->mnt_opts;
    size_t len = strlen(opt);
    while (s && *s) {
        if (strncmp(s, opt, len) == 0 && (s[len] == '\0' || s[len] == ',' || s[len] == '='))
            return s;
        if (!(s = strchr(s, ','))) break;
        s++;
    }
    return NULL;
}

static inline void *reallocarray(void *ptr, size_t nmemb, size_t size) {
    size_t total;
    if (__builtin_mul_overflow(nmemb, size, &total)) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(ptr, total);
}

static inline int xfs_getsubopt(char **optionp, char *const *tokens, char **valuep) {
    char *p = *optionp;
    char *next;
    int i;

    if (!p || !*p) return -1;

    next = strchr(p, ',');
    if (next) *next++ = '\0';
    *optionp = next;

    *valuep = strchr(p, '=');
    if (*valuep) *(*valuep)++ = '\0';
    else *valuep = NULL;

    for (i = 0; tokens[i]; i++) {
        if (strcmp(p, tokens[i]) == 0) return i;
    }
    return -1;
}

#undef getsubopt
#define getsubopt xfs_getsubopt

#ifdef __cplusplus
}
#endif

#endif
