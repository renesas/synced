/**
 * @file raw_socket.c
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

#include <arpa/inet.h>
#include <errno.h>
#include <linux/filter.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esmc.h"
#include "raw_socket.h"
#include "../../common/common.h"
#include "../../common/print.h"

#define RAW_SOCKET_NUM_FILTER_BLOCKS   14

#define RAW_SOCKET_MAC_ADDR_FIRST_TWO_MSB_BLOCK   7
#define RAW_SOCKET_MAC_ADDR_LAST_FOUR_LSB_BLOCK   5

#define RAW_SOCKET_MAC_ADDR_GET_TWO_MSB(byte0, byte1)                  ((byte0 << 8) | (byte1 << 0))
#define RAW_SOCKET_MAC_ADDR_GET_FOUR_LSB(byte2, byte3, byte4, byte5)   ((byte2 << 24) | (byte3 << 16) | (byte4 << 8) | (byte5 << 0))

/*
 * Raw socket filter
 *
 * Raw socket filter is used to permit reception of only valid ESMC PDUs by raw socket. Filter is attached to socket to
 * filter incoming packets. As a result, filtering is very efficient because packet matching is facilitated by kernel as
 * opposed to application.
 *
 * Filter discards non-Ethernet Slow Protocol frames by checking that frames specify the required IEEE Slow protocol
 * multicast MAC address, stops interfaces from capturing its own transmission, and ignores packets with a length
 * less than 28 bytes and greater than 128 bytes. ESMC PDUs have a minimum length of 28 bytes and maximum length of 128
 * bytes.
 *
 * Application validates Slow Protocol subtype, ITU OUI, ITU subtype, and (ITU OSSP) version values.
 *
 * Packet-matching code is generated via tcpdump.
 *
 * Command used to generate packet-matching code as C program fragment (stored in g_rawsocket_filter):
 * sudo tcpdump -dd ether dst 01:80:C2:00:00:02 and not ether src aa:bb:cc:dd:ee:ff and ether proto 0x8809 and less 128.
 *
 * Command used to generate packet-matching code in human readable form (see below):
 * sudo tcpdump -d ether dst 01:80:C2:00:00:02 and not ether src aa:bb:cc:dd:ee:ff and ether proto 0x8809 and less 128.
 *
 * (000) ld       [2]
 * (001) jeq      #0xc2000002      jt 2   jf 13
 * (002) ldh      [0]
 * (003) jeq      #0x180           jt 4   jf 13
 * (004) ld       [8]
 * (005) jeq      #0xccddeeff      jt 6   jf 8
 * (006) ldh      [6]
 * (007) jeq      #0xaabb          jt 13  jf 8
 * (008) ldh      [12]
 * (009) jeq      #0x8809          jt 10  jf 13
 * (010) ld       #pktlen
 * (011) jgt      #0x80            jt 13  jf 12
 * (012) ret      #262144
 * (013) ret      #0
 */
static struct sock_filter g_rawsocket_filter[RAW_SOCKET_NUM_FILTER_BLOCKS] = {
  {0x20, 0, 0, 0x00000002},  /* (000) */
  {0x15, 0, 11, 0xc2000002}, /* (001) Last four LSB of destination address (i.e. IEEE Slow protocol multicast MAC address): 0xC2000002 */
  {0x28, 0, 0, 0x00000000},  /* (002) */
  {0x15, 0, 9, 0x00000180},  /* (003) First two MSB of source MAC address: 0X0180 */
  {0x20, 0, 0, 0x00000008},  /* (004) */
  {0x15, 0, 2, 0xccddeeff},  /* (005) Last four LSB of source MAC address: 0xCCDDEEFF (RAW_SOCKET_MAC_ADDR_LAST_FOUR_LSB_BLOCK) */
  {0x28, 0, 0, 0x00000006},  /* (006) */
  {0x15, 5, 0, 0x0000aabb},  /* (007) First two MSB of source MAC address: 0XAABB (RAW_SOCKET_MAC_ADDR_FIRST_TWO_MSB_BLOCK) */
  {0x28, 0, 0, 0x0000000c},  /* (008) */
  {0x15, 0, 3, 0x00008809},  /* (009) */
  {0x80, 0, 0, 0x00000000},  /* (010) */
  {0x25, 1, 0, 0x00000080},  /* (011) */
  {0x6, 0, 0, 0x00040000},   /* (012) */
  {0x6, 0, 0, 0x00000000},   /* (013) */
};

int raw_socket_open(const char *name, T_port_num port_num, struct sockaddr_ll *mac_addr)
{
  int fd;
  struct sockaddr_ll addr;
  struct sock_fprog fprg = {RAW_SOCKET_NUM_FILTER_BLOCKS, g_rawsocket_filter};
  struct packet_mreq mreq;
  unsigned char mcast_addr[ETH_ALEN] = ESMC_PDU_IEEE_SLOW_PROTO_MCAST_ADDR;

  int first_two_msb = RAW_SOCKET_MAC_ADDR_GET_TWO_MSB(mac_addr->sll_addr[0], mac_addr->sll_addr[1]);
  int last_four_lsb = RAW_SOCKET_MAC_ADDR_GET_FOUR_LSB(mac_addr->sll_addr[2], mac_addr->sll_addr[3], mac_addr->sll_addr[4], mac_addr->sll_addr[5]);

  g_rawsocket_filter[RAW_SOCKET_MAC_ADDR_LAST_FOUR_LSB_BLOCK].k = last_four_lsb & 0xFFFFFFFF;
  g_rawsocket_filter[RAW_SOCKET_MAC_ADDR_FIRST_TWO_MSB_BLOCK].k = first_two_msb & 0x0000FFFF;

  if((fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
    pr_err("%s: %s", __func__, strerror(errno));
    return UNINITIALIZED_FD;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sll_ifindex = port_num;
  addr.sll_family = AF_PACKET;
  addr.sll_protocol = htons(ETH_P_SLOW);
  if(bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
    goto err;
  }
  if(setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, name, strlen(name))) {
    goto err;
  }

  if(setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &fprg, sizeof(fprg)) < 0) {
    goto err;
  }

  memset(&mreq, 0, sizeof(mreq));
  mreq.mr_ifindex = port_num;
  mreq.mr_type = PACKET_MR_MULTICAST;
  mreq.mr_alen = ETH_ALEN;
  memcpy(mreq.mr_address, mcast_addr, ETH_ALEN);

  if(setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    goto err;
  }

  return fd;

err:
  close(fd);
  pr_err("%s: %s", __func__, strerror(errno));
  return UNINITIALIZED_FD;
}

int raw_socket_send(int fd, void *msg, int msg_len, int flags, struct sockaddr_ll *dst_addr, int dst_addr_len)
{
  return (int)sendto(fd, msg, msg_len, flags, (struct sockaddr *)dst_addr, (socklen_t)dst_addr_len);
}

int raw_socket_recv(int fd, void *msg, int msg_len, int flags, struct sockaddr_ll *src_addr, int src_addr_len)
{
  return (int)recvfrom(fd, msg, msg_len, flags, (struct sockaddr *)src_addr, (socklen_t *)&src_addr_len);
}

int raw_socket_close(int fd)
{
  return close(fd);
}
