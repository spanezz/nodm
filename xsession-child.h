/*
 * xsession-child - child side of X session
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

#ifndef NODM_XSESSION_CHILD_H
#define NODM_XSESSION_CHILD_H

#include <stdbool.h>
#include <pwd.h>
#include <security/pam_appl.h>

struct nodm_xserver;

struct nodm_xsession_child
{
    /// X server we connect to
    struct nodm_xserver* srv;

    /// If set to true, perform ~/.xsession-errors cleanup
    bool conf_cleanup_xse;

    /// Information about the user we run the session for
    struct passwd pwent;

    /// PAM session handle (or NULL if not used)
    pam_handle_t *pamh;

    /// Return code of the last PAM function called
    int pam_status;

    /// Command line to run
    const char** argv;

    /// Child exit status
    int exit_status;
};

/// Setup common environment bits in the child process
int nodm_xsession_child_common_env(struct nodm_xsession_child* s);

/// Just exec the session
int nodm_xsession_child(struct nodm_xsession_child* s);

/// Run a child process inside a PAM session
int nodm_xsession_child_pam(struct nodm_xsession_child* s);

#endif
