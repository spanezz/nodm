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
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[])
{
    test_start("test-internals", false);

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
    ensure_equals(s.srv.argv[2], "-nolisten");
    ensure_equals(s.srv.argv[3], "tcp");
    ensure_equals(s.srv.argv[4], NULL);
    ensure_equals(s.srv.name, ":0");
    ensure_equali(s.vt.conf_initial_vt, 7);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/X");
    ensure_equals(s.srv.argv[1], ":0");
    ensure_equals(s.srv.argv[2], "foo");
    ensure_equals(s.srv.argv[3], "-nolisten");
    ensure_equals(s.srv.argv[4], "tcp");
    ensure_equals(s.srv.argv[5], NULL);
    ensure_equals(s.srv.name, ":0");
    ensure_equali(s.vt.conf_initial_vt, 7);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":0");
    ensure_equals(s.srv.argv[2], "-nolisten");
    ensure_equals(s.srv.argv[3], "tcp");
    ensure_equals(s.srv.argv[4], NULL);
    ensure_equals(s.srv.name, ":0");
    ensure_equali(s.vt.conf_initial_vt, 7);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, ":1");
    ensure_equals(s.srv.argv[0], "/usr/bin/X");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], "-nolisten");
    ensure_equals(s.srv.argv[3], "tcp");
    ensure_equals(s.srv.argv[4], NULL);
    ensure_equals(s.srv.name, ":1");
    ensure_equali(s.vt.conf_initial_vt, 7);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest :1");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], "-nolisten");
    ensure_equals(s.srv.argv[3], "tcp");
    ensure_equals(s.srv.argv[4], NULL);
    ensure_equals(s.srv.name, ":1");
    ensure_equali(s.vt.conf_initial_vt, 7);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":0");
    ensure_equals(s.srv.argv[2], "foo");
    ensure_equals(s.srv.argv[3], "-nolisten");
    ensure_equals(s.srv.argv[4], "tcp");
    ensure_equals(s.srv.argv[5], NULL);
    ensure_equals(s.srv.name, ":0");
    ensure_equali(s.vt.conf_initial_vt, 7);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, ":1 foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/X");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], "foo");
    ensure_equals(s.srv.argv[3], "-nolisten");
    ensure_equals(s.srv.argv[4], "tcp");
    ensure_equals(s.srv.argv[5], NULL);
    ensure_equals(s.srv.name, ":1");
    ensure_equali(s.vt.conf_initial_vt, 7);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest :1 foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], "foo");
    ensure_equals(s.srv.argv[3], "-nolisten");
    ensure_equals(s.srv.argv[4], "tcp");
    ensure_equals(s.srv.argv[5], NULL);
    ensure_equals(s.srv.name, ":1");
    ensure_equali(s.vt.conf_initial_vt, 7);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "vt2");
    ensure_equals(s.srv.argv[0], "/usr/bin/X");
    ensure_equals(s.srv.argv[1], ":0");
    ensure_equals(s.srv.argv[2], "vt2");
    ensure_equals(s.srv.argv[3], "-nolisten");
    ensure_equals(s.srv.argv[4], "tcp");
    ensure_equals(s.srv.argv[5], NULL);
    ensure_equals(s.srv.name, ":0");
    ensure_equali(s.vt.conf_initial_vt, -1);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest :1 vt42 foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], "vt42");
    ensure_equals(s.srv.argv[3], "foo");
    ensure_equals(s.srv.argv[4], "-nolisten");
    ensure_equals(s.srv.argv[5], "tcp");
    ensure_equals(s.srv.argv[6], NULL);
    ensure_equals(s.srv.name, ":1");
    ensure_equali(s.vt.conf_initial_vt, -1);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest :1 vt42 -nolisten foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], "vt42");
    ensure_equals(s.srv.argv[3], "-nolisten");
    ensure_equals(s.srv.argv[4], "foo");
    ensure_equals(s.srv.argv[5], "-nolisten");
    ensure_equals(s.srv.argv[6], "tcp");
    ensure_equals(s.srv.argv[7], NULL);
    ensure_equals(s.srv.name, ":1");
    ensure_equali(s.vt.conf_initial_vt, -1);
    nodm_display_manager_cleanup(&s);

    nodm_display_manager_init(&s);
    nodm_display_manager_parse_xcmdline(&s, "/usr/bin/Xnest :1 vt42 -nolisten tcp foo");
    ensure_equals(s.srv.argv[0], "/usr/bin/Xnest");
    ensure_equals(s.srv.argv[1], ":1");
    ensure_equals(s.srv.argv[2], "vt42");
    ensure_equals(s.srv.argv[3], "-nolisten");
    ensure_equals(s.srv.argv[4], "tcp");
    ensure_equals(s.srv.argv[5], "foo");
    ensure_equals(s.srv.argv[6], NULL);
    ensure_equals(s.srv.name, ":1");
    ensure_equali(s.vt.conf_initial_vt, -1);
    nodm_display_manager_cleanup(&s);

    test_ok();
}
