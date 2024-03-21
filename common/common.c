/**
 * @file common.c
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

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "print.h"

/* Static data */

/* See T_esmc_ql in types.h */
static const char *g_ql_enum_to_str[] = {
  /* Network option 1 start */
  "QL-ePRTC",
  "QL-PRTC",
  "QL-ePRC",
  "QL-PRC",
  "QL-SSUA",
  "QL-SSUB",
  "QL-eSEC",
  "QL-SEC",
  "QL-DNU",
  "QL-INV0",
  "QL-INV1",
  "QL-INV3",
  "QL-INV5",
  "QL-INV6",
  "QL-INV7",
  "QL-INV9",
  "QL-INV10",
  "QL-INV12",
  "QL-INV13",
  "QL-INV14",
  /* Network option 1 end */

  /* Network option 2 start */
  "QL-ePRTC",
  "QL-PRTC",
  "QL-ePRC",
  "QL-PRS",
  "QL-STU",
  "QL-ST2",
  "QL-TNC",
  "QL-ST3E",
  "QL-eEEC",
  "QL-ST3",
  "QL-SMC",
  "QL-ST4",
  "QL-PROV",
  "QL-DUS",
  "QL-INV2",
  "QL-INV3",
  "QL-INV5",
  "QL-INV6",
  "QL-INV8",
  "QL-INV9",
  "QL-INV11",
  /* Network option 2 end */

  /* Network option 3 start */
  "QL-ePRTC",
  "QL-PRTC",
  "QL-UNK",
  "QL-eSEC",
  "QL-SEC",
  "QL-INV1",
  "QL-INV2",
  "QL-INV3",
  "QL-INV4",
  "QL-INV5",
  "QL-INV6",
  "QL-INV7",
  "QL-INV8",
  "QL-INV9",
  "QL-INV10",
  "QL-INV12",
  "QL-INV13",
  "QL-INV14",
  "QL-INV15",

  "QL-NSUPP",
  "QL-UNC",
  "QL-Failed"
  /* Network option 3 end */
};
COMPILE_TIME_ASSERT((sizeof(g_ql_enum_to_str)/sizeof(g_ql_enum_to_str[0])) == E_esmc_ql_max, "Invalid array size for g_ql_enum_to_str!")

/* See T_device_dpll_state in device_adaptor.h */
static const char *g_device_dpll_state_enum_to_str[] = {
  "freerun",
  "lock acquisition-recovery",
  "locked",
  "holdover"
};
COMPILE_TIME_ASSERT((sizeof(g_device_dpll_state_enum_to_str)/sizeof(g_device_dpll_state_enum_to_str[0])) == E_device_dpll_state_max, "Invalid array size for g_device_dpll_state_enum_to_str!")

/* See T_sync_clk_state in sync.h */
static const char *g_sync_clk_state_enum_to_str[] = {
  "unqualified",
  "qualified"
};
COMPILE_TIME_ASSERT((sizeof(g_sync_clk_state_enum_to_str)/sizeof(g_sync_clk_state_enum_to_str[0])) == E_sync_clk_state_max, "Invalid array size for g_sync_clk_state_enum_to_str!")

/* See T_sync_state in sync.h */
static const char *g_sync_state_enum_to_str[] = {
  "normal",
  "hold-off",
  "wait-to-restore",
  "forced"
};
COMPILE_TIME_ASSERT((sizeof(g_sync_state_enum_to_str)/sizeof(g_sync_state_enum_to_str[0])) == E_sync_state_max, "Invalid array size for g_sync_state_enum_to_str!")

/* See T_sync_type in sync.h */
static const char *g_sync_type_enum_to_str[] = {
  "Sync-E clock",
  "External clock",
  "Sync-E monitoring",
  "Sync-E TX only"
};
COMPILE_TIME_ASSERT((sizeof(g_sync_type_enum_to_str)/sizeof(g_sync_type_enum_to_str[0])) == E_sync_type_max, "Invalid array size for g_sync_type_enum_to_str!")

/* See T_esmc_event_type in esmc_adaptor.h */
static const char *g_esmc_event_type_enum_to_str[] = {
  "Invalid RX QL",
  "QL change",
  "RX timeout",
  "Port link up",
  "Port link down",
  "Immediate timing loop",
  "Originator timing loop"
};
COMPILE_TIME_ASSERT((sizeof(g_esmc_event_type_enum_to_str)/sizeof(g_esmc_event_type_enum_to_str[0])) == E_esmc_event_type_max, "Invalid array size for g_esmc_event_type_enum_to_str!")

static const char *g_api_code_to_str[] = {
  "Get sync info list",
  "Get current status",
  "Get sync info",
  "Set forced QL",
  "Clear forced QL",
  "Clear holdover timer",
  "Clear Sync-E clock wait-to-restore timer",
  "Assign new Sync-E clock port",
  "Set priority",
  "Set max message level"
};
COMPILE_TIME_ASSERT((sizeof(g_api_code_to_str)/sizeof(g_api_code_to_str[0])) == E_mng_api_max, "Invalid array size for g_api_code_to_str!")

/* Global functions */

void generate_clock_id(const unsigned char mac_addr[ETH_ALEN], unsigned char clock_id[MAX_CLK_ID_LEN])
{
  clock_id[0] = mac_addr[0];
  clock_id[1] = mac_addr[1];
  clock_id[2] = mac_addr[2];
  clock_id[3] = 0xFF;
  clock_id[4] = 0xFE;
  clock_id[5] = mac_addr[3];
  clock_id[6] = mac_addr[4];
  clock_id[7] = mac_addr[5];
}

void extract_mac_addr(const unsigned char clock_id[MAX_CLK_ID_LEN], unsigned char mac_addr[ETH_ALEN])
{
  mac_addr[0] = clock_id[0];
  mac_addr[1] = clock_id[1];
  mac_addr[2] = clock_id[2];

  mac_addr[3] = clock_id[5];
  mac_addr[4] = clock_id[6];
  mac_addr[5] = clock_id[7];
}

int conv_net_opt_to_do_not_use_ql(T_esmc_network_option net_opt, T_esmc_ql *do_not_use_ql)
{
  switch(net_opt) {
    case E_esmc_network_option_1:
      *do_not_use_ql = E_esmc_ql_net_opt_1_DNU;
      break;
    case E_esmc_network_option_2:
      *do_not_use_ql = E_esmc_ql_net_opt_2_DUS;
      break;
    case E_esmc_network_option_3:
      *do_not_use_ql = E_esmc_ql_net_opt_3_INV1;
      break;
    default:
      return -1;
  }

  return 0;
}

const char *conv_ql_enum_to_str(T_esmc_ql ql) {
  if(ql >= E_esmc_ql_max) {
    pr_err("Failed to convert QL enumeration to string");
    return "**unknown**";
  }

  return g_ql_enum_to_str[ql];
}

const char *conv_synce_dpll_state_enum_to_str(T_device_dpll_state synce_dpll_state) {
  if(synce_dpll_state >= E_device_dpll_state_max) {
    pr_err("Failed to convert Sync-E DPLL state enumeration to string");
    return "**unknown**";
  }

  return g_device_dpll_state_enum_to_str[synce_dpll_state];
}

const char *conv_sync_clk_state_enum_to_str(T_sync_clk_state sync_clk_state) {
  if(sync_clk_state >= E_sync_clk_state_max) {
    pr_err("Failed to convert sync clock state enumeration to string");
    return "**unknown**";
  }

  return g_sync_clk_state_enum_to_str[sync_clk_state];
}

const char *conv_sync_state_enum_to_str(T_sync_state sync_state) {
  if(sync_state >= E_sync_state_max) {
    pr_err("Failed to convert sync state enumeration to string");
    return "**unknown**";
  }

  return g_sync_state_enum_to_str[sync_state];
}

const char *conv_sync_type_enum_to_str(T_sync_type sync_type) {
  if(sync_type >= E_sync_type_max) {
    pr_err("Failed to convert sync type enumeration to string");
    return "**unknown**";
  }

  return g_sync_type_enum_to_str[sync_type];
}

const char *conv_esmc_event_type_enum_to_str(T_esmc_event_type esmc_event_type)
{
  if(esmc_event_type >= E_esmc_event_type_max) {
    pr_err("Failed to convert ESMC event type enumeration to string");
    return "**unknown**";
  }

  return g_esmc_event_type_enum_to_str[esmc_event_type];
}

const char *conv_api_code_to_str(T_mng_api api_code)
{
  if(api_code >= E_mng_api_max) {
    pr_err("Failed to convert API code enumeration to string");
    return "**unknown**";
  }

  return g_api_code_to_str[api_code];
}

int check_ql_setting(T_esmc_network_option net_opt, T_esmc_ql ql)
{
  int valid_flag = -1;

  if(net_opt >= E_esmc_network_option_max) {
    return valid_flag;
  }

  if(ql >= E_esmc_ql_max) {
    return valid_flag;
  }

  switch(net_opt) {
    case E_esmc_network_option_1:
      if((ql >= ESMC_QL_NET_OPT_1_START) && (ql <= ESMC_QL_NET_OPT_1_END)) {
        valid_flag = 0;
      }
      break;
    case E_esmc_network_option_2:
      if((ql >= ESMC_QL_NET_OPT_2_START) && (ql <= ESMC_QL_NET_OPT_2_END)) {
        valid_flag = 0;
      }
      break;
    case E_esmc_network_option_3:
      if((ql >= ESMC_QL_NET_OPT_3_START) && (ql <= ESMC_QL_NET_OPT_3_END)) {
        valid_flag = 0;
      }
      break;
    default:
      break;
  }

  return valid_flag;
}

void mac_addr_arr_to_str(struct sockaddr_ll *mac_addr, char mac_addr_str[MAX_MAC_ADDR_STR_LEN])
{
  const char *format = "%02X:%02X:%02X:%02X:%02X:%02X";
  snprintf(mac_addr_str,
           MAX_MAC_ADDR_STR_LEN - 1,
           format,
           mac_addr->sll_addr[0], mac_addr->sll_addr[1], mac_addr->sll_addr[2],
           mac_addr->sll_addr[3], mac_addr->sll_addr[4], mac_addr->sll_addr[5]);
}

int calculate_rank(int ql, unsigned char priority, unsigned char hops)
{
  return (((int)ql << 16) | (priority << 8) | hops);
}

void print_sync_info(T_management_sync_info *sync_info)
{
  pr_info_dump("  [%s]\n", sync_info->name);
  pr_info_dump("    Type: %s\n", conv_sync_type_enum_to_str(sync_info->type));
  switch(sync_info->type) {
    case E_sync_type_synce:
      pr_info_dump("    Configured priority: %d\n", sync_info->synce_clk_info.config_pri);
      pr_info_dump("    Current QL: %s\n", conv_ql_enum_to_str(sync_info->synce_clk_info.current_ql));
      pr_info_dump("    State: %s\n", conv_sync_state_enum_to_str(sync_info->synce_clk_info.state));
      if(sync_info->synce_clk_info.tx_bundle_num >= 0) {
        pr_info_dump("    TX bundle number: %d\n", sync_info->synce_clk_info.tx_bundle_num);
      }
      pr_info_dump("    Clock index: %d\n", sync_info->synce_clk_info.clk_idx);
      if((sync_info->synce_clk_info.state == E_sync_state_hold_off) ||
         (sync_info->synce_clk_info.state == E_sync_state_wait_to_restore)) {
        pr_info_dump("    Remaining time: %u ms\n", sync_info->synce_clk_info.remaining_time_ms);
      }
      pr_info_dump("    Number of hops: %d\n", sync_info->synce_clk_info.current_num_hops);
      pr_info_dump("    Rank: 0x%06X\n", sync_info->synce_clk_info.rank);
      pr_info_dump("    Reference monitor status: %s\n", conv_sync_clk_state_enum_to_str(sync_info->synce_clk_info.clk_state));
      if(sync_info->synce_clk_info.clk_state == E_sync_clk_state_unqualified) {
        pr_info_dump("      Frequency offset alarm: %d\n", sync_info->synce_clk_info.ref_mon_status.frequency_offset_alarm_status);
        pr_info_dump("      No activity alarm: %d\n", sync_info->synce_clk_info.ref_mon_status.no_activity_alarm_status);
        pr_info_dump("      Loss of signal alarm: %d\n", sync_info->synce_clk_info.ref_mon_status.loss_of_signal_alarm_status);
      }
      pr_info_dump("    RX timeout: %s\n", sync_info->synce_clk_info.rx_timeout_flag ? "yes" : "no");
      pr_info_dump("    Port link: %s\n", sync_info->synce_clk_info.port_link_down_flag ? "down" : "up");
      break;

    case E_sync_type_monitoring:
      pr_info_dump("    Configured priority: %d\n", sync_info->synce_mon_info.config_pri);
      pr_info_dump("    Current QL: %s\n", conv_ql_enum_to_str(sync_info->synce_mon_info.current_ql));
      pr_info_dump("    State: %s\n", conv_sync_state_enum_to_str(sync_info->synce_mon_info.state));
      if(sync_info->synce_mon_info.tx_bundle_num >= 0) {
        pr_info_dump("    TX bundle number: %d\n", sync_info->synce_mon_info.tx_bundle_num);
      }
      if((sync_info->synce_mon_info.state == E_sync_state_hold_off) ||
          (sync_info->synce_mon_info.state == E_sync_state_wait_to_restore)) {
        pr_info_dump("    Remaining time: %u ms\n", sync_info->synce_mon_info.remaining_time_ms);
      }
      pr_info_dump("    Number of hops: %d\n", sync_info->synce_mon_info.current_num_hops);
      pr_info_dump("    Rank: 0x%06X\n", sync_info->synce_mon_info.rank);
      pr_info_dump("    RX timeout: %s\n", sync_info->synce_mon_info.rx_timeout_flag ? "yes" : "no");
      pr_info_dump("    Port link: %s\n", sync_info->synce_mon_info.port_link_down_flag ? "down" : "up");
      break;

    case E_sync_type_external:
      pr_info_dump("    Configured priority: %d\n", sync_info->ext_clk_info.config_pri);
      pr_info_dump("    Current QL: %s\n", conv_ql_enum_to_str(sync_info->ext_clk_info.current_ql));
      pr_info_dump("    State: %s\n", conv_sync_state_enum_to_str(sync_info->ext_clk_info.state));
      pr_info_dump("    Clock index: %d\n", sync_info->ext_clk_info.clk_idx);
      pr_info_dump("    Rank: 0x%06X\n", sync_info->ext_clk_info.rank);
      pr_info_dump("    Reference monitor status: %s\n", conv_sync_clk_state_enum_to_str(sync_info->ext_clk_info.clk_state));
      if(sync_info->ext_clk_info.clk_state == E_sync_clk_state_unqualified) {
        pr_info_dump("      Frequency offset alarm: %d\n", sync_info->ext_clk_info.ref_mon_status.frequency_offset_alarm_status);
        pr_info_dump("      No activity alarm: %d\n", sync_info->ext_clk_info.ref_mon_status.no_activity_alarm_status);
        pr_info_dump("      Loss of signal alarm: %d\n", sync_info->ext_clk_info.ref_mon_status.loss_of_signal_alarm_status);
      }
      break;

    case E_sync_type_tx_only:
      if(sync_info->synce_tx_only_info.tx_bundle_num >= 0) {
        pr_info_dump("    TX bundle number: %d\n", sync_info->synce_tx_only_info.tx_bundle_num);
      }
      pr_info_dump("    Port link: %s\n", sync_info->synce_tx_only_info.port_link_down_flag ? "down" : "up");
      break;

    default:
      break;
  }
}

void print_current_status(T_management_status *status)
{
  pr_info_dump("  Current QL: %s\n", conv_ql_enum_to_str(status->current_ql));
  pr_info_dump("  Sync-E DPLL state: %s\n", conv_synce_dpll_state_enum_to_str(status->dpll_state));

  if(status->clk_idx < 0) {
    pr_info_dump("  Selected clock: LO\n");
    if(status->dpll_state == E_device_dpll_state_holdover) {
      pr_info_dump("  Holdover remaining time: %u ms\n", status->holdover_remaining_time_ms);
    }
  } else {
    pr_info_dump("  Selected port: %s\n", status->port_name);
    pr_info_dump("  Selected clock index: %d\n", status->clk_idx);
  }
}
