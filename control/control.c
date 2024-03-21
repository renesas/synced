/**
 * @file control.c
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

#include <stdlib.h>
#include <string.h>

#include "control.h"
#include "../common/common.h"
#include "../common/print.h"
#include "../common/os.h"
#include "../device/device_adaptor/device_adaptor.h"
#include "../esmc/esmc_adaptor/esmc_adaptor.h"
#include "../management/management.h"

#define MAX_NUMBER_HOPS   255

#define LO_NUMBER_OF_HOPS   0xFF

/* Static data */

static pthread_mutex_t g_control_mutex;
static T_control_data g_control_data;

/* Static functions */

static int control_find_best_clock(T_sync_entry *table, int *rank)
{
  /* Mutex must be taken before this function can be called */

  T_sync_entry *sync_entry = table;
  T_sync_entry *best_sync_entry = NULL;
  int i;
  int best_rank;
  int best_clk_idx = -1;

  if(g_control_data.lo_ql < g_control_data.do_not_use_ql) {
    best_rank = calculate_rank((int)g_control_data.lo_ql, g_control_data.lo_pri, LO_NUMBER_OF_HOPS);
  } else {
    best_rank = calculate_rank((int)g_control_data.do_not_use_ql, 0, 0);
  }

  for(i = 0; i < g_control_data.num_syncs; i++) {
    if((sync_entry->type == E_sync_type_synce) || (sync_entry->type == E_sync_type_external)) {
      /* Only consider Sync-E clock and external clock ports in selection */
      if(sync_entry->rank < best_rank) {
        best_rank = sync_entry->rank;
        best_clk_idx = sync_entry->clk_idx;
        best_sync_entry = sync_entry;
      }
    }
    sync_entry++;
  }

  if(best_sync_entry != NULL) {
    /* Change sync type for selected sync to avoid re-selection */
    best_sync_entry->type = E_sync_type_max;
  }

  *rank = best_rank;
  return best_clk_idx;
}

static void control_update_device_priority_table(void)
{
  /* Mutex must be taken before this function is called */

  T_device_clock_priority_entry priority_array[MAX_NUM_OF_CLOCKS];
  T_sync_entry *temp_sync_table;
  int i;
  int priority = 0;
  T_device_clock_priority_table table;
  int best_clk_idx;
  int best_rank;
  int err;
  char buff[1024];
  const char *format = " %d";
  int pos = 0;
  int ret;

  temp_sync_table = calloc(g_control_data.num_syncs, sizeof(*temp_sync_table));
  if(!temp_sync_table) {
    return;
  }

  /* Make copy of sync table */
  memcpy(temp_sync_table, g_control_data.sync_table, g_control_data.num_syncs * sizeof(*temp_sync_table));

  /* Clear priority array */
  memset(&priority_array, 0, sizeof(priority_array));

  for(i = 0; i < g_control_data.num_syncs; i++) {
    best_clk_idx = control_find_best_clock(temp_sync_table, &best_rank);
    if(best_clk_idx < 0) {
      break;
    }

    /* Best clock index is between [0, g_control_data.num_syncs); fill new entry in priority array */
    priority_array[priority].clk_idx = best_clk_idx;
    priority_array[priority].rank = best_rank;

    priority++;
  }

  if(priority > 0) {
    pr_debug("Best clock has clock index %d and rank 0x%06X", priority_array[0].clk_idx, priority_array[0].rank);
  } else {
    pr_debug("Best clock is LO");
  }

  table.num_entries = priority;
  table.clock_priority_table = &priority_array[0];

  /* Set priority table */
  err = device_adaptor_call_set_clock_priorities_cb(&table);
  if(err < 0) {
    pr_err("Failed to set device clock priorities");
  } else {
    /* Clear update priority table flag */
    g_control_data.update_priority_table_flag = 0;
  }

  for(priority = 0; priority < table.num_entries; priority++) {
    if(pos >= (int)sizeof(buff) - 1) {
      break;
    }

    /* snprintf returns number of characters stored in array, not including null character */
    ret = snprintf(&buff[pos],
                   sizeof(buff) - 1 - pos,
                   format,
                   table.clock_priority_table[priority].clk_idx);

    if(ret <= 0) {
      break;
    }

    pos += ret;
  }
  buff[pos] = 0;

  pr_debug("Set device clock priorities (%d):%s (ordered list of clock indices)", table.num_entries, buff);

  free(temp_sync_table);
  temp_sync_table = NULL;
}

static int control_tx_event_cb(T_esmc_adaptor_tx_event_cb_data *cb_data)
{
  T_esmc_event_type event_type = cb_data->event_type;
  int sync_idx = cb_data->sync_idx;
  T_sync_entry *sync_entry;

  os_mutex_lock(&g_control_mutex);
  sync_entry = &g_control_data.sync_table[sync_idx];

  if(sync_entry->type != E_sync_type_tx_only) {
    os_mutex_unlock(&g_control_mutex);
    return -1;
  }

  switch(event_type) {
    case E_esmc_event_type_port_link_up:
      sync_entry->port_link_down_flag = 0;
      break;
    case E_esmc_event_type_port_link_down:
      sync_entry->port_link_down_flag = 1;
      break;
    default:
      break;
  }

  os_mutex_unlock(&g_control_mutex);

  return 0;
}

static int control_rx_event_cb(T_esmc_adaptor_rx_event_cb_data *cb_data)
{
  T_esmc_event_type event_type = cb_data->event_type;
  int sync_idx = cb_data->sync_idx;
  T_esmc_ql new_ql;
  int new_total_num_hops;
  T_sync_entry *sync_entry;
  T_esmc_ql do_not_use_ql;
  T_esmc_ql old_ql;
  T_sync_state new_state;
  T_alarm_data alarm_data;

  os_mutex_lock(&g_control_mutex);
  sync_entry = &g_control_data.sync_table[sync_idx];

  if((sync_entry->type != E_sync_type_synce) && (sync_entry->type != E_sync_type_monitoring)) {
    os_mutex_unlock(&g_control_mutex);
    return -1;
  }

  do_not_use_ql = g_control_data.do_not_use_ql;
  old_ql = sync_entry->esmc_ql;
  new_ql = old_ql;

  switch(event_type) {
    case E_esmc_event_type_invalid_rx_ql:
      alarm_data.alarm_type = E_alarm_type_invalid_rx_ql;
      alarm_data.alarm_invalid_ql.port_name = sync_entry->name;
      management_call_notify_alarm_cb(&alarm_data);
      break;
    case E_esmc_event_type_ql_change:
      /* QL */
      new_ql = cb_data->event_data.ql_change.new_ql;
      /* Total number of hops (sum of cascaded eEEC and EEC clocks, but limited to MAX_NUMBER_HOPS) */
      new_total_num_hops = cb_data->event_data.ql_change.new_num_cascaded_eEEC + cb_data->event_data.ql_change.new_num_cascaded_EEC;
      new_total_num_hops = (new_total_num_hops > MAX_NUMBER_HOPS) ? MAX_NUMBER_HOPS : new_total_num_hops;
      sync_entry->current_num_hops = new_total_num_hops;
      sync_entry->rx_timeout_flag = 0;
      break;
    case E_esmc_event_type_rx_timeout:
      new_ql = E_esmc_ql_FAILED;
      sync_entry->rx_timeout_flag = 1;
      break;
    case E_esmc_event_type_port_link_up:
      sync_entry->port_link_down_flag = 0;
      break;
    case E_esmc_event_type_port_link_down:
      new_ql = do_not_use_ql;
      sync_entry->port_link_down_flag = 1;
      break;
    case E_esmc_event_type_immediate_timing_loop:
      alarm_data.alarm_type = E_alarm_type_timing_loop;
      alarm_data.alarm_timing_loop.loop_type = E_timing_loop_type_immediate;
      alarm_data.alarm_timing_loop.mac_addr = cb_data->event_data.timing_loop.mac_addr;
      alarm_data.alarm_timing_loop.port_name = sync_entry->name;
      management_call_notify_alarm_cb(&alarm_data);
      break;
    case E_esmc_event_type_originator_timing_loop:
      alarm_data.alarm_type = E_alarm_type_timing_loop;
      alarm_data.alarm_timing_loop.loop_type = E_timing_loop_type_originator;
      alarm_data.alarm_timing_loop.mac_addr = cb_data->event_data.timing_loop.mac_addr;
      alarm_data.alarm_timing_loop.port_name = sync_entry->name;
      management_call_notify_alarm_cb(&alarm_data);
      break;
    default:
      new_ql = do_not_use_ql;
      break;
  }

  if(old_ql == new_ql) {
    os_mutex_unlock(&g_control_mutex);
    return 0;
  }

  /* Change in QL */
  sync_entry->esmc_ql = new_ql;

  if(sync_entry->state == E_sync_state_forced) {
    os_mutex_unlock(&g_control_mutex);
    return 0;
  }

  new_state = sync_entry->state;
  if(new_ql < do_not_use_ql) {
    if(g_control_data.wait_to_restore_timer_s == 0) {
      /* No wait-to-restore timer */
      new_state = E_sync_state_normal;
    } else if(old_ql >= do_not_use_ql) {
      new_state = E_sync_state_wait_to_restore;
      sync_entry->temporary_state_monotonic_time_ms = os_get_monotonic_milliseconds() + (g_control_data.wait_to_restore_timer_s * 1000);
    } else {
      /* Sync is in normal state (sync was already in normal state or was in hold-off state) */
      new_state = E_sync_state_normal;
    }
  } else if(new_ql == E_esmc_ql_FAILED) {
    if((g_control_data.hold_off_timer_ms == 0) || (sync_entry->state == E_sync_state_wait_to_restore)) {
      /* No hold-off timer */
      new_state = E_sync_state_normal;
    } else if(sync_entry->state == E_sync_state_normal) {
      new_state = E_sync_state_hold_off;
      sync_entry->temporary_state_monotonic_time_ms = os_get_monotonic_milliseconds() + g_control_data.hold_off_timer_ms;
      sync_entry->hold_off_ql = old_ql;
    } else {
      /* Continue in hold-off state */
    }
  } else {
    new_state = E_sync_state_normal;
  }
  sync_entry->state = new_state;

  os_mutex_unlock(&g_control_mutex);

  management_call_notify_sync_current_state_cb(sync_entry->name, new_state);

  return 0;
}

/* Return 1 if clock is qualified and 0 otherwise */
static int control_check_qualification_status(int clk_idx, T_device_clk_reference_monitor_status *ref_mon_status)
{
  int alarm_raised_flag;
  int err;

  if(clk_idx >= MAX_NUM_OF_CLOCKS) {
    return 0;
  }

  err = device_adaptor_call_get_reference_monitor_status_cb(clk_idx, ref_mon_status);
  if(err < 0) {
    pr_err("Failed to get reference monitor status of clock index %d", clk_idx);
    return 0;
  }

  alarm_raised_flag = (ref_mon_status->frequency_offset_alarm_status ||
                       ref_mon_status->no_activity_alarm_status ||
                       ref_mon_status->loss_of_signal_alarm_status);

  if(alarm_raised_flag) {
    /* One or more of the alarms were raised */
    return 0;
  } else {
    /* None of the alarms were raised */
    return 1;
  }
}

/* Global functions */

int control_init(T_control_config const *control_config)
{
  T_esmc_ql do_not_use_ql;
  int num_syncs;
  T_sync_config const *sync_config;
  T_sync_entry *sync_entry;
  int sync_idx;
  int clk_idx;
  T_esmc_ql init_ql;

  /* Initialize mutex */
  if(os_mutex_init(&g_control_mutex) < 0) {
    return -1;
  }

  memset(&g_control_data, 0, sizeof(g_control_data));

  g_control_data.net_opt = control_config->net_opt;

  g_control_data.no_ql_en = control_config->no_ql_en;

  g_control_data.synce_forced_ql_en = control_config->synce_forced_ql_en;

  g_control_data.lo_ql = control_config->lo_ql;

  g_control_data.lo_pri = control_config->lo_pri;

  do_not_use_ql = control_config->do_not_use_ql;
  g_control_data.do_not_use_ql = do_not_use_ql;

  g_control_data.hold_off_timer_ms = control_config->hold_off_timer_ms;

  g_control_data.wait_to_restore_timer_s = control_config->wait_to_restore_timer_s;

  num_syncs = control_config->num_syncs;
  g_control_data.num_syncs = num_syncs;

  g_control_data.sync_table = calloc(num_syncs, sizeof(*g_control_data.sync_table));
  if(!g_control_data.sync_table) {
    os_mutex_deinit(&g_control_mutex);
    memset(&g_control_data, 0, sizeof(g_control_data));
    return -1;
  }

  sync_config = control_config->sync_config_array;
  for(sync_idx = 0; sync_idx < num_syncs; sync_idx++) {
    sync_entry = &g_control_data.sync_table[sync_idx];

    sync_entry->name = sync_config->name;

    sync_entry->type = sync_config->type;

    clk_idx = sync_config->clk_idx;
    sync_entry->clk_idx = clk_idx;

    sync_entry->config_pri = sync_config->config_pri;

    if(g_control_data.no_ql_en && (sync_entry->type == E_sync_type_external)) {
      /* If no QL mode is enabled and sync is external clock port, then force initial and current QL to LO QL */
      init_ql = g_control_data.lo_ql;
    } else {
      init_ql = sync_config->init_ql;
    }

    sync_entry->init_ql = init_ql;

    sync_entry->current_ql = init_ql;

    sync_entry->esmc_ql = init_ql;

    sync_entry->forced_ql = init_ql;

    sync_entry->hold_off_ql = do_not_use_ql;

    sync_entry->current_num_hops = 0;

    sync_entry->rank = 0;

    sync_entry->clk_state = E_sync_clk_state_unqualified;

    sync_entry->state = E_sync_state_normal;

    sync_entry->temporary_state_monotonic_time_ms = 0;

    sync_entry->tx_bundle_num = sync_config->tx_bundle_num;

    sync_entry->rx_timeout_flag = 0;

    sync_entry->port_link_down_flag = 0;

    sync_config++;
  }

  g_control_data.update_priority_table_flag = 0;

  /* Update sync table */
  control_update_sync_table();

  /* Register control TX event callback with ESMC stack */
  esmc_adaptor_register_tx_cb(control_tx_event_cb);

  /* Register control RX event callback with ESMC stack */
  esmc_adaptor_register_rx_cb(control_rx_event_cb);

  return 0;
}

/* Update sync clock state, sync state, and current QL */
void control_update_sync_table(void)
{
  int i;
  T_sync_entry *sync_entry;
  const char *port_name;
  T_sync_clk_state old_clk_state;
  int old_rank;
  int change_flag = 0;
  int rank;
  T_esmc_ql current_ql;
  T_sync_clk_state clk_state;
  int clk_idx;

  os_mutex_lock(&g_control_mutex);
  for(i = 0; i < g_control_data.num_syncs; i++) {
    sync_entry = &g_control_data.sync_table[i];

    port_name = sync_entry->name;

    if(sync_entry->type == E_sync_type_tx_only)
      continue;

    /* Update sync clock state for active Sync-E ports and external clock ports.
       Do not update the clock state for the monitoring ports. */
    old_clk_state = sync_entry->clk_state;
    if((sync_entry->type == E_sync_type_synce) || (sync_entry->type == E_sync_type_external)) {
      if(control_check_qualification_status(sync_entry->clk_idx, &sync_entry->ref_mon_status)) {
        sync_entry->clk_state = E_sync_clk_state_qualified;
      } else {
        sync_entry->clk_state = E_sync_clk_state_unqualified;
      }
    }

    /* Update sync state */
    if((sync_entry->type == E_sync_type_synce) || (sync_entry->type == E_sync_type_monitoring)) {
      switch(sync_entry->state) {
        case E_sync_state_hold_off:
        case E_sync_state_wait_to_restore:
          if(os_get_monotonic_milliseconds() > sync_entry->temporary_state_monotonic_time_ms) {
            sync_entry->state = E_sync_state_normal;

            os_mutex_unlock(&g_control_mutex);
            management_call_notify_sync_current_state_cb(port_name, E_sync_state_normal);
            os_mutex_lock(&g_control_mutex);
          }
          break;

        case E_sync_state_normal:
        case E_sync_state_forced:
        default:
          break;
      }
    }

    /* Update current QL */
    if(sync_entry->type == E_sync_type_external) {
      if(sync_entry->state == E_sync_state_normal) {
        sync_entry->current_ql = sync_entry->init_ql;
      } else {
        sync_entry->current_ql = sync_entry->forced_ql;
      }
    } else {
      if(sync_entry->state == E_sync_state_normal) {
        sync_entry->current_ql = sync_entry->esmc_ql;
      } else if(sync_entry->state == E_sync_state_hold_off) {
        sync_entry->current_ql = sync_entry->hold_off_ql;
      } else if(sync_entry->state == E_sync_state_wait_to_restore) {
        sync_entry->current_ql = g_control_data.do_not_use_ql;
      } else {
        sync_entry->current_ql = sync_entry->forced_ql;
      }
    }

    old_rank = sync_entry->rank;
    if(g_control_data.no_ql_en && (sync_entry->current_ql < g_control_data.do_not_use_ql)) {
      rank = calculate_rank((int)g_control_data.lo_ql, sync_entry->config_pri, sync_entry->current_num_hops);
    } else {
      rank = calculate_rank((int)sync_entry->current_ql, sync_entry->config_pri, sync_entry->current_num_hops);
    }
    if(old_rank != rank) {
      change_flag = 1;
      sync_entry->rank = rank;

      if(g_control_data.no_ql_en) {
        current_ql = E_esmc_ql_NSUPP;
      } else {
        current_ql = sync_entry->current_ql;
      }

      os_mutex_unlock(&g_control_mutex);
      management_call_notify_sync_current_ql_cb(port_name, current_ql, rank);
      os_mutex_lock(&g_control_mutex);
    }
    if(old_clk_state != sync_entry->clk_state) {
      clk_state = sync_entry->clk_state;
      clk_idx = sync_entry->clk_idx;

      os_mutex_unlock(&g_control_mutex);
      management_call_notify_sync_current_clk_state_cb(port_name, clk_idx, clk_state);
      os_mutex_lock(&g_control_mutex);
    }
  }

  if(g_control_data.update_priority_table_flag == 1) {
    /* Change in clock index mapping */
    change_flag = 1;
  }

  if(change_flag) {
    control_update_device_priority_table();
  }
  os_mutex_unlock(&g_control_mutex);
}

int control_get_sync_idx(int clk_idx)
{
  int i;
  T_sync_entry *sync_entry;

  if((clk_idx < 0) || (clk_idx >= MAX_NUM_OF_CLOCKS)) {
    return INVALID_SYNC_IDX;
  }

  os_mutex_lock(&g_control_mutex);
  for(i = 0; i < g_control_data.num_syncs; i++) {
    sync_entry = &g_control_data.sync_table[i];
    if(sync_entry->clk_idx == clk_idx) {
      os_mutex_unlock(&g_control_mutex);
      return i;
    }
  }
  os_mutex_unlock(&g_control_mutex);
  return INVALID_SYNC_IDX;
}

T_esmc_ql control_get_ql(int sync_idx)
{
  T_sync_entry *sync_entry;
  T_esmc_ql ql = E_esmc_ql_max;

  if((sync_idx < 0) || (sync_idx >= g_control_data.num_syncs)) {
    pr_err("Failed to get QL due to invalid sync index %d", sync_idx);
    return ql;
  }

  os_mutex_lock(&g_control_mutex);
  sync_entry = &g_control_data.sync_table[sync_idx];
  ql = sync_entry->current_ql;
  os_mutex_unlock(&g_control_mutex);

  return ql;
}

void control_get_sync_name(int sync_idx, char *port_name)
{
  T_sync_entry *sync_entry;

  if(sync_idx >= g_control_data.num_syncs) {
    pr_warning("Failed to get port name due to invalid sync index %d", sync_idx);
    port_name[0] = '\0';
    return;
  }
  if(sync_idx < 0) {
    strcpy(port_name, "LO");
    return;
  }

  sync_entry = &g_control_data.sync_table[sync_idx];
  strcpy(port_name, sync_entry->name);
}

int control_set_forced_ql(const char *port_name, T_esmc_ql forced_ql)
{
  int i;
  T_sync_entry *sync_entry;
  T_esmc_ql old_forced_ql;

  if(g_control_data.no_ql_en) {
    pr_err("Set forced QL not supported because no QL mode is enabled");
    return -2;
  }

  if(check_ql_setting(g_control_data.net_opt, forced_ql) < 0) {
    pr_err("Specified incompatible forced QL %s (%d) for port %s",
           conv_ql_enum_to_str(forced_ql),
           forced_ql,
           port_name);
    return -1;
  }

  os_mutex_lock(&g_control_mutex);
  for(i = 0; i < g_control_data.num_syncs; i++) {
    sync_entry = &g_control_data.sync_table[i];

    if(!strcmp(sync_entry->name, port_name)) {
      old_forced_ql = sync_entry->forced_ql;

      if(sync_entry->state == E_sync_state_forced) {
        if(forced_ql == old_forced_ql) {
          os_mutex_unlock(&g_control_mutex);
          pr_warning("Forced QL is already set to %s (%d) for port %s", conv_ql_enum_to_str(forced_ql), forced_ql, port_name);
          return 0;
        }
      }

      if(sync_entry->type != E_sync_type_external) {
        if(sync_entry->type == E_sync_type_tx_only) {
          os_mutex_unlock(&g_control_mutex);
          pr_warning("Forced QL is not supported for %s because it is Sync-E TX only port", port_name);
          return -2;
        } else if(!g_control_data.synce_forced_ql_en) {
          os_mutex_unlock(&g_control_mutex);
          pr_warning("Forced QL is not supported for %s because forced QL mode is disabled", port_name);
          return -2;
        }
      }

      /* Change in QL */
      sync_entry->forced_ql = forced_ql;
      sync_entry->state = E_sync_state_forced;
      os_mutex_unlock(&g_control_mutex);

      management_call_notify_sync_current_state_cb(port_name, E_sync_state_forced);

      pr_info("Changed forced QL to %s (%d) for port %s",
              conv_ql_enum_to_str(forced_ql),
              forced_ql,
              port_name);

      return 0;
    }
  }
  os_mutex_unlock(&g_control_mutex);

  return -1;
}

int control_clear_forced_ql(const char *port_name)
{
  int i;
  T_sync_entry *sync_entry;
  T_esmc_ql old_forced_ql;

  if(g_control_data.no_ql_en) {
    pr_err("Clear forced QL not supported because no QL mode is enabled");
    return -2;
  }

  os_mutex_lock(&g_control_mutex);
  for(i = 0; i < g_control_data.num_syncs; i++) {
    sync_entry = &g_control_data.sync_table[i];

    if(!strcmp(sync_entry->name, port_name)) {
      if(sync_entry->state == E_sync_state_forced) {
        old_forced_ql = sync_entry->forced_ql;
        sync_entry->state = E_sync_state_normal;
        os_mutex_unlock(&g_control_mutex);

        management_call_notify_sync_current_state_cb(port_name, E_sync_state_normal);

        pr_info("Cleared forced QL %s (%d) for port %s", conv_ql_enum_to_str(old_forced_ql), old_forced_ql, port_name);
      } else {
        os_mutex_unlock(&g_control_mutex);
        pr_warning("No forced QL is set for port %s", port_name);
      }

      return 0;
    }
  }
  os_mutex_unlock(&g_control_mutex);

  return -1;
}

int control_get_sync_info_by_index(int sync_idx, T_management_sync_info *sync_info)
{
  T_sync_entry *sync_entry;
  unsigned long long monotonic_time_now_ms;

  memset(sync_info, 0, sizeof(*sync_info));

  if(sync_idx >= g_control_data.num_syncs) {
    return -1;
  }

  sync_entry = &g_control_data.sync_table[sync_idx];

  os_mutex_lock(&g_control_mutex);
  strcpy(sync_info->name, sync_entry->name);
  sync_info->type = sync_entry->type;

  switch(sync_entry->type) {
    case E_sync_type_synce:
      sync_info->synce_clk_info.config_pri = sync_entry->config_pri;
      if(g_control_data.no_ql_en) {
        sync_info->synce_clk_info.current_ql = E_esmc_ql_NSUPP;
      } else {
        sync_info->synce_clk_info.current_ql = sync_entry->current_ql;
      }
      sync_info->synce_clk_info.state = sync_entry->state;
      sync_info->synce_clk_info.tx_bundle_num = sync_entry->tx_bundle_num;
      sync_info->synce_clk_info.clk_idx = sync_entry->clk_idx;
      monotonic_time_now_ms = os_get_monotonic_milliseconds();
      if(monotonic_time_now_ms >= sync_entry->temporary_state_monotonic_time_ms) {
        sync_info->synce_clk_info.remaining_time_ms = 0;
      } else {
        sync_info->synce_clk_info.remaining_time_ms = (unsigned int)(sync_entry->temporary_state_monotonic_time_ms - monotonic_time_now_ms);
      }
      sync_info->synce_clk_info.current_num_hops = sync_entry->current_num_hops;
      sync_info->synce_clk_info.rank = sync_entry->rank;
      sync_info->synce_clk_info.clk_state = sync_entry->clk_state;
      sync_info->synce_clk_info.ref_mon_status = sync_entry->ref_mon_status;
      sync_info->synce_clk_info.rx_timeout_flag = sync_entry->rx_timeout_flag;
      sync_info->synce_clk_info.port_link_down_flag = sync_entry->port_link_down_flag;
      break;

    case E_sync_type_monitoring:
      sync_info->synce_mon_info.config_pri = sync_entry->config_pri;
      if(g_control_data.no_ql_en) {
        sync_info->synce_mon_info.current_ql = E_esmc_ql_NSUPP;
      } else {
        sync_info->synce_mon_info.current_ql = sync_entry->current_ql;
      }
      sync_info->synce_mon_info.state = sync_entry->state;
      sync_info->synce_mon_info.tx_bundle_num = sync_entry->tx_bundle_num;
      monotonic_time_now_ms = os_get_monotonic_milliseconds();
      if(monotonic_time_now_ms >= sync_entry->temporary_state_monotonic_time_ms) {
        sync_info->synce_mon_info.remaining_time_ms = 0;
      } else {
        sync_info->synce_mon_info.remaining_time_ms = (unsigned int)(sync_entry->temporary_state_monotonic_time_ms - monotonic_time_now_ms);
      }
      sync_info->synce_mon_info.current_num_hops = sync_entry->current_num_hops;
      sync_info->synce_mon_info.rank = sync_entry->rank;
      sync_info->synce_mon_info.rx_timeout_flag = sync_entry->rx_timeout_flag;
      sync_info->synce_mon_info.port_link_down_flag = sync_entry->port_link_down_flag;
      break;

    case E_sync_type_external:
      sync_info->ext_clk_info.config_pri = sync_entry->config_pri;
      if(g_control_data.no_ql_en) {
        sync_info->ext_clk_info.current_ql = E_esmc_ql_NSUPP;
      } else {
        sync_info->ext_clk_info.current_ql = sync_entry->current_ql;
      }
      sync_info->ext_clk_info.state = sync_entry->state;
      sync_info->ext_clk_info.clk_idx = sync_entry->clk_idx;
      sync_info->ext_clk_info.rank = sync_entry->rank;
      sync_info->ext_clk_info.clk_state = sync_entry->clk_state;
      sync_info->ext_clk_info.ref_mon_status = sync_entry->ref_mon_status;
      break;

    case E_sync_type_tx_only:
      sync_info->synce_tx_only_info.tx_bundle_num = sync_entry->tx_bundle_num;
      sync_info->synce_tx_only_info.port_link_down_flag = sync_entry->port_link_down_flag;
      break;

    default:
      break;
  }

  os_mutex_unlock(&g_control_mutex);

  return 0;
}

int control_get_sync_info_by_name(const char *port_name, T_management_sync_info *sync_info)
{
  int i;
  T_sync_entry *sync_entry;

  memset(sync_info, 0, sizeof(*sync_info));

  /* Do not need to lock mutex */
  for(i = 0; i < g_control_data.num_syncs; i++) {
    sync_entry = &g_control_data.sync_table[i];
    if(!strcmp(sync_entry->name, port_name)) {
      control_get_sync_info_by_index(i, sync_info);
      return 0;
    }
  }

  return -1;
}

int control_clear_synce_clk_wtr_timer(const char *port_name)
{
  int i;
  T_sync_entry *sync_entry;
  int cleared_flag = 0;

  os_mutex_lock(&g_control_mutex);
  for(i = 0; i < g_control_data.num_syncs; i++) {
    sync_entry = &g_control_data.sync_table[i];
    if(!strcmp(sync_entry->name, port_name)) {
      if((sync_entry->type == E_sync_type_external) || (sync_entry->type == E_sync_type_tx_only)) {
        os_mutex_unlock(&g_control_mutex);
        pr_err("Attempted to clear wait-to-restore timer for %s port %s",
               conv_sync_type_enum_to_str(sync_entry->type),
               port_name);
        return -2;
      }
      sync_entry->temporary_state_monotonic_time_ms = 0;
      cleared_flag = 1;
      break;
    }
  }
  os_mutex_unlock(&g_control_mutex);

  if(cleared_flag == 0) {
    return -1;
  }

  pr_info("Cleared wait-to-restore timer for Sync-E port %s", port_name);

  return 0;
}

int control_update_sync_table_entry_clk_idx(const char *new_port_name, int clk_idx)
{
  int i;
  T_sync_entry *new_sync_entry = NULL;
  T_sync_entry *sync_entry;

  os_mutex_lock(&g_control_mutex);
  for(i = 0; i < g_control_data.num_syncs; i++) {
    sync_entry = &g_control_data.sync_table[i];
    if(!strcmp(sync_entry->name, new_port_name)) {
      new_sync_entry = sync_entry;
      break;
    }
  }

  if(new_sync_entry == NULL) {
    os_mutex_unlock(&g_control_mutex);
    pr_err("Specified port %s does not exist", new_port_name);
    return -1;
  }

  if(new_sync_entry->clk_idx == clk_idx) {
    os_mutex_unlock(&g_control_mutex);
    pr_info("%s is already assigned to clock index %d",
            new_port_name,
            clk_idx);
    return 0;
  }

  for(i = 0; i < g_control_data.num_syncs; i++) {
    sync_entry = &g_control_data.sync_table[i];

    if(sync_entry->clk_idx == clk_idx) {
      sync_entry->type = E_sync_type_monitoring;
      sync_entry->clk_idx = MISSING_CLK_IDX;
    }
  }

  new_sync_entry->type = E_sync_type_synce;
  new_sync_entry->clk_idx = clk_idx;
  
  /* Trigger priority table update due to change in clock index */
  g_control_data.update_priority_table_flag = 1;

  os_mutex_unlock(&g_control_mutex);

  pr_info("%s becomes active Sync-E clock port for clock index %d",
          new_port_name,
          clk_idx);

  return 0;
}

int control_set_pri(const char *port_name, int pri)
{
  int i;
  T_sync_entry *sync_entry;
  T_sync_entry *target_sync_entry = NULL;
  int old_pri;

  if(g_control_data.lo_pri == pri) {
    pr_err("Priority is already set to %d for LO", pri);
    return -1;
  }

  os_mutex_lock(&g_control_mutex);
  for(i = 0; i < g_control_data.num_syncs; i++) {
    sync_entry = &g_control_data.sync_table[i];
    if(!strcmp(sync_entry->name, port_name)) {
      if(sync_entry->config_pri == pri) {
        os_mutex_unlock(&g_control_mutex);
        pr_err("Priority is already set to %d for port %s", pri, port_name);
        return -1;
      }
      /* Save entry */
      target_sync_entry = sync_entry;
      old_pri = sync_entry->config_pri;
    } else if(sync_entry->config_pri == pri) {
      os_mutex_unlock(&g_control_mutex);
      pr_err("Failed to set priority %d for %s because priority already assigned to %s", pri, port_name, sync_entry->name);
      return -1;
    }
  }

  if(target_sync_entry == NULL) {
    os_mutex_unlock(&g_control_mutex);
    return -1;
  }

  target_sync_entry->config_pri = pri;

  os_mutex_unlock(&g_control_mutex);

  pr_info("Changed priority from %d to %d for port %s",
          old_pri,
          pri,
          port_name);

  return 0;
}

unsigned long long control_get_tx_bundle_bitmap(int sync_idx)
{
  unsigned long long tx_bundle_bitmap;
  int tx_bundle_num;
  int i;
  T_sync_entry *sync_entry;

  if(sync_idx == INVALID_SYNC_IDX) {
    return 0;
  }

  os_mutex_lock(&g_control_mutex);
  sync_entry = &g_control_data.sync_table[sync_idx];

  if(sync_entry->type == E_sync_type_external) {
    os_mutex_unlock(&g_control_mutex);
    return 0;
  }

  tx_bundle_num = sync_entry->tx_bundle_num;
  if(tx_bundle_num == NO_TX_BUNDLE_NUM) {
    tx_bundle_bitmap = (unsigned long long)1 << sync_idx;
  } else {
    tx_bundle_bitmap = 0;
    for(i = 0; i < g_control_data.num_syncs; i++) {
      sync_entry = &g_control_data.sync_table[i];
      if(tx_bundle_num == sync_entry->tx_bundle_num) {
        tx_bundle_bitmap |= (unsigned long long)1 << i;
      }
    }
  }

  os_mutex_unlock(&g_control_mutex);

  return tx_bundle_bitmap;
}

void control_deinit(void)
{
  /* Deinitialize mutex */
  os_mutex_deinit(&g_control_mutex);

  g_control_data.net_opt = E_esmc_network_option_max;
  g_control_data.no_ql_en = 0;
  g_control_data.synce_forced_ql_en = 0;
  g_control_data.lo_ql = E_esmc_ql_max;
  g_control_data.lo_pri = 255;
  g_control_data.do_not_use_ql = E_esmc_ql_max;
  g_control_data.hold_off_timer_ms = 0;
  g_control_data.wait_to_restore_timer_s = 0;
  g_control_data.num_syncs = 0;
  free(g_control_data.sync_table);
  g_control_data.sync_table = NULL;
  g_control_data.update_priority_table_flag = 0;
}
