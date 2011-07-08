/*
 * xserver - X server startup functions
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

#ifndef NODM_SERVER_H
#define NODM_SERVER_H

#include <sys/types.h>
#include <X11/Xlib.h>

/// Supervise an X server
struct nodm_xserver
{
    /// Timeout (in seconds) to use waiting for X to start
    int conf_timeout;

    /// X server command line
    const char **argv;
    /// X display name
    const char *name;
    /// X window path (dynamically allocated and owned by this structure)
    char *windowpath;
    /// X server pid
    pid_t pid;
    /// xlib Display connected to the server
    Display *dpy;
    /// Original signal mask at program startup
    sigset_t orig_signal_mask;
};

/**
 * Initialise a struct nodm_xserver with NULL values
 */
void nodm_xserver_init(struct nodm_xserver* srv);

/**
 * Start the X server and wait until it is ready to accept connections.
 *
 * @param srv
 *   The struct nodm_xserver with X server information. argv and name are expected to
 *   be filled, pid is filled.
 * @return
 *   Exit status as described by the E_* constants
 */
int nodm_xserver_start(struct nodm_xserver* srv);

/// Stop the X server
int nodm_xserver_stop(struct nodm_xserver* srv);

/// Dump all internal status to stderr
void nodm_xserver_dump_status(struct nodm_xserver* srv);

/**
 * Connect to the X server
 *
 * Uses srv->name, sets srv->dpy.
 *
 * @return
 *   Exit status as described by the E_* constants
 */
int nodm_xserver_connect(struct nodm_xserver* srv);

/**
 * Close connection to the X server
 *
 * Uses srv->dpy, sets it to NULL.
 *
 * @return
 *   Exit status as described by the E_* constants
 */
int nodm_xserver_disconnect(struct nodm_xserver* srv);

/**
 * Get the WINDOWPATH value for the server
 *
 * Uses srv->dpy, sets srv->windowpath
 *
 * @return
 *   Exit status as described by the E_* constants
 */
int nodm_xserver_read_window_path(struct nodm_xserver* srv);

/// Report that the X session has quit
void nodm_xserver_report_exit(struct nodm_xserver* s, int status);

#endif
