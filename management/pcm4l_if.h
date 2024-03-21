/**
 * @file pcm4l_if.h
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

#ifndef PCM4L_IF_H
#define PCM4L_IF_H

#include "../common/common.h"

/* pcm4l error codes */
typedef enum
{
  E_pcm4l_error_code_ok       = 0,
  E_pcm4l_error_code_timeout  = 1,
  E_pcm4l_error_code_fail     = 2,
  E_pcm4l_error_code_not_sent = 3
} T_pcm4l_error_code;

int pcm4l_if_start(const char *pcm4l_if_ip_addr, int pcm4l_if_port_num);
void pcm4l_if_stop(void);

T_pcm4l_error_code pcm4l_if_send(const unsigned char *req_buff, unsigned int req_len, unsigned char *rsp_buff, unsigned int rsp_len);

#endif  /* PCM4L_IF_H */
