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

#ifndef NODM_DEFS_H
#define NODM_DEFS_H

#include <sys/types.h>

// Exit codes used by shadow programs
#define E_SUCCESS             0     ///< success
#define E_NOPERM              1     ///< permission denied
#define E_USAGE               2     ///< invalid command syntax
#define E_BAD_ARG             3     ///< invalid argument to option
#define E_PASSWD_NOTFOUND     14    ///< not found password file
#define E_SHADOW_NOTFOUND     15    ///< not found shadow password file
#define E_GROUP_NOTFOUND      16    ///< not found group file
#define E_GSHADOW_NOTFOUND    17    ///< not found shadow group file
#define E_CMD_NOEXEC          126   ///< can't run command/shell
#define E_CMD_NOTFOUND        127   ///< can't find command/shell to run

// Other nodm-specific exit codes
#define E_PROGRAMMING         200   ///< Programming error
#define E_PAM_ERROR           201   ///< something wrong talking with PAM
#define E_OS_ERROR            202   ///< something wrong talking with the Operating System
#define E_XLIB_ERROR          203   ///< Xlib error
#define E_VT_ALLOC_ERROR      204   ///< VT allocation error
#define E_X_SERVER_DIED       210   ///< Server died
#define E_X_SERVER_TIMEOUT    211   ///< Server not ready before timeout
#define E_X_SERVER_CONNECT    212   ///< Could not connect to X server
#define E_SESSION_DIED        220   ///< X session died
#define E_USER_QUIT           221   ///< Quit requested

/// Return the basename of a path, as a pointer inside \a str
const char* nodm_basename (const char* str);

/**
 * Like getenv, but if the variable is not defined it returns \a def
 */
const char* getenv_with_default(const char* envname, const char* def);

/**
 * Like strcpy but:
 *
 *  * it works only for sized character arrays (it expects sizeof on them)
 *  * it always null-terminates the destination string
 *  * it returns false if the string was truncated, else true
 */
#define bounded_strcpy(dst, src) (snprintf(dst, sizeof(dst), "%s", (src)) < sizeof(dst))

/// Return the string description of an exit code
const char* nodm_strerror(int code);

/**
 * Check if a child has died.
 *
 * @param pid
 *   process ID to check
 * @retval quit
 *   0 if it is still running
 *   1 if it has just quit and we got its exit status
 *   2 if the pid does not exist
 * @retval status
 *   the exit status if it has quit
 */
int child_has_quit(pid_t pid, int* quit, int* status);

/**
 * Kill a child process if it still running and wait for it to end
 *
 * @param pid
 *   The child pid
 * @param procdesc
 *   Child process description to use in the logs
 */
int child_must_exit(pid_t pid, const char* procdesc);

#endif
