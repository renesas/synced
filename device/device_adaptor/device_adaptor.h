/**
 * @file device_adaptor.h
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

#ifndef DEVICE_ADAPTOR_H
#define DEVICE_ADAPTOR_H

#include <stdint.h>

#ifndef DEVICE
#error User must define DEVICE. See README.
#endif /* DEVICE */

#define TOKENPASTE(x, y) x##y
#define XTOKENPASTE(x, y) TOKENPASTE(x, y)

typedef enum {
  E_device_dpll_state_freerun,
  E_device_dpll_state_lock_acquisition_recovery,
  E_device_dpll_state_locked,
  E_device_dpll_state_holdover,
  E_device_dpll_state_max
} T_device_dpll_state;

typedef struct {
  int synce_dpll_idx;
  const char *device_name;
  const char *device_cfg_file;
} T_device_config;

typedef struct {
  int synce_dpll_idx;
  const char *device_name;
  const char *device_cfg_file;
} T_device_adaptor_data;

typedef struct {
  int clk_idx;
  int rank;
} T_device_clock_priority_entry;

typedef struct {
  int num_entries;
  T_device_clock_priority_entry *clock_priority_table;
} T_device_clock_priority_table;

typedef struct {
  unsigned char frequency_offset_alarm_status : 1;
  unsigned char no_activity_alarm_status : 1;
  unsigned char loss_of_signal_alarm_status : 1;
} T_device_clk_reference_monitor_status;

typedef struct
{
  int (*init_device)(T_device_adaptor_data *device_adaptor_data);
  int (*get_current_clk_idx)(int synce_dpll_idx, int *clk_idx);
  int (*set_clock_priorities)(int synce_dpll_idx, T_device_clock_priority_table const *table);
  int (*get_reference_monitor_status)(int clk_idx, T_device_clk_reference_monitor_status *ref_mon_status);
  int (*get_synce_dpll_state)(int synce_dpll_idx, T_device_dpll_state *synce_dpll_state);
  int (*deinit_device)(void);
} T_device_adaptor_callbacks;

#define DEVICE_REGISTER_CALLBACKS_DECLARE() \
  extern void XTOKENPASTE(DEVICE, _register_callbacks(T_device_adaptor_callbacks *device_adaptor_callbacks);)

#define DEVICE_REGISTER_CALLBACKS_CALL(device_adaptor_callbacks) \
  XTOKENPASTE(DEVICE, _register_callbacks(device_adaptor_callbacks))

int device_adaptor_init(T_device_config const *device_config);
void device_adaptor_deinit(void);

/* Callback wrappers */

int device_adaptor_call_init_device_cb(void);
int device_adaptor_call_get_current_clk_idx_cb(int *clk_idx);
int device_adaptor_call_set_clock_priorities_cb(T_device_clock_priority_table const *table);
int device_adaptor_call_get_reference_monitor_status_cb(int clk_idx, T_device_clk_reference_monitor_status *ref_mon_status);
int device_adaptor_call_get_synce_dpll_state_cb(T_device_dpll_state *synce_dpll_state);
int device_adaptor_call_deinit_device_cb(void);

#endif /* DEVICE_ADAPTOR_H */
