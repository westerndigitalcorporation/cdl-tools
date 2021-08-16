// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */

#include "cdl.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>

/*
 * Print usage.
 */
static void cdladm_usage(void)
{
	printf("Usage: cdladm [options] <device path>\n");
	printf("Common Options:\n"
	       "  --version            : Print version number and exit\n"
	       "  --help | -h          : General help message\n"
	       "  --verbose | -v       : Verbose output\n");
	printf("Mutually exclusive options:\n"
	       "  --list-pages         : List supported pages\n"
	       "  --show-pages         : Show all supported pages\n"
	       "  --show-page <page>   : Show a supported page\n"
	       "  --save-page <page>   : Save a page to a file\n"
	       "  --upload-page <file> : Upload a page to the device\n");
}

static int cdl_list_pages(struct cdl_dev *dev, char *page)
{
	char cmd[256];
	int i, j, n;

	cdl_dev_info(dev, "Supported pages:\n");
	for (i = 0; i < CDL_MAX_PAGES; i++) {
		if (!cdl_page_supported(dev, i))
			continue;

		memset(cmd, 0, sizeof(cmd));
		for (j = 0, n = 0; j < CDL_CMD_MAX; j++) {
			if ((int)dev->cmd_cdlp[j] != i)
				continue;
			n += sprintf(&cmd[n], "%s%s",
				     n == 0 ? "" : ", ",
				     cdl_cmd_str(j));
		}

		printf("    %s (%s)\n",
		       cdl_page_name(i), cmd);
	}

	return 0;
}

static int cdl_show_pages(struct cdl_dev *dev, char *page)
{
	int i;

	for (i = 0; i < CDL_MAX_PAGES; i++) {
		if (!cdl_page_supported(dev, i))
			continue;
		printf("Page %s:\n", cdl_page_name(i));
		cdl_print_page(&dev->cdl_pages[i], stdout);
	}

	return 0;
}

static int cdl_show_page(struct cdl_dev *dev, char *page)
{
	int cdlp;

	cdlp = cdl_page_name2cdlp(page);
	if (cdlp < 0)
		return -1;

	if (!cdl_page_supported(dev, cdlp)) {
		printf("Page %s is not supported¥n", page);
		return -1;
	}

	cdl_print_page(&dev->cdl_pages[cdlp], stdout);

	return 0;
}

static int cdl_save_page(struct cdl_dev *dev, char *page)
{
	enum cdl_p cdlp;
	char *path;
	FILE *f;
	int ret = 0;

	cdlp = cdl_page_name2cdlp(page);
	if (cdlp < 0)
		return -1;

	if (!cdl_page_supported(dev, cdlp)) {
		printf("Page %s is not supported¥n", page);
		return -1;
	}

	if (asprintf(&path, "%s-cdl-%s", dev->name, cdl_page_name(cdlp)) < 0) {
		fprintf(stderr, "Failed to allocate file path\n");
		return -ENOMEM;
	}

	f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "Open page %s file %s failed\n",
			cdl_page_name(cdlp), path);
		ret = -errno;
		goto out;
	}

	printf("Saving page %s to file %s\n",
	       cdl_page_name(cdlp), path);

	cdl_print_page(&dev->cdl_pages[cdlp], f);

	fclose(f);

out:
	free(path);

	return ret;
}

static int cdl_upload_page(struct cdl_dev *dev, char *path)
{
	struct cdl_page page;
	FILE *f;
	int ret;

	/* Open the file and parse it */
	f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Open file %s failed\n",
			path);
		return -errno;
	}

	printf("Parsing file %s...\n", path);
	ret = cdl_parse_page_file(f, &page);
	fclose(f);
	if (ret)
		return ret;

	printf("Uploading page %s:\n",
	       cdl_page_name(page.cdlp));
	cdl_print_page(&page, stdout);

	ret = cdl_write_page(dev, &page);
	if (ret)
		return ret;

	return 0;
}

/*
 * Possible commands.
 */
enum {
	CDL_LIST_PAGES,
	CDL_SHOW_PAGES,
	CDL_SHOW_PAGE,
	CDL_SAVE_PAGE,
	CDL_UPLOAD_PAGE,
};
typedef int (*cdl_command_t)(struct cdl_dev *, char *arg);
static const cdl_command_t cdl_command[] = {
	cdl_list_pages,
	cdl_show_pages,
	cdl_show_page,
	cdl_save_page,
	cdl_upload_page,
};

/*
 * Main function.
 */
int main(int argc, char **argv)
{
	struct cdl_dev dev;
	bool cdl_supported;
	char *arg = NULL;
	int command = -1;
	int i, ret;

	/* Initialize */
	memset(&dev, 0, sizeof(dev));
	dev.fd = -1;
	for (i = 0; i < CDL_CMD_MAX; i++)
		dev.cmd_cdlp[i] = CDLP_NONE;
	for (i = 0; i < CDL_MAX_PAGES; i++)
		dev.cdl_pages[i].cdlp = CDLP_NONE;

	/* Parse options */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--version") == 0) {
			printf("cdladm, version %s\n", PACKAGE_VERSION);
			printf("Copyright (C) 2021, Western Digital Corporation"
			       " or its affiliates.\n");
			return 0;
		}

		if (strcmp(argv[i], "--help") == 0 ||
		    strcmp(argv[i], "-h") == 0) {
			cdladm_usage();
			printf("See \"man cdladm\" for more information\n");
			return 0;
		}

		if (strcmp(argv[i], "--verbose") == 0 ||
		    strcmp(argv[i], "-v") == 0) {
			dev.flags |= CDL_VERBOSE;
			continue;
		}

		if (strcmp(argv[i], "--list-pages") == 0) {
			if (command != -1)
				goto err_cmd_line;
			command = CDL_LIST_PAGES;
			continue;
		}

		if (strcmp(argv[i], "--show-pages") == 0) {
			if (command != -1)
				goto err_cmd_line;
			command = CDL_SHOW_PAGES;
			continue;
		}

		if (strcmp(argv[i], "--show-page") == 0) {
			if (command != -1)
				goto err_cmd_line;
			command = CDL_SHOW_PAGE;
			i++;
			if (i >= argc - 1)
				goto err_cmd_line;
			arg = argv[i];
			continue;
		}

		if (strcmp(argv[i], "--save-page") == 0) {
			if (command != -1)
				goto err_cmd_line;
			command = CDL_SAVE_PAGE;
			i++;
			if (i >= argc - 1)
				goto err_cmd_line;
			arg = argv[i];
			continue;
		}

		if (strcmp(argv[i], "--upload-page") == 0) {
			if (command != -1)
				goto err_cmd_line;
			command = CDL_UPLOAD_PAGE;
			i++;
			if (i >= argc - 1)
				goto err_cmd_line;
			arg = argv[i];
			continue;
		}

		if (argv[i][0] == '-') {
			fprintf(stderr, "Invalid option '%s'\n", argv[i]);
			return 1;
		} else {
			break;
		}
	}

	if (i != argc - 1) {
		fprintf(stderr, "Invalid command line\n");
		return 1;
	}

	/* Get device path */
	dev.path = realpath(argv[i], NULL);
	if (!dev.path) {
		fprintf(stderr, "Failed to get device real path\n");
		return 1;
	}

	/* Open the device */
	if (cdl_open_dev(&dev) < 0)
		return 1;

	printf("%s:\n", dev.name);
	printf("    Vendor: %s\n", dev.vendor);
	printf("    Product: %s\n", dev.id);
	printf("    Revision: %s\n", dev.rev);
	printf("    %llu 512-byte sectors (%llu.%03llu TB)\n",
	       dev.capacity,
	       (dev.capacity << 9) / 1000000000000,
	       ((dev.capacity << 9) % 1000000000000) / 1000000000);

	cdl_supported = cdl_check_support(&dev);
	if (!cdl_supported) {
		printf("%s: command duration limits is not supported\n",
		       dev.name);
		ret = 1;
		goto out;
	}

	printf("%s: command duration limits is supported\n",
	       dev.name);
	ret = cdl_read_pages(&dev);
	if (ret)
		goto out;

	/* Execute the command */
	ret = (cdl_command[command])(&dev, arg);

out:
	cdl_close_dev(&dev);

	return ret;

err_cmd_line:
	fprintf(stderr, "Invalid command line\n");

	return 1;
}

