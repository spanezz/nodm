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
};

/**
 * Initialise a struct nodm_xserver with NULL values
 */
void nodm_xserver_init(struct nodm_xserver* srv);

/// Start the X server
int nodm_xserver_start(struct nodm_xserver* srv);

/// Stop the X server
int nodm_xserver_stop(struct nodm_xserver* srv);

#endif
