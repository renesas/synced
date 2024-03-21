/**
 * @file mng_if.h
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

#ifndef MNG_IF_H
#define MNG_IF_H

#include "../common/common.h"
#include "management.h"

#define MAX_SYNC_INFO_STRUCTURES   MAX_NUM_OF_SYNC_ENTRIES

/* CLI request structures */
typedef struct {
  int print_flag;
  int max_num_syncs;
} T_mng_api_request_get_sync_info_list;

typedef struct {
  int print_flag;
} T_mng_api_request_get_current_status;

typedef struct {
  int print_flag;
  char port_name[INTERFACE_MAX_NAME_LEN];
} T_mng_api_request_get_sync_info;

typedef struct {
  int print_flag;
  char port_name[INTERFACE_MAX_NAME_LEN];
  T_esmc_ql forced_ql;
} T_mng_api_request_set_forced_ql;

typedef struct {
  int print_flag;
  char port_name[INTERFACE_MAX_NAME_LEN];
} T_mng_api_request_clear_forced_ql;

typedef struct {
  int print_flag;
} T_mng_api_request_clear_holdover_timer;

typedef struct {
  int print_flag;
  char port_name[INTERFACE_MAX_NAME_LEN];
} T_mng_api_request_clear_synce_clk_wtr_timer;

typedef struct {
  int print_flag;
  char port_name[INTERFACE_MAX_NAME_LEN];
  int clk_idx;
} T_mng_api_request_assign_new_synce_clk_port;

typedef struct {
  int print_flag;
  char port_name[INTERFACE_MAX_NAME_LEN];
  int pri;
} T_mng_api_request_set_pri;

typedef struct {
  int print_flag;
  int max_msg_lvl;
} T_mng_api_request_set_max_msg_lvl;

/* CLI request message */
typedef struct {
  T_mng_api api_code;
  union {
    T_mng_api_request_get_sync_info_list           request_get_sync_info_list;
    T_mng_api_request_get_current_status           request_get_current_status;
    T_mng_api_request_get_sync_info                request_get_sync_info;
    T_mng_api_request_set_forced_ql                request_set_forced_ql;
    T_mng_api_request_clear_forced_ql              request_clear_forced_ql;
    T_mng_api_request_clear_holdover_timer         request_clear_holdover_timer;
    T_mng_api_request_clear_synce_clk_wtr_timer    request_clear_synce_clk_wtr_timer;
    T_mng_api_request_assign_new_synce_clk_port    request_assign_new_synce_clk_port;
    T_mng_api_request_set_pri                      request_set_pri;
    T_mng_api_request_set_max_msg_lvl              request_set_max_msg_lvl;
  };
} T_mng_api_request_msg;

/* CLI response structures */
typedef struct {
  int num_syncs;
  T_management_sync_info sync_info_list[MAX_SYNC_INFO_STRUCTURES];
} T_mng_api_response_get_sync_info_list;

typedef struct {
  T_management_status current_status;
} T_mng_api_response_get_current_status;

typedef struct {
  T_management_sync_info sync_info;
} T_mng_api_response_get_sync_info;

/* CLI response message */
typedef struct {
  T_mng_api api_code;
  T_management_api_response response;
  union {
    T_mng_api_response_get_sync_info_list           response_get_sync_info_list;
    T_mng_api_response_get_current_status           response_get_current_status;
    T_mng_api_response_get_sync_info                response_get_sync_info;

    /* No data for following APIs:
     *   - set_forced_ql
     *   - clear_forced_ql
     *   - clear_synce_clk_wtr_timer
     *   - assign_new_synce_clk_port
     *   - set_pri
     *   - set_max_msg_lvl
     */
  };
} T_mng_api_response_msg;

int mng_if_start(const char *mng_if_ip_addr, int mng_if_port_num);
void mng_if_stop(void);

#endif  /* MNG_IF_H */
