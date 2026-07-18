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

#define LOG_TAG "UDF-BRIDGE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/**
 * Nota de autor: "format Stick Pro creat de Șpac Dumitru"
 */
extern "C" const char* get_udf_bridge_auth() {
    return "Zm9ybWF0IFN0aWNrIFBybyBjcmVhdCBkZSDFn3BhYyBEdW1pdHJ1";
}

extern "C" {
    // mkudffs entry point (renamed from main)
    int udf_format_main(int argc, char *argv[]);

    jmp_buf g_udf_jmp;

    static JNIEnv* g_env = nullptr;
    static jobject g_obj = nullptr;
    static jmethodID g_mid_msg = nullptr;

    void udf_msg(const char *fmt, ...) {
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

    jint startUDFFormat_Internal(
            JNIEnv* env, jobject obj,
            jint fd, jlong part_sectors, jstring labelObj) {

        const char* label = env->GetStringUTFChars(labelObj, nullptr);
        std::string volLabel = label ? label : "UDFStick";
        env->ReleaseStringUTFChars(labelObj, label);

        part_sectors &= ~3LL; // Align to 2KB (4 sectors) for Windows compatibility
        LOGI("Starting UDF formatting via BRIDGE. Sectors: %lld", (long long)part_sectors);

        g_env = env;
        g_obj = obj;
        if (g_env && g_obj) {
            jclass cls = env->GetObjectClass(g_obj);
            g_mid_msg = env->GetMethodID(cls, "reportStage", "(Ljava/lang/String;)V");
        } else {
            g_mid_msg = nullptr;
        }

        std::vector<char*> args;
        args.push_back(strdup("mkudffs"));
        args.push_back(strdup("--utf8"));
        args.push_back(strdup("--media-type=hd"));
        args.push_back(strdup("--udfrev=201")); // 2.01 is the max supported for 'hd' media type in this version
        args.push_back(strdup("--blocksize=512")); // Maintain 512 for Windows compatibility
        args.push_back(strdup("--packetlen=64")); // Keep 32KB alignment (crucial for Windows)

        std::string lvidArg = "--lvid=" + volLabel;
        args.push_back(strdup(lvidArg.c_str()));

        std::string vidArg = "--vid=" + volLabel;
        args.push_back(strdup(vidArg.c_str()));

        std::string vsidArg = "--vsid=" + volLabel;
        args.push_back(strdup(vsidArg.c_str()));

        std::string fsidArg = "--fsid=" + volLabel;
        args.push_back(strdup(fsidArg.c_str()));

        args.push_back(strdup("usb_device"));

        char sectorsStr[32];
        // Pass the raw sector count (for blocksize 512)
        snprintf(sectorsStr, sizeof(sectorsStr), "%llu", (unsigned long long)part_sectors);
        args.push_back(strdup(sectorsStr));

        args.push_back(nullptr);

        int argc_count = (int)args.size() - 1;
        LOGI("Launching udf_format_main...");

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
        int exit_code = setjmp(g_udf_jmp);
        if (exit_code == 0) {
            result = udf_format_main(argc_count, args.data());
        } else {
            LOGI("udf called exit with status: %d. Returning via longjmp.", exit_code);
            result = (exit_code == -1) ? 0 : exit_code;
        }

        if (result == 0) {
            LOGI("UDF formatting finished successfully.");
            usb_scsi_sync();
        }

        for (auto arg : args) if (arg) free(arg);
        g_env = nullptr;
        g_obj = nullptr;
        g_mid_msg = nullptr;

        return result == 0 ? 0 : result;
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_formatstickpro_app_FormatService_startUDFFormat_1JNI(
        JNIEnv* env, jobject obj,
        jint fd, jlong part_sectors, jstring labelObj) {
    // In service, this is a separate external function,
    // but native-lib.cpp might also call startUDFFormat_Internal directly.
    return startUDFFormat_Internal(env, obj, fd, part_sectors, labelObj);
}
