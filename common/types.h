/**
 * @file types.h
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

#ifndef TYPES_H
#define TYPES_H

#define INVALID_PORT_NUM   -1
#define INVALID_SYNC_IDX   -1
#define INVALID_CLK_IDX    -1
#define UNINITIALIZED_FD   -1

/* Applies when port is external clock port or Sync-E clock or Sync-E monitoring port with TX disabled */
#define NO_TX_BUNDLE_NUM   -1

/* Default clock index for Sync-E monitoring ports */
#define MISSING_CLK_IDX   255

/* Applies when port is Sync-E TX only port */
#define NO_PRI   -1

/* See T_esmc_ql */
#define ESMC_QL_NET_OPT_1_START   E_esmc_ql_net_opt_1_ePRTC
#define ESMC_QL_NET_OPT_1_END     E_esmc_ql_net_opt_1_INV14
#define ESMC_QL_NET_OPT_2_START   E_esmc_ql_net_opt_2_ePRTC
#define ESMC_QL_NET_OPT_2_END     E_esmc_ql_net_opt_2_INV11
#define ESMC_QL_NET_OPT_3_START   E_esmc_ql_net_opt_3_UNK
#define ESMC_QL_NET_OPT_3_END     E_esmc_ql_net_opt_3_INV15

typedef int T_port_num;

/* Type (maximum of 10 ESMC PDUs [information and/or event] can be sent in any one-second period) */
typedef enum {
  E_esmc_pdu_type_information = 0, /* Information ESMC PDU is sent when ql change occurs */
  E_esmc_pdu_type_event       = 1  /* Event ESMC PDU is sent every once per second */
} T_esmc_pdu_type;

/* Network option */
typedef enum {
  E_esmc_network_option_1   = 1,
  E_esmc_network_option_2   = 2,
  E_esmc_network_option_3   = 3,
  E_esmc_network_option_max = 4
} T_esmc_network_option;

/* Enhanced (e) SSM codes */
typedef enum {
  E_esmc_e_ssm_code_NONE  = 0x00,
  E_esmc_e_ssm_code_EEC   = 0xFF,
  E_esmc_e_ssm_code_PRTC  = 0x20,
  E_esmc_e_ssm_code_ePRTC = 0x21,
  E_esmc_e_ssm_code_eEEC  = 0x22,
  E_esmc_e_ssm_code_ePRC  = 0x23
} T_esmc_e_ssm_code;

/* Combination of ITU-T G.781 (04/2020) Amd. 1 (11/2022) and ITU-T G.8264 (08/2017) Amd. 1 (03/2018) QLs */
typedef enum {
  /* Network option 1 start */
  E_esmc_ql_net_opt_1_ePRTC,
  E_esmc_ql_net_opt_1_PRTC,
  E_esmc_ql_net_opt_1_ePRC,
  E_esmc_ql_net_opt_1_PRC,
  E_esmc_ql_net_opt_1_SSUA,
  E_esmc_ql_net_opt_1_SSUB,
  E_esmc_ql_net_opt_1_eSEC,  /* Equivalent to ITU-T G.8264 (08/2017) Amd. 1 (03/2018) QL-eEEC */
  E_esmc_ql_net_opt_1_SEC,   /* Equivalent to ITU-T G.8264 (08/2017) Amd. 1 (03/2018) QL-EEC1 */
  E_esmc_ql_net_opt_1_DNU,
  E_esmc_ql_net_opt_1_INV0,
  E_esmc_ql_net_opt_1_INV1,
  E_esmc_ql_net_opt_1_INV3,
  E_esmc_ql_net_opt_1_INV5,
  E_esmc_ql_net_opt_1_INV6,
  E_esmc_ql_net_opt_1_INV7,
  E_esmc_ql_net_opt_1_INV9,
  E_esmc_ql_net_opt_1_INV10,
  E_esmc_ql_net_opt_1_INV12,
  E_esmc_ql_net_opt_1_INV13,
  E_esmc_ql_net_opt_1_INV14,
  /* Network option 1 end */

  /* Network option 2 start */
  E_esmc_ql_net_opt_2_ePRTC,
  E_esmc_ql_net_opt_2_PRTC,
  E_esmc_ql_net_opt_2_ePRC,
  E_esmc_ql_net_opt_2_PRS,
  E_esmc_ql_net_opt_2_STU,
  E_esmc_ql_net_opt_2_ST2,
  E_esmc_ql_net_opt_2_TNC,
  E_esmc_ql_net_opt_2_ST3E,
  E_esmc_ql_net_opt_2_eEEC,
  E_esmc_ql_net_opt_2_ST3,   /* Equivalent to ITU-T G.8264 (08/2017) Amd. 1 (03/2018) QL-EEC2 */
  E_esmc_ql_net_opt_2_SMC,
  E_esmc_ql_net_opt_2_ST4,
  E_esmc_ql_net_opt_2_PROV,
  E_esmc_ql_net_opt_2_DUS,
  E_esmc_ql_net_opt_2_INV2,
  E_esmc_ql_net_opt_2_INV3,
  E_esmc_ql_net_opt_2_INV5,
  E_esmc_ql_net_opt_2_INV6,
  E_esmc_ql_net_opt_2_INV8,
  E_esmc_ql_net_opt_2_INV9,
  E_esmc_ql_net_opt_2_INV11,
  /* Network option 2 end */

  /* Network option 3 start */
  E_esmc_ql_net_opt_3_ePRTC,
  E_esmc_ql_net_opt_3_PRTC,
  E_esmc_ql_net_opt_3_UNK,
  E_esmc_ql_net_opt_3_eSEC,
  E_esmc_ql_net_opt_3_SEC,
  E_esmc_ql_net_opt_3_INV1,
  E_esmc_ql_net_opt_3_INV2,
  E_esmc_ql_net_opt_3_INV3,
  E_esmc_ql_net_opt_3_INV4,
  E_esmc_ql_net_opt_3_INV5,
  E_esmc_ql_net_opt_3_INV6,
  E_esmc_ql_net_opt_3_INV7,
  E_esmc_ql_net_opt_3_INV8,
  E_esmc_ql_net_opt_3_INV9,
  E_esmc_ql_net_opt_3_INV10,
  E_esmc_ql_net_opt_3_INV12,
  E_esmc_ql_net_opt_3_INV13,
  E_esmc_ql_net_opt_3_INV14,
  E_esmc_ql_net_opt_3_INV15,
  /* Network option 3 end */

  E_esmc_ql_NSUPP,
  E_esmc_ql_UNC,
  E_esmc_ql_FAILED,
  E_esmc_ql_max
} T_esmc_ql;

/* Sync type */
typedef enum {
  E_sync_type_synce,
  E_sync_type_external,
  E_sync_type_monitoring,
  E_sync_type_tx_only,
  E_sync_type_max
} T_sync_type;

/* Management APIs */
typedef enum {
  E_mng_api_get_sync_info_list,
  E_mng_api_get_current_status,
  E_mng_api_get_sync_info,
  E_mng_api_set_forced_ql,
  E_mng_api_clear_forced_ql,
  E_mng_api_clear_holdover_timer,
  E_mng_api_clear_synce_clk_wtr_timer,
  E_mng_api_assign_new_synce_clk_port,
  E_mng_api_set_pri,
  E_mng_api_set_max_msg_lvl,
  E_mng_api_max
} T_mng_api;

#endif /* TYPES_H */
