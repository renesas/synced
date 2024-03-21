/**
 * @file pcm4l_msg.h
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

#ifndef PCM4L_MSG_H
#define PCM4L_MSG_H

#include "pcm4l_if.h"
#include "../common/common.h"

/* pcm4l API codes */
typedef enum {
  E_pcm4l_api_set_clock_category = 97
} T_pcm4l_api;

/* Physical clock categories */
typedef enum
{
  E_physical_clock_category_1       = 1,
  E_physical_clock_category_2       = 2,
  E_physical_clock_category_3       = 3,
  E_physical_clock_category_4       = 4,
  E_physical_clock_category_DNU     = 5, /* Do not use */
  E_physical_clock_category_INVALID = 6
} T_physical_clock_category;

typedef union {
  T_physical_clock_category clock_category;
} T_pcm4l_api_data;

/* pcm4l message */
typedef struct
{
  T_pcm4l_api api_code;
  T_pcm4l_error_code error_code;
  T_pcm4l_api_data data;
} T_pcm4l_message;

T_pcm4l_error_code pcm4l_msg_set_clock_category(T_esmc_ql ql, int wait_for_response);

#endif  /* PCM4L_MSG_H */
