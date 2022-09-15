/**
 * @file print.h
 * @brief Logging support functions
 * @note Copyright (C) 2011 Richard Cochran <richardcochran@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/********************************************************************************************************************
* Release Tag: 1-0-3
* Pipeline ID: 123302
* Commit Hash: 0d4d9ea7
********************************************************************************************************************/

#ifndef PRINT_H
#define PRINT_H

#include <syslog.h>

#define PRINT_LEVEL_MIN   LOG_EMERG
#define PRINT_LEVEL_MAX   LOG_DEBUG

#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
void print(int level, char const *format, ...);

#define pr_emerg(...)     print(LOG_EMERG, __VA_ARGS__)
#define pr_alert(...)     print(LOG_ALERT, __VA_ARGS__)
#define pr_crit(...)      print(LOG_CRIT, __VA_ARGS__)
#define pr_err(...)       print(LOG_ERR, __VA_ARGS__)
#define pr_warning(...)   print(LOG_WARNING, __VA_ARGS__)
#define pr_notice(...)    print(LOG_NOTICE, __VA_ARGS__)
#define pr_info(...)      print(LOG_INFO, __VA_ARGS__)
#define pr_debug(...)     print(LOG_DEBUG, __VA_ARGS__)

void print_set_prog_name(const char *name);
void print_set_msg_tag(const char *tag);
void print_set_max_msg_level(int level);
void print_set_stdout_en(int value);
void print_set_syslog_en(int value);

#endif /* PRINT_H */
