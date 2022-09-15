/**
 * @file i2c.c
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
* Release Tag: 1-0-3
* Pipeline ID: 123302
* Commit Hash: 0d4d9ea7
********************************************************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <linux/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "i2c.h"
#include "../../../common/print.h"
#include "../../../common/types.h"

#define RSMU_MAGIC   '?'

#define RSMU_REG_READ    _IOR(RSMU_MAGIC, 100, struct rsmu_reg_rw)
#define RSMU_REG_WRITE   _IOR(RSMU_MAGIC, 101, struct rsmu_reg_rw)

struct rsmu_reg_rw {
  __u32 offset;
  __u8 byte_count;
  __u8 bytes[256];
};

/* Static data */

static int g_fd = UNINITIALIZED_FD;

/* Static functions */

/* Global functions */

void i2c_init(const char *i2c_device_name)
{
  /* Open device */
  g_fd = open(i2c_device_name, O_WRONLY);
  if(g_fd < 0) {
    pr_err("Failed to open I2C device %s: %s", i2c_device_name, strerror(errno));
    return;
  }
  pr_info("Opened I2C device %s", i2c_device_name);
}

void i2c_deinit(void)
{
  /* Close device */
  if(g_fd > 0) {
    close(g_fd);
    g_fd = UNINITIALIZED_FD;
    pr_info("Closed I2C device");
  }
}

uint32_t i2c_read(void *user_data,
                  uint32_t address_in_page,
                  uint8_t *buffer,
                  uint16_t number_of_bytes)
{
  (void)user_data;

  uint32_t offset = address_in_page;
  uint8_t byte_count;
  struct rsmu_reg_rw get = {0};
  uint16_t i = 0;

  while(number_of_bytes > 0) {
    byte_count = (number_of_bytes > 255) ? 255 : number_of_bytes;

    get.offset = address_in_page;
    get.byte_count = byte_count;

    if(ioctl(g_fd, RSMU_REG_READ, &get) < 0) {
      pr_err("%s failed: %s", __func__, strerror(errno));
      return 0;
    }

    memcpy(buffer + i, get.bytes, byte_count);
    i += byte_count; 
    offset += byte_count;
    number_of_bytes -= byte_count;
  }

  return 0;
}

uint32_t i2c_write(void *user_data,
                   uint32_t address_in_page,
                   uint8_t const *buffer,
                   uint16_t number_of_bytes)
{
  (void)user_data;

  uint32_t offset = address_in_page;
  uint8_t byte_count;
  struct rsmu_reg_rw set = {0};
  uint16_t i = 0;

  while(byte_count > 0) {
    byte_count = (number_of_bytes > 255) ? 255 : number_of_bytes;

    set.offset = offset;
    set.byte_count = byte_count;

    memcpy(set.bytes, buffer + i, byte_count);

    if(ioctl(g_fd, RSMU_REG_WRITE, &set) < 0) {
      pr_err("%s failed: %s", __func__, strerror(errno));
      return 0;
    }

    i += byte_count; 
    offset += byte_count;
    number_of_bytes -= byte_count;
  }

  return 0;
}
