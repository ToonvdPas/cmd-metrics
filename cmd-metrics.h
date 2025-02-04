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
