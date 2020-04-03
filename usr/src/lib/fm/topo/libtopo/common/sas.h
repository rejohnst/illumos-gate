/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2020 Joyent, Inc.
 */

#ifndef	_FM_SAS_H
#define	_FM_SAS_H

#ifdef	__cplusplus
extern "C" {
#endif

#define	SAS_VERSION	1

extern int sas_init(topo_mod_t *, topo_version_t);
extern void sas_fini(topo_mod_t *);

#define	TOPO_METH_SAS2DEV		"sas_dev_fmri"
#define	TOPO_METH_SAS2DEV_DESC		"compute dev-scheme FMRI"
#define	TOPO_METH_SAS2DEV_VERSION	0

#define	TOPO_METH_SAS2HC		"sas_hc_fmri"
#define	TOPO_METH_SAS2HC_DESC		"compute hc-scheme FMRI"
#define	TOPO_METH_SAS2HC_VERSION	0

#define	TOPO_METH_SAS_DEV_PROP		"sas_device_props_set"
#define	TOPO_METH_SAS_DEV_PROP_DESC	"retrieve device properties"
#define	TOPO_METH_SAS_DEV_PROP_VERSION	0

#define	TOPO_METH_SAS_PHY_ERR		"sas_get_phy_err_counter"
#define	TOPO_METH_SAS_PHY_ERR_DESC	"get PHY link state error counters"
#define	TOPO_METH_SAS_PHY_ERR_VERSION	0

#define	TOPO_METH_SAS_LINK_RATE		"sas_get_phy_get_link_rate"
#define	TOPO_METH_SAS_LINK_RATE_DESC	"get PHY link transmission rate"
#define	TOPO_METH_SAS_LINK_RATE_VERSION	0

/*
 * A common pattern when reading SCSI mode/log pages is to first issue a
 * command to do a partial read in order to determine the full page length and
 * then do a second command to read the full page in.  For our narrow use case
 * we use a statically-sized buffer that we know will be large enough to hold
 * the pages we're interested in, to avoid hitting the disk twice.
 */
#define	PAGE_BUFSZ		4096

/*
 * To avoid re-reading the same pages when sequentually executing various
 * prop methods for the same device, we briefly cache the contents of the most
 * recently read mode sense and log sense pages.
 */
#define	PAGE_CACHE_TTL		SEC2NSEC(5)

typedef struct sas_scsi_cache_ent {
	hrtime_t	ssce_ts;
	char		ssce_devpath[PATH_MAX + 1];
	uchar_t		ssce_pagebuf[PAGE_BUFSZ];
} sas_scsi_cache_ent_t;

typedef struct sas_scsi_cache {
	sas_scsi_cache_ent_t	ssc_modepage_cache;
	sas_scsi_cache_ent_t	ssc_logpage_cache;
} sas_scsi_cache_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _FM_SAS_H */
