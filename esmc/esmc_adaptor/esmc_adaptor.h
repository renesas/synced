/**
 * @file esmc_adaptor.h
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

#ifndef ESMC_ADAPTOR_H
#define ESMC_ADAPTOR_H

#include <linux/if_ether.h>
#include <stdio.h>

#include "../../common/types.h"

#define ESMC_MAX_NUMBER_OF_PORTS   32

typedef struct {
  const char *name;
  T_port_num port_num;
  unsigned char mac_addr[ETH_ALEN];
  int sync_idx;
  int check_link_status;
} T_tx_port_info;

typedef struct {
  const char *name;
  T_port_num port_num;
  unsigned char mac_addr[ETH_ALEN];
  int sync_idx;
} T_rx_port_info;

typedef struct {
  T_esmc_network_option net_opt;
  T_esmc_ql init_ql;
  T_esmc_ql do_not_use_ql;
  int num_tx_ports;
  int num_rx_ports;
  T_tx_port_info const *tx_port_array;
  T_rx_port_info const *rx_port_array;
} T_esmc_config;

typedef enum {
  E_esmc_event_type_invalid_rx_ql,
  E_esmc_event_type_ql_change,
  E_esmc_event_type_rx_timeout,
  E_esmc_event_type_port_link_up,
  E_esmc_event_type_port_link_down,
  E_esmc_event_type_immediate_timing_loop,
  E_esmc_event_type_originator_timing_loop,
  E_esmc_event_type_max
} T_esmc_event_type;

typedef struct {
  T_esmc_ql new_ql;
  int new_num_cascaded_eEEC;
  int new_num_cascaded_EEC;
} T_event_ql_change;

typedef struct {
  unsigned char *mac_addr;
} T_event_timing_loop;

typedef struct {
  T_esmc_event_type event_type;
  int sync_idx;
} T_esmc_adaptor_tx_event_cb_data;

typedef struct {
  T_esmc_event_type event_type;
  int sync_idx;
  union {
    T_event_ql_change ql_change;
    T_event_timing_loop timing_loop;
  } event_data;
} T_esmc_adaptor_rx_event_cb_data;

typedef int (*T_esmc_adaptor_tx_event_cb)(T_esmc_adaptor_tx_event_cb_data *cb_data);
typedef int (*T_esmc_adaptor_rx_event_cb)(T_esmc_adaptor_rx_event_cb_data *cb_data);

int esmc_adaptor_init(T_esmc_config *config);
int esmc_adaptor_start(void);
int esmc_adaptor_set_tx_ql(T_esmc_ql ql, int best_sync_idx, unsigned long long sync_tx_bundle_bitmap);
int esmc_adaptor_register_tx_cb(T_esmc_adaptor_tx_event_cb cb);
int esmc_adaptor_register_rx_cb(T_esmc_adaptor_rx_event_cb cb);
int esmc_adaptor_stop(void);
int esmc_adaptor_deinit(void);

int esmc_adaptor_check_mac_addr(const unsigned char mac_addr[ETH_ALEN]);

#endif /* ESMC_ADAPTOR_H */
