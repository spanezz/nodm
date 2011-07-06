/*
 * test - nodm test utilities
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

#include "test.h"
#include "common.h"
#include <string.h>
#include <stdio.h>

void test_start(const char* testname)
{
    static struct log_config cfg;
    cfg.program_name = testname,
    cfg.log_to_syslog = false,
    cfg.log_to_stderr = true,
    cfg.info_to_stderr = true,
    log_start(&cfg);
}

void test_fail()
{
    log_end();
    exit(1);
}

void test_ok()
{
    log_end();
    exit(0);
}

void ensure_equals(const char* a, const char* b)
{
    if (a == NULL && b == NULL)
        return;
    if (a == NULL || b == NULL || strcmp(a, b) != 0)
    {
        log_warn("strings differ: \"%s\" != \"%s\"", a, b);
        test_fail();
    }
}

void _ensure_succeeds(int code, const char* file, int line, const char* desc)
{
    if (code != E_SUCCESS)
    {
        log_err("%s:%d:%s failed with code %d (%s)", file, line, desc, code, nodm_strerror(code));
        test_fail();
    }
}
