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

    /// Log to syslog
    bool log_to_syslog;

    /// Log to stderr
    bool log_to_stderr;

    /**
     * Log level:
     *  1. errors
     *  2. warnings
     *  3. info
     *  4. verbose
     */
    enum {
        NODM_LL_ERR = 1,
        NODM_LL_WARN = 2,
        NODM_LL_INFO = 3,
        NODM_LL_VERB = 4
    }  log_level;
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

// Note: all log functions return true if the message was logged, false
// otherwise. This can be used to test for log levels: if (log_verb(NULL)) { /*
// complex debugging code */ }

/// Log a message about the trivial normal progress of things
bool log_verb(const char* fmt, ...);

/// Log a message about the relevant normal progress of things
bool log_info(const char* fmt, ...);

/// Log a warning message
bool log_warn(const char* fmt, ...);

/// Log an error message
bool log_err(const char* fmt, ...);

#endif
