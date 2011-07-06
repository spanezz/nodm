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
#include "dm.h"
#include "xserver.h"
#include "xsession-child.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

int test_session(struct nodm_xsession_child* s)
{
    return E_SUCCESS;
}

int test_session_bad(struct nodm_xsession_child* s)
{
    return E_USAGE;
}

int test_session_x_killer(struct nodm_xsession_child* s)
{
    if (s->srv->pid == -1)
    {
        fprintf(stderr, "server PID has not been set\n");
        return E_BAD_ARG;
    }
    kill(s->srv->pid, SIGTERM);
    sleep(10);
    return E_SUCCESS;
}

// X server starts, X session quits with success
void test_trivial_session()
{
    struct nodm_display_manager dm;
    nodm_display_manager_init(&dm);
    ensure_succeeds(nodm_display_manager_parse_xcmdline(&dm, "/usr/bin/Xnest :1 -geometry 1x1+0+0"));
    dm.session.conf_use_pam = false;
    dm.session.conf_cleanup_xse = false;
    dm.session.conf_run_as[0] = 0;
    dm.session.child_body = test_session;
    ensure_succeeds(nodm_display_manager_start(&dm));
    ensure_succeeds(nodm_display_manager_wait(&dm));
    ensure_succeeds(nodm_display_manager_stop(&dm));
    nodm_display_manager_cleanup(&dm);
}

// X server does not start
void test_bad_x_server()
{
    struct nodm_display_manager dm;
    nodm_display_manager_init(&dm);
    ensure_succeeds(nodm_display_manager_parse_xcmdline(&dm, "/bin/false :1 -geometry 1x1+0+0"));
    dm.session.conf_use_pam = false;
    dm.session.conf_cleanup_xse = false;
    dm.session.conf_run_as[0] = 0;
    dm.session.child_body = test_session;

    ensure_equali(nodm_display_manager_start(&dm), E_X_SERVER_DIED);

    // If xserver died before being ready for connections, it should reflect on
    // the tracked pid
    ensure_equali(dm.srv.pid, -1);

    // Session must not have been started
    ensure_equali(dm.session.pid, -1);

    ensure_succeeds(nodm_display_manager_stop(&dm));
    nodm_display_manager_cleanup(&dm);
}

// X server starts, X session quits with error
void test_failing_x_session()
{
    struct nodm_display_manager dm;
    nodm_display_manager_init(&dm);
    ensure_succeeds(nodm_display_manager_parse_xcmdline(&dm, "/usr/bin/Xnest :1 -geometry 1x1+0+0"));
    dm.session.conf_use_pam = false;
    dm.session.conf_cleanup_xse = false;
    dm.session.conf_run_as[0] = 0;
    dm.session.child_body = test_session_bad;

    ensure_succeeds(nodm_display_manager_start(&dm));
    ensure_succeeds(nodm_display_manager_wait(&dm));
    ensure_succeeds(nodm_display_manager_stop(&dm));
    nodm_display_manager_cleanup(&dm);
}

// X server starts, X session starts, then server dies
void test_dying_x_server()
{
    struct nodm_display_manager dm;
    nodm_display_manager_init(&dm);
    ensure_succeeds(nodm_display_manager_parse_xcmdline(&dm, "/usr/bin/Xnest :1 -geometry 1x1+0+0"));
    dm.session.conf_use_pam = false;
    dm.session.conf_cleanup_xse = false;
    dm.session.conf_run_as[0] = 0;
    dm.session.child_body = test_session_x_killer;

    ensure_succeeds(nodm_display_manager_start(&dm));
    ensure_succeeds(nodm_display_manager_wait(&dm));
    ensure_succeeds(nodm_display_manager_stop(&dm));
    nodm_display_manager_cleanup(&dm);
}

int main(int argc, char* argv[])
{
    test_start("test-xsession", false);

    test_trivial_session();
    test_bad_x_server();
    test_failing_x_session();
    test_dying_x_server();

    // TODO:
    //  - test a wrong username (error before starting X session)

    test_ok();
}
