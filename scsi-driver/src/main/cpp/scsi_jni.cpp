#include <jni.h>
#include "usb_scsi.h"
#include <android/log.h>

#define LOG_TAG "SCSI-Driver-JNI"

extern "C" JNIEXPORT void JNICALL
Java_com_formatstickpro_scsidriver_SCSIDriver_nativeInit(JNIEnv *env, jobject thiz, jint fd) {
    // Note: This is a simplified wrapper.
    // In a real scenario, you'd use the fd to get a libusb_device_handle.
    // Since usb_scsi_init expects a handle, we assume the user provides it or we manage it here.
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Initializing SCSI Driver with FD: %d", fd);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_formatstickpro_scsidriver_SCSIDriver_getAuthorSignature(JNIEnv *env, jobject thiz) {
    // Base64: "format Stick Pro creat de Șpac Dumitru"
    return env->NewStringUTF("Zm9ybWF0IFN0aWNrIFBybyBjcmVhdCBkZSDFn3BhYyBEdW1pdHJ1");
}

extern "C" JNIEXPORT jint JNICALL
Java_com_formatstickpro_scsidriver_SCSIDriver_nativeSync(JNIEnv *env, jobject thiz) {
    return usb_scsi_sync();
}
