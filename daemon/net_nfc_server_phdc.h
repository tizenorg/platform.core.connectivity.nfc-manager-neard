/*
* Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*					http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
 */
#ifndef __NET_NFC_SERVER_PHDC_H__
#define __NET_NFC_SERVER_PHDC_H__

#include <gio/gio.h>
#include "net_nfc_typedef_internal.h"

gboolean net_nfc_server_phdc_init(GDBusConnection *connection);

void net_nfc_server_phdc_deinit(void);

/* server side */
void net_nfc_server_phdc_transport_disconnect_indication(void);

void net_nfc_server_phdc_transport_connect_indication(net_nfc_phdc_handle_h handle);

void net_nfc_server_phdc_data_received_indication(data_s *arg_data);

void net_nfc_server_phdc_data_sent(net_nfc_error_e result, gpointer user_data);

#endif //__NET_NFC_SERVER_PHDC_H__