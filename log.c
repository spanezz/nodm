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

#include "log.h"
#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

static const struct log_config* config = 0;

void log_config_init(struct log_config* conf)
{
    conf->program_name = 0;
    conf->log_to_syslog = true;
    conf->log_to_stderr = true;
    conf->log_level = LOG_INFO;
}

void log_start(const struct log_config* conf)
{
    config = conf;

    if (config->log_to_syslog)
        openlog("nodm", LOG_PID, LOG_AUTHPRIV);
}

void log_end()
{
    if (config->log_to_syslog)
        closelog();
}

static void log_common(int prio, const char* fmt, va_list ap)
{
    if (config->log_to_stderr)
    {
        va_list loc;
        va_copy(loc, ap);
        struct timeval now;
        gettimeofday(&now, NULL);
        fprintf(stderr, "%u.%u %s:",
               (unsigned)now.tv_sec,
               (unsigned)now.tv_usec,
               config->program_name);
        vfprintf(stderr, fmt, loc);
        va_end(loc);
        fputc('\n', stderr);
    }

    if (config->log_to_syslog)
    {
        va_list loc;
        va_copy(loc, ap);
        vsyslog(prio, fmt, loc);
        va_end(loc);
    }
}

bool log_verb(const char* fmt, ...)
{
    if (config->log_level < NODM_LL_VERB) return false;
    if (fmt == NULL) return true;

    va_list ap;
    va_start(ap, fmt);
    log_common(LOG_INFO, fmt, ap);
    va_end(ap);
    return true;
}

bool log_info(const char* fmt, ...)
{
    if (config->log_level < NODM_LL_INFO) return false;
    if (fmt == NULL) return true;

    va_list ap;
    va_start(ap, fmt);
    log_common(LOG_NOTICE, fmt, ap);
    va_end(ap);
    return true;
}

bool log_warn(const char* fmt, ...)
{
    if (config->log_level < NODM_LL_WARN) return false;
    if (fmt == NULL) return true;

    va_list ap;
    va_start(ap, fmt);
    log_common(LOG_WARNING, fmt, ap);
    va_end(ap);
    return true;
}

bool log_err(const char* fmt, ...)
{
    if (config->log_level < NODM_LL_ERR) return false;
    if (fmt == NULL) return true;

    va_list ap;
    va_start(ap, fmt);
    log_common(LOG_ERR, fmt, ap);
    va_end(ap);
    return true;
}
