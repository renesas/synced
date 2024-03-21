/**
 * @file sync.h
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

#ifndef SYNC_H
#define SYNC_H

#include "../common/types.h"
#include "../device/device_adaptor/device_adaptor.h"

typedef enum {
  E_sync_clk_state_unqualified, /* Sync is not qualified by reference monitor */
  E_sync_clk_state_qualified,   /* Sync is qualified by reference monitor */
  E_sync_clk_state_max
} T_sync_clk_state;

typedef enum {
  E_sync_state_normal,          /* QL is derived from ESMC PDU */
  E_sync_state_hold_off,        /* QL is previous QL */
  E_sync_state_wait_to_restore, /* QL is DNU QL */
  E_sync_state_forced,          /* QL is configured forced QL */
  E_sync_state_max
} T_sync_state;

typedef struct {
  const char *name;
  T_sync_type type;
  int clk_idx;
  int config_pri;
  T_esmc_ql init_ql;
  int tx_bundle_num;
} T_sync_config;

typedef struct {
  /* Configured name */
  const char *name;

  /* Current sync type */
  T_sync_type type;

  /* Configured clock index */
  int clk_idx;
  
  /* Configured priority */
  int config_pri;

  /* Configured QL */
  T_esmc_ql init_ql;

  /* Current QL (used for selection) */
  T_esmc_ql current_ql;

  /* Current ESMC QL */
  T_esmc_ql esmc_ql;

  /* Configured forced QL */
  T_esmc_ql forced_ql;

  /* Hold-off QL (used by selector while in hold-off state) */
  T_esmc_ql hold_off_ql;

  /* Current total number of hops (sum of cascaded eEEC and EEC clocks, but limited to MAX_NUMBER_HOPS) */
  int current_num_hops;

  /* Current rank (used for selection) */
  int rank;

  /* Current clock reference monitor status */
  T_device_clk_reference_monitor_status ref_mon_status;
  
  /* Current sync clock state */
  T_sync_clk_state clk_state;

  /* Current sync state */
  T_sync_state state;

  /*
   * Temporary state monotonic time in milliseconds.
   * Applicable to only wait-to-restore and hold-off states.
   * When this monotonic time passes, sync will transition from hold-off/wait-to-restore state to normal state.
   */
  unsigned long long temporary_state_monotonic_time_ms;

  /* TX bundle number */
  int tx_bundle_num;

  /* RX timeout flag */
  int rx_timeout_flag;

  /* Port link down flag */
  int port_link_down_flag;
} T_sync_entry;

#endif /* SYNC_H */
