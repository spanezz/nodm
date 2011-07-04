/*
 * server - X server startup functions
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

#define NODM_SERVER_SUCCESS 0            ///< Server is ready for connections
#define NODM_SERVER_ERROR_PROGRAMMING 2  ///< Programming error
#define NODM_SERVER_ERROR_SYSTEM 3       ///< Unexpected OS error
#define NODM_SERVER_ERROR_SERVER_DIED 4  ///< Server died
#define NODM_SERVER_ERROR_TIMEOUT 5      ///< Server not ready before timeout
#define NODM_SERVER_ERROR_CONNECT 6      ///< Could not connect to X server
#define NODM_SERVER_ERROR_XLIB 7         ///< Xlib error


struct server
{
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
 * Initialise a struct server with NULL values
 */
void server_init(struct server* srv);

/**
 * Start the X server and wait until it's ready to accept connections.
 *
 * @param srv
 *   The struct server with X server information. argv and name are expected to
 *   be filled, pid is filled.
 * @param timeout_sec
 *   Timeout in seconds after which if the X server is not ready, we give up
 *   and return an error.
 * @return
 *   Exit status as described by the NODM_SERVER_* constants
 */
int server_start(struct server* srv, unsigned timeout_sec);

/// Kill the X server
int server_stop(struct server* srv);

/**
 * Connect to the X server
 */
int server_connect(struct server* srv);

int server_disconnect(struct server* srv);

int server_read_window_path(struct server* srv);

#endif
