/**
 * @file management.h
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

#ifndef MANAGEMENT_H
#define MANAGEMENT_H

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "../common/config.h"
#include "../common/os.h"
#include "../common/types.h"
#include "../control/sync.h"
#include "../device/device_adaptor/device_adaptor.h"

typedef enum {
  E_management_api_response_ok,
  E_management_api_response_invalid,
  E_management_api_response_failed,
  E_management_api_response_not_supported
} T_management_api_response;

typedef enum {
  E_alarm_type_invalid_clock_idx,
  E_alarm_type_invalid_sync_idx,
  E_alarm_type_timing_loop,
  E_alarm_type_invalid_rx_ql
} T_alarm_type;

typedef enum {
  E_timing_loop_type_immediate,
  E_timing_loop_type_originator
} T_timing_loop_type;

typedef struct {
  T_timing_loop_type loop_type;
  const char *port_name;
  const unsigned char *mac_addr;
} T_alarm_data_timing_loop;

typedef struct {
  const char *port_name;
} T_alarm_data_invalid_rx_ql;

typedef struct {
  int config_pri;
  T_esmc_ql current_ql;
  T_sync_state state;
  int tx_bundle_num;
  int clk_idx;
  unsigned int remaining_time_ms;                       /* Remaining time in milliseconds in hold-off/wait-to-restore state */
  int current_num_hops;
  int rank;
  T_sync_clk_state clk_state;
  T_device_clk_reference_monitor_status ref_mon_status;
  int rx_timeout_flag;
  int port_link_down_flag;
} T_management_synce_clk_info;

typedef struct {
  int config_pri;
  T_esmc_ql current_ql;
  T_sync_state state;
  int tx_bundle_num;
  unsigned int remaining_time_ms; /* Remaining time in milliseconds in hold-off/wait-to-restore state */
  int current_num_hops;
  int rank;
  int rx_timeout_flag;
  int port_link_down_flag;
} T_management_synce_mon_info;

typedef struct {
  int config_pri;
  T_esmc_ql current_ql;
  T_sync_state state;
  int clk_idx;
  int rank;
  T_sync_clk_state clk_state;
  T_device_clk_reference_monitor_status ref_mon_status;
} T_management_ext_clk_info;

typedef struct {
  int tx_bundle_num;
  int port_link_down_flag;
} T_management_synce_tx_only_info;

typedef struct {
  char name[INTERFACE_MAX_NAME_LEN];
  T_sync_type type;
  union {
    T_management_synce_clk_info synce_clk_info;
    T_management_synce_mon_info synce_mon_info;
    T_management_ext_clk_info ext_clk_info;
    T_management_synce_tx_only_info synce_tx_only_info;
  };
} T_management_sync_info;

typedef struct {
  T_esmc_ql current_ql;
  char port_name[INTERFACE_MAX_NAME_LEN];
  int clk_idx;
  T_device_dpll_state dpll_state;
  unsigned int holdover_remaining_time_ms;
} T_management_status;

typedef struct {
  T_alarm_type alarm_type;
  union {
    T_alarm_data_timing_loop alarm_timing_loop;
    T_alarm_data_invalid_rx_ql alarm_invalid_ql;
  };
} T_alarm_data;

typedef void (*T_management_current_ql_change_cb)(const char *port_name, T_esmc_ql current_ql);
typedef void (*T_management_sync_current_ql_change_cb)(const char *port_name, T_esmc_ql current_ql, int rank);
typedef void (*T_management_synce_dpll_current_state_change_cb)(T_device_dpll_state synce_dpll_state);
typedef void (*T_management_sync_current_clk_state_change_cb)(const char *port_name, int clk_idx, T_sync_clk_state clk_state);
typedef void (*T_management_sync_current_state_change_cb)(const char *port_name, T_sync_state state);
typedef void (*T_management_alarm_cb)(const T_alarm_data *alarm_data);
typedef void (*T_management_pcm4l_connection_status_change_cb)(int on);

typedef struct {
  T_management_current_ql_change_cb notify_current_ql;
  T_management_sync_current_ql_change_cb notify_sync_current_ql;
  T_management_synce_dpll_current_state_change_cb notify_synce_dpll_current_state;
  T_management_sync_current_clk_state_change_cb notify_sync_current_clk_state;
  T_management_sync_current_state_change_cb notify_sync_current_state;
  T_management_alarm_cb notify_alarm;
  T_management_pcm4l_connection_status_change_cb notify_pcm4l_connection_status;
} T_management_callbacks;

int management_init(void);
int management_deinit(void);

/* Callback management APIs */

void management_call_notify_current_ql_cb(const char *port_name,
                                          T_esmc_ql current_ql);

void management_call_notify_sync_current_ql_cb(const char *port_name,
                                               T_esmc_ql current_ql,
                                               int rank);

void management_call_notify_synce_dpll_current_state_cb(T_device_dpll_state synce_dpll_state);

void management_call_notify_sync_current_clk_state_cb(const char *port_name,
                                                      int clk_idx,
                                                      T_sync_clk_state clk_state);

void management_call_notify_sync_current_state_cb(const char *port_name,
                                                  T_sync_state state);

void management_call_notify_alarm_cb(const T_alarm_data *alarm_data);

void management_call_notify_pcm4l_connection_status_cb(int on);

/* Generic management APIs */

/*
 * Get sync information list of all Sync-E clock, Sync-E monitoring, external clock, and Sync-E TX only ports
 *
 * The user is responsible for allocating and deallocating the memory used to store the list.
 * max_num_syncs indicates the maximum number of list entries to be returned.
 * print_flag indicates whether to print the retrieved information.
 * sync_info_list is a pointer to the sync information list.
 * Returns the number of sync information list entries that were returned.
 */
T_management_api_response management_get_sync_info_list(int print_flag,
                                                        int max_num_syncs,
                                                        T_management_sync_info *sync_info_list,
                                                        int *num_syncs);

/*
 * Get current QL, selected port name, selected clock index, Sync-E DPLL state and holdover remaining time
 */
T_management_api_response management_get_current_status(int print_flag,
                                                        T_management_status *status);

/* Get sync information for specified Sync-E clock, Sync-E monitoring, external clock, or Sync-E TX only port name */
T_management_api_response management_get_sync_info(int print_flag,
                                                   const char *port_name,
                                                   T_management_sync_info *sync_info);

/*
 * Set forced QL for specified Sync-E clock, Sync-E monitoring, or external clock port name
 *
 * Until management_clear_forced_ql() is called, the QL of the specified port will be fixed.
 * For Sync-E clock and monitoring ports, this means even if an ESMC event occurs (e.g. QL change, RX timeout, or port link down event), the QL cannot change.
 */
T_management_api_response management_set_forced_ql(int print_flag,
                                                   const char *port_name,
                                                   T_esmc_ql forced_ql);

/*
 * Clear forced QL for specified Sync-E clock, Sync-E monitoring, or external clock port name
 *
 * The QL will revert to configured QL.
 * For Sync-E clock and monitoring ports, this means if an ESMC event occurs (e.g. QL change, RX timeout, or port link down event), the QL can change.
 */
T_management_api_response management_clear_forced_ql(int print_flag,
                                                     const char *port_name);

/* Clear holdover timer */
T_management_api_response management_clear_holdover_timer(int print_flag);

/* Clear wait-to-restore timer for specified Sync-E clock port name */
T_management_api_response management_clear_synce_clk_wtr_timer(int print_flag,
                                                               const char *port_name);

/* Assign new Sync-E clock port for specified clock index */
T_management_api_response management_assign_new_synce_clk_port(int print_flag,
                                                               int clk_idx,
                                                               const char *port_name);

/* Set priority for specified port name */
T_management_api_response management_set_pri(int print_flag,
                                             const char *port_name,
                                             int pri);

/* Set max message level (see print.h for message levels) */
T_management_api_response management_set_max_msg_level(int print_flag,
                                                       int max_msg_lvl);

#endif /* MANAGEMENT_H */
