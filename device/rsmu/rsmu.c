/**
 * @file rsmu.c
 * @note Copyright (C) [2023-2024] Renesas Electronics Corporation and/or its affiliates
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

#include <fcntl.h>
#include <linux/types.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>

#include "rsmu.h"
#include "../../common/common.h"
#include "../../common/print.h"

#define RSMU_DEVICE_NAME_PREFIX   "dev/rsmu"

/* Static data */

static int g_rsmu_fd = UNINITIALIZED_FD;

/* Static functions */

/* Callback functions */

static int rsmu_init_device(T_device_adaptor_data *device_adaptor_data)
{
  const char *device_name = device_adaptor_data->device_name;

  if(strstr(device_name, RSMU_DEVICE_NAME_PREFIX) == NULL) {
    pr_err("%s is an invalid RSMU device name", device_name);
    return -1;
  }

  if(g_rsmu_fd == UNINITIALIZED_FD) {
    if((g_rsmu_fd = open(device_name, O_WRONLY)) < 0) {
      pr_err("Opening %s failed: %s", device_name, strerror(errno));
      g_rsmu_fd = UNINITIALIZED_FD;
      return -1;
    }
  } else {
    pr_warning("%s: RSMU device already opened", __func__);
  }

  return 0;
}

static int rsmu_get_current_clk_idx(int synce_dpll_idx, int *clk_idx)
{
  struct rsmu_current_clock_index get;

  memset(&get, 0, sizeof(get));
  get.dpll = synce_dpll_idx;

  if(ioctl(g_rsmu_fd, RSMU_GET_CURRENT_CLOCK_INDEX, &get)) {
    pr_err("%s failed: %s", __func__, strerror(errno));
    return -1;
  }

  *clk_idx = (int)get.clock_index;

  return 0;
}

static int rsmu_set_clock_priorities(int synce_dpll_idx, T_device_clock_priority_table const *priority_table)
{
  /* Received ranked clock priority table */

  int num_entries = (priority_table->num_entries > MAX_NUM_PRIORITY_ENTRIES) ? MAX_NUM_PRIORITY_ENTRIES : priority_table->num_entries;
  uint8_t i;
  T_device_clock_priority_entry *priority_entry = priority_table->clock_priority_table;
  uint8_t priority = 0;
  int prev_rank = -1;
  struct rsmu_clock_priorities set;

  memset(&set, 0, sizeof(set));
  set.dpll = synce_dpll_idx;
  set.num_entries = num_entries;

  /* Write clock priority table starting with highest priority */
  set.priority_entry[0].clock_index = priority_entry->clk_idx;
  set.priority_entry[0].priority = priority;
  prev_rank = priority_entry->rank;
  priority_entry++;
  for(i = 1; i < num_entries; i++) {
    set.priority_entry[i].clock_index = priority_entry->clk_idx;
    if(prev_rank != priority_entry->rank) {
      prev_rank = priority_entry->rank;
      priority++;
    }

    set.priority_entry[i].priority = priority;

    priority_entry++;
  }

  if(ioctl(g_rsmu_fd, RSMU_SET_CLOCK_PRIORITIES, &set)) {
    pr_err("%s failed: %s", __func__, strerror(errno));
    return -1;
  }

  return 0;
}

static int rsmu_get_reference_monitor_status(int clk_idx, T_device_clk_reference_monitor_status *ref_mon_status)
{
  struct rsmu_reference_monitor_status get;

  memset(ref_mon_status, 0, sizeof(*ref_mon_status));

  memset(&get, 0, sizeof(get));
  get.clock_index = clk_idx;

  if(ioctl(g_rsmu_fd, RSMU_GET_REFERENCE_MONITOR_STATUS, &get)) {
    pr_err("%s failed: %s", __func__, strerror(errno));
    return -1;
  }

  ref_mon_status->frequency_offset_alarm_status = get.alarms.frequency_offset_limit;
  ref_mon_status->no_activity_alarm_status = get.alarms.no_activity;
  ref_mon_status->loss_of_signal_alarm_status = get.alarms.los;

  return 0;
}

static int rsmu_get_synce_dpll_state(int synce_dpll_idx, T_device_dpll_state *synce_dpll_state)
{
  struct rsmu_get_state get;

  memset(&get, 0, sizeof(get));
  get.dpll = synce_dpll_idx;

  if(ioctl(g_rsmu_fd, RSMU_GET_STATE, &get)) {
    pr_err("%s failed: %s", __func__, strerror(errno));
    return -1;
  }

  switch(get.state) {
    case 1:
      *synce_dpll_state = E_device_dpll_state_freerun;
      break;
    case 2:
      *synce_dpll_state = E_device_dpll_state_lock_acquisition_recovery;
      break;
    case 4:
      *synce_dpll_state = E_device_dpll_state_locked;
      break;
    case 5:
      *synce_dpll_state = E_device_dpll_state_holdover;
      break;
    default:
      *synce_dpll_state = E_device_dpll_state_max;
      break;
  }

  return 0;
}

static int rsmu_deinit_device(void)
{
  return 0;
}

/* Global functions */

void rsmu_register_callbacks(T_device_adaptor_callbacks *device_adaptor_callbacks)
{
  device_adaptor_callbacks->init_device = &rsmu_init_device;
  device_adaptor_callbacks->get_current_clk_idx = &rsmu_get_current_clk_idx;
  device_adaptor_callbacks->set_clock_priorities = &rsmu_set_clock_priorities;
  device_adaptor_callbacks->get_reference_monitor_status = &rsmu_get_reference_monitor_status;
  device_adaptor_callbacks->get_synce_dpll_state = &rsmu_get_synce_dpll_state;
  device_adaptor_callbacks->deinit_device = &rsmu_deinit_device;
}
