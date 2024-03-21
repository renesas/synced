/**
 * @file port.c
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
#include <linux/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "esmc.h"
#include "port.h"
#include "raw_socket.h"
#include "../esmc_adaptor/esmc_adaptor.h"
#include "../../common/common.h"
#include "../../common/os.h"
#include "../../common/print.h"
#include "../../common/types.h"

#define PORT_MAX_NAME_LEN   INTERFACE_MAX_NAME_LEN

typedef enum {
  E_port_type_tx,
  E_port_type_rx
} T_port_type;

typedef enum {
  E_port_thread_type_tx,
  E_port_thread_type_rx,
  E_port_thread_type_max
} T_port_thread_type;

typedef enum {
  E_port_state_unknown,
  E_port_state_created,
  E_port_state_initialized,
  E_port_state_failed
} T_port_state;

typedef enum  {
  E_port_thread_state_not_started,
  E_port_thread_state_starting,
  E_port_thread_state_started,
  E_port_thread_state_stopping, 
  E_port_thread_state_stopped,
  E_port_thread_state_failed,
} T_port_thread_state;

typedef struct {
  char name[PORT_MAX_NAME_LEN];
  T_port_num port_num;
  struct sockaddr_ll mac_addr;

  int fd;

  int port_link_down_flag;

  T_port_ext_ql_tlv_data ext_ql_tlv;

  pthread_t thread_id;
  T_port_thread_state thread_state;
} T_port_cmn_thread_data;

typedef struct {
  T_port_cmn_thread_data cmn_thread_data;

  int check_link_status;
} T_port_tx_thread_data;

typedef struct {
  T_port_cmn_thread_data cmn_thread_data;

  unsigned long long rx_timeout_monotonic_time_ms;
  int rx_timeout_flag;

  int enhanced_flag;

  T_esmc_ql last_ql;

  int ext_ql_tlv_received_flag;
} T_port_rx_thread_data;

struct T_port_tx_data {
  LIST_ENTRY(tx_port) list;

  T_port_state state;

  T_port_tx_thread_data thread_data;
};

struct T_port_rx_data {
  LIST_ENTRY(rx_port) list;

  T_port_state state;

  T_port_rx_thread_data thread_data;
};

/* Static data */

static pthread_mutex_t g_port_print_mutex = PTHREAD_MUTEX_INITIALIZER;

/* See T_port_thread_type */
static const char *g_port_thread_type_enum_to_str[] = {
  "TX",
  "RX"
};
COMPILE_TIME_ASSERT((sizeof(g_port_thread_type_enum_to_str)/sizeof(g_port_thread_type_enum_to_str[0])) == E_port_thread_type_max, "Invalid array size for g_port_thread_type_enum_to_str!")

/* Static functions */

static int port_open_tx(const char *name, T_port_num port_num, struct sockaddr_ll *mac_addr)
{
  int fd;

  fd = raw_socket_open(name, port_num, mac_addr);
  if(fd == UNINITIALIZED_FD) {
    pr_err("Failed to open TX for port %s (port number: %d)", name, port_num);
    return fd;
  }

  pr_info("Opened TX for port %s (port number: %d)", name, port_num);

  return fd;
}

static int port_open_rx(const char *name, T_port_num port_num, struct sockaddr_ll *mac_addr)
{
  int fd;

  fd = raw_socket_open(name, port_num, mac_addr);
  if(fd == UNINITIALIZED_FD) {
    pr_err("Failed to open RX for port %s (port number: %d)", name, port_num);
    return fd;
  }

  pr_info("Opened RX for port %s (port number: %d)", name, port_num);

  return fd;
}

static void port_close_tx(int fd)
{
  raw_socket_close(fd);
}

static void port_close_rx(int fd)
{
  raw_socket_close(fd);
}

static const char *conv_port_thread_type_enum_to_str(T_port_thread_type thread_type) {
  if(thread_type >= E_port_thread_type_max) {
    pr_err("Failed to convert port thread type enumeration to string");
    return "**unknown**";
  }

  return g_port_thread_type_enum_to_str[thread_type];
}

static int port_thread_start_wait(T_port_thread_type thread_type, T_port_cmn_thread_data *cmn_thread_data)
{
  const int timeout_us = 2000000;
  const int poll_interval_us = 50000;
  int count = (timeout_us / poll_interval_us) + 1;
  volatile T_port_thread_state *thread_state = &cmn_thread_data->thread_state;

  *thread_state = E_port_thread_state_starting;

  while(count--) {
    if(*thread_state == E_port_thread_state_started) {
      pr_debug("Started %s thread for port %s (port number: %d)", conv_port_thread_type_enum_to_str(thread_type), cmn_thread_data->name, cmn_thread_data->port_num);
      return 0;
    }
    usleep(poll_interval_us);
  }

  pr_err("Failed to start %s thread for port %s (port number: %d)", conv_port_thread_type_enum_to_str(thread_type), cmn_thread_data->name, cmn_thread_data->port_num);
  return -1;
}

static void port_thread_stop_wait(T_port_cmn_thread_data *cmn_thread_data)
{
  const int timeout_us = 2000000;
  const int poll_interval_us = 50000;
  int count = (timeout_us / poll_interval_us) + 1;

  if(cmn_thread_data->thread_state == E_port_thread_state_started) {
    cmn_thread_data->thread_state = E_port_thread_state_stopping;
  }

  while(count-- && (cmn_thread_data->thread_state != E_port_thread_state_stopped)) {
    usleep(poll_interval_us);
  }
}

static int port_check_link(int fd, const char *name)
{
  struct ifreq ifreq;
  memset(&ifreq, 0, sizeof(ifreq));
  strncpy(ifreq.ifr_name, name, IFNAMSIZ);

  if(ioctl(fd, SIOCGIFFLAGS, &ifreq) < 0) {
    pr_err("%s: %s", __func__, strerror(errno));
    close(fd);
    return -1;
  }

  /*
   * IFF_UP: indicates that port is active and ready to transfer packets (i.e. up);
   * IFF_RUNNING: indicates that port is operational and cable is plugged in (i.e. running)
   */
  if(ifreq.ifr_flags & IFF_RUNNING) {
    return 0;
  } else {
    return -1;
  }
}

static void *port_tx_thread(void *arg)
{
  T_port_tx_thread_data *tx_thread_data = (T_port_tx_thread_data *)arg;
  T_port_cmn_thread_data *cmn_thread_data;
  volatile T_port_thread_state *thread_state;

  char name[PORT_MAX_NAME_LEN];
  T_port_num port_num;
  unsigned char src_mac_addr[ETH_ALEN];
  int check_link_status;
  int fd;

  T_esmc *esmc;

  struct sockaddr_ll dst_mac_addr;

  const unsigned char slow_proto_mcast_addr[ETH_ALEN] = ESMC_PDU_IEEE_SLOW_PROTO_MCAST_ADDR;

  if(!tx_thread_data) {
    pr_err("No thread data");
    goto err;
  }

  cmn_thread_data = &tx_thread_data->cmn_thread_data;
  thread_state = &cmn_thread_data->thread_state;

  while(*thread_state != E_port_thread_state_starting) {
    usleep(20 * 1000);
  }

  *thread_state = E_port_thread_state_started;

  strncpy(name, cmn_thread_data->name, PORT_MAX_NAME_LEN);
  port_num = cmn_thread_data->port_num;
  memcpy(src_mac_addr, cmn_thread_data->mac_addr.sll_addr, ETH_ALEN);
  check_link_status = tx_thread_data->check_link_status;
  fd = cmn_thread_data->fd;

  esmc = esmc_get_stack_data();

  memset(&dst_mac_addr, 0, sizeof(dst_mac_addr));
  memcpy(dst_mac_addr.sll_addr, slow_proto_mcast_addr, ETH_ALEN);
  dst_mac_addr.sll_halen = ETH_ALEN;
  dst_mac_addr.sll_pkttype = PACKET_MULTICAST;
  dst_mac_addr.sll_ifindex = port_num;

  while(*thread_state == E_port_thread_state_started) {
    T_esmc_pdu_type msg_type;
    T_esmc_pdu msg;
    int msg_len;
    int num_bytes_tx;

    int timeout_flag;

    T_esmc_ql composed_ql;

    T_esmc_tx_event_cb_data cb_data;

    os_mutex_lock(&esmc->best_ql_mutex);
    if(os_cond_timed_wait(&esmc->best_ql_cond, &esmc->best_ql_mutex, ESMC_TX_HEARTBEAT_PERIOD_MS, &timeout_flag) < 0) {
      goto err;
    }
    os_mutex_unlock(&esmc->best_ql_mutex);

    /* Send information ESMC PDU when timeout occurs */
    if(timeout_flag == 1) {
      msg_type = E_esmc_pdu_type_information;
    } else {
      msg_type = E_esmc_pdu_type_event;
    }

    /* Update the link status */
    if(check_link_status != 0) {
      if(port_check_link(fd, name) < 0) {
        /* ESMC TX event : port link down */
        if(cmn_thread_data->port_link_down_flag == 0) {
          cmn_thread_data->port_link_down_flag = 1;

          pr_warning("Link went down on port %s (port number: %d)", name, port_num);

          memset(&cb_data, 0, sizeof(cb_data));
          cb_data.event_type = E_esmc_event_type_port_link_down;
          cb_data.port_num = port_num;

          esmc_call_tx_cb(&cb_data);
        }
        /* No need to compose and send the PDU */
        continue;
      } else if(cmn_thread_data->port_link_down_flag == 1) {
        /* ESMC TX event : port link up */
        cmn_thread_data->port_link_down_flag = 0;

        pr_info("Link is up on port %s (port number: %d)", name, port_num);

        memset(&cb_data, 0, sizeof(cb_data));
        cb_data.event_type = E_esmc_event_type_port_link_up;
        cb_data.port_num = port_num;

        esmc_call_tx_cb(&cb_data);
      }
    }

    memset(&msg, 0, sizeof(msg));
    msg_len = esmc_compose_pdu(&msg, msg_type, src_mac_addr, &esmc->best_ext_ql_tlv_data, port_num, &composed_ql);

    if(msg_len == ESMC_PDU_LEN) {
      num_bytes_tx = raw_socket_send(fd, &msg, msg_len, 0, &dst_mac_addr, sizeof(dst_mac_addr));
      if(num_bytes_tx != ESMC_PDU_LEN) {
        pr_err("Send failed on port %s (port number: %d): %s", name, port_num, strerror(errno));
      } else {
        /* Sent ESMC_PDU_LEN bytes */
        os_mutex_lock(&g_port_print_mutex);
        pr_debug("<<Sent %s ESMC PDU with %s (%d) (extended QL TLV: %s) on port %s (port number: %d)>>",
                 (msg_type == E_esmc_pdu_type_event) ? "event" : "information",
                 conv_ql_enum_to_str(composed_ql),
                 composed_ql,
                 (msg.ext_ql_tlv.esmc_e_ssm_code != 0) ? "yes" : "no",
                 name,
                 port_num);

#if (SYNCED_DEBUG_MODE == 1)
        esmc_print_esmc_pdu(&msg, E_esmc_print_esmc_pdu_type_tx);
#endif
        os_mutex_unlock(&g_port_print_mutex);
      }
    } else {
      pr_err("Failed to compose ESMC PDU on port %s (port number: %d)", name, port_num);
      goto err;
    }
  }

  *thread_state = (*thread_state == E_port_thread_state_stopping) ? E_port_thread_state_stopped : *thread_state;

err:
  pthread_exit(NULL);
}

static void *port_rx_thread(void *arg)
{
  T_port_rx_thread_data *rx_thread_data = (T_port_rx_thread_data *)arg;
  T_port_cmn_thread_data *cmn_thread_data;
  volatile T_port_thread_state *thread_state;

  char name[PORT_MAX_NAME_LEN];
  int port_num;
  int fd;

  if(!rx_thread_data) {
    pr_err("No thread data");
    goto err;
  }

  cmn_thread_data = &rx_thread_data->cmn_thread_data;
  thread_state = &cmn_thread_data->thread_state;

  while(*thread_state != E_port_thread_state_starting) {
    usleep(20 * 1000);
  }

  *thread_state = E_port_thread_state_started;

  strncpy(name, cmn_thread_data->name, PORT_MAX_NAME_LEN);
  port_num = cmn_thread_data->port_num;
  fd = cmn_thread_data->fd;

  rx_thread_data->last_ql = E_esmc_ql_max;

  /* Initialize RX timeout monotonic time */
  rx_thread_data->rx_timeout_monotonic_time_ms = os_get_monotonic_milliseconds() + (ESMC_RX_TIMEOUT_PERIOD_S * 1000);
  rx_thread_data->rx_timeout_flag = 0;

  while(*thread_state == E_port_thread_state_started) {
    struct pollfd poll_fd;
    int ret;
    T_esmc_rx_event_cb_data cb_data;
    T_esmc_pdu msg;
    struct sockaddr_ll src_mac_addr;
    unsigned char originator_mac_addr[ETH_ALEN];
    int num_bytes_rx;

    int enhanced_flag;
    int ql_change_flag;
    int ext_ql_tlv_change_flag;

    T_esmc_ql parsed_ql;
    T_port_ext_ql_tlv_data parsed_ext_ql_tlv_data;

    usleep(ESMC_RX_HEARTBEAT_PERIOD_MS * 1000);

    memset(&src_mac_addr, 0, sizeof(src_mac_addr));

    memset(&poll_fd, 0, sizeof(poll_fd));
    poll_fd.fd = fd;
    poll_fd.events = POLLIN;
    
    ret = poll(&poll_fd, 1, 0);

    if((ret > 0) && (poll_fd.revents & POLLIN)) {
      num_bytes_rx = raw_socket_recv(fd, &msg, sizeof(msg), 0, &src_mac_addr, sizeof(src_mac_addr));
      if(cmn_thread_data->port_link_down_flag == 0) {
        if(num_bytes_rx >= ESMC_PDU_LEN) {
          if(esmc_parse_pdu(&msg, &enhanced_flag, &parsed_ql, &parsed_ext_ql_tlv_data) < 0) {
            /* ESMC RX event: invalid QL */
            pr_err("Failed to parse ESMC PDU on port %s (port number: %d)", name, port_num);

            memset(&cb_data, 0, sizeof(cb_data));
            cb_data.event_type = E_esmc_event_type_invalid_rx_ql;
            cb_data.port_num = port_num;

            esmc_call_rx_cb(&cb_data);
          } else {
            /* Check the source MAC address and the originator clock */
            if(esmc_adaptor_check_mac_addr(src_mac_addr.sll_addr) != 0) {
              /* ESMC RX event: immediate timing loop */
              memset(&cb_data, 0, sizeof(cb_data));
              cb_data.event_type = E_esmc_event_type_immediate_timing_loop;
              cb_data.port_num = port_num;
              cb_data.event_data.timing_loop.mac_addr = src_mac_addr.sll_addr;

              esmc_call_rx_cb(&cb_data);
            } else if(enhanced_flag) {
              extract_mac_addr(parsed_ext_ql_tlv_data.originator_clock_id, originator_mac_addr);
              if(esmc_adaptor_check_mac_addr(originator_mac_addr) != 0) {
                /* ESMC RX event: originator timing loop */
                memset(&cb_data, 0, sizeof(cb_data));
                cb_data.event_type = E_esmc_event_type_originator_timing_loop;
                cb_data.port_num = port_num;
                cb_data.event_data.timing_loop.mac_addr = originator_mac_addr;

                esmc_call_rx_cb(&cb_data);
              }
            }

            rx_thread_data->enhanced_flag = enhanced_flag;

            ql_change_flag = (parsed_ql != rx_thread_data->last_ql);

            if(ql_change_flag) {
              /* ESMC RX event: QL change */
              pr_info("QL changed to %s (%d) on port %s (port number: %d)",
                      conv_ql_enum_to_str(parsed_ql),
                      parsed_ql,
                      name,
                      port_num);

              memset(&cb_data, 0, sizeof(cb_data));
              cb_data.event_type = E_esmc_event_type_ql_change;
              cb_data.port_num = port_num;
              cb_data.event_data.ql_change.new_ql = parsed_ql;

              if(enhanced_flag) {
                /* Received extended QL TLV */

                /* memcmp() returns non-zero value if there is difference (i.e. change in extended QL TLV data) */
                ext_ql_tlv_change_flag = memcmp(&parsed_ext_ql_tlv_data, &cmn_thread_data->ext_ql_tlv, sizeof(parsed_ext_ql_tlv_data));

                if(ext_ql_tlv_change_flag != 0) {
                  /* Extended QL TLV change */
                  pr_info("Extended QL TLV data changed on port %s (port number: %d)",
                          name,
                          port_num);

                  cb_data.event_data.ql_change.new_num_cascaded_eEEC = parsed_ext_ql_tlv_data.num_cascaded_eEEC;
                  cb_data.event_data.ql_change.new_num_cascaded_EEC = parsed_ext_ql_tlv_data.num_cascaded_EEC;
                }

                /* Store parsed extended QL TLV data */
                memcpy(&cmn_thread_data->ext_ql_tlv, &parsed_ext_ql_tlv_data, sizeof(cmn_thread_data->ext_ql_tlv));
              }

              esmc_call_rx_cb(&cb_data);

              /* Store parsed QL */
              rx_thread_data->last_ql = parsed_ql;
            }

            if(enhanced_flag) {
              if(rx_thread_data->ext_ql_tlv_received_flag == 0) {
                rx_thread_data->ext_ql_tlv_received_flag = 1;
                pr_warning("Extended QL TLV appeared on port %s (port number: %d)", name, port_num);
              }
            } else if(rx_thread_data->ext_ql_tlv_received_flag == 1) {
              rx_thread_data->ext_ql_tlv_received_flag = 0;
              pr_warning("Extended QL TLV disappeared on port %s (port number: %d)", name, port_num);
            }

            /* Recalculate RX timeout monotonic time */
            rx_thread_data->rx_timeout_monotonic_time_ms = os_get_monotonic_milliseconds() + (ESMC_RX_TIMEOUT_PERIOD_S * 1000);
            rx_thread_data->rx_timeout_flag = 0;
            os_mutex_lock(&g_port_print_mutex);
            pr_debug(">>Received ESMC PDU with %s (%d) (extended QL TLV: %s) on port %s (port number: %d)<<",
                    conv_ql_enum_to_str(parsed_ql),
                    parsed_ql,
                    (enhanced_flag == 1) ? "yes" : "no",
                    name,
                    port_num);
  #if (SYNCED_DEBUG_MODE == 1)
            esmc_print_esmc_pdu(&msg, E_esmc_print_esmc_pdu_type_rx);
  #endif
            os_mutex_unlock(&g_port_print_mutex);
          }
        } else {
          pr_err("Invalid ESMC PDU length %d on port %s (port number: %d)", num_bytes_rx, name, port_num);
        }
      }
    } else if(ret < 0) {
      /* Timeout */
      pr_err("Failed to poll on port %s (port number: %d): %s", name, port_num, strerror(errno));
    } else if(poll_fd.revents & POLLERR) {
      /* Error occurred */
      pr_err("Detected poll error on port %s (port number: %d)", name, port_num);
    }

    if(port_check_link(fd, name) < 0) {
      if(cmn_thread_data->port_link_down_flag == 0) {
        /* ESMC RX event: port link down */
        cmn_thread_data->port_link_down_flag = 1;

        pr_warning("Link went down on port %s (port number: %d)", name, port_num);

        memset(&cb_data, 0, sizeof(cb_data));
        cb_data.event_type = E_esmc_event_type_port_link_down;
        cb_data.port_num = port_num;

        esmc_call_rx_cb(&cb_data);
        rx_thread_data->last_ql = E_esmc_ql_max;
      }
    } else if(cmn_thread_data->port_link_down_flag == 1) {
      /* ESMC RX event: port link up */
      cmn_thread_data->port_link_down_flag = 0;

      pr_info("Link is up on port %s (port number: %d)", name, port_num);

      memset(&cb_data, 0, sizeof(cb_data));
      cb_data.event_type = E_esmc_event_type_port_link_up;
      cb_data.port_num = port_num;

      esmc_call_rx_cb(&cb_data);
      rx_thread_data->last_ql = E_esmc_ql_max;
    }

    if(rx_thread_data->rx_timeout_flag == 0) {
      if(os_get_monotonic_milliseconds() > rx_thread_data->rx_timeout_monotonic_time_ms) {
        /* ESMC RX event: RX timeout */
        rx_thread_data->rx_timeout_flag = 1;

        pr_info("RX timeout occurred (QL not received within %d seconds period) on port %s (port number: %d)",
                ESMC_RX_TIMEOUT_PERIOD_S,
                name,
                port_num);

        memset(&cb_data, 0, sizeof(cb_data));
        cb_data.event_type = E_esmc_event_type_rx_timeout;
        cb_data.port_num = port_num;

        esmc_call_rx_cb(&cb_data);
        rx_thread_data->last_ql = E_esmc_ql_max;
      }
    }
  }

  *thread_state = (*thread_state == E_port_thread_state_stopping) ? E_port_thread_state_stopped : *thread_state;

err:
  pthread_exit(NULL);
}

/* Global functions */

T_port_tx_data *port_create_tx(T_tx_port_info const *tx_port)
{
  T_port_tx_data *tx_p;
  struct sockaddr_ll mac_addr;
  int fd;

  tx_p = calloc(1, sizeof(*tx_p));
  if(!tx_p) {
    return NULL;
  }

  memset(&mac_addr, 0, sizeof(mac_addr));
  memcpy(mac_addr.sll_addr, tx_port->mac_addr, ETH_ALEN);

  fd = port_open_tx(tx_port->name, tx_port->port_num, &mac_addr);
  if(fd == UNINITIALIZED_FD) {
    port_destroy_tx(tx_p);
    return NULL;
  }

  strncpy(tx_p->thread_data.cmn_thread_data.name, tx_port->name, PORT_MAX_NAME_LEN);
  tx_p->thread_data.cmn_thread_data.port_num = tx_port->port_num;
  memcpy(tx_p->thread_data.cmn_thread_data.mac_addr.sll_addr, mac_addr.sll_addr, ETH_ALEN);
  tx_p->thread_data.check_link_status = tx_port->check_link_status;
  tx_p->thread_data.cmn_thread_data.fd = fd;

  if(os_thread_create(&tx_p->thread_data.cmn_thread_data.thread_id, port_tx_thread, &tx_p->thread_data) < 0) {
    port_destroy_tx(tx_p);
    return NULL;
  }

  tx_p->state = E_port_state_created;

  if(port_thread_start_wait(E_port_thread_type_tx, &tx_p->thread_data.cmn_thread_data) < 0) {
    port_destroy_tx(tx_p);
    return NULL;
  }

  return tx_p;
}

T_port_rx_data *port_create_rx(T_rx_port_info const *rx_port)
{
  T_port_rx_data *rx_p;
  struct sockaddr_ll mac_addr;
  int fd;

  rx_p = calloc(1, sizeof(*rx_p));
  if(!rx_p) {
    return NULL;
  }

  memset(&mac_addr, 0, sizeof(mac_addr));
  memcpy(mac_addr.sll_addr, rx_port->mac_addr, ETH_ALEN);

  fd = port_open_rx(rx_port->name, rx_port->port_num, &mac_addr);
  if(fd == UNINITIALIZED_FD) {
    port_destroy_rx(rx_p);
    return NULL;
  }

  strncpy(rx_p->thread_data.cmn_thread_data.name, rx_port->name, PORT_MAX_NAME_LEN);
  rx_p->thread_data.cmn_thread_data.port_num = rx_port->port_num;
  memcpy(rx_p->thread_data.cmn_thread_data.mac_addr.sll_addr, mac_addr.sll_addr, ETH_ALEN);
  rx_p->thread_data.cmn_thread_data.fd = fd;

  if(os_thread_create(&rx_p->thread_data.cmn_thread_data.thread_id, port_rx_thread, &rx_p->thread_data) < 0) {
    port_destroy_rx(rx_p);
    return NULL;
  }

  rx_p->state = E_port_state_created;

  if(port_thread_start_wait(E_port_thread_type_rx, &rx_p->thread_data.cmn_thread_data) < 0) {
    port_destroy_rx(rx_p);
    return NULL;
  }

  return rx_p;
}

int port_init_tx(T_port_tx_data *tx_p)
{
  tx_p->state = E_port_state_initialized;

  tx_p->thread_data.cmn_thread_data.ext_ql_tlv.num_cascaded_eEEC = 1;
  tx_p->thread_data.cmn_thread_data.ext_ql_tlv.num_cascaded_EEC = 0;

  return 0;
}

int port_init_rx(T_port_rx_data *rx_p)
{
  rx_p->state = E_port_state_initialized;

  rx_p->thread_data.cmn_thread_data.ext_ql_tlv.num_cascaded_eEEC = 1;
  rx_p->thread_data.cmn_thread_data.ext_ql_tlv.num_cascaded_EEC = 0;

  return 0;
}

int port_check_tx(T_port_tx_data *tx_p)
{
  if(tx_p->state < E_port_state_initialized) {
    goto err;
  }

  if(tx_p->thread_data.cmn_thread_data.thread_state < E_port_thread_state_started) {
    goto err;
  }

  return 0;

err:
  tx_p->state = E_port_state_failed;
  tx_p->thread_data.cmn_thread_data.thread_state = E_port_thread_state_failed;
  return -1;
}

int port_check_rx(T_port_rx_data *rx_p)
{
  if(rx_p->state < E_port_state_initialized) {
    goto err;
  }

  if(rx_p->thread_data.cmn_thread_data.thread_state < E_port_thread_state_started) {
    goto err;
  }

  return 0;

err:
  rx_p->state = E_port_state_failed;
  rx_p->thread_data.cmn_thread_data.thread_state = E_port_thread_state_failed;
  return -1;
}

int port_get_rx_ext_ql_tlv_data(T_port_rx_data *rx_p, T_port_num best_port_num, T_port_ext_ql_tlv_data *best_ext_ql_tlv_data)
{
  T_port_cmn_thread_data *cmn_thread_data = &rx_p->thread_data.cmn_thread_data;

  if(best_port_num == cmn_thread_data->port_num) {
    if(rx_p->thread_data.ext_ql_tlv_received_flag == 0) {
      best_ext_ql_tlv_data->num_cascaded_eEEC = 0;
      best_ext_ql_tlv_data->num_cascaded_EEC = 1;
      best_ext_ql_tlv_data->mixed_EEC_eEEC = 1;
      best_ext_ql_tlv_data->partial_chain = 1;

      memset(best_ext_ql_tlv_data->originator_clock_id, 0, ESMC_PDU_EXT_QL_TLV_SYNCE_CLOCK_ID_LEN);
    } else {
      best_ext_ql_tlv_data->num_cascaded_eEEC = cmn_thread_data->ext_ql_tlv.num_cascaded_eEEC;
      best_ext_ql_tlv_data->num_cascaded_EEC = cmn_thread_data->ext_ql_tlv.num_cascaded_EEC;
      best_ext_ql_tlv_data->mixed_EEC_eEEC = cmn_thread_data->ext_ql_tlv.mixed_EEC_eEEC;
      best_ext_ql_tlv_data->partial_chain = cmn_thread_data->ext_ql_tlv.partial_chain;

      memcpy(best_ext_ql_tlv_data->originator_clock_id, cmn_thread_data->ext_ql_tlv.originator_clock_id, ESMC_PDU_EXT_QL_TLV_SYNCE_CLOCK_ID_LEN);
    }

    best_ext_ql_tlv_data->num_cascaded_eEEC++;

    return 1;
  }

  return 0;
}

void port_destroy_tx(T_port_tx_data *tx_p)
{
  port_thread_stop_wait(&tx_p->thread_data.cmn_thread_data);
  port_close_tx(tx_p->thread_data.cmn_thread_data.fd);

  tx_p->state = E_port_state_unknown;
  tx_p->thread_data.cmn_thread_data.fd = UNINITIALIZED_FD;
}

void port_destroy_rx(T_port_rx_data *rx_p)
{
  port_thread_stop_wait(&rx_p->thread_data.cmn_thread_data);
  port_close_rx(rx_p->thread_data.cmn_thread_data.fd);

  rx_p->state = E_port_state_unknown;
  rx_p->thread_data.cmn_thread_data.fd = UNINITIALIZED_FD;
}
