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
