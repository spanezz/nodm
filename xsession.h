/*
 * xsession - nodm X session
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

#ifndef NODM_SESSION_H
#define NODM_SESSION_H

#include <stdbool.h>
#include <sys/types.h>

struct nodm_xserver;
struct nodm_xsession_child;

/// Supervise an X session
struct nodm_xsession
{
    /// Command to run as the X session
    char conf_session_command[1024];

    /**
     * Username to use for the X session.
     *
     * Empty string means 'do not change user'
     */
    char conf_run_as[128];

    /// If true, wrap the session in a PAM session
    bool conf_use_pam;

    /// If set to true, perform ~/.xsession-errors cleanup
    bool conf_cleanup_xse;

    /// X session pid
    pid_t pid;

    /// If non-NULL, use as child process main body (used for tests)
    int (*child_body)(struct nodm_xsession_child* s);

    /// Original signal mask at program startup
    sigset_t orig_signal_mask;
};

/// Initialise a struct nodm_session with default values
int nodm_xsession_init(struct nodm_xsession* s);

/// Start the X session
int nodm_xsession_start(struct nodm_xsession* s, struct nodm_xserver* srv);

/// Stop the X session
int nodm_xsession_stop(struct nodm_xsession* s);

/// Dump all internal status to stderr
void nodm_xsession_dump_status(struct nodm_xsession* s);

/// Report that the X session has quit
void nodm_xsession_report_exit(struct nodm_xsession* s, int status);

#endif
