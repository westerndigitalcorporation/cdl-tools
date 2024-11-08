// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */
#ifndef CDL_H
#define CDL_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <scsi/scsi.h>
#include <scsi/sg.h>

/*
 * ATA Command Duration Limits log page size.
 */
#define CDL_ATA_LOG_SIZE	512

/*
 * Commands supporting duration limits.
 */
enum cdl_cmd {
	CDL_READ_16,
	CDL_WRITE_16,
	CDL_READ_32,
	CDL_WRITE_32,

	CDL_CMD_MAX,
};

/*
 * Command duration limits sub-pages for the control mode page 0Ah.
 */
enum cdl_p {
	CDLP_A,
	CDLP_B,
	CDLP_T2A,
	CDLP_T2B,
	CDLP_NONE,

	CDL_MAX_PAGES = CDLP_NONE,
};

enum cdl_rw {
	CDL_READ,
	CDL_WRITE,
};

enum cdl_limit {
	CDLP_MAX_INACTIVE_TIME,
	CDLP_MAX_ACTIVE_TIME,
	CDLP_DURATION_GUIDELINE,
};

struct cdl_desc {
	uint8_t		cdltunit;
	uint16_t	max_inactive_time;
	uint16_t	max_active_time;
	uint16_t	duration;
	uint8_t		max_inactive_policy;
	uint8_t		max_active_policy;
	uint8_t		duration_policy;
};

#define CDL_MAX_DESC	7

struct cdl_page {
	enum cdl_p		cdlp;
	enum cdl_rw		rw;
	uint8_t			perf_vs_duration_guideline;
	struct cdl_desc		descs[CDL_MAX_DESC];
	uint8_t			*msbuf;
	size_t			msbufsz;
};

/*
 * Device flags.
 */
#define CDL_ATA				(1 << 0)
#define CDL_DEV_SUPPORTED		(1 << 1)
#define CDL_DEV_ENABLED			(1 << 2)
#define CDL_GUIDELINE_DEV_SUPPORTED	(1 << 3)
#define CDL_HIGHPRI_DEV_SUPPORTED	(1 << 4)
#define CDL_HIGHPRI_DEV_ENABLED		(1 << 5)
#define CDL_USE_MS_SP			(1 << 6)
#define CDL_SHOW_RAW_VAL		(1 << 7)
#define CDL_SHOW_COUNT			(1 << 8)
#define CDL_USE_ATA			(1 << 9)
#define CDL_FORCE_DEV			(1 << 10)
#define CDL_STATISTICS_SUPPORTED	(1 << 11)

#define CDL_SYS_SUPPORTED		(1 << 16)
#define CDL_SYS_DEV_SUPPORTED		(1 << 17)
#define CDL_SYS_ENABLED			(1 << 18)

#define CDL_VERBOSE			(1 << 31)

#define CDL_VENDOR_LEN	9
#define CDL_ID_LEN	17
#define CDL_REV_LEN	5

#define CDL_SAT_VENDOR_LEN	9
#define CDL_SAT_PRODUCT_LEN	17
#define CDL_SAT_REV_LEN		5

struct cdl_ata_stats_desc {
	uint8_t		selector;
	uint8_t		flags;
	uint32_t	val;
};

struct cdl_ata_stats {
	struct cdl_ata_stats_desc reads_a[CDL_MAX_DESC];
	struct cdl_ata_stats_desc reads_b[CDL_MAX_DESC];
	struct cdl_ata_stats_desc writes_a[CDL_MAX_DESC];
	struct cdl_ata_stats_desc writes_b[CDL_MAX_DESC];
};

struct cdl_scsi_stats_desc {
	uint8_t		du : 1;
	uint8_t		tsd : 1;
	uint8_t		format_and_linking : 2;
	uint32_t	nr_inactive_target_miss_cmds;
	uint32_t	nr_active_target_miss_cmds;
	uint32_t	nr_target_miss_cmds;
	uint32_t	nr_cmds;
};

struct cdl_scsi_stats {
	struct cdl_scsi_stats_desc achievable_latency_target;
	struct cdl_scsi_stats_desc t2a[CDL_MAX_DESC];
	struct cdl_scsi_stats_desc t2b[CDL_MAX_DESC];
};

struct cdl_dev {
	/* Device file path and basename */
	char			*path;
	char			*name;

	/* Device file descriptor */
	int			fd;

	/* Device info */
	unsigned int		flags;
	unsigned int		acs_ver;
	char			vendor[CDL_VENDOR_LEN];
	char			id[CDL_ID_LEN];
	char			rev[CDL_REV_LEN];
	char			sat_vendor[CDL_SAT_VENDOR_LEN];
	char			sat_product[CDL_SAT_PRODUCT_LEN];
	char			sat_rev[CDL_SAT_REV_LEN];
	unsigned long long	capacity;
	bool			cdl_supported;
	enum cdl_p		cmd_cdlp[CDL_CMD_MAX];
	enum cdl_rw		cdlrw[CDL_MAX_PAGES];
	struct cdl_page		cdl_pages[CDL_MAX_PAGES];
	uint64_t		cmd_timeout;

	/* Minimum and maximum limits in nanoseconds */
	unsigned long long	min_limit;
	unsigned long long	max_limit;

	/* For ATA CDL log page caching */
	uint8_t			ata_cdl_log[CDL_ATA_LOG_SIZE];

	/*
	 * CDL statistics configuration: the format for ATA and SCSI differs
	 * and is not easily translatable from one to the other.
	 * So maintain different formats for each.
	 */
	union {
		struct cdl_ata_stats ata;
		struct cdl_scsi_stats scsi;
	} cdl_stats;
};

/*
 * For SG commands.
 */
#define CDL_SG_SENSE_MAX_LENGTH		64
#define CDL_SG_BUF_MAX_SIZE		4096
#define CDL_SG_CDB_MAX_SIZE		32
#define CDL_SG_CDB_DEFAULT_SIZE		16

struct cdl_sg_cmd {
	uint8_t		buf[CDL_SG_BUF_MAX_SIZE];
	size_t		bufsz;
	uint8_t		cdb[CDL_SG_CDB_MAX_SIZE];
	uint8_t		sense_buf[CDL_SG_SENSE_MAX_LENGTH];
	sg_io_hdr_t	io_hdr;
	uint8_t		sense_key;
	uint16_t	asc_ascq;
};

#define CDL_LINE_MAX_LEN	512

/* In cdl_dev.c */
int cdl_open_dev(struct cdl_dev *dev, mode_t mode);
void cdl_close_dev(struct cdl_dev *dev);
void cdl_revalidate_dev(struct cdl_dev *dev);
void cdl_init_cmd(struct cdl_sg_cmd *cmd, int cdb_len,
		  int direction, size_t bufsz);
int cdl_exec_cmd(struct cdl_dev *dev, struct cdl_sg_cmd *cmd);
void cdl_sg_get_str(char *dst, uint8_t *buf, int len);
void cdl_sg_set_be16(uint8_t *buf, uint16_t val);
void cdl_sg_set_be32(uint8_t *buf, uint32_t val);
void cdl_sg_set_be64(uint8_t *buf, uint64_t val);
void cdl_sg_set_le16(uint8_t *buf, uint16_t val);
void cdl_sg_set_le32(uint8_t *buf, uint32_t val);
void cdl_sg_set_le64(uint8_t *buf, uint64_t val);
uint16_t cdl_sg_get_be16(uint8_t *buf);
uint32_t cdl_sg_get_be32(uint8_t *buf);
uint64_t cdl_sg_get_be64(uint8_t *buf);
uint16_t cdl_sg_get_le16(uint8_t *buf);
uint32_t cdl_sg_get_le32(uint8_t *buf);
uint64_t cdl_sg_get_le64(uint8_t *buf);

/* In cdl.c */
const char *cdl_page_name(enum cdl_p cdlp);
uint8_t cdl_page_code(enum cdl_p cdlp);
int cdl_page_name2cdlp(char *page);
uint64_t cdl_t2time(uint64_t val, uint8_t t2cdlunit);

const char *cdl_cmd_str(enum cdl_cmd cmd);
uint8_t cdl_cmd_opcode(enum cdl_cmd cmd);
uint16_t cdl_cmd_sa(enum cdl_cmd cmd);

int cdl_page_show(struct cdl_page *page, unsigned int flags);
void cdl_page_save(struct cdl_page *page, FILE *f);
int cdl_page_parse_file(FILE *f, struct cdl_dev *dev, struct cdl_page *page);

int cdl_read_pages(struct cdl_dev *dev);
bool cdl_page_supported(struct cdl_dev *dev, enum cdl_p cdlp);
int cdl_write_page(struct cdl_dev *dev, struct cdl_page *page);
int cdl_check_enabled(struct cdl_dev *dev, bool enabled);
int cdl_statistics_show(struct cdl_dev *dev, int cdlp);
int cdl_statistics_reset(struct cdl_dev *dev);
int cdl_statistics_save(struct cdl_dev *dev, FILE *f);
int cdl_statistics_upload(struct cdl_dev *dev, FILE *f);

bool cdl_sysfs_exists(struct cdl_dev *dev, const char *format, ...);
unsigned long cdl_sysfs_get_ulong_attr(struct cdl_dev *dev,
				       const char *format, ...);
int cdl_sysfs_set_attr(struct cdl_dev *dev, const char *val,
		       const char *format, ...);

char *cdl_get_line(FILE *f, char *line);
char *cdl_skip_spaces(char *str, int skip);

/* In cdl_ata.c */
int cdl_ata_init(struct cdl_dev *dev);
int cdl_ata_read_page(struct cdl_dev *dev, enum cdl_p cdlp,
		      struct cdl_page *page);
int cdl_ata_write_page(struct cdl_dev *dev, struct cdl_page *page);
int cdl_ata_check_enabled(struct cdl_dev *dev, bool enabled);
int cdl_ata_enable(struct cdl_dev *dev, bool enable, bool highpri);
void cdl_ata_revalidate(struct cdl_dev *dev);
int cdl_ata_get_acs_ver(struct cdl_dev *dev);
const char *cdl_ata_acs_ver(struct cdl_dev *dev);
int cdl_ata_get_limits(struct cdl_dev *dev, struct cdl_sg_cmd *cmd);
int cdl_ata_get_statistics_supported(struct cdl_dev *dev);
int cdl_ata_statistics_show(struct cdl_dev *dev, int cdlp);
int cdl_ata_statistics_reset(struct cdl_dev *dev);
int cdl_ata_statistics_save(struct cdl_dev *dev, FILE *f);
int cdl_ata_statistics_upload(struct cdl_dev *dev, FILE *f);

/* In cdl_scsi.c */
void cdl_scsi_get_ata_information(struct cdl_dev *dev);
int cdl_scsi_init(struct cdl_dev *dev);
int cdl_scsi_read_page(struct cdl_dev *dev, enum cdl_p cdlp,
		       struct cdl_page *page);
int cdl_scsi_write_page(struct cdl_dev *dev, struct cdl_page *page);
int cdl_scsi_check_enabled(struct cdl_dev *dev, bool enabled);
void cdl_scsi_revalidate(struct cdl_dev *dev);
int cdl_scsi_statistics_show(struct cdl_dev *dev, int cdlp);
int cdl_scsi_statistics_reset(struct cdl_dev *dev);
int cdl_scsi_statistics_save(struct cdl_dev *dev, FILE *f);
int cdl_scsi_statistics_upload(struct cdl_dev *dev, FILE *f);

static inline bool cdl_dev_is_ata(struct cdl_dev *dev)
{
	return dev->flags & CDL_ATA;
}

static inline bool cdl_dev_use_ata(struct cdl_dev *dev)
{
	return dev->flags & CDL_USE_ATA;
}

static inline bool cdl_dev_statistics_supported(struct cdl_dev *dev)
{
	return dev->flags & CDL_STATISTICS_SUPPORTED;
}

static inline bool cdl_verbose(struct cdl_dev *dev)
{
	return dev->flags & CDL_VERBOSE;
}

#define cdl_dev_info(dev,format,args...)		\
	printf("%s: " format, (dev)->name, ##args)

#define cdl_dev_err(dev,format,args...)			\
	fprintf(stderr, "[ERROR] %s: " format, (dev)->name, ##args)

#endif /* CDL_H */
