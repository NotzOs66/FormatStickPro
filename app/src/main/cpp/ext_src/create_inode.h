#ifndef _CREATE_INODE_H
#define _CREATE_INODE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "et/com_err.h"
#include "ext2fs/ext2fs.h"

struct fs_ops_callbacks {
	errcode_t (* create_new_inode)(ext2_filsys fs, const char *target_path,
		const char *name, ext2_ino_t parent_ino, ext2_ino_t root,
		mode_t mode);
	errcode_t (* end_create_new_inode)(ext2_filsys fs,
		const char *target_path, const char *name,
		ext2_ino_t parent_ino, ext2_ino_t root, mode_t mode);
};

#define POPULATE_FS_LINK_APPEND 0x0001
#define POPULATE_FS_NO_COPY_XATTRS 0x0002

extern errcode_t populate_fs3(ext2_filsys fs, ext2_ino_t parent_ino,
			      const char *source_dir, ext2_ino_t root,
			      int flags, struct fs_ops_callbacks *fs_callbacks);

#endif /* _CREATE_INODE_H */
