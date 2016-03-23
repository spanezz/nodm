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

#ifndef NODM_DISPLAY_MANAGER_H
#define NODM_DISPLAY_MANAGER_H

#include "xserver.h"
#include "xsession.h"
#include "vt.h"
#include <time.h>
#include <signal.h>

struct nodm_display_manager
{
    /// X server supervision
    struct nodm_xserver srv;

    /// X session supervision
    struct nodm_xsession session;

    /// VT allocation
    struct nodm_vt vt;

    /**
     * The minimum time (in seconds) that a session should last to be
     * considered successful
     */
    int conf_minimum_session_time;

    /// Time the last session started
    time_t last_session_start;

    /// Original signal mask at program startup
    sigset_t orig_signal_mask;

    /// Storage for split server arguments used by nodm_x_cmdline_split
    char** _srv_split_argv;
    void* _srv_split_args;

    /// Storage for vtN argument from dynamic VT allocation
    char _vtarg[10];
};

/// Initialise a display_manager structure with default values
void nodm_display_manager_init(struct nodm_display_manager* dm);

/// Cleanup at the end of the display manager
void nodm_display_manager_cleanup(struct nodm_display_manager* dm);

/**
 * Start X and the X session
 *
 * This function sets the signal mask to block all signals. The original signal
 * mask is restored by nodm_display_manager_cleanup().
 */
int nodm_display_manager_start(struct nodm_display_manager* dm);

/// Restart X and the X session after they died
int nodm_display_manager_restart(struct nodm_display_manager* dm);

/// Wait for X or the X session to end
int nodm_display_manager_wait(struct nodm_display_manager* dm, int* session_status);

/// Stop X and the X session
int nodm_display_manager_stop(struct nodm_display_manager* dm);

/**
 * nodm wait/restart loop.
 *
 * Wait for the X server or session to terminate and restart them.
 *
 * If the session was very short-lived, it wants for an incremental amount of
 * time before restarting it.
 */
int nodm_display_manager_wait_restart_loop(struct nodm_display_manager* dm);

/**
 * Split xcmdline using wordexp shell-like expansion and set dm->srv.argv.
 *
 * If the first token starts with '/' or '.', it is used as the X server, else
 * "X" is used as the server.
 *
 * If the second token (or the first if the first was not recognised as a path
 * to the X server) looks like ":<NUMBER>", it is used as the display name,
 * else ":0" is used.
 */
int nodm_display_manager_parse_xcmdline(struct nodm_display_manager* dm, const char* xcmdline);

/// Dump all internal status to stderr
void nodm_display_manager_dump_status(struct nodm_display_manager* dm);

#endif
