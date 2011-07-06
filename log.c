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

static const struct log_config* config = 0;

void log_config_init(struct log_config* conf)
{
    conf->program_name = 0;
    conf->log_to_syslog = true;
    conf->log_to_stderr = true;
    conf->info_to_stderr = false;
    conf->verbose = false;
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

void log_verbose(const char* fmt, ...)
{
    if (!config->verbose) return;

    va_list ap;

    if (config->info_to_stderr)
    {
        fprintf(stderr, "%s:", config->program_name);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
    }

    if (config->log_to_syslog)
    {
        va_start(ap, fmt);
        vsyslog(LOG_WARNING, fmt, ap);
        va_end(ap);
    }
}

void log_info(const char* fmt, ...)
{
    va_list ap;

    if (config->info_to_stderr)
    {
        fprintf(stderr, "%s:", config->program_name);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
    }

    if (config->log_to_syslog)
    {
        va_start(ap, fmt);
        vsyslog(LOG_WARNING, fmt, ap);
        va_end(ap);
    }
}

void log_warn(const char* fmt, ...)
{
    va_list ap;

    if (config->log_to_stderr)
    {
        fprintf(stderr, "%s:", config->program_name);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
    }

    if (config->log_to_syslog)
    {
        va_start(ap, fmt);
        vsyslog(LOG_WARNING, fmt, ap);
        va_end(ap);
    }
}

void log_err(const char* fmt, ...)
{
    va_list ap;

    if (config->log_to_stderr)
    {
        fprintf(stderr, "%s:", config->program_name);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
    }

    if (config->log_to_syslog)
    {
        va_start(ap, fmt);
        vsyslog(LOG_ERR, fmt, ap);
        va_end(ap);
    }
}
