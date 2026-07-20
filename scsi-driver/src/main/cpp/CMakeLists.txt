cmake_minimum_required(VERSION 3.22.1)

project("scsidriver")

# Build libusb static
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

# Build the main SCSI driver library
# We add a JNI wrapper to make it accessible from Java/Kotlin
add_library(scsidriver SHARED
        usb_scsi.cpp
        scsi_jni.cpp
)

# Mocking NTFS-3G parts if not present, to keep it standalone
# or ensuring include paths are relative.
# For a pure library, we might want to decouple it from NTFS-3G completely.
target_compile_definitions(scsidriver PRIVATE
    STANDALONE_LIB
    HAVE_CONFIG_H
)

target_include_directories(scsidriver PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        libusb
)

target_link_libraries(scsidriver
        android
        log
        usb
)
