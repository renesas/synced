/**
 * @file synced.c
 * @brief synced main program
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
#include <getopt.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "common/config.h"
#include "common/interface.h"
#include "common/missing.h"
#include "common/os.h"
#include "common/print.h"
#include "control/sync.h"
#include "control/control.h"
#include "device/device_adaptor/device_adaptor.h"
#include "esmc/esmc_adaptor/esmc_adaptor.h"
#include "management/management.h"
#include "management/mng_if.h"
#include "management/pcm4l_if.h"
#include "monitor/monitor.h"

#ifndef __linux__
#error __linux__ is not defined!
#endif /* __linux__ */

#define VERSION_ID    "2.0.5"
#define PIPELINE_ID   "310964"
#define COMMIT_ID     "b166f770"

#define MAIN_LOOP_INTERVAL_MS   100

struct interface {
  STAILQ_ENTRY(interface) list;
};

/* Static data */

static const char *g_version = VERSION_ID;
static const char *g_pipeline = PIPELINE_ID;
static const char *g_commit = COMMIT_ID;

static int g_prog_running = 1;

/* Static functions */

static void usage(char *prog_name)
{
  fprintf(stderr,
          "usage: %s [options]\n"
          "options:\n"
          "  -1 Configure ESMC to handle network option 1 clocks.\n"
          "  -2 Configure ESMC to handle network option 2 clocks.\n"
          "  -3 Configure ESMC to handle network option 3 clocks.\n"
          "  -f [file] Read configuration from 'file'.\n"
          "  -h Display command-line options (i.e. print this message).\n"
          "  -l [level] Set maximum message level to 'level'.\n"
          "  -o Enable printing of messages to stdout.\n"
          "  -s Enable logging of messages to syslog.\n"
          "  -t [tag] Set message tag to 'tag'.\n"
          "  -v Display software version.\n"
          "  --option [value] Set 'option' to 'value' (e.g. --net_opt 1). Applicable to only global options.\n",
          prog_name);
}

static void sig_handler(int sig_num)
{
  (void)sig_num;

  g_prog_running = 0;
}

static int set_sig_action(int sig, const struct sigaction *new_action)
{
  struct sigaction old_action;

  sigaction(sig, NULL, &old_action);

  if(SIG_IGN != old_action.sa_handler) {
    if(sigaction(sig, new_action, NULL) < 0) {
      fprintf(stderr, "%s: %s", __func__, strerror(errno));
      return -1;
    }
  }

  return 0;
}

static int register_all_prog_term_sig_handlers(void)
{
  struct sigaction new_action;

  new_action.sa_handler = sig_handler;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;

  if(set_sig_action(SIGHUP, &new_action) < 0) {
    return -1;
  }
  if(set_sig_action(SIGINT, &new_action) < 0) {
    return -1;
  }
  if(set_sig_action(SIGQUIT, &new_action) < 0) {
    return -1;
  }
  if(set_sig_action(SIGTERM, &new_action) < 0) {
    return -1;
  }

  return 0;
}

static int get_port_info(struct config *cfg, T_esmc_network_option net_opt, int no_ql_en, int *num_tx_ports, int *num_rx_ports, int *num_syncs)
{
  struct interface *iface;
  const char *port_name;
  T_sync_type sync_type;
  int tx_en;
  int rx_en;
  int tx_bundle_num;
  int clk_idx;
  int pri;
  struct sockaddr_ll mac_addr;
  char mac_addr_str[MAX_MAC_ADDR_STR_LEN];
  const char *ql_str;
  T_esmc_ql ql;

  STAILQ_FOREACH(iface, &cfg->interfaces, list) {
    port_name = interface_get_name(iface);

    /* Initialize sync type */
    sync_type = E_sync_type_max;

    tx_en = config_get_int(cfg, port_name, "tx_en");
    if(tx_en)
      (*num_tx_ports)++;
    interface_config_tx_en(iface, tx_en);

    rx_en = config_get_int(cfg, port_name, "rx_en");
    if(rx_en)
      (*num_rx_ports)++;
    interface_config_rx_en(iface, rx_en);

    tx_bundle_num = config_get_int(cfg, port_name, "tx_bundle_num");
    interface_config_tx_bundle_num(iface, tx_bundle_num);

    clk_idx = config_get_int(cfg, port_name, "clk_idx");
    interface_config_clk_idx(iface, clk_idx);

    pri = config_get_int(cfg, port_name, "pri");
    if(pri == NO_PRI) {
      /* Priority was not configured */
      if((!tx_en && !rx_en) || rx_en) {
        pr_err("%s must have priority", port_name);
        return -1;
      }
    } else {
      /* Priority was configured */
      if(tx_en && !rx_en) {
        pr_err("Sync-E TX only port %s does not support priority", port_name);
        return -1;
      }
    }
    interface_config_pri(iface, pri);

    if(tx_en || rx_en) {
      if((tx_bundle_num != NO_TX_BUNDLE_NUM) && !tx_en) {
        pr_err("TX bundle number %d for %s not supported because TX disabled", tx_bundle_num, port_name);
        return -1;
      }

      if(interface_config_idx_and_mac_addr(iface) < 0) {
        pr_err("Failed to get index and MAC address for port %s", port_name);
        return -1;
      }
      mac_addr = interface_get_mac_addr(iface);
      mac_addr_arr_to_str(&mac_addr, mac_addr_str);
      if(clk_idx == MISSING_CLK_IDX) {
        if(!rx_en) {
          /* Sync-E TX only port */
          sync_type = E_sync_type_tx_only;
          pr_info("%s (%s) is Sync-E TX only port", port_name, mac_addr_str);
        } else {
        /* Sync-E monitoring port */
          sync_type = E_sync_type_monitoring;
          pr_info("%s (%s) is Sync-E monitoring port", port_name, mac_addr_str);
        }
      } else if(!rx_en) {
        pr_err("Sync-E TX only port %s does not support clock index", port_name);
        return -1;
      } else {
        /* Sync-E clock port */
        sync_type = E_sync_type_synce;
        pr_info("%s (%s) is Sync-E clock port", port_name, mac_addr_str);
      }
    } else {
      /* External clock port */
      if(tx_bundle_num != NO_TX_BUNDLE_NUM) {
        pr_err("External clock port %s does not support TX bundle number", port_name);
        return -1;
      }
      if(clk_idx == MISSING_CLK_IDX) {
        pr_err("External clock port %s must have clock index", port_name);
        return -1;
      }
      sync_type = E_sync_type_external;
      pr_info("%s is an external clock port", port_name);
    }

    interface_config_type(iface, sync_type);

    ql_str = config_get_string(cfg, port_name, "init_ql");

    if(sync_type == E_sync_type_tx_only) {
      if(strcmp(ql_str, "FAILED")) {
        pr_err("Port %s does not support initial QL", port_name);
        return -1;
      }
    } else if(no_ql_en) {
      if(strcmp(ql_str, "FAILED")) {
        pr_err("Port %s does not support initial QL because no QL mode is enabled", port_name);
        return -1;
      }
    } else {
      if(!strcmp(ql_str, "FAILED")) {
        pr_warning("Port %s initial QL set to QL-FAILED", port_name);
      }
    }

    if(config_ql_str_to_enum_conv(net_opt, ql_str, &ql) < 0) {
      pr_err("Port %s initial QL (QL-%s) incompatible with network option %d", port_name, ql_str, net_opt);
      return -1;
    }
    interface_config_ql(iface, ql);

    (*num_syncs)++;
  }

  return 0;
}

static int create_esmc_config(struct config *cfg, T_esmc_network_option net_opt, T_tx_port_info const *tx_port, T_rx_port_info const *rx_port, int num_tx_ports, int num_rx_ports, T_esmc_config *esmc_config)
{
  const char *lo_ql_str;
  T_esmc_ql lo_ql;
  T_esmc_ql do_not_use_ql;

  esmc_config->net_opt = net_opt;
  pr_info("Set ESMC network option to %d", net_opt);

  lo_ql_str = config_get_string(cfg, "global", "lo_ql");
  if(!strcmp(lo_ql_str, "FAILED")) {
    pr_err("LO QL not specified");
    return -1;
  }
  if(config_ql_str_to_enum_conv(net_opt, lo_ql_str, &lo_ql) < 0) {
    pr_err("LO QL (QL-%s) incompatible with network option %d", lo_ql_str, net_opt);
    return -1;
  }
  if(conv_net_opt_to_do_not_use_ql(net_opt, &do_not_use_ql) < 0) {
    return -1;
  }
  if(lo_ql > do_not_use_ql) {
    lo_ql = do_not_use_ql;
  }
  esmc_config->init_ql = lo_ql;
  esmc_config->do_not_use_ql = do_not_use_ql;
  pr_info("Set LO QL to QL-%s (%d)", lo_ql_str, lo_ql);

  esmc_config->tx_port_array = tx_port;
  esmc_config->rx_port_array = rx_port;
  esmc_config->num_tx_ports = num_tx_ports;
  esmc_config->num_rx_ports = num_rx_ports;

  return 0;
}

static int create_device_config(struct config *cfg, T_device_config *device_config)
{
  device_config->device_cfg_file = config_get_string(cfg, "global", "device_cfg_file");
  device_config->device_name = config_get_string(cfg, "global", "device_name");

  device_config->synce_dpll_idx = config_get_int(cfg, "global", "synce_dpll_idx");
  pr_info("Set Sync-E DPLL index to %d", device_config->synce_dpll_idx);

  return 0;
}

static int create_control_config(struct config *cfg, T_esmc_network_option net_opt, int no_ql_en, T_esmc_ql lo_ql, T_esmc_ql do_not_use_ql, int num_syncs, T_sync_config const *sync_config, T_control_config *control_config)
{
  control_config->net_opt = net_opt;
  control_config->no_ql_en = no_ql_en;

  control_config->synce_forced_ql_en = config_get_int(cfg, "global", "synce_forced_ql_en");
  pr_info("Forced QL for Sync-E ports is %s", control_config->synce_forced_ql_en ? "enabled" : "disabled");

  control_config->lo_ql = lo_ql;

  control_config->lo_pri = config_get_int(cfg, "global", "lo_pri");
  pr_info("Set LO priority to %d", control_config->lo_pri);

  control_config->do_not_use_ql = do_not_use_ql;

  control_config->hold_off_timer_ms = config_get_int(cfg, "global", "hoff_tmr");
  pr_info("Set hold-off timer to %u milliseconds", control_config->hold_off_timer_ms);

  control_config->wait_to_restore_timer_s = config_get_int(cfg, "global", "wtr_tmr");
  pr_info("Set wait-to-restore timer to %u seconds", control_config->wait_to_restore_timer_s);

  control_config->num_syncs = num_syncs;
  pr_debug("Set number of syncs to %d", control_config->num_syncs);

  control_config->sync_config_array = sync_config;

  return 0;
}

static int create_monitor_config(struct config *cfg, T_esmc_network_option net_opt, T_esmc_ql lo_ql, T_monitor_config *monitor_config)
{
  const char *holdover_ql_str;
  T_esmc_ql holdover_ql;

  monitor_config->lo_ql = lo_ql;

  holdover_ql_str = config_get_string(cfg, "global", "holdover_ql");
  if(!strcmp(holdover_ql_str, "FAILED")) {
    pr_err("Holdover QL not specified");
    return -1;
  }
  if(config_ql_str_to_enum_conv(net_opt, holdover_ql_str, &holdover_ql) < 0) {
    pr_err("Holdover QL (QL-%s) incompatible with network option %d", holdover_ql_str, net_opt);
    return -1;
  }
  if(holdover_ql > lo_ql) {
    pr_err("Holdover QL (QL-%s) must equal to or better than LO QL (%s)", holdover_ql_str, conv_ql_enum_to_str(lo_ql));
    return -1;
  }
  monitor_config->holdover_ql = holdover_ql;
  pr_info("Set holdover QL to QL-%s (%d)", holdover_ql_str, holdover_ql);

  monitor_config->holdover_timer_s = config_get_int(cfg, "global", "holdover_tmr");
  pr_info("Set holdover timer to %u seconds", monitor_config->holdover_timer_s);

  monitor_config->advanced_holdover_en = config_get_int(cfg, "global", "advanced_holdover_en");
  pr_info("Set advanced holdover enable to %u", monitor_config->advanced_holdover_en);

  return 0;
}

int dump_cfg_file(char *cfg_file_dump, long cfg_file_dump_size)
{
  char *dump_start;
  char *current_character;
  char *line_start;
  char *line_end;

  if(cfg_file_dump != NULL) {
    dump_start = cfg_file_dump;
    current_character = cfg_file_dump;
    line_start = cfg_file_dump;
    line_end = NULL;

    while(current_character - dump_start != cfg_file_dump_size) {
      if(*current_character == '\n') {
        line_end = current_character;
        pr_info_dump("%.*s", (int)(line_end - line_start), line_start);
        line_start = current_character;
      }
      current_character++;
    }
    pr_info_dump("\n");

    return 0;
  }

  return -1;
}

/* Global functions */

int main(int argc, char *argv[])
{
  int err = -1;

  char *prog_name = strrchr(argv[0], '/');

  /* Command-line interface */
  int c;
  int idx;

  /* Configuration interface */
  char *cfg_file_name = NULL;
  struct option *opts;
  struct config *cfg;
  char *cfg_file_dump = NULL;
  long cfg_file_dump_size = 0;

  /* Message logging */
  int print_level;

  int num_tx_ports = 0;
  int num_rx_ports = 0;
  int num_syncs = 0;

  T_tx_port_info *tx_port = NULL;
  T_tx_port_info *init_tx_port = NULL;
  T_rx_port_info *rx_port = NULL;
  T_rx_port_info *init_rx_port = NULL;

  T_esmc_network_option net_opt;
  int no_ql_en;

  struct interface *iface = NULL;
  const char *name = NULL;
  struct sockaddr_ll mac_addr;
  int clk_idx;
  int pri;
  int tx_en;
  int rx_en;
  int tx_bundle_num;
  T_esmc_ql init_ql;
  T_sync_type sync_type;

  int sync_idx = INVALID_SYNC_IDX;
  int sync_counter = 0;

  T_sync_config *sync_config = NULL;
  T_sync_config *init_sync_config = NULL;

  T_esmc_config esmc_config;
  T_device_config device_config;
  T_control_config control_config;
  T_monitor_config monitor_config;

  int device_adaptor_init_flag = 0;
  int esmc_init_flag = 0;
  int control_init_flag = 0;
  int monitor_init_flag = 0;
  int management_init_flag = 0;
  int pcm4l_if_en = 0;
  int mng_if_en = 0;

  if(prog_name)
    prog_name++;
  else
    prog_name = argv[0];

/* Debug */
#if (SYNCED_DEBUG_MODE == 1)
  printf("Enabled debug mode\n");
#endif

  /* Allocate memory for and initialize configuration */
  cfg = config_create();
  if(!cfg) {
    fprintf(stderr, "Failed to allocate memory for configuration\n");
    return -1;
  }

  opts = config_long_options(cfg);
  while(EOF != (c = getopt_long(argc, argv, "123f:hi:l:ost:v", opts, &idx))) {
    switch(c) {
      case 0:
        if(config_parse_option(cfg, opts[idx].name, optarg) < 0)
          goto quick_end;
        printf("Set %s to %s through command-line interface\n", opts[idx].name, optarg);
        break;
      case '1':
        if(config_set_int(cfg, "net_opt", E_esmc_network_option_1) < 0)
          goto quick_end;
        printf("Set %s to %d through command-line interface\n", "net_opt", E_esmc_network_option_1);
        break;
      case '2':
        if(config_set_int(cfg, "net_opt", E_esmc_network_option_2) < 0)
          goto quick_end;
        printf("Set %s to %d through command-line interface\n", "net_opt", E_esmc_network_option_2);
        break;
      case '3':
        if(config_set_int(cfg, "net_opt", E_esmc_network_option_3) < 0)
          goto quick_end;
        printf("Set %s to %d through command-line interface\n", "net_opt", E_esmc_network_option_3);
        break;
      case 'f':
        cfg_file_name = optarg;
        break;
      case 'h':
        usage(prog_name);
        goto quick_end;
        break;
      case 'l':
        if(config_get_arg_val_i(c, optarg, &print_level, PRINT_LEVEL_MIN, PRINT_LEVEL_MAX))
          goto quick_end;
        if(config_set_int(cfg, "max_msg_lvl", print_level) < 0)
          goto quick_end;
        printf("Set %s to %d through command-line interface\n", "max_msg_lvl", print_level);
        break;
      case 'o':
        if(config_set_int(cfg, "stdout_en", 1) < 0)
          goto quick_end;
        printf("Set %s to %d through command-line interface\n", "stdout_en", 1);
        break;
      case 's':
        if(config_set_int(cfg, "syslog_en", 1) < 0)
          goto quick_end;
        printf("Set %s to %d through command-line interface\n", "syslog_en", 1);
        break;
      case 't':
        if(config_set_string(cfg, "msg_tag", optarg) < 0)
          goto quick_end;
        printf("Set %s to %s through command-line interface\n", "msg_tag", optarg);
        break;
      case 'v':
        printf("%s version: %s.%s.%s\n", prog_name, g_version, g_pipeline, g_commit);
        goto quick_end;
      case '?':
        usage(prog_name);
        goto quick_end;
      case ':':
        usage(prog_name);
        goto quick_end;
      default:
        usage(prog_name);
        goto quick_end;
    }
  }

  /* If specified, read and parse configuration file */
  if((cfg_file_name != NULL) && (config_read(cfg_file_name, cfg, &cfg_file_dump, &cfg_file_dump_size) < 0)) {
    fprintf(stderr, "Failed to read %s\n", cfg_file_name);
    goto quick_end;
  }

  /* End application because no ports were specified in configuration */
  if(STAILQ_EMPTY(&cfg->interfaces)) {
    fprintf(stderr, "No ports specified\n");
    usage(prog_name);
    goto quick_end;
  }

  /* Set stdout-and-syslog-related parameters */
  print_set_prog_name(prog_name);
  print_set_msg_tag(config_get_string(cfg, "global", "msg_tag"));
  print_set_max_msg_level(config_get_int(cfg, "global", "max_msg_lvl"));
  print_set_stdout_en(config_get_int(cfg, "global", "stdout_en"));
  print_set_syslog_en(config_get_int(cfg, "global", "syslog_en"));

  /* Register signal handlers used to end application */
  if(register_all_prog_term_sig_handlers() < 0) {
    pr_err("Failed to register program termination signal handlers");
    goto quick_end;
  }

  pr_info("---Started %s (version: %s.%s.%s %s %s)---", prog_name, g_version, g_pipeline, g_commit, __DATE__, __TIME__);

  pr_info("Opened configuration file %s:", cfg_file_name);
  if(dump_cfg_file(cfg_file_dump, cfg_file_dump_size) < 0) {
    pr_err("Failed to dump configuration file");
    free(cfg_file_dump);
    goto quick_end;
  }
  free(cfg_file_dump);

  net_opt = config_get_int(cfg, "global", "net_opt");
  no_ql_en = config_get_int(cfg, "global", "no_ql_en");
  pr_info("No QL mode is %s", no_ql_en ? "enabled" : "disabled");

  /* Get port information */
  if(get_port_info(cfg, net_opt, no_ql_en, &num_tx_ports, &num_rx_ports, &num_syncs) < 0) {
    pr_err("Failed to store port configuration (check configuration file)");
    goto end;
  }

  init_tx_port = calloc(num_tx_ports, sizeof(*init_tx_port));
  if(!init_tx_port) {
    pr_err("Failed to allocate memory for TX port configuration");
    goto end;
  }
  tx_port = init_tx_port;

  init_rx_port = calloc(num_rx_ports, sizeof(*init_rx_port));
  if(!init_rx_port) {
    pr_err("Failed to allocate memory for RX port configuration");
    goto end;
  }
  rx_port = init_rx_port;

  init_sync_config = calloc(num_syncs, sizeof(*init_sync_config));
  if(!init_sync_config) {
    pr_err("Failed to allocate memory for sync configuration");
    goto end;
  }
  sync_config = init_sync_config;

  STAILQ_FOREACH(iface, &cfg->interfaces, list) {
    name = interface_get_name(iface);
    clk_idx = interface_get_clk_idx(iface);
    pri = interface_get_pri(iface);
    tx_en = interface_get_tx_en(iface);
    rx_en = interface_get_rx_en(iface);
    tx_bundle_num = interface_get_tx_bundle_num(iface);
    init_ql = interface_get_ql(iface);

    sync_type = interface_get_type(iface);
    sync_idx = sync_counter;

    if(tx_en == 1) {
      tx_port->name = name;
      tx_port->port_num = interface_get_idx(iface);
      mac_addr = interface_get_mac_addr(iface);
      memcpy(tx_port->mac_addr, mac_addr.sll_addr, ETH_ALEN);
      tx_port->sync_idx = sync_idx;
      tx_port->check_link_status = (interface_get_type(iface) == E_sync_type_tx_only) ? 1 : 0;
      tx_port++;
    }

    if(rx_en == 1) {
      rx_port->name = name;
      rx_port->port_num = interface_get_idx(iface);
      mac_addr = interface_get_mac_addr(iface);
      memcpy(rx_port->mac_addr, mac_addr.sll_addr, ETH_ALEN);
      rx_port->sync_idx = sync_idx;
      rx_port++;
    }

    /* Set sync configuration */
    sync_config->name = name;
    sync_config->type = sync_type;
    sync_config->clk_idx = clk_idx;
    sync_config->config_pri = pri;
    sync_config->init_ql = init_ql;
    sync_config->tx_bundle_num = tx_bundle_num;

    sync_counter++;
    sync_config++;
  }

  /* Create ESMC configuration */
  if(create_esmc_config(cfg, net_opt, init_tx_port, init_rx_port, num_tx_ports, num_rx_ports, &esmc_config) < 0) {
    pr_err("Failed to create ESMC configuration");
    goto end;
  }
  pr_info("Created ESMC configuration");

  /* Create device configuration */
  if(create_device_config(cfg, &device_config) < 0) {
    pr_err("Failed to create device configuration");
    goto end;
  }
  pr_info("Created device configuration");

  /* Create control configuration */
  if(create_control_config(cfg, net_opt, no_ql_en, esmc_config.init_ql, esmc_config.do_not_use_ql, num_syncs, init_sync_config, &control_config) < 0) {
    pr_err("Failed to create control configuration");
    goto end;
  }
  pr_info("Created control configuration");

  /* Create monitor configuration */
  if(create_monitor_config(cfg, net_opt, esmc_config.init_ql, &monitor_config) < 0) {
    pr_err("Failed to create monitor configuration");
    goto end;
  }
  pr_info("Created monitor configuration");

  /* Initialize management, device, ESMC stack, control, and monitor */
  if(management_init() == 0) {
    pr_info("Initialized management");
    management_init_flag = 1;
  } else {
    pr_err("Failed to initialize management");
    goto end;
  }
  if(device_adaptor_init(&device_config) == 0) {
    pr_info("Initialized Sync-E DPLL");
    device_adaptor_init_flag = 1;
  } else {
    pr_err("Failed to initialize Sync-E DPLL");
    goto end;
  }
  if(control_init(&control_config) == 0) {
    pr_info("Initialized control");
    control_init_flag = 1;
  } else {
    pr_err("Failed to initialize control");
    goto end;
  }
  if(monitor_init(&monitor_config) == 0) {
    pr_info("Initialized Sync-E DPLL monitor");
    monitor_init_flag = 1;
  } else {
    pr_err("Failed to initialize Sync-E DPLL monitor");
    goto end;
  }
  if(esmc_adaptor_init(&esmc_config) == 0) {
    pr_info("Initialized ESMC");
    esmc_init_flag = 1;
  } else {
    pr_err("Failed to initialize ESMC");
    goto end;
  }

  /* Start ESMC stack */
  if(esmc_adaptor_start() < 0) {
    pr_err("Failed to start the ESMC stack");
    goto end;
  }

  /* Start pcm4l interface */
  pcm4l_if_en = config_get_int(cfg, "global", "pcm4l_if_en");
  if(pcm4l_if_en == 1) {
    if(pcm4l_if_start(config_get_string(cfg, "global", "pcm4l_if_ip_addr"),
                      config_get_int(cfg, "global", "pcm4l_if_port_num")) < 0) {
      pr_err("Failed to start the pcm4l interface");
      goto end;
    }
  }

  /* Start management interface */
  mng_if_en = config_get_int(cfg, "global", "mng_if_en");
  if(mng_if_en == 1) {
    if(mng_if_start(config_get_string(cfg, "global", "mng_if_ip_addr"),
                    config_get_int(cfg, "global", "mng_if_port_num")) < 0) {
      pr_err("Failed to start the management interface");
      goto end;
    }
  }

  err = 0;

  while(g_prog_running) {
    /* Run Sync-E DPLL monitor to retrieve current QL and clock index */
    monitor_determine_ql();
    /* Run control state machine and update device reference priority table */
    control_update_sync_table();
    /* Wait */
    usleep(MAIN_LOOP_INTERVAL_MS * 1000);
  }

  /* Stop ESMC stack */
  esmc_adaptor_stop();

end:
  /* Stop the management interface */
  mng_if_stop();

  /* Stop the pcm4l interface */
  pcm4l_if_stop();

  /* Deinitialize management, monitor, control, ESMC stack, and device */
  if(management_init_flag) {
    management_deinit();
  }
  if(esmc_init_flag) {
    esmc_adaptor_deinit();
  }
  if(monitor_init_flag) {
    monitor_deinit();
  }
  if(control_init_flag) {
    control_deinit();
  }
  if(device_adaptor_init_flag) {
    device_adaptor_deinit();
  }

  /* Free sync and TX/RX port configurations */
  if(init_sync_config) {
    free(init_sync_config);
    init_sync_config = NULL;
  }
  if(init_rx_port) {
    free(init_rx_port);
    init_rx_port = NULL;
  }
  if(init_tx_port) {
    free(init_tx_port);
    init_tx_port = NULL;
  }

quick_end:

  /* Destroy configuration */
  if(cfg) {
    config_destroy(cfg);
  }

  pr_info("---Ended %s---", prog_name);

  return err;
}
