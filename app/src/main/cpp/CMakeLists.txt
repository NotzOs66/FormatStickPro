cmake_minimum_required(VERSION 3.22.1)

project("formatstickpro")

# ==============================================================================
# Construim macaraua: libusb
# ==============================================================================
add_library(usb STATIC
        libusb/core.c
        libusb/descriptor.c
        libusb/hotplug.c
        libusb/io.c
        libusb/sync.c
        libusb/strerror.c
        libusb/os/linux_usbfs.c
        libusb/os/linux_netlink.c
        libusb/os/threads_posix.c
        libusb/os/events_posix.c
)
target_compile_definitions(usb PRIVATE OS_LINUX _GNU_SOURCE)
target_include_directories(usb PUBLIC libusb)
target_link_libraries(usb log)

# ==============================================================================
# Construim ntfs-3g
# ==============================================================================
add_library(ntfs3g STATIC
        ntfs-3g-lib/libntfs-3g/acls.c
        ntfs-3g-lib/libntfs-3g/attrib.c
        ntfs-3g-lib/libntfs-3g/attrlist.c
        ntfs-3g-lib/libntfs-3g/bitmap.c
        ntfs-3g-lib/libntfs-3g/bootsect.c
        ntfs-3g-lib/libntfs-3g/cache.c
        ntfs-3g-lib/libntfs-3g/collate.c
        ntfs-3g-lib/libntfs-3g/compat.c
        ntfs-3g-lib/libntfs-3g/compress.c
        ntfs-3g-lib/libntfs-3g/debug.c
        ntfs-3g-lib/libntfs-3g/device.c
        ntfs-3g-lib/libntfs-3g/dir.c
        ntfs-3g-lib/libntfs-3g/ea.c
        ntfs-3g-lib/libntfs-3g/efs.c
        ntfs-3g-lib/libntfs-3g/index.c
        ntfs-3g-lib/libntfs-3g/inode.c
        ntfs-3g-lib/libntfs-3g/ioctl.c
        ntfs-3g-lib/libntfs-3g/lcnalloc.c
        ntfs-3g-lib/libntfs-3g/logfile.c
        ntfs-3g-lib/libntfs-3g/logging.c
        ntfs-3g-lib/libntfs-3g/mft.c
        ntfs-3g-lib/libntfs-3g/misc.c
        ntfs-3g-lib/libntfs-3g/mst.c
        ntfs-3g-lib/libntfs-3g/object_id.c
        ntfs-3g-lib/libntfs-3g/realpath.c
        ntfs-3g-lib/libntfs-3g/reparse.c
        ntfs-3g-lib/libntfs-3g/runlist.c
        ntfs-3g-lib/libntfs-3g/security.c
        ntfs-3g-lib/libntfs-3g/unistr.c
        ntfs-3g-lib/libntfs-3g/unix_io.c
        ntfs-3g-lib/libntfs-3g/volume.c
        ntfs-3g-lib/libntfs-3g/xattrs.c
        ntfs-3g-lib/ntfsprogs/utils.c
        ntfs-3g-lib/ntfsprogs/attrdef.c
        ntfs-3g-lib/ntfsprogs/sd.c
        ntfs-3g-lib/ntfsprogs/boot.c
)
target_compile_definitions(ntfs3g PRIVATE HAVE_CONFIG_H)
target_include_directories(ntfs3g PUBLIC
        ntfs-3g-lib/include/ntfs-3g
        ntfs-3g-lib/include
        ntfs-3g-lib
)

# ==============================================================================
# Construim exfat
# ==============================================================================
add_library(exfat STATIC
        exfat_src/cbm.c
        exfat_src/cluster.c
        exfat_src/fat.c
        exfat_src/log.c
        exfat_src/lookup.c
        exfat_src/mkexfat.c
        exfat_src/mount.c
        exfat_src/node.c
        exfat_src/repair.c
        exfat_src/rootdir.c
        exfat_src/time.c
        exfat_src/uct.c
        exfat_src/uctc.c
        exfat_src/utf.c
        exfat_src/utils.c
        exfat_src/vbr.c
)
target_compile_definitions(exfat PRIVATE
    HAVE_CONFIG_H
    _FILE_OFFSET_BITS=64
    PACKAGE="exfat-utils"
)
target_include_directories(exfat PUBLIC exfat_src)

# ==============================================================================
# Construim e2fsprogs (Ext2/3/4)
# ==============================================================================
add_library(ext2_com_err STATIC
        ext_src/et/com_err.c
        ext_src/et/error_message.c
        ext_src/et/et_name.c
)
target_include_directories(ext2_com_err PUBLIC ext_src/et ext_src)

add_library(ext2_uuid STATIC
        ext_src/uuid/clear.c
        ext_src/uuid/compare.c
        ext_src/uuid/copy.c
        ext_src/uuid/gen_uuid.c
        ext_src/uuid/isnull.c
        ext_src/uuid/pack.c
        ext_src/uuid/parse.c
        ext_src/uuid/unpack.c
        ext_src/uuid/unparse.c
        ext_src/uuid/uuid_time.c
)
target_include_directories(ext2_uuid PUBLIC ext_src/uuid ext_src ext_src/et)

add_library(ext2_blkid STATIC
        ext_src/blkid/cache.c
        ext_src/blkid/dev.c
        ext_src/blkid/devname.c
        ext_src/blkid/devno.c
        ext_src/blkid/getsize.c
        ext_src/blkid/llseek.c
        ext_src/blkid/probe.c
        ext_src/blkid/read.c
        ext_src/blkid/resolve.c
        ext_src/blkid/save.c
        ext_src/blkid/tag.c
        ext_src/blkid/version.c
)
target_include_directories(ext2_blkid PUBLIC ext_src/blkid ext_src ext_src/et)

add_library(ext2_e2p STATIC
        ext_src/e2p/feature.c
        ext_src/e2p/ostype.c
        ext_src/e2p/hashstr.c
        ext_src/e2p/ls.c
        ext_src/e2p/uuid.c
        ext_src/e2p/pe.c
        ext_src/e2p/ps.c
        ext_src/e2p/errcode.c
        ext_src/e2p/encoding.c
        ext_src/e2p/mntopts.c
        ext_src/e2p/parse_num.c
        ext_src/e2p/percent.c
)
target_include_directories(ext2_e2p PUBLIC ext_src/e2p ext_src ext_src/et)

add_library(ext2fs STATIC
        ext_src/ext2fs/alloc.c
        ext_src/ext2fs/alloc_sb.c
        ext_src/ext2fs/alloc_stats.c
        ext_src/ext2fs/alloc_tables.c
        ext_src/ext2fs/atexit.c
        ext_src/ext2fs/badblocks.c
        ext_src/ext2fs/bb_inode.c
        ext_src/ext2fs/bitmaps.c
        ext_src/ext2fs/bitops.c
        ext_src/ext2fs/blkmap64_ba.c
        ext_src/ext2fs/blkmap64_rb.c
        ext_src/ext2fs/blknum.c
        ext_src/ext2fs/block.c
        ext_src/ext2fs/bmap.c
        ext_src/ext2fs/check_desc.c
        ext_src/ext2fs/closefs.c
        ext_src/ext2fs/crc16.c
        ext_src/ext2fs/crc32c.c
        ext_src/ext2fs/csum.c
        ext_src/ext2fs/dblist.c
        ext_src/ext2fs/dblist_dir.c
        ext_src/ext2fs/dirblock.c
        ext_src/ext2fs/dirhash.c
        ext_src/ext2fs/dir_iterate.c
        ext_src/ext2fs/expanddir.c
        ext_src/ext2fs/ext_attr.c
        ext_src/ext2fs/extent.c
        ext_src/ext2fs/fallocate.c
        ext_src/ext2fs/fileio.c
        ext_src/ext2fs/finddev.c
        ext_src/ext2fs/flushb.c
        ext_src/ext2fs/freefs.c
        ext_src/ext2fs/gen_bitmap.c
        ext_src/ext2fs/gen_bitmap64.c
        ext_src/ext2fs/get_num_dirs.c
        ext_src/ext2fs/get_pathname.c
        ext_src/ext2fs/getsize.c
        ext_src/ext2fs/getsectsize.c
        ext_src/ext2fs/hashmap.c
        ext_src/ext2fs/i_block.c
        ext_src/ext2fs/icount.c
        ext_src/ext2fs/imager.c
        ext_src/ext2fs/ind_block.c
        ext_src/ext2fs/initialize.c
        ext_src/ext2fs/inline.c
        ext_src/ext2fs/inline_data.c
        ext_src/ext2fs/inode.c
        ext_src/ext2fs/io_manager.c
        ext_src/ext2fs/ismounted.c
        ext_src/ext2fs/link.c
        ext_src/ext2fs/llseek.c
        ext_src/ext2fs/lookup.c
        ext_src/ext2fs/mkdir.c
        ext_src/ext2fs/mkjournal.c
        ext_src/ext2fs/mmp.c
        ext_src/ext2fs/namei.c
        ext_src/ext2fs/native.c
        ext_src/ext2fs/newdir.c
        ext_src/ext2fs/nls_utf8.c
        ext_src/ext2fs/openfs.c
        ext_src/ext2fs/progress.c
        ext_src/ext2fs/punch.c
        ext_src/ext2fs/rbtree.c
        ext_src/ext2fs/read_bb.c
        ext_src/ext2fs/read_bb_file.c
        ext_src/ext2fs/res_gdt.c
        ext_src/ext2fs/rw_bitmaps.c
        ext_src/ext2fs/sha256.c
        ext_src/ext2fs/sha512.c
        ext_src/ext2fs/swapfs.c
        ext_src/ext2fs/symlink.c
        ext_src/ext2fs/undo_io.c
        ext_src/ext2fs/unix_io.c
        ext_src/ext2fs/unlink.c
        ext_src/ext2fs/valid_blk.c
        ext_src/ext2fs/version.c
        ext_src/ext2fs/ext2_err.c
        ext_src/ext2fs/getenv.c
        ext_src/ext2fs/sparse_io.c
        ext_src/ext2fs/orphan.c
        ext_src/ext2fs/tdb.c
        ext_src/ext2fs/digest_encode.c
        ext_src/ext2fs/inode_io.c
)
target_include_directories(ext2fs PUBLIC ext_src/ext2fs ext_src ext_src/et)
target_link_libraries(ext2fs ext2_com_err ext2_uuid)

# ==============================================================================
# Build F2FS separately to isolate its includes (config.h, types.h)
# ==============================================================================
add_library(f2fs_core STATIC
        f2fs_src/lib/libf2fs.c
        f2fs_src/lib/libf2fs_io.c
        f2fs_src/lib/libf2fs_zoned.c
        f2fs_src/lib/nls_utf8.c
        f2fs_src/mkfs/f2fs_format.c
        f2fs_src/mkfs/f2fs_format_main.c
        f2fs_src/mkfs/f2fs_format_utils.c
)
target_compile_definitions(f2fs_core PRIVATE
    HAVE_CONFIG_H
    _FILE_OFFSET_BITS=64
    PAGE_SIZE=4096
)
target_include_directories(f2fs_core PRIVATE
        f2fs_src/include
        ext_src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# ==============================================================================
# Construim UDF separately
# ==============================================================================
add_library(udf_core STATIC
        udf_src/libudffs/crc.c
        udf_src/libudffs/misc.c
        udf_src/libudffs/extent.c
        udf_src/libudffs/unicode.c
        udf_src/mkudffs/mkudffs.c
        udf_src/mkudffs/options.c
        udf_src/mkudffs/defaults.c
        udf_src/mkudffs/main.c
        udf_src/mkudffs/file.c
)
target_include_directories(udf_core PRIVATE
        udf_src/include
        udf_src/mkudffs
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(udf_core PRIVATE
    HAVE_CONFIG_H
    _FILE_OFFSET_BITS=64
)

# ==============================================================================
# Construim XFS separately
# ==============================================================================
add_library(xfs_core STATIC
        xfs_src/libxfs/init.c
        xfs_src/libxfs/kmem.c
        xfs_src/libxfs/rdwr.c
        xfs_src/libxfs/util.c
        xfs_src/libxfs/cache.c
        xfs_src/libxfs/inode.c
        xfs_src/libxfs/trans.c
        xfs_src/libxfs/xfile.c
        xfs_src/libxfs/xfblob.c
        xfs_src/libxfs/xfs_ag.c
        xfs_src/libxfs/xfs_sb.c
        xfs_src/libxfs/buf_mem.c
        xfs_src/libxfs/iunlink.c
        xfs_src/libxfs/logitem.c
        xfs_src/libxfs/xfs_bit.c
        xfs_src/libxfs/topology.c
        xfs_src/libxfs/xfs_attr.c
        xfs_src/libxfs/xfs_bmap.c
        xfs_src/libxfs/xfs_dir2.c
        xfs_src/libxfs/xfs_rmap.c
        xfs_src/libxfs/listxattr.c
        xfs_src/libxfs/xfs_alloc.c
        xfs_src/libxfs/xfs_btree.c
        xfs_src/libxfs/xfs_defer.c
        xfs_src/libxfs/xfs_group.c
        xfs_src/libxfs/xfs_types.c
        xfs_src/libxfs/xfs_zones.c
        xfs_src/libxfs/defer_item.c
        xfs_src/libxfs/xfs_ialloc.c
        xfs_src/libxfs/xfs_parent.c
        xfs_src/libxfs/xfs_ag_resv.c
        xfs_src/libxfs/xfs_dir2_sf.c
        xfs_src/libxfs/xfs_metadir.c
        xfs_src/libxfs/xfs_rtgroup.c
        xfs_src/libxfs/xfs_da_btree.c
        xfs_src/libxfs/xfs_exchmaps.c
        xfs_src/libxfs/xfs_metafile.c
        xfs_src/libxfs/xfs_refcount.c
        xfs_src/libxfs/xfs_rtbitmap.c
        xfs_src/libxfs/xfs_attr_leaf.c
        xfs_src/libxfs/xfs_btree_mem.c
        xfs_src/libxfs/xfs_dir2_data.c
        xfs_src/libxfs/xfs_dir2_leaf.c
        xfs_src/libxfs/xfs_dir2_node.c
        xfs_src/libxfs/xfs_dquot_buf.c
        xfs_src/libxfs/xfs_iext_tree.c
        xfs_src/libxfs/xfs_inode_buf.c
        xfs_src/libxfs/xfs_bmap_btree.c
        xfs_src/libxfs/xfs_dir2_block.c
        xfs_src/libxfs/xfs_inode_fork.c
        xfs_src/libxfs/xfs_inode_util.c
        xfs_src/libxfs/xfs_log_rlimit.c
        xfs_src/libxfs/xfs_rmap_btree.c
        xfs_src/libxfs/xfs_trans_resv.c
        xfs_src/libxfs/xfs_alloc_btree.c
        xfs_src/libxfs/xfs_attr_remote.c
        xfs_src/libxfs/xfs_trans_inode.c
        xfs_src/libxfs/xfs_trans_space.c
        xfs_src/libxfs/xfs_ialloc_btree.c
        xfs_src/libxfs/xfs_rtrmap_btree.c
        xfs_src/libxfs/xfs_btree_staging.c
        xfs_src/libxfs/xfs_refcount_btree.c
        xfs_src/libxfs/xfs_symlink_remote.c
        xfs_src/libxfs/xfs_rtrefcount_btree.c

        # libfrog
        xfs_src/libfrog/util.c
        xfs_src/libfrog/avl64.c
        xfs_src/libfrog/crc32.c
        xfs_src/libfrog/linux.c
        xfs_src/libfrog/paths.c
        xfs_src/libfrog/ptvar.c
        xfs_src/libfrog/zones.c
        xfs_src/libfrog/bitmap.c
        xfs_src/libfrog/fsgeom.c
        xfs_src/libfrog/convert.c
        xfs_src/libfrog/flagmap.c
        xfs_src/libfrog/fsprops.c
        xfs_src/libfrog/logging.c
        xfs_src/libfrog/bulkstat.c
        xfs_src/libfrog/projects.c
        xfs_src/libfrog/file_attr.c
        xfs_src/libfrog/histogram.c
        xfs_src/libfrog/list_sort.c
        xfs_src/libfrog/randbytes.c
        xfs_src/libfrog/workqueue.c
        xfs_src/libfrog/getparents.c
        xfs_src/libfrog/radix-tree.c
        xfs_src/libfrog/healthevent.c
        xfs_src/libfrog/fsproperties.c
        xfs_src/libfrog/file_exchange.c

        # libxcmd
        xfs_src/libxcmd/help.c
        xfs_src/libxcmd/quit.c
        xfs_src/libxcmd/input.c
        xfs_src/libxcmd/command.c

        # libxlog
        xfs_src/libxlog/util.c
        xfs_src/libxlog/xfs_log_recover.c

        # mkfs
        xfs_src/mkfs/proto.c
        xfs_src/mkfs/xfs_mkfs.c

        # local dependencies
        xfs_src/include/ini.c
)
target_include_directories(xfs_core PRIVATE
        xfs_src/include
        xfs_src/libxfs
        xfs_src/mkfs
        xfs_src
        ext_src/uuid
        ext_src/blkid
        ext_src
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(xfs_core PRIVATE
    HAVE_CONFIG_H
    _FILE_OFFSET_BITS=64
    PAGE_SIZE=4096
    OVERRIDE_SYSTEM_STATX
)
# XFS source code is heavily coupled with kernel, needs some slack on warnings/errors
target_compile_options(xfs_core PRIVATE -Wno-implicit-function-declaration -Wno-format)

# ==============================================================================
# Legăm macaraua de codul nostru principal
# ==============================================================================
add_library(${CMAKE_PROJECT_NAME} SHARED
        native-lib.cpp
        exfat_bridge.cpp
        ext_bridge.cpp
        f2fs_bridge.cpp
        udf_bridge.cpp
        xfs_bridge.cpp
        usb_scsi.cpp
        ntfs-3g-lib/ntfsprogs/mkntfs.c
        ntfs-3g-lib/ntfsprogs/utils.c
        # mke2fs core
        ext_src/mke2fs.c
        ext_src/util.c
        ext_src/mk_hugefiles.c
        ext_src/default_profile.c
        ext_src/support/devname.c
        ext_src/support/profile.c
        ext_src/support/prof_err.c
        ext_src/support/parse_qtype.c
        ext_src/support/plausible.c
        ext_src/support/mkquota.c
        ext_src/support/quotaio.c
        ext_src/support/quotaio_tree.c
        ext_src/support/quotaio_v2.c
        ext_src/support/dict.c
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    HAVE_CONFIG_H
    _FILE_OFFSET_BITS=64
    PAGE_SIZE=4096
)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -fvisibility=hidden)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ntfs-3g-lib/include/ntfs-3g
        ntfs-3g-lib/include
        ntfs-3g-lib
        exfat_src
        f2fs_src/include
        ext_src
        ext_src/ext2fs
        ext_src/et
        ext_src/support
        ext_src/uuid
        xfs_src/include
)

target_link_libraries(${CMAKE_PROJECT_NAME}
        android
        log
        usb
        ntfs3g
        exfat
        ext2fs
        ext2_blkid
        ext2_e2p
        f2fs_core
        udf_core
        xfs_core
)
