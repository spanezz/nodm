/*
 * xserver - X server startup functions
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

/*
 * nodm_xserver_read_window_path is taken from xdm's dm.c which is:
 *
 * Copyright 1988, 1998  The Open Group
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of The Open Group shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization
 * from The Open Group.
 */


#define _GNU_SOURCE
#include "xserver.h"
#include "common.h"
#include "log.h"
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xfuncproto.h>
#include <X11/Xatom.h>
#include <stdint.h>
#include <stdio.h>


// Signal handlers
static bool server_started = false;
static void on_sigusr1(int sig) { server_started = true; }
static void on_sigchld(int sig) {}

void nodm_xserver_init(struct nodm_xserver* srv)
{
    srv->conf_timeout = 5;
    srv->argv = 0;
    srv->name = 0;
    srv->pid = -1;
    srv->dpy = NULL;
    srv->windowpath = NULL;
}

/**
 * Start the X server and wait until it's ready to accept connections.
 *
 * @param srv
 *   The struct nodm_xserver with X server information. argv and name are expected to
 *   be filled, pid is filled.
 * @param timeout_sec
 *   Timeout in seconds after which if the X server is not ready, we give up
 *   and return an error.
 * @return
 *   Exit status as described by the E_* constants
 */
static int xserver_start(struct nodm_xserver* srv, unsigned timeout_sec)
{
    // Function return code
    int return_code = E_SUCCESS;

    // Initialise common signal handling machinery
    struct sigaction sa;
    sa.sa_flags = 0;
    if (sigemptyset(&sa.sa_mask) < 0)
    {
        log_err("sigemptyset failed: %m");
        return E_PROGRAMMING;
    }

    // Take note of the old SIGCHLD handler
    struct sigaction sa_sigchld_old;
    if (sigaction(SIGCHLD, NULL, &sa_sigchld_old) == -1)
    {
        log_err("sigaction failed: %m");
        return E_PROGRAMMING;
    }

    // Catch server ready notifications via SIGUSR1
    struct sigaction sa_usr1_old;
    server_started = false;
    sa.sa_handler = on_sigusr1;
    if (sigaction(SIGUSR1, &sa, &sa_usr1_old) == -1)
    {
        log_err("sigaction failed: %m");
        return E_PROGRAMMING;
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

        execv(srv->argv[0], (char *const*)srv->argv);
        log_err("cannot start %s: %m", srv->argv[0]);
        exit(errno == ENOENT ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
    } else if (child == -1) {
        log_err("cannot fork to run %s: %m", srv->argv[0]);
        return_code = E_OS_ERROR;
        goto cleanup;
    }

    // Get notified on sigchld, so nanosleep later will exit with EINTR if the
    // X server dies. If the server died before we set this signal handler,
    // that's fine, since waitpid will notice it anyway
    sa.sa_handler = on_sigchld;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        log_err("sigaction failed: %m");
        return_code = E_PROGRAMMING;
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
            return_code = E_OS_ERROR;
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
            return_code = E_X_SERVER_DIED;
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
            return_code = E_OS_ERROR;
            goto cleanup;
        } else {
            log_err("X server did not respond after %u seconds", timeout_sec);
            return_code = E_X_SERVER_TIMEOUT;
            goto cleanup;
        }
    }

    log_verbose("X is ready to accept connections");

    // Set the DISPLAY env var
    if (setenv("DISPLAY", srv->name, 1) == -1)
    {
        log_err("setenv DISPLAY=%s failed: %m", srv->name);
        return_code = E_OS_ERROR;
        goto cleanup;
    }

cleanup:
    // Kill the X server if an error happened
    if (child > 0 && return_code != E_SUCCESS)
        kill(child, SIGTERM);
    else
        srv->pid = child;
    // Restore signal handlers
    sigaction(SIGCHLD, NULL, &sa_sigchld_old);
    sigaction(SIGUSR1, NULL, &sa_usr1_old);
    return return_code;
}

/// Kill the X server
static int xserver_stop(struct nodm_xserver* srv)
{
    kill(srv->pid, SIGTERM);
    kill(srv->pid, SIGCONT);
    // TODO: wait
    srv->pid = -1;
    unsetenv("DISPLAY");
    return E_SUCCESS;
}

static int xopendisplay_error_handler(Display* dpy)
{
    log_err("I/O error in XOpenDisplay");
    log_end();
    exit(E_XLIB_ERROR);
}

/*
static int x_error_handler(Display* dpy, XErrorEvent* e)
{
    log_err("X error");
    return 0;
}
*/

/**
 * Connect to the X server
 *
 * Uses srv->name, sets srv->dpy.
 *
 * @return
 *   Exit status as described by the E_* constants
 */
static int xserver_connect(struct nodm_xserver* srv)
{
    //XSetErrorHandler(x_error_handler);

    XSetIOErrorHandler(xopendisplay_error_handler);
    srv->dpy = XOpenDisplay(srv->name);
    XSetIOErrorHandler((int (*)(Display *))0);

    if (srv->dpy == NULL)
        log_err("could not connect to X server on \"%s\"", srv->name);

    // from xdm: remove close-on-exec and register to close at the next fork, why? We'll find out
    // RegisterCloseOnFork (ConnectionNumber (d->dpy));
    // fcntl (ConnectionNumber (d->dpy), F_SETFD, 0);
    return srv->dpy == NULL ? E_X_SERVER_CONNECT : E_SUCCESS;
}

/**
 * Close connection to the X server
 *
 * Uses srv->dpy, sets it to NULL.
 *
 * @return
 *   Exit status as described by the E_* constants
 */
static int xserver_disconnect(struct nodm_xserver* srv)
{
    // TODO: get/check pending errors (how?)
    XCloseDisplay(srv->dpy);
    srv->dpy = NULL;
    return E_SUCCESS;
}

/**
 * Get the WINDOWPATH value for the server
 *
 * Uses srv->dpy, sets srv->windowpath
 *
 * @return
 *   Exit status as described by the E_* constants
 */
static int xserver_read_window_path(struct nodm_xserver* srv)
{
    /* setting WINDOWPATH for clients */
    Atom prop;
    Atom actualtype;
    int actualformat;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *buf;
    const char *windowpath;
    char *newwindowpath;
    unsigned long num;

    prop = XInternAtom(srv->dpy, "XFree86_VT", False);
    if (prop == None)
    {
        log_err("no XFree86_VT atom");
        return E_XLIB_ERROR;
    }
    if (XGetWindowProperty(srv->dpy, DefaultRootWindow(srv->dpy), prop, 0, 1,
                False, AnyPropertyType, &actualtype, &actualformat,
                &nitems, &bytes_after, &buf))
    {
        log_err("no XFree86 VT property");
        return E_XLIB_ERROR;
    }
    if (nitems == 0)
        num = DefaultRootWindow(srv->dpy);
    else
    {
        if (nitems != 1)
        {
            log_err("%lu!=1 items in XFree86_VT property", nitems);
            XFree(buf);
            return E_XLIB_ERROR;
        }
        switch (actualtype) {
            case XA_CARDINAL:
            case XA_INTEGER:
            case XA_WINDOW:
                switch (actualformat) {
                case  8:
                    num = (*(uint8_t  *)(void *)buf);
                    break;
                case 16:
                    num = (*(uint16_t *)(void *)buf);
                    break;
                case 32:
                    num = (*(uint32_t *)(void *)buf);
                    break;
                default:
                    log_err("unsupported format %d in XFree86_VT property", actualformat);
                    XFree(buf);
                    return E_XLIB_ERROR;
                }
                break;
            default:
                log_err("unsupported type %lx in XFree86_VT property", actualtype);
                XFree(buf);
                return E_XLIB_ERROR;
        }
    }
    XFree(buf);
    windowpath = getenv("WINDOWPATH");
    if (!windowpath)
        asprintf(&newwindowpath, "%lu", num);
    else
        asprintf(&newwindowpath, "%s:%lu", windowpath, num);
    if (srv->windowpath) free(srv->windowpath);
    srv->windowpath = newwindowpath;

    return E_SUCCESS;
}

int nodm_xserver_start(struct nodm_xserver* srv)
{
    int return_code = E_SUCCESS;

    return_code = xserver_start(srv, srv->conf_timeout);
    if (return_code != E_SUCCESS)
        goto cleanup;

    return_code = xserver_connect(srv);
    if (return_code != E_SUCCESS)
        goto cleanup;

    return_code = xserver_read_window_path(srv);
    if (return_code != E_SUCCESS)
        goto cleanup;

cleanup:
    if (return_code != E_SUCCESS)
        nodm_xserver_stop(srv);
    return return_code;
}

int nodm_xserver_stop(struct nodm_xserver* srv)
{
    int res1 = E_SUCCESS, res2 = E_SUCCESS;

    if (srv->dpy != NULL)
        res1 = xserver_disconnect(srv);
    if (srv->pid != -1)
        res2 = xserver_stop(srv);

    if (res1 != E_SUCCESS) return res1;
    return res2;
}

void nodm_xserver_dump_status(struct nodm_xserver* srv)
{
    fprintf(stderr, "xserver start timeout: %d\n", srv->conf_timeout);
    fprintf(stderr, "xserver command line:");
    for (const char** s = srv->argv; *s; ++s)
        fprintf(stderr, " %s", *s);
    fputc('\n', stderr);
    fprintf(stderr, "xserver name: %s\n", srv->name);
    fprintf(stderr, "xserver window path: %s\n", srv->windowpath);
    fprintf(stderr, "xserver PID: %d\n", (int)srv->pid);
    fprintf(stderr, "xserver connected: %s\n", (srv->dpy != NULL) ? "yes" : "no");
}
