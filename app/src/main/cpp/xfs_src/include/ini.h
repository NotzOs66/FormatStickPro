#ifndef __INI_H__
#define __INI_H__

typedef int (*ini_handler)(void* user, const char* section,
                           const char* name, const char* value);

int ini_parse(const char* filename, ini_handler handler, void* user);

#endif
