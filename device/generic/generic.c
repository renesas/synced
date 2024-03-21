/**
 * @file generic.c
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

#include <pthread.h>

#include "generic.h"
#include "../../common/common.h"
#include "../../common/print.h"

#define GENERIC_MAX_DPLL_IDX   7

#define GENERIC_MAX_PRIORITY   18

#define GENERIC_REF_MON_FREQ_OFF_ALARM_STATUS_LSB   2
#define GENERIC_REF_MON_NO_ACT_ALARM_STATUS_LSB     1
#define GENERIC_REF_MON_LOS_ALARM_STATUS_LSB        0

/* Static functions */

/* Helper functions */

static void generic_init_i2c_helper(const char *device_name)
{
  (void)device_name;
}

static void generic_config_device_helper(const char *device_cfg_file)
{
  (void)device_cfg_file;
}

static int generic_get_dpll_ref_helper(int synce_dpll_idx)
{
  (void)synce_dpll_idx;

  return INVALID_CLK_IDX;
}

static void generic_set_dpll_ref_pri_helper(int synce_dpll_idx,
                                            int priority_index,
                                            int enable,
                                            int clk_idx)
{
  (void)synce_dpll_idx;
  (void)priority_index;
  (void)enable;
  (void)clk_idx;
}

static int generic_get_ref_mon_status_helper(int clk_idx)
{
  (void)clk_idx;

  return 0;
}

static T_device_dpll_state generic_get_dpll_state_helper(int synce_dpll_idx)
{
  (void)synce_dpll_idx;

  return E_device_dpll_state_freerun;
}

static void generic_deinit_i2c_helper(void)
{
  /* Left intentionally empty */
}

/* Callback functions */

static int generic_template_init_device(T_device_adaptor_data *device_adaptor_data)
{
  /* This function is a template; the user must implement their own code here or register their own functions using generic_register_callbacks(). */

  int synce_dpll_idx = device_adaptor_data->synce_dpll_idx;
  const char *device_name = device_adaptor_data->device_name;
  const char *device_cfg_file = device_adaptor_data->device_cfg_file;

  if(synce_dpll_idx > GENERIC_MAX_DPLL_IDX) {
    pr_err("Invalid Sync-E DPLL channel index %d", (int)synce_dpll_idx);
    return -1;
  }

  /* Initialize I2C */
  generic_init_i2c_helper(device_name);

  /* Register I2C read/write functions */

  /* Configure device */
  generic_config_device_helper(device_cfg_file);

  return 0;
}

static int generic_template_get_current_clk_idx(int synce_dpll_idx, int *clk_idx)
{
  /* This function is a template; the user must implement their own code here or register their own functions using generic_register_callbacks(). */

  *clk_idx = generic_get_dpll_ref_helper(synce_dpll_idx);

  return 0;
}

static int generic_template_set_clock_priorities(int synce_dpll_idx, T_device_clock_priority_table const *table)
{
  /* This function is a template; the user must implement their own code here or register their own functions using generic_register_callbacks(). */

  /* Received ranked clock priority table */

  int num_entries = (table->num_entries > GENERIC_MAX_PRIORITY) ? GENERIC_MAX_PRIORITY : table->num_entries;
  uint8_t i;
  T_device_clock_priority_entry *priority_entry = table->clock_priority_table;

  /* Write clock priority table starting with highest priority */
  for(i = 0; i < num_entries; i++) {
    generic_set_dpll_ref_pri_helper(synce_dpll_idx,
                                    i,                        /* Priority index */
                                    1,                        /* Enable */
                                    priority_entry->clk_idx);
    priority_entry++;
  }

  /* Continue to disable other num_entries */
  for(; i < GENERIC_MAX_PRIORITY; i++) {
    generic_set_dpll_ref_pri_helper(synce_dpll_idx,
                                    i,              /* Priority index */
                                    0,              /* Disable */
                                    0);
  }

  return 0;
}

static int generic_template_get_reference_monitor_status(int clk_idx, T_device_clk_reference_monitor_status *ref_mon_status)
{
  /* This function is a template; the user must implement their own code here or register their own functions using generic_register_callbacks(). */

  unsigned char reg_val;

  reg_val = generic_get_ref_mon_status_helper(clk_idx);

  ref_mon_status->loss_of_signal_alarm_status = (reg_val >> GENERIC_REF_MON_LOS_ALARM_STATUS_LSB) & 1;
  ref_mon_status->no_activity_alarm_status = (reg_val >> GENERIC_REF_MON_NO_ACT_ALARM_STATUS_LSB) & 1;
  ref_mon_status->frequency_offset_alarm_status = (reg_val >> GENERIC_REF_MON_FREQ_OFF_ALARM_STATUS_LSB) & 1;

  return 0;
}

static int generic_template_get_synce_dpll_state(int synce_dpll_idx, T_device_dpll_state *synce_dpll_state)
{
  /* This function is a template; the user must implement their own code here or register their own functions using generic_register_callbacks(). */

  *synce_dpll_state = generic_get_dpll_state_helper(synce_dpll_idx);

  return 0;
}

static int generic_template_deinit_device(void)
{
  /* This function is a template; the user must implement their own code here or register their own functions using generic_register_callbacks(). */

  /* Deinitialize I2C */
  generic_deinit_i2c_helper();

  return 0;
}

/* Global functions */

void generic_register_callbacks(T_device_adaptor_callbacks *device_adaptor_callbacks)
{
  device_adaptor_callbacks->init_device = &generic_template_init_device;
  device_adaptor_callbacks->get_current_clk_idx = &generic_template_get_current_clk_idx;
  device_adaptor_callbacks->set_clock_priorities = &generic_template_set_clock_priorities;
  device_adaptor_callbacks->get_reference_monitor_status = &generic_template_get_reference_monitor_status;
  device_adaptor_callbacks->get_synce_dpll_state = &generic_template_get_synce_dpll_state;
  device_adaptor_callbacks->deinit_device = &generic_template_deinit_device;
}
