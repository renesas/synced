/**
 * @file management.c
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

#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "management.h"
#include "pcm4l_msg.h"
#include "../common/common.h"
#include "../common/print.h"
#include "../control/control.h"
#include "../device/device_adaptor/device_adaptor.h"
#include "../esmc/esmc_adaptor/esmc_adaptor.h"
#include "../monitor/monitor.h"

/*
 * External Mux Control:
 *
 * ENABLE_EXTERNAL_MUX_CONTROL enables a generic code
 * to control multiple external muxes
 * with multiple ports as inputs and up to two outputs.
 *           Mux
 *           ____
 *   In0 ___|    \
 *   In1 ___|     |___ Primary output
 *   In2 ___|     |___ Secondary output
 *   In3 ___|     |
 *          |____/
 *            |
 *           Sel
 * The mux is able to forward two inputs to the outputs.
 * The mux control procedure is responsible for selecting the best clock and the second best clock.
 * If the mux has only one output, then the secondary output must be set to -1.
 */
#define ENABLE_EXTERNAL_MUX_CONTROL   0

#if ENABLE_EXTERNAL_MUX_CONTROL
#define EXTERNAL_MUX_NUM     4
#define EXTERNAL_MUX_PORTS   4

typedef struct {
  /* Static info, initialized according to hardware settings */
  const char *names[EXTERNAL_MUX_PORTS];
  const int primary_clk_idx;
  const int secondary_clk_idx;
  /* Dynamic info */
  int ranks[EXTERNAL_MUX_PORTS]; /* Initialize all with maximum value */
  int chosen_primary_port_idx;   /* Initialize according to hardware settings or -1 (unknown) */
  int chosen_secondary_port_idx; /* Initialize according to hardware settings or -1 (unknown) */
} T_ext_mux_entry;
#endif

/* Static data */

static T_esmc_ql g_current_ql;

#if ENABLE_EXTERNAL_MUX_CONTROL
static T_ext_mux_entry g_external_mux_table[EXTERNAL_MUX_NUM] = {
  /* Mux 0 */
  {
    .names = {"eth13", "eth2", "eth7", ""},
    .primary_clk_idx = 2,
    .secondary_clk_idx = -1,                       /* No secondary output */
    .ranks = {INT_MAX, INT_MAX, INT_MAX, INT_MAX},
    .chosen_primary_port_idx = -1,
    .chosen_secondary_port_idx = -1
  },
  /* Mux 1 */
  {
    .names = {"eth8", "eth1", "eth14", "eth15"},
    .primary_clk_idx = 3,
    .secondary_clk_idx = 8,
    .ranks = {INT_MAX, INT_MAX, INT_MAX, INT_MAX},
    .chosen_primary_port_idx = -1,
    .chosen_secondary_port_idx = -1
  },
  /* Mux 2 */
  {
    .names = {"eth6", "eth12", "", ""},
    .primary_clk_idx = 1,
    .secondary_clk_idx = -1,                       /* No secondary output */
    .ranks = {INT_MAX, INT_MAX, INT_MAX, INT_MAX},
    .chosen_primary_port_idx = -1,
    .chosen_secondary_port_idx = -1
  },
  /* Mux 3 */
  {
    .names = {"eth3", "eth5", "eth9", "eth11"},
    .primary_clk_idx = 5,
    .secondary_clk_idx = 6,
    .ranks = {INT_MAX, INT_MAX, INT_MAX, INT_MAX},
    .chosen_primary_port_idx = -1,
    .chosen_secondary_port_idx = -1
  }
};
#endif

static T_management_callbacks g_management_callbacks;
static int g_management_init_flag = 0;

/* Static functions */

static void management_template_notify_current_ql(const char *port_name, T_esmc_ql current_ql)
{
  /* This function is a template; the user must implement their own code here or register their own functions using management_init(). */

  /* Notify pcm4l */
  pcm4l_msg_set_clock_category(current_ql, 0);

  g_current_ql = current_ql;

  pr_info("Selected port is %s and current QL is %s (%d)", port_name, conv_ql_enum_to_str(current_ql), current_ql);
}

#if ENABLE_EXTERNAL_MUX_CONTROL
static void management_example_ext_mux_control(const char *port_name, T_esmc_ql current_ql, int rank)
{
  /* This function is an example on how to control external muxes. */
  int mux_idx;
  int port_idx;
  T_ext_mux_entry *mux_entry;
  int primary_port_idx;
  int secondary_port_idx;
  int primary_rank;
  int secondary_rank;
  T_management_api_response rsp;

  (void)current_ql;

  for(mux_idx = 0; mux_idx < EXTERNAL_MUX_NUM; mux_idx++) {
    mux_entry = &g_external_mux_table[mux_idx];
    for(port_idx = 0; port_idx < EXTERNAL_MUX_PORTS; port_idx++) {
      if(!strcmp(port_name, mux_entry->names[port_idx])) {
        /* Found the mux for this port, so update the rank */
        mux_entry->ranks[port_idx] = rank;
        goto found;
      }
    }
  }

  return;

found:
  /* Search for the best and second best ports of this mux (mux_idx) */
  primary_port_idx = -1;
  secondary_port_idx = -1;
  primary_rank = INT_MAX;
  secondary_rank = INT_MAX;
  for(port_idx = 0; port_idx < EXTERNAL_MUX_PORTS; port_idx++) {
    /* The smaller the rank, the better */
    if(primary_rank > mux_entry->ranks[port_idx]) {
      secondary_rank = primary_rank;
      secondary_port_idx = primary_port_idx;
      primary_rank = mux_entry->ranks[port_idx];
      primary_port_idx = port_idx;
    } else if(secondary_rank > mux_entry->ranks[port_idx]) {
      secondary_rank = mux_entry->ranks[port_idx];
      secondary_port_idx = port_idx;
    }
  }

  if((mux_entry->primary_clk_idx >= 0) &&
     (mux_entry->primary_clk_idx < MAX_NUM_OF_CLOCKS) &&
     (primary_port_idx != mux_entry->chosen_primary_port_idx)) {
    /* Notify control module */
    rsp = management_assign_new_synce_clk_port(0,                                   /* print_flag */
                                               mux_entry->primary_clk_idx,
                                               mux_entry->names[primary_port_idx]);
    if(E_management_api_response_ok == rsp) {
      /* Call the function to select port 'primary_port_idx' on mux mux_idx here */

      mux_entry->chosen_primary_port_idx = primary_port_idx;
      pr_info("%s: selected port %s on external mux %d for primary clock index %d", __func__, mux_entry->names[primary_port_idx], mux_idx, mux_entry->primary_clk_idx);
    } else {
      pr_warning("%s: failed to select port %s on external mux %d for primary clock index %d", __func__, mux_entry->names[primary_port_idx], mux_idx, mux_entry->primary_clk_idx);
    }
  }

  if((mux_entry->secondary_clk_idx >= 0) &&
     (mux_entry->secondary_clk_idx < MAX_NUM_OF_CLOCKS) &&
     (secondary_port_idx != mux_entry->chosen_secondary_port_idx)) {
    /* Notify control module */
    rsp = management_assign_new_synce_clk_port(0,                                     /* print_flag */
                                               mux_entry->secondary_clk_idx,
                                               mux_entry->names[secondary_port_idx]);
    if(E_management_api_response_ok == rsp) {
      /* Call the function to select port 'secondary_port_idx' on mux mux_idx here */

      mux_entry->chosen_secondary_port_idx = secondary_port_idx;
      pr_info("%s: selected port %s on external mux %d for secondary clock index %d", __func__, mux_entry->names[secondary_port_idx], mux_idx, mux_entry->secondary_clk_idx);
    } else {
      pr_warning("%s: failed to select port %s on external mux %d for secondary clock index %d", __func__, mux_entry->names[secondary_port_idx], mux_idx, mux_entry->secondary_clk_idx);
    }
  }
}
#else
static void management_template_notify_sync_current_ql(const char *port_name, T_esmc_ql current_ql, int rank)
{
  /* This function is a template; the user must implement their own code here or register their own functions using management_init(). */

  pr_info("Current QL for port %s changed to %s (%d); new rank: 0x%06X", port_name, conv_ql_enum_to_str(current_ql), current_ql, rank);
}
#endif

static void management_template_notify_synce_dpll_current_state(T_device_dpll_state synce_dpll_state)
{
  /* This function is a template; the user must implement their own code here or register their own functions using management_init(). */

  pr_info("Current Sync-E DPLL state changed to %s", conv_synce_dpll_state_enum_to_str(synce_dpll_state));
}

static void management_template_notify_sync_current_clk_state(const char *port_name, int clk_idx, T_sync_clk_state clk_state)
{
  /* This function is a template; the user must implement their own code here or register their own functions using management_init(). */

  pr_info("State of clock index %d associated with port %s changed to %s", clk_idx, port_name, conv_sync_clk_state_enum_to_str(clk_state));
}

static void management_template_notify_sync_current_state(const char *port_name, T_sync_state state)
{
  /* This function is a template; the user must implement their own code here or register their own functions using management_init(). */

  pr_info("Current state for port %s changed to %s", port_name, conv_sync_state_enum_to_str(state));
}

static void management_template_notify_alarm(const T_alarm_data *alarm_data)
{
  /* This function is a template; the user must implement their own code here or register their own functions using management_init(). */

  switch(alarm_data->alarm_type) {
    case E_alarm_type_invalid_clock_idx:
      pr_warning("Invalid current clock index");
      break;

    case E_alarm_type_invalid_sync_idx:
      pr_warning("Invalid current sync index");
      break;

    case E_alarm_type_timing_loop:
      pr_warning("%s timing loop detected on port %s (ESMC PDU received from MAC address %02X:%02X:%02X:%02X:%02X:%02X)",
                 (alarm_data->alarm_timing_loop.loop_type == E_timing_loop_type_originator) ? "Originator clock" : "Immediate",
                 alarm_data->alarm_timing_loop.port_name,
                 alarm_data->alarm_timing_loop.mac_addr[0],
                 alarm_data->alarm_timing_loop.mac_addr[1],
                 alarm_data->alarm_timing_loop.mac_addr[2],
                 alarm_data->alarm_timing_loop.mac_addr[3],
                 alarm_data->alarm_timing_loop.mac_addr[4],
                 alarm_data->alarm_timing_loop.mac_addr[5]);
      break;

    case E_alarm_type_invalid_rx_ql:
      pr_warning("Invalid received QL on port %s", alarm_data->alarm_invalid_ql.port_name);
      break;

    default:
      break;
  }
}

static void management_template_notify_pcm4l_connection_status(int on)
{
  /* This function is a template; the user must implement their own code here or register their own functions using management_init(). */

  if(on) {
    /* Update pcm4l with the current QL */
    if(g_current_ql < E_esmc_ql_max) {
      pcm4l_msg_set_clock_category(g_current_ql, 0);
    }
    pr_info("pcm4l connection is on");
  } else {
    pr_warning("pcm4l connection is off");
  }
}

/* Global functions */

int management_init(void)
{
  if(g_management_init_flag) {
    pr_warning("Management interface already initialized");
    return 0;
  }

  g_current_ql = E_esmc_ql_max;

  /* Register management callbacks */
  memset(&g_management_callbacks, 0, sizeof(g_management_callbacks));
  g_management_callbacks.notify_current_ql = &management_template_notify_current_ql;
#if ENABLE_EXTERNAL_MUX_CONTROL
  g_management_callbacks.notify_sync_current_ql = &management_example_ext_mux_control;
#else
  g_management_callbacks.notify_sync_current_ql = &management_template_notify_sync_current_ql;
#endif
  g_management_callbacks.notify_synce_dpll_current_state = &management_template_notify_synce_dpll_current_state;
  g_management_callbacks.notify_sync_current_clk_state = &management_template_notify_sync_current_clk_state;
  g_management_callbacks.notify_sync_current_state = &management_template_notify_sync_current_state;
  g_management_callbacks.notify_alarm = &management_template_notify_alarm;
  g_management_callbacks.notify_pcm4l_connection_status = &management_template_notify_pcm4l_connection_status;

  g_management_init_flag = 1;

  return 0;
}

int management_deinit(void)
{
  if(!g_management_init_flag) {
    pr_warning("Management interface already deinitialized");
    return 0;
  }

  memset(&g_management_callbacks, 0, sizeof(g_management_callbacks));

  g_management_init_flag = 0;

  return 0;
}

/* Callback management APIs */

void management_call_notify_current_ql_cb(const char *port_name, T_esmc_ql current_ql)
{
  if(g_management_callbacks.notify_current_ql != NULL) {
    g_management_callbacks.notify_current_ql(port_name, current_ql);
  }
}

void management_call_notify_sync_current_ql_cb(const char *port_name, T_esmc_ql current_ql, int rank)
{
  if(port_name == NULL) {
    return;
  }

  if(g_management_callbacks.notify_sync_current_ql != NULL) {
    g_management_callbacks.notify_sync_current_ql(port_name, current_ql, rank);
  }
}

void management_call_notify_synce_dpll_current_state_cb(T_device_dpll_state synce_dpll_state)
{
  if(g_management_callbacks.notify_synce_dpll_current_state != NULL) {
    g_management_callbacks.notify_synce_dpll_current_state(synce_dpll_state);
  }
}

void management_call_notify_sync_current_clk_state_cb(const char *port_name, int clk_idx, T_sync_clk_state clk_state)
{
  if(port_name == NULL) {
    return;
  }

  if(g_management_callbacks.notify_sync_current_clk_state != NULL) {
    g_management_callbacks.notify_sync_current_clk_state(port_name, clk_idx, clk_state);
  }
}

void management_call_notify_sync_current_state_cb(const char *port_name, T_sync_state state)
{
  if(port_name == NULL) {
    return;
  }

  if(g_management_callbacks.notify_sync_current_state != NULL) {
    g_management_callbacks.notify_sync_current_state(port_name, state);
  }
}

void management_call_notify_alarm_cb(const T_alarm_data *alarm_data)
{
  if(alarm_data == NULL) {
    return;
  }

  if(g_management_callbacks.notify_alarm != NULL) {
    g_management_callbacks.notify_alarm(alarm_data);
  }
}

void management_call_notify_pcm4l_connection_status_cb(int on)
{
  if(g_management_callbacks.notify_pcm4l_connection_status != NULL) {
    g_management_callbacks.notify_pcm4l_connection_status(on);
  }
}

/* Generic management APIs */

T_management_api_response management_get_sync_info_list(int print_flag, int max_num_syncs, T_management_sync_info *sync_info_list, int *num_syncs)
{
  T_management_sync_info *sync_info_list_entry;
  int i;
  int num_entries_returned = 0;

  if(max_num_syncs == 0) {
    return E_management_api_response_invalid;
  }

  if(sync_info_list == NULL) {
    return E_management_api_response_invalid;
  }

  if(print_flag) {
    pr_info("**%s**", __func__);
  }

  sync_info_list_entry = sync_info_list;

  for(i = 0; i < max_num_syncs; i++) {

    if(control_get_sync_info_by_index(i, sync_info_list_entry) < 0) {
      *num_syncs = num_entries_returned;
      return E_management_api_response_ok;
    }

    if(print_flag) {
      print_sync_info(sync_info_list_entry);
    }

    sync_info_list_entry++;
    num_entries_returned++;
  }

  *num_syncs = num_entries_returned;
  return E_management_api_response_ok;
}

T_management_api_response management_get_current_status(int print_flag, T_management_status *status)
{
  if(status == NULL) {
    return E_management_api_response_invalid;
  }

  monitor_get_current_status(&status->current_ql,
                             status->port_name,
                             &status->clk_idx,
                             &status->dpll_state,
                             &status->holdover_remaining_time_ms);

  if(print_flag) {
    pr_info("**%s**", __func__);
    print_current_status(status);
  }

  return E_management_api_response_ok;
}

T_management_api_response management_get_sync_info(int print_flag, const char *port_name, T_management_sync_info *sync_info)
{
  if(port_name == NULL) {
    return E_management_api_response_invalid;
  }

  if(sync_info == NULL) {
    return E_management_api_response_invalid;
  }

  if(print_flag) {
    pr_info("**%s**", __func__);
  }

  if(control_get_sync_info_by_name(port_name, sync_info) < 0) {
    pr_err("Failed to find port %s", port_name);
    return E_management_api_response_failed;
  }

  if(print_flag) {
    print_sync_info(sync_info);
  }

  return E_management_api_response_ok;
}

T_management_api_response management_set_forced_ql(int print_flag, const char *port_name, T_esmc_ql forced_ql)
{
  int resp;

  if(port_name == NULL) {
    return E_management_api_response_invalid;
  }

  if(print_flag) {
    pr_info("**%s**", __func__);
  }

  resp = control_set_forced_ql(port_name, forced_ql);
  if(resp < 0) {
    pr_err("Failed to set forced QL for port %s", port_name);
    if(resp == -1) {
      return E_management_api_response_failed;
    } else {
      return E_management_api_response_not_supported;
    }
  }

  return E_management_api_response_ok;
}

T_management_api_response management_clear_forced_ql(int print_flag, const char *port_name)
{
  int resp;

  if(port_name == NULL) {
    return E_management_api_response_invalid;
  }

  if(print_flag) {
    pr_info("**%s**", __func__);
  }

  resp = control_clear_forced_ql(port_name);
  if(resp < 0) {
    pr_err("Failed to clear forced QL for port %s", port_name);
    if(resp == -1) {
      return E_management_api_response_failed;
    } else {
      return E_management_api_response_not_supported;
    }
  }

  return E_management_api_response_ok;
}

T_management_api_response management_clear_holdover_timer(int print_flag)
{
  if(print_flag) {
    pr_info("**%s**", __func__);
  }

  monitor_clear_holdover_timer();

  return E_management_api_response_ok;
}

T_management_api_response management_clear_synce_clk_wtr_timer(int print_flag, const char *port_name)
{
  int resp;

  if(port_name == NULL) {
    return E_management_api_response_invalid;
  }

  if(print_flag) {
    pr_info("**%s**", __func__);
  }

  resp = control_clear_synce_clk_wtr_timer(port_name);
  if(resp < 0) {
    pr_err("Failed to clear wait-to-restore timer for port %s", port_name);
    if(resp == -1) {
      return E_management_api_response_failed;
    } else {
      return E_management_api_response_not_supported;
    }
  }

  return E_management_api_response_ok;
}

T_management_api_response management_assign_new_synce_clk_port(int print_flag, int clk_idx, const char *port_name)
{
  if(print_flag) {
    pr_info("**%s**", __func__);
  }

  /* Check if clock index is valid */
  if((clk_idx < 0) || (clk_idx >= MAX_NUM_OF_CLOCKS)) {
    pr_err("Invalid clock index %d", clk_idx);
    return E_management_api_response_invalid;
  }

  if(port_name == NULL) {
    return E_management_api_response_invalid;
  }

  /* Update sync table */
  if(control_update_sync_table_entry_clk_idx(port_name, clk_idx) < 0) {
    pr_err("Failed to assign new Sync-E clock port %s", port_name);
    return E_management_api_response_failed;
  }

  return E_management_api_response_ok;
}

T_management_api_response management_set_pri(int print_flag, const char *port_name, int pri)
{
  if(print_flag) {
    pr_info("**%s**", __func__);
  }

  if(port_name == NULL) {
    return E_management_api_response_invalid;
  }

  /* Check if priority is valid */
  if((pri < 0) || (pri > 255)) {
    pr_err("Invalid priority %d", pri);
    return E_management_api_response_invalid;
  }

  if(control_set_pri(port_name, pri) < 0) {
    pr_err("Failed to set priority for port %s", port_name);
    return E_management_api_response_failed;
  }

  return E_management_api_response_ok;
}

T_management_api_response management_set_max_msg_level(int print_flag, int max_msg_lvl)
{
  if(print_flag) {
    pr_info("**%s**", __func__);
  }

  /* Check if message level is valid */
  if((max_msg_lvl < PRINT_LEVEL_MIN) || (max_msg_lvl > PRINT_LEVEL_MAX)) {
    pr_err("Invalid message level %d", max_msg_lvl);
    return E_management_api_response_invalid;
  }

  print_set_max_msg_level(max_msg_lvl);

  pr_info("Updated max message level to %d", max_msg_lvl);

  return E_management_api_response_ok;
}
