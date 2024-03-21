/**
 * @file raw_socket.h
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

#ifndef RAW_SOCKET_H
#define RAW_SOCKET_H

int raw_socket_open(const char *name, T_port_num port_num, struct sockaddr_ll *mac_addr);
int raw_socket_send(int fd, void *msg, int msg_len, int flags, struct sockaddr_ll *dst_addr, int dst_addr_len);
int raw_socket_recv(int fd, void *msg, int msg_len, int flags, struct sockaddr_ll *src_addr, int src_addr_len);
int raw_socket_close(int fd);

#endif /* RAW_SOCKET_H */
