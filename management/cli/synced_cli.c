/**
 * @file synced_cli.c
 * @brief synced command-line interface main program
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
#include <getopt.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include "../mng_if.h"
#include "../../common/common.h"
#include "../../common/config.h"
#include "../../common/print.h"

#ifndef __linux__
#error __linux__ is not defined!
#endif /* __linux__ */

#define VERSION_ID    "2.0.5"
#define PIPELINE_ID   "310964"
#define COMMIT_ID     "b166f770"

#define CLI_BUFF_SIZE   128

#define COMMAND_QUEUE_SIZE   64

typedef struct {
  char port_name[INTERFACE_MAX_NAME_LEN];
} T_command_get_sync_info;

typedef struct {
  char port_name[INTERFACE_MAX_NAME_LEN];
  T_esmc_ql forced_ql;
} T_command_set_forced_ql;

typedef struct {
  char port_name[INTERFACE_MAX_NAME_LEN];
} T_command_clear_forced_ql;

typedef struct {
  char port_name[INTERFACE_MAX_NAME_LEN];
} T_command_clear_synce_clk_wtr_timer;

typedef struct {
  char port_name[INTERFACE_MAX_NAME_LEN];
  int clk_idx;
} T_command_assign_new_synce_clk_port;

typedef struct {
  char port_name[INTERFACE_MAX_NAME_LEN];
  int pri;
} T_command_set_pri;

typedef struct {
  int max_msg_lvl;
} T_command_set_max_msg_lvl;

typedef struct {
  T_mng_api api_code;
  union {
    T_command_get_sync_info             command_line_info_get_sync_info;
    T_command_set_forced_ql             command_line_info_set_forced_ql;
    T_command_clear_forced_ql           command_line_info_clear_forced_ql;
    T_command_clear_synce_clk_wtr_timer command_line_info_clear_synce_clk_wtr_timer;
    T_command_assign_new_synce_clk_port command_line_info_assign_new_synce_clk_port;
    T_command_set_pri                   command_line_info_set_pri;
    T_command_set_max_msg_lvl           command_line_info_set_max_msg_lvl;

    /* No data for following APIs:
     *   - get_sync_info_list
     *   - get_current_status
     *   - clear_holdover_timer
     */
  };
} T_command;


/* Static data */

static const char *g_version = VERSION_ID;
static const char *g_pipeline = PIPELINE_ID;
static const char *g_commit = COMMIT_ID;

static char cli_buffer[CLI_BUFF_SIZE];
static int print_flag;

/* g_command_line_mode = 0 -> interactive mode; g_command_line_mode = 1 -> command-line mode */
static int g_command_line_mode = 0;

/*
 * Command queue:
 *
 *                                                           0
 *                                                  +----------------+
 *                                                  |      -c 0      | <-- Head (Pop command from queue, execute, and increment head)
 *                                                  +----------------+
 *                                                  |      -c 1      |
 *                                                  +----------------+
 *  command-line -> getopt() -> parse_command() --> |      -c 5      | <-- Tail (push command to queue and increment tail)
 *                                                  +----------------+
 *                                                  |                |
 *                                                  +----------------+
 *                                                  |                |
 *                                                  +----------------+
 *                                                  COMMAND_QUEUE_SIZE
 */
static T_command g_command_queue[COMMAND_QUEUE_SIZE] = {0};
static int g_command_queue_head = 0;                        /* Index of next command to be popped from queue and then executed */
static int g_command_queue_tail = 0;                        /* Index of next command to be pushed to queue */

static const char *g_api_code_to_api_code_str[] = {
  "get_sync_info_list",
  "get_current_status",
  "get_sync_info",
  "set_forced_ql",
  "clear_forced_ql",
  "clear_holdover_timer",
  "clear_synce_clk_wtr_timer",
  "assign_new_synce_clk_port",
  "set_pri",
  "set_max_msg_lvl"
};
COMPILE_TIME_ASSERT((sizeof(g_api_code_to_api_code_str)/sizeof(g_api_code_to_api_code_str[0])) == E_mng_api_max, "Invalid array size for g_api_code_to_api_code_str!")
COMPILE_TIME_ASSERT(E_mng_api_get_sync_info_list == 0, "Invalid index for 'get_sync_info_list' in g_api_code_to_api_code_str")
COMPILE_TIME_ASSERT(E_mng_api_get_current_status == 1, "Invalid index for 'get_current_status' in g_api_code_to_api_code_str")
COMPILE_TIME_ASSERT(E_mng_api_get_sync_info == 2, "Invalid index for 'get_sync_info' in g_api_code_to_api_code_str")
COMPILE_TIME_ASSERT(E_mng_api_set_forced_ql == 3, "Invalid index for 'set_forced_ql' in g_api_code_to_api_code_str")
COMPILE_TIME_ASSERT(E_mng_api_clear_forced_ql == 4, "Invalid index for 'clear_forced_ql' in g_api_code_to_api_code_str")
COMPILE_TIME_ASSERT(E_mng_api_clear_holdover_timer == 5, "Invalid index for 'clear_holdover_timer' in g_api_code_to_api_code_str")
COMPILE_TIME_ASSERT(E_mng_api_clear_synce_clk_wtr_timer == 6, "Invalid index for 'clear_synce_clk_wtr_timer' in g_api_code_to_api_code_str")
COMPILE_TIME_ASSERT(E_mng_api_assign_new_synce_clk_port == 7, "Invalid index for 'assign_new_synce_clk_port' in g_api_code_to_api_code_str")
COMPILE_TIME_ASSERT(E_mng_api_set_pri == 8, "Invalid index for 'set_pri' in g_api_code_to_api_code_str")
COMPILE_TIME_ASSERT(E_mng_api_set_max_msg_lvl == 9, "Invalid index for 'set_max_msg_lvl' in g_api_code_to_api_code_str")

/* Static functions */

static void usage(char *prog_name)
{
  fprintf(stderr,
          "usage: %s IP_address port_number print_flag [options]\n"
          "options:\n"
          "  -c [command] [args] Execute specified Management API via command-line (maximum %d commands).\n"
          "  -h Display command-line options (i.e. print this message).\n"
          "  -l Display list of Management API codes (in square brackets on left) and strings (in parentheses on right).\n"
          "  -v Display software version.\n",
          prog_name,
          COMMAND_QUEUE_SIZE);
}

static const char *conv_api_enum_to_str(T_mng_api api_code)
{
  if(api_code >= E_mng_api_max) {
    printf("Failed to convert API code enumeration to string");
    return "**unknown**";
  }

  return g_api_code_to_api_code_str[api_code];
}

static int conv_api_code_str_to_enum(const char *api_code_str, T_mng_api *api_code)
{
  int i;

  for(i = 0; i < E_mng_api_max; i++) {
    if(!strcmp(g_api_code_to_api_code_str[i], api_code_str)) {
      /* Match */
      *api_code = (T_mng_api)i;
      return 0;
    }
  }

  /* No match */
  return -1;
}

static void print_help(void)
{
  int i;

  printf("\n=== Commands:\n");
  printf("In interactive mode, enter the code in the square brackets on the left.\n"
         "In command-line mode, enter the code in the square brackets on the left or the string in parentheses on the right.\n\n");

  for(i = 0; i < E_mng_api_max; i++) {
    printf("[%d]: %s (%s)\n", i, conv_api_code_to_str(i), conv_api_enum_to_str(i));
  }
  printf("\nOther options: \"exit\", \"help\"\n");
}

static void get_cli_string(void)
{
  char input;
  int counter = 0;

  do {
    input = getchar();
    if(input == '\n') {
      input = 0;
    }
    if(counter < (CLI_BUFF_SIZE - 1)) {
      cli_buffer[counter] = input;
      counter++;
    } else {
      cli_buffer[counter] = 0;
      return;
    }
  }
  while(input != 0);
}

static int command_queue_push(T_command *command)
{
    if(g_command_queue_tail == COMMAND_QUEUE_SIZE) {
      printf("***Error: Command queue is full");
      return -1;
    }
    g_command_queue[g_command_queue_tail] = *command;
    g_command_queue_tail++;
    return 0;
}

static int command_queue_pop(T_command *command)
{
  if(g_command_queue_head == g_command_queue_tail) {
    return -1;
  }
  *command = g_command_queue[g_command_queue_head];
  g_command_queue_head++;
  return 0;
}

static int parse_command(T_command *command, int optind, char *argv[])
{
  switch(command->api_code) {
    case E_mng_api_get_sync_info_list:
    case E_mng_api_get_current_status:
    case E_mng_api_clear_holdover_timer:
      /* Left intentionally empty */
      break;

    case E_mng_api_get_sync_info:
      {
        const char *arg = argv[optind];
        if(arg == NULL) {
          printf("***Error: %s: %s expected port name\n", __func__, conv_api_code_to_str(E_mng_api_get_sync_info));
          return -1;
        }
        strncpy(command->command_line_info_get_sync_info.port_name, arg, INTERFACE_MAX_NAME_LEN - 1);
        command->command_line_info_get_sync_info.port_name[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
      }
      break;

    case E_mng_api_set_forced_ql:
      {
        const char *first_arg = argv[optind];
        const char *second_arg = argv[optind + 1];
        if((first_arg == NULL) || (second_arg == NULL)) {
          printf("***Error: %s: %s expected port name and forced QL\n", __func__, conv_api_code_to_str(E_mng_api_set_forced_ql));
          return -1;
        }
        strncpy(command->command_line_info_set_forced_ql.port_name, first_arg, INTERFACE_MAX_NAME_LEN - 1);
        command->command_line_info_set_forced_ql.port_name[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
        command->command_line_info_set_forced_ql.forced_ql = (T_esmc_ql)atoi(second_arg);
      }
      break;

    case E_mng_api_clear_forced_ql:
      {
        const char *arg = argv[optind];
        if(arg == NULL) {
          printf("***Error: %s: %s expected port name\n", __func__, conv_api_code_to_str(E_mng_api_clear_forced_ql));
          return -1;
        }
        strncpy(command->command_line_info_clear_forced_ql.port_name, arg, INTERFACE_MAX_NAME_LEN - 1);
        command->command_line_info_clear_forced_ql.port_name[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
      }
      break;

    case E_mng_api_clear_synce_clk_wtr_timer:
      {
        const char *arg = argv[optind];
        if(arg == NULL) {
          printf("***Error: %s: %s expected port name\n", __func__, conv_api_code_to_str(E_mng_api_clear_synce_clk_wtr_timer));
          return -1;
        }
        strncpy(command->command_line_info_clear_synce_clk_wtr_timer.port_name, arg, INTERFACE_MAX_NAME_LEN - 1);
        command->command_line_info_clear_synce_clk_wtr_timer.port_name[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
      }
      break;

    case E_mng_api_assign_new_synce_clk_port:
      {
        const char *first_arg = argv[optind];
        const char *second_arg = argv[optind + 1];
        if((first_arg == NULL) || (second_arg == NULL)) {
          printf("***Error: %s: %s expected port name and clock index\n", __func__, conv_api_code_to_str(E_mng_api_assign_new_synce_clk_port));
          return -1;
        }
        strncpy(command->command_line_info_assign_new_synce_clk_port.port_name, first_arg, INTERFACE_MAX_NAME_LEN - 1);
        command->command_line_info_assign_new_synce_clk_port.port_name[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
        command->command_line_info_assign_new_synce_clk_port.clk_idx = atoi(second_arg);
      }
      break;

    case E_mng_api_set_pri:
      {
        const char *first_arg = argv[optind];
        const char *second_arg = argv[optind + 1];
        if((first_arg == NULL) || (second_arg == NULL)) {
          printf("***Error: %s: %s expected expected port name and priority\n", __func__, conv_api_code_to_str(E_mng_api_set_pri));
          return -1;
        }
        strncpy(command->command_line_info_set_pri.port_name, first_arg, INTERFACE_MAX_NAME_LEN - 1);
        command->command_line_info_set_pri.port_name[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
        command->command_line_info_set_pri.pri = atoi(second_arg);
      }
      break;

    case E_mng_api_set_max_msg_lvl:
      {
        const char *arg = argv[optind];
        if(arg == NULL) {
          printf("***Error: %s: %s expected maximum message log level\n", __func__, conv_api_code_to_str(E_mng_api_set_max_msg_lvl));
          return -1;
        }
        command->command_line_info_set_max_msg_lvl.max_msg_lvl = atoi(arg);
      }
      break;

    default:
      printf("***Error: %s: unknown API\n", __func__);
      return -1;
  }

  return 0;
}

static void compose_req_msg_in_command_line_mode(T_command *command, T_mng_api_request_msg *req_msg)
{
  req_msg->api_code = command->api_code;
  switch(req_msg->api_code) {
    case E_mng_api_get_sync_info_list:
      req_msg->request_get_sync_info_list.print_flag = print_flag;
      req_msg->request_get_sync_info_list.max_num_syncs = MAX_SYNC_INFO_STRUCTURES;
      break;

    case E_mng_api_get_current_status:
      req_msg->request_get_current_status.print_flag = print_flag;
      break;

    case E_mng_api_get_sync_info:
      req_msg->request_get_sync_info.print_flag = print_flag;
      strcpy(req_msg->request_get_sync_info.port_name, command->command_line_info_get_sync_info.port_name);
      break;

    case E_mng_api_set_forced_ql:
      req_msg->request_set_forced_ql.print_flag = print_flag;
      strcpy(req_msg->request_set_forced_ql.port_name, command->command_line_info_set_forced_ql.port_name);
      req_msg->request_set_forced_ql.forced_ql = (T_esmc_ql)command->command_line_info_set_forced_ql.forced_ql;
      break;

    case E_mng_api_clear_forced_ql:
      req_msg->request_clear_forced_ql.print_flag = print_flag;
      strcpy(req_msg->request_clear_forced_ql.port_name, command->command_line_info_clear_forced_ql.port_name);
      break;

    case E_mng_api_clear_holdover_timer:
      req_msg->request_clear_holdover_timer.print_flag = print_flag;
      break;

    case E_mng_api_clear_synce_clk_wtr_timer:
      req_msg->request_clear_synce_clk_wtr_timer.print_flag = print_flag;
      strcpy(req_msg->request_clear_synce_clk_wtr_timer.port_name, command->command_line_info_clear_synce_clk_wtr_timer.port_name);
      break;

    case E_mng_api_assign_new_synce_clk_port:
      req_msg->request_assign_new_synce_clk_port.print_flag = print_flag;
      strcpy(req_msg->request_assign_new_synce_clk_port.port_name, command->command_line_info_assign_new_synce_clk_port.port_name);
      req_msg->request_assign_new_synce_clk_port.clk_idx = command->command_line_info_assign_new_synce_clk_port.clk_idx;
      break;

    case E_mng_api_set_pri:
      req_msg->request_set_pri.print_flag = print_flag;
      strcpy(req_msg->request_set_pri.port_name, command->command_line_info_set_pri.port_name);
      req_msg->request_set_pri.pri = command->command_line_info_set_pri.pri;
      break;

    case E_mng_api_set_max_msg_lvl:
      req_msg->request_set_max_msg_lvl.print_flag = print_flag;
      req_msg->request_set_max_msg_lvl.max_msg_lvl = command->command_line_info_set_max_msg_lvl.max_msg_lvl;
      break;

    default:
      printf("***Error: %s: unknown API\n", __func__);
      break;
  }
}

static void compose_req_msg_in_interactive_mode(T_mng_api api_code, T_mng_api_request_msg *req_msg)
{
  req_msg->api_code = api_code;
  switch(req_msg->api_code) {
    case E_mng_api_get_sync_info_list:
      req_msg->request_get_sync_info_list.print_flag = print_flag;
      req_msg->request_get_sync_info_list.max_num_syncs = MAX_SYNC_INFO_STRUCTURES;
      break;

    case E_mng_api_get_current_status:
      req_msg->request_get_current_status.print_flag = print_flag;
      break;

    case E_mng_api_get_sync_info:
      req_msg->request_get_sync_info.print_flag = print_flag;
      printf("port name: ");
      get_cli_string();
      cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
      strcpy(req_msg->request_get_sync_info.port_name, cli_buffer);
      break;

    case E_mng_api_set_forced_ql:
      req_msg->request_set_forced_ql.print_flag = print_flag;
      printf("port name: ");
      get_cli_string();
      cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
      strcpy(req_msg->request_set_forced_ql.port_name, cli_buffer);
      printf("forced QL: ");
      get_cli_string();
      req_msg->request_set_forced_ql.forced_ql = (T_esmc_ql)atoi(cli_buffer);
      break;

    case E_mng_api_clear_forced_ql:
      req_msg->request_clear_forced_ql.print_flag = print_flag;
      printf("port name: ");
      get_cli_string();
      cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
      strcpy(req_msg->request_clear_forced_ql.port_name, cli_buffer);
      break;

    case E_mng_api_clear_holdover_timer:
      req_msg->request_clear_holdover_timer.print_flag = print_flag;
      break;

    case E_mng_api_clear_synce_clk_wtr_timer:
      req_msg->request_clear_synce_clk_wtr_timer.print_flag = print_flag;
      printf("port name: ");
      get_cli_string();
      cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
      strcpy(req_msg->request_clear_synce_clk_wtr_timer.port_name, cli_buffer);
      break;

    case E_mng_api_assign_new_synce_clk_port:
      req_msg->request_assign_new_synce_clk_port.print_flag = print_flag;
      printf("port name: ");
      get_cli_string();
      cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
      strcpy(req_msg->request_assign_new_synce_clk_port.port_name, cli_buffer);
      printf("clock index: ");
      get_cli_string();
      req_msg->request_assign_new_synce_clk_port.clk_idx = atoi(cli_buffer);
      break;

    case E_mng_api_set_pri:
      req_msg->request_set_pri.print_flag = print_flag;
      printf("port name: ");
      get_cli_string();
      cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
      strcpy(req_msg->request_set_pri.port_name, cli_buffer);
      printf("priority: ");
      get_cli_string();
      req_msg->request_set_pri.pri = atoi(cli_buffer);
      break;

    case E_mng_api_set_max_msg_lvl:
      req_msg->request_set_max_msg_lvl.print_flag = print_flag;
      printf("maximum message level: ");
      get_cli_string();
      req_msg->request_set_max_msg_lvl.max_msg_lvl = atoi(cli_buffer);
      break;

    default:
      printf("***Error: %s: unknown API\n", __func__);
      break;
  }
}

static void parse_rsp_msg(T_mng_api api_code, T_mng_api_response_msg *rsp_msg)
{
  if(rsp_msg->api_code >= E_mng_api_max) {
    printf("***Error: Invalid response\n");
    return;
  }

  if(api_code != rsp_msg->api_code) {
    printf("***Error: Requested '%s' and received '%s'\n",
           conv_api_code_to_str(api_code),
           conv_api_code_to_str(rsp_msg->api_code));
    return;
  }

  if(rsp_msg->response != E_management_api_response_ok) {
    switch(rsp_msg->response) {
      case E_management_api_response_invalid:
        printf("Invalid response\n");
        break;

      case E_management_api_response_failed:
        printf("Failed response\n");
        break;

      case E_management_api_response_not_supported:
        printf("Not supported response\n");
        break;

      default:
        printf("***Error: %s: unknown response\n", __func__);
        break;
    }
    return;
  }

  switch(rsp_msg->api_code) {
    case E_mng_api_get_sync_info_list:
      printf("Sync info list:\n");
      {
        T_management_sync_info *sync_info = &rsp_msg->response_get_sync_info_list.sync_info_list[0];
        int i = rsp_msg->response_get_sync_info_list.num_syncs;

        if(i > MAX_SYNC_INFO_STRUCTURES) {
          i = MAX_SYNC_INFO_STRUCTURES;
        }

        while(i > 0) {
          print_sync_info(sync_info);
          sync_info++;
          i--;
        }
      }
      break;

    case E_mng_api_get_current_status:
      printf("Current status:\n");
      {
        T_management_status *status = &rsp_msg->response_get_current_status.current_status;
        print_current_status(status);
      }
      break;

    case E_mng_api_get_sync_info:
      printf("Sync info:\n");
      {
        T_management_sync_info *sync_info = &rsp_msg->response_get_sync_info.sync_info;
        print_sync_info(sync_info);
      }
      break;

    case E_mng_api_set_forced_ql:
      printf("Set forced QL was successful\n");
      break;

    case E_mng_api_clear_forced_ql:
      printf("Clear forced QL was successful\n");
      break;

    case E_mng_api_clear_holdover_timer:
      printf("Clear holdover timer was successful\n");
      break;

    case E_mng_api_clear_synce_clk_wtr_timer:
      printf("Clear wait-to-restore timer was successful\n");
      break;

    case E_mng_api_assign_new_synce_clk_port:
      printf("Assign new Sync-E clock port was successful\n");
      break;

    case E_mng_api_set_pri:
      printf("Set priority was successful\n");
      break;

    case E_mng_api_set_max_msg_lvl:
      printf("Set maximum message level was successful\n");
      break;

    default:
      printf("***Error: %s: unknown API\n", __func__);
      break;
  }
}

/* Global functions */

int main(int argc, char *argv[])
{
  int err = -1;
  char *prog_name = strrchr(argv[0], '/') + 1;
  int opt;
  T_mng_api api_code;
  T_command command;
  int fd;
  struct sockaddr_in server;
  T_mng_api_request_msg req_msg;
  T_mng_api_response_msg rsp_msg;
  int status;

  memset(&server, 0, sizeof(server));
  memset(&req_msg, 0, sizeof(req_msg));
  memset(&rsp_msg, 0, sizeof(rsp_msg));

  /* Minus (-) instructs getopt() to not move all non-option arguments to the end of the command-line */
  while(EOF != (opt = getopt(argc, argv, "-c:hlv"))) {
    switch(opt) {
      case 'c':
        api_code = atoi(optarg);
        if(strcmp(optarg, "0") && (api_code == 0)) {
          /* Not a number; might be a command string */
          if(conv_api_code_str_to_enum(optarg, &api_code) < 0) {
            printf("***Error: Failed to recognize Management API string\n");
            goto quick_end;
          }
        }
        command.api_code = api_code;
        if(parse_command(&command, optind, argv) < 0) {
          goto quick_end;
        }
        if(command_queue_push(&command) < 0) {
          printf("***Warning: Maximum of %u commands will be executed\n", COMMAND_QUEUE_SIZE);
        }
        break;

      case 'l':
        print_help();
        goto quick_end;

      case 'v':
        printf("%s version: %s.%s.%s\n", prog_name, g_version, g_pipeline, g_commit);
        goto quick_end;

      case 'h':
      case '?':
      case ':':
        usage(prog_name);
        goto quick_end;

      default:
        if(optind > 4) {
          usage(prog_name);
          goto quick_end;
        }
    }
  }

  if(argc <= 3) {
    printf("***Error: Must specify IP address, port number, and print flag (ex: 127.0.0.2, 2400, and 1)\n");
    usage(prog_name);
    goto quick_end;
  }

  /* optind points to the next element in argv; if optind = 4, then 3 arguments were entered by user */
  if(optind == 4) {
    printf("%s mode: interactive\n", prog_name);
  } else {
    printf("%s mode: command-line\n", prog_name);
    g_command_line_mode = 1;
  }

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0) {
    printf("***Error: Failed to create socket\n");
    usage(prog_name);
    goto quick_end;
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons(atoi(argv[2]));
  print_flag = atoi(argv[3]);

  if(connect(fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
    printf("***Error: Failed to connect\n");
    goto end;
  }
  printf("Connected to socket with IP Address: %s, port number: %s, and print flag: %s\n", argv[1], argv[2], argv[3]);

  print_set_prog_name(prog_name);
  print_set_max_msg_level(LOG_DEBUG);
  print_set_stdout_en(1);

  printf("---Started %s (version: %s.%s.%s %s %s)---\n", prog_name, g_version, g_pipeline, g_commit, __DATE__, __TIME__);

  memset(&command, 0, sizeof(command));
  while(1) {
    if(g_command_line_mode == 1) {
      /* Command-line mode */

      if(command_queue_pop(&command) < 0) {
        /* No more commands in queue */
        goto end;
      }

      api_code = command.api_code;
    } else {
      /* Interactive mode */

      /* Print supported commands */
      print_help();

      /* Print command prompt */
      printf("\n>");

      /* Get command */
      get_cli_string();

      /* Check command */
      if(strcmp(cli_buffer, "exit") == 0) {
        break;
      }
      if(strcmp(cli_buffer, "help") == 0) {
        print_help();
        continue;
      }

      api_code = (T_mng_api)atoi(cli_buffer);

      /* atoi() returns 0 if no valid conversion can be performed */
      if(strcmp(cli_buffer, "0") && (api_code == 0)) {
        printf("***Error: Invalid request\n");
        continue;
      }
    }

    if(api_code >= E_mng_api_max) {
      printf("***Error: Invalid request\n");
      continue;
    }

    /* Print API request */
    printf("%s\n", conv_api_code_to_str(api_code));

    /* Compose request message */
    if(g_command_line_mode == 1) {
      /* Command-line mode */
      compose_req_msg_in_command_line_mode(&command, &req_msg);
    } else {
      /* Interactive mode */
      compose_req_msg_in_interactive_mode(api_code, &req_msg);
    }

    /* Discard stale messages */
    do {
      status = recv(fd, &rsp_msg, sizeof(rsp_msg), MSG_DONTWAIT);
    } while(status > 0);

    status = send(fd, &req_msg, sizeof(req_msg), 0);
    if(status < 0) {
      printf("***Error: Failed to send message\n");
      break;
    }

    status = recv(fd, &rsp_msg, sizeof(rsp_msg), 0);
    if(status <= 0) {
      /* No response from management interface */
      printf("***Error: Connection lost\n");
      break;
    }

    /* Parse response message */
    parse_rsp_msg(api_code, &rsp_msg);
  }

  err = 0;

end:
  close(fd);

quick_end:
  printf("---Ended %s---\n", prog_name);

  return err;
}
