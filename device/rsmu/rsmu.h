/**
 * @file rsmu.h
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

#ifndef RSMU_H
#define RSMU_H

#include "../device_adaptor/device_adaptor.h"
#include "../../common/common.h"

#ifndef IDTSMU_MAGIC
#define MAX_NUM_PRIORITY_ENTRIES 32

struct rsmu_get_state
{
  __u8 dpll;
  __u8 state;
};

struct rsmu_current_clock_index
{
  __u8 dpll;
  __s8 clock_index;
};

struct rsmu_priority_entry
{
  __u8 clock_index;
  __u8 priority;
};

struct rsmu_clock_priorities
{
  __u8 dpll;
  __u8 num_entries;
  struct rsmu_priority_entry priority_entry[MAX_NUM_PRIORITY_ENTRIES];
};

struct rsmu_reference_monitor_status_alarms
{
  __u8 los;
  __u8 no_activity;
  __u8 frequency_offset_limit;
};

struct rsmu_reference_monitor_status
{
  __u8 clock_index;
  struct rsmu_reference_monitor_status_alarms alarms;
};

#define RSMU_MAGIC   '?'

#define RSMU_GET_STATE                      _IOR(RSMU_MAGIC, 2, struct rsmu_get_state)
#define RSMU_GET_CURRENT_CLOCK_INDEX        _IOR(RSMU_MAGIC, 6, struct rsmu_current_clock_index)
#define RSMU_SET_CLOCK_PRIORITIES           _IOW(RSMU_MAGIC, 7, struct rsmu_clock_priorities)
#define RSMU_GET_REFERENCE_MONITOR_STATUS   _IOR(RSMU_MAGIC, 8, struct rsmu_reference_monitor_status)
#endif /* IDTSMU_MAGIC */

void rsmu_register_callbacks(T_device_adaptor_callbacks *device_adaptor_callbacks);

#endif /* RSMU_H */
