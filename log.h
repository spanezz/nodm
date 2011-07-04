/*
 * log - print and/or log messages
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

#ifndef NODM_LOG_H
#define NODM_LOG_H

#include <stdbool.h>

/// Logging configuration
struct log_config
{
    /// The program name to use in error messages
    const char* program_name;

    /// Log all messages to syslog
    bool log_to_syslog;

    /// Log warnings and errors to stderr
    bool log_to_stderr;

    /// Also log info messages to stderr
    bool info_to_stderr;
};

/**
 * Initialise log configuration with default values
 */
void log_config_init(struct log_config* conf);

/**
 * Start the logging system
 *
 * @param conf
 *   The logging configuration. Note that it only stores a copy of the pointer,
 *   but does not make a copy of the structure, so you need to make sure that
 *   the string remains valid until log_end() is called.
 */
void log_start(const struct log_config* conf);

/**
 * Shut down the logging system.
 *
 * This should also be called after each fork.
 */
void log_end();

void log_info(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_err(const char* fmt, ...);

#endif
