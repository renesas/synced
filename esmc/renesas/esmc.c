/**
 * @file esmc.c
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

#include <errno.h>
#include <linux/if_packet.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "esmc.h"
#include "raw_socket.h"
#include "../../common/missing.h"
#include "../../common/os.h"
#include "../../common/print.h"

#define ESMC_PDU_SLOW_PROTO_SUBTYPE   0xA
#define ESMC_PDU_ITU_OUI              {0x00, 0x19, 0xA7}
#define ESMC_PDU_ITU_SUBTYPE          {0x00, 0x01}
#define ESMC_PDU_VER                  1

#define ESMC_PDU_EVENT_FLAG_LSB   3
#define ESMC_PDU_VER_LSB          4

#define ESMC_PDU_GET_EVENT_FLAG(val)   (val >> ESMC_PDU_EVENT_FLAG_LSB)
#define ESMC_PDU_GET_VER(val)          (val >> ESMC_PDU_VER_LSB)

#define ESMC_PDU_QL_TLV_TYPE     0x1
#define ESMC_PDU_QL_TLV_LENGTH   {0x00, 0x04}

#define ESMC_PDU_EXT_QL_TLV_TYPE     0x2
#define ESMC_PDU_EXT_QL_TLV_LENGTH   {0x00, 0x14}

#define ESMC_PDU_EXT_QL_TLV_FLAG_GET_MIXED_FLAG(val)           ((val >> ESMC_PDU_EXT_QL_TLV_MIXED_EEC_EEEC_FLAG_LSB) & 1)
#define ESMC_PDU_EXT_QL_TLV_FLAG_GET_PARTIAL_CHAIN_FLAG(val)   ((val >> ESMC_PDU_EXT_QL_TLV_PARTIAL_CHAIN_FLAG_LSB) & 1)

struct T_port_tx_data {
  LIST_ENTRY(T_port_tx_data) list;
};

struct T_port_rx_data {
  LIST_ENTRY(T_port_rx_data) list;
};

/* Static data */

static T_esmc g_esmc;

static const unsigned char g_dst_addr[ETH_ALEN] = ESMC_PDU_IEEE_SLOW_PROTO_MCAST_ADDR;
static const unsigned char g_itu_oui[ESMC_PDU_ITU_OUI_LEN] = ESMC_PDU_ITU_OUI;
static const unsigned char g_itu_subtype[ESMC_PDU_ITU_SUBTYPE_LEN] = ESMC_PDU_ITU_SUBTYPE;
static const unsigned char g_ql_tlv_length[ESMC_PDU_QL_TLV_LENGTH_LEN] = ESMC_PDU_QL_TLV_LENGTH;
static const unsigned char g_ext_ql_tlv_length[ESMC_PDU_EXT_QL_TLV_LENGTH_LEN] = ESMC_PDU_EXT_QL_TLV_LENGTH;

static T_esmc_tx_event_cb g_tx_cb = NULL;
static T_esmc_rx_event_cb g_rx_cb = NULL;

/* Static functions */

static int esmc_add_tx_port(T_port_tx_data *tx_p)
{
  T_port_tx_data *tx_p_iter = NULL;
  T_port_tx_data *last_tx_p = NULL;
  T_esmc *esmc = &g_esmc;

  LIST_FOREACH(tx_p_iter, &esmc->tx_ports, list) {
    last_tx_p = tx_p_iter;
  }

  if(last_tx_p) {
    LIST_INSERT_AFTER(last_tx_p, tx_p, list);
  }
  else {
    LIST_INSERT_HEAD(&esmc->tx_ports, tx_p, list);
  }
  esmc->num_tx_ports++;

  return 0;
}

static int esmc_add_rx_port(T_port_rx_data *rx_p)
{
  T_port_rx_data *rx_p_iter = NULL;
  T_port_rx_data *last_rx_p = NULL;
  T_esmc *esmc = &g_esmc;

  LIST_FOREACH(rx_p_iter, &esmc->rx_ports, list) {
    last_rx_p = rx_p_iter;
  }

  if(last_rx_p) {
    LIST_INSERT_AFTER(last_rx_p, rx_p, list);
  }
  else {
    LIST_INSERT_HEAD(&esmc->rx_ports, rx_p, list);
  }
  esmc->num_rx_ports++;

  return 0;
}

/* Send functions */

static int esmc_compose_eth_hdr(T_esmc_pdu *msg, unsigned char src_mac_addr[ETH_ALEN])
{
  memcpy(msg->eth_hdr.h_dest, g_dst_addr, ETH_ALEN);
  memcpy(msg->eth_hdr.h_source, src_mac_addr, ETH_ALEN);
  msg->eth_hdr.h_proto = htons(ETH_P_SLOW);

  return ESMC_PDU_ETH_HDR_LEN;
}

static int esmc_ql_to_ssm_and_e_ssm_map(T_esmc_network_option net_opt, T_esmc_ql ql, unsigned char *ssm_code, unsigned char *e_ssm_code)
{
  switch(net_opt) {
    case E_esmc_network_option_1:
      switch(ql) {
        case E_esmc_ql_net_opt_1_ePRTC:
          *ssm_code = 0x02;
          *e_ssm_code = 0x21;
          break;
        case E_esmc_ql_net_opt_1_PRTC:
          *ssm_code = 0x02;
          *e_ssm_code = 0x20;
          break;
        case E_esmc_ql_net_opt_1_ePRC:
          *ssm_code = 0x02;
          *e_ssm_code = 0x23;
          break;
        case E_esmc_ql_net_opt_1_PRC:
          *ssm_code = 0x02;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_SSUA:
          *ssm_code = 0x04;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_SSUB:
          *ssm_code = 0x08;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_eSEC:
          *ssm_code = 0x0B;
          *e_ssm_code = 0x22;
          break;
        case E_esmc_ql_net_opt_1_SEC:
          *ssm_code = 0x0B;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_DNU:
          *ssm_code = 0x0F;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV0:
          *ssm_code = 0x00;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV1:
          *ssm_code = 0x01;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV3:
          *ssm_code = 0x03;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV5:
          *ssm_code = 0x05;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV6:
          *ssm_code = 0x06;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV7:
          *ssm_code = 0x07;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV9:
          *ssm_code = 0x09;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV10:
          *ssm_code = 0x0A;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV12:
          *ssm_code = 0x0C;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV13:
          *ssm_code = 0x0D;
          *e_ssm_code = 0xFF;
          break;
        case E_esmc_ql_net_opt_1_INV14:
          *ssm_code = 0x0E;
          *e_ssm_code = 0xFF;
          break;
        default:
          return -1;
      }
      break;

    case E_esmc_network_option_2:
      switch(ql) {
      case E_esmc_ql_net_opt_2_ePRTC:
        *ssm_code = 0x01;
        *e_ssm_code = 0x21;
        break;
      case E_esmc_ql_net_opt_2_PRTC:
        *ssm_code = 0x01;
        *e_ssm_code = 0x20;
        break;
      case E_esmc_ql_net_opt_2_ePRC:
        *ssm_code = 0x01;
        *e_ssm_code = 0x23;
        break;
      case E_esmc_ql_net_opt_2_PRS:
        *ssm_code = 0x01;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_STU:
        *ssm_code = 0x00;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_ST2:
        *ssm_code = 0x07;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_TNC:
        *ssm_code = 0x4;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_ST3E:
        *ssm_code = 0x0D;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_eEEC:
        *ssm_code = 0x0A;
        *e_ssm_code = 0x22;
        break;
      case E_esmc_ql_net_opt_2_ST3:
        *ssm_code = 0x0A;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_SMC:
        *ssm_code = 0x0C;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_PROV:
        *ssm_code = 0x0E;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_DUS:
        *ssm_code = 0x0F;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_INV2:
        *ssm_code = 0x02;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_INV3:
        *ssm_code = 0x03;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_INV5:
        *ssm_code = 0x05;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_INV6:
        *ssm_code = 0x06;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_INV8:
        *ssm_code = 0x08;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_INV9:
        *ssm_code = 0x09;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_2_INV11:
        *ssm_code = 0x0B;
        *e_ssm_code = 0xFF;
        break;
      default:
        return -1;
      }
      break;
  
    case E_esmc_network_option_3:
      switch(ql) {
      case E_esmc_ql_net_opt_3_UNK:
        *ssm_code = 0x00;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_SEC:
        *ssm_code = 0x0B;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV1:
        *ssm_code = 0x01;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV2:
        *ssm_code = 0x02;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV3:
        *ssm_code = 0x03;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV4:
        *ssm_code = 0x04;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV5:
        *ssm_code = 0x05;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV6:
        *ssm_code = 0x06;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV7:
        *ssm_code = 0x07;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV8:
        *ssm_code = 0x08;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV9:
        *ssm_code = 0x09;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV10:
        *ssm_code = 0x0A;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV12:
        *ssm_code = 0x0C;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV13:
        *ssm_code = 0x0D;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV14:
        *ssm_code = 0x0E;
        *e_ssm_code = 0xFF;
        break;
      case E_esmc_ql_net_opt_3_INV15:
        *ssm_code = 0x0F;
        *e_ssm_code = 0xFF;
        break;
      default:
        return -1;
      }
      break;

    default:
      return -1;
  }

  return 0;
}

static int esmc_compose_ql_tlv(T_esmc_pdu *msg, unsigned char ssm_code)
{
  msg->ql_tlv.type = ESMC_PDU_QL_TLV_TYPE;
  memcpy(msg->ql_tlv.length, g_ql_tlv_length, ESMC_PDU_QL_TLV_LENGTH_LEN);
  msg->ql_tlv.ssm_code = ssm_code;

  return ESMC_PDU_QL_TLV_LEN;
}

static int esmc_compose_extended_ql_tlv(T_esmc_pdu *msg, unsigned char e_ssm_code, T_port_ext_ql_tlv_data const *ext_ql_tlv_data)
{
  msg->ext_ql_tlv.type = ESMC_PDU_EXT_QL_TLV_TYPE;
  memcpy(msg->ext_ql_tlv.length, g_ext_ql_tlv_length, ESMC_PDU_EXT_QL_TLV_LENGTH_LEN);
  msg->ext_ql_tlv.esmc_e_ssm_code = e_ssm_code;

  memcpy(msg->ext_ql_tlv.syncE_clock_id, ext_ql_tlv_data->originator_clock_id, ESMC_PDU_EXT_QL_TLV_SYNCE_CLOCK_ID_LEN);

  msg->ext_ql_tlv.flag = ((ext_ql_tlv_data->partial_chain << ESMC_PDU_EXT_QL_TLV_PARTIAL_CHAIN_FLAG_LSB) | (ext_ql_tlv_data->mixed_EEC_eEEC << ESMC_PDU_EXT_QL_TLV_MIXED_EEC_EEEC_FLAG_LSB));

  msg->ext_ql_tlv.num_cascaded_eEEC = ext_ql_tlv_data->num_cascaded_eEEC;
  msg->ext_ql_tlv.num_cascaded_EEC = ext_ql_tlv_data->num_cascaded_EEC;

  memset(msg->ext_ql_tlv.reserved, 0, ESMC_PDU_EXT_QL_TLV_RSVD_LEN);

  return ESMC_PDU_EXT_QL_TLV_LEN;
}

/* Receive functions */

static int esmc_ssm_and_essm_to_ql_map(T_esmc_network_option net_opt, unsigned char ssm_code, unsigned char e_ssm_code, T_esmc_ql *ql)
{
  switch(net_opt) {
    case E_esmc_network_option_1:
      switch(ssm_code) {
        case 0x00:
          *ql = E_esmc_ql_net_opt_1_INV0;
          break;
        case 0x01:
          *ql = E_esmc_ql_net_opt_1_INV1;
          break;
        case 0x02:
          switch(e_ssm_code) {
            case E_esmc_e_ssm_code_NONE:
            case E_esmc_e_ssm_code_EEC:
              *ql = E_esmc_ql_net_opt_1_PRC;
              break;
            case E_esmc_e_ssm_code_PRTC:
              *ql = E_esmc_ql_net_opt_1_PRTC;
              break;
            case E_esmc_e_ssm_code_ePRTC:
              *ql = E_esmc_ql_net_opt_1_ePRTC;
              break;
            case E_esmc_e_ssm_code_eEEC:
              *ql = E_esmc_ql_net_opt_1_eSEC;
              break;
            case E_esmc_e_ssm_code_ePRC:
              *ql = E_esmc_ql_net_opt_1_ePRC;
              break;
            default:
              return -1;
          }
          break;
        case 0x03:
          *ql = E_esmc_ql_net_opt_1_INV3;
          break;
        case 0x04:
          *ql = E_esmc_ql_net_opt_1_SSUA;
          break;
        case 0x05:
          *ql = E_esmc_ql_net_opt_1_INV5;
          break;
        case 0x06:
          *ql = E_esmc_ql_net_opt_1_INV6;
          break;
        case 0x07:
          *ql = E_esmc_ql_net_opt_1_INV7;
          break;
        case 0x08:
          *ql = E_esmc_ql_net_opt_1_SSUB;
          break;
        case 0x09:
          *ql = E_esmc_ql_net_opt_1_INV9;
          break;
        case 0x0A:
          *ql = E_esmc_ql_net_opt_1_INV10;
          break;
        case 0x0B:
          switch(e_ssm_code) {
            case E_esmc_e_ssm_code_NONE:
            case E_esmc_e_ssm_code_EEC:
              *ql = E_esmc_ql_net_opt_1_SEC;
              break;
            case E_esmc_e_ssm_code_eEEC:
              *ql = E_esmc_ql_net_opt_1_eSEC;
              break;
            default:
              return -1;
          }
          break;
        case 0x0C:
          *ql = E_esmc_ql_net_opt_1_INV12;
          break;
        case 0x0D:
          *ql = E_esmc_ql_net_opt_1_INV13;
          break;
        case 0x0E:
          *ql = E_esmc_ql_net_opt_1_INV14;
          break;
        case 0x0F:
          *ql = E_esmc_ql_net_opt_1_DNU;
          break;
        default:
          return -1;
      }
      break;
  
    case E_esmc_network_option_2:
      switch(ssm_code) {
        case 0x00:
          *ql = E_esmc_ql_net_opt_2_STU;
          break;
        case 0x01:
          switch(e_ssm_code) {
            case E_esmc_e_ssm_code_NONE:
            case E_esmc_e_ssm_code_EEC:
              *ql = E_esmc_ql_net_opt_2_PRS;
              break;
            case E_esmc_e_ssm_code_PRTC:
              *ql = E_esmc_ql_net_opt_1_PRTC;
              break;
            case E_esmc_e_ssm_code_ePRTC:
              *ql = E_esmc_ql_net_opt_1_ePRTC;
              break;
            case E_esmc_e_ssm_code_ePRC:
              *ql = E_esmc_ql_net_opt_2_ePRC;
              break;
            default:
              return -1;
          }
          break;
        case 0x02:
          *ql = E_esmc_ql_net_opt_2_INV2;
          break;
        case 0x03:
          *ql = E_esmc_ql_net_opt_2_INV3;
          break;
        case 0x04:
          *ql = E_esmc_ql_net_opt_2_TNC;
          break;
        case 0x05:
          *ql = E_esmc_ql_net_opt_2_INV5;
          break;
        case 0x06:
          *ql = E_esmc_ql_net_opt_2_INV6;
          break;
        case 0x07:
          *ql = E_esmc_ql_net_opt_2_ST2;
          break;
        case 0x08:
          *ql = E_esmc_ql_net_opt_2_INV8;
          break;
        case 0x09:
          *ql = E_esmc_ql_net_opt_2_INV9;
          break;
        case 0x0A:
          switch(e_ssm_code) {
            case E_esmc_e_ssm_code_NONE:
            case E_esmc_e_ssm_code_EEC:
              *ql = E_esmc_ql_net_opt_2_ST3;
              break;
            case E_esmc_e_ssm_code_eEEC:
              *ql = E_esmc_ql_net_opt_2_eEEC;
              break;
            default:
              return -1;
          }
          break;
        case 0x0B:
          *ql = E_esmc_ql_net_opt_2_INV11;
          break;
        case 0x0C:
          *ql = E_esmc_ql_net_opt_2_SMC;
          break;
        case 0x0D:
          *ql = E_esmc_ql_net_opt_2_ST3E;
          break;
        case 0x0E:
          *ql = E_esmc_ql_net_opt_2_PROV;
          break;
        case 0x0F:
          *ql = E_esmc_ql_net_opt_2_DUS;
          break;
        default:
          return -1;
      }
      break;
  
    case E_esmc_network_option_3:
      switch(ssm_code) {
        case 0x00:
          *ql = E_esmc_ql_net_opt_3_UNK;
          break;
        case 0x01:
          *ql = E_esmc_ql_net_opt_3_INV1;
          break;
        case 0x02:
          *ql = E_esmc_ql_net_opt_3_INV2;
          break;
        case 0x03:
          *ql = E_esmc_ql_net_opt_3_INV3;
          break;
        case 0x04:
          *ql = E_esmc_ql_net_opt_3_INV4;
          break;
        case 0x05:
          *ql = E_esmc_ql_net_opt_3_INV5;
          break;
        case 0x06:
          *ql = E_esmc_ql_net_opt_3_INV6;
          break;
        case 0x07:
          *ql = E_esmc_ql_net_opt_3_INV7;
          break;
        case 0x08:
          *ql = E_esmc_ql_net_opt_3_INV8;
          break;
        case 0x09:
          *ql = E_esmc_ql_net_opt_3_INV9;
          break;
        case 0x0A:
          *ql = E_esmc_ql_net_opt_3_INV10;
          break;
        case 0x0B:
          *ql = E_esmc_ql_net_opt_3_SEC;
          break;
        case 0x0C:
          *ql = E_esmc_ql_net_opt_3_INV12;
          break;
        case 0x0D:
          *ql = E_esmc_ql_net_opt_3_INV13;
          break;
        case 0x0E:
          *ql = E_esmc_ql_net_opt_3_INV14;
          break;
        case 0x0F:
          *ql = E_esmc_ql_net_opt_3_INV15;
          break;
        default:
          return -1;
      }
      break;
  
    default:
      return -1;
  }

  return 0;
}

static inline int esmc_check_slow_proto_subtype(unsigned char *subtype)
{
  return (*subtype == ESMC_PDU_SLOW_PROTO_SUBTYPE) ? 0 : -1;
}

static inline int esmc_check_itu_oui(unsigned char itu_oui[ESMC_PDU_ITU_OUI_LEN])
{
  return ((itu_oui[0] == g_itu_oui[0]) && (itu_oui[1] == g_itu_oui[1]) && (itu_oui[2] == g_itu_oui[2])) ? 0 : -1;
}

static inline int esmc_check_itu_subtype(unsigned char itu_subtype[ESMC_PDU_ITU_SUBTYPE_LEN])
{
  return ((itu_subtype[0] == g_itu_subtype[0]) && (itu_subtype[1] == g_itu_subtype[1])) ? 0 : -1;
}

static inline int esmc_check_event_version(unsigned char *version_event_flag_reserved)
{
  return (ESMC_PDU_GET_VER(*version_event_flag_reserved) == ESMC_PDU_VER) ? 0 : -1;
}

static inline int esmc_check_ql_tlv(T_esmc_ql_tlv *ql_tlv) {
  if(ql_tlv->type != ESMC_PDU_QL_TLV_TYPE_LEN)
    return -1;

  if(memcmp(ql_tlv->length, g_ql_tlv_length, ESMC_PDU_QL_TLV_LENGTH_LEN))
    return -1;

  /* SSM code can be zero (e.g. QL-STU has SSM code equal to 0x0 and eSSM code equal to 0xFF) */

  return 0;
}

static inline int esmc_check_ext_ql_tlv(T_esmc_ext_ql_tlv *ext_ql_tlv, int *enhanced) {

  /* Received ESMC PDU does not originate from enhanced node (i.e. eSSM code is equal to 0x0), so no need for additional parsing */
  if(ext_ql_tlv->esmc_e_ssm_code == E_esmc_e_ssm_code_NONE) {
    *enhanced = 0;
    return 0;
  }

  /* Received ESMC PDU has non-zero enhanced SSM code, but need to check if other fields are valid */

  if(ext_ql_tlv->type != ESMC_PDU_EXT_QL_TLV_TYPE) {
    *enhanced = 1;
    return -1;
  }
  if(memcmp(ext_ql_tlv->length, g_ext_ql_tlv_length, ESMC_PDU_EXT_QL_TLV_LENGTH_LEN)) {
    *enhanced = 1;
    return -1;
  }

  *enhanced = 1;
  return 0;
}

static void esmc_parse_ext_ql_tlv_data(T_esmc_ext_ql_tlv *ext_ql_tlv, T_port_ext_ql_tlv_data *parsed_ext_ql_tlv_data)
{
  parsed_ext_ql_tlv_data->mixed_EEC_eEEC = ESMC_PDU_EXT_QL_TLV_FLAG_GET_MIXED_FLAG(ext_ql_tlv->flag);
  parsed_ext_ql_tlv_data->partial_chain = ESMC_PDU_EXT_QL_TLV_FLAG_GET_PARTIAL_CHAIN_FLAG(ext_ql_tlv->flag);
  memcpy(parsed_ext_ql_tlv_data->originator_clock_id, ext_ql_tlv->syncE_clock_id, ESMC_PDU_EXT_QL_TLV_SYNCE_CLOCK_ID_LEN);
  parsed_ext_ql_tlv_data->num_cascaded_eEEC = ext_ql_tlv->num_cascaded_eEEC;
  parsed_ext_ql_tlv_data->num_cascaded_EEC = ext_ql_tlv->num_cascaded_EEC;
}

/* Global functions */

int esmc_create_stack(void)
{
  T_esmc *esmc = &g_esmc;
  memset(esmc, 0, sizeof(*esmc));

  esmc->state = E_esmc_state_created_stack;

  return 0;
}

int esmc_create_tx_ports(T_tx_port_info const *tx_port, int num_tx_ports)
{
  T_esmc *esmc = &g_esmc;
  int i;
  const char *name;
  T_port_num port_num;
  T_port_tx_data *tx_p;

  if(esmc->state < E_esmc_state_intitialized_stack) {
    return -1;
  }

  LIST_INIT(&esmc->tx_ports);

  for(i = 0; i < num_tx_ports; i++) {
    name = tx_port->name;
    port_num = tx_port->port_num;

    tx_p = port_create_tx(tx_port);
    if(!tx_p) {
      pr_err("Failed to create port %s (port number: %d) TX", name, port_num);
      return -1;
    }

    if(esmc_add_tx_port(tx_p)) {
      pr_debug("Failed to add port %s (port number: %d) TX to ESMC stack", name, port_num);
      return -1;
    }

    pr_info("Created port %s (port number: %d) TX", name, port_num);

    tx_port++;
  }

  if(!esmc->num_tx_ports) {
    pr_warning("No TX ports created");
  } else {
    pr_info("Created %d TX ports", esmc->num_tx_ports);
  }

  esmc->state = E_esmc_state_created_tx_ports;

  return 0;
}

int esmc_create_rx_ports(T_rx_port_info const *rx_port, int num_rx_ports)
{
  T_esmc *esmc = &g_esmc;
  int i;
  const char *name;
  T_port_num port_num;
  T_port_rx_data *rx_p;

  if(esmc->state < E_esmc_state_created_tx_ports) {
    return -1;
  }

  LIST_INIT(&esmc->rx_ports);

  for(i = 0; i < num_rx_ports; i++) {
    name = rx_port->name;
    port_num = rx_port->port_num;

    rx_p = port_create_rx(rx_port);
    if(!rx_p) {
      pr_err("Failed to create port %s (port number: %d) RX", name, port_num);
      return -1;
    }

    if(esmc_add_rx_port(rx_p)) {
      pr_debug("Failed to add port %s (port number: %d) RX to ESMC stack", name, port_num);
      return -1;
    }

    pr_info("Created port %s (port number: %d) RX", name, port_num);
    rx_port++;
  }

  if(!esmc->num_rx_ports) {
    pr_warning("No RX ports created");
  } else {
    pr_info("Created %d RX ports", esmc->num_rx_ports);
  }

  esmc->state = E_esmc_state_created_rx_ports;

  return 0;
}

int esmc_init_stack(T_esmc_network_option net_opt, T_esmc_ql init_ql, T_esmc_ql do_not_use_ql)
{
  T_esmc *esmc = &g_esmc;

  if(esmc->state != E_esmc_state_created_stack) {
    goto err;
  }

  esmc->net_opt = net_opt;
  esmc->init_ql = init_ql;
  esmc->do_not_use_ql = do_not_use_ql;

  esmc->best_ql = init_ql;
  esmc->best_ext_ql_tlv_data.num_cascaded_eEEC = 1;
  esmc->best_ext_ql_tlv_data.num_cascaded_EEC = 0;
  esmc->best_port_num = INVALID_PORT_NUM;
  esmc->port_tx_bundle_info.num_ports = 0;

  if(os_mutex_init(&esmc->best_ql_mutex) < 0) {
    goto err;
  }
  if(os_cond_init(&esmc->best_ql_cond) < 0) {
    os_mutex_deinit(&esmc->best_ql_mutex);
    goto err;
  }

  esmc->state = E_esmc_state_intitialized_stack;

  return 0;

err:
  esmc->state = E_esmc_state_failed;
  return -1;
}

int esmc_init_tx_ports(void)
{
  T_esmc *esmc = &g_esmc;
  T_port_tx_data *tx_p;
  int count = 0;

  if(esmc->state < E_esmc_state_created_tx_ports) {
    goto err;
  }

  if(esmc->num_tx_ports == 0) {
    /* No TX ports to initialize */
    esmc->state = E_esmc_state_initialized_tx_ports;
    return 0;
  }

  LIST_FOREACH(tx_p, &esmc->tx_ports, list) {
    if(port_init_tx(tx_p) < 0) {
      pr_err("Failed to initialize TX port");
      port_destroy_tx(tx_p);
      LIST_REMOVE(tx_p, list);
      free(tx_p);
      goto err;
    }

    count++;
  }

  if(count == 0) {
    pr_err("No TX ports initialized");
    goto err;
  } else if(count != esmc->num_tx_ports) {
    pr_warning("Initialized only %d out of %d previously created TX ports", count, esmc->num_tx_ports);
    esmc->num_tx_ports = count;
  }

  esmc->state = E_esmc_state_initialized_tx_ports;
  return 0;

err:
  esmc->state = E_esmc_state_failed;
  return -1;
}

int esmc_init_rx_ports(void)
{
  T_esmc *esmc = &g_esmc;
  T_port_rx_data *rx_p;
  int count = 0;

  if(esmc->state < E_esmc_state_created_rx_ports) {
    goto err;
  }

  if(esmc->num_rx_ports == 0) {
    /* No RX ports to initialize */
    esmc->state = E_esmc_state_initialized_rx_ports;
    return 0;
  }

  LIST_FOREACH(rx_p, &esmc->rx_ports, list) {
    if(port_init_rx(rx_p) < 0) {
      pr_err("Failed to initialize RX port");
      port_destroy_rx(rx_p);
      LIST_REMOVE(rx_p, list);
      free(rx_p);
      goto err;
    }

    count++;
  }

  if(count == 0) {
    pr_err("No RX ports initialized");
    goto err;
  } else if(count != esmc->num_rx_ports) {
    pr_warning("Initialized only %d out of %d previously created RX ports", count, esmc->num_rx_ports);
    esmc->num_tx_ports = count;
  }

  esmc->state = E_esmc_state_initialized_rx_ports;
  return 0;

err:
  esmc->state = E_esmc_state_failed;
  return -1;
}

int esmc_check_init(void)
{
  T_esmc *esmc = &g_esmc;
  T_port_tx_data *tx_p;
  T_port_rx_data *rx_p;
  const int interval_us = 100000;
  const int num_tries = 20;
  int i;
  int tx_count;
  int rx_count;

  if(esmc->state < E_esmc_state_initialized_rx_ports) {
    goto err;
  }

  /* Give threads time to start */
  for (i = 0; i < num_tries; i++) {
    tx_count = 0;
    LIST_FOREACH(tx_p, &esmc->tx_ports, list) {
      if(port_check_tx(tx_p) == 0) {
        tx_count++;
      }
    } 

    rx_count = 0;
    LIST_FOREACH(rx_p, &esmc->rx_ports, list) {
      if(port_check_rx(rx_p) == 0) {
        rx_count++;
      }
    }

    if((tx_count == esmc->num_tx_ports) && (rx_count == esmc->num_rx_ports)) {
      esmc->state = E_esmc_state_running;
      return 0;
    }

    usleep(interval_us);
  }

err:
  esmc->state = E_esmc_state_failed;
  return -1;
}

void esmc_destroy_stack(void)
{
  T_esmc *esmc = &g_esmc;

  os_mutex_deinit(&esmc->best_ql_mutex);
  os_cond_deinit(&esmc->best_ql_cond);

  esmc->state = E_esmc_state_unknown;
}

void esmc_destroy_tx_ports(void)
{
  T_esmc *esmc = &g_esmc;
  T_port_tx_data *tx_p;
  T_port_tx_data *tmp_tx_p;

  LIST_FOREACH_SAFE(tx_p, &esmc->tx_ports, list, tmp_tx_p) {
    port_destroy_tx(tx_p);
    LIST_REMOVE(tx_p, list);
    free(tx_p);
  }

  esmc->num_tx_ports = 0;
}

void esmc_destroy_rx_ports(void)
{
  T_esmc *esmc = &g_esmc;
  T_port_rx_data *rx_p;
  T_port_rx_data *tmp_rx_p;

  LIST_FOREACH_SAFE(rx_p, &esmc->rx_ports, list, tmp_rx_p) {
    port_destroy_rx(rx_p);
    LIST_REMOVE(rx_p, list);
    free(rx_p);
  }

  esmc->num_rx_ports = 0;
}

void esmc_set_best_ql(T_esmc_ql best_ql, T_port_num best_port_num, T_port_tx_bundle_info *port_tx_bundle_info)
{
  T_esmc *esmc = &g_esmc;

  T_port_rx_data *rx_p;
  T_port_rx_data *tmp_rx_p;

  os_mutex_lock(&esmc->best_ql_mutex);
  esmc->best_ql = best_ql;
  esmc->best_port_num = best_port_num;
  memcpy(&esmc->port_tx_bundle_info, port_tx_bundle_info, sizeof(*port_tx_bundle_info));

  if(best_port_num == INVALID_PORT_NUM) {
    /* Best clock is external clock or LO */
    esmc->best_ext_ql_tlv_data.num_cascaded_eEEC = 1;
    esmc->best_ext_ql_tlv_data.num_cascaded_EEC = 0;
    esmc->best_ext_ql_tlv_data.mixed_EEC_eEEC = 0;
    esmc->best_ext_ql_tlv_data.partial_chain = 0;
    memset(esmc->best_ext_ql_tlv_data.originator_clock_id, 0, ESMC_PDU_EXT_QL_TLV_SYNCE_CLOCK_ID_LEN);
  } else {
    /* Best clock is Sync-E clock */
    LIST_FOREACH_SAFE(rx_p, &esmc->rx_ports, list, tmp_rx_p) {
      if(port_get_rx_ext_ql_tlv_data(rx_p, best_port_num, &esmc->best_ext_ql_tlv_data) == 1) {
        break;
      }
    }
  }

  /* Unblock all threads waiting on condition */
  os_cond_broadcast(&esmc->best_ql_cond);
  os_mutex_unlock(&esmc->best_ql_mutex);
}

int esmc_reg_tx_cb(T_esmc_tx_event_cb event_cb)
{
  g_tx_cb = event_cb;

  return 0;
}

int esmc_reg_rx_cb(T_esmc_rx_event_cb event_cb)
{
  g_rx_cb = event_cb;

  return 0;
}

int esmc_call_tx_cb(T_esmc_tx_event_cb_data *cb_data)
{
  if(g_tx_cb != NULL) {
    g_tx_cb(cb_data);
  }

  return 0;
}

int esmc_call_rx_cb(T_esmc_rx_event_cb_data *cb_data)
{
  if(g_rx_cb != NULL) {
    g_rx_cb(cb_data);
  }

  return 0;
}

int esmc_compose_pdu(T_esmc_pdu *msg, T_esmc_pdu_type msg_type, unsigned char src_mac_addr[ETH_ALEN], T_port_ext_ql_tlv_data const *best_ext_ql_tlv_data, T_port_num port_num, T_esmc_ql *composed_ql)
{
  T_esmc *esmc = &g_esmc;
  T_esmc_network_option net_opt = esmc->net_opt;
  T_port_num best_port_num = esmc->best_port_num;
  T_esmc_ql do_not_use_ql = esmc->do_not_use_ql;

  T_esmc_ql best_ql;
  T_port_ext_ql_tlv_data ext_ql_tlv_data;

  unsigned char flag;
  unsigned char ssm_code;
  unsigned char e_ssm_code;
  int off;

  int i;

  os_mutex_lock(&esmc->best_ql_mutex);
  best_ql = esmc->best_ql;
  for(i = 0; i < esmc->port_tx_bundle_info.num_ports; i++) {
    if(port_num == esmc->port_tx_bundle_info.port_nums[i]) {
      best_ql = do_not_use_ql;
      break;
    }
  }
  os_mutex_unlock(&esmc->best_ql_mutex);

  *composed_ql = best_ql;

  memcpy(&ext_ql_tlv_data, best_ext_ql_tlv_data, sizeof(ext_ql_tlv_data));

  if((best_port_num == INVALID_PORT_NUM) || (CHECK_CLOCK_ID_NULL(ext_ql_tlv_data.originator_clock_id))) {
    /* Best clock is external clock or LO or Sync-E clock without originator clock ID, so overwrite originator clock ID */
    generate_clock_id(src_mac_addr, ext_ql_tlv_data.originator_clock_id);
  }

  off = esmc_compose_eth_hdr(msg, src_mac_addr);

  msg->slow_proto_subtype = ESMC_PDU_SLOW_PROTO_SUBTYPE;
  off += ESMC_PDU_SLOW_PROTO_SUBTYPE_LEN;

  memcpy(msg->itu_oui, g_itu_oui, sizeof(g_itu_oui));
  off += ESMC_PDU_ITU_OUI_LEN;

  memcpy(msg->itu_subtype, g_itu_subtype, sizeof(g_itu_subtype));
  off += ESMC_PDU_ITU_SUBTYPE_LEN;

  flag = (msg_type == E_esmc_pdu_type_event) ? 1 : 0;
  msg->version_event_flag_reserved = (flag << ESMC_PDU_EVENT_FLAG_LSB) | (ESMC_PDU_VER << ESMC_PDU_VER_LSB);
  off += ESMC_PDU_VER_EVENT_LEN;

  /* Reserved field already set to zero */
  off += ESMC_PDU_RSVD_LEN;

  if(esmc_ql_to_ssm_and_e_ssm_map(net_opt, best_ql, &ssm_code, &e_ssm_code) < 0)
    return -1;

  if(esmc_compose_ql_tlv(msg, ssm_code) < 0) {
    pr_debug("Failed to compose extended QL TLV");
    return -1;
  }
  off += ESMC_PDU_QL_TLV_LEN;

  if(esmc_compose_extended_ql_tlv(msg, e_ssm_code, &ext_ql_tlv_data) < 0) {
    pr_debug("Failed to compose extended QL TLV");
    return -1;
  }
  off += ESMC_PDU_EXT_QL_TLV_LEN;

  /* Padding already set to zero */
  off += ESMC_PDU_PADDING_LEN;

  if(off > ESMC_PDU_LEN) {
    pr_err("Failed to construct ESMC PDU (message length %d exceeds maximum message length %d)", off, ESMC_PDU_LEN);
    return -1;
  }

  return off;
}

int esmc_parse_pdu(T_esmc_pdu *msg, int *enhanced_flag, T_esmc_ql *parsed_ql, T_port_ext_ql_tlv_data *parsed_ext_ql_tlv_data)
{
  T_esmc *esmc = &g_esmc;

  if(esmc_check_slow_proto_subtype(&msg->slow_proto_subtype) < 0) {
    return -1;
  }
  if(esmc_check_itu_oui(msg->itu_oui) < 0) {
    return -1;
  }
  if(esmc_check_itu_subtype(msg->itu_subtype) < 0) {
    return -1;
  }
  if(esmc_check_event_version(&msg->version_event_flag_reserved) < 0) {
    return -1;
  }
  if(esmc_check_ql_tlv(&msg->ql_tlv) < 0) {
    return -1;
  }
  if(esmc_check_ext_ql_tlv(&msg->ext_ql_tlv, enhanced_flag) < 0) {
    return -1;
  }

  /* Parse and derive QL based on SSM and enhanced SSM code combination (enhanced SSM code may be zero) using mapping functions */
  if(esmc_ssm_and_essm_to_ql_map(esmc->net_opt, msg->ql_tlv.ssm_code, msg->ext_ql_tlv.esmc_e_ssm_code, parsed_ql) < 0) {
    return -1;
  }

  if(*enhanced_flag == 1) {
    /* Extended QL TLV was detected and checked, so parse data */
    esmc_parse_ext_ql_tlv_data(&msg->ext_ql_tlv, parsed_ext_ql_tlv_data);
  } else {
    memset(parsed_ext_ql_tlv_data, 0, sizeof(*parsed_ext_ql_tlv_data));
  }

  return 0;
}

#if (SYNCED_DEBUG_MODE == 1)
void esmc_print_esmc_pdu(T_esmc_pdu *msg, T_esmc_print_esmc_pdu_type type)
{
  unsigned char *ptr = (unsigned char *)msg;

  if(type == E_esmc_print_esmc_pdu_type_tx) {
    pr_debug("  <<   TX  >>");
  } else if(type == E_esmc_print_esmc_pdu_type_rx) {
    pr_debug("  >>   RX  <<");
  } else {
    return;
  }

  /* Ethernet header */
  pr_debug("  [Ethernet header] Destination address: %02X:%02X:%02X:%02X:%02X:%02X",
           ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
  ptr += ESMC_PDU_ETH_HDR_DST_ADDR_LEN;

  pr_debug("  [Ethernet header] Source address: %02X:%02X:%02X:%02X:%02X:%02X",
           ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
  ptr += ESMC_PDU_ETH_HDR_SRC_ADDR_LEN;

  pr_debug("  [Ethernet header] Slow protocol ethertype: 0x%02X%02X",
           ptr[0], ptr[1]);
  ptr += ESMC_PDU_ETH_HDR_SLOW_PROTO_ETHERTYPE_LEN;

  pr_debug("  Slow protocol subtype: 0x%02X",
           *ptr);
  ptr += ESMC_PDU_SLOW_PROTO_SUBTYPE_LEN;

  pr_debug("  ITU OUI: 0x%02X-0x%02X-0x%02X",
           ptr[0], ptr[1], ptr[2]);
  ptr += ESMC_PDU_ITU_OUI_LEN;

  pr_debug("  ITU subtype: 0x%02X-0x%02X",
           ptr[0], ptr[1]);
  ptr += ESMC_PDU_ITU_SUBTYPE_LEN;

  pr_debug("  Version: %d",
           ESMC_PDU_GET_VER(*ptr));
  pr_debug("  Type: %s",
           (ESMC_PDU_GET_EVENT_FLAG(*ptr) & E_esmc_pdu_type_event) ? "event message" : "information message");
  ptr += ESMC_PDU_VER_EVENT_LEN;

  pr_debug("  Reserved: 0x%02X-0x%02X-0x%02X",
           ptr[0], ptr[1], ptr[2]);
  ptr += ESMC_PDU_RSVD_LEN;

  /* QL TLV */
  pr_debug("  [QL TLV] Type: 0x%02X",
           *ptr);
  ptr += ESMC_PDU_QL_TLV_TYPE_LEN;

  pr_debug("  [QL TLV] Length: 0x%02X%02X",
           ptr[0], ptr[1]);
  ptr += ESMC_PDU_QL_TLV_LENGTH_LEN;

  pr_debug("  [QL TLV] SSM code: 0x%02X",
           *ptr);
  ptr += ESMC_PDU_QL_SSM_CODE_LEN;

  /* Extended QL TLV */
  pr_debug("  [Extended QL TLV] Type: 0x%02X",
           *ptr);
  ptr += ESMC_PDU_EXT_QL_TLV_TYPE_LEN;

  pr_debug("  [Extended QL TLV] Length: 0x%02X%02X",
           ptr[0], ptr[1]);
  ptr += ESMC_PDU_EXT_QL_TLV_LENGTH_LEN;

  pr_debug("  [Extended QL TLV] Enhanced SSM code: 0x%02X",
           *ptr);
  ptr += ESMC_PDU_EXT_QL_TLV_ENHANCED_SSM_CODE_LEN;

  pr_debug("  [Extended QL TLV] Sync-E clock identity: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
           ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7]);
  ptr += ESMC_PDU_EXT_QL_TLV_SYNCE_CLOCK_ID_LEN;

  pr_debug("  [Extended QL TLV] Flag: 0x%02X",
           *ptr);
  ptr += ESMC_PDU_EXT_QL_TLV_FLAG_LEN;

  pr_debug("  [Extended QL TLV] Number of cascaded eEECs: 0x%02X",
           *ptr);
  ptr += ESMC_PDU_EXT_QL_TLV_NUM_CASCADED_EEEC_LEN;

  pr_debug("  [Extended QL TLV] Number of cascaded EECs: 0x%02X",
           *ptr);
  ptr += ESMC_PDU_EXT_QL_TLV_NUM_CASCADED_EEC_LEN;

  pr_debug("  [Extended QL TLV] Reserved: 0x%02X-0x%02X-0x%02X-0x%02X-0x%02X",
           ptr[0], ptr[1], ptr[2], ptr[3], ptr[4]);
  ptr += ESMC_PDU_RSVD_LEN;

  pr_debug("  Padding: 0x%02X-0x%02X-0x%02X-0x%02X-0x%02X-0x%02X-0x%02X-0x%02X-0x%02X-0x%02X-0x%02X-0x%02X",
           ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8], ptr[9], ptr[10], ptr[11]);
  ptr += ESMC_PDU_PADDING_LEN;

  if(type == E_esmc_print_esmc_pdu_type_tx) {
    pr_debug("  <<   TX  >>");
  } else if(type == E_esmc_print_esmc_pdu_type_rx) {
    pr_debug("  >>   RX  <<");
  }
}
#endif

T_esmc *esmc_get_stack_data(void)
{
  return &g_esmc;
}
