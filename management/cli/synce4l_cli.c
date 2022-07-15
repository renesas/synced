/**
 * @file synce4l_cli.c
 * @brief synce4l command-line interface main program
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
* Release Tag: 1-0-1
* Pipeline ID: 113278
* Commit Hash: 8af68511
********************************************************************************************************************/

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include "../mng_if.h"
#include "../../common/common.h"
#include "../../common/print.h"

#define CLI_BUFF_SIZE   128

/* Static data */

static char cli_buffer[CLI_BUFF_SIZE];
static int print_flag;

/* Static functions */

static void print_help(void)
{
  int i;

  printf("\n=== Commands:\n");

  for(i = 0; i < E_mng_api_max; i++) {
    printf("\t%d: %s\n", i, conv_api_code_to_str(i));
  }
  printf("\tOther options: \"exit\", \"help\"\n");
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

/* Main function */

int main(int argc, char *argv[])
{
  (void)argc;
  char *prog_name = strrchr(argv[0], '/') + 1;
  E_mng_api api_code;
  int fd;
  struct sockaddr_in server;
  T_mng_api_request_msg req_msg;
  T_mng_api_response_msg rsp_msg;
  int status;

  printf("%s started\n", prog_name);

  if(argc <= 3) {
    printf("Must provide IP address, port number, and print flag (ex: 127.0.0.2, 2400, and 1)\n");
    return -1;
  }
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0) {
    printf("***Error: Failed to create socket\n");
    return -1;
  }

  printf("Socket created successfully (IP Address: %s, port number: %s, and print flag: %s)\n", argv[1], argv[2], argv[3]);

  /* Print supported commands */
  print_help();

  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons(atoi(argv[2]));
  print_flag = atoi(argv[3]);

  if(connect(fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
    printf("***Error: Failed to connect\n");
    close(fd);
    return -1;
  }
  
  printf("\nSuccessfully connected to socket\n");

  print_set_prog_name(prog_name);
  print_set_max_msg_level(LOG_DEBUG);
  print_set_stdout_en(1);

  while(1) {
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

    api_code = (E_mng_api)atoi(cli_buffer);

    if(api_code >= E_mng_api_max) {
      printf("***Error: Invalid request\n");
      continue;
    }

    /* Print API name */
    printf("\t%s\n", conv_api_code_to_str(api_code));

    req_msg.api_code = api_code;
    switch(api_code) {
      case E_mng_api_get_sync_info_list:
        req_msg.request_get_sync_info_list.print_flag = print_flag;
        req_msg.request_get_sync_info_list.max_num_syncs = MAX_SYNC_INFO_STRUCTURES;
        break;

      case E_mng_api_get_current_status:
        req_msg.request_get_current_status.print_flag = print_flag;
        break;

      case E_mng_api_get_sync_info:
        req_msg.request_get_sync_info.print_flag = print_flag;
        printf("\tport name: ");
        get_cli_string();
        cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
        strcpy(req_msg.request_get_sync_info.port_name, cli_buffer);
        break;

      case E_mng_api_set_forced_ql:
        req_msg.request_set_forced_ql.print_flag = print_flag;
        printf("\tport name: ");
        get_cli_string();
        cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
        strcpy(req_msg.request_set_forced_ql.port_name, cli_buffer);
        printf("\tforced QL: ");
        get_cli_string();
        req_msg.request_set_forced_ql.forced_ql = (T_esmc_ql)atoi(cli_buffer);
        break;

      case E_mng_api_clear_forced_ql:
        req_msg.request_clear_forced_ql.print_flag = print_flag;
        printf("\tport name: ");
        get_cli_string();
        cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
        strcpy(req_msg.request_clear_forced_ql.port_name, cli_buffer);
        break;

      case E_mng_api_clear_holdover_timer:
        req_msg.request_clear_holdover_timer.print_flag = print_flag;
        break;

      case E_mng_api_clear_synce_clk_wtr_timer:
        req_msg.request_clear_synce_clk_wtr_timer.print_flag = print_flag;
        printf("\tport name: ");
        get_cli_string();
        cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
        strcpy(req_msg.request_clear_synce_clk_wtr_timer.port_name, cli_buffer);
        break;

      case E_mng_api_assign_new_synce_clk_port:
        req_msg.request_assign_new_synce_clk_port.print_flag = print_flag;
        printf("\tport name: ");
        get_cli_string();
        cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
        strcpy(req_msg.request_assign_new_synce_clk_port.port_name, cli_buffer);
        printf("\tclock index: ");
        get_cli_string();
        req_msg.request_assign_new_synce_clk_port.clk_idx = atoi(cli_buffer);
        break;

      case E_mng_api_set_pri:
        req_msg.request_set_pri.print_flag = print_flag;
        printf("\tport name: ");
        get_cli_string();
        cli_buffer[INTERFACE_MAX_NAME_LEN - 1] = 0; /* Limit to maximum 15 characters + null */
        strcpy(req_msg.request_set_pri.port_name, cli_buffer);
        printf("\tpriority: ");
        get_cli_string();
        req_msg.request_set_pri.pri = (T_esmc_ql)atoi(cli_buffer);
        break;

      case E_mng_api_set_max_msg_lvl:
        req_msg.request_set_max_msg_lvl.print_flag = print_flag;
        printf("\tmaximum message level: ");
        get_cli_string();
        req_msg.request_set_max_msg_lvl.max_msg_lvl = (T_esmc_ql)atoi(cli_buffer);
        break;

      default:
        printf("***Error: Unknown API\n");
        continue;
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

    if(rsp_msg.api_code >= E_mng_api_max) {
      printf("***Error: Invalid response\n");
      continue;
    }

    if(api_code != rsp_msg.api_code) {
      printf("***Error: Requested '%s' and received '%s'\n",
             conv_api_code_to_str(api_code),
             conv_api_code_to_str(rsp_msg.api_code));
      continue;
    }

    if(rsp_msg.response != E_management_api_response_ok) {
      switch(rsp_msg.response) {
        case E_management_api_response_invalid:
          printf("\tInvalid response\n");
          break;

        case E_management_api_response_failed:
          printf("\tFailed response\n");
          break;

        case E_management_api_response_not_supported:
          printf("\tNot supported response\n");
          break;

        default:
          printf("\tUnknown response\n");
          break;
      }
      continue;
    }

    switch(rsp_msg.api_code) {
      case E_mng_api_get_sync_info_list:
        printf("\tSync info list:\n");
        {
          T_management_sync_info *sync_info = &rsp_msg.response_get_sync_info_list.sync_info_list[0];
          int i = rsp_msg.response_get_sync_info_list.num_syncs;

          if(i > MAX_SYNC_INFO_STRUCTURES) {
            printf("\t***Error: Invalid number of structures %d\n", i);
            i = MAX_SYNC_INFO_STRUCTURES;
          }

          while(i > 0) {
            printf("\t--------------\n");
            print_sync_info(sync_info);
            sync_info++;
            i--;
          }
        }
        break;

      case E_mng_api_get_current_status:
        printf("\tCurrent status:\n");
        {
          T_management_status *status = &rsp_msg.response_get_current_status.current_status;
          print_current_status(status);
        }
        break;

      case E_mng_api_get_sync_info:
        printf("\tSync info:\n");
        {
          T_management_sync_info *sync_info = &rsp_msg.response_get_sync_info.sync_info;
          print_sync_info(sync_info);
        }
        break;

      case E_mng_api_set_forced_ql:
        printf("\tSet forced QL was successful\n");
        break;

      case E_mng_api_clear_forced_ql:
        printf("\tClear forced QL was successful\n");
        break;

      case E_mng_api_clear_holdover_timer:
        printf("\tClear holdover timer was successful\n");
        break;

      case E_mng_api_clear_synce_clk_wtr_timer:
        printf("\tClear wait-to-restore timer was successful\n");
        break;

      case E_mng_api_assign_new_synce_clk_port:
        printf("\tAssign new Sync-E clock port was successful\n");
        break;

      case E_mng_api_set_pri:
        printf("\tSet priority was successful\n");
        break;

      case E_mng_api_set_max_msg_lvl:
        printf("\tSet maximum message level was successful\n");
        break;

      default:
        continue;
    }
  }

  close(fd);
  return 0;
}
