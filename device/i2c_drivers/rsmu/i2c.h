/**
 * @file i2c.h
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
* Release Tag: 1-0-2
* Pipeline ID: 118059
* Commit Hash: 5a4424ad
********************************************************************************************************************/

#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2c_init(void);
void i2c_deinit(void);

uint32_t i2c_read(void *user_data,
                  uint32_t address_in_page,
                  uint8_t *buffer,
                  uint16_t number_of_bytes);
uint32_t i2c_write(void *user_data,
                   uint32_t address_in_page,
                   uint8_t const *buffer,
                   uint16_t number_of_bytes);

#endif /* I2C_H */
