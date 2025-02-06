# cmd-metrics

Utility monitoring the collective resource usage of all processes running with a specific executable (cmd) file over time, with the intention to detect memory leaks.  Each time interval it will aggregate the resources of all instances of the specified executable(s), as well as the delta's compared to the previous sample.

# Compile
```
$ make
```
# Clean up
```
$ make clean
```
# Install
`cmd-metrics` requires two two sources of information:

* the process-statistics, and
* the socket-information

The socket-part requires read-access to files in /proc/PID/fd<br>
However, one needs special privileges to read the files in that directory.<br>
In order to run `cmd-metrics` as a normal user, you therefore need to assign special capabilities to the executable.<br>
You can do this as follows:
```
# setcap CAP_DAC_READ_SEARCH,CAP_SYS_PTRACE=ep cmd-metrics
```
This should be secure, al long as you make sure the program is owned by root and only writable by root, like this:
```
# chown root: cmd-metrics
# chmod 0755  cmd-metrics
```
# Help
```
# ./cmd-metrics -h
syntax: $ cmd-metrics -d -c <cmd> [-c <cmd> ...] [-i <interval (s)>] [-r <repeat-header>] [-s]
    or: $ cmd-metrics -u <uid> [-u <uid> ...]     (list processes running with a specific numeric uid)
    or: $ cmd-metrics -c <cmd> [-c <cmd> ...]     (list processes running with a specific command)
    or: $ cmd-metrics -u <uid> -a [-c <cmd> ...]  (list processes running with a specific uid AND command)
    or: $ cmd-metrics [-t]                        (full list of processes, optionally including LWP's (threads))
    or: $ cmd-metrics -h         (this help text)

Arguments:
<<<<<<< HEAD
        -a                 Switch the filter options (-c and -u) to AND mode.  (default mode is OR)
=======
        -a                 Switch the filter options (-c and -u) to AND mode.  (default mode is OR
>>>>>>> 8e995a1 (Updated and improved the help text.)
                           In the AND mode, proc-records are only listed if both cmd and uid match.
        -c <command>       Program name (executable) to filter on.
                           Multiple -c arguments are allowed.
                           When specifying only part of a command, it will match all processes
                           whose command name start with that string.
        -d                 Delta-mode.  In this mode the program calculates the allocation and
                           release of resources.  Those resources are VSZ, RSS, and (optionally) sockets.
        -h                 This help text.
        -i <interval>      Interval in seconds.
        -s                 Provide some information on socket use.
                           This option is only available in delta mode.
        -r <repeat-header> Interval for printing the header line. (only has effect in delta-mode)
                           -1: only print heading at start of run
                            0: don't print heading at all
                           >0: heading-interval in number of lines
        -t                 Include threads (Light Weight Processes, LWP) in the listing.
                           This option does not work in delta mode.
        -u <uid>           Numeric userid to filter on.  Multiple -u arguments are allowed.

<<<<<<< HEAD
This program collects information on the usage of the resources VSZ, RSS, and (optionally) sockets.
Per program, it adds up the metrics of all running instances and shows the totals.
Originally, this was the reason for writing the tool: aggregation of the resource usage
of all processes which are running from the very same program (executable file).
This is called the 'delta-mode' of the tool, which is activated by the argument -d.
=======
This program collects information on the usage of the resources VSZ, RSSi, and (optional) sockets.
Per program, it adds up the metrics of all running instances and shows the totals.
Originally, this was the reason for writing the tool: aggregation of the resource usage
of all processes which are running from the very same program (executable file).
It is called the 'delta-mode' of the tool, which is activated by the argument -d.
>>>>>>> 8e995a1 (Updated and improved the help text.)
Primary purpose of this mode is to detect probable memory leaks.
See examples 1 and 2.

Note: rss (resident segment size) and vsz (virtual segment size) are in KiB units.
      utime (user time) and stime (system time) are in seconds since the starti of the processes.

Example 1 (delta-mode without socket-counting):
        # ./cmd-metrics -d -c nginx -c cache-main -i 5
                      |nginx                                                                  |cache-main
        datetime      |procs          vsz  delta-vsz          rss  delta-rss    utime    stime|procs          vsz  delta-vsz          rss  delta-rss    utime    stime
        20210430113405|    9   5258100736          0   4277514240          0      0.0      0.0|    1   8768339968          0   4414558208          0      0.0      0.0
        20210430113410|    9   5258100736          0   4277583872      69632      5.4      3.2|    1   8768339968          0   4410761216   -3796992     18.6      0.0
        20210430113415|    9   5258100736          0   4277583872          0      5.6      1.2|    1   8768339968          0   4412370944    1609728     14.4      3.4

        Example 1 gathers metrics for two programs; nginx, and cache-main (=Varnish).
        Every interval a single line of metrics is printed.
        This mode (the delta-mode) is selected by the use of argument -d.
        We also specified a 5-second interval, and printing the headers after every 20 cycles.
        In delta-mode, the program will run forever, until interrupted for instance by ^C.
        Delta-mode without the option -s can safely be used on heavily loaded systems with many active processes.

Example 2 (delta-mode with socket-counting):
        # ./cmd-metrics -d -s -c nginx -c cache-main -i 5
                      |nginx                                                                                                      |cache-main
        datetime      |procs          vsz  delta-vsz          rss  delta-rss    utime    stime socks dsock estab cl_wt listn  rest|procs          vsz  delta-vsz          rss  delta-rss    utime    stime socks dsock estab cl_wt listn  rest
        20210430105510|    9   5257912320          0   4277346304          0      6.0      1.6   606    21   491     0    27    88|    1   8768339968          0   4419424256     331776      8.4      4.0   297    14   290     2     1     4
        20210430105515|    9   5257912320          0   4277362688      16384      4.0      2.6   499  -107   383     0    27    89|    1   8768339968          0   4419411968     -12288      6.0      2.0   238   -59   233     0     1     4
        20210430105520|    9   5258059776     147456   4277399552      36864      4.4      1.2   526    27   411     0    27    88|    1   8768339968          0   4419600384     188416     13.6      0.0   249    11   245     0     1     3
        20210430105525|    9   5258059776          0   4277399552          0      4.6      2.8   543    17   427     0    27    89|    1   8768339968          0   4420235264     634880     10.8      1.0   259    10   255     0     1     3

        Example 2 is equal to example 1, except for the extra option -s (include socket-counting).
        WARNING: Option -s requires special privileges to gain access to socket-information.
                 Please consult the README file for instructions on how to install the cmd-metrics object file.

Example 3 (process listing-mode):
        $ cmd-metrics
        command          pid   ppid  euid euser           vsz        rss     utime    stime
        systemd            1      0     0 root       57032704    8163328     28621    89512
        kthreadd           2      0     0 root              0          0         0      346
        kworker/0:0H       4      2     0 root              0          0         0        0
        ksoftirqd/0        6      2     0 root              0          0         0      973
        .
        .
        .
        Example 3 lists all active processes on the system and quits.

Example 4 (process listing-mode for specific uid's):
        $ ./cmd-metrics -u 992 -u 994
        command          pid   ppid  euid euser           vsz        rss     utime    stime
        nginx          34365  91602   994 nginx     583729152  218914816    105968    95669
        nginx          34366  91602   994 nginx     584949760  229904384    185921   130785
        nginx          34367  91602   994 nginx     597446656  263946240   1998303   944788
        nginx          34368  91602   994 nginx     584265728  223789056    134254   107540
        nginx          34369  91602   994 nginx     589692928  253509632    934723   471431
        nginx          34370  91602   994 nginx     585297920  235864064    289201   178765
        nginx          34371  91602   994 nginx     608522240  275410944   2661996  1238526
        nginx          34372  91602   994 nginx     587210752  244871168    490474   270729
        varnishd       80609      1   992 varnish    43372544    5591040      1145     2064
        cache-main     80620  80609   992 varnish  6243368960 3189968896  10885847  3166217

        Example 4 lists the active processes with uid's 992 and 994.
        Please note that Varnish apparently runs from TWO different object files.
        For a complete picture, in delta mode both object files should be specified in delta mode.
        So to monitor the processes in the above listing, use this command:.
        $ cmd-metrics -d -c nginx -c varnishd -c cache-main -i 5

Signal handling:
        SIGHUP:   Reopen stdout (for logfile-rotation)
        SIGINT:   Flush stdout and terminate.
        SIGTERM:  Flush stdout and terminate.
<<<<<<< HEAD
=======

The OS-kernel on this system reports the following parameters:
        - Number of active CPU's:   16
        - Memory phys pages:        16231969
        - Memory phys pages avail:  1870808
        - Memory page size (bytes): 4096
        - Memory capacity (MB):     63406
        - Clock ticks per second:   100
>>>>>>> 8e995a1 (Updated and improved the help text.)
```
# To Do
* Reduce the number of calls to malloc() by making use of the memory-pool for the allocation of memory for the procps linked list.
* Add an option to generate CSV-formatted output.