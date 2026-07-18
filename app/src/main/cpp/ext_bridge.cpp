/*
 * Copyright (C) 2026 FormatStickPro (NotzOs66)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <android/log.h>
#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "ext2fs/ext2_io.h"
#include "usb_scsi.h"

/**
 * Nota de autor: "format Stick Pro creat de Șpac Dumitru"
 */
extern "C" const char* get_ext_bridge_auth() {
    return "Zm9ybWF0IFN0aWNrIFBybyBjcmVhdCBkZSDFn3BhYyBEdW1pdHJ1";
}

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "EXT-BRIDGE", __VA_ARGS__)

static errcode_t scsi_open(const char *name, int flags, io_channel *channel);
static errcode_t scsi_close(io_channel channel);
static errcode_t scsi_set_blksize(io_channel channel, int blksize);
static errcode_t scsi_read_blk(io_channel channel, unsigned long block, int count, void *data);
static errcode_t scsi_write_blk(io_channel channel, unsigned long block, int count, const void *data);
static errcode_t scsi_flush(io_channel channel);
static errcode_t scsi_write_byte(io_channel channel, unsigned long offset, int count, const void *data);
static errcode_t scsi_set_option(io_channel channel, const char *option, const char *arg);
static errcode_t scsi_get_stats(io_channel channel, io_stats *stats);
static errcode_t scsi_read_blk64(io_channel channel, unsigned long long block, int count, void *data);
static errcode_t scsi_write_blk64(io_channel channel, unsigned long long block, int count, const void *data);
static errcode_t scsi_discard(io_channel channel, unsigned long long block, unsigned long long count);

static struct struct_io_manager struct_scsi_manager = {
    EXT2_ET_MAGIC_IO_MANAGER,
    "USB SCSI IO Manager",
    scsi_open,
    scsi_close,
    scsi_set_blksize,
    scsi_read_blk,
    scsi_write_blk,
    scsi_flush,
    scsi_write_byte,
    scsi_set_option,
    scsi_get_stats,
    scsi_read_blk64,
    scsi_write_blk64,
    scsi_discard,
    NULL, // cache_readahead
    NULL, // zeroout
    {0}   // reserved
};

io_manager usb_scsi_io_manager = &struct_scsi_manager;

static errcode_t scsi_open(const char *name, int flags, io_channel *channel) {
    io_channel io;
    if (!name) return EXT2_ET_BAD_DEVICE_NAME;

    io = (io_channel)calloc(1, sizeof(struct struct_io_channel));
    if (!io) return ENOMEM;

    io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
    io->manager = usb_scsi_io_manager;
    io->name = strdup(name);
    io->block_size = 4096;
    io->refcount = 1;

    *channel = io;
    LOGI("Opened SCSI channel for %s", name);
    return 0;
}

static errcode_t scsi_close(io_channel channel) {
    if (channel->magic != EXT2_ET_MAGIC_IO_CHANNEL) return EXT2_ET_MAGIC_IO_CHANNEL;
    if (--channel->refcount > 0) return 0;

    if (channel->name) free(channel->name);
    free(channel);
    return 0;
}

static errcode_t scsi_set_blksize(io_channel channel, int blksize) {
    channel->block_size = blksize;
    return 0;
}

static errcode_t scsi_read_blk64(io_channel channel, unsigned long long block, int count, void *data) {
    int64_t offset = (int64_t)block * channel->block_size;
    int64_t size = (int64_t)count * channel->block_size;

    if (count < 0) {
        size = -count;
    }

    int64_t ret = usb_scsi_pread_internal(data, size, offset);
    return (ret == size) ? 0 : EXT2_ET_SHORT_READ;
}

static errcode_t scsi_read_blk(io_channel channel, unsigned long block, int count, void *data) {
    return scsi_read_blk64(channel, block, count, data);
}

static errcode_t scsi_write_blk64(io_channel channel, unsigned long long block, int count, const void *data) {
    int64_t offset = (int64_t)block * channel->block_size;
    int64_t size = (int64_t)count * channel->block_size;

    if (count < 0) {
        size = -count;
    }

    int64_t ret = usb_scsi_pwrite_internal(data, size, offset);
    return (ret == size) ? 0 : EXT2_ET_SHORT_WRITE;
}

static errcode_t scsi_write_blk(io_channel channel, unsigned long block, int count, const void *data) {
    return scsi_write_blk64(channel, block, count, data);
}

static errcode_t scsi_write_byte(io_channel channel, unsigned long offset, int count, const void *data) {
    int64_t ret = usb_scsi_pwrite_internal(data, count, offset);
    return (ret == count) ? 0 : EXT2_ET_SHORT_WRITE;
}

static errcode_t scsi_flush(io_channel channel) {
    return 0;
}

static errcode_t scsi_set_option(io_channel channel, const char *option, const char *arg) {
    return 0;
}

static errcode_t scsi_get_stats(io_channel channel, io_stats *stats) {
    return 0;
}

static errcode_t scsi_discard(io_channel channel, unsigned long long block, unsigned long long count) {
    return 0;
}
