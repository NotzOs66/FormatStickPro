#ifndef MKE2FS_H
#define MKE2FS_H

#include "ext2_config.h"
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_io.h>
#include <blkid/blkid.h>

#ifdef __cplusplus
extern "C" {
#endif

/* mke2fs.c */
extern const char * program_name;
extern int	quiet;
extern int	verbose;
extern int	zero_hugefile;
extern char **fs_types;
extern int  journal_size;
extern int  journal_flags;
extern int  journal_fc_size;
extern char *journal_device;
extern char *journal_location_string;

extern char *get_string_from_profile(char **types, const char *opt,
				     const char *def_val);
extern int get_int_from_profile(char **types, const char *opt, int def_val);
extern int get_bool_from_profile(char **types, const char *opt, int def_val);
extern int int_log10(unsigned long long arg);

/* mk_hugefiles.c */
extern errcode_t mk_hugefiles(ext2_filsys fs, const char *device_name);

/* Missing exports for bridge and linking */
extern io_manager sparse_io_manager;
extern const char *mke2fs_default_profile;
extern char *get_devname(blkid_cache cache, const char *token, const char *value);
extern unsigned long parse_num_blocks(const char *arg, int log_block_size);
extern unsigned long long parse_num_blocks2(const char *arg, int log_block_size);

#ifdef __cplusplus
}
#endif

#endif

#include <setjmp.h>
extern jmp_buf g_mke2fs_jmp;
extern io_manager usb_scsi_io_manager;
#include <stdio.h>
extern void redirect_stderr_to_logcat();
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
extern int ext2fs_open_file(const char *pathname, int flags, mode_t mode);
