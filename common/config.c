/**
 * @file config.c
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

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "config.h"
#include "hash.h"
#include "print.h"

#define CONFIG_LABEL_SIZE   32

#define CFG_ITEM_STATIC   (1 << 0) /* Statically allocated (not to be freed) */
#define CFG_ITEM_LOCKED   (1 << 1) /* Command-line argument (cannot be changed) */
#define CFG_ITEM_PORT     (1 << 2) /* Interface (port) configuration */
#define CFG_ITEM_DYNSTR   (1 << 4) /* String value (dynamically allocated) */

#define N_CONFIG_ITEMS   (sizeof(g_config_table) / sizeof(g_config_table[0]))

#define CONFIG_ITEM_DBL(_label, _port, _default, _min, _max) { \
  .label = _label, \
  .type = CFG_TYPE_DOUBLE, \
  .flags = _port ? CFG_ITEM_PORT : 0, \
  .val.d = _default, \
  .min.d = _min, \
  .max.d = _max, \
}
#define CONFIG_ITEM_ENUM(_label, _port, _default, _table) { \
  .label = _label, \
  .type = CFG_TYPE_ENUM, \
  .flags = _port ? CFG_ITEM_PORT : 0, \
  .tab = _table, \
  .val.i = _default, \
}
#define CONFIG_ITEM_INT(_label, _port, _default, _min, _max) { \
  .label = _label, \
  .type = CFG_TYPE_INT, \
  .flags = _port ? CFG_ITEM_PORT : 0, \
  .val.i = _default, \
  .min.i = _min, \
  .max.i = _max, \
}
#define CONFIG_ITEM_STRING(_label, _port, _default) { \
  .label = _label, \
  .type = CFG_TYPE_STRING, \
  .flags = _port ? CFG_ITEM_PORT : 0, \
  .val.s = _default, \
}

#define GLOB_ITEM_DBL(label, _default, min, max) \
  CONFIG_ITEM_DBL(label, 0, _default, min, max)
#define GLOB_ITEM_ENU(label, _default, table) \
  CONFIG_ITEM_ENUM(label, 0, _default, table)
#define GLOB_ITEM_INT(label, _default, min, max) \
  CONFIG_ITEM_INT(label, 0, _default, min, max)
#define GLOB_ITEM_STR(label, _default) \
  CONFIG_ITEM_STRING(label, 0, _default)
#define PORT_ITEM_DBL(label, _default, min, max) \
  CONFIG_ITEM_DBL(label, 1, _default, min, max)
#define PORT_ITEM_ENU(label, _default, table) \
  CONFIG_ITEM_ENUM(label, 1, _default, table)
#define PORT_ITEM_INT(label, _default, min, max) \
  CONFIG_ITEM_INT(label, 1, _default, min, max)
#define PORT_ITEM_STR(label, _default) \
  CONFIG_ITEM_STRING(label, 1, _default)

enum config_section {
  GLOBAL_SECTION,
  PORT_SECTION,
  UNKNOWN_SECTION,
};

enum config_type {
  CFG_TYPE_INT,
  CFG_TYPE_DOUBLE,
  CFG_TYPE_ENUM,
  CFG_TYPE_STRING,
};

struct config_enum {
  const char *label;
  int value;
};

typedef union {
  int i;
  double d;
  char *s;
} any;

struct config_item {
  char label[CONFIG_LABEL_SIZE];
  enum config_type type;
  struct config_enum *tab;
  unsigned int flags;
  any val;
  any min;
  any max;
};

struct interface {
  STAILQ_ENTRY(interface) list;
};

struct config_esmc_ql {
  char label[10];
  T_esmc_ql ql;
};

/* Static data */

static char g_config_port_name[MAX_NUM_OF_SYNC_ENTRIES][INTERFACE_MAX_NAME_LEN];
static int g_config_clk_idx_flag[MAX_NUM_OF_CLOCKS] = {0};
static int g_config_pri_flag[MAX_NUM_OF_PRIORITIES] = {0};

static struct config_enum g_config_esmc_net_opt_enu[] = {
  {"1", E_esmc_network_option_1},
  {"2", E_esmc_network_option_2},
  {"3", E_esmc_network_option_3},
  {NULL, E_esmc_network_option_1},
};

static struct config_esmc_ql g_config_esmc_net_opt_1_ql[] = {
  {"ePRTC", E_esmc_ql_net_opt_1_ePRTC},
  {"PRTC", E_esmc_ql_net_opt_1_PRTC},
  {"ePRC", E_esmc_ql_net_opt_1_ePRC},
  {"PRC", E_esmc_ql_net_opt_1_PRC},
  {"SSUA", E_esmc_ql_net_opt_1_SSUA},
  {"SSUB", E_esmc_ql_net_opt_1_SSUB},
  {"eSEC", E_esmc_ql_net_opt_1_eSEC},
  {"SEC", E_esmc_ql_net_opt_1_SEC},
  {"DNU", E_esmc_ql_net_opt_1_DNU},
  {"INV0", E_esmc_ql_net_opt_1_INV0},
  {"INV1", E_esmc_ql_net_opt_1_INV1},
  {"INV3", E_esmc_ql_net_opt_1_INV3},
  {"INV5", E_esmc_ql_net_opt_1_INV5},
  {"INV6", E_esmc_ql_net_opt_1_INV6},
  {"INV7", E_esmc_ql_net_opt_1_INV7},
  {"INV9", E_esmc_ql_net_opt_1_INV9},
  {"INV10", E_esmc_ql_net_opt_1_INV10},
  {"INV12", E_esmc_ql_net_opt_1_INV12},
  {"INV13", E_esmc_ql_net_opt_1_INV13},
  {"INV14", E_esmc_ql_net_opt_1_INV14},
  {"NSUPP", E_esmc_ql_NSUPP},
  {"UNC", E_esmc_ql_UNC},
  {"FAILED", E_esmc_ql_FAILED}
};

static struct config_esmc_ql g_config_esmc_net_opt_2_ql[] = {
  {"ePRTC", E_esmc_ql_net_opt_2_ePRTC},
  {"PRTC", E_esmc_ql_net_opt_2_PRTC},
  {"ePRC", E_esmc_ql_net_opt_2_ePRC},
  {"PRS", E_esmc_ql_net_opt_2_PRS},
  {"STU", E_esmc_ql_net_opt_2_STU},
  {"ST2", E_esmc_ql_net_opt_2_ST2},
  {"TNC", E_esmc_ql_net_opt_2_TNC},
  {"ST3E", E_esmc_ql_net_opt_2_ST3E},
  {"eEEC", E_esmc_ql_net_opt_2_eEEC},
  {"ST3", E_esmc_ql_net_opt_2_ST3},
  {"SMC", E_esmc_ql_net_opt_2_SMC},
  {"ST4", E_esmc_ql_net_opt_2_ST4},
  {"PROV", E_esmc_ql_net_opt_2_PROV},
  {"DUS", E_esmc_ql_net_opt_2_DUS},
  {"INV2", E_esmc_ql_net_opt_2_INV2},
  {"INV3", E_esmc_ql_net_opt_2_INV3},
  {"INV5", E_esmc_ql_net_opt_2_INV5},
  {"INV6", E_esmc_ql_net_opt_2_INV6},
  {"INV8", E_esmc_ql_net_opt_2_INV8},
  {"INV9", E_esmc_ql_net_opt_2_INV9},
  {"INV11", E_esmc_ql_net_opt_2_INV11},
  {"NSUPP", E_esmc_ql_NSUPP},
  {"UNC", E_esmc_ql_UNC},
  {"FAILED", E_esmc_ql_FAILED}
};

static struct config_esmc_ql g_config_esmc_net_opt_3_ql[] = {
  // {"ePRTC", E_esmc_ql_net_opt_3_ePRTC},
  // {"PRTC", E_esmc_ql_net_opt_3_PRTC},
  {"UNK", E_esmc_ql_net_opt_3_UNK},
  // {"eSEC", E_esmc_ql_net_opt_3_eSEC},
  {"SEC", E_esmc_ql_net_opt_3_SEC},
  {"INV1", E_esmc_ql_net_opt_3_INV1},
  {"INV2", E_esmc_ql_net_opt_3_INV2},
  {"INV3", E_esmc_ql_net_opt_3_INV3},
  {"INV4", E_esmc_ql_net_opt_3_INV4},
  {"INV5", E_esmc_ql_net_opt_3_INV5},
  {"INV6", E_esmc_ql_net_opt_3_INV6},
  {"INV7", E_esmc_ql_net_opt_3_INV7},
  {"INV8", E_esmc_ql_net_opt_3_INV8},
  {"INV9", E_esmc_ql_net_opt_3_INV9},
  {"INV10", E_esmc_ql_net_opt_3_INV10},
  {"INV12", E_esmc_ql_net_opt_3_INV12},
  {"INV13", E_esmc_ql_net_opt_3_INV13},
  {"INV14", E_esmc_ql_net_opt_3_INV14},
  {"INV15", E_esmc_ql_net_opt_3_INV15},
  {"NSUPP", E_esmc_ql_NSUPP},
  {"UNC", E_esmc_ql_UNC},
  {"FAILED", E_esmc_ql_FAILED}
};

static struct config_item g_config_table[] = {
  /* Global variables */
  GLOB_ITEM_ENU("net_opt", E_esmc_network_option_1, g_config_esmc_net_opt_enu),
  GLOB_ITEM_INT("no_ql_en", 0, 0, 1),
  GLOB_ITEM_INT("synce_forced_ql_en", 0, 0, 1),
  GLOB_ITEM_STR("lo_ql", "FAILED"),
  GLOB_ITEM_INT("lo_pri", 255, 0, 255),
  GLOB_ITEM_STR("msg_tag", NULL),
  GLOB_ITEM_INT("max_msg_lvl", PRINT_LEVEL_MAX, PRINT_LEVEL_MIN, PRINT_LEVEL_MAX),
  GLOB_ITEM_INT("stdout_en", 1, 0, 1),
  GLOB_ITEM_INT("syslog_en", 0, 0, 1),
  GLOB_ITEM_STR("device_cfg_file", NULL),                                          /* Applicable for generic device */
  GLOB_ITEM_STR("device_name", "/dev/rsmu1"),
  GLOB_ITEM_INT("synce_dpll_idx", 0, 0, 7),
  GLOB_ITEM_STR("holdover_ql", "FAILED"),
  GLOB_ITEM_INT("holdover_tmr", 300, 0, INT32_MAX),                                /* Seconds */
  GLOB_ITEM_INT("hoff_tmr", 300, 0, INT16_MAX),                                    /* Milliseconds */
  GLOB_ITEM_INT("wtr_tmr", 300, 0, INT16_MAX),                                     /* Seconds */
  GLOB_ITEM_INT("advanced_holdover_en", 0, 0, 1),
  GLOB_ITEM_INT("pcm4l_if_en", 0, 0, 1),
  GLOB_ITEM_STR("pcm4l_if_ip_addr", ""),
  GLOB_ITEM_INT("pcm4l_if_port_num", 2400, 1024, UINT16_MAX),
  GLOB_ITEM_INT("mng_if_en", 0, 0, 1),
  GLOB_ITEM_STR("mng_if_ip_addr", ""),
  GLOB_ITEM_INT("mng_if_port_num", 2400, 1024, UINT16_MAX),

  /* Interface (port) variables */
  PORT_ITEM_INT("clk_idx", MISSING_CLK_IDX, 0, 15), /* Default value is MISSING_CLK_IDX, which means Sync-E monitoring port */
  PORT_ITEM_INT("pri", NO_PRI, 0, 255),
  PORT_ITEM_STR("init_ql", "FAILED"),
  PORT_ITEM_INT("tx_en", 0, 0, 1),
  PORT_ITEM_INT("rx_en", 0, 0, 1),
  PORT_ITEM_INT("tx_bundle_num", NO_TX_BUNDLE_NUM, NO_TX_BUNDLE_NUM, 255)
};

/* Static functions */

static struct config_item *config_section_item(struct config *cfg,
                                               const char *section,
                                               const char *name)
{
  char buf[CONFIG_LABEL_SIZE + INTERFACE_MAX_NAME_LEN];

  snprintf(buf, sizeof(buf), "%s.%s", section, name);

  return hash_lookup(cfg->config_hash_table, buf);
}

static struct config_item *config_global_item(struct config *cfg,
                                              const char *name)
{
  return config_section_item(cfg, "global", name);
}

static struct config_item *config_find_item(struct config *cfg,
                                            const char *section,
                                            const char *name)
{
  struct config_item *ci;

  if(section) {
    ci = config_section_item(cfg, section, name);

    if(ci) {
      return ci;
    }
  }

  return config_global_item(cfg, name);
}

static struct config_item *config_item_alloc(struct config *cfg,
                                             const char *section,
                                             const char *name,
                                             enum config_type type)
{
  struct config_item *ci;
  char buf[CONFIG_LABEL_SIZE + INTERFACE_MAX_NAME_LEN];

  ci = calloc(1, sizeof(*ci));
  if(!ci) {
    fprintf(stderr, "Insufficient memory\n");
    return NULL;
  }

  strncpy(ci->label, name, CONFIG_LABEL_SIZE - 1);

  ci->type = type;

  snprintf(buf, sizeof(buf), "%s.%s", section, ci->label);

  if(hash_insert(cfg->config_hash_table, buf, ci)) {
    fprintf(stderr, "Failed to insert %s due to low memory or because duplicate item\n", name);
    free(ci);
    return NULL;
  }

  return ci;
}

static void config_item_free(void *ptr)
{
  struct config_item *ci = ptr;

  if(ci->type == CFG_TYPE_STRING && ci->flags & CFG_ITEM_DYNSTR)
    free(ci->val.s);

  if(ci->flags & CFG_ITEM_STATIC)
    return;

  free(ci);
}

static enum config_parser_result config_parse_section_line(char *s,
                                                           enum config_section *section)
{
  if(!strcasecmp(s, "[global]")) {
    *section = GLOBAL_SECTION;
  } else if(s[0] == '[') {
    char c;
    while((c = *s) != 0) {
      if(c == ']') {
        *s = ' ';
      }
      s++;
    }
    *section = PORT_SECTION;
  } else {
    return NOT_PARSED;
  }

  return PARSED_OK;
}

static enum config_parser_result config_get_ranged_double(const char *str_val,
                                                          double *result,
                                                          double min,
                                                          double max)
{
  double parsed_val;
  char *endptr = NULL;
  errno = 0;

  parsed_val = strtod(str_val, &endptr);

  if(*endptr != '\0' || endptr == str_val)
    return MALFORMED;

  if(errno == ERANGE || parsed_val < min || parsed_val > max)
    return OUT_OF_RANGE;

  *result = parsed_val;

  return PARSED_OK;
}

static enum config_parser_result config_parse_item(struct config *cfg,
                                                   int commandline,
                                                   const char *section,
                                                   const char *option,
                                                   const char *value)
{
  enum config_parser_result r = BAD_VALUE;
  struct config_item *cgi, *dst;
  struct config_enum *cte;
  double df;
  int val;

  /* Section is NULL when it is global section */
  if(!section) {
    if(!strcmp(option, "lo_ql")) {
      struct config_item *ci = config_find_item(cfg, "global", "net_opt");
      T_esmc_network_option net_opt = ci->val.i;
      const char* lo_ql = value;
      int len;
      struct config_esmc_ql *table;
      int i;

      if(net_opt == E_esmc_network_option_1) {
        len = sizeof(g_config_esmc_net_opt_1_ql)/sizeof(g_config_esmc_net_opt_1_ql[0]);
        table = g_config_esmc_net_opt_1_ql;
      } else if(net_opt == E_esmc_network_option_2) {
        len = sizeof(g_config_esmc_net_opt_2_ql)/sizeof(g_config_esmc_net_opt_2_ql[0]);
        table = g_config_esmc_net_opt_2_ql;
      } else if(net_opt == E_esmc_network_option_3) {
        len = sizeof(g_config_esmc_net_opt_3_ql)/sizeof(g_config_esmc_net_opt_3_ql[0]);
        table = g_config_esmc_net_opt_3_ql;
      } else {
        return BAD_VALUE;
      }

      for(i = 0; i < len; i++) {
        if(!strcmp(table[i].label, lo_ql)) {
          r = PARSED_OK;
        }
      }

      if(r != PARSED_OK) {
        return r;
      }
    } else if(!strcmp(option, "lo_pri")) {
      int val = strtol(value, NULL, 10);
      if(g_config_pri_flag[val] == 0) {
        g_config_pri_flag[val] = 1;
      } else {
        fprintf(stderr, "Failed to assign priority %d for LO because priority already used\n", val);
        return BAD_VALUE;
      }
    }
  }

  cgi = config_global_item(cfg, option);
  if(!cgi) {
    return NOT_PARSED;
  }

  switch(cgi->type) {
  case CFG_TYPE_INT:
    r = config_get_ranged_int(value, &val, cgi->min.i, cgi->max.i);
    break;
  case CFG_TYPE_DOUBLE:
    r = config_get_ranged_double(value, &df, cgi->min.d, cgi->max.d);
    break;
  case CFG_TYPE_ENUM:
    for(cte = cgi->tab; cte->label; cte++) {
      if(!strcasecmp(cte->label, value)) {
        val = cte->value;
        r = PARSED_OK;
        break;
      }
    }
    break;
  case CFG_TYPE_STRING:
    r = PARSED_OK;
    break;
  }
  if(r != PARSED_OK) {
    return r;
  }

  if(section) {
    if(!(cgi->flags & CFG_ITEM_PORT)) {
      return NOT_PARSED;
    }
    /* Create or update this specific item */
    dst = config_section_item(cfg, section, option);
    if(!dst) {
      dst = config_item_alloc(cfg, section, option, cgi->type);
      if(!dst) {
        return NOT_PARSED;
      }
    } else {
      /* Return to prevent reassignment of value */
      return PARSED_OK;
    }
    if(!strcmp(option, "clk_idx")) {
      int val = strtol(value, NULL, 10);
      if(g_config_clk_idx_flag[val] == 0) {
        g_config_clk_idx_flag[val] = 1;
      } else {
        fprintf(stderr, "Failed to assign clock index %d for %s because clock index already used\n", val, section);
        return BAD_VALUE;
      }
    } else if(!strcmp(option, "pri")) {
      int val = strtol(value, NULL, 10);
      if(g_config_pri_flag[val] == 0) {
        g_config_pri_flag[val] = 1;
      } else {
        fprintf(stderr, "Failed to assign priority %d for %s because priority already used\n", val, section);
        return BAD_VALUE;
      }
    }
  } else if(!commandline && cgi->flags & CFG_ITEM_LOCKED) {
    return PARSED_OK;
  } else {
    /* Update global default value */
    dst = cgi;
  }

  switch(dst->type) {
  case CFG_TYPE_INT:
  case CFG_TYPE_ENUM:
    dst->val.i = val;
    break;
  case CFG_TYPE_DOUBLE:
    dst->val.d = df;
    break;
  case CFG_TYPE_STRING:
    if(dst->flags & CFG_ITEM_DYNSTR) {
      free(dst->val.s);
    }
    dst->val.s = strdup(value);
    if(!dst->val.s) {
      return NOT_PARSED;
    }
    dst->flags |= CFG_ITEM_DYNSTR;
    break;
  }

  if(commandline) {
    dst->flags |= CFG_ITEM_LOCKED;
  }

  return PARSED_OK;
}

static enum config_parser_result config_parse_setting_line(char *line,
                                                           const char **option,
                                                           const char **value)
{
  *option = line;

  while(!isspace(line[0])) {
    if(line[0] == '\0')
      return NOT_PARSED;
    line++;
  }

  while(isspace(line[0])) {
    line[0] = '\0';
    line++;
  }

  *value = line;

  return PARSED_OK;
}

static struct option *config_alloc_longopts(void)
{
  struct config_item *ci;
  struct option *opts;
  int i;

  opts = calloc(1, (1 + N_CONFIG_ITEMS) * sizeof(*opts));
  if(!opts) {
    return NULL;
  }
  for(i = 0; i < (int)N_CONFIG_ITEMS; i++) {
    ci = &g_config_table[i];
    opts[i].name = ci->label;
    opts[i].has_arg = required_argument;
  }

  return opts;
}

/* Global functions */

int config_read(const char *name,
                struct config *cfg,
                char **dump_buf,
                long *dump_buf_size)
{
  enum config_section current_section = UNKNOWN_SECTION;
  enum config_parser_result parser_res;
  FILE *fp;
  char buf[1024], *line, *c;
  const char *option, *value;
  struct interface *current_port = NULL;
  int line_num;
  int i;
  int port_counter = 0; /* Should not exceed MAX_NUM_OF_SYNC_ENTRIES */
  const long init_pos = 0;
  long file_size;
  long num_bytes_read;

  *dump_buf = NULL;
  *dump_buf_size = 0;

  fp = 0 == strncmp(name, "-", 2) ? stdin : fopen(name, "r");

  if(!fp) {
    fprintf(stderr, "Failed to fopen file %s\n", name);
    return -1;
  }

  /* Dump file */
  if(fseek(fp, init_pos, SEEK_END)) {
    fprintf(stderr, "Failed to fseek (SEEK_END) file %s\n", name);
    return -1;
  }
  file_size = ftell(fp) + 1; /* Add 1 to account for null character */
  if(file_size < 0) {
    fprintf(stderr, "Failed to ftell file %s\n", name);
    return -1;
  }
  if(fseek(fp, init_pos, SEEK_SET)) {
    fprintf(stderr, "Failed to fseek (SEEK_SET) file %s\n", name);
    return -1;
  }
  *dump_buf = calloc(1, file_size * sizeof(char));
  if(*dump_buf == NULL) {

    fprintf(stderr, "Failed to allocate memory for file %s dump\n", name);
    return -1;
  }
  num_bytes_read = fread(*dump_buf, sizeof(char), file_size, fp);
  if(num_bytes_read != (file_size - 1)) {
    fprintf(stderr, "Failed to fread file %s\n", name);
    free(*dump_buf);
    *dump_buf = NULL;
    return -1;
  }
  *dump_buf_size = file_size;
  if(fseek(fp, init_pos, SEEK_SET)) {
    fprintf(stderr, "Failed to fseek (SEEK_SET) file %s\n", name);
    free(*dump_buf);
    *dump_buf = NULL;
    *dump_buf_size = 0;
    return -1;
  }

  for(line_num = 1; fgets(buf, sizeof(buf), fp); line_num++) {
    c = buf;

    /* Skip whitespace characters */
    while(isspace(*c))
      c++;

    /* Ignore empty lines and comments */
    if(*c == '#' || *c == '\n' || *c == '\0')
      continue;

    line = c;

    /* Remove trailing whitespace characters and newlines */
    c += strlen(line) - 1;
    while(c > line && (*c == '\n' || isspace(*c)))
      *c-- = '\0';

    if(config_parse_section_line(line, &current_section) == PARSED_OK) {
      if(current_section == PORT_SECTION) {
        char port[INTERFACE_MAX_NAME_LEN];

        port_counter++;
        if(port_counter > MAX_NUM_OF_SYNC_ENTRIES) {
          fprintf(stderr,
                  "Failed to parse configuration on line %d because maximum number of ports %d reached\n",
                  line_num,
                  MAX_NUM_OF_SYNC_ENTRIES);
          goto parse_error;
        }

        if(strlen(line) > (INTERFACE_MAX_NAME_LEN - 1)) {
          line[INTERFACE_MAX_NAME_LEN - 1] = 0;
          fprintf(stderr,
                  "Failed to parse port name %s on line %d because number of characters exceeds %d\n",
                  line,
                  line_num,
                  INTERFACE_MAX_NAME_LEN - 1);
          goto parse_error;
        }

        /* sscanf() adds null character */
        if(sscanf(line, "[%s", port) != 1) {
          fprintf(stderr, "Failed to parse port name %s on line %d\n", port, line_num);
          goto parse_error;
        }

        for(i = 0; i < MAX_NUM_OF_SYNC_ENTRIES; i++) {
          if(!strcmp(port, g_config_port_name[i])) {
            fprintf(stderr, "Failed to parse configuration for port %s on line %d because name already used\n", port, line_num);
            goto parse_error;
          }
        }

        current_port = config_create_interface(port, cfg);
        if(!current_port) {
          goto parse_error;
        }

        strncpy(g_config_port_name[port_counter - 1], port, strlen(port) + 1);
      }

      continue;
    }

    if(current_section == UNKNOWN_SECTION) {
      fprintf(stderr, "Line %d is not in a section\n", line_num);
      goto parse_error;
    }

    if(config_parse_setting_line(line, &option, &value)) {
      fprintf(stderr,
              "Failed to parse line %d in %s section\n",
              line_num,
              current_section == GLOBAL_SECTION ? "global" : interface_get_name(current_port));
      goto parse_error;
    }

    parser_res = config_parse_item(cfg,
                                   0,
                                   (current_section == GLOBAL_SECTION) ? NULL : interface_get_name(current_port),
                                   option,
                                   value);

    switch(parser_res) {
    case PARSED_OK:
      break;
    case NOT_PARSED:
      fprintf(stderr, "Unknown option %s at line %d in %s section\n",
              option,
              line_num,
              current_section == GLOBAL_SECTION ? "global" : interface_get_name(current_port));
      goto parse_error;
    case BAD_VALUE:
      fprintf(stderr,
              "%s is a bad value for option %s at line %d\n",
              value,
              option,
              line_num);
      goto parse_error;
    case MALFORMED:
      fprintf(stderr,
              "%s is a malformed value for option %s at line %d\n",
              value,
              option,
              line_num);
      goto parse_error;
    case OUT_OF_RANGE:
      fprintf(stderr,
              "%s is an out of range value for option %s at line %d\n",
              value,
              option,
              line_num);
      goto parse_error;
    }
  }

  fclose(fp);
  return 0;

parse_error:
  fprintf(stderr, "Failed to parse configuration file %s\n", name);
  fclose(fp);
  free(*dump_buf);
  *dump_buf = NULL;
  *dump_buf_size = 0;
  return -1;
}

struct interface *config_create_interface(const char *name,
                                          struct config *cfg)
{
  struct interface *iface;
  const char *ifname;

  /* Create each interface only once by name */
  STAILQ_FOREACH(iface, &cfg->interfaces, list) {
    ifname = interface_get_name(iface);
    if(strncmp(name, ifname, INTERFACE_MAX_NAME_LEN) == 0)
      return iface;
  }

  iface = interface_create(name);
  if(!iface) {
    fprintf(stderr, "Failed to create interface %s\n", name);
    return NULL;
  }

  STAILQ_INSERT_TAIL(&cfg->interfaces, iface, list);
  cfg->n_interfaces++;
  return iface;
}

void config_destroy(struct config *cfg)
{
  struct interface *iface;

  while((iface = STAILQ_FIRST(&cfg->interfaces))) {
    STAILQ_REMOVE_HEAD(&cfg->interfaces, list);
    interface_destroy(iface);
    iface = NULL;
  }

  hash_destroy(cfg->config_hash_table, config_item_free);
  free(cfg->opts);
  free(cfg);
}

struct config *config_create(void)
{
  char buf[CONFIG_LABEL_SIZE + 8];
  struct config_item *ci;
  struct config *cfg;
  int i;

  cfg = calloc(1, sizeof(*cfg));
  if(!cfg) {
    return NULL;
  }

  STAILQ_INIT(&cfg->interfaces);

  cfg->opts = config_alloc_longopts();
  if(!cfg->opts) {
    free(cfg);
    return NULL;
  }

  cfg->config_hash_table = hash_create();
  if(!cfg->config_hash_table) {
    free(cfg->opts);
    free(cfg);
    return NULL;
  }

  /* Populate hash table with global defaults */
  for(i = 0; i < (int)N_CONFIG_ITEMS; i++) {
    ci = &g_config_table[i];
    ci->flags |= CFG_ITEM_STATIC;
    snprintf(buf, sizeof(buf), "global.%s", ci->label);
    if(hash_insert(cfg->config_hash_table, buf, ci)) {
      fprintf(stderr, "Duplicate item %s\n", ci->label);
      goto fail;
    }
  }

  /* Perform built-in-self-test (BIST) */
  for(i = 0; i < (int)N_CONFIG_ITEMS; i++) {
    ci = &g_config_table[i];
    ci = config_global_item(cfg, ci->label);
    if(ci != &g_config_table[i]) {
      fprintf(stderr, "Configuration BIST failed at %s\n", g_config_table[i].label);
      goto fail;
    }
  }

  return cfg;

fail:
  hash_destroy(cfg->config_hash_table, NULL);
  free(cfg->opts);
  free(cfg);
  return NULL;
}

double config_get_double(struct config *cfg,
                         const char *section,
                         const char *option)
{
  struct config_item *ci = config_find_item(cfg, section, option);

  if(!ci || ci->type != CFG_TYPE_DOUBLE) {
    fprintf(stderr, "Missing or invalid option %s\n", option);
    exit(-1);
  }

  return ci->val.d;
}

int config_get_int(struct config *cfg,
                   const char *section,
                   const char *option)
{
  struct config_item *ci = config_find_item(cfg, section, option);

  if(!ci) {
    fprintf(stderr, "Missing or invalid option %s\n", option);
    exit(-1);
  }

  switch(ci->type) {
  case CFG_TYPE_DOUBLE:
  case CFG_TYPE_STRING:
    exit(-1);
  case CFG_TYPE_INT:
  case CFG_TYPE_ENUM:
    break;
  }

  return ci->val.i;
}

char *config_get_string(struct config *cfg,
                        const char *section,
                        const char *option)
{
  struct config_item *ci = config_find_item(cfg, section, option);

  if(!ci || ci->type != CFG_TYPE_STRING) {
    fprintf(stderr, "Missing or invalid option %s\n", option);
    return NULL;
  }

  return ci->val.s;
}

int config_set_double(struct config *cfg,
                      const char *option,
                      double val)
{
  struct config_item *ci = config_find_item(cfg, NULL, option);

  if(!ci || ci->type != CFG_TYPE_DOUBLE) {
    return -1;
  }

  ci->flags |= CFG_ITEM_LOCKED;
  ci->val.d = val;

  return 0;
}

int config_set_section_int(struct config *cfg,
                           const char *section,
                           const char *option,
                           int val)
{
  struct config_item *cgi, *dst;

  cgi = config_find_item(cfg, NULL, option);
  if(!cgi) {
    return -1;
  }

  switch(cgi->type) {
  case CFG_TYPE_DOUBLE:
  case CFG_TYPE_STRING:
    return -1;
  case CFG_TYPE_INT:
  case CFG_TYPE_ENUM:
    break;
  }

  if(!section) {
    cgi->flags |= CFG_ITEM_LOCKED;
    cgi->val.i = val;
    return 0;
  }

  dst = config_section_item(cfg, section, option);
  if(!dst) {
    dst = config_item_alloc(cfg, section, option, cgi->type);
    if(!dst) {
      return -1;
    }
  }
  dst->val.i = val;

  return 0;
}

int config_set_string(struct config *cfg,
                      const char *option,
                      const char *val)
{
  struct config_item *ci = config_find_item(cfg, NULL, option);

  if(!ci || ci->type != CFG_TYPE_STRING) {
    return -1;
  }

  ci->flags |= CFG_ITEM_LOCKED;

  if(ci->flags & CFG_ITEM_DYNSTR) {
    free(ci->val.s);
  }

  ci->val.s = strdup(val);

  if(!ci->val.s) {
    return -1;
  }

  ci->flags |= CFG_ITEM_DYNSTR;

  return 0;
}

int config_parse_option(struct config *cfg,
                        const char *opt,
                        const char *val)
{
  enum config_parser_result result = config_parse_item(cfg, 1, NULL, opt, val);

  switch(result) {
  case PARSED_OK:
    return 0;
  case NOT_PARSED:
    fprintf(stderr, "Unknown option %s\n", opt);
    break;
  case BAD_VALUE:
    fprintf(stderr, "%s is a bad value for option %s\n", val, opt);
    break;
  case MALFORMED:
    fprintf(stderr, "%s is a malformed value for option %s\n", val, opt);
    break;
  case OUT_OF_RANGE:
    fprintf(stderr, "%s is an out of range value for option %s\n", val, opt);
    break;
  }

  return -1;
}

int config_ql_str_to_enum_conv(T_esmc_network_option net_opt,
                               const char *ql_str,
                               T_esmc_ql *ql)
{
  int len;
  struct config_esmc_ql *table;
  int i;

  if(net_opt == E_esmc_network_option_1) {
    len = sizeof(g_config_esmc_net_opt_1_ql)/sizeof(g_config_esmc_net_opt_1_ql[0]);
    table = g_config_esmc_net_opt_1_ql;
  } else if(net_opt == E_esmc_network_option_2) {
    len = sizeof(g_config_esmc_net_opt_2_ql)/sizeof(g_config_esmc_net_opt_2_ql[0]);
    table = g_config_esmc_net_opt_2_ql;
  } else if(net_opt == E_esmc_network_option_3) {
    len = sizeof(g_config_esmc_net_opt_3_ql)/sizeof(g_config_esmc_net_opt_3_ql[0]);
    table = g_config_esmc_net_opt_3_ql;
  } else {
    return -1;
  }

  for(i = 0; i < len; i++) {
    if(!strcmp(table[i].label, ql_str)) {
      /* Match */
      *ql = table[i].ql;
      return 0;
    }
  }

  /* No match */
  return -1;
}

enum config_parser_result config_get_ranged_int(const char *str_val,
                                                int *result,
                                                int min,
                                                int max)
{
  long parsed_val;
  char *endptr = NULL;
  errno = 0;

  parsed_val = strtol(str_val, &endptr, 0);

  if(*endptr != '\0' || endptr == str_val)
    return MALFORMED;

  if(errno == ERANGE || parsed_val < min || parsed_val > max)
    return OUT_OF_RANGE;

  *result = parsed_val;

  return PARSED_OK;
}

int config_get_arg_val_i(int op,
                         const char *optarg,
                         int *val,
                         int min,
                         int max)
{
  enum config_parser_result r = config_get_ranged_int(optarg, val, min, max);

  if(r == MALFORMED) {
    fprintf(stderr, "%c: %s is a malformed value\n", op, optarg);
    return -1;
  }

  if(r == OUT_OF_RANGE) {
    fprintf(stderr, "%c: %s is out of range; must be in the range %d to %d\n", op, optarg, min, max);
    return -1;
  }

  return 0;
}
