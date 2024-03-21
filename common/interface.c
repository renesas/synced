/**
 * @file interface.c
 * @brief Implements network interface data structures.
 * @note Copyright (C) 2020 Richard Cochran <richardcochran@gmail.com>
 * @note SPDX-License-Identifier: GPL-2.0+
 */
/********************************************************************************************************************
* Release Tag: 2-0-5
* Pipeline ID: 310964
* Commit Hash: b166f770
********************************************************************************************************************/

#include <errno.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common.h"
#include "interface.h"
#include "print.h"
#include "types.h"

struct interface {
  STAILQ_ENTRY(interface) list;

  char name[INTERFACE_MAX_NAME_LEN];
  int idx;

  struct sockaddr_ll mac_addr;

  int clk_idx;
  int pri;

  int tx_en;
  int rx_en;
  int tx_bundle_num;

  T_esmc_ql ql;
  T_sync_type type;
};

/* Static functions */

static int interface_request_net_device_idx(int fd, const char *name, int *idx)
{
  struct ifreq ifreq;
  memset(&ifreq, 0, sizeof(ifreq));
  strncpy(ifreq.ifr_name, name, IFNAMSIZ);

  if(ioctl(fd, SIOCGIFINDEX, &ifreq) < 0) {
    pr_err("%s: %s", __func__, strerror(errno));
    return -1;
  }

  *idx = ifreq.ifr_ifindex;
  return 0;
}

static int interface_request_net_device_mac_addr(int fd, const char *name, struct sockaddr_ll *addr)
{
  struct ifreq ifreq;
  memset(&ifreq, 0, sizeof(ifreq));
  strncpy(ifreq.ifr_name, name, IFNAMSIZ);
  
  if(ioctl(fd, SIOCGIFHWADDR, &ifreq) < 0) {
    pr_err("%s: %s", __func__, strerror(errno));
    return -1;
  }

  memcpy(addr->sll_addr, ifreq.ifr_hwaddr.sa_data, ETH_ALEN);
  addr->sll_family = AF_PACKET;
  addr->sll_halen = ETH_ALEN;
  return 0;
}

static void interface_config_name(struct interface *iface, const char *name)
{
  strncpy(iface->name, name, INTERFACE_MAX_NAME_LEN);
}

static void interface_config_idx(struct interface *iface, int idx)
{
  iface->idx = idx;
}

static void interface_config_mac_addr(struct interface *iface, struct sockaddr_ll *mac_addr)
{
  memcpy(&iface->mac_addr, mac_addr, sizeof(struct sockaddr_ll));
}

/* Global functions */

struct interface *interface_create(const char *name)
{
  struct interface *iface = calloc(1, sizeof(*iface));

  if(!iface)
    return NULL;

  interface_config_name(iface, name);

  return iface;
}

void interface_destroy(struct interface *iface)
{
  free(iface);
}

int interface_config_idx_and_mac_addr(struct interface *iface)
{
  /* Applicable to only Sync-E clock and monitoring ports (external clock ports do not have interface indexes and MAC addresses) */

  const char *name;
  int fd;
  int idx;
  int ret = 0;
  struct sockaddr_ll mac_addr;

  name = interface_get_name(iface);
  if(strlen(name) > (IFNAMSIZ - 1)) {
    /* Length of name (including null character) exceeds IFNAMSIZ, which is size of ifr_name */
    return -1;
  }

  if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    pr_err("%s: %s", __func__, strerror(errno));
    return -1;
  }

  if(interface_request_net_device_idx(fd, name, &idx) < 0) {
    ret = -1;
  }
  interface_config_idx(iface, idx);

  memset(&mac_addr, 0, sizeof(mac_addr));
  if(interface_request_net_device_mac_addr(fd, name, &mac_addr) < 0) {
    ret = -1;
  }
  interface_config_mac_addr(iface, &mac_addr);

  close(fd);
  return ret;
}

void interface_config_clk_idx(struct interface *iface, int clk_idx)
{
  iface->clk_idx = clk_idx;
}

void interface_config_pri(struct interface *iface, int pri)
{
  iface->pri = pri;
}

void interface_config_tx_en(struct interface *iface, int tx_en)
{
  iface->tx_en = tx_en;
}

void interface_config_rx_en(struct interface *iface, int rx_en)
{
  iface->rx_en = rx_en;
}

void interface_config_ql(struct interface *iface, T_esmc_ql ql)
{
  iface->ql = ql;
}

void interface_config_type(struct interface *iface, T_sync_type type)
{
  iface->type = type;
}

void interface_config_tx_bundle_num(struct interface *iface, int tx_bundle_num)
{
  iface->tx_bundle_num = tx_bundle_num;
}

const char *interface_get_name(struct interface *iface)
{
  return iface->name;
}

int interface_get_idx(struct interface *iface)
{
  return iface->idx;
}

struct sockaddr_ll interface_get_mac_addr(struct interface *iface)
{
  return iface->mac_addr;
}

int interface_get_clk_idx(struct interface *iface)
{
  return iface->clk_idx;
}

unsigned int interface_get_pri(struct interface *iface)
{
  return iface->pri;
}

int interface_get_tx_en(struct interface *iface)
{
  return iface->tx_en;
}

int interface_get_rx_en(struct interface *iface)
{
  return iface->rx_en;
}

T_esmc_ql interface_get_ql(struct interface *iface)
{
  return iface->ql;
}

T_sync_type interface_get_type(struct interface *iface)
{
  return iface->type;
}

int interface_get_tx_bundle_num(struct interface *iface)
{
  return iface->tx_bundle_num;
}
