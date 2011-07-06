/*
 * session - nodm X display manager
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


void nodm_display_manager_init(struct nodm_display_manager* dm)
{
    nodm_xserver_init(&(dm->srv));
    nodm_xsession_init(&(dm->session));
    dm->_srv_split_args = NULL;
}

void nodm_display_manager_cleanup(struct nodm_display_manager* dm)
{
    // Deallocate parsed arguments, if used
    if (dm->_srv_split_args)
    {
        wordexp_t* we = (wordexp_t*)dm->_srv_split_args;
        wordfree(we);
        free(we);
        dm->_srv_split_args = NULL;
    }
}

int nodm_display_manager_start(struct nodm_display_manager* dm)
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

int nodm_display_manager_wait(struct nodm_display_manager* dm)
{
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
            return status;
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
    char **argv = (char**)malloc((toks->we_wordc + 3) * sizeof(char*));
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
        argv[argc++] = toks->we_wordv[in_arg++];
    argv[argc] = NULL;

    s->srv.argv = (const char**)argv;
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
