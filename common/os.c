/**
 * @file os.c
 * @note Copyright (C) [2021-2024] Renesas Electronics Corporation and/or its affiliates
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
/********************************************************************************************************************
* Release Tag: 2-0-5
* Pipeline ID: 310964
* Commit Hash: b166f770
********************************************************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "os.h"
#include "print.h"
#include "types.h"

/* Static functions */

/* Global functions */

int os_mutex_init(pthread_mutex_t *mutex)
{
  pthread_mutexattr_t attr;

  if(pthread_mutexattr_init(&attr) != 0) {
    pr_err("Mutex attribute initialization failed: %s", strerror(errno));
    return -1;
  }

  if(pthread_mutex_init(mutex, &attr) != 0) {
    pr_err("Mutex initialization failed: %s", strerror(errno));
    pthread_mutexattr_destroy(&attr);
    return -1;
  }

  pthread_mutexattr_destroy(&attr);

  return 0;
}

int os_mutex_lock(pthread_mutex_t *mutex)
{
  if(pthread_mutex_lock(mutex) != 0) {
    pr_err("Mutex lock failed: %s", strerror(errno));
    return -1;
  }

  return 0;
}

int os_mutex_unlock(pthread_mutex_t *mutex)
{
  if(pthread_mutex_unlock(mutex) != 0) {
    pr_err("Mutex unlock failed: %s", strerror(errno));
    return -1;
  }

  return 0;
}

int os_mutex_deinit(pthread_mutex_t *mutex)
{
  os_mutex_lock(mutex);
  os_mutex_unlock(mutex);
  pthread_mutex_destroy(mutex);

  return 0;
}

unsigned long long os_get_monotonic_milliseconds(void)
{
  struct timespec current_time;
  unsigned long long monotonic_milliseconds;

  clock_gettime(CLOCK_MONOTONIC, &current_time);
  monotonic_milliseconds = ((current_time.tv_sec * 1000) + (current_time.tv_nsec / 1000000));
  return monotonic_milliseconds;
}

int os_thread_create(pthread_t *thread, void *(*start_routine) (void *), void *arg)
{
  pthread_attr_t attr;

  if(pthread_attr_init(&attr) != 0) {
    pr_err("Thread attribute initialization failed: %s", strerror(errno));
    return -1;
  }

  if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
    pr_err("Thread attribute set detach state failed: %s", strerror(errno));
    pthread_attr_destroy(&attr);
    return -1;
  }

  if(pthread_create(thread, &attr, start_routine, arg) != 0) {
    pr_err("Thread creation failed: %s", strerror(errno));
    pthread_attr_destroy(&attr);
    return -1;
  }

  pthread_attr_destroy(&attr);

  return 0;
}

int os_cond_init(pthread_cond_t *cond)
{
  pthread_condattr_t attr;

  if(pthread_condattr_init(&attr) != 0) {
    pr_err("Condition attribute initialization failed: %s", strerror(errno));
    return -1;
  }

  if(pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) != 0) {
    pr_err("Condition attribute set clock to monotonic time failed: %s", strerror(errno));
    pthread_condattr_destroy(&attr);
    return -1;
  }

  if(pthread_cond_init(cond, &attr) != 0) {
    pr_err("Condition initialization failed: %s", strerror(errno));
    pthread_condattr_destroy(&attr);
    return -1;
  }

  pthread_condattr_destroy(&attr);

  return 0;
}

int os_cond_timed_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, unsigned int timeout_ms, int *timeout_flag)
{
  int err;
  struct timespec time;

  clock_gettime(CLOCK_MONOTONIC, &time);

  time.tv_sec += timeout_ms / 1000;
  time.tv_nsec += (timeout_ms % 1000) * 1000000;
  if(time.tv_nsec >= 1000000000) {
    time.tv_nsec -= 1000000000;
    time.tv_sec++;
  }

  *timeout_flag = 0;

  err = pthread_cond_timedwait(cond, mutex, &time);

  if(err == ETIMEDOUT) {
    *timeout_flag = 1;
    return 0;
  } else if(err == 0) {
    return 0;
  }

  pr_err("Condition timed wait failed: %s", strerror(errno));
  return -1;
}

int os_cond_broadcast(pthread_cond_t *cond)
{
  if(pthread_cond_broadcast(cond) != 0) {
    pr_err("Condition broadcast failed: %s", strerror(errno));
    return -1;
  }

  return 0;
}

int os_cond_deinit(pthread_cond_t *cond)
{
  pthread_cond_destroy(cond);

  return 0;
}
