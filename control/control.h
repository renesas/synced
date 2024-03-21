/**
 * @file control.h
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

#ifndef CONTROL_H
#define CONTROL_H

#include "sync.h"
#include "../management/management.h"

typedef struct {
  T_esmc_network_option net_opt;
  int no_ql_en;
  int synce_forced_ql_en;
  T_esmc_ql lo_ql;
  int lo_pri;
  T_esmc_ql do_not_use_ql;
  unsigned int hold_off_timer_ms;       /* Milliseconds */
  unsigned int wait_to_restore_timer_s; /* Seconds */
  int num_syncs;
  T_sync_config const *sync_config_array;
} T_control_config;

typedef struct {
  T_esmc_network_option net_opt;
  int no_ql_en;
  int synce_forced_ql_en;
  T_esmc_ql lo_ql;
  int lo_pri;
  T_esmc_ql do_not_use_ql;
  unsigned int hold_off_timer_ms;       /* Milliseconds */
  unsigned int wait_to_restore_timer_s; /* Seconds */
  int num_syncs;
  T_sync_entry *sync_table;
  int update_priority_table_flag;
} T_control_data;

int control_init(T_control_config const *control_config);
void control_update_sync_table(void);
int control_get_sync_idx(int clk_idx);
T_esmc_ql control_get_ql(int sync_idx);
void control_get_sync_name(int sync_idx, char *port_name);
int control_set_forced_ql(const char *port_name, T_esmc_ql forced_ql);
int control_clear_forced_ql(const char *port_name);
int control_get_sync_info_by_index(int sync_idx, T_management_sync_info *sync_info);
int control_get_sync_info_by_name(const char *port_name, T_management_sync_info *sync_info);
int control_clear_synce_clk_wtr_timer(const char *port_name);
int control_update_sync_table_entry_clk_idx(const char *new_port_name, int clk_idx);
int control_set_pri(const char *port_name, int pri);
unsigned long long control_get_tx_bundle_bitmap(int sync_idx);
void control_deinit(void);

#endif /* CONTROL_H */
