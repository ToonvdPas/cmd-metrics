    // libproc2 related variables (see libproc2/pids.h)
    enum pids_item pids_items[] =  {PIDS_CMD,
                                    PIDS_ID_EUID,
                                    PIDS_ID_EUSER,
                                    PIDS_ID_PID, 
                                    PIDS_ID_PPID,
                                    PIDS_MEM_VIRT,
                                    PIDS_MEM_RES,
                                    PIDS_TICS_USER,
                                    PIDS_TICS_SYSTEM};
     enum rel_items {pids_cmd,
                     pids_euid,
                     pids_euser,
                     pids_pid,
                     pids_ppid,
                     pids_vsz,
                     pids_rss,
                     pids_utime,
                     pids_stime};
     int number_of_items = sizeof(pids_items)/sizeof(pids_items[0]);
     struct pids_info *pids_info_data = NULL;
     struct pids_stack *pids_stack_data = NULL;
