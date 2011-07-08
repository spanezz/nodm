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

#ifndef NODM_TEST_H
#define NODM_TEST_H

#include "log.h"
#include <stdlib.h>
#include <stdbool.h>

struct nodm_display_manager;

/// Setup the logging system for a test script
void test_start(const char* testname, bool verbose);

/// exit() the program reporting a test failure
void test_fail() __attribute__((noreturn));

/// exit() the program reporting a success
void test_ok() __attribute__((noreturn));

/**
 * Setup DM for tests
 *
 * If running as root and no $DISPLAY is set, tries to start X, else tries to
 * start xnest.
 *
 * @param xcmdline
 *   Can be NULL for the appropriate default.
 */
void test_setup_dm(struct nodm_display_manager* dm, const char* xcmdline);

/// Ensure that two strings are the same
void ensure_equals(const char* a, const char* b);

/// Ensure that two strings are different
void ensure_not_equals(const char* a, const char* b);

/// Ensure that two integers are the same
void ensure_equali(int a, int b);

#define ensure_succeeds(val) _ensure_succeeds((val), __FILE__, __LINE__, #val)
void _ensure_succeeds(int code, const char* file, int line, const char* desc);

#endif
