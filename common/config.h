/**
 * @file config.h
 * @brief Configuration file code
 * @note Copyright (C) 2011 Richard Cochran <richardcochran@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/********************************************************************************************************************
* Release Tag: 2-0-5
* Pipeline ID: 310964
* Commit Hash: b166f770
********************************************************************************************************************/

#ifndef CONFIG_H
#define CONFIG_H

#include <getopt.h>
#include <sys/queue.h>
#include <stddef.h>

#include "interface.h"
#include "types.h"

struct config {
  /* Interfaces (ports) configured via configuration file or command-line interface */
  STAILQ_HEAD(interfaces_head, interface) interfaces;
  int n_interfaces;

  /* Command-line interface */
  struct option *opts;

  struct hash *config_hash_table;
};

enum config_parser_result {
  PARSED_OK,
  NOT_PARSED,
  BAD_VALUE,
  MALFORMED,
  OUT_OF_RANGE,
};

/* General configuration functions */
int config_read(const char *name, struct config *cfg, char **dump_buf, long *dump_buf_size);
struct interface *config_create_interface(const char *name, struct config *cfg);
void config_destroy(struct config *cfg);

/* Hash table-related configuration functions */
struct config *config_create(void);
double config_get_double(struct config *cfg, const char *section, const char *option);
int config_get_int(struct config *cfg, const char *section, const char *option);
char *config_get_string(struct config *cfg, const char *section, const char *option);
int config_set_double(struct config *cfg, const char *option, double val);
int config_set_section_int(struct config *cfg, const char *section, const char *option, int val);
static inline int config_set_int(struct config *cfg, const char *option, int val) {
  return config_set_section_int(cfg, NULL, option, val);
}
int config_set_string(struct config *cfg, const char *option, const char *val);

/* Command-line interface-related configuration functions */
static inline struct option *config_long_options(struct config *cfg) {
  return cfg->opts;
}
int config_parse_option(struct config *cfg, const char *opt, const char *val);

/* ESMC-related configuration functions */
int config_ql_str_to_enum_conv(T_esmc_network_option net_opt, const char *ql_str, T_esmc_ql *ql);

enum config_parser_result config_get_ranged_int(const char *str_val, int *result, int min, int max);
int config_get_arg_val_i(int op, const char *optarg, int *val, int min, int max);

#endif /* CONFIG_H */
