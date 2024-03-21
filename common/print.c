/**
 * @file print.c
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
* Release Tag: 2-0-5
* Pipeline ID: 310964
* Commit Hash: b166f770
********************************************************************************************************************/

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "print.h"

static const char *g_print_prog_name = NULL;

static const char *g_msg_tag = NULL;

static int g_pr_level = LOG_INFO;
static int g_stdout_en = 0;
static int g_syslog_en = 0;

void print_set_prog_name(const char *name)
{
  g_print_prog_name = name;
}

void print_set_msg_tag(const char *tag)
{
  if(tag != NULL)
    printf("Set message tag to %s\n", tag);
    
  g_msg_tag = tag;
}

void print_set_max_msg_level(int level)
{
  g_pr_level = level;
}

void print_set_stdout_en(int value)
{
  g_stdout_en = value ? 1 : 0;
}

void print_set_syslog_en(int value)
{
  g_syslog_en = value ? 1 : 0;
}

void print(int level, int timestamp_en, char const *format, ...)
{
  struct timespec ts;
  va_list ap;
  char buf[PRINT_BUFFER_SIZE];
  FILE *fp;

  if(level > g_pr_level)
    return;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  if(g_stdout_en) {
    fp = level >= LOG_NOTICE ? stdout : stderr;
    if(timestamp_en) {
      fprintf(fp, "%s[%lld.%03ld]: %s%s%s\n",
              g_print_prog_name ? g_print_prog_name : "",
              (long long)ts.tv_sec, ts.tv_nsec / 1000000,
              g_msg_tag ? g_msg_tag : "", g_msg_tag ? " " : "",
              buf);
    } else {
      fprintf(fp, "%s",
              buf);
    }
    fflush(fp);
  }
  if(g_syslog_en) {
    if(timestamp_en) {
      syslog(level, "[%lld.%03ld] %s%s%s",
             (long long)ts.tv_sec, ts.tv_nsec / 1000000,
             g_msg_tag ? g_msg_tag : "", g_msg_tag ? " " : "",
             buf);
    } else {
      syslog(level, "%s",
             buf);
    }
  }
}
