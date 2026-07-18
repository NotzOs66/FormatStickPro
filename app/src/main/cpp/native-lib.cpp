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

#include <jni.h>
#include <string>
#include <vector>
#include <android/log.h>
#include <unistd.h>
#include <libusb.h>
#include <cstdio>
#include <cstdlib>
#include <setjmp.h>
#include "usb_scsi.h"

extern "C" {
#include "ntfs-3g/device.h"
#include "ntfs-3g/logging.h"
#include "et/com_err.h"
}

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FormatStickPro", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FormatStickPro", __VA_ARGS__)

/**
 * Nota de autor: "format Stick Pro creat de Șpac Dumitru"
 */
extern "C" const char* get_native_core_auth() {
    return "Zm9ybWF0IFN0aWNrIFBybyBjcmVhdCBkZSDFn3BhYyBEdW1pdHJ1";
}

// Prototypes for mkntfs_main and mke2fs_main
extern "C" int mkntfs_main(int argc, char *argv[]);
extern "C" int mke2fs_main(int argc, char *argv[]);

jmp_buf g_mke2fs_jmp;

// Override exit() for mke2fs to not kill the entire Android process
extern "C" void mke2fs_exit(int status) {
    LOGI("mke2fs called exit with status: %d. Returning via longjmp.", status);
    longjmp(g_mke2fs_jmp, status ? status : -1);
}

static JavaVM* g_vm = nullptr;
static jobject g_service_obj = nullptr;
static int g_target_progress = 0;

// Helper function to report progress back to Java
void report_progress_to_java(int progress) {
    if (progress > g_target_progress) g_target_progress = progress;

    if (!g_service_obj || !g_vm) return;
    JNIEnv* env = nullptr;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return;

    jclass cls = env->GetObjectClass(g_service_obj);
    jmethodID mid = env->GetMethodID(cls, "reportProgress", "(I)V");
    if (mid) {
        env->CallVoidMethod(g_service_obj, mid, progress);
    }
}

// Helper to report stages/logs to Java
void report_stage_to_java(const char* m) {
    if (!g_service_obj || !g_vm) return;
    JNIEnv* env = nullptr;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return;

    jclass cls = env->GetObjectClass(g_service_obj);
    jmethodID mid = env->GetMethodID(cls, "reportStage", "(Ljava/lang/String;)V");
    if (mid) {
        jstring msg = env->NewStringUTF(m);
        env->CallVoidMethod(g_service_obj, mid, msg);
        env->DeleteLocalRef(msg);
    }
}

// Log handler for NTFS-3G that sends to Java
static int ntfs_log_java_handler(const char *function, const char *file, int line,
                                 u32 level, void *data, const char *format, va_list args) {
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), format, args);
    __android_log_print(ANDROID_LOG_INFO, "NTFS-LOG", "%s", buffer);
    report_stage_to_java(buffer);
    return 0;
}

// Log handler for ext2fs that sends to Java
static void local_ext_log_handler(const char *format, va_list args) {
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), format, args);
    __android_log_print(ANDROID_LOG_INFO, "EXT-LOG", "%s", buffer);
    report_stage_to_java(buffer);
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

__attribute__((constructor))
void my_init() {
    __android_log_print(ANDROID_LOG_INFO, "FormatStickPro", "formatstickpro library constructor called!");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_formatstickpro_app_FormatService_startStableFormat(
        JNIEnv* env, jobject thiz, jint fd, jstring usbPath, jint clusterSize, jint partitionType, jint fsType) {

    // Save global reference for progress
    g_service_obj = env->NewGlobalRef(thiz);
    g_target_progress = 0;

    const char *path = env->GetStringUTFChars(usbPath, nullptr);
    LOGI("Starting format via TRANSLATOR. FS: %d, Cluster: %d, Partition: %d", fsType, clusterSize, partitionType);
    report_stage_to_java("Initializing libusb...");
    report_progress_to_java(2);

    // 1. Initialize libusb
    libusb_context *ctx = nullptr;

    struct libusb_init_option options[] = {
        { .option = LIBUSB_OPTION_NO_DEVICE_DISCOVERY, .value = { .ival = 0 } },
        { .option = LIBUSB_OPTION_LOG_LEVEL, .value = { .ival = LIBUSB_LOG_LEVEL_INFO } }
    };

    int libusb_res = libusb_init_context(&ctx, options, 2);
    if (libusb_res != 0) {
        LOGE("Error initializing libusb! Code: %d (%s)", libusb_res, libusb_strerror((enum libusb_error)libusb_res));
        env->DeleteGlobalRef(g_service_obj);
        g_service_obj = nullptr;
        return JNI_FALSE;
    }
    report_progress_to_java(5);

    // 2. Enroll the File Descriptor received from Android UsbManager
    libusb_device_handle *dev_handle = nullptr;
    if (libusb_wrap_sys_device(ctx, (intptr_t)fd, &dev_handle) != 0) {
        LOGE("Error libusb_wrap_sys_device!");
        libusb_exit(ctx);
        env->DeleteGlobalRef(g_service_obj);
        g_service_obj = nullptr;
        return JNI_FALSE;
    }

    LOGI("Stick successfully acquired via libusb!");
    report_stage_to_java("USB connection established.");
    report_progress_to_java(10);

    // 3. Initialize SCSI Translator
    usb_scsi_init(dev_handle);
    report_progress_to_java(12);

    uint64_t disk_sectors = usb_scsi_get_sector_count();
    uint64_t offset = (partitionType > 0) ? 4096 : 0; // 2MB Align
    uint64_t reserved_end = (partitionType == 2) ? 34 : 0; // GPT backup

    if (disk_sectors <= offset + reserved_end) {
        LOGE("Error: Stick is too small for the chosen structure! Sectors: %llu", (unsigned long long)disk_sectors);
        libusb_close(dev_handle);
        libusb_exit(ctx);
        env->DeleteGlobalRef(g_service_obj);
        g_service_obj = nullptr;
        return JNI_FALSE;
    }

    uint64_t max_usable_sectors = disk_sectors - offset - reserved_end;

    // Calculate maximum allowed size for FS (FAT limits)
    uint64_t fs_cap_sectors = 0;
    if (fsType == 3) fs_cap_sectors = 65535; // FAT12
    else if (fsType == 2) { // FAT16
        uint32_t effCluster = clusterSize ? (uint32_t)clusterSize : 32768;
        fs_cap_sectors = (uint64_t)65524 * (effCluster / 512);
    }

    uint64_t part_sectors = max_usable_sectors;
    if (fs_cap_sectors > 0 && fs_cap_sectors < max_usable_sectors) {
        part_sectors = fs_cap_sectors;
    }

    usb_scsi_set_partition_type(partitionType);

    // Clean critical areas
    report_stage_to_java("Wiping old tables...");
    usb_scsi_wipe_mbr();
    usb_scsi_wipe_tail();
    report_progress_to_java(15);

    uint8_t partition_id = 0x83; // Linux / General
    if (fsType == 0 || fsType == 4 || fsType == 9) partition_id = 0x07; // NTFS, exFAT, UDF (Windows Safe)
    else if (fsType == 1) partition_id = (partitionType == 1) ? 0x0C : 0x0B; // FAT32
    else if (fsType == 2) partition_id = (partitionType == 1) ? 0x0E : 0x06; // FAT16
    else if (fsType == 3) partition_id = 0x01; // FAT12

    report_stage_to_java("Creating partition...");
    if (partitionType == 1) {
        usb_scsi_write_mbr(part_sectors, partition_id);
        usb_scsi_set_partition_offset(4096);
        usb_scsi_set_partition_size(part_sectors);
        usb_scsi_wipe_partition_head(fsType, report_progress_to_java);
    } else if (partitionType == 2) {
        usb_scsi_write_gpt(part_sectors, fsType);
        usb_scsi_set_partition_offset(4096);
        usb_scsi_set_partition_size(part_sectors);
        usb_scsi_wipe_partition_head(fsType, report_progress_to_java);
    } else {
        usb_scsi_set_partition_offset(0);
        usb_scsi_set_partition_size(part_sectors);
        usb_scsi_wipe_partition_head(fsType, report_progress_to_java);
    }
    report_progress_to_java(50);

    int formatRes = -1;
    optind = 1;

    if (fsType == 0) {
        // --- NTFS ---
        report_stage_to_java("Preparing NTFS...");
        report_progress_to_java(55);
        ntfs_log_set_handler(ntfs_log_java_handler);
        ntfs_log_set_levels(NTFS_LOG_LEVEL_INFO | NTFS_LOG_LEVEL_ERROR | NTFS_LOG_LEVEL_VERBOSE);
        std::vector<char*> args;
        args.push_back(strdup("mkntfs"));
        args.push_back(strdup("-f")); // Quick format
        args.push_back(strdup("-F")); // Force
        args.push_back(strdup("-v")); // Verbose
        args.push_back(strdup("-L"));
        args.push_back(strdup("USB_DISK"));
        if (partitionType > 0) {
            args.push_back(strdup("-p"));
            args.push_back(strdup("4096"));
        }
        if (clusterSize > 0) {
            args.push_back(strdup("-c"));
            char clusterStr[32]; snprintf(clusterStr, sizeof(clusterStr), "%d", clusterSize);
            args.push_back(strdup(clusterStr));
        }

        // Disable compression explicitly for large clusters if possible via command line
        // (though NTFS-3G's mkntfs usually does this automatically as seen in logs)

        args.push_back(strdup("usb_device"));
        char sectorsStr[32]; snprintf(sectorsStr, sizeof(sectorsStr), "%llu", (unsigned long long)part_sectors);
        args.push_back(strdup(sectorsStr));
        args.push_back(nullptr);

        LOGI("Launching mkntfs_main (NTFS-3G)...");
        formatRes = mkntfs_main((int)args.size() - 1, args.data());
        report_progress_to_java(90);
        usb_scsi_sync(); // Force cache flush after mkntfs
        for (auto arg : args) if (arg) free(arg);
    }
    else if (fsType == 4) {
        // --- exFAT ---
        report_stage_to_java("Formatting exFAT...");
        report_progress_to_java(55);
        LOGI("Launching exFAT formatting...");
        formatRes = usb_scsi_format_exfat((uint32_t)clusterSize, "USB_DISK");
        report_progress_to_java(90);
    }
    else if (fsType >= 5 && fsType <= 7) {
        // --- Ext2, Ext3, Ext4 ---
        report_stage_to_java("Formatting EXT...");
        report_progress_to_java(55);
        std::vector<char*> args;
        args.push_back(strdup("mke2fs"));
        args.push_back(strdup("-F"));
        args.push_back(strdup("-v"));

        // FORCE full formatting (no lazy init)
        args.push_back(strdup("-E"));
        args.push_back(strdup("lazy_itable_init=0,lazy_journal_init=0,root_perms=0777"));

        if (fsType == 7) {
            args.push_back(strdup("-O"));
            args.push_back(strdup("64bit,extents,flex_bg,metadata_csum,has_journal,^uninit_bg"));
        } else if (fsType == 6) {
            args.push_back(strdup("-O"));
            args.push_back(strdup("has_journal,^uninit_bg"));
        } else {
            args.push_back(strdup("-O"));
            args.push_back(strdup("^uninit_bg"));
        }

        if (fsType == 5) args.push_back(strdup("-t")), args.push_back(strdup("ext2"));
        else if (fsType == 6) args.push_back(strdup("-t")), args.push_back(strdup("ext3"));
        else if (fsType == 7) args.push_back(strdup("-t")), args.push_back(strdup("ext4"));

        args.push_back(strdup("-m"));
        args.push_back(strdup("0"));
        args.push_back(strdup("-L"));
        args.push_back(strdup("USB_DISK"));
        args.push_back(strdup("-b"));
        args.push_back(strdup("4096"));
        args.push_back(strdup("usb_device"));

        uint64_t total_blocks = part_sectors / 8;
        char blocksStr[32]; snprintf(blocksStr, sizeof(blocksStr), "%llu", (unsigned long long)total_blocks);
        args.push_back(strdup(blocksStr));
        args.push_back(nullptr);

        setenv("MKE2FS_DEVICE_SECTSIZE", "512", 1);
        setenv("MKE2FS_DEVICE_PHYS_SECTSIZE", "512", 1);
        set_com_err_hook(local_ext_log_handler);

        usb_scsi_sync();

        int exit_code = setjmp(g_mke2fs_jmp);
        if (exit_code == 0) {
            formatRes = mke2fs_main((int)args.size() - 1, args.data());
        } else {
            formatRes = (exit_code == -1) ? 0 : exit_code;
        }

        report_progress_to_java(90);
        usb_scsi_sync();
        for (auto arg : args) if (arg) free(arg);
    }
    else if (fsType == 8) {
        // --- F2FS ---
        report_stage_to_java("Formatting F2FS...");
        report_progress_to_java(55);
        extern int startF2FSFormat_Internal(JNIEnv* env, jobject obj, jint fd, jlong part_sectors, jint clusterSize, jstring labelObj, jlong start_sector);
        jstring labelObj = env->NewStringUTF("USB_DISK");
        uint64_t start_sector = (partitionType > 0) ? 4096 : 0;
        formatRes = startF2FSFormat_Internal(env, g_service_obj, fd, (long long)part_sectors, clusterSize, labelObj, (long long)start_sector);
        env->DeleteLocalRef(labelObj);
        report_progress_to_java(90);
    }
    else if (fsType == 9) {
        // --- UDF ---
        report_stage_to_java("Formatting UDF...");
        report_progress_to_java(55);
        extern int startUDFFormat_Internal(JNIEnv* env, jobject obj, jint fd, jlong part_sectors, jstring labelObj);
        jstring labelObj = env->NewStringUTF("USB_DISK");
        formatRes = startUDFFormat_Internal(env, g_service_obj, fd, (long long)part_sectors, labelObj);
        env->DeleteLocalRef(labelObj);
        report_progress_to_java(90);
    }
    else if (fsType == 10) {
        // --- XFS ---
        report_stage_to_java("Formatting XFS...");
        report_progress_to_java(55);
        extern int startXFSFormat_Internal(JNIEnv* env, jobject obj, jint fd, jlong part_sectors, jstring labelObj);
        jstring labelObj = env->NewStringUTF("USB_DISK");
        formatRes = startXFSFormat_Internal(env, g_service_obj, fd, (long long)part_sectors, labelObj);
        env->DeleteLocalRef(labelObj);
        report_progress_to_java(90);
    }
    else {
        // --- FAT32, FAT16, FAT12 ---
        report_stage_to_java("Formatting FAT...");
        report_progress_to_java(55);
        formatRes = usb_scsi_format_fat(fsType, (uint32_t)clusterSize, "USB_DISK");
        report_progress_to_java(90);
    }

    // --- FINAL SYNC ---
    report_stage_to_java("Finalizing write...");
    usb_scsi_sync();
    report_progress_to_java(100);

    libusb_close(dev_handle);
    libusb_exit(ctx);
    env->ReleaseStringUTFChars(usbPath, path);
    env->DeleteGlobalRef(g_service_obj);
    g_service_obj = nullptr;

    return (formatRes == 0) ? JNI_TRUE : JNI_FALSE;
}

#include <pthread.h>

void* stderr_logger_thread(void* arg) {
    int pipe_rd = *(int*)arg;
    free(arg);
    char buf[1024];
    while (1) {
        ssize_t n = read(pipe_rd, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = 0;
        char *line = strtok(buf, "\n");
        while (line) {
            __android_log_print(ANDROID_LOG_ERROR, "FS-STDERR", "%s", line);
            report_stage_to_java(line);
            line = strtok(NULL, "\n");
        }
    }
    return NULL;
}

extern "C" void redirect_stderr_to_logcat() {
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) return;
    dup2(pipe_fds[1], STDERR_FILENO);
    close(pipe_fds[1]);
    int *arg = (int*)malloc(sizeof(int));
    *arg = pipe_fds[0];
    pthread_t t;
    pthread_create(&t, NULL, stderr_logger_thread, arg);
    pthread_detach(t);
}
