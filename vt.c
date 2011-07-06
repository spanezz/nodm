/*
 * vt - VT allocation
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

#include "vt.h"
#include "common.h"
#include "log.h"
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/vt.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * Issue a VT_GETSTATE ioctl on the given device, if it supports it.
 *
 * @return true if it succeeded, false otherwise
 */
static bool try_vtstate(const char* dev, struct vt_stat* vtstat)
{
    bool res = false;
    int fd = open(dev, O_WRONLY | O_NOCTTY, 0);
    if (fd < 0)
        goto cleanup;
    if (ioctl (fd, VT_GETSTATE, vtstat) < 0)
        goto cleanup;
    res = true;

cleanup:
    if (fd >= 0) close(fd);
    return res;
}

// Find a device that supports the VT_GETSTATE ioctl
static bool get_vtstate(struct vt_stat* vtstat)
{
    if (try_vtstate("/dev/tty", vtstat)) return true;
    if (try_vtstate("/dev/tty0", vtstat)) return true;
    if (try_vtstate("/dev/console", vtstat)) return true;
    return false;
}

void nodm_vt_init(struct nodm_vt* vt)
{
    vt->conf_initial_vt = strtol(getenv_with_default("NODM_FIRST_VT", "7"), NULL, 10);
    vt->fd = -1;
    vt->num = -1;
}

int nodm_vt_start(struct nodm_vt* vt)
{
    if (vt->conf_initial_vt == -1)
        return E_SUCCESS;

    int vtnum = vt->conf_initial_vt;

    struct vt_stat vtstat;
    if (!get_vtstate(&vtstat))
    {
        log_err("cannot find or open the console");
        return E_VT_ALLOC_ERROR;
    }

    unsigned short vtmask;
    for (vtmask = 1 << vtnum; vtstat.v_state & vtmask; ++vtnum, vtmask <<= 1)
        ;
    if (!vtmask)
    {
        log_err("all VTs seem to be busy");
        return E_VT_ALLOC_ERROR;
    }

    char vtname[15];
    snprintf(vtname, 15, "/dev/tty%d", vtnum);

    vt->fd = open(vtname, O_RDWR | O_NOCTTY, 0);
    if (vt->fd < 0)
    {
        log_err("cannot open %s: %m", vtname);
        return E_OS_ERROR;
    }

    vt->num = vtnum;

    return E_SUCCESS;
}

void nodm_vt_stop(struct nodm_vt* vt)
{
    if (vt->fd != -1)
    {
        close(vt->fd);
        vt->fd = -1;
        vt->num = -1;
    }
}
