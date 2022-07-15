/**
 * @file device_adaptor.c
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

#include <pthread.h>

#include "device_adaptor.h"
#include "../../common/os.h"
#include "../../common/print.h"

#include DEVICE_DRIVER_PATH(idt_clockmatrix_register_commons.h)
#include DEVICE_DRIVER_PATH(idt_clockmatrix_api/idt_clockmatrix_api.h)
#include DEVICE_DRIVER_PATH(idt_clockmatrix_api/idt_clockmatrix_isr_api.h)
#include DEVICE_I2C_DRIVER_PATH(i2c.h)

#define CM_MAX_DPLL_IDX   7

#define CM_MAX_PRIORITY   18

#define CM_DEFAULT_PRIORITY_GROUP   0
#define CM_MAX_PRIORITY_GROUP       3

#define CM_REF_MON_FREQ_OFF_LIM_LIVE_LSB   2
#define CM_REF_MON_NO_ACT_LIVE_LSB         1
#define CM_REF_MON_LOS_LIVE_LSB            0

/* Static data */

static pthread_mutex_t g_device_mutex;
static Channel_id_t g_device_synce_dpll_ch_id;

static const DEV_driver_t g_device_driver = {
  .read = &i2c_read,
  .write = &i2c_write,
  .protect = NULL,
  .wait = NULL
};

/* Global functions */

int device_init(int synce_dpll_idx, const char *tcs_file)
{
  if(synce_dpll_idx > CM_MAX_DPLL_IDX) {
    pr_err("Invalid Sync-E DPLL index");
    return -1;
  }

  /* Initialize I2C */
  i2c_init();

  /* Register I2C read/write functions */
  if(PLL_init(0, 1, &g_device_driver, NULL) != IDT_EOK) {
    return -1;
  }

#if (SYNCE4L_DEBUG_MODE == 1)
  /* SCSR_RESET_CTRL.SCSR_BOOT_CMD */
  idt_clockmatrix_writeRegisterWord_32(0x2010C00C, 0x80000300);
  /* MISC_CTRL.BLOCK_RESET_AND_CG_ENABLE */
  idt_clockmatrix_writeRegisterWord_8(0x20108180, 0x01);
  /* MISC_CTRL.SYS_FORCE_RESET */
  idt_clockmatrix_writeRegisterWord_8(0x2010818F, 0x03);
#endif

  g_device_synce_dpll_ch_id = (Channel_id_t)synce_dpll_idx;

  /* Configure device */
  PLL_reg_static_config(tcs_file);

  /* Initialize mutex */
  return os_mutex_init(&g_device_mutex);
}

int device_get_current_clk_idx(void)
{
  PLL_ref_id_t ref_sel;
  idt_status_t status;
  
  os_mutex_lock(&g_device_mutex);
  status = PLL_get_dpll_mode_ref_sel(g_device_synce_dpll_ch_id, &ref_sel);
  os_mutex_unlock(&g_device_mutex);

  if((status != IDT_EOK) || (ref_sel > PLL_REF_ID_15)) {
    /* Freerun or holdover */
    return INVALID_CLK_IDX;
  }

  return (int)ref_sel;
}

int device_set_clock_priorities(T_device_clock_priority_table const *table)
{
  /* Received ranked clock priority table */

  int entries = (table->entries > CM_MAX_PRIORITY) ? CM_MAX_PRIORITY : table->entries;
  uint8_t i;
  idt_status_t status;
  T_device_clock_priority_entry *priority_entry = table->clock_priority_table;
  bool trigger = false;

  int prev_priority_group = 0;
  uint8_t current_priority_group;
  int prev_rank = -1;

  /* Write clock priority table starting with highest priority */
  for(i = 0; i < entries; i++) {
    if(priority_entry->rank == prev_rank) {
      current_priority_group = prev_priority_group;
    } else if((i < (entries - 1)) && (priority_entry->rank == (priority_entry + 1)->rank)) {
      if(prev_priority_group < CM_MAX_PRIORITY_GROUP) {
        prev_priority_group++;
        current_priority_group = prev_priority_group;
      } else {
        current_priority_group = CM_DEFAULT_PRIORITY_GROUP;
      }
    } else {
      current_priority_group = CM_DEFAULT_PRIORITY_GROUP;
    }

    prev_rank = priority_entry->rank;

    /* Write to trigger if clock priority table is full */
    if(i == (CM_MAX_PRIORITY - 1)) {
      trigger = true;
    }

    os_mutex_lock(&g_device_mutex);
    status = PLL_set_dpll_ref_priority(g_device_synce_dpll_ch_id,
                                       i,                                     /* Priority index */
                                       true,                                  /* Enable */
                                       (PLL_ref_id_t)priority_entry->clk_idx,
                                       current_priority_group,
                                       trigger);
    os_mutex_unlock(&g_device_mutex);

    if(status != IDT_EOK) {
      return -1;
    }

    priority_entry++;
  }

  /* Continue to disable other entries */
  for(; i < CM_MAX_PRIORITY; i++) {
    /* Write to trigger if clock priority table is full */
    if(i == (CM_MAX_PRIORITY - 1)) {
      trigger = true;
    }

    os_mutex_lock(&g_device_mutex);
    status = PLL_set_dpll_ref_priority(g_device_synce_dpll_ch_id,
                                       i,                         /* Priority index */
                                       false,                     /* Disable */
                                       (PLL_ref_id_t)0,
                                       CM_DEFAULT_PRIORITY_GROUP,
                                       trigger);
    os_mutex_unlock(&g_device_mutex);

    if(status != IDT_EOK) {
      return -1;
    }
  }

  return 0;
}

int device_get_reference_monitor_status(int clk_idx, T_device_clk_reference_monitor_status *ref_mon_status)
{
  idt_status_t status;
  unsigned char reg_val;

  os_mutex_lock(&g_device_mutex);
  status = PLL_ref_mon_status_read((PLL_ref_id_t)clk_idx, &reg_val);
  os_mutex_unlock(&g_device_mutex);

  if(status != IDT_EOK) {
    ref_mon_status->frequency_offset_live_status = 1;
    ref_mon_status->no_activity_live_status = 1;
    ref_mon_status->loss_of_signal_live_status = 1;
    return -1;
  }

  ref_mon_status->frequency_offset_live_status = (reg_val >> CM_REF_MON_FREQ_OFF_LIM_LIVE_LSB) & 1;
  ref_mon_status->no_activity_live_status = (reg_val >> CM_REF_MON_NO_ACT_LIVE_LSB) & 1;
  ref_mon_status->loss_of_signal_live_status = (reg_val >> CM_REF_MON_LOS_LIVE_LSB) & 1;

  return 0;
}

T_device_dpll_state device_get_synce_dpll_state(void)
{
  idt_status_t status;
  PLL_dpll_state_t dpll_state;
  T_device_dpll_state synce_dpll_state;

  os_mutex_lock(&g_device_mutex);
  status = PLL_get_dpll_state(g_device_synce_dpll_ch_id, &dpll_state);
  os_mutex_unlock(&g_device_mutex);

  if(status != IDT_EOK) {
    return E_device_dpll_state_max;
  }

  switch(dpll_state) {
    case PLL_DPLL_STATE_FREERUN:
    case PLL_DPLL_STATE_OPENLOOP:
      synce_dpll_state = E_device_dpll_state_freerun;
      break;

    case PLL_DPLL_STATE_LOCKACQ:
      synce_dpll_state = E_device_dpll_state_lock_acquisition;
      break;
    case PLL_DPLL_STATE_LOCKREC:
      synce_dpll_state = E_device_dpll_state_lock_recovery;
      break;
    case PLL_DPLL_STATE_LOCKED:
      synce_dpll_state = E_device_dpll_state_locked;
      break;

    case PLL_DPLL_STATE_HOLDOVER:
      synce_dpll_state = E_device_dpll_state_holdover;
      break;

    default:
      synce_dpll_state = E_device_dpll_state_max;
      break;
  }

  return synce_dpll_state;
}

void device_deinit(void)
{
  g_device_synce_dpll_ch_id = CHANNEL_ID_MAX;

  /* Deinitialize mutex */
  os_mutex_deinit(&g_device_mutex);

  i2c_deinit();
}
