/**
 * @file os.h
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

#ifndef OS_H
#define OS_H

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

int os_mutex_init(pthread_mutex_t *mutex);
int os_mutex_lock(pthread_mutex_t *mutex);
int os_mutex_unlock(pthread_mutex_t *mutex);
int os_mutex_deinit(pthread_mutex_t *mutex);

unsigned long long os_get_monotonic_milliseconds(void);

int os_thread_create(pthread_t *thread, void *(*start_routine) (void *), void *arg);

int os_cond_init(pthread_cond_t *cond);
int os_cond_timed_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, unsigned int timeout_ms, int *timeout_flag);
int os_cond_broadcast(pthread_cond_t *cond);
int os_cond_deinit(pthread_cond_t *cond);

#endif /* OS_H */
