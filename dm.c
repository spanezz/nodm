/*
 * dm - nodm X display manager
 *
 * Copyright 2011  Enrico Zini <enrico@enricozini.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "dm.h"
#include "common.h"
#include "log.h"
#include <wordexp.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>


void nodm_display_manager_init(struct nodm_display_manager* dm)
{
    nodm_xserver_init(&dm->srv);
    nodm_xsession_init(&dm->session);
    nodm_vt_init(&dm->vt);
    dm->conf_minimum_session_time = atoi(getenv_with_default("NODM_MIN_SESSION_TIME", "60"));
    dm->_srv_split_args = NULL;
    dm->_srv_split_argv = NULL;
}

void nodm_display_manager_cleanup(struct nodm_display_manager* dm)
{
    nodm_vt_stop(&dm->vt);

    // Deallocate parsed arguments, if used
    if (dm->_srv_split_args)
    {
        wordexp_t* we = (wordexp_t*)dm->_srv_split_args;
        wordfree(we);
        free(we);
        dm->_srv_split_args = NULL;
    }

    if (dm->_srv_split_argv)
    {
        free(dm->_srv_split_argv);
        dm->_srv_split_argv = NULL;
    }
}

int nodm_display_manager_start(struct nodm_display_manager* dm)
{
    int res = nodm_vt_start(&dm->vt);
    if (res != E_SUCCESS) return res;

    if (dm->vt.num != -1)
    {
        // Create the vtN argument
        snprintf(dm->_vtarg, sizeof(dm->_vtarg), "vt%d", dm->vt.num);
        // Append it to srv args
        const char** s = dm->srv.argv;
        while (*s) ++s;
        *s++ = dm->_vtarg;
        *s = NULL;
    }

    dm->last_session_start = time(NULL);

    res = nodm_display_manager_restart(dm);
    if (res != E_SUCCESS) return res;

    return E_SUCCESS;
}

int nodm_display_manager_restart(struct nodm_display_manager* dm)
{
    int res = nodm_xserver_start(&dm->srv);
    if (res != E_SUCCESS) return res;

    res = nodm_xsession_start(&dm->session, &dm->srv);
    if (res != E_SUCCESS) return res;

    return E_SUCCESS;
}

int nodm_display_manager_stop(struct nodm_display_manager* dm)
{
    int res = nodm_xsession_stop(&dm->session);
    if (res != E_SUCCESS) return res;

    res = nodm_xserver_stop(&dm->srv);
    if (res != E_SUCCESS) return res;

    return E_SUCCESS;
}

int nodm_display_manager_wait(struct nodm_display_manager* dm, int* session_status)
{
    *session_status = -1;
    while (true)
    {
        // Wait for one child to exit
        int status;
        pid_t child = waitpid(-1, &status, 0);
        if (child == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                log_warn("waitpid error: %m");
                return E_OS_ERROR;
            }
        }

        if (child == dm->srv.pid)
        {
            // Server died
            log_warn("X server died with status %d", status);
            return E_X_SERVER_DIED;
        } else if (child == dm->session.pid) {
            // Session died
            log_warn("X session died with status %d", status);
            *session_status = status;
            return E_SESSION_DIED;
        }
    }
}

int nodm_display_manager_parse_xcmdline(struct nodm_display_manager* s, const char* xcmdline)
{
    int return_code = E_SUCCESS;

    // tokenize xoptions
    wordexp_t* toks = (wordexp_t*)calloc(1, sizeof(wordexp_t));
    switch (wordexp(xcmdline, toks, WRDE_NOCMD))
    {
        case 0: break;
        case WRDE_NOSPACE:
            return_code = E_OS_ERROR;
            goto cleanup;
        default:
            toks->we_wordv = NULL;
            return_code = E_BAD_ARG;
            goto cleanup;
    }

    unsigned in_arg = 0;
    unsigned argc = 0;
    // +1 for the X server pathname, +1 for the display name,
    // +1 for the VT number, +1 for the trailing NULL
    char **argv = (char**)malloc((toks->we_wordc + 4) * sizeof(char*));
    if (argv == NULL)
    {
        return_code = E_OS_ERROR;
        goto cleanup;
    }

    // Server command
    if (in_arg < toks->we_wordc &&
           (toks->we_wordv[in_arg][0] == '/' || toks->we_wordv[in_arg][0] == '.'))
        argv[argc++] = toks->we_wordv[in_arg++];
    else
        argv[argc++] = "/usr/bin/X";

    // Server name
    if (in_arg < toks->we_wordc &&
           toks->we_wordv[in_arg][0] == ':' && isdigit(toks->we_wordv[in_arg][1]))
    {
        argv[argc] = toks->we_wordv[in_arg++];
        s->srv.name = argv[argc];
        ++argc;
    }
    else
    {
        argv[argc] = ":0";
        s->srv.name = argv[argc];
        ++argc;
    }

    // Copy other args
    while (in_arg < toks->we_wordc)
    {
        int vtn;
        if (sscanf(toks->we_wordv[in_arg], "vt%d", &vtn) == 1)
            // if vtN has been provided by the caller, disable VT allocation
            s->vt.conf_initial_vt = -1;
        argv[argc++] = toks->we_wordv[in_arg++];
    }
    argv[argc] = NULL;

    s->srv.argv = (const char**)argv;
    s->_srv_split_argv = argv;
    s->_srv_split_args = toks;
    argv = NULL;
    toks = NULL;

cleanup:
    if (toks != NULL)
    {
        if (toks->we_wordv)
            wordfree(toks);
        free(toks);
    }
    if (argv != NULL)
        free(argv);

    return return_code;
}

void nodm_display_manager_dump_status(struct nodm_display_manager* dm)
{
    nodm_xserver_dump_status(&dm->srv);
    nodm_xsession_dump_status(&dm->session);
}

int nodm_display_manager_wait_restart_loop(struct nodm_display_manager* dm)
{
    static int retry_times[] = { 0, 0, 30, 30, 60, 60, -1 };
    int restart_count = 0;
    int res;

    while (1)
    {
        int sstatus;
        res = nodm_display_manager_wait(dm, &sstatus);
        time_t end = time(NULL);
        nodm_display_manager_stop(dm);

        switch (res)
        {
            case E_X_SERVER_DIED:
                break;
            case E_SESSION_DIED:
                log_info("X session quit with status %d", sstatus);
                break;
            default:
                return res;
        }

        /* Check if the session was too short */
        if (end - dm->last_session_start < dm->conf_minimum_session_time)
        {
            if (retry_times[restart_count+1] != -1)
                ++restart_count;
        }
        else
            restart_count = 0;

        /* Sleep a bit if the session was too short */
        if (retry_times[restart_count] > 0)
        {
            log_warn("session lasted less than %d seconds: sleeping %d seconds before restarting it",
                    dm->conf_minimum_session_time, retry_times[restart_count]);
            struct timespec tosleep = { .tv_sec = retry_times[restart_count], .tv_nsec = 0 };
            struct timespec remaining;
            while (true)
            {
                int r = nanosleep(&tosleep, &remaining);
                if (r != -1)
                    break;
                else if (errno == EINTR)
                {
                    tosleep = remaining;
                }
                else
                {
                    log_warn("sleep aborted: %m (ignoring error");
                    break;
                }
            }
        }

        log_info("restarting session");
        res = nodm_display_manager_restart(dm);
        if (res != E_SUCCESS) return res;
    }
}
