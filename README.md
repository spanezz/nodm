nodm is a minimal display manager that simply logs in as a given user and
starts an X session, without asking for username or password.

On a normal computer, using nodm is a big security issue because it would give
anyone access to the computer.

However, there are cases where automatic login is needed: for example in an
embedded system such as a mobile phone, or in a kiosk setup, or in a control
panel for industrial machinery.  For those cases, nodm is simple to setup,
lightweight, and it should do exactly the right thing.


## Features

nodm is as small as it could be, and tries to provide the minimum amount of
features needed to do a good job, following as much as possible the principle
of least surprise.  This is what is offered:

 - Automatic login with a fixed user, doing all that needs to be done like
   setting up the session via PAM, updating lastlog, logging to syslog.
 - nodm performs VT allocation, looking for a free virtual terminal in which to
   run X and keeping it allocated across X restarts.
 - X is started (by default, /usr/bin/X)
 - once the X esrver is ready to accept connections, the X session is set up:
    - the DISPLAY and WINDOWPATH environment variables are set
    - the session is wrapped in a PAM session, which sets up the user
      environment
    - ~/.xsession-error is truncated if it exists
 - The session script is run (by default, /etc/X11/Xsession) using "sh -l"
 - If the X server or the X session exit, the other is killed and then both are
   restarted.
 - If a session exits too soon, nodm will wait a bit before restarting.  The
   waiting times go as follow:
    - The first time the session exits too soon, restart immediately
    - The second and third time, wait 30 seconds
    - All remaining times, wait 1 minute.
   Once a session lasts long enough, the waiting time goes back to zero.

nodm does NOT currently fork and run in the background like a proper daemon:
most distributions have tools that do that, and nodm plays just fine with them.
This is not a particular design choice: quite simply, so far no one has felt
the need to implement it.


## Configuration

Configuration is made via these environment variables:

 * `NODM_USER`:
    Controls the user that is used to automatically log in.
 * `NODM_X_OPTIONS`:
    X server command line (for example: "vt7 -nolisten tcp").
    It is expanded using wordexp, with tilde expansion, variable substitution,
    arithmetic expansion, wildcard expansion and quote removal, but no command
    substitution. If command substitution is needed, please get in touch
    providing a real-life use case for it.

    If the first optiom starts with '/' or '.', it is used as the X server, else
    "X" is used as the server.

    If the second option (or the first if the first was not recognised as a path
    to the X server) looks like ":<NUMBER>", it is used as the display name, else
    ":0" is used.

    If the command line contains a "vt<N>" virtual terminal indicator, automatic
    VT allocation is switched off. Otherwise, the appropriate vt<N> option is
    appended to the X command line according to the virtual terminal that has
    been allocated.
 * `NODM_MIN_SESSION_TIME`:
    Minimum time (in seconds) that a session should last in order for nodm to
    decide that it has not quit too soon. If an X session runs for less than
    this time, nodm will wait an increasing amount of time before restarting it
    (default: 60).
 * `NODM_XSESSION`:
    X session command (default: /etc/X11/Xsession). It is run using the shell, so
    it can be any shell command.
 * `NODM_XINIT`
    Was used by older versions of nodm as the path to the xinit program, but it
    is now ignored.
 * `NODM_X_TIMEOUT`
    Timeout (in seconds) to wait for X to be ready to accept connections. If X is
    not ready before this timeout, it is killed and restarted.
