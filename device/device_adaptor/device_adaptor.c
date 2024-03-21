/**
 * @file device_adaptor.c
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
#include <string.h>

#include "device_adaptor.h"
#include "../../common/print.h"
#include "../../common/os.h"

DEVICE_REGISTER_CALLBACKS_DECLARE()

/* External data */

T_device_adaptor_data g_device_adaptor_data;
T_device_adaptor_callbacks g_device_adaptor_callbacks;
pthread_mutex_t g_device_adaptor_mutex;
int g_device_adaptor_init_flag = 0;

/* Static data */

/* Static functions */

/* Global functions */

int device_adaptor_init(T_device_config const *device_config)
{
  if(g_device_adaptor_init_flag) {
    pr_warning("Device adaptor already initialized");
    return 0;
  }

  memset(&g_device_adaptor_data, 0, sizeof(g_device_adaptor_data));
  g_device_adaptor_data.synce_dpll_idx = device_config->synce_dpll_idx;
  g_device_adaptor_data.device_name = device_config->device_name;
  g_device_adaptor_data.device_cfg_file = device_config->device_cfg_file;

  memset(&g_device_adaptor_callbacks, 0, sizeof(g_device_adaptor_callbacks));
  DEVICE_REGISTER_CALLBACKS_CALL(&g_device_adaptor_callbacks);

  /* Initialize mutex */
  if(os_mutex_init(&g_device_adaptor_mutex) < 0) {
    return -1;
  }

  g_device_adaptor_init_flag = 1;

  pr_debug("Initialized device adaptor");

  /* Initialize device */
  if(device_adaptor_call_init_device_cb() < 0) {
    pr_err("Failed to initialize device");
    return -1;
  }

  return 0;
}

void device_adaptor_deinit(void)
{
  if(!g_device_adaptor_init_flag) {
    pr_warning("Device adaptor already deinitialized");
    return;
  }

  /* Deinitialize device */
  device_adaptor_call_deinit_device_cb();

  /* Deinitialize mutex */
  os_mutex_deinit(&g_device_adaptor_mutex);


  memset(&g_device_adaptor_data, 0, sizeof(g_device_adaptor_data));
  g_device_adaptor_data.synce_dpll_idx = -1;
  g_device_adaptor_data.device_name = NULL;
  g_device_adaptor_data.device_cfg_file = NULL;

  memset(&g_device_adaptor_callbacks, 0, sizeof(g_device_adaptor_callbacks));

  g_device_adaptor_init_flag = 0;
}

/* Callback wrappers */

int device_adaptor_call_init_device_cb(void)
{
  int err = -1;

  os_mutex_lock(&g_device_adaptor_mutex);
  if(g_device_adaptor_callbacks.init_device != NULL) {
    err = g_device_adaptor_callbacks.init_device(&g_device_adaptor_data);
  }
  os_mutex_unlock(&g_device_adaptor_mutex);

  return err;
}

int device_adaptor_call_get_current_clk_idx_cb(int *clk_idx)
{
  int err = -1;

  os_mutex_lock(&g_device_adaptor_mutex);
  if(g_device_adaptor_callbacks.get_current_clk_idx != NULL) {
    err = g_device_adaptor_callbacks.get_current_clk_idx(g_device_adaptor_data.synce_dpll_idx, clk_idx);
  }
  os_mutex_unlock(&g_device_adaptor_mutex);

  return err;
}

int device_adaptor_call_set_clock_priorities_cb(T_device_clock_priority_table const *table)
{
  int err = -1;

  os_mutex_lock(&g_device_adaptor_mutex);
  if(g_device_adaptor_callbacks.set_clock_priorities != NULL) {
    err = g_device_adaptor_callbacks.set_clock_priorities(g_device_adaptor_data.synce_dpll_idx, table);
  }
  os_mutex_unlock(&g_device_adaptor_mutex);

  return err;
}

int device_adaptor_call_get_reference_monitor_status_cb(int clk_idx, T_device_clk_reference_monitor_status *ref_mon_status)
{
  int err = -1;

  os_mutex_lock(&g_device_adaptor_mutex);
  if(g_device_adaptor_callbacks.get_reference_monitor_status != NULL) {
    err = g_device_adaptor_callbacks.get_reference_monitor_status(clk_idx, ref_mon_status);
  }
  os_mutex_unlock(&g_device_adaptor_mutex);

  return err;
}

int device_adaptor_call_get_synce_dpll_state_cb(T_device_dpll_state *synce_dpll_state)
{
  int err = -1;

  os_mutex_lock(&g_device_adaptor_mutex);
  if(g_device_adaptor_callbacks.get_synce_dpll_state != NULL) {
    err = g_device_adaptor_callbacks.get_synce_dpll_state(g_device_adaptor_data.synce_dpll_idx, synce_dpll_state);
  }
  os_mutex_unlock(&g_device_adaptor_mutex);

  return err;
}

int device_adaptor_call_deinit_device_cb(void)
{
  int err = -1;

  os_mutex_lock(&g_device_adaptor_mutex);
  if(g_device_adaptor_callbacks.deinit_device != NULL) {
    err = g_device_adaptor_callbacks.deinit_device();
  }
  os_mutex_unlock(&g_device_adaptor_mutex);

  return err;
}
