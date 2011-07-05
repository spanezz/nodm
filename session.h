/*
 * session - nodm X session
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

#include "server.h"
#include <stdbool.h>
#include <pwd.h>
#include <security/pam_appl.h>

struct session
{
    /// If set to true, do a PAM-aware session
    bool conf_use_pam;

    /// If set to true, perform ~/.xsession-errors cleanup
    bool conf_cleanup_xse;

    /**
     * Username to use for the X session.
     *
     * Empty string means 'do not change user'
     */
    char conf_run_as[128];

    /// Command to run as the X session
    char conf_session_command[1024];

    /// X server information
    struct server srv;

    /// Information about the user we run the session for
    struct passwd pwent;

    /// PAM session handle (or NULL if not used)
    pam_handle_t *pamh;

    /// Return code of the last PAM function called
    int pam_status;

    /// Storage for split server arguments used by nodm_x_cmdline_split
    void* srv_split_args;
};

/// Initialise a session structure with default values
void nodm_session_init(struct session* s);

/// Cleanup at the end of a session
void nodm_session_cleanup(struct session* s);

/**
 * nodm X session
 *
 * Perform PAM bookkeeping, init the session environment and start the X
 * session requested by the user
 */
int nodm_session(struct session* s);

/**
 * Start the X server using the given command line, change user to $NODM_USER
 * and run $NODM_XSESSION
 */
int nodm_x_with_session(struct session* s);

/**
 * Split xcmdline using wordexp shell-like expansion and set s->srv.argv.
 *
 * If the first token starts with '/' or '.', it is used as the X server, else
 * "X" is used as the server.
 *
 * If the second token (or the first if the first was not recognised as a path
 * to the X server) looks like ":<NUMBER>", it is used as the display name,
 * else ":0" is used.
 */
int nodm_session_parse_cmdline(struct session* s, const char* xcmdline);

#endif
