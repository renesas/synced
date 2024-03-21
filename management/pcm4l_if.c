/**
 * @file pcm4l_if.c
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

#include <arpa/inet.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "pcm4l_if.h"
#include "../common/print.h"

typedef enum  {
  E_pcm4l_if_thread_state_not_started,
  E_pcm4l_if_thread_state_started,
  E_pcm4l_if_thread_state_connected,
  E_pcm4l_if_thread_state_stopping,
  E_pcm4l_if_thread_state_stopped
} T_pcm4l_if_thread_state;

typedef struct {
  int enabled;
  pthread_t thread_id;
  struct sockaddr_in pcm4l_server;
  T_pcm4l_if_thread_state thread_state;
} T_pcm4l_if_thread_data;

/* Static data */

static int g_pcm4l_if_fd = UNINITIALIZED_FD;
static pthread_mutex_t g_pcm4l_mutex;
static T_pcm4l_if_thread_data g_pcm4l_if_thread_data;

/* Static functions */

static int connect_to_pcm4l(void)
{
  if(connect(g_pcm4l_if_fd, (struct sockaddr *)&g_pcm4l_if_thread_data.pcm4l_server, sizeof(struct sockaddr_in)) < 0 ) {
    return -1;
  }

  pr_info("pcm4l interface is connected");
  return 0;
}

static int check_connection(void)
{
  unsigned char flush_buff[1024];
  int status;

  status = recv(g_pcm4l_if_fd, flush_buff, sizeof(flush_buff), MSG_DONTWAIT);
  if((status < 0) && ((errno == EWOULDBLOCK) || (errno == EAGAIN))) {
    /* On */
    return 1;
  }

  if(status <= 0) {
    /* Off */
    return 0;
  }

  /* Flush the socket */
  while(status == sizeof(flush_buff)) {
    status = recv(g_pcm4l_if_fd, flush_buff, sizeof(flush_buff), MSG_DONTWAIT);
  }

  /* On */
  return 1;
}

static void *pcm4l_if_thread(void *arg)
{
  volatile T_pcm4l_if_thread_data *thread_data = (volatile T_pcm4l_if_thread_data *)arg;
  int conn_on = 0;
  int conn_status;
  struct timeval tval;
  int status_changed_flag;

  thread_data->thread_state = E_pcm4l_if_thread_state_started;

  while(1) {
    if(thread_data->thread_state == E_pcm4l_if_thread_state_stopping) {
      thread_data->thread_state = E_pcm4l_if_thread_state_stopped;
      break;
    }

    status_changed_flag = 0;

    os_mutex_lock(&g_pcm4l_mutex);

    if(thread_data->thread_state == E_pcm4l_if_thread_state_started) {
      if(g_pcm4l_if_fd == UNINITIALIZED_FD) {
        /* Set up socket interface */
        if((g_pcm4l_if_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
          pr_err("%s: %s", __func__, strerror(errno));
          g_pcm4l_if_fd = UNINITIALIZED_FD;
        }
      }

      if(g_pcm4l_if_fd != UNINITIALIZED_FD) {
        /* Set receive timeout to 50 ms */
        tval.tv_sec = 0;
        tval.tv_usec = 50000;
        if(setsockopt(g_pcm4l_if_fd, SOL_SOCKET, SO_RCVTIMEO, (void const *)&tval, sizeof(struct timeval)) < 0) {
          pr_err("%s: %s", __func__, strerror(errno));
          close(g_pcm4l_if_fd);
          g_pcm4l_if_fd = UNINITIALIZED_FD;
        } else if(connect_to_pcm4l() == 0) {
          conn_status = check_connection();
          if(conn_status) {
            conn_on = 1;
            status_changed_flag = 1;
            thread_data->thread_state = E_pcm4l_if_thread_state_connected;
          }
        }
      }
    } else if(thread_data->thread_state == E_pcm4l_if_thread_state_connected) {
      conn_status = check_connection();
      if(conn_status == 0) {
        conn_on = 0;
        status_changed_flag = 1;
        thread_data->thread_state = E_pcm4l_if_thread_state_started;
        close(g_pcm4l_if_fd);
        g_pcm4l_if_fd = UNINITIALIZED_FD;
      }
    }

    os_mutex_unlock(&g_pcm4l_mutex);

    if(status_changed_flag) {
      management_call_notify_pcm4l_connection_status_cb(conn_on);
    }

    /* Run every 100 ms */
    usleep(100000);
  }

  pthread_exit(NULL);
}

static void pcm4l_if_thread_start_wait(T_pcm4l_if_thread_state *state)
{
  const int timeout_us = 2000000;
  const int poll_interval_us = 50000;
  int count = (timeout_us / poll_interval_us) + 1;
  unsigned long long start_time_ms;
  int time_diff_ms;

  start_time_ms = os_get_monotonic_milliseconds();

  while(count-- && (*(volatile T_pcm4l_if_thread_state*)state == E_pcm4l_if_thread_state_not_started)) {
    usleep(poll_interval_us);
  }

  time_diff_ms = os_get_monotonic_milliseconds() - start_time_ms;
  pr_info("pcm4l interface started in %d milliseconds", time_diff_ms);
}

static void pcm4l_if_thread_stop_wait(T_pcm4l_if_thread_state *state)
{
  const int timeout_us = 2000000;
  const int poll_interval_us = 50000;
  int count = (timeout_us / poll_interval_us) + 1;

  unsigned long long stop_time_ms;
  int time_diff_ms;

  stop_time_ms = os_get_monotonic_milliseconds();

  os_mutex_lock(&g_pcm4l_mutex);
  *state = E_pcm4l_if_thread_state_stopping;
  os_mutex_unlock(&g_pcm4l_mutex);

  while(count-- && (*(volatile T_pcm4l_if_thread_state*)state != E_pcm4l_if_thread_state_stopped)) {
    usleep(poll_interval_us);
  }

  time_diff_ms = os_get_monotonic_milliseconds() - stop_time_ms;
  pr_info("pcm4l interface stopped in %d milliseconds", time_diff_ms);
}

/* Global functions */

int pcm4l_if_start(const char *pcm4l_if_ip_addr, int pcm4l_if_port_num)
{
  memset(&g_pcm4l_if_thread_data.pcm4l_server, 0, sizeof(struct sockaddr_in));
  g_pcm4l_if_thread_data.pcm4l_server.sin_addr.s_addr = inet_addr(pcm4l_if_ip_addr);
  g_pcm4l_if_thread_data.pcm4l_server.sin_port = htons(pcm4l_if_port_num);
  g_pcm4l_if_thread_data.pcm4l_server.sin_family = AF_INET;

  /* Create pcm4l interface thread */
  if(os_thread_create(&g_pcm4l_if_thread_data.thread_id, pcm4l_if_thread, (void*)&g_pcm4l_if_thread_data) < 0) {
    goto err;
  }

  /* Initialize mutex */
  if(os_mutex_init(&g_pcm4l_mutex) < 0) {
    goto err;
  }

  g_pcm4l_if_thread_data.enabled = 1;
  pcm4l_if_thread_start_wait(&g_pcm4l_if_thread_data.thread_state);

  return 0;

err:
  close(g_pcm4l_if_fd);
  g_pcm4l_if_fd = UNINITIALIZED_FD;
  return -1;
}

void pcm4l_if_stop(void)
{
  if(g_pcm4l_if_thread_data.enabled != 0) {
    pcm4l_if_thread_stop_wait(&g_pcm4l_if_thread_data.thread_state);
    /* Deinitialize mutex */
    os_mutex_deinit(&g_pcm4l_mutex);
    pthread_cancel(g_pcm4l_if_thread_data.thread_id);

    if(g_pcm4l_if_fd != UNINITIALIZED_FD) {
      close(g_pcm4l_if_fd);
      g_pcm4l_if_fd = UNINITIALIZED_FD;
    }
  }
}

/*
 * Function to send a request to pcm4l.
 * If the caller expects a response, then it must provide a response buffer and the maximum response length.
 */
T_pcm4l_error_code pcm4l_if_send(const unsigned char *req_buff, unsigned int req_len, unsigned char *rsp_buff, unsigned int max_rsp_len)
{
  unsigned char flush_buff[1024];
  int status;
  T_pcm4l_error_code ret;

  if(g_pcm4l_if_thread_data.enabled == 0) {
    return E_pcm4l_error_code_not_sent;
  }

  os_mutex_lock(&g_pcm4l_mutex);

  if(g_pcm4l_if_thread_data.thread_state != E_pcm4l_if_thread_state_connected) {
    os_mutex_unlock(&g_pcm4l_mutex);
    pr_warning("%s: Cannot send message to pcm4l", __func__);
    return E_pcm4l_error_code_not_sent;
  }

  /* Discard old messages */
  do {
    status = recv(g_pcm4l_if_fd, flush_buff, sizeof(flush_buff), MSG_DONTWAIT);
  } while(status > 0);

  /* Send the message */
  if(send(g_pcm4l_if_fd, req_buff, req_len, 0) != req_len) {
    os_mutex_unlock(&g_pcm4l_mutex);
    pr_warning("%s: Failed to send message to pcm4l", __func__);
    return E_pcm4l_error_code_fail;
  }

  ret = E_pcm4l_error_code_ok;

  if((rsp_buff != NULL) && (max_rsp_len > 0)) {
    status = recv(g_pcm4l_if_fd, rsp_buff, max_rsp_len, 0);
    if(status == 0) {
      ret = E_pcm4l_error_code_timeout;
    } else if(status < 0) {
      ret = E_pcm4l_error_code_fail;
    }
  }

  os_mutex_unlock(&g_pcm4l_mutex);

  return ret;
}
