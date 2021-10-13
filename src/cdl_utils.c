// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */

#include "cdl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

/*
 * Command duration limits sub-pages for the control mode page 0Ah.
 */
static const struct cdlp_info {
	uint8_t		subpage;
	const char	*name;
} cdl_page[CDL_MAX_PAGES + 1] = {
	{ 0x03,	"A"	},
	{ 0x04,	"B"	},
	{ 0x07,	"T2A"	},
	{ 0x08,	"T2B"	},
	{ 0x00,	"none"	},
};

const char *cdl_page_name(enum cdl_p cdlp)
{
	return cdl_page[cdlp].name;
}

uint8_t cdl_page_code(enum cdl_p cdlp)
{
	return cdl_page[cdlp].subpage;
}

int cdl_page_name2cdlp(char *page)
{
	uint8_t cdlp = 0;

	while (cdl_page[cdlp].subpage) {
		if (strcmp(page, cdl_page[cdlp].name) == 0)
			return cdlp;
		cdlp++;
	}

	fprintf(stderr, "Unknown page name %s\n", page);

	return -1;
}

static const struct cdl_cmd_info {
	uint8_t		opcode;
	uint16_t	sa;
	const char	*name;
} cdl_cmd[CDL_CMD_MAX] = {
	{ 0x88,		0x0000,		"READ_16"	},
	{ 0x8a,		0x0000,		"WRITE_16"	},
	{ 0x7f,		0x0009,		"READ_32"	},
	{ 0x0f,		0x000b,		"WRITE_32"	},
};

const char *cdl_cmd_str(enum cdl_cmd cmd)
{
	return cdl_cmd[cmd].name;
}

uint8_t cdl_cmd_opcode(enum cdl_cmd cmd)
{
	return cdl_cmd[cmd].opcode;
}

uint16_t cdl_cmd_sa(enum cdl_cmd cmd)
{
	return cdl_cmd[cmd].sa;
}

/*
 * CDL pages A and B time limits in nanoseconds.
 */
static uint64_t cdl_simple_time(uint64_t val, uint8_t cdlunit)
{
	switch (cdlunit) {
	case 0x00:
		return 0;
	case 0x04:
		/* 1 microsecond */
		return val * 1000ULL;
	case 0x05:
		/* 10 milliseconds */
		return val * 10000000ULL;
	case 0x06:
		/* 500 milliseconds */
		return val * 500000000ULL;
	default:
		return 0;
	}
}

static char *cdl_simple_time_str(char *str, uint64_t time, uint8_t cdlunit)
{
	uint64_t t = cdl_simple_time(time, cdlunit);

	if (t >= 1000000)
		sprintf(str, "%" PRIu64 " ms", t / 1000000);
	else
		sprintf(str, "%" PRIu64 " us", t / 1000);

	return str;
}

/*
 * CDL pages T2A and T2B time limits in nanoseconds.
 */
static uint64_t cdl_t2time(uint64_t val, uint8_t t2cdlunit)
{
	switch (t2cdlunit) {
	case 0x00:
		return 0;
	case 0x06:
		/* 500 nanoseconds */
		return val * 500;
	case 0x08:
		/* 1 microsecond */
		return val;
	case 0x0A:
		/* 10 milliseconds */
		return val * 10000000ULL;
	case 0x0E:
		/* 500 milliseconds */
		return val * 500000000ULL;
	default:
		return 0;
	}
}

static char *cdl_t2time_str(char *str, uint64_t time, uint8_t t2cdlunit)
{
	uint64_t t = cdl_t2time(time, t2cdlunit);

	if (t >= 1000000)
		sprintf(str, "%" PRIu64 " ms", t / 1000000);
	else if (t > 1000 && t % 1000 == 0)
		sprintf(str, "%" PRIu64 " us", t / 1000);
	else
		sprintf(str, "%" PRIu64 " ns", t);

	return str;
}

static const char *cdl_perf_str(uint8_t val)
{
	switch (val) {
	case 0x00:
		return "0";
	case 0x01:
		return "0.5";
	case 0x02:
		return "1.0";
	case 0x03:
		return "1.5";
	case 0x04:
		return "2.0";
	case 0x05:
		return "2.5";
	case 0x06:
		return "3";
	case 0x07:
		return "4";
	case 0x08:
		return "5";
	case 0x09:
		return "8";
	case 0x0A:
		return "10";
	case 0x0B:
		return "15";
	case 0x0C:
		return "20";
	default:
		return "?";
	}
}

static const char *cdl_policy_str(uint8_t policy)
{
	switch (policy) {
	case 0x00:
		return "complete-earliest";
	case 0x01:
		return "continue-next-limit";
	case 0x02:
		return "continue-no-limit";
	case 0x0d:
		return "complete-unavailable";
	case 0x0e:
		return "abort-recovery";
	case 0x0f:
		return "abort";
	default:
		return "?";
	}
}

static void cdl_page_show_simple_raw(struct cdl_page *page)
{
	struct cdl_desc *desc;
	int i;

	for (i = 0, desc = &page->descs[0]; i < CDL_MAX_DESC; i++, desc++) {
		printf("  Descriptor %d:\n", i + 1);
		printf("    duration guideline: 0x%04x\n",
		       (unsigned int)desc->duration);
	}
}

static void cdl_page_show_simple(struct cdl_page *page)
{
	struct cdl_desc *desc;
	char str[64];
	int i;

	for (i = 0, desc = &page->descs[0]; i < CDL_MAX_DESC; i++, desc++) {
		printf("  Descriptor %d:\n", i + 1);

		if (desc->duration)
			printf("    duration guideline: %s\n",
			       cdl_simple_time_str(str, desc->duration,
						   desc->cdltunit));
		else
			printf("    duration guideline: no limit\n");
	}
}

static int cdl_page_save_simple(struct cdl_page *page, FILE *f)
{
	struct cdl_desc *desc;
	int i;

	fprintf(f, "# %s page format:\n", cdl_page_name(page->cdlp));
	fprintf(f,
		"# t2cdlunits can be one of:\n"
		"#   - none   : 0x0\n"
		"#   - 1us    : 0x4\n"
		"#   - 10ms   : 0x5\n"
		"#   - 500ms  : 0x6\n");
	fprintf(f, "\n");

	fprintf(f, "cdlp: %s\n", cdl_page_name(page->cdlp));
	fprintf(f, "\n");

	for (i = 0, desc = &page->descs[0]; i < CDL_MAX_DESC; i++, desc++) {
		fprintf(f, "== descriptor: %d\n", i + 1);
		fprintf(f, "cdlunit: 0x%1x\n",
			(unsigned int)desc->cdltunit);
		fprintf(f, "duration-guideline: %u\n",
		       (unsigned int)desc->duration);
		fprintf(f, "\n");
	}

	return 0;
}

static void cdl_page_show_t2_raw(struct cdl_page *page)
{
	struct cdl_desc *desc;
	int i;

	if (page->cdlp == CDLP_T2A)
		printf("  perf_vs_duration_guideline : 0x%1x\n",
		       (unsigned int)page->perf_vs_duration_guideline);

	for (i = 0, desc = &page->descs[0]; i < CDL_MAX_DESC; i++, desc++) {
		printf("  Descriptor %d:\n", i + 1);

		printf("    T2 CDL units             : 0x%1x\n",
		       (unsigned int)desc->cdltunit);

		printf("    max inactive time        : 0x%04x\n",
		       (unsigned int)desc->max_inactive_time);
		printf("    max inactive policy      : 0x%1x\n",
		       (unsigned int)desc->max_inactive_policy);

		printf("    max active time          : 0x%04x\n",
		       (unsigned int)desc->max_active_time);
		printf("    max active policy        : 0x%1x\n",
		       (unsigned int)desc->max_active_policy);

		printf("    duration guideline       : 0x%04x\n",
		       (unsigned int)desc->duration);
		printf("    duration guideline policy: 0x%1x\n",
		       (unsigned int)desc->duration_policy);
	}
}

static void cdl_page_show_t2(struct cdl_page *page)
{
	struct cdl_desc *desc;
	char str[64];
	int i;

	if (page->cdlp == CDLP_T2A)
		printf("  perf_vs_duration_guideline : %s%%\n",
		       cdl_perf_str(page->perf_vs_duration_guideline));

	for (i = 0, desc = &page->descs[0]; i < CDL_MAX_DESC; i++, desc++) {
		printf("  Descriptor %d:\n", i + 1);

		if (desc->max_inactive_time) {
			printf("    max inactive time        : %s\n",
			       cdl_t2time_str(str, desc->max_inactive_time,
					      desc->cdltunit));
			printf("    max inactive policy      : %s\n",
			       cdl_policy_str(desc->max_inactive_policy));
		} else {
			printf("    max inactive time        : no limit\n");
		}

		if (desc->max_active_time) {
			printf("    max active time          : %s\n",
			       cdl_t2time_str(str, desc->max_active_time,
					      desc->cdltunit));
			printf("    max active policy        : %s\n",
			       cdl_policy_str(desc->max_active_policy));
		} else {
			printf("    max active time          : no limit\n");
		}

		if (desc->duration) {
			printf("    duration guideline       : %s\n",
			       cdl_t2time_str(str, desc->duration,
					      desc->cdltunit));
			printf("    duration guideline policy: %s\n",
			       cdl_policy_str(desc->duration_policy));
		} else {
			printf("    duration guideline       : no limit\n");
		}
	}
}

static int cdl_page_save_t2(struct cdl_page *page, FILE *f)
{
	struct cdl_desc *desc;
	int i;

	/* File legend */
	fprintf(f, "# %s page format:\n", cdl_page_name(page->cdlp));
	if (page->cdlp == CDLP_T2A)
		fprintf(f,
			"# perf-vs-duration-guideline can be one of:\n"
			"#   - 0%%    : 0x0\n"
			"#   - 0.5%%  : 0x1\n"
			"#   - 1.0%%  : 0x2\n"
			"#   - 1.5%%  : 0x3\n"
			"#   - 2.0%%  : 0x4\n"
			"#   - 2.5%%  : 0x5\n"
			"#   - 3%%    : 0x6\n"
			"#   - 4%%    : 0x7\n"
			"#   - 5%%    : 0x8\n"
			"#   - 8%%    : 0x9\n"
			"#   - 10%%   : 0xa\n"
			"#   - 15%%   : 0xb\n"
			"#   - 20%%   : 0xc\n");
	fprintf(f,
		"# t2cdlunits can be one of:\n"
		"#   - none   : 0x0\n"
		"#   - 500ns  : 0x6\n"
		"#   - 1us    : 0x8\n"
		"#   - 10ms   : 0xa\n"
		"#   - 500ms  : 0xe\n");
	fprintf(f,
		"# max-inactive-time-policy can be one of:\n"
		"#   - complete-earliest    : 0x0\n"
		"#   - complete-unavailable : 0xd\n"
		"#   - abort                : 0xf\n");
	fprintf(f,
		"# max-active-time-policy can be one of:\n"
		"#   - complete-earliest    : 0x0\n"
		"#   - complete-unavailable : 0xd\n"
		"#   - abort-recovery       : 0xe\n"
		"#   - abort                : 0xf\n");
	fprintf(f,
		"# duration-guideline-policy can be one of:\n"
		"#   - complete-earliest    : 0x0\n"
		"#   - continue-next-limit  : 0x1\n"
		"#   - continue-no-limit    : 0x2\n"
		"#   - complete-unavailable : 0x0d\n"
		"#   - abort                : 0xf\n");
	fprintf(f, "\n");

	fprintf(f, "cdlp: %s\n\n", cdl_page_name(page->cdlp));

	if (page->cdlp == CDLP_T2A)
		fprintf(f, "perf-vs-duration-guideline: 0x%1x\n\n",
			page->perf_vs_duration_guideline);

	for (i = 0, desc = &page->descs[0]; i < CDL_MAX_DESC; i++, desc++) {
		fprintf(f, "== descriptor: %d\n", i + 1);

		fprintf(f, "t2cdlunits: 0x%1x\n",
			(unsigned int)desc->cdltunit);

		fprintf(f, "max-inactive-time: %u\n",
		       (unsigned int)desc->max_inactive_time);
		fprintf(f, "max-inactive-time-policy: 0x%1x\n",
			(unsigned int)desc->max_inactive_policy);

		fprintf(f, "max-active-time: %u\n",
		       (unsigned int)desc->max_active_time);
		fprintf(f, "max-active-time-policy: 0x%1x\n",
			(unsigned int)desc->max_active_policy);

		fprintf(f, "duration-guideline: %u\n",
			(unsigned int)desc->duration);
		fprintf(f, "duration-guideline-policy: 0x%1x\n",
			(unsigned int)desc->duration_policy);
		fprintf(f, "\n");
	}

	return 0;
}

void cdl_page_show(struct cdl_page *page, bool raw)
{
	if (page->cdlp == CDLP_A || page->cdlp == CDLP_B) {
		if (raw) {
			cdl_page_show_simple_raw(page);
			return;
		}
		cdl_page_show_simple(page);
		return;
	}

	if (raw) {
		cdl_page_show_t2_raw(page);
		return;
	}

	cdl_page_show_t2(page);
}

void cdl_page_save(struct cdl_page *page, FILE *f)
{
	if (page->cdlp == CDLP_A || page->cdlp == CDLP_B)
		cdl_page_save_simple(page, f);
	else
		cdl_page_save_t2(page, f);
}

#define CDL_LINE_MAX_LEN	512

static char *cdl_get_line(FILE *f, char *line)
{
	char *str, *s;
	int len;

	while (1) {

		memset(line, 0, CDL_LINE_MAX_LEN);
		if (!fgets(line, CDL_LINE_MAX_LEN, f))
			return NULL;

		/* Ignore leading spaces & tabs */
		str = &line[0];
		while (*str &&
		       (isblank(*str) ||
			*str == '\n' ||
			*str == '\r'))
			str++;

		/* Skip comment lines and empty lines */
		if (!*str || *str == '#')
			continue;

		break;
	}

	/* Remove end of line spaces and cariage returns */
	len = strlen(str);
	s = str + len - 1;
	while (s != str &&
	       (isblank(*s) ||
		*s == '\n' ||
		*s == '\r')) {
		*s = '\0';
		s--;
	}

	return str;
}

static char *cdl_skip_spaces(char *str, int skip)
{
	str += skip;
	while (*str && isblank(*str))
		str++;

	return *str ? str : NULL;
}

static int cdl_parse_cdlp(struct cdl_page *page, FILE *f, char *line)
{
	char *str;

	str = cdl_get_line(f, line);
	if (!str || strncmp(str, "cdlp:", 5) != 0) {
		fprintf(stderr, "No cdlp field in file\n");
		return -1;
	}

	str = cdl_skip_spaces(str, 5);
	if (!str) {
		fprintf(stderr, "No cdlp field value specified\n");
		return -1;
	}

	if (strcmp(str, "A") == 0) {
		page->cdlp = CDLP_A;
	} else if (strcmp(str, "B") == 0) {
		page->cdlp = CDLP_B;
	} else if (strcmp(str, "T2A") == 0) {
		page->cdlp = CDLP_T2A;
	} else if (strcmp(str, "T2B") == 0) {
		page->cdlp = CDLP_T2B;
	} else {
		fprintf(stderr, "Invalid cdlp %s\n", str);
		return -1;
	}

	return 0;
}

static int cdl_parse_pvsdg(struct cdl_page *page, FILE *f, char *line)
{
	char *str;

	str = cdl_get_line(f, line);
	if (!str || strncmp(str, "perf-vs-duration-guideline:", 27) != 0) {
		fprintf(stderr,
			"No perf-vs-duration-guideline field in file\n");
		return -1;
	}

	str = cdl_skip_spaces(str, 27);
	if (!str) {
		fprintf(stderr,
			"No perf-vs-duration-guideline field value specified\n");
		return -1;
	}

	page->perf_vs_duration_guideline = strtol(str, NULL, 16);
	if (page->perf_vs_duration_guideline > 0xc) {
		fprintf(stderr,
			"Invalid perf-vs-duration-guideline field value\n");
		return -1;
	}

	return 0;
}

static unsigned long cdl_parse_val(FILE *f, char *line, char *field, int base)
{
	int len = strlen(field);
	char *str;

	str = cdl_get_line(f, line);
	if (!str || strncmp(str, field, len) != 0) {
		fprintf(stderr,
			"Field %s not found\n", field);
		return -1;
	}

	str = cdl_skip_spaces(str, len);
	if (!str) {
		fprintf(stderr,
			"No value specified for field %s\n", field);
		return -1;
	}

	return strtol(str, NULL, base);
}

static int cdl_parse_desc(struct cdl_page *page, int d,
			  FILE *f, char *line)
{
	struct cdl_desc *desc = &page->descs[d];
	char desc_line[128];
	char *str;

	sprintf(desc_line, "== descriptor: %d", d + 1);
	str = cdl_get_line(f, line);
	if (!str || strcmp(str, desc_line) != 0) {
		fprintf(stderr, "No descriptor %d found\n", d + 1);
		return -1;
	}

	if (page->cdlp == CDLP_A || page->cdlp == CDLP_B) {
		desc->cdltunit =
			cdl_parse_val(f, line, "cdlunit:", 16);
		desc->duration =
			cdl_parse_val(f, line, "duration-guideline:", 10);
	} else {
		desc->cdltunit =
			cdl_parse_val(f, line, "t2cdlunits:", 16);
		desc->max_inactive_time =
			cdl_parse_val(f, line, "max-inactive-time:", 10);
		desc->max_inactive_policy =
			cdl_parse_val(f, line, "max-inactive-time-policy:", 16);
		desc->max_active_time =
			cdl_parse_val(f, line, "max-active-time:", 10);
		desc->max_active_policy =
			cdl_parse_val(f, line, "max-active-time-policy:", 16);
		desc->duration =
			cdl_parse_val(f, line, "duration-guideline:", 10);
		desc->duration_policy =
			cdl_parse_val(f, line, "duration-guideline-policy:", 16);
	}

	return 0;
}

int cdl_page_parse_file(FILE *f, struct cdl_page *page)
{
	char line[CDL_LINE_MAX_LEN];
	int i, ret = 0;

	/* Initialize the page as completely empty */
	memset(page, 0, sizeof(struct cdl_page));
	page->cdlp = CDLP_NONE;

	/* cdlp must be first */
	ret = cdl_parse_cdlp(page, f, line);
	if (ret)
		return ret;

	/*
	 * For the T2A page, we must have the perf-vs-duration-guideline
	 * field next.
	 */
	if (page->cdlp == CDLP_T2A) {
		ret = cdl_parse_pvsdg(page, f, line);
		if (ret)
			return ret;
	}

	/* Next, we should have 7 descriptors */
	for (i = 0; i < CDL_MAX_DESC; i++) {
		ret = cdl_parse_desc(page, i, f, line);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * Read all supported pages.
 */
int cdl_read_pages(struct cdl_dev *dev)
{
	struct cdl_page *page;
	uint8_t cdlp;
	int i, ret;

	/* Read supported pages */
	for (i = 0; i < CDL_CMD_MAX; i++) {
		cdlp = dev->cmd_cdlp[i];
		if (cdlp == CDLP_NONE)
			continue;

		page = &dev->cdl_pages[cdlp];
		if (page->cdlp == cdlp) {
			/* We already read this one */
			continue;
		}

		ret = cdl_read_page(dev, cdlp, page);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * Check if a page type is supported.
 */
bool cdl_page_supported(struct cdl_dev *dev, enum cdl_p cdlp)
{
	int i;

	for (i = 0; i < CDL_CMD_MAX; i++) {
		if (dev->cmd_cdlp[i] == cdlp)
			return true;
	}

	return false;
}

/*
 * Check CDL support.
 */
bool cdl_check_support(struct cdl_dev *dev)
{
	bool cdl_supported = false;
	int i;

	/*
	 * Command duration limits is supported only with READ 16, WRITE 16,
	 * READ 32 and WRITE 32. Go through all these commands one at a time
	 * and check if any support duration limits.
	 */
	for (i = 0; i < CDL_CMD_MAX; i++) {
		dev->cmd_cdlp[i] = cdl_get_cmd_cdlp(dev, i);
		if (dev->cmd_cdlp[i] != CDLP_NONE)
			cdl_supported = true;
	}

	return cdl_supported;
}
