/* Implementatie van de nieuwe API van progps-ng.
 * Dit is slechts een testprogrammaatje om vat te krijgen op de nieuwe API.
 * Procps is overgegaan op de nieuwe API vanaf versie 4.
 *
 * Compileren als volgt:
 *
 *     $ gcc -l proc2 -o proc-info proc-info.c
 *
 * (2025) TvdP
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libproc2/pids.h>


int main(int argc, char *argv[]) {
	enum pids_item pids_items[] = {PIDS_ID_PID, PIDS_ID_PPID, PIDS_NLWP, PIDS_ID_TID, PIDS_ID_TGID, PIDS_CMD};
	enum rel_items                {    rel_pid,     rel_ppid,  rel_nlwp,     rel_tid,     rel_tgid,  rel_cmd};
	int number_of_items = sizeof(pids_items) / sizeof(pids_items[0]);
	struct pids_info *pids_info_data = NULL;
	struct pids_stack *pids_stack_data;
	char *pid_cmd, *ptype;
	int pid_pid, pid_ppid, pid_nlwp, pid_tid, pid_tgid;
	int retval;

	if ((retval = procps_pids_new(&pids_info_data, pids_items, number_of_items)) < 0) {
		printf("ERROR - procps_pids_new returned %d\n", retval);
		exit(2);
	}
	while ((pids_stack_data = procps_pids_get(pids_info_data, PIDS_FETCH_THREADS_TOO))) {
		pid_pid  = PIDS_VAL(rel_pid,  s_int, pids_stack_data, pids_info_data);
		pid_ppid = PIDS_VAL(rel_ppid, s_int, pids_stack_data, pids_info_data);
		pid_nlwp = PIDS_VAL(rel_nlwp, s_int, pids_stack_data, pids_info_data);
		pid_tid  = PIDS_VAL(rel_tid,  s_int, pids_stack_data, pids_info_data);
		pid_tgid = PIDS_VAL(rel_tgid, s_int, pids_stack_data, pids_info_data);
		pid_cmd  = PIDS_VAL(rel_cmd,  str,   pids_stack_data, pids_info_data);
		if (pid_pid == pid_tgid) {
			ptype = "PROC";
		} else {
			ptype = "LWP";
		}
			printf("pid %7d, ppid %7d, nlwp %4d, tid %7d, tgid %7d, ptype %-4s, cmd %s\n",
			        pid_pid, pid_ppid, pid_nlwp, pid_tid, pid_tgid, ptype, pid_cmd);
	}

	procps_pids_unref(&pids_info_data);
}
