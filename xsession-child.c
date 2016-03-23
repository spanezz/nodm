/*
 * xsession-child - child side of X session
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
/*
 * Some parts are taken from su(1) which is:
 * Copyright 1989 - 1994, Julianne Frances Haugh
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Julianne F. Haugh nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JULIE HAUGH AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL JULIE HAUGH OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * With some parts substantially derived from an ancestor of:
 * su for GNU.  Run a shell with substitute user and group IDs.
 * Copyright (C) 1992-2003 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "xsession-child.h"
#include "xserver.h"
#include "common.h"
#include "log.h"
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <errno.h>

/* compatibility with different versions of Linux-PAM */
#ifndef PAM_ESTABLISH_CRED
#define PAM_ESTABLISH_CRED PAM_CRED_ESTABLISH
#endif
#ifndef PAM_DELETE_CRED
#define PAM_DELETE_CRED PAM_CRED_DELETE
#endif

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

/*
 * setup_uid_gid() split in two functions for PAM support -
 * pam_setcred() needs to be called after initgroups(), but
 * before setuid().
 */
static int setup_groups(const struct passwd *info)
{
    /*
     * Set the real group ID to the primary group ID in the password
     * file.
     */
    if (setgid (info->pw_gid) == -1) {
        log_err("bad group ID `%d' for user `%s': %m\n", info->pw_gid, info->pw_name);
        return E_OS_ERROR;
    }

    /*
     * For systems which support multiple concurrent groups, go get
     * the group set from the /etc/group file.
     */
    if (initgroups (info->pw_name, info->pw_gid) == -1) {
        log_err("initgroups failed for user `%s': %m\n", info->pw_name);
        return E_OS_ERROR;
    }
    return E_SUCCESS;
}

static int change_uid (const struct passwd *info)
{
    /*
     * Set the real UID to the UID value in the password file.
     */
    if (setuid(info->pw_uid)) {
        log_err("bad user ID `%d' for user `%s': %m\n", (int)info->pw_uid, info->pw_name);
        return E_OS_ERROR;
    }
    return E_SUCCESS;
}

static int setup_pam(struct nodm_xsession_child* s)
{
    static struct pam_conv conv = {
        misc_conv,
        NULL
    };

    char *cp;
    const char *tty = 0;    // Name of tty SU is run from

    /* We only run if we are root */
    if (getuid() != 0)
    {
        log_err("can only be run by root");
        return E_NOPERM;
    }

    // Set up the nodm_xsession_child structure
    s->pamh = NULL;
    s->pam_status = PAM_SUCCESS;

    /*
     * Get the tty name. Entries will be logged indicating that the user
     * tried to change to the named new user from the current terminal.
     */
    if (isatty (0) && (cp = ttyname (0))) {
        if (strncmp (cp, "/dev/", 5) == 0)
            tty = cp + 5;
        else
            tty = cp;
    } else {
        tty = "???";
    }

    s->pam_status = pam_start("nodm", s->pwent.pw_name, &conv, &s->pamh);
    if (s->pam_status != PAM_SUCCESS) {
        log_err("pam_start: error %d", s->pam_status);
        return E_PAM_ERROR;
    }

    s->pam_status = pam_set_item(s->pamh, PAM_TTY, (const void *) tty);
    if (s->pam_status == PAM_SUCCESS)
        s->pam_status = pam_set_item(s->pamh, PAM_RUSER, (const void *) "root");
    if (s->pam_status == PAM_SUCCESS)
        s->pam_status = pam_set_item(s->pamh, PAM_XDISPLAY, (const void *)
          s->srv->name);
    if (s->pam_status != PAM_SUCCESS) {
        log_err("pam_set_item: %s", pam_strerror(s->pamh, s->pam_status));
        return E_PAM_ERROR;
    }

    signal (SIGINT, SIG_IGN);
    signal (SIGQUIT, SIG_IGN);

    /* FIXME: should we ignore this, or honour it?
     * this can fail if the current user's account is invalid. "This
     * includes checking for password and account expiration, as well as
     * verifying access hour restrictions."
     */
    s->pam_status = pam_acct_mgmt(s->pamh, 0);
    if (s->pam_status != PAM_SUCCESS)
        log_warn("%s (Ignored)", pam_strerror(s->pamh, s->pam_status));

    signal (SIGINT, SIG_DFL);
    signal (SIGQUIT, SIG_DFL);

    /* save SU information */
    log_info("Successful su on %s for %s by %s", tty, s->pwent.pw_name, "root");

    /* set primary group id and supplementary groups */
    if (setup_groups(&(s->pwent))) {
        s->pam_status = PAM_ABORT;
        return E_OS_ERROR;
    }

    /*
     * pam_setcred() may do things like resource limits, console groups,
     * and much more, depending on the configured modules
     */
    s->pam_status = pam_setcred(s->pamh, PAM_ESTABLISH_CRED);
    if (s->pam_status != PAM_SUCCESS) {
        log_err("pam_setcred: %s", pam_strerror(s->pamh, s->pam_status));
        return E_PAM_ERROR;
    }

    s->pam_status = pam_open_session(s->pamh, 0);
    if (s->pam_status != PAM_SUCCESS) {
        log_err("pam_open_session: %s", pam_strerror(s->pamh, s->pam_status));
        pam_setcred(s->pamh, PAM_DELETE_CRED);
        return E_PAM_ERROR;
    }

    /* update environment with all pam set variables */
    char **envcp = pam_getenvlist(s->pamh);
    if (envcp) {
        while (*envcp) {
            putenv(*envcp);
            envcp++;
        }
    }

    /* become the new user */
    if (change_uid(&(s->pwent)) != 0) {
        pam_close_session(s->pamh, 0);
        pam_setcred(s->pamh, PAM_DELETE_CRED);
        s->pam_status = PAM_ABORT;
        return E_OS_ERROR;
    }

    return E_SUCCESS;
}

static void shutdown_pam(struct nodm_xsession_child* s)
{
    if (s->pam_status == PAM_SUCCESS)
    {
        s->pam_status = pam_close_session(s->pamh, 0);
        if (s->pam_status != PAM_SUCCESS)
            log_err("pam_close_session: %s", pam_strerror(s->pamh, s->pam_status));
    }

    pam_end(s->pamh, s->pam_status);
    s->pamh = 0;
}

int nodm_xsession_child_common_env(struct nodm_xsession_child* s)
{
    int return_code = E_SUCCESS;

    // Setup environment
    setenv("HOME", s->pwent.pw_dir, 1);
    setenv("USER", s->pwent.pw_name, 1);
    setenv("USERNAME", s->pwent.pw_name, 1);
    setenv("LOGNAME", s->pwent.pw_name, 1);
    setenv("PWD", s->pwent.pw_dir, 1);
    setenv("SHELL", s->pwent.pw_shell, 1);
    setenv("DISPLAY", s->srv->name, 1);

    // Read the WINDOWPATH value from the X server
    return_code = nodm_xserver_connect(s->srv);
    if (return_code != E_SUCCESS) goto cleanup;
    return_code = nodm_xserver_read_window_path(s->srv);
    if (return_code != E_SUCCESS) goto cleanup;
    return_code = nodm_xserver_disconnect(s->srv);
    if (return_code != E_SUCCESS) goto cleanup;

    setenv("WINDOWPATH", s->srv->windowpath, 1);


    // Clear the NODM_* environment variables
    unsetenv("NODM_USER");
    unsetenv("NODM_XINIT");
    unsetenv("NODM_XSESSION");
    unsetenv("NODM_X_OPTIONS");
    unsetenv("NODM_MIN_SESSION_TIME");

    // Move to home directory
    if (chdir(s->pwent.pw_dir) == 0)
    {
        // Truncate ~/.xsession-errors
        if (s->conf_cleanup_xse)
            cleanup_xse(0, s->pwent.pw_dir);
    }

cleanup:
    if (s->srv->dpy != NULL)
        nodm_xserver_disconnect(s->srv);
    return return_code;
}

int nodm_xsession_child(struct nodm_xsession_child* s)
{
    int res = nodm_xsession_child_common_env(s);
    if (res != E_SUCCESS) return res;

    /*
     * This is a workaround for Linux libc bug/feature (?) - the
     * /dev/log file descriptor is open without the close-on-exec flag
     * and used to be passed to the new shell. There is "fcntl(LogFile,
     * F_SETFD, 1)" in libc/misc/syslog.c, but it is commented out (at
     * least in 5.4.33). Why?  --marekm
     */
    log_end();

    /*
     * PAM_DATA_SILENT is not supported by some modules, and
     * there is no strong need to clean up the process space's
     * memory since we will either call exec or exit.
    pam_end (pamh, PAM_SUCCESS | PAM_DATA_SILENT);
     */
    (void)execv(s->argv[0], (char **)s->argv);
    exit(errno == ENOENT ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
}

/* Signal handler for parent process later */
static int caught = 0;
static void catch_signals (int sig)
{
    ++caught;
}

/* This I ripped out of su.c from sh-utils after the Mandrake pam patch
 * have been applied.  Some work was needed to get it integrated into
 * su.c from shadow.
 */
int nodm_xsession_child_pam(struct nodm_xsession_child* s)
{
    int child = -1;
    sigset_t ourset;
    struct sigaction action;

    int res = setup_pam(s);
    if (res != E_SUCCESS)
        return res;

    // Read current signal mask
    sigset_t origmask;

    // Block all signals
    sigfillset (&ourset);
    if (sigprocmask (SIG_BLOCK, &ourset, &origmask)) {
        log_err("sigprocmask malfunction");
        goto killed;
    }

    child = fork ();
    if (child == 0) {   /* child shell */
        // Restore original signal mask
        if (sigprocmask (SIG_SETMASK, &origmask, NULL)) {
            log_err("sigprocmask malfunction");
            goto killed;
        }

        int res = nodm_xsession_child_common_env(s);
        if (res != E_SUCCESS) return res;

        /*
         * This is a workaround for Linux libc bug/feature (?) - the
         * /dev/log file descriptor is open without the close-on-exec flag
         * and used to be passed to the new shell. There is "fcntl(LogFile,
         * F_SETFD, 1)" in libc/misc/syslog.c, but it is commented out (at
         * least in 5.4.33). Why?  --marekm
         */
        log_end();

        /*
         * PAM_DATA_SILENT is not supported by some modules, and
         * there is no strong need to clean up the process space's
         * memory since we will either call exec or exit.
        pam_end (pamh, PAM_SUCCESS | PAM_DATA_SILENT);
         */
        (void) execv(s->argv[0], (char **)s->argv);
        exit (errno == ENOENT ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
    } else if (child == -1) {
        log_err("cannot fork user shell: %m");
        return E_OS_ERROR;
    }

    /* parent only */

    /* Reset caught signal flag */
    caught = 0;

    /* Catch SIGTERM and SIGALRM using 'catch_signals' */
    action.sa_handler = catch_signals;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;

    sigemptyset (&ourset);
    if (sigaddset (&ourset, SIGTERM)
#ifdef DEBUG_NODM
        || sigaddset (&ourset, SIGINT)
        || sigaddset (&ourset, SIGQUIT)
#endif
        || sigaddset (&ourset, SIGALRM)
        || sigaction (SIGTERM, &action, NULL)
        || sigprocmask (SIG_UNBLOCK, &ourset, NULL)
        ) {
        log_err("signal masking malfunction");
        goto killed;
    }

    do {
        int pid;

        pid = waitpid (-1, &(s->exit_status), WUNTRACED);

        if (WIFSTOPPED (s->exit_status)) {
            kill (getpid (), SIGSTOP);
            /* once we get here, we must have resumed */
            kill (pid, SIGCONT);
        }
    } while (WIFSTOPPED (s->exit_status));

    /* Unblock signals */
    sigfillset (&ourset);
    if (sigprocmask (SIG_UNBLOCK, &ourset, NULL)) {
        log_err("signal malfunction");
        goto killed;
    }

    if (caught)
        goto killed;

    shutdown_pam(s);

    return E_SUCCESS;

killed:
    if (child != -1)
    {
        log_warn("session terminated, killing shell...");
        kill (child, SIGTERM);
        sleep (2);
        kill (child, SIGKILL);
        log_warn(" ...shell killed.");
    }

    shutdown_pam(s);

    return E_SESSION_DIED;
}
