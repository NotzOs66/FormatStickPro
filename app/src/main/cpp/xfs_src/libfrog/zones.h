// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025, Western Digital Corporation or its affiliates.
 */
#ifndef __LIBFROG_ZONES_H__
#define __LIBFROG_ZONES_H__

struct xfrog_zone_report {
	struct blk_zone_report	rep;
	struct blk_zone		zones[];
};

struct xfrog_zone_report *
xfrog_report_zones(int fd, uint64_t sector, struct xfrog_zone_report *rep);

#endif /* __LIBFROG_ZONES_H__ */
