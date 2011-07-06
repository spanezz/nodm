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

#ifndef NODM_VT_H
#define NODM_VT_H

/// VT allocation state
struct nodm_vt
{
    /**
     * First VT to try to allocate. -1 means 'no VT allocation'
     */
    int conf_initial_vt;

    /// Number of the VT that has been allocated (-1 for none)
    int num;

    /// File decriptor pointing to the open VT (-1 for none)
    int fd;
};

/// Initialise a vt structure with default values
void nodm_vt_init(struct nodm_vt* vt);

/// Allocate a virtual terminal and keep it open
int nodm_vt_start(struct nodm_vt* vt);

/// Release the virtual terminal
void nodm_vt_stop(struct nodm_vt* vt);

#endif
