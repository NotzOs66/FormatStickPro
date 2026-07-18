#include "ext2_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "blkid/blkid.h"
#include "devname.h"

char *get_devname(blkid_cache cache, const char *token, const char *value)
{
    int cache_created = 0;
    char *name;

    if (!cache) {
        if (blkid_get_cache(&cache, NULL) < 0)
            return NULL;
        cache_created = 1;
    }

    name = blkid_get_devname(cache, token, value);

    if (cache_created)
        blkid_put_cache(cache);

    return name;
}
