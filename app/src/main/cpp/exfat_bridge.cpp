/*
 * Copyright (C) 2026 FormatStickPro (NotzOs66)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <android/log.h>
#include "usb_scsi.h"

/**
 * Nota de autor: "format Stick Pro creat de Șpac Dumitru"
 */
extern "C" const char* get_exfat_bridge_auth() {
    return "Zm9ybWF0IFN0aWNrIFBybyBjcmVhdCBkZSDFn3BhYyBEdW1pdHJ1";
}

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "exFAT-Bridge", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "exFAT-Bridge", __VA_ARGS__)

extern "C" {

// Satisfy C99 requirement for exFAT headers
#ifndef __STDC_VERSION__
#define __STDC_VERSION__ 199901L
#endif

#include <exfat.h>
#include "vbr.h"
#include "fat.h"
#include "cbm.h"
#include "uct.h"
#include "rootdir.h"

// exfat_dev structure required by the library
struct exfat_dev {
    int64_t pos;
};

// Global parameters required by the mkfs engine (from main.c)
static struct {
    int sector_bits;
    int spc_bits;
    int64_t volume_size;
    le16_t volume_label[EXFAT_ENAME_MAX + 1];
    uint32_t volume_serial;
    uint64_t first_sector;
} param;

// Definition of the object array required by mkfs
const struct fs_object* objects[] = {
    &vbr,
    &vbr,
    &fat,
    /* clusters heap */
    &cbm,
    &uct,
    &rootdir,
    NULL,
};

// Implementation of the getters required by the library
int get_sector_bits(void) { return param.sector_bits; }
int get_spc_bits(void) { return param.spc_bits; }
off_t get_volume_size(void) { return (off_t)param.volume_size; }
const le16_t* get_volume_label(void) { return param.volume_label; }
uint32_t get_volume_serial(void) { return param.volume_serial; }
uint64_t get_first_sector(void) { return param.first_sector; }
int get_sector_size(void) { return 1 << get_sector_bits(); }
int get_cluster_size(void) { return (int)((uint64_t)get_sector_size() << get_spc_bits()); }

// Bridge to the SCSI Driver
struct exfat_dev* exfat_open(const char* spec, enum exfat_mode mode) {
    struct exfat_dev* dev = (struct exfat_dev*)malloc(sizeof(struct exfat_dev));
    if (dev) dev->pos = 0;
    return dev;
}

int exfat_close(struct exfat_dev* dev) {
    if (dev) free(dev);
    return 0;
}

int exfat_fsync(struct exfat_dev* dev) { return 0; }
enum exfat_mode exfat_get_mode(const struct exfat_dev* dev) { return EXFAT_MODE_RW; }
off_t exfat_get_size(const struct exfat_dev* dev) { return (off_t)param.volume_size; }

off_t exfat_seek(struct exfat_dev* dev, off_t offset, int whence) {
    if (whence == SEEK_SET) dev->pos = offset;
    else if (whence == SEEK_CUR) dev->pos += offset;
    else if (whence == SEEK_END) dev->pos = param.volume_size + offset;
    return (off_t)dev->pos;
}

ssize_t exfat_read(struct exfat_dev* dev, void* buffer, size_t size) {
    int64_t res = usb_scsi_pread_internal(buffer, (int64_t)size, dev->pos);
    if (res > 0) dev->pos += res;
    return (ssize_t)res;
}

ssize_t exfat_write(struct exfat_dev* dev, const void* buffer, size_t size) {
    int64_t res = usb_scsi_pwrite_internal(buffer, (int64_t)size, dev->pos);
    if (res > 0) dev->pos += res;
    return (ssize_t)res;
}

ssize_t exfat_pread(struct exfat_dev* dev, void* buffer, size_t size, off_t offset) {
    return (ssize_t)usb_scsi_pread_internal(buffer, (int64_t)size, (int64_t)offset);
}

ssize_t exfat_pwrite(struct exfat_dev* dev, const void* buffer, size_t size, off_t offset) {
    return (ssize_t)usb_scsi_pwrite_internal(buffer, (int64_t)size, (int64_t)offset);
}

// Logic for initializing exFAT parameters
static int setup_spc_bits(int sector_bits, int user_defined, int64_t volume_size) {
    if (user_defined > 0) {
        int bits = 0;
        while ((1 << bits) < user_defined) bits++;
        return bits;
    }
    if (volume_size < 256LL * 1024 * 1024) return 12 - sector_bits; // 4KB
    return 15 - sector_bits; // 32KB
}

int mkfs(struct exfat_dev* dev, off_t volume_size); // mkexfat.c

int usb_scsi_format_exfat(uint32_t cluster_size, const char* label) {
    LOGI("Starting exFAT formatting...");

    param.volume_size = (int64_t)usb_scsi_get_usable_sectors() * 512;
    param.sector_bits = 9; // 512 bytes
    param.spc_bits = setup_spc_bits(9, (int)(cluster_size / 512), param.volume_size);
    param.volume_serial = (uint32_t)rand();
    param.first_sector = 2048;

    memset(param.volume_label, 0, sizeof(param.volume_label));
    exfat_utf8_to_utf16(param.volume_label, label, EXFAT_ENAME_MAX + 1, strlen(label));

    struct exfat_dev* dev = exfat_open("usb", EXFAT_MODE_RW);
    int res = mkfs(dev, (off_t)param.volume_size);
    exfat_close(dev);

    return res;
}

} // extern "C"
