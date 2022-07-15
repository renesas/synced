/**
 * @file device_adaptor.h
 * @note Copyright (C) [2021-2022] Renesas Electronics Corporation and/or its affiliates
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
* Release Tag: 1-0-1
* Pipeline ID: 113278
* Commit Hash: 8af68511
********************************************************************************************************************/

#ifndef DEVICE_ADAPTOR_H
#define DEVICE_ADAPTOR_H

#include <stdint.h>

typedef enum {
  E_device_dpll_state_freerun,
  E_device_dpll_state_lock_acquisition,
  E_device_dpll_state_lock_recovery,
  E_device_dpll_state_locked,
  E_device_dpll_state_holdover,
  E_device_dpll_state_max
} T_device_dpll_state;

typedef struct {
  unsigned char frequency_offset_live_status : 1;
  unsigned char no_activity_live_status : 1;
  unsigned char loss_of_signal_live_status : 1;
} T_device_clk_reference_monitor_status;

typedef struct {
  int clk_idx;
  int rank;
} T_device_clock_priority_entry;

typedef struct {
  int entries;
  T_device_clock_priority_entry *clock_priority_table;
} T_device_clock_priority_table;

int device_init(int synce_dpll_idx, const char *tcs_file);
int device_get_current_clk_idx(void);
int device_set_clock_priorities(T_device_clock_priority_table const *table);
int device_get_reference_monitor_status(int clk_idx, T_device_clk_reference_monitor_status *ref_mon_status);
T_device_dpll_state device_get_synce_dpll_state(void);
void device_deinit(void);

#endif /* DEVICE_ADAPTOR_H */
