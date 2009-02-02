/*
 * Copyright 1989 - 1994, Julianne Frances Haugh
 * Copyright 2008, Enrico Zini
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
 */
/* Some parts substantially derived from an ancestor of: */
/* su for GNU.  Run a shell with substitute user and group IDs.
   Copyright (C) 1992-2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#define PACKAGE "nodm"

#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

static struct pam_conv conv = {
	misc_conv,
	NULL
};

/* compatibility with different versions of Linux-PAM */
#ifndef PAM_ESTABLISH_CRED
#define PAM_ESTABLISH_CRED PAM_CRED_ESTABLISH
#endif
#ifndef PAM_DELETE_CRED
#define PAM_DELETE_CRED PAM_CRED_DELETE
#endif
#ifndef PAM_NEW_AUTHTOK_REQD
#define PAM_NEW_AUTHTOK_REQD PAM_AUTHTOKEN_REQD
#endif
#ifndef PAM_DATA_SILENT
#define PAM_DATA_SILENT 0
#endif

#define LOG_WARN LOG_WARNING
#define SYSLOG(x) syslog x
#define SYSLOG_OPTIONS (LOG_PID)
#define SYSLOG_FACILITY LOG_AUTHPRIV
#define OPENLOG(progname) openlog(progname, SYSLOG_OPTIONS, SYSLOG_FACILITY)
#define _(...) (__VA_ARGS__)
/* Copy string pointed by B to array A with size checking.  It was originally
   in lmain.c but is _very_ useful elsewhere.  Some setuid root programs with
   very sloppy coding used to assume that BUFSIZ will always be enough...  */
                                        /* danger - side effects */
#define STRFCPY(A,B) \
        (strncpy((A), (B), sizeof(A) - 1), (A)[sizeof(A) - 1] = '\0')

/*
 * Exit codes used by shadow programs
 */
#define E_SUCCESS               0       /* success */
#define E_NOPERM                1       /* permission denied */
#define E_USAGE                 2       /* invalid command syntax */
#define E_BAD_ARG               3       /* invalid argument to option */
#define E_PASSWD_NOTFOUND       14      /* not found password file */
#define E_SHADOW_NOTFOUND       15      /* not found shadow password file */
#define E_GROUP_NOTFOUND        16      /* not found group file */
#define E_GSHADOW_NOTFOUND      17      /* not found shadow group file */
#define E_CMD_NOEXEC            126     /* can't run command/shell */
#define E_CMD_NOTFOUND          127     /* can't find command/shell to run */

#define RETSIGTYPE void

/*
 * Assorted #defines to control su's behavior
 */
/*
 * Global variables
 */
/* not needed by sulog.c anymore */
static char name[BUFSIZ];
static char oldname[BUFSIZ];

static pam_handle_t *pamh = NULL;
static int caught = 0;

static char *Prog;
struct passwd pwent;

/*
 * External identifiers
 */

extern char **newenvp;
extern char **environ;
extern size_t newenvc;

/* local function prototypes */

static char *Basename (char *str)
{
	char *cp = strrchr (str, '/');

	return cp ? cp + 1 : str;
}

/*
 * setup_uid_gid() split in two functions for PAM support -
 * pam_setcred() needs to be called after initgroups(), but
 * before setuid().
 */
static int setup_groups (const struct passwd *info)
{
	/*
	 * Set the real group ID to the primary group ID in the password
	 * file.
	 */
	if (setgid (info->pw_gid) == -1) {
		perror ("setgid");
		SYSLOG ((LOG_ERR, "bad group ID `%d' for user `%s': %m\n",
			 info->pw_gid, info->pw_name));
		closelog ();
		return -1;
	}
#ifdef HAVE_INITGROUPS
	/*
	 * For systems which support multiple concurrent groups, go get
	 * the group set from the /etc/group file.
	 */
	if (initgroups (info->pw_name, info->pw_gid) == -1) {
		perror ("initgroups");
		SYSLOG ((LOG_ERR, "initgroups failed for user `%s': %m\n",
			 info->pw_name));
		closelog ();
		return -1;
	}
#endif
	return 0;
}

int change_uid (const struct passwd *info)
{
	/*
	 * Set the real UID to the UID value in the password file.
	 */
	if (setuid (info->pw_uid)) {
		perror ("setuid");
		SYSLOG ((LOG_ERR, "bad user ID `%d' for user `%s': %m\n",
			 (int) info->pw_uid, info->pw_name));
		closelog ();
		return -1;
	}
	return 0;
}

/*
 * sulog - log a SU command execution result
 */
static void sulog (const char *tty, int success, const char *oldname, const char *name)
{
        if (success) {
                SYSLOG ((LOG_INFO,
                        "Successful su for %s by %s",name,oldname));
        } else {
                SYSLOG ((LOG_NOTICE,
                        "FAILED su for %s by %s",name,oldname));
        }
}

static void su_failure (const char *tty)
{
	sulog (tty, 0, oldname, name);	/* log failed attempt */
#ifdef USE_SYSLOG
	if (getdef_bool ("SYSLOG_SU_ENAB"))
		SYSLOG ((pwent.pw_uid ? LOG_INFO : LOG_NOTICE,
			 "- %s %s:%s", tty,
			 oldname[0] ? oldname : "???", name[0] ? name : "???"));
	closelog ();
#endif
	exit (1);
}


/* Signal handler for parent process later */
static void catch_signals (int sig)
{
	++caught;
}

/* This I ripped out of su.c from sh-utils after the Mandrake pam patch
 * have been applied.  Some work was needed to get it integrated into
 * su.c from shadow.
 */
static void run_shell ()
{
	int child;
	sigset_t ourset;
	int status;
	int ret;
	char* argv0 = getenv("NODM_XINIT");
	if (argv0 == NULL)
		argv0 = "/usr/bin/xinit";

	child = fork ();
	if (child == 0) {	/* child shell */
		/*
		 * PAM_DATA_SILENT is not supported by some modules, and
		 * there is no strong need to clean up the process space's
		 * memory since we will either call exec or exit.
		pam_end (pamh, PAM_SUCCESS | PAM_DATA_SILENT);
		 */
		char* args[5];
		args[0] = argv0;
		args[1] = getenv("NODM_XSESSION");
		if (args[1] == NULL) args[1] = "/etc/X11/Xsession";
		args[2] = "--";
		args[3] = getenv("NODM_X_OPTIONS");
		args[4] = NULL;
		(void) execv (args[0], (char **) args);
		exit (errno == ENOENT ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
	} else if (child == -1) {
		(void) fprintf (stderr, "%s: Cannot fork user shell\n", Prog);
		SYSLOG ((LOG_WARN, "Cannot execute %s", argv0));
		closelog ();
		exit (1);
	}
	/* parent only */
	sigfillset (&ourset);
	if (sigprocmask (SIG_BLOCK, &ourset, NULL)) {
		(void) fprintf (stderr, "%s: signal malfunction\n", Prog);
		caught = 1;
	}
	if (!caught) {
		struct sigaction action;

		action.sa_handler = catch_signals;
		sigemptyset (&action.sa_mask);
		action.sa_flags = 0;
		sigemptyset (&ourset);

		if (sigaddset (&ourset, SIGTERM)
		    || sigaddset (&ourset, SIGALRM)
		    || sigaction (SIGTERM, &action, NULL)
		    || sigprocmask (SIG_UNBLOCK, &ourset, NULL)
		    ) {
			fprintf (stderr,
				 "%s: signal masking malfunction\n", Prog);
			caught = 1;
		}
	}

	if (!caught) {
		do {
			int pid;

			pid = waitpid (-1, &status, WUNTRACED);

			if (WIFSTOPPED (status)) {
				kill (getpid (), SIGSTOP);
				/* once we get here, we must have resumed */
				kill (pid, SIGCONT);
			}
		} while (WIFSTOPPED (status));
	}

	if (caught) {
		fprintf (stderr, "\nSession terminated, killing shell...");
		kill (child, SIGTERM);
	}

	ret = pam_close_session (pamh, 0);
	if (ret != PAM_SUCCESS) {
		SYSLOG ((LOG_ERR, "pam_close_session: %s",
			 pam_strerror (pamh, ret)));
		fprintf (stderr, _("%s: %s\n"), Prog, pam_strerror (pamh, ret));
		pam_end (pamh, ret);
		exit (1);
	}

	ret = pam_end (pamh, PAM_SUCCESS);

	if (caught) {
		sleep (2);
		kill (child, SIGKILL);
		fprintf (stderr, " ...killed.\n");
		exit (-1);
	}

	exit (WIFEXITED (status)
	      ? WEXITSTATUS (status)
	      : WTERMSIG (status) + 128);
}

static struct passwd *get_my_pwent (void)
{
	struct passwd *pw;
	const char *cp = getlogin ();
	uid_t ruid = getuid ();

	/*
	 * Try getlogin() first - if it fails or returns a non-existent
	 * username, or a username which doesn't match the real UID, fall
	 * back to getpwuid(getuid()).  This should work reasonably with
	 * usernames longer than the utmp limit (8 characters), as well as
	 * shared UIDs - but not both at the same time...
	 *
	 * XXX - when running from su, will return the current user (not
	 * the original user, like getlogin() does).  Does this matter?
	 */
	if (cp && *cp && (pw = getpwnam (cp)) && pw->pw_uid == ruid)
		return pw;

	return getpwuid (ruid);
}

/*
 * su - switch user id
 *
 *	su changes the user's ids to the values for the specified user.  if
 *	no new user name is specified, "root" is used by default.
 *
 *	Any additional arguments are passed to the user's shell. In
 *	particular, the argument "-c" will cause the next argument to be
 *	interpreted as a command by the common shell programs.
 */
int main (int argc, char **argv)
{
	char *cp;
	const char *tty = 0;	/* Name of tty SU is run from        */
	int amroot = 0;
	uid_t my_uid;
	struct passwd *pw = 0;
	char **envcp;
	int ret;

	/*
	 * Get the program name. The program name is used as a prefix to
	 * most error messages.
	 */
	Prog = Basename (argv[0]);

	OPENLOG ("su");

	/*
	 * Process the command line arguments. 
	 */

	my_uid = getuid ();
	amroot = (my_uid == 0);

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
		/*
		 * Be more paranoid, like su from SimplePAMApps.  --marekm
		 */
		if (!amroot) {
			fprintf (stderr,
				 _("%s: must be run from a terminal\n"), Prog);
			exit (1);
		}
		tty = "???";
	}

	if (getenv("NODM_USER") == NULL)
		strcpy(name, "root");
	else
		strncpy(name, getenv("NODM_USER"), BUFSIZ);

	/*
	 * Get the user's real name. The current UID is used to determine
	 * who has executed su. That user ID must exist.
	 */
	pw = get_my_pwent ();
	if (!pw) {
		SYSLOG ((LOG_CRIT, "Unknown UID: %u", my_uid));
		su_failure (tty);
	}
	STRFCPY (oldname, pw->pw_name);

	ret = pam_start ("su", name, &conv, &pamh);
	if (ret != PAM_SUCCESS) {
		SYSLOG ((LOG_ERR, "pam_start: error %d", ret);
			fprintf (stderr, _("%s: pam_start: error %d\n"),
				 Prog, ret));
		exit (1);
	}

	ret = pam_set_item (pamh, PAM_TTY, (const void *) tty);
	if (ret == PAM_SUCCESS)
		ret = pam_set_item (pamh, PAM_RUSER, (const void *) oldname);
	if (ret != PAM_SUCCESS) {
		SYSLOG ((LOG_ERR, "pam_set_item: %s",
			 pam_strerror (pamh, ret)));
		fprintf (stderr, _("%s: %s\n"), Prog, pam_strerror (pamh, ret));
		pam_end (pamh, ret);
		exit (1);
	}

	/*
	 * This is the common point for validating a user whose name is
	 * known. It will be reached either by normal processing, or if the
	 * user is to be logged into a subsystem root.
	 *
	 * The password file entries for the user is gotten and the account
	 * validated.
	 */
	if (!(pw = getpwnam (name))) {
		(void) fprintf (stderr, _("Unknown id: %s\n"), name);
		closelog ();
		exit (1);
	}
	pwent = *pw;

	signal (SIGINT, SIG_IGN);
	signal (SIGQUIT, SIG_IGN);

	ret = pam_acct_mgmt (pamh, 0);
	if (ret != PAM_SUCCESS) {
		if (amroot) {
			fprintf (stderr, _("%s: %s\n(Ignored)\n"), Prog,
				 pam_strerror (pamh, ret));
		} else if (ret == PAM_NEW_AUTHTOK_REQD) {
			ret = pam_chauthtok (pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
			if (ret != PAM_SUCCESS) {
				SYSLOG ((LOG_ERR, "pam_chauthtok: %s",
					 pam_strerror (pamh, ret)));
				fprintf (stderr, _("%s: %s\n"), Prog,
					 pam_strerror (pamh, ret));
				pam_end (pamh, ret);
				su_failure (tty);
			}
		} else {
			SYSLOG ((LOG_ERR, "pam_acct_mgmt: %s",
				 pam_strerror (pamh, ret)));
			fprintf (stderr, _("%s: %s\n"), Prog,
				 pam_strerror (pamh, ret));
			pam_end (pamh, ret);
			su_failure (tty);
		}
	}

	signal (SIGINT, SIG_DFL);
	signal (SIGQUIT, SIG_DFL);

	setenv ("PATH", "/bin:/usr/bin", 1);

	sulog (tty, 1, oldname, name);	/* save SU information */

#ifdef USE_SYSLOG
	SYSLOG ((LOG_INFO, "+ %s %s:%s", tty,
		 oldname[0] ? oldname : "???", name[0] ? name : "???"));
#endif

	/* set primary group id and supplementary groups */
	if (setup_groups (&pwent)) {
		pam_end (pamh, PAM_ABORT);
		exit (1);
	}

	/*
	 * pam_setcred() may do things like resource limits, console groups,
	 * and much more, depending on the configured modules
	 */
	ret = pam_setcred (pamh, PAM_ESTABLISH_CRED);
	if (ret != PAM_SUCCESS) {
		SYSLOG ((LOG_ERR, "pam_setcred: %s", pam_strerror (pamh, ret)));
		fprintf (stderr, _("%s: %s\n"), Prog, pam_strerror (pamh, ret));
		pam_end (pamh, ret);
		exit (1);
	}

	ret = pam_open_session (pamh, 0);
	if (ret != PAM_SUCCESS) {
		SYSLOG ((LOG_ERR, "pam_open_session: %s",
			 pam_strerror (pamh, ret)));
		fprintf (stderr, _("%s: %s\n"), Prog, pam_strerror (pamh, ret));
		pam_setcred (pamh, PAM_DELETE_CRED);
		pam_end (pamh, ret);
		exit (1);
	}

	/* update environment with all pam set variables */
	envcp = pam_getenvlist (pamh);
	if (envcp) {
		while (*envcp) {
			putenv(*envcp);
			envcp++;
		}
	}

	/* become the new user */
	if (change_uid (&pwent)) {
		pam_close_session (pamh, 0);
		pam_setcred (pamh, PAM_DELETE_CRED);
		pam_end (pamh, PAM_ABORT);
		exit (1);
	}

	setenv ("HOME", pwent.pw_dir, 1);
	setenv ("USER", pwent.pw_name, 1);
	setenv ("LOGNAME", pwent.pw_name, 1);

	/*
	 * This is a workaround for Linux libc bug/feature (?) - the
	 * /dev/log file descriptor is open without the close-on-exec flag
	 * and used to be passed to the new shell. There is "fcntl(LogFile,
	 * F_SETFD, 1)" in libc/misc/syslog.c, but it is commented out (at
	 * least in 5.4.33). Why?  --marekm
	 */
	closelog ();

	run_shell();

	/* NOT REACHED */
	exit (1);
}
