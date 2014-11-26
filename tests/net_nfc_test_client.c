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


#include <glib-object.h>

#include "net_nfc_test_client.h"
#include "net_nfc_client_context.h"
#include "net_nfc_test_client.h"
#include "net_nfc_typedef_internal.h"


static void run_next_callback(gpointer user_data);

static void run_next_callback(gpointer user_data)
{
	if (user_data)
	{
		GCallback callback;

		callback = (GCallback)(user_data);
		callback();
	}
}

void net_nfc_test_initialize(gpointer data, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;

	result = net_nfc_client_initialize();

	if(result != NET_NFC_OK)
	{
		g_print("Client Initialization failed & Result is %d\n", result);
		return;
	}

	g_print("Client Initialization Completed & Result is %d\n", result);

	run_next_callback(user_data);
}

void net_nfc_test_deinitialize(gpointer data, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;

	result = net_nfc_client_deinitialize();

	g_print("Client Deinitialization Completed & Result is %d\n", result);
}

void net_nfc_test_is_nfc_supported(gpointer data, gpointer user_data)
{
	int feature = 0;
	net_nfc_error_e result = NET_NFC_OK;

	result = net_nfc_client_is_nfc_supported(&feature);

	if((result != NET_NFC_OK) || (feature != 1))
	{
		g_print("Client is_nfc_supported failed & result is %d\n", result);
		g_print("Client is_nfc_supported failed & feature value is %d\n", feature);
		return;
	}

	g_print("Client is_nfc_supported completed & feature value is %d\n", feature);
	g_print("Client is_nfc_supported completed & result is %d\n", result);

	run_next_callback(user_data);
}

void net_nfc_test_get_nfc_state(gpointer data, gpointer user_data)
{
	int state = 0;
	net_nfc_error_e result = NET_NFC_OK;

	result = net_nfc_client_get_nfc_state(&state);

	if((result != NET_NFC_OK) || (state != 1))
	{
		g_print("Client get_nfc_state failed & result is %d\n", result);
		g_print("Client get_nfc_state failed & state value is %d\n", state);
		return;
	}

	g_print("Client get_nfc_state completed & state value is %d\n", state);
	g_print("Client get_nfc_state completed & result is %d\n", result);

	run_next_callback(user_data);
}
