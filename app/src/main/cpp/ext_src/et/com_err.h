/*
 * Header file for the error reporting library
 */

#ifndef _COM_ERR_H
#define _COM_ERR_H

#include <stdarg.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long errcode_t;

struct error_table {
    char const * const * msgs;
    long base;
    int n_msgs;
};

typedef char * (*gettextf)(const char *);

extern void com_err (const char *, errcode_t, const char *, ...);
extern void com_err_va (const char *, errcode_t, const char *, va_list);
extern const char * error_message (errcode_t);
extern void (*set_com_err_hook (void (*) (const char *, errcode_t, const char *, va_list)))
    (const char *, errcode_t, const char *, va_list);
extern void (*reset_com_err_hook (void)) (const char *, errcode_t, const char *, va_list);

extern errcode_t add_error_table(const struct error_table *);
extern errcode_t remove_error_table(const struct error_table *);

extern const char * error_table_name(errcode_t num);

extern gettextf set_com_err_gettext(gettextf new_proc);

#ifndef COM_ERR_ATTR
#define COM_ERR_ATTR(x)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _COM_ERR_H */
extern void local_ext_log_handler(const char *whoami, errcode_t code, const char *fmt, va_list args);
