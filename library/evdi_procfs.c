// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (c) 2015 - 2024 DisplayLink (UK) Ltd.

#include "evdi_lib.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const int command_length = 50;
const int name_length = 30;

static FILE *open_process_folder(const char *pid)
{
	FILE *pf;
	char command[command_length];

	snprintf(command, command_length, "/proc/%s/stat", pid);
	pf = fopen(command, "r");

	return pf;
}

static void close_folder(FILE *pf)
{
	if (fclose(pf) != 0)
		exit(EXIT_FAILURE);
}

static char *process_name(FILE *pf)
{
	int Xorg_name_len = 7;	// 6 + 1 for null terminator
	char *name = (char *)malloc(Xorg_name_len * sizeof(char));

	fscanf(pf, "%*s");
	fscanf(pf, "%6s", name); // Xorg process had name (Xorg)

	return name;
}

static bool is_name_Xorg(const char *name)
{
	return strstr(name, "Xorg") != NULL;
}

static bool is_Xorg(const char *pid)
{
	FILE *pf;
	bool result;
	char *name;

	pf = open_process_folder(pid);
	if (pf == NULL)
		return false;
	name = process_name(pf);
	result = is_name_Xorg(name);

	close_folder(pf);
	free(name);

	return result;
}

static bool is_numeric(const char *str)
{
	return str[0] >= '0' && str[0] <= '9';
}

static bool is_Xorg_process_folder(const struct dirent *proc_entry)
{
	const char *folder_name = proc_entry->d_name;

	if (is_numeric(folder_name) && is_Xorg(folder_name))
		return true;

	return false;
}

static bool iterate_through_all_process_folders_and_find_Xorg(void)
{
	DIR *proc_dir;
	const struct dirent *proc_entry;
	bool result = false;

	proc_dir = opendir("/proc");
	if (proc_dir == NULL)
		return false;


	while ((proc_entry = readdir(proc_dir)) != NULL) {
		if (is_Xorg_process_folder(proc_entry)) {
			result = true;
			break;
		}
	}

	closedir(proc_dir);
	return result;
}

bool Xorg_running(void)
{
	return iterate_through_all_process_folders_and_find_Xorg();
}
