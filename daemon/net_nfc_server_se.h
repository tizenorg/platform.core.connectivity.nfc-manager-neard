/*
 * Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __NET_NFC_SERVER_SE_H__
#define __NET_NFC_SERVER_SE_H__

#include <gio/gio.h>

#include "net_nfc_typedef_internal.h"


/***************************************************************/

uint8_t net_nfc_server_se_get_se_type();

uint8_t net_nfc_server_se_get_se_mode();

net_nfc_error_e net_nfc_server_se_change_se(uint8_t type);

net_nfc_error_e net_nfc_server_se_disable_card_emulation();

/***************************************************************/

gboolean net_nfc_server_se_init(GDBusConnection *connection);

void net_nfc_server_se_deinit(void);

void net_nfc_server_se_detected(void *info);

void net_nfc_server_se_transaction_received(void *info);

#endif //__NET_NFC_SERVER_SE_H__
