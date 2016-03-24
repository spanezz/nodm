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
    fprintf(out, " --verbose      verbose outpout or logging\n");
    fprintf(out, " --quiet        only log warnings and errors\n");
    fprintf(out, " --nested       run a nested X server, does not require root.\n");
    fprintf(out, "                The server defaults to \"/usr/bin/Xnest :1\",\n");
    fprintf(out, "                override with NODM_X_OPTIONS\n");
    fprintf(out, " --[no-]syslog  enable/disable logging to syslog\n");
    fprintf(out, " --[no-]stderr  enable/disable logging to stderr\n");
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
    static int opt_verbose = 0;
    static int opt_quiet = 0;
    static int opt_nested = 0;
    static int opt_log_syslog = -1; // -1 for 'default'
    static int opt_log_stderr = -1; // -1 for 'default'
    static struct option options[] =
    {
        /* These options set a flag. */
        {"help",    no_argument,       &opt_help, 1},
        {"version", no_argument,       &opt_version, 1},
        {"verbose", no_argument,       &opt_verbose, 1},
        {"quiet",   no_argument,       &opt_quiet, 1},
        {"nested",  no_argument,       &opt_nested, 1},
        {"syslog",  no_argument,       &opt_log_syslog, 1},
        {"stderr",  no_argument,       &opt_log_stderr, 1},
        {"no-syslog", no_argument,     &opt_log_syslog, 0},
        {"no-stderr", no_argument,     &opt_log_stderr, 0},
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
    if (!opt_nested && getuid() != 0)
    {
        fprintf(stderr, "%s: can only be run by root\n", nodm_basename(argv[0]));
        return E_NOPERM;
    }

    // Setup logging
    struct log_config cfg = {
        .program_name = nodm_basename(argv[0]),
    };
    if (opt_quiet)
        cfg.log_level = NODM_LL_WARN;
    else if (opt_verbose)
        cfg.log_level = NODM_LL_VERB;
    else
        cfg.log_level = NODM_LL_INFO;
    if (opt_nested)
    {
        if (opt_log_syslog == -1) opt_log_syslog = 0;
        if (opt_log_stderr == -1) opt_log_stderr = 1;
    } else {
        if (opt_log_syslog == -1) opt_log_syslog = 1;
        if (opt_log_stderr == -1) opt_log_stderr = 0;
    }
    cfg.log_to_syslog = opt_log_syslog ? true : false;
    cfg.log_to_stderr = opt_log_stderr ? true : false;
    log_start(&cfg);

    log_info("starting nodm");

    // Setup the display manager
    struct nodm_display_manager dm;
    nodm_display_manager_init(&dm);

    // Choose the default X server
    const char* default_x_server = opt_nested ? "/usr/bin/Xnest :1" : "";

    // Parse X server command line
    int res = nodm_display_manager_parse_xcmdline(&dm,
            getenv_with_default("NODM_X_OPTIONS", default_x_server));
    if (res != E_SUCCESS) goto cleanup;

    if (opt_nested)
    {
        // For nested servers, disable PAM, user change, ~/.xsession-error
        // cleanup and VT allocation
        dm.session.conf_use_pam = false;
        dm.session.conf_cleanup_xse = false;
        dm.session.conf_run_as[0] = 0;
        dm.vt.conf_initial_vt = -1;
    }

    // Start the first session
    res = nodm_display_manager_start(&dm);
    if (res != E_SUCCESS) goto cleanup;

    // Enter the wait/restart loop
    res = nodm_display_manager_wait_restart_loop(&dm);
    if (res != E_SUCCESS) goto cleanup;

cleanup:
    nodm_display_manager_cleanup(&dm);
    log_end();
    return res;
}
