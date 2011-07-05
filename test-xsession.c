/*
 * test-xsession - test that we are able to run a X session
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
#include "common.h"
#include "xsession.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    struct log_config cfg = {
        .program_name = "test-xsession",
        .log_to_syslog = false,
        .log_to_stderr = true,
        .info_to_stderr = true,
    };
    log_start(&cfg);

    //const char* xcmdline = "/usr/bin/Xnest :1";
    setenv("NODM_SESSION", "/bin/true", 1);
    setenv("NODM_USER", getenv_with_default("USER", "root"), 1);

    struct nodm_xsession s;
    nodm_xsession_init(&s);

    int res = E_SUCCESS;

    //res = nodm_session_parse_cmdline(&s, xcmdline);
    //if (res != E_SUCCESS) goto cleanup;

    //res = nodm_x_with_session(&s);
    //fprintf(stderr, "nodm_x_with_session_cmdline returned %d\n", res);
    if (res != E_SUCCESS) goto cleanup;

cleanup:
    log_end();
    return res;
}
