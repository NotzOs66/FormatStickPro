// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025, Western Digital Corporation or its affiliates.
 */
#include "platform_defs.h"
#include "atomic.h"
#include "libfrog/zones.h"
#include <sys/ioctl.h>

/* random size that allows efficient processing */
#define ZONES_PER_REPORT		16384

static atomic_t cached_reporting_disabled;

struct xfrog_zone_report *
xfrog_report_zones(
	int			fd,
	uint64_t		sector,
	struct xfrog_zone_report *rep)
{
	if (!rep) {
		rep = calloc(1, struct_size(rep, zones, ZONES_PER_REPORT));
		if (!rep) {
			fprintf(stderr, "%s\n",
_("Failed to allocate memory for reporting zones."));
			return NULL;
		}
	}

	/*
	 * Try cached report zones first. If this fails, fallback to the regular
	 * (slower) report zones.
	 */
	rep->rep.sector = sector;
	rep->rep.nr_zones = ZONES_PER_REPORT;

	if (atomic_read(&cached_reporting_disabled))
		goto uncached;

	rep->rep.flags = BLK_ZONE_REP_CACHED;
	if (ioctl(fd, BLKREPORTZONEV2, &rep->rep)) {
		atomic_inc(&cached_reporting_disabled);
		goto uncached;
	}

	return rep;

uncached:
	rep->rep.flags = 0;
	if (ioctl(fd, BLKREPORTZONE, rep)) {
		fprintf(stderr, "%s %s\n",
_("ioctl(BLKREPORTZONE) failed:\n"),
			strerror(-errno));
		free(rep);
		return NULL;
	}

	return rep;
}
