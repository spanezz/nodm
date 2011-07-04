/*
 * test-xstart - test that we are able to start X
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

#include "log.h"
#include "server.h"
#include <stdio.h>

int main(int argc, char* argv[])
{
    struct log_config cfg = {
        .program_name = "test-xstart",
        .log_to_syslog = false,
        .log_to_stderr = true,
        .info_to_stderr = true,
    };
    log_start(&cfg);

    struct server srv;
    server_init(&srv);

    const char* server_argv[] = { "/usr/bin/Xnest", ":1", NULL };
    srv.argv = server_argv;
    srv.name = ":1";

    int res = server_start(&srv, 5);
    if (res != NODM_SERVER_SUCCESS)
    {
        fprintf(stderr, "server_start return code: %d\n", res);
        return 1;
    }

    res = server_connect(&srv);
    if (res != NODM_SERVER_SUCCESS)
    {
        fprintf(stderr, "server_connect return code: %d\n", res);
        return 2;
    }

    res = server_disconnect(&srv);
    if (res != NODM_SERVER_SUCCESS)
    {
        fprintf(stderr, "server_disconnect return code: %d\n", res);
        return 3;
    }

    log_end();
    return 0;
}
