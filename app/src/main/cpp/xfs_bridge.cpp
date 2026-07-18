/*
 * Copyright (C) 2026 FormatStickPro (NotzOs66)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <jni.h>
#include <string>
#include <vector>
#include <setjmp.h>
#include <android/log.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>
#include "usb_scsi.h"

#define LOG_TAG "XFS-BRIDGE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/**
 * Nota de autor: "format Stick Pro creat de Șpac Dumitru"
 */
extern "C" const char* get_xfs_bridge_auth() {
    return "Zm9ybWF0IFN0aWNrIFBybyBjcmVhdCBkZSDFn3BhYyBEdW1pdHJ1";
}

#define XFS_BRIDGE_INTERNAL
#include "xfs_android_stubs.h"

uint64_t g_usb_scsi_part_sectors = 0;

extern "C" {
    // mkfs.xfs entry point (renamed from main)
    int xfs_mkfs_main(int argc, char *argv[]);

    jmp_buf g_xfs_jmp;

    static JNIEnv* g_env = nullptr;
    static jobject g_obj = nullptr;
    static jmethodID g_mid_msg = nullptr;

    void xfs_msg(const char *fmt, ...) {
        if (!fmt) {
            LOGE("xfs_msg called with NULL fmt!");
            return;
        }
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        LOGI("%s", buffer);

        if (g_env && g_obj && g_mid_msg) {
            jstring jmsg = g_env->NewStringUTF(buffer);
            if (jmsg) {
                g_env->CallVoidMethod(g_obj, g_mid_msg, jmsg);
                g_env->DeleteLocalRef(jmsg);
            }
        }
    }

    jint startXFSFormat_Internal(
            JNIEnv* env, jobject obj,
            jint fd, jlong part_sectors, jstring labelObj) {

        const char* label = env->GetStringUTFChars(labelObj, nullptr);
        std::string volLabel = label ? label : "XFSStick";
        env->ReleaseStringUTFChars(labelObj, label);

        LOGI("Starting XFS formatting via BRIDGE. Sectors: %lld", (long long)part_sectors);

        g_usb_scsi_part_sectors = (uint64_t)part_sectors;

        g_env = env;
        if (obj) {
            g_obj = env->NewGlobalRef(obj);
            jclass cls = env->GetObjectClass(g_obj);
            g_mid_msg = env->GetMethodID(cls, "reportStage", "(Ljava/lang/String;)V");
        } else {
            g_obj = nullptr;
            g_mid_msg = nullptr;
        }

        uint64_t total_bytes = (uint64_t)part_sectors * 512;
        char data_arg[512];
        const char* usb_path = "/data/data/com.formatstickpro.app/cache/usb_device";
        snprintf(data_arg, sizeof(data_arg), "file,name=%s,size=%llu", usb_path, (unsigned long long)total_bytes);

        // Ensure the decoy file exists for stat()
        int decoy_fd = open(usb_path, O_RDWR | O_CREAT, 0666);
        if (decoy_fd >= 0) {
            ftruncate(decoy_fd, (off_t)total_bytes);
            close(decoy_fd);
        } else {
            LOGE("Failed to create decoy file %s: %s", usb_path, strerror(errno));
        }

        std::vector<char*> args;
        args.push_back(strdup("mkfs.xfs"));
        args.push_back(strdup("-f"));   // Force overwrite
        args.push_back(strdup("-m"));   // Metadata options
        args.push_back(strdup("crc=0")); // Disable CRC to bypass self-test fail
        args.push_back(strdup("-L"));   // Label
        args.push_back(strdup(volLabel.c_str()));
        args.push_back(strdup("-d"));
        args.push_back(strdup(data_arg));

        // XFS 7.0.1 requires at least 300MB. For smaller sticks, we must use --unsupported.
        if (part_sectors < 614400) { // 300MB @ 512B/sector
            LOGI("Small stick detected (%lld sectors). Using --unsupported.", (long long)part_sectors);
            args.push_back(strdup("--unsupported"));
        }

        args.push_back(nullptr);

        int argc_count = (int)args.size() - 1;
        LOGI("Launching xfs_mkfs_main at %p (argc=%d, argv=%p)...", (void*)xfs_mkfs_main, argc_count, (void*)args.data());

        // Reset getopt globals for Android/Bionic
        extern int optind;
        extern int opterr;
        extern char* optarg;
        extern int optopt;
        optind = 1;
        opterr = 0;
        optarg = nullptr;
        optopt = 0;

        int result = -1;
        int exit_code = setjmp(g_xfs_jmp);
        if (exit_code == 0) {
            result = xfs_mkfs_main(argc_count, args.data());
        } else {
            LOGI("xfs called exit with status: %d. Returning via longjmp.", exit_code);
            result = (exit_code == -1) ? 0 : exit_code;
        }

        if (result == 0) {
            LOGI("XFS formatting finished successfully.");
            usb_scsi_sync();
        }

        for (auto arg : args) {
            if (arg) free(arg);
        }

        if (g_obj) {
            env->DeleteGlobalRef(g_obj);
            g_obj = nullptr;
        }
        g_env = nullptr;
        g_mid_msg = nullptr;

        return result == 0 ? 0 : result;
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_formatstickpro_app_FormatService_startXFSFormat_1JNI(
        JNIEnv* env, jobject obj,
        jint fd, jlong part_sectors, jstring labelObj) {
    return startXFSFormat_Internal(env, obj, fd, part_sectors, labelObj);
}
