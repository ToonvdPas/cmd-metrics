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

#define _POSIX_SOURCE 1
#define CMD_LIST_LEN 10
#define CMD_STRING_LEN 64+1
#define TIME_STRING_LEN 16
#define USER_STRING_LEN 64+1
#define UID_LIST_LEN 10
#define PID_LIST_LEN 10
#define POOL_SIZE_INO 65536*2*2
//#define PANIC(text) (panic(text, __FILE__, __FUNCTION__, __LINE__))

#define LIST_PROCS_HEADER_FMT_STR_NO_THREADS   "%-40s%8s%8s%11s %-25s%14s%14s%10s%10s\n"
#define LIST_PROCS_HEADER_PR_ARGS_NO_THREADS   "command", "pid", "ppid", "euid", "euser", "vsz" ,"rss", "utime", "stime"

#define LIST_PROCS_HEADER_FMT_STR_WITH_THREADS "%-40s%8s%8s%8s%6s%11s %-25s%14s%14s%10s%10s\n"
#define LIST_PROCS_HEADER_PR_ARGS_WITH_THREADS "command", "pid", "ppid", "tgid", "ptype", "euid", "euser", "vsz" ,"rss", "utime", "stime"

#define LIST_PROCS_FMT_STR_NO_THREADS "%-40s%8d%8d%11d %-25s%14ld%14ld%10.2f%10.2f\n"
#define LIST_PROCS_PR_ARGS_NO_THREADS llnode_cur->proc_info.cmd, \
                                      llnode_cur->proc_info.pid, \
                                      llnode_cur->proc_info.ppid, \
                                      llnode_cur->proc_info.euid, \
                                      llnode_cur->proc_info.euser, \
                                      llnode_cur->proc_info.vsz, \
                                      llnode_cur->proc_info.rss, \
                              (float)(llnode_cur->proc_info.utime)/ticks_per_sec, \
                              (float)(llnode_cur->proc_info.stime)/ticks_per_sec

#define LIST_PROCS_FMT_STR_WITH_THREADS "%-40s%8d%8d%8d%6s%11d %-25s%14ld%14ld%10.2f%10.2f\n"
#define LIST_PROCS_PR_ARGS_WITH_THREADS llnode_cur->proc_info.cmd, \
                                      llnode_cur->proc_info.pid, \
                                      llnode_cur->proc_info.ppid, \
                                      llnode_cur->proc_info.tgid, \
				      llnode_cur->proc_info.tgid == llnode_cur->proc_info.pid ? "PROC" : "LWP", \
                                      llnode_cur->proc_info.euid, \
                                      llnode_cur->proc_info.euser, \
                                      llnode_cur->proc_info.vsz, \
                                      llnode_cur->proc_info.rss, \
                              (float)(llnode_cur->proc_info.utime)/ticks_per_sec, \
                              (float)(llnode_cur->proc_info.stime)/ticks_per_sec

#ifndef likely
    #define likely(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
    #define unlikely(x) __builtin_expect(!!(x), 0)
#endif

typedef struct procinfo_node {
    struct proc_info {
        char cmd[CMD_STRING_LEN];
	unsigned int euid;
	char euser[USER_STRING_LEN];
	int  pid;
	int  ppid;
	int  tgid;
	unsigned long vsz;
	unsigned long rss;
	unsigned long long utime;
	unsigned long long stime;
    } proc_info;
    struct procinfo_node *next;
} LLNODE_PROCINFO;


typedef struct cmd_metrics {
    char cmd[CMD_STRING_LEN];
    int process_cnt;
    struct metric_prev {
        long vsz;
        long rss;
        unsigned long long utime;
        unsigned long long stime;
        sock_aggr_t sock;         // substruct met de socket-stats (gedefinieerd in inode-stats.h)
    } metric_prev;
    struct metric_curr {
        long vsz;
        long rss;
        unsigned long long utime;
        unsigned long long stime;
        sock_aggr_t sock;         // substruct met de socket-stats (gedefinieerd in inode-stats.h)
    } metric_curr;
} CMD_METRICS;

typedef enum {false, true} bool;
bool shouldStop         = false;  // flag wordt op true gezet door de SIGTERM- en SIGINT-handlers
bool shouldReopenStdout = false;  // flag wordt op true gezet door de SIGHUP_handler
