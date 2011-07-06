/*
 * nodm - nodm X display manager
 *
 * Copyright 2009--2011  Enrico Zini <enrico@enricozini.org>
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


#define NAME "nodm"
#ifndef NODM_SESSION
#define NODM_SESSION "/usr/sbin/nodm"
#endif
/* #define DEBUG_NODM */


#include "config.h"
#include "common.h"
#include "dm.h"
#include "log.h"
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void do_help(int argc, char** argv, FILE* out)
{
	fprintf(out, "Usage: %s [options]\n\n", argv[0]);
	fprintf(out, "Options:\n");
	fprintf(out, " --help         print this help message\n");
	fprintf(out, " --version      print %s's version number\n", NAME);
	fprintf(out, " --session=cmd  run cmd instead of %s\n", NODM_SESSION);
	fprintf(out, "                (use for testing)\n");
}


/*
 * nodm - start X with autologin to a given user
 *
 * First, X is started as root, and nodm itself is used as the session.
 *
 * When run as the session, nodm performs a proper login to a given user and
 * starts the X session.
 */
int main (int argc, char **argv)
{
    static int opt_help = 0;
    static int opt_version = 0;
    static struct option options[] =
    {
        /* These options set a flag. */
        {"help",    no_argument,       &opt_help, 1},
        {"version", no_argument,       &opt_version, 1},
        {0, 0, 0, 0}
    };

    // Parse command line options
    while (1)
    {
        int option_index = 0;
        int c = getopt_long(argc, argv, ":", options, &option_index);
        if (c == -1) break;
        switch (c)
        {
            case 0: break;
            default:
                fprintf(stderr, "Invalid command line option\n");
                do_help(argc, argv, stderr);
                return E_USAGE;
        }
    }
    if (opt_help)
    {
        do_help(argc, argv, stdout);
        return E_SUCCESS;
    }
    if (opt_version)
    {
        printf("%s version %s\n", NAME, VERSION);
        return E_SUCCESS;
    }

    // We only run if we are root
    if (getuid() != 0)
    {
        fprintf(stderr, "%s: can only be run by root\n", basename(argv[0]));
        return E_NOPERM;
    }

    // TODO: implement --verbose --no-syslog --no-stderr and the like
    struct log_config cfg = {
        .program_name = basename(argv[0]),
        .log_to_syslog = true,
        .log_to_stderr = false,
        .info_to_stderr = false,
        .verbose = false
    };
    log_start(&cfg);

    log_info("starting nodm");

    // Run the display manager
    struct nodm_display_manager dm;
    nodm_display_manager_init(&dm);

    int res = nodm_display_manager_parse_xcmdline(&dm, getenv_with_default("NODM_X_OPTIONS", ""));
    if (res != E_SUCCESS) goto cleanup;

    res = nodm_display_manager_start(&dm);
    if (res != E_SUCCESS) goto cleanup;

    res = nodm_display_manager_wait_restart_loop(&dm);
    if (res != E_SUCCESS) goto cleanup;

cleanup:
    nodm_display_manager_cleanup(&dm);
    log_end();
    return res;
}
