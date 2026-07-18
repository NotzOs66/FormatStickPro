#include "ext2_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mke2fs.h"

void check_mount(const char *device, int force, const char *type)
{
    // Bypass mount checks for virtual USB translator
}

char *get_progname(char *argv_zero)
{
    char *cp = strrchr(argv_zero, '/');
    if (!cp) return argv_zero;
    return cp + 1;
}

void proceed_question(int delay)
{
    // Auto-confirm for non-interactive Android environment
}

void populate_fs3(ext2_filsys fs, const char *src_root)
{
    // Stub
}
