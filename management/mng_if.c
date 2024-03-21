/**
 * @file mng_if.c
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

#include "mng_if.h"
#include "../common/print.h"

typedef enum  {
  E_mng_if_thread_state_not_started,
  E_mng_if_thread_state_started,
  E_mng_if_thread_state_stopping,
  E_mng_if_thread_state_stopped
} T_mng_if_thread_state;

typedef struct {
  pthread_t thread_id;
  T_mng_if_thread_state thread_state;
} T_mng_if_thread_data;

/* Static data */

static int g_mng_if_fd = UNINITIALIZED_FD;
T_mng_if_thread_data g_mng_if_thread_data;

/* Static functions */

static int create_new_connection(void)
{
  int new_conn;

  new_conn = accept(g_mng_if_fd, NULL, NULL);
  if(new_conn == UNINITIALIZED_FD) {
    /* Wait for 50 ms */
    usleep(50000);
    return UNINITIALIZED_FD;
  }

  pr_info("Management interface thread accepted connection %d", new_conn);

  return new_conn;
}

static int respond_to_request(int conn_fd)
{
  T_mng_api_request_msg req_msg;
  T_mng_api_response_msg rsp_msg;
  int status;
  struct iovec iov;
  struct msghdr msgh;

  memset(&req_msg, 0, sizeof(req_msg));
  memset(&rsp_msg, 0, sizeof(rsp_msg));
  memset(&iov, 0, sizeof(iov));
  memset(&msgh, 0, sizeof(msgh));

  iov.iov_base = &req_msg;
  iov.iov_len = sizeof(req_msg);

  msgh.msg_iov = &iov;
  msgh.msg_iovlen = 1;

  status = recvmsg(conn_fd, &msgh, 0);
  if(status < 0) {
    if((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
      return conn_fd;
    }

    pr_err("Management interface connection failed");
    close(conn_fd);
    return UNINITIALIZED_FD;
  } else if(status == 0) {
    pr_info("Management interface thread terminated connection %d", conn_fd);
    close(conn_fd);
    return UNINITIALIZED_FD;
  }

  /* Request message received */
  if(req_msg.api_code < E_mng_api_max) {
    pr_info("Management API: %s", conv_api_code_to_str(req_msg.api_code));

    memset(&rsp_msg, 0, sizeof(rsp_msg));

    rsp_msg.api_code = req_msg.api_code;

    switch(rsp_msg.api_code) {
      case E_mng_api_get_sync_info_list:
      {
        int print_flag = req_msg.request_get_sync_info_list.print_flag;
        int max_num_syncs = req_msg.request_get_sync_info_list.max_num_syncs;
        if(max_num_syncs > MAX_SYNC_INFO_STRUCTURES) {
          max_num_syncs = MAX_SYNC_INFO_STRUCTURES;
        }
        rsp_msg.response = management_get_sync_info_list(print_flag,
                                                         max_num_syncs,
                                                         &rsp_msg.response_get_sync_info_list.sync_info_list[0],
                                                         &rsp_msg.response_get_sync_info_list.num_syncs);
      }
      break;

      case E_mng_api_get_current_status:
      {
        int print_flag = req_msg.request_get_current_status.print_flag;
        rsp_msg.response = management_get_current_status(print_flag,
                                                         &rsp_msg.response_get_current_status.current_status);
      }
      break;

      case E_mng_api_get_sync_info:
      {
        int print_flag = req_msg.request_get_sync_info.print_flag;
        const char *port_name = req_msg.request_get_sync_info.port_name;
        rsp_msg.response = management_get_sync_info(print_flag,
                                                    port_name,
                                                    &rsp_msg.response_get_sync_info.sync_info);
      }
      break;

      case E_mng_api_set_forced_ql:
      {
        int print_flag = req_msg.request_set_forced_ql.print_flag;
        const char *port_name = req_msg.request_set_forced_ql.port_name;
        T_esmc_ql forced_ql = req_msg.request_set_forced_ql.forced_ql;
        rsp_msg.response = management_set_forced_ql(print_flag,
                                                    port_name,
                                                    forced_ql);
      }
      break;

      case E_mng_api_clear_forced_ql:
      {
        int print_flag = req_msg.request_clear_forced_ql.print_flag;
        const char *port_name = req_msg.request_clear_forced_ql.port_name;
        rsp_msg.response = management_clear_forced_ql(print_flag,
                                                      port_name);
      }
      break;

      case E_mng_api_clear_holdover_timer:
      {
        int print_flag = req_msg.request_clear_holdover_timer.print_flag;
        rsp_msg.response = management_clear_holdover_timer(print_flag);
      }
      break;

      case E_mng_api_clear_synce_clk_wtr_timer:
      {
        int print_flag = req_msg.request_clear_synce_clk_wtr_timer.print_flag;
        const char *port_name = req_msg.request_clear_synce_clk_wtr_timer.port_name;
        rsp_msg.response = management_clear_synce_clk_wtr_timer(print_flag,
                                                                port_name);
      }
      break;

      case E_mng_api_assign_new_synce_clk_port:
      {
        int print_flag = req_msg.request_assign_new_synce_clk_port.print_flag;
        int clk_idx = req_msg.request_assign_new_synce_clk_port.clk_idx;
        const char *port_name = req_msg.request_assign_new_synce_clk_port.port_name;
        rsp_msg.response = management_assign_new_synce_clk_port(print_flag,
                                                                clk_idx,
                                                                port_name);
      }
      break;

      case E_mng_api_set_pri:
      {
        int print_flag = req_msg.request_set_pri.print_flag;
        const char *port_name = req_msg.request_set_pri.port_name;
        int pri = req_msg.request_set_pri.pri;
        rsp_msg.response = management_set_pri(print_flag,
                                              port_name,
                                              pri);
      }
      break;

      case E_mng_api_set_max_msg_lvl:
      {
        int print_flag = req_msg.request_set_max_msg_lvl.print_flag;
        int max_msg_level = req_msg.request_set_max_msg_lvl.max_msg_lvl;
        rsp_msg.response = management_set_max_msg_level(print_flag,
                                                      max_msg_level);
      }
      break;

      default:
        break;
    }

    send(conn_fd, &rsp_msg, sizeof(rsp_msg), 0);
  }

  return conn_fd;
}

static void *mng_if_thread(void *arg)
{
  volatile T_mng_if_thread_data *thread_data = (volatile T_mng_if_thread_data *)arg;
  int conn_fd = UNINITIALIZED_FD;

  thread_data->thread_state = E_mng_if_thread_state_started;

  while(1) {
    if(thread_data->thread_state == E_mng_if_thread_state_stopping) {
      if(conn_fd != UNINITIALIZED_FD) {
        close(conn_fd);
      }
      thread_data->thread_state = E_mng_if_thread_state_stopped;
      break;
    }

    if(conn_fd == UNINITIALIZED_FD) {
      conn_fd = create_new_connection();
    } else {
      conn_fd = respond_to_request(conn_fd);
    }
  }

  pthread_exit(NULL);
}

static void mng_if_thread_start_wait(T_mng_if_thread_state *state)
{
  const int timeout_us = 2000000;
  const int poll_interval_us = 50000;
  int count = (timeout_us / poll_interval_us) + 1;
  unsigned long long start_time_ms;
  int time_diff_ms;

  start_time_ms = os_get_monotonic_milliseconds();

  while(count-- && (*(volatile T_mng_if_thread_state*)state == E_mng_if_thread_state_not_started)) {
    usleep(poll_interval_us);
  }

  time_diff_ms = os_get_monotonic_milliseconds() - start_time_ms;
  pr_info("Mangement interface started in %d milliseconds", time_diff_ms);
}

static void mng_if_thread_stop_wait(T_mng_if_thread_state *state)
{
  const int timeout_us = 2000000;
  const int poll_interval_us = 50000;
  int count = (timeout_us / poll_interval_us) + 1;

  unsigned long long stop_time_ms;
  int time_diff_ms;

  stop_time_ms = os_get_monotonic_milliseconds();

  *state = E_mng_if_thread_state_stopping;

  while(count-- && (*(volatile T_mng_if_thread_state*)state != E_mng_if_thread_state_stopped)) {
    usleep(poll_interval_us);
  }

  time_diff_ms = os_get_monotonic_milliseconds() - stop_time_ms;
  pr_info("Mangement interface stopped in %d milliseconds", time_diff_ms);
}

/* Global functions */

int mng_if_start(const char *mng_if_ip_addr, int mng_if_port_num)
{
  struct timeval tval;
  int reuse_addr_flag = 1;
  struct sockaddr_in cli_addr;

  /* Set up socket interface */
  if((g_mng_if_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    pr_err("%s: %s", __func__, strerror(errno));
    g_mng_if_fd = UNINITIALIZED_FD;
    return -1;
  }

  /* Set receive timeout to 50 ms */
  tval.tv_sec = 0;
  tval.tv_usec = 50000;
  if(setsockopt(g_mng_if_fd, SOL_SOCKET, SO_RCVTIMEO, (void const *)&tval, sizeof(struct timeval)) < 0) {
    pr_err("%s: %s", __func__, strerror(errno));
    goto err;
  }

  if(setsockopt(g_mng_if_fd, SOL_SOCKET, SO_REUSEADDR, (void const *)&reuse_addr_flag, sizeof(int)) < 0) {
    pr_err("%s: %s", __func__, strerror(errno));
    goto err;
  }

  memset(&cli_addr, 0, sizeof(cli_addr));
  cli_addr.sin_addr.s_addr = inet_addr(mng_if_ip_addr);
  cli_addr.sin_port = htons(mng_if_port_num);
  cli_addr.sin_family = AF_INET;

  if(bind(g_mng_if_fd, (struct sockaddr*)&cli_addr, sizeof(struct sockaddr_in)) < 0) {
    pr_err("%s: %s", __func__, strerror(errno));
    goto err;
  }

  /* Listen to one connection */
  listen(g_mng_if_fd, 1);

  /* Create management interface thread */
  if(os_thread_create(&g_mng_if_thread_data.thread_id, mng_if_thread, (void*)&g_mng_if_thread_data) < 0) {
    goto err;
  }

  mng_if_thread_start_wait(&g_mng_if_thread_data.thread_state);

  return 0;

err:
  close(g_mng_if_fd);
  g_mng_if_fd = UNINITIALIZED_FD;
  return -1;
}

void mng_if_stop(void)
{
  if(g_mng_if_fd != UNINITIALIZED_FD) {
    mng_if_thread_stop_wait(&g_mng_if_thread_data.thread_state);
    pthread_cancel(g_mng_if_thread_data.thread_id);
    close(g_mng_if_fd);
  }

  g_mng_if_fd = UNINITIALIZED_FD;
}
