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

#include "usb_scsi.h"
#include "ntfs-3g/device.h"
#include <android/log.h>
#include <cstring>
#include <errno.h>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <cstdlib>
#include <cstdio>
#include <ctime>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "USB-SCSI", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "USB-SCSI", __VA_ARGS__)

extern "C" {

// --- USB Mass Storage (BOT) Structures ---
struct cbw {
    uint32_t signature;
    uint32_t tag;
    uint32_t data_transfer_length;
    uint8_t flags;
    uint8_t lun;
    uint8_t cmd_length;
    uint8_t cmd[16];
} __attribute__((packed));

struct csw {
    uint32_t signature;
    uint32_t tag;
    uint32_t data_residue;
    uint8_t status;
} __attribute__((packed));

// --- Driver State ---
static libusb_device_handle *g_usb_handle = nullptr;
static uint8_t g_ep_in = 0;
static uint8_t g_ep_out = 0;
static uint32_t g_tag = 1;
static uint64_t g_sector_count = 0;
static uint32_t g_sector_size = 512;
static int64_t g_cur_pos = 0;
static uint64_t g_drive_offset_sectors = 0;
static uint64_t g_partition_sectors = 0;
static int g_partition_type = 0;

static uint8_t g_bounce_buffer[1048576]; // 1MB buffer for stability
static uint8_t* g_write_cache = nullptr;
static uint64_t g_cache_lba = 0;
static uint32_t g_cache_sectors = 0;
static const uint32_t K_MAX_CACHE_SECTORS = 2048; // 1MB cache block

static int64_t g_total_io_bytes = 0;
static int64_t g_last_log_io_bytes = 0;

/**
 * Această funcție stochează semnătura autorului original sub formă codificată.
 * Șirul conține: "format Stick Pro creat de Șpac Dumitru"
 */
extern "C" const char* get_secure_bridge_auth() {
    return "Zm9ybWF0IFN0aWNrIFBybyBjcmVhdCBkZSDFn3BhYyBEdW1pdHJ1";
}

// --- SCSI Helpers ---

static int usb_transfer(uint8_t ep, unsigned char *data, int len, int *actual) {
    if (!g_usb_handle) {
        errno = ENODEV;
        return LIBUSB_ERROR_NO_DEVICE;
    }
    // Increased timeout to 60 seconds to allow slow sticks to process writes
    int res = libusb_bulk_transfer(g_usb_handle, ep, data, len, actual, 60000);
    if (res != 0) {
        if (res == LIBUSB_ERROR_NO_DEVICE) errno = ENODEV;
        else if (res == LIBUSB_ERROR_TIMEOUT) errno = ETIMEDOUT;
        else if (res == LIBUSB_ERROR_PIPE) {
            libusb_clear_halt(g_usb_handle, ep);
            usleep(100000); // 100ms pause for hardware to recover
            errno = EPIPE;
        } else errno = EIO;
    }
    return res;
}

static bool g_sync_cache_supported = true;

static int scsi_command(uint8_t *cmd, uint8_t cmd_len, uint8_t direction, unsigned char *data, uint32_t data_len) {
    if (cmd[0] == 0x35 && !g_sync_cache_supported) return 0;

    struct cbw cbw_pkt;
    memset(&cbw_pkt, 0, sizeof(cbw_pkt));
    cbw_pkt.signature = 0x43425355; // "USBC"
    cbw_pkt.tag = g_tag++;
    cbw_pkt.data_transfer_length = data_len;
    cbw_pkt.flags = direction; // 0x80 for IN, 0x00 for OUT
    cbw_pkt.cmd_length = cmd_len;
    memcpy(cbw_pkt.cmd, cmd, cmd_len);

    int sent;
    if (usb_transfer(g_ep_out, (unsigned char *)&cbw_pkt, sizeof(cbw_pkt), &sent) != 0) return -1;

    int actual;
    if (data_len > 0 && data != nullptr) {
        uint8_t ep = (direction == 0x80) ? g_ep_in : g_ep_out;
        int res_data = usb_transfer(ep, data, (int)data_len, &actual);
        if (res_data != 0) {
            LOGE("SCSI Data Phase Error: Tag %u, Dir %02x, Len %u, Libusb: %d (%s)",
                 cbw_pkt.tag, direction, data_len, res_data, libusb_strerror((libusb_error)res_data));
            return -1;
        }
    }

    struct csw csw_pkt;
    memset(&csw_pkt, 0, sizeof(csw_pkt));
    if (usb_transfer(g_ep_in, (unsigned char *)&csw_pkt, sizeof(csw_pkt), &actual) != 0) {
        LOGE("SCSI Status Phase Error: Tag %u", cbw_pkt.tag);
        return -1;
    }

    if (csw_pkt.status != 0) {
        if (cmd[0] == 0x35) {
            LOGI("SCSI SYNCHRONIZE CACHE failed (normal for some sticks). Disabling it.");
            g_sync_cache_supported = false;
            return 0;
        }
        LOGE("SCSI Command Failed: Tag %u, Status %02x, Opcode %02x", cbw_pkt.tag, csw_pkt.status, cmd[0]);

        // Request Sense if failed
        uint8_t sense_cmd[6] = {0x03, 0, 0, 0, 18, 0};
        uint8_t sense_data[18];
        struct cbw scbw;
        memset(&scbw, 0, sizeof(scbw));
        scbw.signature = 0x43425355; scbw.tag = g_tag++;
        scbw.data_transfer_length = 18; scbw.flags = 0x80; scbw.cmd_length = 6;
        memcpy(scbw.cmd, sense_cmd, 6);

        if (usb_transfer(g_ep_out, (unsigned char *)&scbw, sizeof(scbw), &sent) == 0) {
            usb_transfer(g_ep_in, sense_data, 18, &actual);
            struct csw scsw;
            usb_transfer(g_ep_in, (unsigned char *)&scsw, sizeof(scsw), &actual);
            LOGE("Sense Key: %02x, ASC: %02x, ASCQ: %02x", sense_data[2] & 0x0F, sense_data[12], sense_data[13]);
        }

        errno = EIO;
        return -1;
    }
    return 0;
}

static int scsi_flush_cache_internal() {
    if (g_cache_sectors == 0 || !g_write_cache) return 0;

    uint8_t w_cmd[10] = {0x2a, 0, (uint8_t)(g_cache_lba >> 24), (uint8_t)(g_cache_lba >> 16), (uint8_t)(g_cache_lba >> 8), (uint8_t)g_cache_lba, 0, (uint8_t)(g_cache_sectors >> 8), (uint8_t)g_cache_sectors, 0};
    int res = scsi_command(w_cmd, 10, 0x00, g_write_cache, g_cache_sectors * g_sector_size);

    g_cache_sectors = 0;
    return res;
}

static int64_t scsi_io_internal(void *buf, int64_t count, int64_t offset, bool is_write) {
    int64_t actual_offset = offset + (int64_t)(g_drive_offset_sectors * g_sector_size);
    int64_t total_done = 0;
    while (total_done < count) {
        uint32_t lba = (uint32_t)((actual_offset + total_done) / g_sector_size);
        uint32_t offset_in_sector = (uint32_t)((actual_offset + total_done) % g_sector_size);
        uint32_t remaining = (uint32_t)(count - total_done);

        if (!is_write) {
            scsi_flush_cache_internal(); // Reading forces write cache flush
            uint32_t sectors = (remaining + offset_in_sector + g_sector_size - 1) / g_sector_size;
            if (sectors > 2048) sectors = 2048; // 1MB

            uint8_t cmd[10] = {0x28, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba, 0, (uint8_t)(sectors >> 8), (uint8_t)sectors, 0};
            if (scsi_command(cmd, 10, 0x80, g_bounce_buffer, sectors * g_sector_size) != 0) return -1;

            uint32_t can_copy = (sectors * g_sector_size) - offset_in_sector;
            uint32_t to_copy = (remaining < can_copy) ? remaining : can_copy;
            memcpy((uint8_t*)buf + total_done, g_bounce_buffer + offset_in_sector, to_copy);
            total_done += to_copy;
        } else {
            // Write Caching Logic (Ported from Beta Engine for Stability on EXT4)
            if (offset_in_sector == 0 && remaining >= g_sector_size) {
                uint32_t sectors_to_process = remaining / g_sector_size;
                if (sectors_to_process > 2048) sectors_to_process = 2048; // 1MB limit

                // Cache aggregation
                bool can_add = (g_cache_sectors > 0 && lba == g_cache_lba + g_cache_sectors && (g_cache_sectors + sectors_to_process <= K_MAX_CACHE_SECTORS));

                if (can_add) {
                    memcpy(g_write_cache + g_cache_sectors * g_sector_size, (uint8_t*)buf + total_done, sectors_to_process * g_sector_size);
                    g_cache_sectors += sectors_to_process;
                } else {
                    scsi_flush_cache_internal();
                    if (!g_write_cache) g_write_cache = (uint8_t*)malloc(K_MAX_CACHE_SECTORS * g_sector_size);
                    if (!g_write_cache) return -1;
                    g_cache_lba = lba;
                    memcpy(g_write_cache, (uint8_t*)buf + total_done, sectors_to_process * g_sector_size);
                    g_cache_sectors = sectors_to_process;
                }
                total_done += (sectors_to_process * g_sector_size);
            } else {
                scsi_flush_cache_internal();
                uint32_t to_write = (g_sector_size - offset_in_sector < remaining) ? (g_sector_size - offset_in_sector) : remaining;
                uint8_t r_cmd[10] = {0x28, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba, 0, 0, 1, 0};
                if (scsi_command(r_cmd, 10, 0x80, g_bounce_buffer, g_sector_size) != 0) return -1;
                memcpy(g_bounce_buffer + offset_in_sector, (uint8_t*)buf + total_done, to_write);
                uint8_t w_cmd[10] = {0x2a, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba, 0, 0, 1, 0};
                if (scsi_command(w_cmd, 10, 0x00, g_bounce_buffer, g_sector_size) != 0) return -1;
                total_done += to_write;
            }
        }
    }

    g_total_io_bytes += total_done;
    if (g_total_io_bytes - g_last_log_io_bytes > 10 * 1024 * 1024) {
        LOGI("I/O Progress: %llu MB processed...", (unsigned long long)(g_total_io_bytes / (1024 * 1024)));
        g_last_log_io_bytes = g_total_io_bytes;
    }

    return total_done;
}

// --- Driver Implementation ---

struct hd_geometry {
    unsigned char heads;
    unsigned char sectors;
    unsigned short cylinders;
    unsigned long start;
};

void usb_scsi_init(libusb_device_handle *handle) {
    g_usb_handle = handle;
    g_drive_offset_sectors = 0;
    g_partition_sectors = 0;
    g_partition_type = 0;
    g_cur_pos = 0;
    g_total_io_bytes = 0;
    g_last_log_io_bytes = 0;

    libusb_device *dev = libusb_get_device(handle);
    struct libusb_config_descriptor *conf;

    libusb_set_auto_detach_kernel_driver(handle, 1);

    if (libusb_get_active_config_descriptor(dev, &conf) != 0) return;

    g_ep_in = 0; g_ep_out = 0;
    for (int i = 0; i < conf->bNumInterfaces; i++) {
        const struct libusb_interface_descriptor *iface = &conf->interface[i].altsetting[0];
        if (iface->bInterfaceClass == 0x08) {
            libusb_claim_interface(handle, iface->bInterfaceNumber);
            for (int e = 0; e < iface->bNumEndpoints; e++) {
                const struct libusb_endpoint_descriptor *ep = &iface->endpoint[e];
                if ((ep->bmAttributes & 0x03) == 0x02) {
                    if (ep->bEndpointAddress & 0x80) g_ep_in = ep->bEndpointAddress;
                    else g_ep_out = ep->bEndpointAddress;
                }
            }
            break;
        }
    }
    libusb_free_config_descriptor(conf);

    if (g_ep_in == 0 || g_ep_out == 0) {
        LOGE("Error: BULK endpoints not found!");
        return;
    }

    LOGI("Endpoints found: IN=%02x, OUT=%02x", g_ep_in, g_ep_out);

    // 1. INQUIRY (Opcode 0x12)
    uint8_t inq_cmd[6] = {0x12, 0, 0, 0, 36, 0};
    uint8_t inq_res[36] = {0};
    if (scsi_command(inq_cmd, 6, 0x80, inq_res, 36) == 0) {
        char vendor[9], product[17], revision[5];
        memcpy(vendor, &inq_res[8], 8); vendor[8] = 0;
        memcpy(product, &inq_res[16], 16); product[16] = 0;
        memcpy(revision, &inq_res[32], 4); revision[4] = 0;
        LOGI("SCSI Inquiry: [%s] [%s] [%s]", vendor, product, revision);
    } else {
        LOGE("SCSI Inquiry failed!");
    }

    // 2. TEST UNIT READY (Opcode 0x00)
    uint8_t tur_cmd[6] = {0x00, 0, 0, 0, 0, 0};
    for (int i = 0; i < 3; i++) {
        if (scsi_command(tur_cmd, 6, 0x00, nullptr, 0) == 0) break;
        usleep(500000);
    }

    // 3. READ CAPACITY (10) (Opcode 0x25)
    for (int retry = 0; retry < 5; retry++) {
        uint8_t cmd[10] = {0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint8_t res[8] = {0};
        if (scsi_command(cmd, 10, 0x80, res, 8) == 0) {
            uint32_t last_lba = ((uint32_t)res[0] << 24) | (res[1] << 16) | (res[2] << 8) | res[3];
            g_sector_size = ((uint32_t)res[4] << 24) | (res[5] << 16) | (res[6] << 8) | res[7];
            g_sector_count = (uint64_t)last_lba + 1;
            LOGI("Stick OK! Sectors: %llu, Size: %u", (unsigned long long)g_sector_count, g_sector_size);
            return;
        }
        usleep(1000000);
    }
}

// --- GPT Helpers ---

static uint32_t crc32_table[256];
static void init_crc32_table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) c = 0xedb88320L ^ (c >> 1);
            else c = c >> 1;
        }
        crc32_table[i] = c;
    }
}

static uint32_t crc32(uint32_t crc, const unsigned char *buf, size_t len) {
    crc = ~crc;
    while (len--) crc = crc32_table[(crc ^ *buf++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

int usb_scsi_wipe_tail() {
    LOGI("Wiping end of stick (GPT backup)...");
    uint8_t zero_buf[512] = {0};
    uint64_t last_lba = g_sector_count - 1;
    // Wipe last 34 sectors
    for (uint64_t lba = last_lba; lba > last_lba - 34; lba--) {
        uint8_t write_cmd[10] = {0x2a, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba, 0, 0, 1, 0};
        if (scsi_command(write_cmd, 10, 0x00, zero_buf, 512) != 0) break;
    }
    uint8_t final_sync[10] = {0x35};
    scsi_command(final_sync, 10, 0x00, nullptr, 0);
    return 0;
}


int usb_scsi_write_gpt(uint64_t partition_sectors, int fsType) {
    LOGI("Creating GPT Partition Table (%llu sectors)...", (unsigned long long)partition_sectors);
    init_crc32_table();

    // 0. Seed random for GUIDs
    srand((unsigned int)(time(nullptr) ^ g_tag));

    // 1. Protective MBR at LBA 0
    uint8_t mbr[512] = {0};
    mbr[440] = (uint8_t)rand(); mbr[441] = (uint8_t)rand();
    mbr[442] = (uint8_t)rand(); mbr[443] = (uint8_t)rand(); // Disk Sig

    uint8_t *p = &mbr[446];
    p[0] = 0x00; // Boot indicator
    p[1] = 0x00; p[2] = 0x02; p[3] = 0x00; // CHS Start: 0/0/2
    p[4] = 0xEE; // Type GPT
    p[5] = 0xFF; p[6] = 0xFF; p[7] = 0xFF; // CHS End: Max

    uint32_t start_lba_p = 1;
    memcpy(&p[8], &start_lba_p, 4);
    uint32_t mbr_size = (g_sector_count > 0xFFFFFFFF) ? 0xFFFFFFFF : (uint32_t)g_sector_count - 1;
    memcpy(&p[12], &mbr_size, 4);

    mbr[510] = 0x55; mbr[511] = 0xAA;

    uint8_t cmd0[10] = {0x2a, 0, 0, 0, 0, 0, 0, 0, 1, 0};
    if (scsi_command(cmd0, 10, 0x00, mbr, 512) != 0) return -1;

    // 2. Partition Entry
    uint8_t entries[512 * 32] = {0}; // 128 entries * 128 bytes
    // Basic Data Partition GUID: EBD0A0A2-B9E5-4433-87C0-68B6B72699C7
    static uint8_t ntfs_type_guid[16] = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};
    // Linux Data Partition GUID: 0FC63DAF-8483-4772-8E79-3D69D8477DE4
    static uint8_t linux_data_guid[16] = {0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47, 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4};

    uint8_t* type_guid = (fsType == 8 || (fsType >= 5 && fsType <= 7)) ? linux_data_guid : ntfs_type_guid;
    memcpy(&entries[0], type_guid, 16);
    // Random Partition GUID
    for (int i=16; i<32; i++) entries[i] = (uint8_t)rand();
    uint64_t p_start_lba = 4096; // 2MB align for Flash performance and F2FS segments
    uint64_t p_end_lba = p_start_lba + partition_sectors - 1;
    if (p_end_lba > g_sector_count - 34) {
        p_end_lba = g_sector_count - 34;
        p_end_lba = ((p_end_lba + 1) & ~7ULL) - 1; // Align to 8-sector boundary
    }

    memcpy(&entries[32], &p_start_lba, 8);
    memcpy(&entries[40], &p_end_lba, 8);
    // Attributes: 0
    // Name: "Basic data partition" in UTF-16LE
    const char16_t *name = u"Basic data partition";
    memcpy(&entries[56], name, 40);

    uint32_t entries_crc = crc32(0, entries, sizeof(entries));

    // 3. GPT Header Primary at LBA 1
    uint8_t header[512] = {0};
    memcpy(&header[0], "EFI PART", 8);
    uint32_t rev = 0x00010000; memcpy(&header[8], &rev, 4);
    uint32_t h_size = 92; memcpy(&header[12], &h_size, 4);
    uint64_t current_lba = 1; memcpy(&header[24], &current_lba, 8);
    uint64_t backup_lba = g_sector_count - 1; memcpy(&header[32], &backup_lba, 8);
    uint64_t first_usable = 34; memcpy(&header[40], &first_usable, 8);
    uint64_t last_usable = g_sector_count - 34; memcpy(&header[48], &last_usable, 8);
    // Random Disk GUID
    for (int i=56; i<72; i++) header[i] = (uint8_t)rand();
    uint64_t entry_lba = 2; memcpy(&header[72], &entry_lba, 8);
    uint32_t num_entries = 128; memcpy(&header[80], &num_entries, 4);
    uint32_t entry_size = 128; memcpy(&header[84], &entry_size, 4);
    memcpy(&header[88], &entries_crc, 4);

    // Calculate CRC (bytes 16-19 must be 0 during calculation)
    uint32_t header_crc = crc32(0, header, 92);
    memcpy(&header[16], &header_crc, 4);

    uint8_t cmd1[10] = {0x2a, 0, 0, 0, 0, 1, 0, 0, 1, 0};
    if (scsi_command(cmd1, 10, 0x00, header, 512) != 0) return -1;

    // 4. Write primary entries to LBA 2-33
    for (uint32_t i = 0; i < 32; i++) {
        uint32_t lba = 2 + i;
        uint8_t cmdE[10] = {0x2a, 0, 0, 0, 0, (uint8_t)lba, 0, 0, 1, 0};
        if (scsi_command(cmdE, 10, 0x00, &entries[i*512], 512) != 0) return -1;
    }

    // 5. GPT Backup Entries at g_sector_count - 33
    LOGI("Writing GPT Backup Entries...");
    for (uint32_t i = 0; i < 32; i++) {
        uint64_t lba = (g_sector_count - 33) + i;
        uint8_t cmdEB[10] = {0x2a, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba, 0, 0, 1, 0};
        if (scsi_command(cmdEB, 10, 0x00, &entries[i*512], 512) != 0) {
            LOGE("Failed to write GPT backup entry at LBA %llu. Some sticks lock their tail.", (unsigned long long)lba);
            break; // Non-fatal, primary GPT is already written
        }
    }

    // 6. GPT Backup Header at g_sector_count - 1
    LOGI("Writing GPT Backup Header...");
    memset(header + 16, 0, 4); // Clear CRC
    current_lba = g_sector_count - 1; memcpy(&header[24], &current_lba, 8);
    backup_lba = 1; memcpy(&header[32], &backup_lba, 8);
    entry_lba = g_sector_count - 33; memcpy(&header[72], &entry_lba, 8);
    header_crc = crc32(0, header, 92);
    memcpy(&header[16], &header_crc, 4);

    uint64_t blba = g_sector_count - 1;
    uint8_t cmdB[10] = {0x2a, 0, (uint8_t)(blba >> 24), (uint8_t)(blba >> 16), (uint8_t)(blba >> 8), (uint8_t)blba, 0, 0, 1, 0};
    if (scsi_command(cmdB, 10, 0x00, header, 512) != 0) {
        LOGE("Failed to write GPT backup header. Proceeding with primary only.");
    }

    uint8_t final_sync[10] = {0x35};
    scsi_command(final_sync, 10, 0x00, nullptr, 0);

    LOGI("GPT Created successfully (including full backup).");
    return 0;
}

int usb_scsi_sync() {
    scsi_flush_cache_internal();
    uint8_t cmd[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    cmd[0] = 0x35; // SYNCHRONIZE CACHE
    return scsi_command(cmd, 10, 0x00, nullptr, 0);
}

int usb_scsi_wipe_mbr() {
    LOGI("Wiping 1MB...");
    uint8_t* zero_buf = (uint8_t*)malloc(65536);
    memset(zero_buf, 0, 65536);
    for (uint32_t lba = 0; lba < 2048; lba += 128) {
        uint8_t write_cmd[10] = {0x2a, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba, 0, 0, 128, 0};
        if (scsi_command(write_cmd, 10, 0x00, zero_buf, 65536) != 0) break;
    }
    uint8_t sync_cmd[10] = {0x35};
    scsi_command(sync_cmd, 10, 0x00, nullptr, 0); // Non-fatal
    free(zero_buf);
    return 0;
}

int usb_scsi_wipe_partition_head(int fsType, progress_callback cb) {
    // For F2FS we clean 128MB (it has many structures that must be removed)
    // For the rest (NTFS, EXT4, FAT), 32MB are enough and much faster.
    int64_t wipe_size = (fsType == 8) ? (128LL * 1024 * 1024) : (32LL * 1024 * 1024);

    LOGI("Wiping partition head (%lld MB)...", wipe_size / (1024 * 1024));
    uint32_t chunk_size = 262144; // 256KB for better stability on cheap sticks
    uint8_t* zero_buf = (uint8_t*)malloc(chunk_size);
    if (!zero_buf) return -1;
    memset(zero_buf, 0, chunk_size);

    for (int64_t offset = 0; offset < wipe_size; offset += chunk_size) {
        if (scsi_io_internal(zero_buf, chunk_size, offset, true) != chunk_size) {
            LOGE("Wipe failed at offset %lld. Retrying...", offset);
            usleep(500000); // 0.5s pause
            if (scsi_io_internal(zero_buf, chunk_size, offset, true) != chunk_size) {
                LOGE("Wipe failed permanently at offset %lld", offset);
                break;
            }
        }

        // Report progress (between 20% and 50% of total process)
        if (cb) {
            int p = 20 + (int)((offset * 30) / wipe_size);
            cb(p);
        }

        if (offset % (8LL * 1024 * 1024) == 0) { // Log every 8MB instead of 32MB
            LOGI("Deep cleaning: %lld/%lld MB finished...", offset / (1024 * 1024), wipe_size / (1024 * 1024));
        }
    }

    free(zero_buf);
    usb_scsi_sync();
    LOGI("Wipe partition head completed.");
    return 0;
}

int usb_scsi_write_mbr(uint64_t partition_sectors, uint8_t partition_type) {
    LOGI("Creating MBR Partition Table (%llu sectors, Type 0x%02X)...", (unsigned long long)partition_sectors, partition_type);
    uint8_t mbr[512] = {0};

    // Partition 1 at offset 446 (0x1BE)
    uint8_t *p = &mbr[446];
    p[0] = 0x80; // Active/Bootable
    p[1] = 0x00; p[2] = 0x00; p[3] = 0x00; // CHS start dummy

    p[4] = partition_type;

    p[5] = 0x00; p[6] = 0x00; p[7] = 0x00; // CHS end dummy

    uint32_t start_lba = 4096; // 2MB align
    uint32_t num_sectors = (uint32_t)partition_sectors;
    if (num_sectors > g_sector_count - start_lba) num_sectors = g_sector_count - start_lba;

    // Start LBA (little endian)
    p[8] = (uint8_t)start_lba;
    p[9] = (uint8_t)(start_lba >> 8);
    p[10] = (uint8_t)(start_lba >> 16);
    p[11] = (uint8_t)(start_lba >> 24);

    // Size in sectors (little endian)
    p[12] = (uint8_t)num_sectors;
    p[13] = (uint8_t)(num_sectors >> 8);
    p[14] = (uint8_t)(num_sectors >> 16);
    p[15] = (uint8_t)(num_sectors >> 24);

    // Signature
    mbr[510] = 0x55;
    mbr[511] = 0xAA;

    uint8_t write_cmd[10] = {0x2a, 0, 0, 0, 0, 0, 0, 0, 1, 0}; // Write sector 0
    if (scsi_command(write_cmd, 10, 0x00, mbr, 512) != 0) {
        LOGE("Failed to write MBR!");
        return -1;
    }

    uint8_t sync_cmd[10] = {0x35};
    scsi_command(sync_cmd, 10, 0x00, nullptr, 0);
    return 0;
}

static int update_mbr_type(uint8_t type) {
    uint8_t mbr[512];
    uint8_t read_cmd[10] = {0x28, 0, 0, 0, 0, 0, 0, 0, 1, 0};
    if (scsi_command(read_cmd, 10, 0x80, mbr, 512) != 0) return -1;

    if (mbr[510] == 0x55 && mbr[511] == 0xAA) {
        mbr[446 + 4] = type;
        uint8_t write_cmd[10] = {0x2a, 0, 0, 0, 0, 0, 0, 0, 1, 0};
        scsi_command(write_cmd, 10, 0x00, mbr, 512);
    }
    return 0;
}

void usb_scsi_set_partition_offset(uint64_t sector_offset) {
    g_drive_offset_sectors = sector_offset;
    LOGI("Driver offset set to sector %llu", (unsigned long long)g_drive_offset_sectors);
}

uint64_t usb_scsi_get_partition_offset() {
    return g_drive_offset_sectors;
}

void usb_scsi_set_partition_type(int type) {
    g_partition_type = type;
}

uint64_t usb_scsi_get_sector_count() {
    return g_sector_count;
}

uint64_t usb_scsi_get_usable_sectors() {
    uint64_t usable = g_partition_sectors;
    if (usable == 0) usable = g_sector_count - g_drive_offset_sectors;
    if (g_partition_type == 2 && g_partition_sectors == 0) usable -= 33;
    return usable;
}

void usb_scsi_set_partition_size(uint64_t sectors) {
    g_partition_sectors = sectors;
}

static int dev_open(struct ntfs_device *dev, int flags) { return 0; }
static int dev_close(struct ntfs_device *dev) {
    usb_scsi_sync();
    return 0;
}

static int64_t dev_read(struct ntfs_device *dev, void *buf, int64_t count) {
    int64_t res = scsi_io_internal(buf, count, g_cur_pos, false);
    if (res > 0) g_cur_pos += res;
    return res;
}

static int64_t dev_write(struct ntfs_device *dev, const void *buf, int64_t count) {
    int64_t res = scsi_io_internal((void*)buf, count, g_cur_pos, true);
    if (res > 0) g_cur_pos += res;
    return res;
}

static int64_t dev_pread(struct ntfs_device *dev, void *buf, int64_t count, int64_t offset) {
    return scsi_io_internal(buf, count, offset, false);
}

static int64_t dev_pwrite(struct ntfs_device *dev, const void *buf, int64_t count, int64_t offset) {
    return scsi_io_internal((void*)buf, count, offset, true);
}

static int64_t dev_seek(struct ntfs_device *dev, int64_t offset, int whence) {
    if (whence == SEEK_SET) g_cur_pos = offset;
    else if (whence == SEEK_CUR) g_cur_pos += offset;
    else if (whence == SEEK_END) g_cur_pos = (int64_t)((g_sector_count - g_drive_offset_sectors) * g_sector_size) + offset;
    return g_cur_pos;
}

static int dev_sync(struct ntfs_device *dev) {
    return usb_scsi_sync();
}

static int dev_stat(struct ntfs_device *dev, struct stat *buf) {
    memset(buf, 0, sizeof(struct stat));
    buf->st_mode = S_IFBLK | 0600;
    uint64_t usable_sectors = g_partition_sectors;
    if (usable_sectors == 0) usable_sectors = g_sector_count - g_drive_offset_sectors;
    if (g_partition_type == 2 && g_partition_sectors == 0) usable_sectors -= 33;
    buf->st_size = (off_t)(usable_sectors * g_sector_size);
    return 0;
}

static int dev_ioctl(struct ntfs_device *dev, unsigned long request, void *argp) {
    uint64_t usable_sectors = g_partition_sectors;
    if (usable_sectors == 0) usable_sectors = g_sector_count - g_drive_offset_sectors;
    if (g_partition_type == 2 && g_partition_sectors == 0) usable_sectors -= 33;

    // BLKGETSIZE64: 0x80081272 (8-byte size), 0x80041272 (4-byte size pointer)
    if (request == 0x80081272 || request == 0x80041272 || request == 0x1260) {
        *(uint64_t*)argp = usable_sectors * g_sector_size;
        return 0;
    }
    if (request == 0x1268 || request == 0x4712) {
        *(int*)argp = (int)g_sector_size;
        return 0;
    }
        if (request == 0x301) {
        struct hd_geometry *geo = (struct hd_geometry *)argp;
        geo->heads = 64;
        geo->sectors = 32;
        geo->cylinders = (unsigned short)(g_sector_count / (64 * 32));
        geo->start = (unsigned long)g_drive_offset_sectors;
        return 0;
    }
    return -1;
}

int64_t usb_scsi_read_internal(void* buf, int64_t count) {
    int64_t res = scsi_io_internal(buf, count, g_cur_pos, false);
    if (res > 0) g_cur_pos += res;
    return res;
}

int64_t usb_scsi_write_internal(const void* buf, int64_t count) {
    int64_t res = scsi_io_internal((void*)buf, count, g_cur_pos, true);
    if (res > 0) g_cur_pos += res;
    return res;
}

int64_t usb_scsi_pread_internal(void* buf, int64_t count, int64_t offset) {
    return (int64_t)scsi_io_internal(buf, count, offset, false);
}

int64_t usb_scsi_pwrite_internal(const void* buf, int64_t count, int64_t offset) {
    return (int64_t)scsi_io_internal((void*)buf, count, offset, true);
}

int64_t usb_scsi_lseek_internal(int64_t offset, int whence) {
    if (whence == SEEK_SET) g_cur_pos = offset;
    else if (whence == SEEK_CUR) g_cur_pos += offset;
    else if (whence == SEEK_END) {
        uint64_t usable_sectors = g_partition_sectors;
        if (usable_sectors == 0) usable_sectors = g_sector_count - g_drive_offset_sectors;
        if (g_partition_type == 2 && g_partition_sectors == 0) usable_sectors -= 33;
        g_cur_pos = (int64_t)(usable_sectors * g_sector_size) + offset;
    }
    return g_cur_pos;
}

struct ntfs_device_operations ntfs_device_usb_scsi_ops = {
    .open = dev_open,
    .close = dev_close,
    .seek = dev_seek,
    .read = dev_read,
    .write = dev_write,
    .pread = dev_pread,
    .pwrite = dev_pwrite,
    .sync = dev_sync,
    .stat = dev_stat,
    .ioctl = dev_ioctl
};

// --- FAT Formatting Implementation ---

static void fat_set_vol_label(uint8_t* dir_entry, const char* label) {
    memset(dir_entry, ' ', 11);
    if (!label) return;
    int len = (int)strlen(label);
    if (len > 11) len = 11;
    for (int i = 0; i < len; i++) dir_entry[i] = (uint8_t)toupper(label[i]);
}

int usb_scsi_format_fat(int fat_type, uint32_t cluster_size, const char* label) {
    LOGI("Starting FAT formatting: type=%d (1=FAT32, 2=FAT16, 3=FAT12), cluster=%u, label=%s", fat_type, cluster_size, label);

    uint64_t usable_sectors = g_sector_count - g_drive_offset_sectors;
    if (g_partition_type == 2) usable_sectors -= 33; // Reserve for GPT backup

    if (usable_sectors == 0) {
        LOGE("Error: Stick size is 0!");
        return -1;
    }

    // File system parameters
    uint16_t reserved_sectors = (fat_type == 1) ? 32 : 8;
    uint8_t num_fats = 2;
    uint16_t root_dir_entries = (fat_type == 1) ? 0 : 512;
    uint16_t bytes_per_sector = 512;

    // Calculate sectors per cluster
    if (cluster_size == 0) {
        if (fat_type == 1) cluster_size = 4096;
        else if (fat_type == 2) cluster_size = 2048;
        else cluster_size = 512;
    }
    uint8_t sectors_per_cluster = (uint8_t)(cluster_size / bytes_per_sector);
    if (sectors_per_cluster == 0) sectors_per_cluster = 1;

    // Calculate FAT table size
    uint32_t fat_size_sectors = 0;
    uint32_t root_dir_sectors = (root_dir_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;
    uint8_t mbr_type = 0x06;

    if (fat_type == 1) { // FAT32
        mbr_type = (g_drive_offset_sectors > 0) ? 0x0C : 0x0B;
        uint64_t tmp = usable_sectors - reserved_sectors;
        fat_size_sectors = (uint32_t)((tmp * 4 + (bytes_per_sector * sectors_per_cluster + 8) - 1) / (bytes_per_sector * sectors_per_cluster + 8));
    } else { // FAT12 / FAT16
        if (fat_type == 3) mbr_type = 0x01; // FAT12
        else {
            if (g_drive_offset_sectors > 0) mbr_type = 0x0E; // FAT16 LBA
            else mbr_type = (usable_sectors > 65535) ? 0x06 : 0x04; // FAT16 CHS
        }
        uint64_t tmp = usable_sectors - reserved_sectors - root_dir_sectors;
        fat_size_sectors = (uint32_t)((tmp * 2 + (bytes_per_sector * sectors_per_cluster + 4) - 1) / (bytes_per_sector * sectors_per_cluster + 4));
    }

    // Boot Sector (LBA 0 inside partition)
    uint8_t bs[512] = {0};
    bs[0] = 0xEB; bs[1] = (fat_type == 1) ? 0x58 : 0x3C; bs[2] = 0x90; // Jump
    memcpy(&bs[3], "MSWIN4.1", 8);
    memcpy(&bs[11], &bytes_per_sector, 2);
    bs[13] = sectors_per_cluster;
    memcpy(&bs[14], &reserved_sectors, 2);
    bs[16] = num_fats;
    memcpy(&bs[17], &root_dir_entries, 2);

    if (usable_sectors < 65536 && fat_type != 1) {
        uint16_t ts16 = (uint16_t)usable_sectors;
        memcpy(&bs[19], &ts16, 2);
    } else {
        uint32_t ts32 = (uint32_t)usable_sectors;
        memcpy(&bs[32], &ts32, 4);
    }

    bs[21] = 0xF8; // Media type (Hard Disk)
    uint16_t spt = 32; uint16_t heads = 64;
    memcpy(&bs[24], &spt, 2);
    memcpy(&bs[26], &heads, 2);
    uint32_t hidden = (uint32_t)g_drive_offset_sectors;
    memcpy(&bs[28], &hidden, 4);

    if (fat_type == 1) { // FAT32 Specific
        memcpy(&bs[36], &fat_size_sectors, 4);
        uint32_t root_cluster = 2;
        memcpy(&bs[44], &root_cluster, 4);
        uint16_t fsinfo_v = 1;
        memcpy(&bs[48], &fsinfo_v, 2);
        uint16_t backup_bs = 6;
        memcpy(&bs[50], &backup_bs, 2);
        bs[66] = 0x29; // Extended boot signature
        uint32_t serial = (uint32_t)rand();
        memcpy(&bs[67], &serial, 4);
        fat_set_vol_label(&bs[71], label);
        memcpy(&bs[82], "FAT32   ", 8);
    } else { // FAT12 / FAT16 Specific
        uint16_t fs16 = (uint16_t)fat_size_sectors;
        memcpy(&bs[22], &fs16, 2);
        bs[38] = 0x29;
        uint32_t serial = (uint32_t)rand();
        memcpy(&bs[39], &serial, 4);
        fat_set_vol_label(&bs[43], label);
        const char* fs_name = (fat_type == 2) ? "FAT16   " : "FAT12   ";
        memcpy(&bs[54], fs_name, 8);
    }
    bs[510] = 0x55; bs[511] = 0xAA;

    // 4. Update MBR if exists
    if (g_partition_type == 1) update_mbr_type(mbr_type);

    // 5. Write Boot Sector
    LOGI("Writing FAT Boot Sector...");
    if (scsi_io_internal(bs, 512, 0, true) != 512) return -1;

    if (fat_type == 1) { // Backup BS and FSInfo
        scsi_io_internal(bs, 512, 6 * 512, true);
        uint8_t fsinfo_s[512] = {0};
        fsinfo_s[0] = 0x52; fsinfo_s[1] = 0x52; fsinfo_s[2] = 0x61; fsinfo_s[3] = 0x41; // Lead Sig
        fsinfo_s[484] = 0x72; fsinfo_s[485] = 0x72; fsinfo_s[486] = 0x41; fsinfo_s[487] = 0x61; // Struc Sig
        uint32_t free_count = 0xFFFFFFFF; // Unknown
        memcpy(&fsinfo_s[488], &free_count, 4);
        uint32_t next_free = 0xFFFFFFFF;
        memcpy(&fsinfo_s[492], &next_free, 4);
        fsinfo_s[510] = 0x55; fsinfo_s[511] = 0xAA;
        scsi_io_internal(fsinfo_s, 512, 1 * 512, true);
    }

    // Initialize FAT tables
    LOGI("Initializing FAT tables...");
    uint8_t chunk[32768] = {0};
    for (int f = 0; f < num_fats; f++) {
        uint32_t fat_offset = (reserved_sectors + f * fat_size_sectors) * bytes_per_sector;

        // First FAT entries
        memset(chunk, 0, sizeof(chunk));
        if (fat_type == 1) { // FAT32
            uint32_t entries[3] = {0x0FFFFFF8, 0x0FFFFFFF, 0x0FFFFFFF}; // Media + EOC + Root
            memcpy(chunk, entries, 12);
        } else if (fat_type == 2) { // FAT16
            uint16_t entries[2] = {0xFFF8, 0xFFFF};
            memcpy(chunk, entries, 4);
        } else { // FAT12
            uint8_t entries[3] = {0xF8, 0xFF, 0xFF};
            memcpy(chunk, entries, 3);
        }

        scsi_io_internal(chunk, 32768, (int64_t)fat_offset, true);
        memset(chunk, 0, 12); // Clear header for remaining writes

        for (uint32_t i = 32768; i < fat_size_sectors * bytes_per_sector; i += 32768) {
            uint32_t to_write = (fat_size_sectors * bytes_per_sector - i > 32768) ? 32768 : (fat_size_sectors * bytes_per_sector - i);
            scsi_io_internal(chunk, to_write, (int64_t)(fat_offset + i), true);
        }
    }

    // Initialize Root Directory (only for FAT12/16)
    if (fat_type != 1) {
        LOGI("Initializing FAT12/16 Root Directory...");
        memset(chunk, 0, sizeof(chunk));
        uint32_t root_offset = (reserved_sectors + num_fats * fat_size_sectors) * bytes_per_sector;
        uint32_t root_size = root_dir_sectors * bytes_per_sector;
        for (uint32_t i = 0; i < root_size; i += 32768) {
            uint32_t to_write = (root_size - i > 32768) ? 32768 : (root_size - i);
            scsi_io_internal(chunk, to_write, (int64_t)(root_offset + i), true);
        }
    }

    LOGI("FAT Formatting successful.");
    return 0;
}

} // extern "C"
