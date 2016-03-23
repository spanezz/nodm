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
#include <setjmp.h>


// Signal handlers
static bool server_started = false;
static void on_sigusr1(int sig) { server_started = true; }
static void on_sigchld(int sig) {}

void nodm_xserver_init(struct nodm_xserver* srv)
{
    // Get the user we should run the session for
    srv->conf_timeout = atoi(getenv_with_default("NODM_X_TIMEOUT", "30"));
    srv->argv = 0;
    srv->name = 0;
    srv->pid = -1;
    srv->dpy = NULL;
    srv->windowpath = NULL;
    if (sigemptyset(&srv->orig_signal_mask) == -1)
        log_err("sigemptyset error: %m");
}

int nodm_xserver_start(struct nodm_xserver* srv)
{
    // Function return code
    int return_code = E_SUCCESS;
    // True if we should restore the signal mask at exit
    bool signal_mask_altered = false;

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

    if (log_verb(NULL))
    {
        // Log the concatenated command line
        char buf[4096];
        int pos = 0;
        const char** s = srv->argv;
        for ( ; *s && pos < 4096; ++s)
        {
            int r = snprintf(buf + pos, 4096 - pos, " %s", *s);
            if (r < 0) break;
            pos += r;
        }
        log_verb("starting X server %s", buf);
    }
    // fork/exec the X server
    srv->pid = fork ();
    if (srv->pid == 0)
    {
        // child

        // Restore the original signal mask
        if (sigprocmask(SIG_SETMASK, &srv->orig_signal_mask, NULL) == -1)
            log_err("sigprocmask failed: %m");

        // Stop the logging subsystem before we quit via exec
        log_end();

        // don't hang on read/write to control tty (from xinit)
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        // Ignore SIGUSR1 to signal the X server that it should send us SIGUSR1
        // when ready
        signal(SIGUSR1, SIG_IGN);

        // prevent the server from getting sighup from vhangup() (from xinit)
        setpgid(0, getpid());

        execv(srv->argv[0], (char *const*)srv->argv);
        log_err("cannot start %s: %m", srv->argv[0]);
        exit(errno == ENOENT ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
    } else if (srv->pid == -1) {
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

    // Unblock SIGCHLD and SIGUSR1
    sigset_t orig_set;
    sigset_t cur_set;
    if (sigemptyset(&cur_set) == -1)
    {
        log_err("sigemptyset failed: %m");
        return_code = E_PROGRAMMING;
        goto cleanup;
    }
    if (sigaddset(&cur_set, SIGCHLD) == -1 || sigaddset(&cur_set, SIGUSR1) == -1)
    {
        log_err("sigaddset failed: %m");
        return_code = E_PROGRAMMING;
        goto cleanup;
    }
    if (sigprocmask(SIG_UNBLOCK, &cur_set, &orig_set) == -1)
    {
        log_err("sigprocmask failed: %m");
        return_code = E_PROGRAMMING;
        goto cleanup;
    }
    signal_mask_altered = true;

    // Wait for SIGUSR1, for the server to die or for a timeout
    struct timespec timeout = { .tv_sec = srv->conf_timeout, .tv_nsec = 0 };
    while (!server_started)
    {
        // Check if the server has died
        int status;
        pid_t res = waitpid(srv->pid, &status, WNOHANG);
        if (res == -1)
        {
            if (errno == EINTR) continue;
            log_err("waitpid on %s failed: %m", srv->argv[0]);
            return_code = E_OS_ERROR;
            goto cleanup;
        }
        if (res == srv->pid)
        {
            nodm_xserver_report_exit(srv, status);
            srv->pid = -1;
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
            log_err("X server did not respond after %u seconds", srv->conf_timeout);
            return_code = E_X_SERVER_TIMEOUT;
            goto cleanup;
        }
    }

    log_verb("X is ready to accept connections");

    return_code = nodm_xserver_connect(srv);
    if (return_code != E_SUCCESS) goto cleanup;

cleanup:
    // Restore signal mask
    if (signal_mask_altered)
        if (sigprocmask(SIG_SETMASK, &orig_set, NULL) == -1)
            log_err("sigprocmask failed: %m");

    // Kill the X server if an error happened
    if (srv->pid > 0 && return_code != E_SUCCESS)
        nodm_xserver_stop(srv);

    // Restore signal handlers
    sigaction(SIGCHLD, NULL, &sa_sigchld_old);
    sigaction(SIGUSR1, NULL, &sa_usr1_old);
    return return_code;
}

int nodm_xserver_stop(struct nodm_xserver* srv)
{
    nodm_xserver_disconnect(srv);

    int res = child_must_exit(srv->pid, "X server");
    srv->pid = -1;

    if (srv->windowpath != NULL)
    {
        free(srv->windowpath);
        srv->windowpath = NULL;
    }
    return res;
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

int nodm_xserver_connect(struct nodm_xserver* srv)
{
    log_verb("connecting to X server");
    //XSetErrorHandler(x_error_handler);

    for (int i = 0; i < 5; ++i)
    {
        if (i > 0)
            log_info("connecting to X server, attempt #%d", i+1);
        XSetIOErrorHandler(xopendisplay_error_handler);
        srv->dpy = XOpenDisplay(srv->name);
        XSetIOErrorHandler((int (*)(Display *))0);

        if (srv->dpy == NULL)
        {
            log_err("could not connect to X server on \"%s\"", srv->name);
            sleep(1);
        } else
            break;
    }

    // from xdm: remove close-on-exec and register to close at the next fork, why? We'll find out
    // RegisterCloseOnFork (ConnectionNumber (d->dpy));
    // fcntl (ConnectionNumber (d->dpy), F_SETFD, 0);
    return srv->dpy == NULL ? E_X_SERVER_CONNECT : E_SUCCESS;
}

static jmp_buf close_env;
static int ignorexio(Display *dpy)
{
    longjmp(close_env, 1);
    // Not reached
    return 0;
}

int nodm_xserver_disconnect(struct nodm_xserver* srv)
{
    log_verb("disconnecting from X server");
    // TODO: get/check pending errors (how?)
    if (srv->dpy != NULL)
    {
        XSetIOErrorHandler(ignorexio);
        if (! setjmp(close_env))
            XCloseDisplay(srv->dpy);
        else
            log_warn("I/O error on display close");
        XSetIOErrorHandler(NULL);
        srv->dpy = NULL;
    }
    return E_SUCCESS;
}

int nodm_xserver_read_window_path(struct nodm_xserver* srv)
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

    log_verb("reading WINDOWPATH value from server");

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

    int path_size;
    if (!windowpath)
        path_size = asprintf(&newwindowpath, "%lu", num);
    else
        path_size = asprintf(&newwindowpath, "%s:%lu", windowpath, num);
    if (path_size > 0) {
        if (srv->windowpath) free(srv->windowpath);
        srv->windowpath = newwindowpath;
        log_verb("WINDOWPATH: %s", srv->windowpath);
    }

    return E_SUCCESS;
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

void nodm_xserver_report_exit(struct nodm_xserver* s, int status)
{
    if (WIFEXITED(status))
        log_warn("X server %d quit with status %d",
               (int)s->pid, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        log_err("X server %d was killed with signal %d",
               (int)s->pid, WTERMSIG(status));
    else
        log_err("X server %d terminated with unknown status %d",
               (int)s->pid, status);
}
