/**
 * @file hash.h
 * @brief Implements a simple hash table.
 * @note Copyright (C) 2015 Richard Cochran <richardcochran@gmail.com>
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

#ifndef HASH_H
#define HASH_H

struct hash;

struct hash *hash_create(void);
void hash_destroy(struct hash *ht, void (*func)(void *));
int hash_insert(struct hash *ht, const char* key, void *data);
void *hash_lookup(struct hash *ht, const char* key);

#endif /* HASH_H */
