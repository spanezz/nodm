/*
 * sstart - X server startup functions
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

#ifndef NODM_SSTART_H
#define NODM_SSTART_H

#include <sys/types.h>

#define SSTART_SUCCESS 0            ///< Server is ready for connections
#define SSTART_ERROR_PROGRAMMING 2  ///< Programming error
#define SSTART_ERROR_SYSTEM 3       ///< Unexpected OS error
#define SSTART_ERROR_SERVER_DIED 4  ///< Server died
#define SSTART_ERROR_TIMEOUT 5      ///< Server not ready before timeout


struct server
{
    const char **argv;
    pid_t pid;
};

int start_server(struct server* srv, unsigned timeout_sec);

#endif
