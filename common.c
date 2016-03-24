/*
 * common - common nodm definitions and utility functions
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

#include "common.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>


const char* nodm_basename (const char* str)
{
    const char *cp = strrchr (str, '/');
    return cp ? cp + 1 : str;
}

const char* getenv_with_default(const char* envname, const char* def)
{
    const char* res = getenv(envname);
    if (res != NULL)
        return res;
    else
        return def;
}

const char* nodm_strerror(int code)
{
    switch (code)
    {
        case E_SUCCESS:            return "success";
        case E_NOPERM:             return "permission denied";
        case E_USAGE:              return "invalid command syntax";
        case E_BAD_ARG:            return "invalid argument to option";
        case E_PASSWD_NOTFOUND:    return "not found password file";
        case E_SHADOW_NOTFOUND:    return "not found shadow password file";
        case E_GROUP_NOTFOUND:     return "not found group file";
        case E_GSHADOW_NOTFOUND:   return "not found shadow group file";
        case E_CMD_NOEXEC:         return "can't run command/shell";
        case E_CMD_NOTFOUND:       return "can't find command/shell to run";
        case E_PROGRAMMING:        return "programming error";
        case E_PAM_ERROR:          return "something wrong talking with PAM";
        case E_OS_ERROR:           return "something wrong talking with the Operating System";
        case E_XLIB_ERROR:         return "Xlib error";
        case E_X_SERVER_DIED:      return "server died";
        case E_X_SERVER_TIMEOUT:   return "server not ready before timeout";
        case E_X_SERVER_CONNECT:   return "could not connect to X server";
        case E_SESSION_DIED:       return "X session died";
        case E_USER_QUIT:          return "quit requested";
        default: return "unknown error";
    }
}

int child_has_quit(pid_t pid, int* quit, int* status)
{
    pid_t res = waitpid(pid, status, WNOHANG);
    if (res == -1)
    {
        if (errno == ECHILD)
            *quit = 2;
        else
        {
            log_err("error checking status of child process %d: %m", (int)pid);
            return E_OS_ERROR;
        }
    } else if (res == 0)
        *quit = 0;
    else
        *quit = 1;
    return E_SUCCESS;
}

int child_must_exit(pid_t pid, const char* procdesc)
{
    int res = E_SUCCESS;
    if (pid > 0)
    {
        // Check what is the child status
        int quit, status;
        res = child_has_quit(pid, &quit, &status);
        if (res != E_SUCCESS) return res;
        switch (quit)
        {
            case 0:
                // still running, we must kill it
                log_info("sending %s %d the TERM signal", procdesc, (int)pid);
                kill(pid, SIGTERM);
                kill(pid, SIGCONT);
                while (true)
                {
                    int status;
                    if (waitpid(pid, &status, 0) == -1)
                    {
                        if (errno == EINTR)
                            continue;
                        if (errno != ECHILD)
                            return E_OS_ERROR;
                    }
                    break;
                }
                break;
            case 1:
                // has just quit
                log_info("%s %d quit with status %d", procdesc, (int)pid, status);
                break;
            case 2:
                // Was not there
                break;
        }
    }
    return E_SUCCESS;
}
