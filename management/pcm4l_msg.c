/**
 * @file pcm4l_msg.c
 * @note Copyright (C) [2021-2023] Renesas Electronics Corporation and/or its affiliates
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
* Release Tag: 2-0-3
* Pipeline ID: 246016
* Commit Hash: 3db24a10
********************************************************************************************************************/

#include "pcm4l_msg.h"
#include "../common/print.h"

/* Global functions */

T_pcm4l_error_code pcm4l_msg_set_clock_category(T_esmc_ql ql, int wait_for_response)
{
  T_pcm4l_message tx_msg;
  T_pcm4l_message rx_msg;

  T_physical_clock_category clock_category;

  T_pcm4l_error_code ret;

  memset(&tx_msg, 0, sizeof(tx_msg));
  tx_msg.api_code = E_pcm4l_api_set_clock_category;

  switch(ql) {
    case E_esmc_ql_net_opt_1_ePRTC:
    case E_esmc_ql_net_opt_1_PRTC:
    case E_esmc_ql_net_opt_2_ePRTC:
    case E_esmc_ql_net_opt_2_PRTC:
      clock_category = E_physical_clock_category_1;
      break;

    case E_esmc_ql_net_opt_1_ePRC:
    case E_esmc_ql_net_opt_1_PRC:
    case E_esmc_ql_net_opt_2_ePRC:
    case E_esmc_ql_net_opt_2_PRS:
      clock_category = E_physical_clock_category_2;
      break;

    case E_esmc_ql_net_opt_1_SSUA:
    case E_esmc_ql_net_opt_1_SSUB:
    case E_esmc_ql_net_opt_2_STU:
    case E_esmc_ql_net_opt_2_ST2:
    case E_esmc_ql_net_opt_2_TNC:
    case E_esmc_ql_net_opt_3_UNK:
      clock_category = E_physical_clock_category_3;
      break;

    case E_esmc_ql_net_opt_1_eSEC:
    case E_esmc_ql_net_opt_1_SEC:
    case E_esmc_ql_net_opt_2_ST3E:
    case E_esmc_ql_net_opt_2_eEEC:
    case E_esmc_ql_net_opt_2_ST3:
    case E_esmc_ql_net_opt_2_SMC:
    case E_esmc_ql_net_opt_2_ST4:
    case E_esmc_ql_net_opt_2_PROV:
    case E_esmc_ql_net_opt_3_SEC:
      clock_category = E_physical_clock_category_4;
      break;

    case E_esmc_ql_net_opt_1_DNU:
    case E_esmc_ql_net_opt_2_DUS:
    case E_esmc_ql_net_opt_3_INV1:
      clock_category = E_physical_clock_category_DNU;
      break;

    default:
      clock_category = E_physical_clock_category_INVALID;
      break;     
  }

  tx_msg.data.clock_category = clock_category;

  if(wait_for_response) {
    memset(&rx_msg, 0, sizeof(rx_msg));
    rx_msg.error_code = E_pcm4l_error_code_fail;
    ret = pcm4l_if_send((unsigned char*)&tx_msg, sizeof(tx_msg), (unsigned char*)&rx_msg, sizeof(rx_msg));
    if((ret == E_pcm4l_error_code_ok) && (rx_msg.error_code != E_pcm4l_error_code_ok)) {
      ret = E_pcm4l_error_code_fail;
    }
  } else {
    ret = pcm4l_if_send((unsigned char*)&tx_msg, sizeof(tx_msg), NULL, 0);
  }

  switch(ret) {
    case E_pcm4l_error_code_ok:
      pr_info("%s: clock category %d - successfully sent to pcm4l", __func__, clock_category);
      break;

    case E_pcm4l_error_code_timeout:
      pr_warning("%s: clock category %d - timeout for acknowledgement from pcm4l", __func__, clock_category);
      break;

    case E_pcm4l_error_code_fail:
      pr_warning("%s: clock category %d - failed to send to pcm4l", __func__, clock_category);
      break;

    case E_pcm4l_error_code_not_sent:
      pr_warning("%s: clock category %d - not sent to pcm4l", __func__, clock_category);
      break;

    default:
      break;
  }

  return ret;
}
