/*
 * Copyright (C) 2026 FormatStickPro (NotzOs66)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this <https://www.gnu.org/licenses/>.
 */

#ifndef USB_SCSI_H
#define USB_SCSI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libusb/libusb.h"
#include <stdint.h>

// Forward declaration to avoid header conflicts
struct ntfs_device_operations;

/**
 * This is the structure that "tricks" NTFS-3G into using our translator.
 */
extern struct ntfs_device_operations ntfs_device_usb_scsi_ops;

/**
 * Initializes the driver for a specific libusb handle.
 */
void usb_scsi_init(libusb_device_handle *handle);

/**
 * Wipes the beginning of the stick (zero out).
 */
int usb_scsi_wipe_mbr();

/**
 * Creates a standard MBR partition table.
 */
int usb_scsi_write_mbr(uint64_t partition_sectors, uint8_t partition_type);

/**
 * Creates a standard GPT partition table.
 */
int usb_scsi_write_gpt(uint64_t partition_sectors, int fsType);

/**
 * Wipes the end of the stick (GPT Backup).
 */
int usb_scsi_wipe_tail();

/**
 * Synchronizes the stick's cache.
 */
int usb_scsi_sync();

/**
 * Wipes the partition start.
 */
typedef void (*progress_callback)(int);
int usb_scsi_wipe_partition_head(int fsType, progress_callback cb);

/**
 * Sets the partition size and offset.
 */
void usb_scsi_set_partition_size(uint64_t sectors);
void usb_scsi_set_partition_offset(uint64_t sector_offset);
uint64_t usb_scsi_get_partition_offset();
void usb_scsi_set_partition_type(int type);

/**
 * Returns the number of sectors (total or partition if set).
 */
uint64_t usb_scsi_get_sector_count();
uint64_t usb_scsi_get_usable_sectors();

/**
 * Formats the stick.
 */
int usb_scsi_format_fat(int fat_type, uint32_t cluster_size, const char* label);
int usb_scsi_format_exfat(uint32_t cluster_size, const char* label);

/**
 * Helpers for bridges (exFAT, XFS).
 */
int64_t usb_scsi_read_internal(void* buf, int64_t count);
int64_t usb_scsi_write_internal(const void* buf, int64_t count);
int64_t usb_scsi_pread_internal(void* buf, int64_t count, int64_t offset);
int64_t usb_scsi_pwrite_internal(const void* buf, int64_t count, int64_t offset);
int64_t usb_scsi_lseek_internal(int64_t offset, int whence);

#ifdef __cplusplus
}
#endif

#endif // USB_SCSI_H
