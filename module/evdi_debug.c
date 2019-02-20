// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/sched.h>

#include "evdi_debug.h"

void evdi_log_process(void)
{
	int task_pid = (int)task_pid_nr(current);
	char task_comm[TASK_COMM_LEN] = { 0 };

	get_task_comm(task_comm, current);

	if (current->group_leader) {
		char process_comm[TASK_COMM_LEN] = { 0 };

		get_task_comm(process_comm, current->group_leader);
		EVDI_INFO("Task %d (%s) of process %d (%s)\n",
			  task_pid,
			  task_comm,
			  (int)task_pid_nr(current->group_leader),
			  process_comm);
	} else {
		EVDI_INFO("Task %d (%s)\n",
			  task_pid,
			  task_comm);
	}
}
