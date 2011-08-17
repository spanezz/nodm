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
#include <string.h>


void nodm_display_manager_init(struct nodm_display_manager* dm)
{
    nodm_xserver_init(&dm->srv);
    nodm_xsession_init(&dm->session);
    nodm_vt_init(&dm->vt);
    dm->conf_minimum_session_time = atoi(getenv_with_default("NODM_MIN_SESSION_TIME", "60"));
    dm->_srv_split_args = NULL;
    dm->_srv_split_argv = NULL;

    // Save original signal mask
    if (sigprocmask(SIG_BLOCK, NULL, &dm->orig_signal_mask) == -1)
        log_err("sigprocmask error: %m");
    dm->srv.orig_signal_mask = dm->orig_signal_mask;
    dm->session.orig_signal_mask = dm->orig_signal_mask;
}

void nodm_display_manager_cleanup(struct nodm_display_manager* dm)
{
    // Restore original signal mask
    if (sigprocmask(SIG_SETMASK, &dm->orig_signal_mask, NULL) == -1)
        log_err("sigprocmask error: %m");

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
        log_verb("allocated VT %d", dm->vt.num);
    } else
        log_verb("skipped VT allocation");

    // Block all signals
    sigset_t blockmask;
    if (sigfillset(&blockmask) == -1)
    {
        log_err("sigfillset error: %m");
        return E_PROGRAMMING;
    }
    if (sigprocmask(SIG_BLOCK, &blockmask, NULL) == -1)
    {
        log_err("sigprocmask error: %m");
        return E_PROGRAMMING;
    }

    res = nodm_display_manager_restart(dm);
    if (res != E_SUCCESS) return res;

    return E_SUCCESS;
}

int nodm_display_manager_restart(struct nodm_display_manager* dm)
{
    dm->last_session_start = time(NULL);

    int res = nodm_xserver_start(&dm->srv);
    if (res != E_SUCCESS) return res;
    log_verb("X server is ready for connections");

    res = nodm_xsession_start(&dm->session, &dm->srv);
    if (res != E_SUCCESS) return res;
    log_verb("X session has started");

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

// Signal handler for wait loop
static int quit_signal_caught = 0;
static void catch_signals (int sig)
{
    ++quit_signal_caught;
}

static int setup_quit_notification(sigset_t* origset)
{
    /* Reset caught signal flag */
    quit_signal_caught = 0;

    struct sigaction action;
    action.sa_handler = catch_signals;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;

    sigset_t ourset;
    if (sigemptyset(&ourset)
        || sigaddset(&ourset, SIGTERM)
        || sigaddset(&ourset, SIGINT)
        || sigaddset(&ourset, SIGQUIT)
        || sigaction(SIGTERM, &action, NULL)
        || sigaction(SIGINT, &action, NULL)
        || sigaction(SIGQUIT, &action, NULL)
        || sigprocmask(SIG_UNBLOCK, &ourset, origset)
        ) {
        log_err("signal operations error: %m");
        return E_PROGRAMMING;
    }
    return E_SUCCESS;
}

static void shutdown_quit_notification(const sigset_t* origset)
{
    if (sigprocmask(SIG_SETMASK, origset, NULL) == -1)
        log_err("sigprocmask error: %m");
}

int nodm_display_manager_wait(struct nodm_display_manager* dm, int* session_status)
{
    int res = E_SUCCESS;

    // Catch the normal termination signals using 'catch_signals'
    sigset_t origset;
    res = setup_quit_notification(&origset);
    if (res != E_SUCCESS) return res;

    *session_status = -1;
    while (true)
    {
        // Wait for one child to exit
        int status;
        pid_t child = waitpid(-1, &status, 0);
        if (child == -1)
        {
            if (errno == EINTR)
            {
                if (quit_signal_caught)
                {
                    log_info("shutdown signal received");
                    res = E_USER_QUIT;
                    goto cleanup;
                }
                else
                    continue;
            }
            else
            {
                log_warn("waitpid error: %m");
                res = E_OS_ERROR;
                goto cleanup;
            }
        }

        if (child == dm->srv.pid)
        {
            // Server died
            nodm_xserver_report_exit(&dm->srv, status);
            res = E_X_SERVER_DIED;
            goto cleanup;
        } else if (child == dm->session.pid) {
            // Session died
            nodm_xsession_report_exit(&dm->session, status);
            *session_status = status;
            res = E_SESSION_DIED;
            goto cleanup;
        }
    }

cleanup:
    shutdown_quit_notification(&origset);
    return res;
}

int nodm_display_manager_parse_xcmdline(struct nodm_display_manager* s, const char* xcmdline)
{
    int return_code = E_SUCCESS;
    char **argv = NULL;

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
    argv =(char**)malloc((toks->we_wordc + 4) * sizeof(char*));
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

static int interruptible_sleep(int seconds)
{
    int res = E_SUCCESS;

    // Catch the normal termination signals using 'catch_signals'
    sigset_t origset;
    res = setup_quit_notification(&origset);
    if (res != E_SUCCESS) return res;

    struct timespec tosleep = { .tv_sec = seconds, .tv_nsec = 0 };
    struct timespec remaining;
    while (true)
    {
        int r = nanosleep(&tosleep, &remaining);
        if (r != -1)
            break;
        else if (errno == EINTR)
        {
            if (quit_signal_caught)
            {
                res = E_USER_QUIT;
                break;
            } else
                tosleep = remaining;
        }
        else
        {
            log_warn("sleep aborted: %m (ignoring error");
            break;
        }
    }

    shutdown_quit_notification(&origset);
    return res;
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
            res = interruptible_sleep(retry_times[restart_count]);
            if (res != E_SUCCESS) return res;
        }

        log_info("restarting session");
        res = nodm_display_manager_restart(dm);
        if (res != E_SUCCESS) return res;
    }
}
