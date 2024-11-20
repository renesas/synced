/**
 * @file port.h
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
* Release Tag: 2-0-8
* Pipeline ID: 426834
* Commit Hash: 62f27b58
********************************************************************************************************************/

#ifndef PORT_H
#define PORT_H

#include <linux/if_packet.h>

#include "../common/common.h"
#include "../esmc_adaptor/esmc_adaptor.h"

typedef struct {
  unsigned char originator_clock_id[MAX_CLK_ID_LEN];
  int mixed_EEC_eEEC;                                /* Equivalent to ITU-T G.8264 (08/2017) Amd. 1 (03/2018) bit 0 of extended QL TLV flag field */
  int partial_chain;                                 /* Equivalent to ITU-T G.8264 (08/2017) Amd. 1 (03/2018) bit 1 of extended QL TLV flag field */
  int num_cascaded_eEEC;
  int num_cascaded_EEC;
} T_port_ext_ql_tlv_data;

typedef struct T_port_tx_data T_port_tx_data;
typedef struct T_port_rx_data T_port_rx_data;

T_port_tx_data *port_tx_create(T_tx_port_info const *tx_port);
T_port_rx_data *port_rx_create(T_rx_port_info const *rx_port);

void port_tx_init(T_port_tx_data *tx_p);
void port_rx_init(T_port_rx_data *rx_p);

int port_tx_check(T_port_tx_data *tx_p);
int port_rx_check(T_port_rx_data *rx_p);

int port_get_rx_ext_ql_tlv_data(T_port_rx_data *rx_p, T_port_num best_port_num, T_port_ext_ql_tlv_data *best_ext_ql_tlv_data);

void port_tx_stop(T_port_tx_data *tx_p);
void port_tx_wait_stop(T_port_tx_data *tx_p);
void port_tx_close(T_port_tx_data *tx_p);

void port_rx_stop(T_port_rx_data *rx_p);
void port_rx_wait_stop(T_port_rx_data *rx_p);
void port_rx_close(T_port_rx_data *rx_p);


#endif /* PORT_H */
