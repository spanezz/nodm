/*
 * test-internals - test nodm internals and helper functions
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
#include "dm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ensure_equals(const char* a, const char* b)
{
    if (a == NULL && b == NULL)
        return;
    if (a == NULL || b == NULL || strcmp(a, b) != 0)
    {
        log_warn("strings differ: \"%s\" != \"%s\"", a, b);
        exit(1);
    }
}

int main(int argc, char* argv[])
{
    struct log_config cfg = {
        .program_name = "test-internals",
        .log_to_syslog = false,
        .log_to_stderr = true,
        .info_to_stderr = true,
    };
    log_start(&cfg);

    // Test getenv_with_default
    setenv("FOO", "foo", 1);
    ensure_equals(getenv_with_default("FOO", "bar"), "foo");
    unsetenv("FOO");
    ensure_equals(getenv_with_default("FOO", "bar"), "bar");

    struct nodm_display_manager s;

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "");
    ensure_equals(s.srv.argv[0], "/usr/bin/X");
    ensure_equals(s.srv.argv[1], ":0");
    ensure_equals(s.srv.argv[2], NULL);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/X");
    ensure_equals(s.srv.argv[1], ":0");
    ensure_equals(s.srv.argv[2], "foo");
    ensure_equals(s.srv.argv[3], NULL);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":0");
    ensure_equals(s.srv.argv[2], NULL);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, ":1");
    ensure_equals(s.srv.argv[0], "/usr/bin/X");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], NULL);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest :1");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], NULL);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":0");
    ensure_equals(s.srv.argv[2], "foo");
    ensure_equals(s.srv.argv[3], NULL);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, ":1 foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/X");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], "foo");
    ensure_equals(s.srv.argv[3], NULL);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest :1 foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], "foo");
    ensure_equals(s.srv.argv[3], NULL);
    nodm_display_manager_cleanup(&s);

    log_end();
    return 0;
}
