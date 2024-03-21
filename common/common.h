/**
 * @file common.h
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

#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <linux/if_packet.h>
#include <time.h>

#include "types.h"
#include "../control/sync.h"
#include "../device/device_adaptor/device_adaptor.h"
#include "../esmc/esmc_adaptor/esmc_adaptor.h"
#include "../management/management.h"

#if defined(static_assert)
#define COMPILE_TIME_ASSERT(expression, message)   static_assert((expression), message);
#else
#define COMPILE_TIME_ASSERT(expression, message)
#pragma message "Static asserts are not supported (i.e. checks will not be executed)!"
#endif

/* AA:BB:CC:DD:EE:FF (18 characters, including null character) */
#define MAX_MAC_ADDR_STR_LEN   18

#define MAX_CLK_ID_LEN   8

#define MAX_NUM_OF_SYNC_ENTRIES   64

/* Maximum number of clocks going into device (includes external clock ports) */
#define MAX_NUM_OF_CLOCKS   32

#define MAX_NUM_OF_PRIORITIES   256

#define CHECK_CLOCK_ID_NULL(clock_id) \
  ((clock_id[0] == 0) && \
   (clock_id[1] == 0) && \
   (clock_id[2] == 0) && \
   (clock_id[3] == 0) && \
   (clock_id[4] == 0) && \
   (clock_id[5] == 0) && \
   (clock_id[6] == 0) && \
   (clock_id[7] == 0))

void generate_clock_id(const unsigned char mac_addr[ETH_ALEN], unsigned char clock_id[MAX_CLK_ID_LEN]);

void extract_mac_addr(const unsigned char clock_id[MAX_CLK_ID_LEN], unsigned char mac_addr[ETH_ALEN]);

int conv_net_opt_to_do_not_use_ql(T_esmc_network_option net_opt, T_esmc_ql *do_not_use_ql);

const char *conv_ql_enum_to_str(T_esmc_ql ql);

const char *conv_synce_dpll_state_enum_to_str(T_device_dpll_state synce_dpll_state);

const char *conv_sync_clk_state_enum_to_str(T_sync_clk_state sync_clk_state);

const char *conv_sync_state_enum_to_str(T_sync_state sync_state);

const char *conv_sync_type_enum_to_str(T_sync_type sync_type);

const char *conv_esmc_event_type_enum_to_str(T_esmc_event_type esmc_event_type);

const char *conv_api_code_to_str(T_mng_api api_code);

int check_ql_setting(T_esmc_network_option net_opt, T_esmc_ql ql);

void mac_addr_arr_to_str(struct sockaddr_ll *mac_addr, char mac_addr_str[MAX_MAC_ADDR_STR_LEN]);

int calculate_rank(int ql, unsigned char priority, unsigned char hops);

void print_sync_info(T_management_sync_info *sync_info);

void print_current_status(T_management_status *status);

#endif /* COMMON_H */
