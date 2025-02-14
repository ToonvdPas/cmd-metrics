/*
 * Het doel van dit programma is om PER PROGRAMMA inzicht te krijgen
 * in het resource-gebruik over de tijd heen.  De gemeten resources zijn:
 *
 *     - VSZ (in gebruik zijnd virtueel geheugen)
 *     - RSS (in gebruik zijnd fysiek geheugen)
 *     - CPU (aparte metingen voor user- en system-cycles)
 *     - TCP-sockets (optioneel via de optie -s)
 *
 * De metingen worden alleen uitgevoerd in delta-mode (-d).
 * In delta-mode dien je de processen te specificeren via de programmanaam (-c).
 * Je kunt in de commandline meerdere -c argumenten gebruiken (bv. -c nginx -c cache-main).
 * Het programma harkt PER PROGRAMMA de metrics van alle aktieve processen bij elkaar,
 * en print die elke interval uit.
 *
 * Op deze manier kun je een goed beeld krijgen van bijvoorbeeld het gedrag
 * van nginx en varnish op een moment dat alle STB's tegelijk verbinding zoeken.
 * (het zogenaamde "thundering herd" probleem)
 *
 * Compileren gaat als volgt:
 *
 *     $ make
 *
 * Zie de help-functie (-h) voor de gebruikshandleiding.
 *
 * (2020, 2021) TvdP
 */

#include <time.h>
#include <sys/time.h>  // voor setitimer()
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <features.h>
#include <linux/limits.h>
#include <libproc2/pids.h>
#include "procps-pids.h"
#include "mempool.h"
#include "inode-stats.h"
#include "cmd-metrics.h"

// PROCTAB *proc;

void SIGTERM_handler(int sig) {
    shouldStop = true;
}

void SIGINT_handler(int sig) {
    shouldStop = true;
}

void SIGHUP_handler(int sig) {
    shouldReopenStdout = true;
}

void SIGALRM_handler(int sig) {
    // no-op
}

void print_syntax(long ticks_per_sec, long cpu_cnt, long pagesize, long physpages, long physpages_avail) {
        fprintf(stderr, "syntax: $ cmd-metrics -d -c <cmd> [-c <cmd> ...] [-i <interval (s)>] [-r <repeat-header>] [-s]\n"
                        "    or: $ cmd-metrics -u <uid> [-u <uid> ...]     (list processes running with a specific numeric uid)\n"
                        "    or: $ cmd-metrics -c <cmd> [-c <cmd> ...]     (list processes running with a specific command)\n"
                        "    or: $ cmd-metrics -u <uid> -a [-c <cmd> ...]  (list processes running with a specific uid AND command)\n"
                        "    or: $ cmd-metrics [-t]                        (full list of processes, optionally including LWP's (threads))\n"
                        "    or: $ cmd-metrics -h         (this help text)\n"
	                "\n"
                        "Arguments:\n"
			"        -a                 Switch the filter options (-c and -u) to AND mode.  (default mode is OR)\n"
			"                           In the AND mode, proc-records are only listed if both cmd and uid match.\n"
                        "        -c <command>       Program name (executable) to filter on.\n"
			"                           Multiple -c arguments are allowed.\n"
			"                           When specifying only part of a command, it will match all processes\n"
			"                           whose command name start with that string.\n"
                        "        -d                 Delta-mode.  In this mode the program calculates the allocation and\n"
			"                           release of resources.  Those resources are VSZ, RSS, and (optionally) sockets.\n"
			"        -h                 This help text.\n"
                        "        -i <interval>      Interval in seconds.\n"
                        "        -s                 Provide some information on socket use.\n"
			"                           This option is only available in delta mode.\n"
                        "        -r <repeat-header> Interval for printing the header line. (only has effect in delta-mode)\n"
                        "                           -1: only print heading at start of run\n"
                        "                            0: don't print heading at all\n"
                        "                           >0: heading-interval in number of lines\n"
			"        -t                 Include threads (Light Weight Processes, LWP) in the listing.\n"
			"                           This option does not work in delta mode.\n"
                        "        -u <uid>           Numeric userid to filter on.  Multiple -u arguments are allowed.\n"
                        "\n"
                        "This program collects information on the usage of the resources VSZ, RSS, and (optionally) sockets.\n"
                        "Per program, it adds up the metrics of all running instances and shows the totals.\n"
                        "Originally, this was the reason for writing the tool: aggregation of the resource usage\n"
                        "of all processes which are running from the very same program (executable file).\n"
                        "This is called the 'delta-mode' of the tool, which is activated by the argument -d.\n"
			"Primary purpose of this mode is to detect probable memory leaks.\n"
                        "See examples 1 and 2.\n"
                        "\n"
			"Note: rss (resident segment size) and vsz (virtual segment size) are in KiB units.\n"
			"      utime (user time) and stime (system time) are in seconds since the starti of the processes.\n"
			"\n"
                        "Example 1 (delta-mode without socket-counting):\n"
                        "        # ./cmd-metrics -d -c nginx -c cache-main -i 5\n"
                        "                      |nginx                                                                  |cache-main\n"
                        "        datetime      |procs          vsz  delta-vsz          rss  delta-rss    utime    stime|procs          vsz  delta-vsz          rss  delta-rss    utime    stime\n"
                        "        20210430113405|    9   5258100736          0   4277514240          0      0.0      0.0|    1   8768339968          0   4414558208          0      0.0      0.0\n"
                        "        20210430113410|    9   5258100736          0   4277583872      69632      5.4      3.2|    1   8768339968          0   4410761216   -3796992     18.6      0.0\n"
                        "        20210430113415|    9   5258100736          0   4277583872          0      5.6      1.2|    1   8768339968          0   4412370944    1609728     14.4      3.4\n"
                        "\n"
                        "        Example 1 gathers metrics for two programs; nginx, and cache-main (=Varnish).\n"
			"        Every interval a single line of metrics is printed.\n"
                        "        This mode (the delta-mode) is selected by the use of argument -d.\n"
			"        We also specified a 5-second interval, and printing the headers after every 20 cycles.\n"
                        "        In delta-mode, the program will run forever, until interrupted for instance by ^C.\n"
                        "        Delta-mode without the option -s can safely be used on heavily loaded systems with many active processes.\n"
                        "\n"
                        "Example 2 (delta-mode with socket-counting):\n"
                        "        # ./cmd-metrics -d -s -c nginx -c cache-main -i 5\n"
                        "                      |nginx                                                                                                      |cache-main\n"
                        "        datetime      |procs          vsz  delta-vsz          rss  delta-rss    utime    stime socks dsock estab cl_wt listn  rest|procs          vsz  delta-vsz          rss  delta-rss    utime    stime socks dsock estab cl_wt listn  rest\n"
                        "        20210430105510|    9   5257912320          0   4277346304          0      6.0      1.6   606    21   491     0    27    88|    1   8768339968          0   4419424256     331776      8.4      4.0   297    14   290     2     1     4\n"
                        "        20210430105515|    9   5257912320          0   4277362688      16384      4.0      2.6   499  -107   383     0    27    89|    1   8768339968          0   4419411968     -12288      6.0      2.0   238   -59   233     0     1     4\n"
                        "        20210430105520|    9   5258059776     147456   4277399552      36864      4.4      1.2   526    27   411     0    27    88|    1   8768339968          0   4419600384     188416     13.6      0.0   249    11   245     0     1     3\n"
                        "        20210430105525|    9   5258059776          0   4277399552          0      4.6      2.8   543    17   427     0    27    89|    1   8768339968          0   4420235264     634880     10.8      1.0   259    10   255     0     1     3\n"

                        "\n"
                        "        Example 2 is equal to example 1, except for the extra option -s (include socket-counting).\n"
                        "        WARNING: Option -s requires special privileges to gain access to socket-information.\n"
			"                 Please consult the README file for instructions on how to install the cmd-metrics object file.\n"
                        "\n"
                        "Example 3 (process listing-mode):\n"
			"        $ cmd-metrics\n"
                        "        command          pid   ppid  euid euser           vsz        rss     utime    stime\n"
                        "        systemd            1      0     0 root       57032704    8163328     28621    89512\n"
                        "        kthreadd           2      0     0 root              0          0         0      346\n"
                        "        kworker/0:0H       4      2     0 root              0          0         0        0\n"
                        "        ksoftirqd/0        6      2     0 root              0          0         0      973\n"
                        "        .\n"
                        "        .\n"
                        "        .\n"
                        "        Example 3 lists all active processes on the system and quits.\n"
                        "\n"
                        "Example 4 (process listing-mode for specific uid's):\n"
                        "        $ ./cmd-metrics -u 992 -u 994\n"
                        "        command          pid   ppid  euid euser           vsz        rss     utime    stime\n"
                        "        nginx          34365  91602   994 nginx     583729152  218914816    105968    95669\n"
                        "        nginx          34366  91602   994 nginx     584949760  229904384    185921   130785\n"
                        "        nginx          34367  91602   994 nginx     597446656  263946240   1998303   944788\n"
                        "        nginx          34368  91602   994 nginx     584265728  223789056    134254   107540\n"
                        "        nginx          34369  91602   994 nginx     589692928  253509632    934723   471431\n"
                        "        nginx          34370  91602   994 nginx     585297920  235864064    289201   178765\n"
                        "        nginx          34371  91602   994 nginx     608522240  275410944   2661996  1238526\n"
                        "        nginx          34372  91602   994 nginx     587210752  244871168    490474   270729\n"
                        "        varnishd       80609      1   992 varnish    43372544    5591040      1145     2064\n"
                        "        cache-main     80620  80609   992 varnish  6243368960 3189968896  10885847  3166217\n"
                        "\n"
                        "        Example 4 lists the active processes with uid's 992 and 994.\n"
                        "        Please note that Varnish apparently runs from TWO different object files.\n"
			"        For a complete picture, in delta mode both object files should be specified in delta mode.\n"
                        "        So to monitor the processes in the above listing, use this command:.\n"
			"        $ cmd-metrics -d -c nginx -c varnishd -c cache-main -i 5\n"
                        "\n"
                        "Signal handling:\n"
                        "        SIGHUP:   Reopen stdout (for logfile-rotation)\n"
                        "        SIGINT:   Flush stdout and terminate.\n"
                        "        SIGTERM:  Flush stdout and terminate.\n"
                        "\n"
                        "The OS-kernel on this system reports the following parameters:\n"
			"        - Number of active CPU's:   %ld\n"
			"        - Memory phys pages:        %ld\n"
			"        - Memory phys pages avail:  %ld\n"
			"        - Memory page size (bytes): %ld\n"
			"        - Memory capacity (MB):     %ld\n"
                        "        - Clock ticks per second:   %ld\n", cpu_cnt, physpages, physpages_avail, pagesize, physpages*pagesize/(1024*1024), ticks_per_sec);
}


bool include_record(char cmd[CMD_LIST_LEN][CMD_STRING_LEN], int cmd_cnt,
                    uid_t uid[UID_LIST_LEN], int uid_cnt, bool uid_AND_cmd,
                    char command[CMD_STRING_LEN], uid_t userid) {
    int i;
    bool cmd_selected = false,
         uid_selected = false,
	 cmd_matched  = false,
	 uid_matched  = false,
	 result = false;

    if (cmd_cnt > 0) {
        cmd_selected = true;
        for (i=0; i<cmd_cnt; i++) {
            if (strncmp(cmd[i], command, strnlen(cmd[i], CMD_STRING_LEN)) == 0) {
	        cmd_matched = true;
		break;
            }
	}
    }

    if (uid_cnt > 0) {
        uid_selected = true;
        if ((uid_AND_cmd && cmd_matched) || (!uid_AND_cmd && !cmd_matched)) {
            for (i=0; i<uid_cnt; i++) {
                if (userid == uid[i]) {
	            uid_matched = true;
		    break;
		}
            }
	}
    }

    if (uid_AND_cmd) {
        if (cmd_matched && uid_matched) {
	    result = true;
	}
    } else {
        if (cmd_matched || uid_matched) {
	    result = true;
	}
    }
    return result;
}

void list_procs(char cmd[CMD_LIST_LEN][CMD_STRING_LEN], int cmd_cnt,
                uid_t uid[UID_LIST_LEN], int uid_cnt, bool uid_AND_cmd,
		LLNODE_PROCINFO *llnode_cur, bool first_iter, bool include_threads, int ticks_per_sec) {

    if (unlikely(first_iter)) {
        if (include_threads) {
            printf(LIST_PROCS_HEADER_FMT_STR_WITH_THREADS, LIST_PROCS_HEADER_PR_ARGS_WITH_THREADS);
        } else {
            printf(LIST_PROCS_HEADER_FMT_STR_NO_THREADS, LIST_PROCS_HEADER_PR_ARGS_NO_THREADS);
        }
    }

    if (include_threads) {
        printf(LIST_PROCS_FMT_STR_WITH_THREADS, LIST_PROCS_PR_ARGS_WITH_THREADS);
    } else {
        printf(LIST_PROCS_FMT_STR_NO_THREADS, LIST_PROCS_PR_ARGS_NO_THREADS);
    }
}

void initialize_metrics(int cmd_cnt, CMD_METRICS *cmd_metrics) {
    int i;
    if (cmd_cnt > 0) {
        for (i=0; i<cmd_cnt; i++) {
	    cmd_metrics[i].process_cnt = 0;
            cmd_metrics[i].metric_prev.vsz                    = cmd_metrics[i].metric_curr.vsz;
            cmd_metrics[i].metric_prev.rss                    = cmd_metrics[i].metric_curr.rss;
            cmd_metrics[i].metric_prev.utime                  = cmd_metrics[i].metric_curr.utime;
            cmd_metrics[i].metric_prev.stime                  = cmd_metrics[i].metric_curr.stime;
            cmd_metrics[i].metric_prev.sock.sock_total        = cmd_metrics[i].metric_curr.sock.sock_total;
            cmd_metrics[i].metric_prev.sock.state.established = cmd_metrics[i].metric_curr.sock.state.established;
            cmd_metrics[i].metric_prev.sock.state.close_wait  = cmd_metrics[i].metric_curr.sock.state.close_wait;
            cmd_metrics[i].metric_prev.sock.state.listener    = cmd_metrics[i].metric_curr.sock.state.listener;
            cmd_metrics[i].metric_prev.sock.state.rest        = cmd_metrics[i].metric_curr.sock.state.rest;

            cmd_metrics[i].metric_curr.vsz   = 0;
            cmd_metrics[i].metric_curr.rss   = 0;
            cmd_metrics[i].metric_curr.utime = 0;
            cmd_metrics[i].metric_curr.stime = 0;
            cmd_metrics[i].metric_curr.sock.sock_total        = 0;
            cmd_metrics[i].metric_curr.sock.state.established = 0;
            cmd_metrics[i].metric_curr.sock.state.close_wait  = 0;
            cmd_metrics[i].metric_curr.sock.state.listener    = 0;
            cmd_metrics[i].metric_curr.sock.state.rest        = 0;
        }
    } else {
        fprintf(stderr, "ERROR: one or more commands must be specified when using the delta mode\n");
	exit(EXIT_FAILURE);
    }
}

void accumulate_cmd_metrics(char cmd[CMD_LIST_LEN][CMD_STRING_LEN], int cmd_cnt, LLNODE_PROCINFO *llnode_cur, CMD_METRICS *cmd_metrics) {
    int i;
    if (cmd_cnt > 0) {
        for (i=0; i<cmd_cnt; i++) {
            if (strncmp(cmd[i], llnode_cur->proc_info.cmd, strnlen(cmd[i], CMD_STRING_LEN)) == 0) {
	        cmd_metrics[i].process_cnt++;
                cmd_metrics[i].metric_curr.vsz   += llnode_cur->proc_info.vsz;
                cmd_metrics[i].metric_curr.rss   += llnode_cur->proc_info.rss;
                cmd_metrics[i].metric_curr.utime += llnode_cur->proc_info.utime;
                cmd_metrics[i].metric_curr.stime += llnode_cur->proc_info.stime;
            }
        }
    } else {
        fprintf(stderr, "ERROR: one or more commands must be specified when using the delta mode\n");
	exit(EXIT_FAILURE);
    }
}

void accumulate_sock_metrics(char cmd[CMD_LIST_LEN][CMD_STRING_LEN], POOL **pool_ino, int cmd_cnt, CMD_METRICS *cmd_metrics, char **read_buf, long *buflen) {
    sock_ino_ent_t *sock_ino_ent_hash[INO_HASH_SIZE];  // Node-structure voor de socket-inode hash-table
    sock_aggr_t *s;                                    // Aggregated socket-stats voor een proces (cmd)
    int i;
    if (cmd_cnt > 0) {
        sock_ino_build_hash_table(sock_ino_ent_hash, pool_ino, read_buf, buflen);
        for (i=0; i<cmd_cnt; i++) {
            s = sock_ino_gather_cmd_stats(sock_ino_ent_hash, pool_ino, cmd[i]);
            cmd_metrics[i].metric_curr.sock.sock_total        = s->sock_total;
            cmd_metrics[i].metric_curr.sock.state.established = s->state.established;
            cmd_metrics[i].metric_curr.sock.state.close_wait  = s->state.close_wait;
            cmd_metrics[i].metric_curr.sock.state.listener    = s->state.listener;
            cmd_metrics[i].metric_curr.sock.state.rest        = s->state.rest;
	}
        sock_ino_destroy_hash_table(sock_ino_ent_hash, *pool_ino);
    } else {
        fprintf(stderr, "ERROR: one or more commands must be specified when using the delta mode\n");
	exit(EXIT_FAILURE);
    }
}

void list_deltas(int cmd_cnt, CMD_METRICS *cmd_metrics, char *time_string, int ticks_per_sec, int loop_interval, int heading_interval, bool include_sockets, bool first_iter, long *line_cnt) {
    long delta_vsz = 0, delta_rss = 0, delta_socket = 0;
    float utime = 0, stime = 0;
    int i;

    if (cmd_cnt > 0) {
        // druk de kopregels af
        if ((first_iter && heading_interval == -1) ||
            (heading_interval > 0 && (*line_cnt % heading_interval == 0))) {
            // druk eerste heading-regel af (procesnamen)
            printf("%14s", " ");
            for (i=0; i<cmd_cnt; i++) {
                if (include_sockets) {
                    printf("|%-109s", cmd_metrics[i].cmd);
                } else {
                    printf("|%-73s", cmd_metrics[i].cmd);
                }
            }
	    printf("\n");
            // druk tweede heading-regel af (kolomnamen)
            printf("%-14s", "datetime");
            for (i=0; i<cmd_cnt; i++) {
                if (include_sockets) {
                    printf("|%5s  %11s  %9s  %11s  %9s  %8s  %8s %5s %5s %5s %5s %5s %5s", "procs", "vsz", "delta-vsz", "rss", "delta-rss", "utime", "stime",
		                                                                           "socks", "dsock", "estab", "cl_wt", "listn", "rest");
                } else {
                    printf("|%5s  %11s  %9s  %11s  %9s  %8s  %8s", "procs", "vsz", "delta-vsz", "rss", "delta-rss", "utime", "stime");
                }
            }
	    printf("\n");
        }
        // druk de metrics-regel af
        printf("%14s", time_string);
        for (i=0; i<cmd_cnt; i++) {
            if (!first_iter) {
                delta_vsz = cmd_metrics[i].metric_curr.vsz - cmd_metrics[i].metric_prev.vsz;
                delta_rss = cmd_metrics[i].metric_curr.rss - cmd_metrics[i].metric_prev.rss;
                if (include_sockets) {
                    delta_socket = cmd_metrics[i].metric_curr.sock.sock_total - cmd_metrics[i].metric_prev.sock.sock_total;
                }
                // Het kan voorkomen dat één of meer processen tijdens de meetinterval gestopt zijn.
                // De ticks van die processen telden dan nog wel mee in prev maar niet curr.
                // Hierdoor kan in zo'n geval de berekening incidenteel negatief worden.
                // Om dit te voorkomen zetten we utime en stime in zo'n geval op 0.
                // Dit is natuurlijk eigenlijk niet netjes, maar anders moeten we van ALLE processen
                // de context gaan bijhouden, en de in curr weggevallen processen ook negeren in prev.
                // Dit is teveel werk.
                if (cmd_metrics[i].metric_prev.utime < cmd_metrics[i].metric_curr.utime) {
                    utime = ((float)cmd_metrics[i].metric_curr.utime - (float)cmd_metrics[i].metric_prev.utime)/ticks_per_sec;
                } else {
                    utime = (float) 0.0;
                }
                if (cmd_metrics[i].metric_prev.stime < cmd_metrics[i].metric_curr.stime) {
                    stime = ((float)cmd_metrics[i].metric_curr.stime - (float)cmd_metrics[i].metric_prev.stime)/ticks_per_sec;
                } else {
                    stime = (float) 0.0;
                }
            }
            if (include_sockets) {
                printf("|%5d  %11ld  %9ld  %11ld  %9ld  %8.2f  %8.2f %5ld %5ld %5ld %5ld %5ld %5ld",
                    cmd_metrics[i].process_cnt,
                    cmd_metrics[i].metric_curr.vsz,
                    delta_vsz,
                    cmd_metrics[i].metric_curr.rss,
                    delta_rss,
//                    utime,   // Liever de totaaltelling dan de meting over het interval (zie onderstaand)
//                    stime,   // Liever de totaaltelling dan de meting over het interval (zie onderstaand)
                    ((float)cmd_metrics[i].metric_curr.utime)/ticks_per_sec,
                    ((float)cmd_metrics[i].metric_curr.stime)/ticks_per_sec,
                    cmd_metrics[i].metric_curr.sock.sock_total,
                    delta_socket,
                    cmd_metrics[i].metric_curr.sock.state.established,
                    cmd_metrics[i].metric_curr.sock.state.close_wait,
                    cmd_metrics[i].metric_curr.sock.state.listener,
                    cmd_metrics[i].metric_curr.sock.state.rest);
            } else {
                printf("|%5d  %11ld  %9ld  %11ld  %9ld  %8.2f  %8.2f",
                    cmd_metrics[i].process_cnt,
                    cmd_metrics[i].metric_curr.vsz,
                    delta_vsz,
                    cmd_metrics[i].metric_curr.rss,
                    delta_rss,
//                    utime,   // Liever de totaaltelling dan de meting over het interval (zie onderstaand)
//                    stime,   // Liever de totaaltelling dan de meting over het interval (zie onderstaand)
                    ((float)cmd_metrics[i].metric_curr.utime)/ticks_per_sec,
                    ((float)cmd_metrics[i].metric_curr.stime)/ticks_per_sec);
            }
        }
	printf("\n");
        *line_cnt += 1;  // hier hogen we de globale variable op, dit blijft dus behouden
    } else {
        fprintf(stderr, "ERROR: one or more commands must be specified when using the delta mode\n");
	exit(EXIT_FAILURE);
    }
}

void populate_linked_list_node(LLNODE_PROCINFO * llnode_new) {
    memset(llnode_new, 0, sizeof(LLNODE_PROCINFO));
    strncpy(llnode_new->proc_info.cmd, PIDS_VAL(pids_cmd,     str,     pids_stack_data, pids_info_data), CMD_STRING_LEN);
    llnode_new->proc_info.euid  = PIDS_VAL(pids_euid,         u_int,   pids_stack_data, pids_info_data);
    strncpy(llnode_new->proc_info.euser, PIDS_VAL(pids_euser, str,     pids_stack_data, pids_info_data), CMD_STRING_LEN);
    llnode_new->proc_info.pid   = PIDS_VAL(pids_pid,          s_int,   pids_stack_data, pids_info_data);
    llnode_new->proc_info.ppid  = PIDS_VAL(pids_ppid,         s_int,   pids_stack_data, pids_info_data);
    llnode_new->proc_info.tgid  = PIDS_VAL(pids_tgid,         s_int,   pids_stack_data, pids_info_data);
    llnode_new->proc_info.vsz   = PIDS_VAL(pids_vsz,          ul_int,  pids_stack_data, pids_info_data);
    llnode_new->proc_info.rss   = PIDS_VAL(pids_rss,          ul_int,  pids_stack_data, pids_info_data);
    llnode_new->proc_info.utime = PIDS_VAL(pids_utime,        ull_int, pids_stack_data, pids_info_data);
    llnode_new->proc_info.stime = PIDS_VAL(pids_stime,        ull_int, pids_stack_data, pids_info_data);
    llnode_new->next = NULL;
}

void add_linked_list_node(LLNODE_PROCINFO **llnode_start, LLNODE_PROCINFO **llnode_cur) {
    LLNODE_PROCINFO *llnode_new = NULL;

    llnode_new = (struct procinfo_node *)malloc(sizeof(LLNODE_PROCINFO));
    populate_linked_list_node(llnode_new);
    if (*llnode_start == NULL) {
         // Dit is de eerste iteratie, special case...
         *llnode_start = llnode_new;
         *llnode_cur   = llnode_new;
     } else {
         // Alle volgende nodes
         (*llnode_cur)->next = llnode_new;
         *llnode_cur         = llnode_new;
     }
}

void destroy_linked_list(LLNODE_PROCINFO *llnode_start) {
    LLNODE_PROCINFO *llnode_cur = NULL,
                    *llnode_prv = NULL;

    llnode_cur = llnode_start;
    while (llnode_cur != NULL) {
        llnode_prv = llnode_cur;
	llnode_cur = llnode_cur->next;
	free(llnode_prv);
    }
}

void current_time(char *time_string) {
    time_t timestamp;
    struct tm* tm_info;

    timestamp = time(NULL);
    tm_info = localtime(&timestamp);

    strftime(time_string, 26, "%Y%m%d%H%M%S", tm_info);
}

int main(int argc, char **argv) {
    long pagesize        = sysconf(_SC_PAGESIZE);         // page size van het geheugen (verschilt van systeem tot systeem)
    long physpages       = sysconf(_SC_PHYS_PAGES);       // physical pages aantal
    long physpages_avail = sysconf(_SC_AVPHYS_PAGES);     // physical pages beschikbaar
    long cpu_cnt         = sysconf(_SC_NPROCESSORS_ONLN); // aantal actieve CPU's
    long ticks_per_sec   = sysconf(_SC_CLK_TCK);          // vraag de clock ticks/seconde op (verschilt van systeem tot systeem)
    char cmd[CMD_LIST_LEN][CMD_STRING_LEN];    // array van programmanamen waarop gefilterd moet worden
    char command[CMD_STRING_LEN];              // werkvariable voor bovenstaande array
    uid_t uid[UID_LIST_LEN];                   // array van UID's waaop gefilterd moet worden
    uid_t userid;                              // werkvariable voor bovenstaande array
    bool delta_mode = false;                   // start op in delta-mode yes/no
    bool uid_AND_cmd = false;                  // when specifying uid as well as cmd, they should both match (or not)
    bool include_sockets = false;              // verzamel ook de tellingen van de TCP-sockets
    bool include_threads = false;              // vraag ook de threads (LWP's) van de processen op
    bool first_iter = true;
    int uid_cnt = 0;
    int cmd_cnt = 0;
    int loop_interval = 0;                     // meet-interval (in seconden)
    int heading_interval = -1;                 // interval (in lines) waarmee de heading moet worden afgedrukt
    long line_cnt = 0;
    int i;
    char time_string[TIME_STRING_LEN];
    int option;
    char *optstring = "ac:dhi:r:stu:";
    char *end_ptr;
    POOL *pool_ino;
    POOL **pool_ino_pp = &pool_ino;
    char *read_buf = NULL;
    long buflen = INITIAL_READBUF_SIZE;

    // Vraag de naam op van de file waar stdout naar schrijft (is waarschijnlijk gezet dmv een redirect).
    // Dit hebben we nodig voor de freopen van stdout in geval van een SIGHUP.
    char stdout_path[PATH_MAX];                // PATH_MAX is gedefinieerd in /usr/include/linux/limits.h
    ssize_t stdout_pathlen;
    stdout_pathlen = readlink("/proc/self/fd/1", stdout_path, sizeof(stdout_path));
    stdout_path[stdout_pathlen] = '\0';

    // Registreer de signal-handlers.
    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIGTERM_handler;
    sigaction(SIGTERM, &sa, NULL);    // Vang SIGTERM af om de stdout-buffer te flushen voordat we stoppen
    sa.sa_handler = SIGINT_handler;
    sigaction(SIGINT, &sa, NULL);     // Vang SIGINT af om de stdout-buffer te flushen voordat we stoppen
    sa.sa_handler = SIGHUP_handler;
    sigaction(SIGHUP, &sa, NULL);     // Vang SIGHUP af om stdout te kunnen heropenen na logfile rotation
    sa.sa_handler = SIGALRM_handler;
    sigaction(SIGALRM, &sa, NULL);    // Vang SIGALRM af ten behoeve van setitimer(). De handler is een no-op.

    // Maak de aggregatietabel voor de metingen aan.
    CMD_METRICS cmd_metrics[CMD_LIST_LEN];

    opterr = 0;
    option = getopt(argc, argv, optstring);
    while (option != -1) {
        switch (option) {
	case 'a': uid_AND_cmd = true;
	          break;
	case 'c': if (cmd_cnt >= CMD_LIST_LEN) {
        	      fprintf(stderr, "ERROR: too many commands (-c) specified (maximum %d allowed)\n", CMD_LIST_LEN);
                      exit(EXIT_FAILURE);
                  }
                  strncpy(cmd[cmd_cnt], optarg, CMD_STRING_LEN);
                  cmd_cnt++;
		  break;
        case 'd': delta_mode = true;
                  break;
        case 'r': heading_interval = strtol(optarg, &end_ptr, 10);
                  if (*end_ptr != '\0') {
        	      fprintf(stderr, "ERROR: repeat-header (-r) must be either -1, 0 or a positive integer\n");
                      exit(EXIT_FAILURE);
                  }
                  break;
        case 's': include_sockets = true;
                  break;
        case 't': include_threads = true;
                  break;
        case 'i': loop_interval = strtol(optarg, &end_ptr, 10);
                  if (*end_ptr != '\0') {
        	      fprintf(stderr, "ERROR: loop-interval (-i) must be a positive integer\n");
                      exit(EXIT_FAILURE);
                  }
                  struct itimerval timer;
                  timer.it_value.tv_sec = loop_interval;
                  timer.it_value.tv_usec = 0;
                  timer.it_interval.tv_sec = loop_interval;
                  timer.it_interval.tv_usec = 0;
                  setitimer(ITIMER_REAL, &timer, NULL);
                  break;
        case 'u': if (uid_cnt >= UID_LIST_LEN) {
                      fprintf(stderr, "ERROR: too many uid's (-u) specified\n");
                      exit(EXIT_FAILURE);
                  }
                  uid[uid_cnt] = strtol(optarg, &end_ptr, 10);
                  if (*end_ptr != '\0') {
                      fprintf(stderr, "ERROR: UID (-u) must be a positive integer\n");
                      exit(EXIT_FAILURE);
                  }
                  uid_cnt++;
                  break;
        case 'h': 
        default:  print_syntax(ticks_per_sec, cpu_cnt, pagesize, physpages, physpages_avail);
                  exit(EXIT_FAILURE);
        }
        option = getopt(argc, argv, optstring);
    }

    if (include_threads && delta_mode) {
        fprintf(stderr, "ERROR: the threads option (-t) is not supported in delta-mode (-d), because it makes no sense.\n");
	exit(EXIT_FAILURE);
    }

    if (include_sockets && !delta_mode) {
        fprintf(stderr, "ERROR: the sockets option (-s) is only supported in delta-mode (-d).\n");
	exit(EXIT_FAILURE);
    }

    // Voor delta-processing, plaats de opgegeven cmd-namen in de cmd_metrics tabel
    if (delta_mode) {
        if (cmd_cnt > 0) {
	    for (i=0; i<cmd_cnt; i++) {
	        strncpy(cmd_metrics[i].cmd, cmd[i], CMD_STRING_LEN);
            }
	} else {
            fprintf(stderr, "ERROR: when using the delta mode (-d), please specify one or more commands (-c)\n");
	    exit(EXIT_FAILURE);
	}
    }

    // Creëer de memory-pool voor de opslag van de socket-hashtabel.
    if (include_sockets)
        pool_ino = pool_create(POOL_SIZE_INO);

    // Definieer pointers naar de nodes voor de procps linked list.
    LLNODE_PROCINFO *llnode_start = NULL,
                    *llnode_prv = NULL,
                    *llnode_cur = NULL;

LOOP_THIS_BABY_FOREVER:

    // Verzamel de proc data.
    if (procps_pids_new(&pids_info_data, pids_items, number_of_items) < 0) {
                fprintf(stderr, "ERROR - procps_pids_new failed\n");
                exit(EXIT_FAILURE);
    }

    // Bouw een linked list op met records uit de process table.
    llnode_start = NULL;
    while ((pids_stack_data = procps_pids_get(pids_info_data, include_threads ? PIDS_FETCH_THREADS_TOO : PIDS_FETCH_TASKS_ONLY))) {
        if (cmd_cnt > 0 || uid_cnt > 0) {
            // Voeg alleen nodes toe voor de opgegeven commando's en userid's.
            strncpy(command, PIDS_VAL(pids_cmd, str, pids_stack_data, pids_info_data), CMD_STRING_LEN);
	    userid = PIDS_VAL(pids_euid, u_int, pids_stack_data, pids_info_data);
            if (include_record(cmd, cmd_cnt, uid, uid_cnt, uid_AND_cmd, command, userid)) {
                add_linked_list_node(&llnode_start, &llnode_cur);
            }
        } else {
            // Er zijn geen commando's en userid's opgegeven; voeg ALLE procinfo records toe aan de linked list.
            add_linked_list_node(&llnode_start, &llnode_cur);
        }
    }

    // Ruim de proc data op (alles staat nu in de linked list).
    procps_pids_unref(&pids_info_data);

    // Initialiseer de cmd_metrics records en verzamel de socket-metrics.
    if (delta_mode) {
        initialize_metrics(cmd_cnt, cmd_metrics);
        if (include_sockets) {
            accumulate_sock_metrics(cmd, pool_ino_pp, cmd_cnt, cmd_metrics, &read_buf, &buflen);   // socket-metrics
        }
    }

    // Doorloop de linked list met proc data en verzamel de cmd-metrics.
    // Hier is een verschil tussen delta-mode=true en delta-mode=false;
    //   - delta-mode=false: de gegevens worden binnen de loop direct afgedrukt via list_procs()
    //   - delta-mode=true:  de gegevens worden binnen de loop alleen verzameld (in cmd_metrics)
    llnode_cur = llnode_start;
    while (llnode_cur != NULL) {
        if (delta_mode) {
	    accumulate_cmd_metrics(cmd, cmd_cnt, llnode_cur, cmd_metrics);                         // cmd-metrics
	} else {
            list_procs(cmd, cmd_cnt, uid, uid_cnt, uid_AND_cmd, llnode_cur, first_iter, include_threads, ticks_per_sec);
            if (unlikely(first_iter)) {
                first_iter = false;
            }
        }
	llnode_cur = llnode_cur->next;
    }

    // Indien we in delta-mode draaien hebben we nu alle gegevens verzameld in cmd_metrics.
    // Die gaan we nu afdrukken via de functie list_deltas().  Dit levert één regel op.
    // De array cmd_metrics[] bevat één record per opgegeven commando (-c).
    if (delta_mode) {
        current_time(time_string);
        list_deltas(cmd_cnt, cmd_metrics, time_string, ticks_per_sec, loop_interval, heading_interval, include_sockets, first_iter, &line_cnt);
        if (unlikely(first_iter)) {
            first_iter = false;
        }
    }

    // Ruim de linked list op.
    destroy_linked_list(llnode_start);

    // Handel de signals af.
    if (loop_interval > 0) {
        // Block het programma totdat de itimer afloopt en we een SIGALRM ontvangen.
        pause();
	if (shouldStop) {
            // We hebben een SIGTERM ontvangen.  Flush en stop.
            fflush(stdout);
        } else {
            if (shouldReopenStdout) {
                // We hebben een SIGHUP ontvangen, vermoedelijk vanwege een logfile-rotation.
                // Reopen stdout.  De variabele stdout_path bevat de volledige naam van de file die
                // bij het opstarten eventueel is meegegeven via redirection (bv. > /var/log/cmd-metrics.log)
                shouldReopenStdout = false;
                fflush(stdout);
                if (freopen(stdout_path, "w", stdout) == NULL) {
		    fprintf(stderr, "WARNING: freopen of %s failed: %d (%s)\n", stdout_path, errno, strerror(errno));
		}
            }
            // GOTO's zijn niet altijd slecht.
            goto LOOP_THIS_BABY_FOREVER;
        }
    }

    exit(EXIT_SUCCESS);
}
