/*
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 				 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __NET_NFC_SERVER_MANAGER_H__
#define __NET_NFC_SERVER_MANAGER_H__

#include <gio/gio.h>

gboolean net_nfc_server_manager_init(GDBusConnection *connection);

void net_nfc_server_manager_deinit(void);

void net_nfc_server_manager_set_active(gboolean is_active);

bool net_nfc_server_manager_get_active();

#endif //__NET_NFC_SERVER_MANAGER_H__
