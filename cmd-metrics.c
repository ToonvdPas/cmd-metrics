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

void print_syntax(long ticks_per_sec, long cpu_cnt, long psize, long physpages, long physpages_avail) {
        fprintf(stderr, "syntax: $ cmd-metrics -d -c <cmd> [-c <cmd> ...] [-i <interval (s)>] [-r <repeat-header>] [-s]\n"
                        "    or: $ cmd-metrics -u <uid>   (list of processes for numeric uid)\n"
                        "    or: $ cmd-metrics            (full list of processes)\n"
                        "    or: $ cmd-metrics -h         (this help text)\n"
	                "\n"
                        "Arguments:\n"
                        "        -d                 Activate delta-mode.  (this was the only single reason for writing this tool!)\n"
			"        -h                 This help text.\n"
                        "        -i <interval>      Interval in seconds.\n"
                        "        -c <command>       Program name (executable) to filter on.  Multiple -c arguments are allowed.\n"
                        "        -s                 Switches on the socket-counting.\n"
                        "        -r <repeat-header> printing-interval for the heading (only for the delta-mode)\n"
                        "                           -1: only print heading at start of run\n"
                        "                            0: don't print heading at all\n"
                        "                           >0: heading-interval in number of lines\n"
                        "        -u <uid>           Numeric userid to filter on.  Multiple -u arguments are allowed.\n"
                        "\n"
                        "This program collects some performance data for specific programs.\n"
                        "Per program, it adds up the metrics for all matching instances, and only shows the totals.\n"
                        "This was the reason for writing this tool: aggregation of the resource usage of\n"
                        "all processes which are running the same program (executable file).\n"
                        "It is called the 'delta-mode' of the tool, which is activated by the argument -d.\n"
                        "See examples 1 and 2.\n"
                        "\n"
                        "Example 1 (delta-mode without socket-counting):\n"
                        "        # ./cmd-metrics -d -c nginx -c cache-main -i 5\n"
                        "                      |nginx                                                                  |cache-main\n"
                        "        datetime      |procs          vsz  delta-vsz          rss  delta-rss   %%utime   %%stime|procs          vsz  delta-vsz          rss  delta-rss   %%utime   %%stime\n"
                        "        20210430113405|    9   5258100736          0   4277514240          0      0.0      0.0|    1   8768339968          0   4414558208          0      0.0      0.0\n"
                        "        20210430113410|    9   5258100736          0   4277583872      69632      5.4      3.2|    1   8768339968          0   4410761216   -3796992     18.6      0.0\n"
                        "        20210430113415|    9   5258100736          0   4277583872          0      5.6      1.2|    1   8768339968          0   4412370944    1609728     14.4      3.4\n"
                        "\n"
                        "        Example 1 gathers metrics for two programs; nginx, and cache-main (=Varnish).\n"
			"        Every interval the metrics for the programs are printed on a single line.\n"
                        "        This mode (the delta-mode) is selected by the use of argument -d.\n"
			"        We also specified a 5-second interval, and printing the headers after every 20 cycles.\n"
                        "        In delta-mode, the program will run forever, until interrupted for instance by ^C.\n"
                        "        Delta-mode without the option -s can safely be used on heavily loaded systems.\n"
                        "\n"
                        "Example 2 (delta-mode with socket-counting):\n"
                        "        # ./cmd-metrics -d -s -c nginx -c cache-main -i 5\n"
                        "                      |nginx                                                                                                      |cache-main\n"
                        "        datetime      |procs          vsz  delta-vsz          rss  delta-rss   %%utime   %%stime socks dsock estab cl_wt listn  rest|procs          vsz  delta-vsz          rss  delta-rss   %%utime   %%stime socks dsock estab cl_wt listn  rest\n"
                        "        20210430105510|    9   5257912320          0   4277346304          0      6.0      1.6   606    21   491     0    27    88|    1   8768339968          0   4419424256     331776      8.4      4.0   297    14   290     2     1     4\n"
                        "        20210430105515|    9   5257912320          0   4277362688      16384      4.0      2.6   499  -107   383     0    27    89|    1   8768339968          0   4419411968     -12288      6.0      2.0   238   -59   233     0     1     4\n"
                        "        20210430105520|    9   5258059776     147456   4277399552      36864      4.4      1.2   526    27   411     0    27    88|    1   8768339968          0   4419600384     188416     13.6      0.0   249    11   245     0     1     3\n"
                        "        20210430105525|    9   5258059776          0   4277399552          0      4.6      2.8   543    17   427     0    27    89|    1   8768339968          0   4420235264     634880     10.8      1.0   259    10   255     0     1     3\n"

                        "\n"
                        "        Example 2 is equal to example 1, except for the extra option -s (include socket-counting).\n"
                        "        WARNING: Option -s requires root-permission to gain access to socket-information of other accounts.\n"
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
                        "        Example 4 lists the active processes with uid 992 (-u) and quits.\n"
                        "        In this mode it only lists processes that match the uid.\n"
                        "        Delta's are not included in the uid-mode.\n"
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
                        "        - Clock ticks per second:   %ld\n", cpu_cnt, physpages, physpages_avail, psize, physpages*psize/(1024*1024), ticks_per_sec);
}

void list_procs(char cmd[CMD_LIST_LEN][CMD_STRING_LEN], int cmd_cnt, LLNODE_PROCINFO *llnode_cur, size_t psize, bool first_iter, int ticks_per_sec) {
    int i;

    if (unlikely(first_iter)) {
        printf("%-32s%8s%8s%11s %-25s%14s%14s%10s%10s\n", "command", "pid", "ppid", "euid", "euser", "vsz" ,"rss", "utime", "stime");
    }
    if (cmd_cnt > 0) {
        for (i=0; i<cmd_cnt; i++) {
            if (strncmp(cmd[i], llnode_cur->proc_info.cmd, CMD_STRING_LEN) == 0) {
                printf("%-32s%8d%8d%11d %-25s%14ld%14ld%10.2f%10.2f\n", llnode_cur->proc_info.cmd,
		                                                        llnode_cur->proc_info.pid,
									llnode_cur->proc_info.ppid,
									llnode_cur->proc_info.euid,
									llnode_cur->proc_info.euser,
									llnode_cur->proc_info.vsz,
									llnode_cur->proc_info.rss,
								(float)(llnode_cur->proc_info.utime)/ticks_per_sec,
								(float)(llnode_cur->proc_info.stime)/ticks_per_sec);
            }
        }
    } else {
        printf("%-32s%8d%8d%11d %-25s%14ld%14ld%10.2f%10.2f\n", llnode_cur->proc_info.cmd,
	                                                        llnode_cur->proc_info.pid,
								llnode_cur->proc_info.ppid,
								llnode_cur->proc_info.euid,
								llnode_cur->proc_info.euser,
								llnode_cur->proc_info.vsz,
								llnode_cur->proc_info.rss,
							(float)(llnode_cur->proc_info.utime)/ticks_per_sec,
							(float)(llnode_cur->proc_info.stime)/ticks_per_sec);
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

void accumulate_cmd_metrics(char cmd[CMD_LIST_LEN][CMD_STRING_LEN], int cmd_cnt, LLNODE_PROCINFO *llnode_cur, size_t psize, CMD_METRICS *cmd_metrics) {
    int i;
    if (cmd_cnt > 0) {
        for (i=0; i<cmd_cnt; i++) {
            if (strncmp(cmd[i], llnode_cur->proc_info.cmd, CMD_STRING_LEN) == 0) {
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

void list_deltas(int cmd_cnt, CMD_METRICS *cmd_metrics, char *time_string, int ticks_per_sec, int loop_interval, int heading_interval, bool sock_include, bool first_iter, long *line_cnt) {
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
                if (sock_include) {
                    printf("|%-107s", cmd_metrics[i].cmd);
                } else {
                    printf("|%-71s", cmd_metrics[i].cmd);
                }
            }
	    printf("\n");
            // druk tweede heading-regel af (kolomnamen)
            printf("%-14s", "datetime");
            for (i=0; i<cmd_cnt; i++) {
                if (sock_include) {
                    printf("|%5s  %11s  %9s  %11s  %9s  %7s  %7s %5s %5s %5s %5s %5s %5s", "procs", "vsz", "delta-vsz", "rss", "delta-rss", "utime", "stime",
		                                                                           "socks", "dsock", "estab", "cl_wt", "listn", "rest");
                } else {
                    printf("|%5s  %11s  %9s  %11s  %9s  %7s  %7s", "procs", "vsz", "delta-vsz", "rss", "delta-rss", "utime", "stime");
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
                if (sock_include) {
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
            if (sock_include) {
                printf("|%5d  %11ld  %9ld  %11ld  %9ld  %7.2f  %7.2f %5ld %5ld %5ld %5ld %5ld %5ld",
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
                printf("|%5d  %11ld  %9ld  %11ld  %9ld  %7.2f  %7.2f",
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

void initialize_llnode_new(LLNODE_PROCINFO * llnode_new) {
    memset(llnode_new, 0, sizeof(LLNODE_PROCINFO));
    strncpy(llnode_new->proc_info.cmd, PIDS_VAL(pids_cmd,     str,     pids_stack_data, pids_info_data), CMD_STRING_LEN);
    llnode_new->proc_info.euid  = PIDS_VAL(pids_euid,         u_int,   pids_stack_data, pids_info_data);
    strncpy(llnode_new->proc_info.euser, PIDS_VAL(pids_euser, str,     pids_stack_data, pids_info_data), CMD_STRING_LEN);
    llnode_new->proc_info.pid   = PIDS_VAL(pids_pid,          s_int,   pids_stack_data, pids_info_data);
    llnode_new->proc_info.ppid  = PIDS_VAL(pids_ppid,         s_int,   pids_stack_data, pids_info_data);
    llnode_new->proc_info.vsz   = PIDS_VAL(pids_vsz,          ul_int,  pids_stack_data, pids_info_data);
    llnode_new->proc_info.rss   = PIDS_VAL(pids_rss,          ul_int,  pids_stack_data, pids_info_data);
    llnode_new->proc_info.utime = PIDS_VAL(pids_utime,        ull_int, pids_stack_data, pids_info_data);
    llnode_new->proc_info.stime = PIDS_VAL(pids_stime,        ull_int, pids_stack_data, pids_info_data);
    llnode_new->next = NULL;
}

void current_time(char *time_string) {
    time_t timestamp;
    struct tm* tm_info;

    timestamp = time(NULL);
    tm_info = localtime(&timestamp);

    strftime(time_string, 26, "%Y%m%d%H%M%S", tm_info);
}

int main(int argc, char **argv) {
    long psize           = sysconf(_SC_PAGESIZE);         // page size van het geheugen (verschilt van systeem tot systeem)
    long physpages       = sysconf(_SC_PHYS_PAGES);       // physical pages aantal
    long physpages_avail = sysconf(_SC_AVPHYS_PAGES);     // physical pages beschikbaar
    long cpu_cnt         = sysconf(_SC_NPROCESSORS_ONLN); // aantal actieve CPU's
    long ticks_per_sec   = sysconf(_SC_CLK_TCK);          // vraag de clock ticks/seconde op (verschilt van systeem tot systeem)
    char cmd[CMD_LIST_LEN][CMD_STRING_LEN];    // eventuele programmanaam waarop gefilterd moet worden
    uid_t uid[UID_LIST_LEN];                   // eventuele UID waaop gefilterd moet worden
    bool delta_mode = false;                   // start op in delta-mode yes/no
    bool sock_include = false;                 // doe ook TCP-socket tellingen yes/no
    bool first_iter = true, new_iter = true;
    int uid_cnt = 0;
    int cmd_cnt = 0;
    int loop_interval = 0;                     // meet-interval (in seconden)
    int heading_interval = -1;                 // interval (in lines) waarmee de heading moet worden afgedrukt
    long line_cnt = 0;
    int i;
    char time_string[TIME_STRING_LEN];
    int option;
    char *optstring = "c:dhi:r:su:";
    char *end_ptr;
    POOL *pool_ino;
    POOL **pool_ino_pp = &pool_ino;
    char *read_buf = NULL;
    long buflen = INITIAL_READBUF_SIZE;
    char command[CMD_STRING_LEN];

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
        case 's': sock_include = true;
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
        default:  print_syntax(ticks_per_sec, cpu_cnt, psize, physpages, physpages_avail);
                  exit(EXIT_FAILURE);
        }
        option = getopt(argc, argv, optstring);
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
    if (sock_include)
        pool_ino = pool_create(POOL_SIZE_INO);

    // Definieer pointers naar de nodes voor de procps linked list.
    LLNODE_PROCINFO *llnode_start = NULL,
                    *llnode_prv = NULL,
                    *llnode_cur = NULL,
		    *llnode_new = NULL;

LOOP_THIS_BABY_FOREVER:

    // Bouw een linked list op met records uit de process table.
    llnode_start = NULL;
    new_iter = true;
    if (procps_pids_new(&pids_info_data, pids_items, number_of_items) < 0) {
                fprintf(stderr, "ERROR - procps_pids_new failed\n");
                exit(EXIT_FAILURE);
    }
    while ((pids_stack_data = procps_pids_get(pids_info_data, PIDS_FETCH_THREADS_TOO))) {
        if (cmd_cnt > 0) {
	    // Voeg alleen nodes toe voor de opgegeven commando's
	    strncpy(command, PIDS_VAL(pids_cmd, str, pids_stack_data, pids_info_data), CMD_STRING_LEN);
            for (i=0; i<cmd_cnt; i++) {
                if (strncmp(cmd[i], command, CMD_STRING_LEN) == 0) {
                    llnode_new = (struct procinfo_node *)malloc(sizeof(LLNODE_PROCINFO));
                    initialize_llnode_new(llnode_new);
                    if (llnode_start == NULL) {
                        // Dit is de eerste iteratie, special case...
                        llnode_start = llnode_new;
                        llnode_cur   = llnode_new;
                    } else {
                        // Alle volgende nodes
                        llnode_cur->next = llnode_new;
                        llnode_cur       = llnode_new;
                    }
		}
            }
        } else {
	    // Voeg nodes toe voor ALLE procinfo records
            llnode_new = (struct procinfo_node *)malloc(sizeof(LLNODE_PROCINFO));
            initialize_llnode_new(llnode_new);
            if (llnode_start == NULL) {
                 // Dit is de eerste iteratie, special case...
                 llnode_start = llnode_new;
                 llnode_cur   = llnode_new;
             } else {
                 // Alle volgende nodes
                 llnode_cur->next = llnode_new;
                 llnode_cur       = llnode_new;
             }
	}
    }
    procps_pids_unref(&pids_info_data);

    // Initialiseer de cmd_metrics records en verzamel de socket-metrics.
    if (delta_mode) {
        initialize_metrics(cmd_cnt, cmd_metrics);
        if (sock_include) {
            accumulate_sock_metrics(cmd, pool_ino_pp, cmd_cnt, cmd_metrics, &read_buf, &buflen);   // socket-metrics
        }
    }

    // Doorloop de procps linked list (llnode) en verzamel de cmd-metrics.
    // Hier is een verschil tussen delta-mode=true en delta-mode=false;
    //   - delta-mode=false: de gegevens worden binnen de loop direct afgedrukt
    //   - delta-mode=true:  de gegevens worden binnen de loop alleen verzameld (in cmd_metrics)
    llnode_cur = llnode_start;
    while (llnode_cur != NULL) {
        if (delta_mode) {
	    accumulate_cmd_metrics(cmd, cmd_cnt, llnode_cur, psize, cmd_metrics);                  // cmd-metrics
	} else {
            list_procs(cmd, cmd_cnt, llnode_cur, psize, first_iter, ticks_per_sec);
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
        list_deltas(cmd_cnt, cmd_metrics, time_string, ticks_per_sec, loop_interval, heading_interval, sock_include, first_iter, &line_cnt);
        if (unlikely(first_iter)) {
            first_iter = false;
        }
//    } else {
//        if (loop_interval) {
//	    printf("\n");
//	}
    }

    // dealloceer de linked list en alle psproc-records waarnaar hij verwijst
    llnode_cur = llnode_start;
    while (llnode_cur != NULL) {
        llnode_prv = llnode_cur;
//        freeproc(llnode_cur->proc_info);
	llnode_cur = llnode_cur->next;
	free(llnode_prv);
    }

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
            // GOTO's are not always harmful...
            goto LOOP_THIS_BABY_FOREVER;
        }
    }

    exit(EXIT_SUCCESS);
}
