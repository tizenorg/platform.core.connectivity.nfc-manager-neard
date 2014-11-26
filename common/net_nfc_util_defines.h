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

#ifndef __NET_NFC_UTIL_DEFINES_H__
#define __NET_NFC_UTIL_DEFINES_H__

#define NET_NFC_UTIL_MSG_TYPE_REQUEST 0
#define NET_NFC_UTIL_MSG_TYPE_RESPONSE 1

#define CONN_HANDOVER_BT_CARRIER_MIME_NAME "application/vnd.bluetooth.ep.oob"
#define CONN_HANDOVER_WIFI_BSS_CARRIER_MIME_NAME "application/vnd.wfa.wsc"
#define CONN_HANDOVER_WIFI_IBSS_CARRIER_MIME_NAME "application/vnd.wfa.wsc;mode=ibss"

#define BLUETOOTH_ADDRESS_LENGTH 6
#define HIDDEN_BT_ADDR_FILE tzplatform_mkpath(TZ_SYS_ETC, ".bd_addr")

#endif //__NET_NFC_UTIL_DEFINES_H__
