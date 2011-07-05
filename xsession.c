/*
 * xsession - nodm X session
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

#include "xsession.h"
#include "xsession-child.h"
#include "xserver.h"
#include "log.h"
#include "common.h"
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/*
 * Truncate ~/.xsession-errors if it is longer than \a maxsize.
 *
 * The function looks for .xsession-errors in the current directory, so when it
 * is called the current directory must be the user's homedir.
 *
 * The function also assumes that we are running as the user. As a consequence
 * it does not worry about symlink attacks, because they would only be possible
 * if the user's home directory is group or world writable.
 *
 * curdirname is the name of the current directory, and it is only used when
 * logging error messages.
 *
 * The function returns true on success, false on failure.
 */
static int cleanup_xse(off_t maxsize, const char* curdirname)
{
    int ret = E_OS_ERROR;
    int xse_fd = -1;
    struct stat xse_st;

    xse_fd = open(".xsession-errors", O_WRONLY | O_CREAT, 0600);
    if (xse_fd < 0)
    {
        log_err("cannot open `%s/%s': %m", curdirname, ".xsession-errors");
        goto cleanup;
    }
    if (fstat(xse_fd, &xse_st) < 0)
    {
        log_err("cannot stat `%s/%s': %m", curdirname, ".xsession-errors");
        goto cleanup;
    }
    if (xse_st.st_size > maxsize)
    {
        if (ftruncate(xse_fd, 0) < 0)
        {
            log_err("cannot truncate `%s/%s': %m", curdirname, ".xsession-errors");
            goto cleanup;
        }
    }

    /* If we made it so far, we succeeded */
    ret = E_SUCCESS;

cleanup:
    if (xse_fd >= 0)
        close(xse_fd);
    return ret;
}

int nodm_xsession_init(struct nodm_xsession* s)
{
    s->conf_use_pam = true;
    s->conf_cleanup_xse = true;

    // Get the user we should run the session for
    if (!bounded_strcpy(s->conf_run_as, getenv_with_default("NODM_USER", "root")))
        log_warn("username has been truncated");

    if (!bounded_strcpy(s->conf_session_command, getenv_with_default("NODM_XSESSION", "/etc/X11/Xsession")))
        log_warn("session command has been truncated");

    s->pid = -1;

    return E_SUCCESS;
}

int nodm_xsession_start(struct nodm_xsession* s, struct nodm_xserver* srv)
{
    struct nodm_xsession_child child;

    // Validate the user using the normal system user database
    struct passwd *pw = 0;
    if (!(pw = getpwnam(s->conf_run_as))) {
        log_err("Unknown username: %s", s->conf_run_as);
        return E_OS_ERROR;
    }
    child.pwent = *pw;

    // Create the argument list
    const char* args[5];
    args[0] = "/bin/sh";
    args[1] = "-l";
    args[2] = "-c";
    args[3] = s->conf_session_command;
    args[4] = NULL;
    child.argv = args;

    // Variables that gdm sets but we do not:
    //
    // This is something that we should see how to handle.
    // What I know so far is:
    //  - It should point to ~/.Xauthority, which should exist.
    //  - 'xauth generate' should be able to create it if missing, but I
    //    have not tested it yet.
    // g_setenv ("XAUTHORITY", d->userauth, TRUE);
    //
    // This is 'gnome', 'kde' and so on, and should probably be set by the
    // X session script:
    // g_setenv ("DESKTOP_SESSION", session, TRUE);
    //
    // This looks gdm specific:
    // g_setenv ("GDMSESSION", session, TRUE);

    // Variables that gdm sets but we delegate other tools to set:
    //
    // This is set by the pam_getenvlist loop above
    // g_setenv ("XDG_SESSION_COOKIE", ck_session_cookie, TRUE);
    //
    // This is set by "sh -l" from /etc/profile
    // if (pwent->pw_uid == 0)
    //   g_setenv ("PATH", gdm_daemon_config_get_value_string (GDM_KEY_ROOT_PATH), TRUE);
    // else
    //   g_setenv ("PATH", gdm_daemon_config_get_value_string (GDM_KEY_PATH), TRUE);
    //

    s->pid = fork();
    if (s->pid == 0)
    {
        // Setup environment
        setenv("HOME", pw->pw_dir, 1);
        setenv("USER", pw->pw_name, 1);
        setenv("USERNAME", pw->pw_name, 1);
        setenv("LOGNAME", pw->pw_name, 1);
        setenv("PWD", pw->pw_dir, 1);
        setenv("SHELL", pw->pw_shell, 1);
        setenv("DISPLAY", srv->name, 1);
        setenv("WINDOWPATH", srv->windowpath, 1);


        // Clear the NODM_* environment variables
        unsetenv("NODM_USER");
        unsetenv("NODM_XINIT");
        unsetenv("NODM_XSESSION");
        unsetenv("NODM_X_OPTIONS");
        unsetenv("NODM_MIN_SESSION_TIME");
        unsetenv("NODM_RUN_SESSION");

        // Move to home directory
        if (chdir(pw->pw_dir) == 0)
        {
            // Truncate ~/.xsession-errors
            if (s->conf_cleanup_xse)
                cleanup_xse(0, pw->pw_dir);
        }

        // child shell */
        if (s->conf_use_pam)
            exit(nodm_xsession_child_pam(&child));
        else
            exit(nodm_xsession_child(&child));
    } else if (s->pid == -1) {
        log_err("cannot fork user shell: %m");
        return E_OS_ERROR;
    }

    return E_SUCCESS;
}

int nodm_xsession_stop(struct nodm_xsession* s)
{
    kill(s->pid, SIGTERM);
    kill(s->pid, SIGCONT);
    // TODO: wait
    s->pid = -1;
    return E_SUCCESS;
}
