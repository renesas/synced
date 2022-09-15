/**
 * @file device_adaptor.c
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
* Release Tag: 1-0-3
* Pipeline ID: 123302
* Commit Hash: 0d4d9ea7
********************************************************************************************************************/

#include <pthread.h>

#include "device_adaptor.h"
#include "../../common/os.h"
#include "../../common/print.h"
#include "../../common/types.h"

#include DEVICE_I2C_DRIVER_PATH(i2c.h)

#define DEVICE_MAX_DPLL_IDX   7
#define DEVICE_MAX_PRIORITY   8

#define REF_MON_FREQ_OFF_LIM_LSB   2
#define REF_MON_NO_ACT_LSB         1
#define REF_MON_LOS_LSB            0

/* Static data */

static pthread_mutex_t g_device_mutex;
static int g_device_synce_dpll_ch_id;

/* Static functions */
static void device_set_dpll_ref_priority(int synce_dpll_idx,
                                         int priority,
                                         int enable,
                                         int clk_idx)
{
  (void)synce_dpll_idx;
  (void)priority;
  (void)enable;
  (void)clk_idx;
}

static int device_get_dpll_reference(int synce_dpll_idx)
{
  (void)synce_dpll_idx;
  return INVALID_CLK_IDX;
}

static int device_get_ref_mon_status(int clk_idx)
{
  (void)clk_idx;
  return 0;
}

static T_device_dpll_state device_get_dpll_state(int synce_dpll_idx)
{
  (void)synce_dpll_idx;
  return E_device_dpll_state_freerun;
}

/* Global functions */

int device_init(T_device_config const *device_config)
{
  if(device_config->synce_dpll_idx > DEVICE_MAX_DPLL_IDX) {
    pr_err("Invalid Sync-E DPLL index");
    return -1;
  }

  /* Initialize I2C */
  i2c_init(device_config->i2c_device_name);

  /* Register i2c_read and i2c_write functions */

  /* Store the SYnc-E DPLL index */
  g_device_synce_dpll_ch_id = device_config->synce_dpll_idx;

  /* Configure device */

  /* Initialize mutex */
  return os_mutex_init(&g_device_mutex);
}

int device_get_current_clk_idx(void)
{
  return device_get_dpll_reference(g_device_synce_dpll_ch_id);
}

int device_set_clock_priorities(T_device_clock_priority_table const *table)
{
  /* Received ranked clock priority table */

  int entries = (table->entries > DEVICE_MAX_PRIORITY) ? DEVICE_MAX_PRIORITY : table->entries;
  uint8_t i;
  T_device_clock_priority_entry *priority_entry = table->clock_priority_table;

/* Write clock priority table starting with highest priority */
  os_mutex_lock(&g_device_mutex);
  for(i = 0; i < entries; i++) {

    /* Set Sync-E DPLL reference priority */
    device_set_dpll_ref_priority(g_device_synce_dpll_ch_id,
                                 i,                         /* priority */
                                 1,                         /* enable */
                                 priority_entry->clk_idx);
    priority_entry++;
  }
  
  /* Continue to disable other entries */
  for(; i < DEVICE_MAX_PRIORITY; i++) {

    device_set_dpll_ref_priority(g_device_synce_dpll_ch_id,
                                 i,                         /* priority */
                                 0,                         /* disable */
                                 0);
  }

  os_mutex_unlock(&g_device_mutex);

  return 0;
}

int device_get_reference_monitor_status(int clk_idx, T_device_clk_reference_monitor_status *ref_mon_status)
{
  unsigned char reg_val;

  os_mutex_lock(&g_device_mutex);

  /* Read the reference monitor status from the device */
  reg_val = device_get_ref_mon_status(clk_idx);

  os_mutex_unlock(&g_device_mutex);

  ref_mon_status->frequency_offset_live_status = (reg_val >> REF_MON_FREQ_OFF_LIM_LSB) & 1;
  ref_mon_status->no_activity_live_status = (reg_val >> REF_MON_NO_ACT_LSB) & 1;
  ref_mon_status->loss_of_signal_live_status = (reg_val >> REF_MON_LOS_LSB) & 1;

  return 0;
}

T_device_dpll_state device_get_synce_dpll_state(void)
{
  T_device_dpll_state synce_dpll_state;

  os_mutex_lock(&g_device_mutex);
  synce_dpll_state = device_get_dpll_state(g_device_synce_dpll_ch_id);
  os_mutex_unlock(&g_device_mutex);

  return synce_dpll_state;
}

void device_deinit(void)
{
  /* Deinitialize mutex */
  os_mutex_deinit(&g_device_mutex);

  i2c_deinit();
}
