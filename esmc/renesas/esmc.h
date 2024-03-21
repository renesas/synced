/**
 * @file esmc.h
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

#ifndef ESMC_H
#define ESMC_H

#include <linux/if.h>
#include <linux/if_ether.h>
#include <pthread.h>
#include <sys/queue.h>

#include "port.h"
#include "../esmc_adaptor/esmc_adaptor.h"
#include "../../common/common.h"
#include "../../common/types.h"
#include "../../management/management.h"

#define ESMC_TX_HEARTBEAT_PERIOD_MS   1000 /* Milliseconds */
#define ESMC_RX_HEARTBEAT_PERIOD_MS   50   /* Milliseconds */
#define ESMC_RX_TIMEOUT_PERIOD_S      5    /* Seconds */

#define ESMC_PDU_IEEE_SLOW_PROTO_MCAST_ADDR   {0x01, 0x80, 0xC2, 0x00, 0x00, 0x02}

#define ESMC_PDU_EXT_QL_TLV_MIXED_EEC_EEEC_FLAG_LSB   0
#define ESMC_PDU_EXT_QL_TLV_PARTIAL_CHAIN_FLAG_LSB    1

/* Network interface controller (NIC) appends an additional 4 bytes for frame check sequence (FCS), which actually brings total length of ESMC PDU to 64 bytes */
#define ESMC_PDU_LEN   60

#define ESMC_PDU_QL_TLV_OFFSET       24
#define ESMC_PDU_EXT_QL_TLV_OFFSET   48

#define ESMC_PDU_ETH_HDR_LEN                        14
#define ESMC_PDU_ETH_HDR_DST_ADDR_LEN               ETH_ALEN
#define ESMC_PDU_ETH_HDR_SRC_ADDR_LEN               ETH_ALEN
#define ESMC_PDU_ETH_HDR_SLOW_PROTO_ETHERTYPE_LEN   2
COMPILE_TIME_ASSERT((ESMC_PDU_ETH_HDR_DST_ADDR_LEN
                     + ESMC_PDU_ETH_HDR_SRC_ADDR_LEN
                     + ESMC_PDU_ETH_HDR_SLOW_PROTO_ETHERTYPE_LEN) == ESMC_PDU_ETH_HDR_LEN,
                    "Invalid lengths for Ethernet header macros!")

#define ESMC_PDU_MISC_FIELD_LEN           10
#define ESMC_PDU_SLOW_PROTO_SUBTYPE_LEN   1
#define ESMC_PDU_ITU_OUI_LEN              3
#define ESMC_PDU_ITU_SUBTYPE_LEN          2
#define ESMC_PDU_VER_EVENT_LEN            1
#define ESMC_PDU_RSVD_LEN                 3
COMPILE_TIME_ASSERT((ESMC_PDU_SLOW_PROTO_SUBTYPE_LEN
                     + ESMC_PDU_ITU_OUI_LEN
                     + ESMC_PDU_ITU_SUBTYPE_LEN
                     + ESMC_PDU_VER_EVENT_LEN
                     + ESMC_PDU_RSVD_LEN) == ESMC_PDU_MISC_FIELD_LEN,
                    "Invalid lengths for miscellaneous ESMC PDU field macros!")

#define ESMC_PDU_QL_TLV_LEN          4
#define ESMC_PDU_QL_TLV_TYPE_LEN     1
#define ESMC_PDU_QL_TLV_LENGTH_LEN   2
#define ESMC_PDU_QL_SSM_CODE_LEN     1
COMPILE_TIME_ASSERT((ESMC_PDU_QL_TLV_TYPE_LEN
                     + ESMC_PDU_QL_TLV_LENGTH_LEN
                     + ESMC_PDU_QL_SSM_CODE_LEN) == ESMC_PDU_QL_TLV_LEN,
                    "Invalid lengths for QL TLV macros!")

#define ESMC_PDU_EXT_QL_TLV_LEN                     20
#define ESMC_PDU_EXT_QL_TLV_TYPE_LEN                1
#define ESMC_PDU_EXT_QL_TLV_LENGTH_LEN              2
#define ESMC_PDU_EXT_QL_TLV_ENHANCED_SSM_CODE_LEN   1
#define ESMC_PDU_EXT_QL_TLV_SYNCE_CLOCK_ID_LEN      8
#define ESMC_PDU_EXT_QL_TLV_FLAG_LEN                1
#define ESMC_PDU_EXT_QL_TLV_NUM_CASCADED_EEEC_LEN   1
#define ESMC_PDU_EXT_QL_TLV_NUM_CASCADED_EEC_LEN    1
#define ESMC_PDU_EXT_QL_TLV_RSVD_LEN                5
COMPILE_TIME_ASSERT((ESMC_PDU_EXT_QL_TLV_TYPE_LEN
                     + ESMC_PDU_EXT_QL_TLV_LENGTH_LEN
                     + ESMC_PDU_EXT_QL_TLV_ENHANCED_SSM_CODE_LEN
                     + ESMC_PDU_EXT_QL_TLV_SYNCE_CLOCK_ID_LEN
                     + ESMC_PDU_EXT_QL_TLV_FLAG_LEN
                     + ESMC_PDU_EXT_QL_TLV_NUM_CASCADED_EEEC_LEN
                     + ESMC_PDU_EXT_QL_TLV_NUM_CASCADED_EEC_LEN
                     + ESMC_PDU_EXT_QL_TLV_RSVD_LEN) == ESMC_PDU_EXT_QL_TLV_LEN,
                    "Invalid lengths for extended QL TLV macros!")

#define ESMC_PDU_PADDING_LEN   12
COMPILE_TIME_ASSERT((ESMC_PDU_LEN
                     - ESMC_PDU_ETH_HDR_LEN
                     - ESMC_PDU_MISC_FIELD_LEN
                     - ESMC_PDU_QL_TLV_LEN
                     - ESMC_PDU_EXT_QL_TLV_LEN) == ESMC_PDU_PADDING_LEN,
                    "Invalid length for ESMC PDU padding macro!")

/* QL TLV */
struct __attribute__((packed)) ql_tlv {
  unsigned char type;
  unsigned char length[ESMC_PDU_QL_TLV_LENGTH_LEN];
  unsigned char ssm_code;
};
typedef struct ql_tlv T_esmc_ql_tlv;
COMPILE_TIME_ASSERT(sizeof(T_esmc_ql_tlv) == ESMC_PDU_QL_TLV_LEN, "Invalid structure size for QL TLV!")

/* Extended QL TLV */
struct __attribute__((packed)) ext_ql_tlv {
  unsigned char type;
  unsigned char length[ESMC_PDU_EXT_QL_TLV_LENGTH_LEN];
  unsigned char esmc_e_ssm_code;
  unsigned char syncE_clock_id[ESMC_PDU_EXT_QL_TLV_SYNCE_CLOCK_ID_LEN];
  unsigned char flag;
  unsigned char num_cascaded_eEEC;
  unsigned char num_cascaded_EEC;
  unsigned char reserved[ESMC_PDU_EXT_QL_TLV_RSVD_LEN];
};
typedef struct ext_ql_tlv T_esmc_ext_ql_tlv;
COMPILE_TIME_ASSERT(sizeof(T_esmc_ext_ql_tlv) == ESMC_PDU_EXT_QL_TLV_LEN, "Invalid structure size for extended QL TLV!")

/*
 * ESMC PDU
 *
 * struct ethhdr eth_hdr                                       14
 * unsigned char slow_proto_subtype                             1
 * unsigned char itu_oui[ESMC_PDU_ITU_OUI_LEN]                  3
 * unsigned char itu_subtype[ESMC_PDU_ITU_SUBTYPE_LEN]          2
 * unsigned char version_event_flag_reserved                    1
 * unsigned char reserved[ESMC_PDU_RSVD_LEN]                    3
 * T_esmc_ql_tlv ql_tlv                                         4
 * T_esmc_ext_ql_tlv ext_ql_tlv                                20
 * unsigned char padding[ESMC_PDU_PADDING_LEN]         +       12
 *                                                       --------
 *                                                       60 bytes
 * FCS (appended by hardware)                          +        4
 *                                                       --------
 *                                                       64 bytes
 */
union __attribute__((packed)) esmc_pdu {
  struct __attribute__((packed)) {
    struct ethhdr eth_hdr;
    unsigned char slow_proto_subtype;
    unsigned char itu_oui[ESMC_PDU_ITU_OUI_LEN];
    unsigned char itu_subtype[ESMC_PDU_ITU_SUBTYPE_LEN];
    unsigned char version_event_flag_reserved;
    unsigned char reserved[ESMC_PDU_RSVD_LEN];
    T_esmc_ql_tlv ql_tlv;
    T_esmc_ext_ql_tlv ext_ql_tlv;
    /* NIC appends 4 bytes for FCS after padding; these bytes are consumed by hardware, so there is nothing to handle by software */
    unsigned char padding[ESMC_PDU_PADDING_LEN];
  };
  unsigned char buff[ESMC_PDU_LEN];
};
typedef union esmc_pdu T_esmc_pdu;
COMPILE_TIME_ASSERT(sizeof(T_esmc_pdu) == ESMC_PDU_LEN, "Invalid structure size for ESMC PDU!")

typedef enum {
  E_esmc_state_unknown,
  E_esmc_state_created_stack,
  E_esmc_state_created_tx_ports,
  E_esmc_state_created_rx_ports,
  E_esmc_state_intitialized_stack,
  E_esmc_state_initialized_tx_ports,
  E_esmc_state_initialized_rx_ports,
  E_esmc_state_running,
  E_esmc_state_failed,
} T_esmc_state;

typedef struct {
  int num_ports;
  T_port_num port_nums[ESMC_MAX_NUMBER_OF_PORTS];
} T_port_tx_bundle_info;

typedef struct {
  T_esmc_network_option net_opt;
  T_esmc_ql init_ql;
  T_esmc_ql do_not_use_ql;

  /* Applicable to TX threads */
  T_esmc_ql best_ql;
  T_port_ext_ql_tlv_data best_ext_ql_tlv_data;
  T_port_num best_port_num;
  T_port_tx_bundle_info port_tx_bundle_info;
  pthread_mutex_t best_ql_mutex;
  pthread_cond_t best_ql_cond;

  LIST_HEAD(tx_ports_head, T_port_tx_data) tx_ports;
  int num_tx_ports;

  LIST_HEAD(rx_ports_head, T_port_rx_data) rx_ports;
  int num_rx_ports;

  T_esmc_state state;
} T_esmc;

typedef unsigned int T_port_bitmap;

typedef struct {
  T_esmc_event_type event_type;
  int port_num;
} T_esmc_tx_event_cb_data;

typedef struct {
  T_esmc_event_type event_type;
  int port_num;
  union {
    T_event_ql_change ql_change;
    T_event_timing_loop timing_loop;
  } event_data;
} T_esmc_rx_event_cb_data;

#if (SYNCED_DEBUG_MODE == 1)
typedef enum {
  E_esmc_print_esmc_pdu_type_tx,
  E_esmc_print_esmc_pdu_type_rx,
  E_esmc_print_esmc_pdu_type_max
} T_esmc_print_esmc_pdu_type;
#endif

typedef int (*T_esmc_tx_event_cb)(T_esmc_tx_event_cb_data *cb_data);
typedef int (*T_esmc_rx_event_cb)(T_esmc_rx_event_cb_data *cb_data);

int esmc_create_stack(void);
int esmc_create_tx_ports(T_tx_port_info const *tx_port, int num_tx_ports);
int esmc_create_rx_ports(T_rx_port_info const *rx_port, int num_rx_ports);

int esmc_init_stack(T_esmc_network_option net_opt, T_esmc_ql init_ql, T_esmc_ql do_not_use_ql);
int esmc_init_tx_ports(void);
int esmc_init_rx_ports(void);

int esmc_check_init(void);

void esmc_destroy_stack(void);
void esmc_destroy_tx_ports(void);
void esmc_destroy_rx_ports(void);

int esmc_reg_tx_cb(T_esmc_tx_event_cb event_cb);
int esmc_reg_rx_cb(T_esmc_rx_event_cb event_cb);
int esmc_call_tx_cb(T_esmc_tx_event_cb_data *cb_data);
int esmc_call_rx_cb(T_esmc_rx_event_cb_data *cb_data);

void esmc_set_best_ql(T_esmc_ql best_ql, T_port_num best_port_num, T_port_tx_bundle_info *port_tx_bundle_info);

int esmc_compose_pdu(T_esmc_pdu *msg, T_esmc_pdu_type msg_type, unsigned char src_mac_addr[ETH_ALEN], T_port_ext_ql_tlv_data const *best_ext_ql_tlv_data, T_port_num port_num, T_esmc_ql *composed_ql);
int esmc_parse_pdu(T_esmc_pdu *msg, int *enhanced_flag, T_esmc_ql *parsed_ql, T_port_ext_ql_tlv_data *parsed_ext_ql_tlv_data);

#if (SYNCED_DEBUG_MODE == 1)
void esmc_print_esmc_pdu(T_esmc_pdu *msg, T_esmc_print_esmc_pdu_type type);
#endif

T_esmc *esmc_get_stack_data(void);

#endif /* ESMC_H */
