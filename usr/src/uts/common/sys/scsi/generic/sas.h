/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 *
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2020 Joyent, Inc.
 */

/*
 * SAS Common Structures and Definitions
 * sas2r14, simplified/reduced
 */
#ifndef	_SAS_H
#define	_SAS_H
#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/sysmacros.h>
/*
 * SAS Address Frames
 * Trailing 4 byte CRC not included.
 */
typedef struct {
	DECL_BITFIELD3(
	    address_frame_type		:4,
	    device_type			:3,
	    reserved0			:1);
	DECL_BITFIELD2(
	    reason			:4,
	    reserved1			:4);
	DECL_BITFIELD5(
	    restricted0			:1,
	    smp_ini_port		:1,
	    stp_ini_port		:1,
	    ssp_ini_port		:1,
	    reserved2			:4);
	DECL_BITFIELD5(
	    restricted1			:1,
	    smp_tgt_port		:1,
	    stp_tgt_port		:1,
	    ssp_tgt_port		:1,
	    reserved3			:4);
	uint8_t		device_name[8];
	uint8_t		sas_address[8];
	uint8_t		phy_identifier;
	DECL_BITFIELD4(
	    break_reply_capable		:1,
	    requested_inside_zpsds	:1,
	    inside_zpsds_persistent	:1,
	    reserved4			:5);
	uint8_t		reserved5[6];
} sas_identify_af_t;

typedef struct {
	DECL_BITFIELD3(
	    address_frame_type		:4,
	    protocol			:3,
	    ini_port			:1);
	DECL_BITFIELD2(
	    connection_rate		:4,
	    features			:4);
	uint16_t	itag;			/* initiator connection tag */
	uint8_t		sas_dst[8];		/* destination sas address */
	uint8_t		sas_src[8];		/* source sas address */
	uint8_t		src_zone_group;		/* source zone group  */
	uint8_t		path_block_count;	/* pathway blocked count */
	uint16_t	arb_wait_time;		/* arbitration wait time */
	uint8_t		compat[4];		/* 'more' compatible features */
} sas_open_af_t;

#define	SAS_AF_IDENTIFY			0
#define	SAS_AF_OPEN			1

#define	SAS_IF_DTYPE_ENDPOINT		1
#define	SAS_IF_DTYPE_EDGE		2
#define	SAS_IF_DTYPE_FANOUT		3	/* obsolete */

#define	SAS_OF_PROTO_SMP		0
#define	SAS_OF_PROTO_SSP		1
#define	SAS_OF_PROTO_STP		2

#define	SAS_SSP_SUPPORT			0x8
#define	SAS_STP_SUPPORT			0x4
#define	SAS_SMP_SUPPORT			0x2


#define	SAS_CONNRATE_1_5_GBPS		0x8
#define	SAS_CONNRATE_3_0_GBPS		0x9
#define	SAS_CONNRATE_6_0_GBPS		0xA

#define	SAS_SATA_SUPPORT		0x1
#define	SAS_ATTACHED_NAME_OFFSET	52	/* SAS-2 only */

/*
 * SSP Definitions
 */
typedef struct {
	uint8_t		lun[8];
	uint8_t		reserved0;
	DECL_BITFIELD3(
	    task_attribute	:2,
	    command_priority	:4,
	    enable_first_burst	:1);
	uint8_t		reserved1;
	DECL_BITFIELD2(
	    reserved2		:2,
	    addi_cdb_len	:6);
	uint8_t		cdb[16];
	/* additional cdb bytes go here, followed by 4 byte CRC */
} sas_ssp_cmd_iu_t;

#define	SAS_CMD_TASK_ATTR_SIMPLE	0x00
#define	SAS_CMD_TASK_ATTR_HEAD		0x01
#define	SAS_CMD_TASK_ATTR_ORDERED	0x02
#define	SAS_CMD_TASK_ATTR_ACA		0x04

typedef struct {
	uint8_t		reserved0[8];
	uint16_t	status_qualifier;
	DECL_BITFIELD2(
	    datapres		:2,
	    reserved1		:6);
	uint8_t		status;
	uint8_t		reserved2[4];
	uint32_t	sense_data_length;
	uint32_t	response_data_length;
	uint8_t		rspd[];
	/* response data followed by sense data goes here */
} sas_ssp_rsp_iu_t;

/* length of bytes up to response data */
#define	SAS_RSP_HDR_SIZE		24

#define	SAS_RSP_DATAPRES_NO_DATA	0x00
#define	SAS_RSP_DATAPRES_RESPONSE_DATA	0x01
#define	SAS_RSP_DATAPRES_SENSE_DATA	0x02

/*
 * When the RSP IU is type RESPONSE_DATA,
 * the first 4 bytes of response data should
 * be a Big Endian representation of one of
 * these codes.
 */
#define	SAS_RSP_TMF_COMPLETE		0x00
#define	SAS_RSP_INVALID_FRAME		0x02
#define	SAS_RSP_TMF_NOT_SUPPORTED	0x04
#define	SAS_RSP_TMF_FAILED		0x05
#define	SAS_RSP_TMF_SUCCEEDED		0x08
#define	SAS_RSP_TMF_INCORRECT_LUN	0x09
#define	SAS_RSP_OVERLAPPED_OIPTTA	0x0A

/*
 * Task Management Functions- should be in a SAM definition file
 */
#define	SAS_ABORT_TASK			0x01
#define	SAS_ABORT_TASK_SET		0x02
#define	SAS_CLEAR_TASK_SET		0x04
#define	SAS_LOGICAL_UNIT_RESET		0x08
#define	SAS_I_T_NEXUS_RESET		0x10
#define	SAS_CLEAR_ACA			0x40
#define	SAS_QUERY_TASK			0x80
#define	SAS_QUERY_TASK_SET		0x81
#define	SAS_QUERY_UNIT_ATTENTION	0x82

/*
 * PHY size changed from SAS1.1 to SAS2.
 */
#define	SAS_PHYNUM_MAX			127
#define	SAS_PHYNUM_MASK			0x7f

#define	SAS2_PHYNUM_MAX			254
#define	SAS2_PHYNUM_MASK		0xff


/*
 * Maximum SMP payload size, including CRC
 */
#define	SAS_SMP_MAX_PAYLOAD		1032

#define	PROTOCOL_SPECIFIC_PAGE		0x18
#define	ENHANCED_PHY_CONTROL_PAGE	0x19

#pragma pack(1)
/*
 * SAS PHY Discovery Mode Sense Page
 *
 * See SPL 4, section 9.2.7.5
 */
typedef struct sas_phys_disc_mode_page {
	DECL_BITFIELD3(
	    spdm_pagecode	:6,
	    spdm_spf		:1,
	    spdm_ps		:1);
	uint8_t spdm_subpagecode;
	uint16_t spdm_pagelen;
	uint8_t _reserved1;
	DECL_BITFIELD2(
	    spdm_proto_id	:4,
	    _reserved2		:4);
	uint8_t spdm_gencode;
	uint8_t spdm_nphys;
	uint8_t spdm_descr[1];
} sas_phys_disc_mode_page_t;

/*
 * SAS Mode PHY Descriptor
 *
 * See SPL 4, section 9.2.7.5
 */
typedef struct sas_phy_descriptor {
	uint8_t _reserved1;
	uint8_t spde_phy_id;
	uint16_t _reserved2;
	DECL_BITFIELD3(
	    spde_attach_reason	:4,
	    spde_attach_devtype	:3,
	    _reserved3		:1);
	DECL_BITFIELD2(
	    spde_neg_rate	:4,
	    spde_reason		:4);
	DECL_BITFIELD5(
	    _reserved4			:1,
	    spde_att_smp_ini_port	:1,
	    spde_att_stp_ini_port	:1,
	    spde_att_ssp_ini_port	:1,
	    _reserved5			:4);
	DECL_BITFIELD5(
	    _reserved6			:1,
	    spde_att_smp_tgt_port	:1,
	    spde_att_stp_tgt_port	:1,
	    spde_att_ssp_tgt_port	:1,
	    _reserved7			:4);
	uint64_t spde_sas_addr;
	uint64_t spde_att_sas_addr;
	uint8_t spde_att_phy_id;
	DECL_BITFIELD7(
		spde_att_brk_reply	:1,
		spde_att_rqst_imside	:1,
		spde_att_inside	:1,
		spde_att_partial_cap	:1,
		spde_att_slumber_cap	:1,
		spde_att_power_cap	:2,
		spde_att_persist_cap	:1);
	DECL_BITFIELD4(
		spde_att_pwr_dis_cap	:1,
		spde_att_smp_prio_cpa	:1,
		spde_att_apta_cap	:1,
		_reserved8		:5);
	uint8_t _reserved9[5];
	DECL_BITFIELD2(
		spde_hw_min_rate	:4,
		spde_prog_min_rate	:4);
	DECL_BITFIELD2(
		spde_hw_max_rate	:4,
		spde_prog_max_rate	:4);
	uint8_t _reserved10[8];
	uint8_t _spde_vendor_spec[2];
	uint8_t _reserved11[4];
} sas_phy_descriptor_t;

/*
 * SAS Protocol-Specific Log Page
 * See SPL 4, section 9.2.8.1
 */
typedef struct sas_log_page {
	DECL_BITFIELD3(
	    slp_pagecode	:6,
	    slp_spf		:1,
	    slp_ds		:1);
	uint8_t slp_subpagecode;
	uint16_t slp_pagelen;
	uint8_t slp_portparam[1];
} sas_log_page_t;

/*
 * Protocol Specific Port log parameter
 *
 * See SPL 4. section 9.2.8.2
 */
typedef struct sas_port_param {
	uint16_t spp_port_id;
	DECL_BITFIELD5(
	    spp_fml		:2,
	    _reserved1		:3,
	    spp_tsd		:1,
	    _reserved2		:1,
	    spp_du		:1);
	uint8_t spp_param_len;
	DECL_BITFIELD2(
	    spp_proto_id	:4,
	    _reserved3		:4);
	uint8_t _reserved4;
	uint8_t spp_gencode;
	uint8_t spp_nphys;
	uint8_t spp_descr[1];
} sas_port_param_t;

/*
 * SAS PHY Log Descriptor
 *
 * See SPL 4, section 9.2.8.2
 */
typedef struct sas_phy_log_descr {
	uint8_t _reserved1;
	uint8_t sld_phy_id;
	uint8_t _reserved2;
	uint8_t sld_len;
	DECL_BITFIELD3(
	    sld_att_reason		:4,
	    sld_att_devtype		:3,
	    _reserved			:1);
	DECL_BITFIELD2(
	    sld_neg_rate		:4,
	    sld_neg_reason		:4);
	DECL_BITFIELD5(
	    _reserved5:1,
	    sld_att_smp_ini_port	:1,
	    sld_att_stp_ini_port	:1,
	    sld_att_ssp_ini_port	:1,
	    _reserved6			:4);
	DECL_BITFIELD5(
	    _reserved7			:1,
	    sld_att_smp_tgt_port	:1,
	    sld_att_stp_tgt_port	:1,
	    sld_att_ssp_tgt_port	:1,
	    _reserved8			:4);
	uint64_t sld_sas_addr;
	uint64_t sld_att_sas_addr;
	uint8_t sld_att_phy_id;
	uint8_t _reserved9[7];

	/* PHY error counters */
	uint32_t sld_inv_dword;
	uint32_t sld_running_disp;
	uint32_t sld_loss_sync;
	uint32_t sld_reset_prob;

	uint8_t _reserved10[2];
	uint8_t sld_event_descr_len;
	uint8_t sld_num_event_descr;
	uint8_t sld_event_descr[1];
} sas_phy_log_descr_t;
#pragma pack()

#ifdef	__cplusplus
}
#endif
#endif	/* _SAS_H */
