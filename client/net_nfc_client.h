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
#ifndef __NET_NFC_CLIENT_H__
#define __NET_NFC_CLIENT_H__

#include <glib.h>

typedef struct _NetNfcCallback
{
	void *callback;
	void *user_data;
}
NetNfcCallback;

void net_nfc_client_gdbus_init(void);

void net_nfc_client_gdbus_deinit(void);

GVariant *net_nfc_client_gdbus_get_privilege();

#endif //__NET_NFC_CLIENT_H__
