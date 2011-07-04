/*
 * sstart - X server startup functions
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

#include "sstart.h"
#include "defs.h"
#include "log.h"
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


// Signal handlers
static bool server_started = false;
static void on_sigusr1(int sig) { server_started = true; }
static void on_sigchld(int sig) {}

void server_init(struct server* srv)
{
    srv->argv = 0;
    srv->name = 0;
    srv->pid = -1;
    srv->dpy = NULL;
}

int server_start(struct server* srv, unsigned timeout_sec)
{
    // Function return code
    int return_code = SSTART_SUCCESS;

    // Initialise common signal handling machinery
    struct sigaction sa;
    sa.sa_flags = 0;
    if (sigemptyset(&sa.sa_mask) < 0)
    {
        log_err("sigemptyset failed: %m");
        return SSTART_ERROR_PROGRAMMING;
    }

    // Take note of the old SIGCHLD handler
    struct sigaction sa_sigchld_old;
    if (sigaction(SIGCHLD, NULL, &sa_sigchld_old) == -1)
    {
        log_err("sigaction failed: %m");
        return SSTART_ERROR_PROGRAMMING;
    }

    // Catch server ready notifications via SIGUSR1
    struct sigaction sa_usr1_old;
    server_started = false;
    sa.sa_handler = on_sigusr1;
    if (sigaction(SIGUSR1, &sa, &sa_usr1_old) == -1)
    {
        log_err("sigaction failed: %m");
        return SSTART_ERROR_PROGRAMMING;
    }
    // From now on we need to perform cleanup before returning

    // fork/exec the X server
    pid_t child = fork ();
    if (child == 0) {   /* child shell */
        // Stop the logging subsystem before we quit via exec
        log_end();

        // Ignore SIGUSR1 to signal the X server that it should send us SIGUSR1
        // when ready
        signal(SIGUSR1, SIG_IGN);

        execv(srv->argv[0], srv->argv);
        log_err("cannot start %s: %m", srv->argv[0]);
        exit(errno == ENOENT ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
    } else if (child == -1) {
        log_err("cannot fork to run %s: %m", srv->argv[0]);
        return_code = SSTART_ERROR_SYSTEM;
        goto cleanup;
    }

    // Get notified on sigchld, so nanosleep later will exit with EINTR if the
    // X server dies. If the server died before we set this signal handler,
    // that's fine, since waitpid will notice it anyway
    sa.sa_handler = on_sigchld;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        log_err("sigaction failed: %m");
        return_code = SSTART_ERROR_PROGRAMMING;
        goto cleanup;
    }

    // Wait for SIGUSR1, for the server to die or for a timeout
    struct timespec timeout = { .tv_sec = timeout_sec, .tv_nsec = 0 };
    while (!server_started)
    {
        // Check if the server has died
        int status;
        pid_t res = waitpid(child, &status, WNOHANG);
        if (res == -1)
        {
            if (errno == EINTR) continue;
            log_err("waitpid on %s failed: %m", srv->argv[0]);
            return_code = SSTART_ERROR_SYSTEM;
            goto cleanup;
        }
        if (res == child)
        {
            if (WIFEXITED(status))
                log_err("X server exited with status=%d", WEXITSTATUS(status));
            else if (WIFSIGNALED(status))
                log_err("X server killed by signal %d", WTERMSIG(status));
            else 
                // This should never happen, but it's better to have a message
                // than to fail silently through an open code path
                log_err("X server quit, waitpid gave unrecognised status=%d", status);
            return_code = SSTART_ERROR_SERVER_DIED;
            goto cleanup;
        }

        // Wait some time for something to happen
        struct timespec rem;
        if (nanosleep(&timeout, &rem) == -1)
        {
            if (errno == EINTR)
            {
                timeout = rem;
                continue;
            }
            log_err("nanosleep failed: %m");
            return_code = SSTART_ERROR_SYSTEM;
            goto cleanup;
        } else {
            log_err("X server did not respond after %u seconds", timeout_sec);
            return_code = SSTART_ERROR_TIMEOUT;
            goto cleanup;
        }
    }

    // Set the DISPLAY env var
    if (setenv("DISPLAY", srv->name, 1) == -1)
    {
        log_err("setenv DISPLAY=%s failed: %m", srv->name);
        return_code = SSTART_ERROR_SYSTEM;
        goto cleanup;
    }

cleanup:
    // Kill the X server if an error happened
    if (child > 0 && return_code != SSTART_SUCCESS)
        kill(child, SIGTERM);
    else
        srv->pid = child;
    // Restore signal handlers
    sigaction(SIGCHLD, NULL, &sa_sigchld_old);
    sigaction(SIGUSR1, NULL, &sa_usr1_old);
    return return_code;
}

static int xopendisplay_error_handler(Display* dpy)
{
    log_err("I/O error in XOpenDisplay");
    log_end();
    exit(E_X_ERROR);
}

int server_connect(struct server* srv)
{
    XSetIOErrorHandler(xopendisplay_error_handler);
    srv->dpy = XOpenDisplay(srv->name);
    XSetIOErrorHandler((int (*)(Display *))0);

    // Remove close-on-exec and register to close at the next fork, why? We'll find
    // RegisterCloseOnFork (ConnectionNumber (d->dpy));
    // fcntl (ConnectionNumber (d->dpy), F_SETFD, 0);
    return srv->dpy == NULL ? SSTART_ERROR_CONNECT : SSTART_SUCCESS;
}

int server_disconnect(struct server* srv)
{
    XCloseDisplay(srv->dpy);
    return SSTART_SUCCESS;
}
