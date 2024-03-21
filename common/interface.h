/**
 * @file interface.h
 * @brief Implements network interface data structures.
 * @note Copyright (C) 2020 Richard Cochran <richardcochran@gmail.com>
 * @note SPDX-License-Identifier: GPL-2.0+
 */
/********************************************************************************************************************
* Release Tag: 2-0-5
* Pipeline ID: 310964
* Commit Hash: b166f770
********************************************************************************************************************/

#ifndef INTERFACE_H
#define INTERFACE_H

#include <sys/queue.h>

#include "types.h"

#define INTERFACE_MAX_NAME_LEN   16

struct interface;

struct interface *interface_create(const char *name);
void interface_destroy(struct interface *iface);

int interface_config_idx_and_mac_addr(struct interface *iface);
void interface_config_clk_idx(struct interface *iface, int clk_idx);
void interface_config_pri(struct interface *iface, int pri);
void interface_config_tx_en(struct interface *iface, int tx_en);
void interface_config_rx_en(struct interface *iface, int rx_en);
void interface_config_ql(struct interface *iface, T_esmc_ql ql);
void interface_config_type(struct interface *iface, T_sync_type type);
void interface_config_tx_bundle_num(struct interface *iface, int tx_bundle_num);

const char *interface_get_name(struct interface *iface);
int interface_get_idx(struct interface *iface);
struct sockaddr_ll interface_get_mac_addr(struct interface *iface);
int interface_get_clk_idx(struct interface *iface);
unsigned int interface_get_pri(struct interface *iface);
int interface_get_tx_en(struct interface *iface);
int interface_get_rx_en(struct interface *iface);
const char *interface_get_ql_str(struct interface *iface);
T_esmc_ql interface_get_ql(struct interface *iface);
T_sync_type interface_get_type(struct interface *iface);
int interface_get_tx_bundle_num(struct interface *iface);

#endif /* INTERFACE_H */
