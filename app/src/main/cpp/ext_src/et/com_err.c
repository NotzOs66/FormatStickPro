#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "com_err.h"

static void default_com_err_proc (const char *whoami, errcode_t code, const char *fmt, va_list args) {
    if (whoami) {
        fprintf(stderr, "%s: ", whoami);
    }
    if (code) {
        fprintf(stderr, "%s ", error_message(code));
    }
    if (fmt) {
        vfprintf(stderr, fmt, args);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}

typedef void (*errf) (const char *, errcode_t, const char *, va_list);
static errf com_err_hook = default_com_err_proc;

void com_err_va (const char *whoami, errcode_t code, const char *fmt, va_list args) {
    (*com_err_hook) (whoami, code, fmt, args);
}

void com_err (const char *whoami, errcode_t code, const char *fmt, ...) {
    va_list pvar;
    va_start(pvar, fmt);
    com_err_va(whoami, code, fmt, pvar);
    va_end(pvar);
}

errf set_com_err_hook (errf new_proc) {
    errf old = com_err_hook;
    com_err_hook = new_proc ? new_proc : default_com_err_proc;
    return old;
}

errf reset_com_err_hook (void) {
    errf old = com_err_hook;
    com_err_hook = default_com_err_proc;
    return old;
}

gettextf set_com_err_gettext(gettextf new_proc) {
    return new_proc;
}

#include <android/log.h>

void local_ext_log_handler(const char *whoami, errcode_t code, const char *fmt, va_list args) {
    char buf[1024];
    int len = 0;
    if (whoami) {
        len += snprintf(buf + len, sizeof(buf) - len, "%s: ", whoami);
    }
    if (code) {
        len += snprintf(buf + len, sizeof(buf) - len, "%s ", error_message(code));
    }
    if (fmt) {
        vsnprintf(buf + len, sizeof(buf) - len, fmt, args);
    }
    __android_log_print(ANDROID_LOG_ERROR, "MKE2FS-ERR", "%s", buf);
}
