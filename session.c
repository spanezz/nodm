/*
 * session - nodm X session
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

#include "session.h"
#include "log.h"
#include "common.h"
#include <errno.h>
#include <signal.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <wordexp.h>

/* compatibility with different versions of Linux-PAM */
#ifndef PAM_ESTABLISH_CRED
#define PAM_ESTABLISH_CRED PAM_CRED_ESTABLISH
#endif
#ifndef PAM_DELETE_CRED
#define PAM_DELETE_CRED PAM_CRED_DELETE
#endif

/* Copy string pointed by B to array A with size checking.  It was originally
   in lmain.c but is _very_ useful elsewhere.  Some setuid root programs with
   very sloppy coding used to assume that BUFSIZ will always be enough...  */
                                        /* danger - side effects */
#define STRFCPY(A,B) \
        (strncpy((A), (B), sizeof(A) - 1), (A)[sizeof(A) - 1] = '\0')

static int run_shell (const char** args, int* status);

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

static int session_user_setup(struct session* s)
{
    /*
     * Validate the user using the normal system user database
     */
    struct passwd *pw = 0;
    if (!(pw = getpwnam(s->conf_run_as))) {
        log_err("Unknown username: %s", s->conf_run_as);
        return E_OS_ERROR;
    }
    s->pwent = *pw;

    return E_SUCCESS;
}

static int session_pam_setup(struct session* s)
{
    static struct pam_conv conv = {
        misc_conv,
        NULL
    };

    char *cp;
    const char *tty = 0;    /* Name of tty SU is run from        */

    /* We only run if we are root */
    if (getuid() != 0)
    {
        log_err("can only be run by root");
        return E_NOPERM;
    }

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

void session_pam_shutdown(struct session* s)
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

void nodm_session_init(struct session* s)
{
    server_init(&(s->srv));
    s->pamh = NULL;
    s->pam_status = PAM_SUCCESS;
    s->srv_split_args = NULL;

    s->conf_use_pam = true;
    s->conf_cleanup_xse = true;

    // Get the user we should run the session for
    strncpy(s->conf_run_as, getenv_with_default("NODM_USER", "root"), 128);
    s->conf_run_as[127] = 0;
}

void nodm_session_cleanup(struct session* s)
{
    // End pam session if used
    if (s->pamh)
        session_pam_shutdown(s);

    // Deallocate parsed arguments, if used
    if (s->srv_split_args)
    {
        wordexp_t* we = (wordexp_t*)s->srv_split_args;
        wordfree(we);
        free(we);
        s->srv_split_args = NULL;
    }
}

/*
 * Start the session, with proper autologin and pam handling
 */
int nodm_session(struct session* s)
{
    const char* xsession;
    const char* args[5];
    int res;

    if ((res = session_user_setup(s)))
        return res;

    if (s->conf_use_pam && ((res = session_pam_setup(s))))
        return res;

    xsession = getenv_with_default("NODM_XSESSION", "/etc/X11/Xsession");

    setenv ("HOME", s->pwent.pw_dir, 1);
    setenv ("USER", s->pwent.pw_name, 1);
    setenv ("USERNAME", s->pwent.pw_name, 1);
    setenv ("LOGNAME", s->pwent.pw_name, 1);
    setenv ("PWD", s->pwent.pw_dir, 1);
    setenv ("SHELL", s->pwent.pw_shell, 1);
    setenv ("DISPLAY", s->srv.name, 1);
    setenv ("WINDOWPATH", s->srv.windowpath, 1);

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

    /* Clear the NODM_* environment variables */
    unsetenv("NODM_USER");
    unsetenv("NODM_XINIT");
    unsetenv("NODM_XSESSION");
    unsetenv("NODM_X_OPTIONS");
    unsetenv("NODM_MIN_SESSION_TIME");
    unsetenv("NODM_RUN_SESSION");

    if (chdir(s->pwent.pw_dir) == 0)
        /* Truncate ~/.xsession-errors */
        cleanup_xse(0, s->pwent.pw_dir);

    args[0] = "/bin/sh";
    args[1] = "-l";
    args[2] = "-c";
    args[3] = xsession;
    args[4] = NULL;

    int status;
    if ((res = run_shell(args, &status)))
        return res;

    nodm_session_cleanup(s);

    return status;
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
static int run_shell (const char** args, int* status)
{
    int child;
    sigset_t ourset;
    struct sigaction action;

    child = fork ();
    if (child == 0) {   /* child shell */
        /*
         * This is a workaround for Linux libc bug/feature (?) - the
         * /dev/log file descriptor is open without the close-on-exec flag
         * and used to be passed to the new shell. There is "fcntl(LogFile,
         * F_SETFD, 1)" in libc/misc/syslog.c, but it is commented out (at
         * least in 5.4.33). Why?  --marekm
         */
        closelog ();

        /*
         * PAM_DATA_SILENT is not supported by some modules, and
         * there is no strong need to clean up the process space's
         * memory since we will either call exec or exit.
        pam_end (pamh, PAM_SUCCESS | PAM_DATA_SILENT);
         */
        (void) execv (args[0], (char **) args);
        exit (errno == ENOENT ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
    } else if (child == -1) {
        log_err("cannot fork user shell: %m");
        return E_OS_ERROR;
    }

    /* parent only */

    /* Reset caught signal flag */
    caught = 0;

    /* Block all signals */
    sigfillset (&ourset);
    if (sigprocmask (SIG_BLOCK, &ourset, NULL)) {
        log_err("sigprocmask malfunction");
        goto killed;
    }

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

        pid = waitpid (-1, status, WUNTRACED);

        if (WIFSTOPPED (*status)) {
            kill (getpid (), SIGSTOP);
            /* once we get here, we must have resumed */
            kill (pid, SIGCONT);
        }
    } while (WIFSTOPPED (*status));

    /* Unblock signals */
    sigfillset (&ourset);
    if (sigprocmask (SIG_UNBLOCK, &ourset, NULL)) {
        log_err("signal malfunction");
        goto killed;
    }

    if (caught)
        goto killed;

    return E_SUCCESS;

killed:
    log_warn("session terminated, killing shell...");
    kill (child, SIGTERM);
    sleep (2);
    kill (child, SIGKILL);
    log_warn(" ...shell killed.");
    return E_SESSION_DIED;
}

int nodm_x_with_session(struct session* s)
{
    int return_code = 0;

    return_code = server_start(&(s->srv), 5);
    if (return_code != E_SUCCESS)
        goto cleanup;

    return_code = server_connect(&(s->srv));
    if (return_code != E_SUCCESS)
        goto cleanup_server;

    return_code = server_read_window_path(&(s->srv));
    if (return_code != E_SUCCESS)
        goto cleanup_connection;

    return_code = nodm_session(s);

cleanup_connection:
    return_code = server_disconnect(&(s->srv));

cleanup_server:
    return_code = server_stop(&(s->srv));

cleanup:
    return return_code;
}

int nodm_session_parse_cmdline(struct session* s, const char* xcmdline)
{
    int return_code = E_SUCCESS;

    // tokenize xoptions
    wordexp_t* toks = (wordexp_t*)calloc(1, sizeof(wordexp_t));
    switch (wordexp(xcmdline, toks, WRDE_NOCMD))
    {
        case 0: break;
        case WRDE_NOSPACE:
            return_code = E_OS_ERROR;
            goto cleanup;
        default:
            toks->we_wordv = NULL;
            return_code = E_BAD_ARG;
            goto cleanup;
    }

    unsigned in_arg = 0;
    unsigned argc = 0;
    char **argv = (char**)malloc((toks->we_wordc + 3) * sizeof(char*));

    // Server command
    if (in_arg < toks->we_wordc &&
           (toks->we_wordv[in_arg][0] == '/' || toks->we_wordv[in_arg][0] == '.'))
        argv[argc++] = toks->we_wordv[in_arg++];
    else
        argv[argc++] = "/usr/bin/X";

    // Server name
    if (in_arg < toks->we_wordc &&
           toks->we_wordv[in_arg][0] == ':' && isdigit(toks->we_wordv[in_arg][1]))
        argv[argc++] = toks->we_wordv[in_arg++];
    else
        argv[argc++] = ":0";

    // Copy other args
    while (in_arg < toks->we_wordc)
        argv[argc++] = toks->we_wordv[in_arg++];
    argv[argc] = NULL;

    s->srv.argv = argv;
    s->srv_split_args = toks;
    argv = NULL;
    toks = NULL;

cleanup:
    if (toks != NULL)
    {
        if (toks->we_wordv)
            wordfree(toks);
        free(toks);
    }
    if (argv != NULL)
        free(argv);

    return E_SUCCESS;
}
