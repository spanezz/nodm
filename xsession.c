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
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int nodm_xsession_init(struct nodm_xsession* s)
{
    s->child_body = NULL;
    s->conf_use_pam = true;
    s->conf_cleanup_xse = true;

    if (sigemptyset(&s->orig_signal_mask) == -1)
        log_err("sigemptyset error: %m");

    // Get the user we should run the session for
    if (!bounded_strcpy(s->conf_run_as, getenv_with_default("NODM_USER", "root")))
        log_warn("username has been truncated");

    // Get the X session command
    if (!bounded_strcpy(s->conf_session_command, getenv_with_default("NODM_XSESSION", "/etc/X11/Xsession")))
        log_warn("session command has been truncated");

    s->pid = -1;

    return E_SUCCESS;
}

int nodm_xsession_start(struct nodm_xsession* s, struct nodm_xserver* srv)
{
    struct nodm_xsession_child child;
    child.srv = srv;
    child.conf_cleanup_xse = s->conf_cleanup_xse;

    // Validate the user using the normal system user database
    struct passwd *pw = 0;
    if (s->conf_run_as[0] == 0)
        pw = getpwuid(getuid());
    else
        pw = getpwnam(s->conf_run_as);
    if (pw == NULL)
    {
        log_err("unknown username: %s", s->conf_run_as);
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

    log_verb("starting X session \"%s\"", s->conf_session_command);

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
        // Restore the original signal mask
        if (sigprocmask(SIG_SETMASK, &s->orig_signal_mask, NULL) == -1)
            log_err("sigprocmask failed: %m");
        // cargogulted from xinit
        setpgid(0, getpid());

        // child shell */
        if (s->child_body)
            exit(s->child_body(&child));
        else if (s->conf_use_pam)
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
    int res = child_must_exit(s->pid, "X session");
    s->pid = -1;
    return res;
}

void nodm_xsession_dump_status(struct nodm_xsession* s)
{
    fprintf(stderr, "xsession command: %s\n", s->conf_session_command);
    fprintf(stderr, "xsession user: %s\n", s->conf_run_as);
    fprintf(stderr, "xsession use PAM: %s\n", s->conf_use_pam ? "yes" : "no");
    fprintf(stderr, "xsession cleanup ~/.xsession-errors: %s\n", s->conf_cleanup_xse ? "yes" : "no");
    fprintf(stderr, "xsession pid: %d\n", (int)s->pid);
    fprintf(stderr, "xsession body overridden by test: %s\n", (s->child_body != NULL) ? "yes" : "no");
}

void nodm_xsession_report_exit(struct nodm_xsession* s, int status)
{
    if (WIFEXITED(status))
        log_warn("X session %d quit with status %d",
               (int)s->pid, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        log_warn("X session %d was killed with signal %d",
               (int)s->pid, WTERMSIG(status));
    else
        log_warn("X session %d terminated with unknown status %d",
               (int)s->pid, status);
}
