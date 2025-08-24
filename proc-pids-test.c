/*
 * Copyright (C) 2025 Toon van der Pas, Houten.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Implementatie van de nieuwe API van progps-ng.
 * Dit is slechts een testprogrammaatje om vat te krijgen op de nieuwe API.
 * Procps is overgegaan op de nieuwe API vanaf versie 4.
 *
 * Compileren als volgt:
 *
 *     $ gcc -l proc2 -o proc-pids-test proc-pids-test.c
 *
 * (2025) TvdP
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libproc2/pids.h>

// libproc2 related variables (see libproc2/pids.h)
enum pids_item pids_items[] =  {PIDS_CMD,
                                PIDS_ID_EUID,
                                PIDS_ID_EUSER,
                                PIDS_ID_PID,
                                PIDS_ID_PPID,
                                PIDS_ID_TGID,
                                PIDS_MEM_VIRT,
                                PIDS_MEM_RES,
                                PIDS_TICS_USER,
                                PIDS_TICS_SYSTEM};
enum rel_items {pids_cmd,
                pids_euid,
                pids_euser,
                pids_pid,
                pids_ppid,
                pids_tgid,
                pids_vsz,
                pids_rss,
                pids_utime,
                pids_stime};
int number_of_items = sizeof(pids_items)/sizeof(pids_items[0]);
struct pids_info *pids_info_data = NULL;
struct pids_stack *pids_stack_data = NULL;

int main(int argc, char *argv[]) {
	char *pid_cmd, *pid_euser, *ptype;
	signed int pid_pid, pid_ppid, pid_tgid;
	unsigned int pid_euid;
	unsigned long pid_vsz, pid_rss;
	unsigned long long pid_utime, pid_stime;

	int retval;

	if ((retval = procps_pids_new(&pids_info_data, pids_items, number_of_items)) < 0) {
		printf("ERROR - procps_pids_new returned %d\n", retval);
		exit(2);
	}
	while ((pids_stack_data = procps_pids_get(pids_info_data, PIDS_FETCH_THREADS_TOO))) {
		pid_cmd   = PIDS_VAL(pids_cmd,   str,     pids_stack_data, pids_info_data);
		pid_pid   = PIDS_VAL(pids_pid,   s_int,   pids_stack_data, pids_info_data);
		pid_ppid  = PIDS_VAL(pids_ppid,  s_int,   pids_stack_data, pids_info_data);
		pid_tgid  = PIDS_VAL(pids_tgid,  s_int,   pids_stack_data, pids_info_data);
		pid_euid  = PIDS_VAL(pids_euid,  u_int,   pids_stack_data, pids_info_data);
		pid_euser = PIDS_VAL(pids_euser, str,     pids_stack_data, pids_info_data);
		pid_vsz   = PIDS_VAL(pids_vsz,   ul_int,  pids_stack_data, pids_info_data);
		pid_rss   = PIDS_VAL(pids_rss,   ul_int,  pids_stack_data, pids_info_data);
		pid_utime = PIDS_VAL(pids_utime, ull_int, pids_stack_data, pids_info_data);
		pid_stime = PIDS_VAL(pids_stime, ull_int, pids_stack_data, pids_info_data);
		if (pid_pid == pid_tgid) {
			ptype = "PROC";
		} else {
			ptype = "LWP";
		}
			printf("cmd %32s, pid %7d, ppid %7d, tgid %7d, ptype %4s, euid %-5d, euser %6s, vsz %9ld, rss %9ld, utime %6lld, stime %6lld\n",
			        pid_cmd, pid_pid, pid_ppid, pid_tgid, ptype, pid_euid, pid_euser, pid_vsz, pid_rss, pid_utime, pid_stime);
	}

	procps_pids_unref(&pids_info_data);
}
