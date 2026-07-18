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
#include "usb_scsi.h"
#include <android/log.h>
#include <stdarg.h>
#include <getopt.h>

#define LOG_TAG "F2FS-BRIDGE"
#ifdef LOGI
#undef LOGI
#endif
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/**
 * Nota de autor: "format Stick Pro creat de Șpac Dumitru"
 */
extern "C" const char* get_f2fs_bridge_auth() {
    return "Zm9ybWF0IFN0aWNrIFBybyBjcmVhdCBkZSDFn3BhYyBEdW1pdHJ1";
}

extern "C" {
    #include "f2fs_fs.h"
    #include "usb_scsi.h"
    jmp_buf g_f2fs_jmp;
    int f2fs_format_main(int argc, char *argv[]);
    int f2fs_fsync_device(void);

    static JNIEnv* g_env = nullptr;
    static jobject g_obj = nullptr;
    static jmethodID g_mid_msg = nullptr;
    static jmethodID g_mid_prog = nullptr;
    static long long g_last_msg_time = 0;

    long long current_time_ms() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }

    void f2fs_msg(int priority, const char *fmt, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        LOGI("%s", buffer);

        // Filter: only priority 0 or important-looking messages to Java
        if (priority > 0 && buffer[0] == '\t') return;
        if (strstr(buffer, "Info:")) return; // Skip most Info logs

        if (g_env && g_obj && g_mid_msg) {
            long long now = current_time_ms();
            if (now - g_last_msg_time < 500) return; // Native rate-limit
            g_last_msg_time = now;

            jstring jmsg = g_env->NewStringUTF(buffer);
            if (jmsg) {
                g_env->CallVoidMethod(g_obj, g_mid_msg, jmsg);
                g_env->DeleteLocalRef(jmsg);
            }
        }
    }

    void f2fs_progress(int progress) {
        if (g_env && g_obj && g_mid_prog) {
            g_env->CallVoidMethod(g_obj, g_mid_prog, (jint)progress);
        }
    }

    jint startF2FSFormat_Internal(
            JNIEnv* env, jobject obj,
            jint fd, jlong part_sectors, jint clusterSize, jstring labelObj, jlong start_sector) {

        part_sectors &= ~7LL; // Align to 4KB (8 sectors)
        const char* label = env->GetStringUTFChars(labelObj, nullptr);
        std::string volLabel = label ? label : "F2FSStick";
        env->ReleaseStringUTFChars(labelObj, label);

        LOGI("Starting F2FS formatting via BRIDGE. Sectors: %lld, Cluster: %d, Start: %lld", (long long)part_sectors, clusterSize, (long long)start_sector);

        // Initialize callbacks
        g_env = env;
        g_obj = obj;
        g_last_msg_time = 0;
        if (obj) {
            jclass cls = env->GetObjectClass(obj);
            g_mid_msg = env->GetMethodID(cls, "reportStage", "(Ljava/lang/String;)V");
            g_mid_prog = env->GetMethodID(cls, "reportProgress", "(I)V");
        } else {
            g_mid_msg = nullptr;
            g_mid_prog = nullptr;
        }

        std::vector<char*> args;
        args.push_back(strdup("mkfs.f2fs"));
        args.push_back(strdup("-f")); // Force overwrite
        args.push_back(strdup("-l")); // Label
        args.push_back(strdup(volLabel.c_str()));
        args.push_back(strdup("-a")); // Heap-based allocation
        args.push_back(strdup("1"));

        args.push_back(strdup("usb_device"));

        char sectorsStr[32];
        snprintf(sectorsStr, sizeof(sectorsStr), "%llu", (unsigned long long)part_sectors);
        args.push_back(strdup(sectorsStr));
        args.push_back(nullptr); // NULL terminator for argv

        int argc_count = (int)args.size() - 1;
        LOGI("Launching f2fs_format_main (args: %d)...", argc_count);
        for(int i=0; i<argc_count; ++i) LOGI("  arg[%d]: %s", i, args[i]);

        optind = 1;
        int result = -1;
        int exit_code = setjmp(g_f2fs_jmp);
        if (exit_code == 0) {
            result = f2fs_format_main(argc_count, args.data());
        } else {
            LOGI("f2fs called exit with status: %d. Returning via longjmp.", exit_code);
            result = (exit_code == -1) ? 0 : exit_code;
        }

        if (result == 0) {
            LOGI("Formatting finished successfully. Syncing data...");
            f2fs_fsync_device();
            usb_scsi_sync();
            LOGI("Sync finished. Waiting 3 seconds for buffer flush...");
            // 3-second pause to allow USB controller to write Checkpoint
            struct timespec ts;
            ts.tv_sec = 3;
            ts.tv_nsec = 0;
            nanosleep(&ts, nullptr);
            LOGI("Pause finished. Closing connection.");
        }

        for (auto arg : args) if (arg) free(arg);

        // Reset callbacks
        g_env = nullptr;
        g_obj = nullptr;
        g_mid_msg = nullptr;
        g_mid_prog = nullptr;

        return result == 0 ? 0 : result;
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_formatstickpro_app_FormatService_startF2FSFormat_1JNI(
        JNIEnv* env, jobject obj,
        jint fd, jlong part_sectors, jint clusterSize, jstring labelObj, jlong start_sector) {
    return startF2FSFormat_Internal(env, obj, fd, part_sectors, clusterSize, labelObj, start_sector);
}
