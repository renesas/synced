/**
 * @file esmc_adaptor.c
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

#include "esmc.h"
#include "../esmc_adaptor/esmc_adaptor.h"
#include "../../common/common.h"

/* Static data */

static int g_esmc_tx_port_to_sync_idx_map[MAX_NUM_PORTS];
static int g_esmc_rx_port_to_sync_idx_map[MAX_NUM_PORTS];
static T_esmc_adaptor_tx_event_cb g_esmc_tx_event_cb = NULL;
static T_esmc_adaptor_rx_event_cb g_esmc_rx_event_cb = NULL;
static unsigned char g_esmc_tx_mac_addr[MAX_NUM_PORTS][ETH_ALEN];
int g_esmc_tx_mac_addr_num;

/* Static functions */

static int esmc_tx_event_cb(T_esmc_tx_event_cb_data *cb_data)
{
  T_esmc_adaptor_tx_event_cb_data generic_cb_data;
  T_port_num port_num;

  if(g_esmc_tx_event_cb == NULL) {
    return -1;
  }

  if(cb_data == NULL) {
    return -1;
  }

  generic_cb_data.event_type = cb_data->event_type;
  port_num = cb_data->port_num;

  switch(cb_data->event_type) {
    case E_esmc_event_type_port_link_up:
    case E_esmc_event_type_port_link_down:
      break;

    default:
      return -1;
  }

  if((port_num < 0) || (port_num >= MAX_NUM_PORTS)) {
    return -1;
  }

  generic_cb_data.sync_idx = g_esmc_tx_port_to_sync_idx_map[port_num];
  g_esmc_tx_event_cb(&generic_cb_data);

  return 0;
}

static int esmc_rx_event_cb(T_esmc_rx_event_cb_data *cb_data)
{
  T_esmc_adaptor_rx_event_cb_data generic_cb_data;
  T_port_num port_num;

  if(g_esmc_rx_event_cb == NULL) {
    return -1;
  }

  if(cb_data == NULL) {
    return -1;
  }

  generic_cb_data.event_type = cb_data->event_type;
  port_num = cb_data->port_num;

  switch(cb_data->event_type) {
    case E_esmc_event_type_invalid_rx_ql:
      break;

    case E_esmc_event_type_ql_change:
      generic_cb_data.event_data.ql_change.new_ql = cb_data->event_data.ql_change.new_ql;
      generic_cb_data.event_data.ql_change.new_num_cascaded_eEEC = cb_data->event_data.ql_change.new_num_cascaded_eEEC;
      generic_cb_data.event_data.ql_change.new_num_cascaded_EEC = cb_data->event_data.ql_change.new_num_cascaded_EEC;
      break;

    case E_esmc_event_type_rx_timeout:
    case E_esmc_event_type_port_link_up:
    case E_esmc_event_type_port_link_down:
      break;

    case E_esmc_event_type_immediate_timing_loop:
    case E_esmc_event_type_originator_timing_loop:
      generic_cb_data.event_data.timing_loop.mac_addr = cb_data->event_data.timing_loop.mac_addr;
      break;

    default:
      return -1;
  }

  if((port_num < 0) || (port_num >= MAX_NUM_PORTS)) {
    return -1;
  }

  generic_cb_data.sync_idx = g_esmc_rx_port_to_sync_idx_map[port_num];
  g_esmc_rx_event_cb(&generic_cb_data);

  return 0;
}

/* Global functions */

int esmc_adaptor_init(T_esmc_config *config)
{
  T_tx_port_info const *tx_port;
  T_rx_port_info const *rx_port;

  int num_tx_ports = (config->num_tx_ports > MAX_NUM_PORTS) ? MAX_NUM_PORTS : config->num_tx_ports;
  int num_rx_ports = (config->num_rx_ports > MAX_NUM_PORTS) ? MAX_NUM_PORTS : config->num_rx_ports;

  T_esmc_network_option net_opt = config->net_opt;
  T_esmc_ql init_ql = config->init_ql;
  T_esmc_ql do_not_use_ql = config->do_not_use_ql;

  int i;
  T_port_num tx_port_num;
  T_port_num rx_port_num;

  /* Initialize static variables */
  for(i = 0; i < MAX_NUM_PORTS; i++) {
    g_esmc_tx_port_to_sync_idx_map[i] = INVALID_SYNC_IDX;
    g_esmc_rx_port_to_sync_idx_map[i] = INVALID_SYNC_IDX;
  }
  memset(&g_esmc_tx_mac_addr, 0, sizeof(g_esmc_tx_mac_addr));
  g_esmc_tx_mac_addr_num = 0;

  /* Create and initialize ESMC stack */
  if(esmc_create_stack() < 0) {
    return -1;
  }
  if(esmc_init_stack(net_opt, init_ql, do_not_use_ql) < 0) {
    return -1;
  }

  /* Create and initialize ESMC TX ports */
  tx_port = config->tx_port_array;
  if(esmc_create_tx_ports(tx_port, num_tx_ports) < 0) {
    return -1;
  }
  if(esmc_init_tx_ports() < 0) {
    return -1;
  }
  /* Set ESMC TX port to sync index map */
  for(i = 0; i < num_tx_ports; i++) {
    tx_port_num = tx_port->port_num;
    if((tx_port_num >= 0) && (tx_port_num < MAX_NUM_PORTS)) {
      g_esmc_tx_port_to_sync_idx_map[tx_port_num] = tx_port->sync_idx;
      memcpy(&g_esmc_tx_mac_addr[g_esmc_tx_mac_addr_num], tx_port->mac_addr, ETH_ALEN);
      g_esmc_tx_mac_addr_num++;
    }
    else {
      return -1;
    }
    tx_port++;
  }

  /* Create and initialize ESMC RX ports */
  rx_port = config->rx_port_array;
  if(esmc_create_rx_ports(rx_port, num_rx_ports) < 0) {
    return -1;
  }
  if(esmc_init_rx_ports() < 0) {
    return -1;
  }
  /* Set ESMC RX port to sync index map */
  for(i = 0; i < num_rx_ports; i++) {
    rx_port_num = rx_port->port_num;
    if((rx_port_num >= 0) && (rx_port_num < MAX_NUM_PORTS)) {
      g_esmc_rx_port_to_sync_idx_map[rx_port_num] = rx_port->sync_idx;
    }
    else {
      return -1;
    }
    rx_port++;
  }

  return 0;
}

int esmc_adaptor_start(void)
{
  return esmc_check_init();
}

int esmc_adaptor_set_tx_ql(T_esmc_ql ql, int best_sync_idx, unsigned long long sync_tx_bundle_bitmap)
{
  T_port_num best_port_num;
  unsigned long long port_tx_bundle_bitmap;
  int sync_idx;
  int i;

  if(best_sync_idx == INVALID_SYNC_IDX) {
    esmc_set_best_ql(ql, INVALID_PORT_NUM, 0);
    return 0;
  }

  /* Find best port number */
  best_port_num = INVALID_PORT_NUM;
  for(i = 0; i < MAX_NUM_PORTS; i++) {
    if(g_esmc_rx_port_to_sync_idx_map[i] == best_sync_idx) {
      best_port_num = i;
      break;
    }
  }

  /* Convert the sync bitmap into port bitmap */
  sync_idx = 0;
  port_tx_bundle_bitmap = 0;
  while(sync_tx_bundle_bitmap != 0) {
    if(sync_tx_bundle_bitmap & 1) {
      for(i = 0; i < MAX_NUM_PORTS; i++) {
        if(g_esmc_rx_port_to_sync_idx_map[i] == sync_idx) {
          port_tx_bundle_bitmap |= (unsigned long long)1 << i;
          break;
        }
      }
    }
    sync_tx_bundle_bitmap >>= 1;
    sync_idx++;
  }

  esmc_set_best_ql(ql, best_port_num, port_tx_bundle_bitmap);

  return 0;
}

int esmc_adaptor_register_tx_cb(T_esmc_adaptor_tx_event_cb cb)
{
  if(cb == NULL) {
    return -1;
  }
  g_esmc_tx_event_cb = cb;

  return esmc_reg_tx_cb(esmc_tx_event_cb);
}

int esmc_adaptor_register_rx_cb(T_esmc_adaptor_rx_event_cb cb)
{
  if(cb == NULL) {
    return -1;
  }
  g_esmc_rx_event_cb = cb;

  return esmc_reg_rx_cb(esmc_rx_event_cb);
}

int esmc_adaptor_stop(void)
{
  return 0;
}

int esmc_adaptor_deinit(void)
{
  int i;

  /* Deinitialize static variables */
  for(i = 0; i < MAX_NUM_PORTS; i++) {
    g_esmc_tx_port_to_sync_idx_map[i] = INVALID_SYNC_IDX;
    g_esmc_rx_port_to_sync_idx_map[i] = INVALID_SYNC_IDX;
  }
  g_esmc_tx_event_cb = NULL;
  g_esmc_rx_event_cb = NULL;
  memset(&g_esmc_tx_mac_addr, 0, sizeof(g_esmc_tx_mac_addr));
  g_esmc_tx_mac_addr_num = 0;

  esmc_destroy_tx_ports();
  esmc_destroy_rx_ports();
  esmc_destroy_stack();

  return 0;
}

int esmc_adaptor_check_mac_addr(const unsigned char mac_addr[ETH_ALEN])
{
  int i;

  for(i = 0; i < g_esmc_tx_mac_addr_num; i++) {
    if(memcmp(&g_esmc_tx_mac_addr[i], mac_addr, ETH_ALEN) == 0) {
      return 1;
    }
  }

  /* Not found */
  return 0;
}
