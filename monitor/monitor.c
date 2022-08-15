/**
 * @file monitor.c
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
* Release Tag: 1-0-2
* Pipeline ID: 118059
* Commit Hash: 5a4424ad
********************************************************************************************************************/

#include <string.h>

#include "management.h"
#include "monitor.h"
#include "pcm4l_if.h"
#include "../common/common.h"
#include "../common/print.h"
#include "../control/control.h"
#include "../device/device_adaptor/device_adaptor.h"
#include "../esmc/esmc_adaptor/esmc_adaptor.h"

/* Static data */

static T_esmc_ql g_lo_ql;
static T_esmc_ql g_holdover_ql;
static unsigned int g_holdover_timer_s;
static pthread_mutex_t g_monitor_mutex;
static T_monitor_data g_monitor_data;

/* Global functions */

int monitor_init(T_monitor_config const *monitor_config)
{
  /* Initialize mutex */
  if(os_mutex_init(&g_monitor_mutex) < 0) {
    return -1;
  }

  g_lo_ql = monitor_config->lo_ql;
  g_holdover_ql = monitor_config->holdover_ql;
  g_holdover_timer_s = monitor_config->holdover_timer_s;

  /* Set local data */
  memset(&g_monitor_data, 0, sizeof(g_monitor_data));

  g_monitor_data.holdover_monotonic_time_ms = 0;
  g_monitor_data.current_synce_dpll_state = E_device_dpll_state_max;
  g_monitor_data.current_ql = E_esmc_ql_max;
  g_monitor_data.current_clk_idx = INVALID_CLK_IDX;
  g_monitor_data.current_sync_idx = INVALID_SYNC_IDX;

  return 0;
}

void monitor_determine_ql(void)
{
  T_device_dpll_state synce_dpll_state;
  T_device_dpll_state old_synce_dpll_state;
  int clk_idx;
  int old_clk_idx;
  T_esmc_ql ql;
  T_esmc_ql old_ql;
  int sync_idx;
  int old_sync_idx;
  unsigned long long tx_bundle_bitmap;
  T_alarm_data alarm_data;

  /* Get status of Sync-E DPLL */
  synce_dpll_state = device_get_synce_dpll_state();
  if(synce_dpll_state >= E_device_dpll_state_max) {
    pr_warning("Sync-E DPLL is in unsupported state");
    return;
  }

  old_synce_dpll_state = g_monitor_data.current_synce_dpll_state;
  old_clk_idx = g_monitor_data.current_clk_idx;
  old_ql = g_monitor_data.current_ql;
  old_sync_idx = g_monitor_data.current_sync_idx;

  if((synce_dpll_state == E_device_dpll_state_lock_acquisition) ||
     (synce_dpll_state == E_device_dpll_state_lock_recovery) ||
     (synce_dpll_state == E_device_dpll_state_locked)) {
    /* Sync-E DPLL is in lock acquisition, lock recovery, or locked state and is tracking a clock */

    /* Get clock index of current clock */
    clk_idx = device_get_current_clk_idx();
    if(clk_idx == INVALID_CLK_IDX) {
      alarm_data.alarm_type = E_alarm_type_invalid_clock_idx;
      management_call_notify_alarm_cb(&alarm_data);
      return;
    }

    /* Get the selected sync index */
    sync_idx = control_get_sync_idx(clk_idx);
    if(sync_idx == INVALID_SYNC_IDX) {
      alarm_data.alarm_type = E_alarm_type_invalid_sync_idx;
      management_call_notify_alarm_cb(&alarm_data);
      return;
    }

    /* Get QL of current clock */
    ql = control_get_ql(sync_idx);
    if(ql >= E_esmc_ql_max) {
      /* Should not happen */
      return;
    }

    if(ql > g_lo_ql) {
      /* Should not happen */
      pr_warning("%s (%d) is worse than LO QL %s (%d)", conv_ql_enum_to_str(ql), ql, conv_ql_enum_to_str(g_lo_ql), g_lo_ql);
      return;
    }

    /* Update the monitor data */
    os_mutex_lock(&g_monitor_mutex);
    g_monitor_data.current_synce_dpll_state = synce_dpll_state;
    g_monitor_data.current_clk_idx = clk_idx;
    g_monitor_data.current_ql = ql;
    g_monitor_data.current_sync_idx = sync_idx;
    os_mutex_unlock(&g_monitor_mutex);

    if(ql != old_ql) {
      /* Change in current QL */
      pr_info("Current QL %s from %s (%d) to %s (%d)",
              (ql < old_ql) ? "upgraded" : "degraded",
              conv_ql_enum_to_str(old_ql),
              old_ql,
              conv_ql_enum_to_str(ql),
              ql);
    }
  } else {
    /* Sync-E DPLL is in freerun or holdover state and is not tracking a clock (i.e. clock is LO) */
    clk_idx = INVALID_CLK_IDX;
    sync_idx = INVALID_SYNC_IDX;

    /* Update the monitor data */
    os_mutex_lock(&g_monitor_mutex);
    if(synce_dpll_state == E_device_dpll_state_freerun) {
      /* Sync-E DPLL is in freerun state (set QL to LO QL) */
      ql = g_lo_ql;
    } else {
      /* Sync-E DPLL is in holdover state (set QL to holdover QL and then to LO QL once holdover timer expires) */
      if(old_synce_dpll_state == E_device_dpll_state_locked) {
        /* Just entered Holdover from Locked state. Advertise holdover QL until holdover timer expires */
        g_monitor_data.holdover_monotonic_time_ms = os_get_monotonic_milliseconds() + (g_holdover_timer_s * 1000);
        /* The new QL is the worst between the previous QL and the holdover QL */
        ql = (old_ql > g_holdover_ql) ? old_ql : g_holdover_ql;
      } else {
        ql = old_ql;
      }

      if(os_get_monotonic_milliseconds() > g_monitor_data.holdover_monotonic_time_ms) {
        /* Advertise LO QL because holdover timer expired */
        ql = g_lo_ql;
      }
    }
    g_monitor_data.current_synce_dpll_state = synce_dpll_state;
    g_monitor_data.current_clk_idx = clk_idx;
    g_monitor_data.current_ql = ql;
    g_monitor_data.current_sync_idx = sync_idx;
    os_mutex_unlock(&g_monitor_mutex);

    if(ql != old_ql) {
      /* Change in current QL */
      if(ql == g_lo_ql) {
        pr_info("Current QL is set to LO QL %s (%d)", conv_ql_enum_to_str(ql), ql);
      } else {
        pr_info("Current QL is set to temporary holdover QL %s (%d)", conv_ql_enum_to_str(ql), ql);
      }
    }
  }

  if((ql != old_ql) || (clk_idx != old_clk_idx) || (sync_idx != old_sync_idx)) {
    /* If QL, clock index or sync index has changed, then update ESMC TX QL */
    tx_bundle_bitmap = control_get_tx_bundle_bitmap(sync_idx);
    esmc_adaptor_set_tx_ql(ql, sync_idx, tx_bundle_bitmap);
  }

  if(synce_dpll_state != old_synce_dpll_state) {
    management_call_notify_synce_dpll_current_state_cb(synce_dpll_state);
  }

  if(ql != old_ql) {
    management_call_notify_current_ql_cb(ql);
  }
}

void monitor_get_current_status(T_esmc_ql *current_ql,
                                char *port_name,
                                int *clk_idx,
                                T_device_dpll_state *dpll_state,
                                unsigned int *holdover_remaining_time_ms)
{
  unsigned long long monotonic_time_now_ms;

  os_mutex_lock(&g_monitor_mutex);
  *current_ql = g_monitor_data.current_ql;
  control_get_sync_name(g_monitor_data.current_sync_idx, port_name);
  *clk_idx = g_monitor_data.current_clk_idx;
  *dpll_state = g_monitor_data.current_synce_dpll_state;
  *holdover_remaining_time_ms = 0;
  if(*dpll_state == E_device_dpll_state_holdover) {
    monotonic_time_now_ms = os_get_monotonic_milliseconds();
    if(monotonic_time_now_ms >= g_monitor_data.holdover_monotonic_time_ms) {
      *dpll_state = E_device_dpll_state_freerun;
    } else {
      *holdover_remaining_time_ms = (unsigned int)(g_monitor_data.holdover_monotonic_time_ms - monotonic_time_now_ms);
    }
  }
  os_mutex_unlock(&g_monitor_mutex);
}

void monitor_clear_holdover_timer(void)
{
  if(g_holdover_timer_s == 0) {
    pr_warning("Holdover timer is already configured to 0 milliseconds");
    return;
  }
  os_mutex_lock(&g_monitor_mutex);
  g_monitor_data.holdover_monotonic_time_ms = 0;
  os_mutex_unlock(&g_monitor_mutex);
  pr_info("Cleared holdover timer");
}

int monitor_deinit(void)
{
  /* Deinitialize mutex */
  os_mutex_deinit(&g_monitor_mutex);

  g_lo_ql = E_esmc_ql_max;
  g_holdover_ql = E_esmc_ql_max;

  /* Clear local data */
  g_holdover_timer_s = 0;
  g_monitor_data.holdover_monotonic_time_ms = 0;
  g_monitor_data.current_synce_dpll_state = E_device_dpll_state_max;
  g_monitor_data.current_ql = E_esmc_ql_max;

  return 0;
}
