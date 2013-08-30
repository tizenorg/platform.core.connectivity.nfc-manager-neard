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

#include <glib-object.h>

#include "net_nfc_test_sys_handler.h"
#include "net_nfc_typedef_internal.h"
#include "net_nfc_client_system_handler.h"
#include "net_nfc_target_info.h"


/*************************** utility Calls ******************************/

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

static void sys_handler_cb(net_nfc_error_e result,
				void *user_data)
{
	g_print("sys_handler_cb Set popup state completed %d\n", result);

	run_next_callback(user_data);
}

/********************** Function Calls ******************************/
void net_nfc_test_sys_handler_set_launch_popup_state(gpointer data,
				gpointer user_data)
{
	net_nfc_error_e result;
	int enable = 1;

	result = net_nfc_client_sys_handler_set_state(enable,sys_handler_cb,user_data);

	if(result != NET_NFC_OK)
	{
		g_print("System handler set launch popup state failed: %d\n", result);
		return;
	}

	g_print("System handler set launch popup state success: %d\n", result);
}


void net_nfc_test_sys_handler_set_launch_popup_state_sync(gpointer data,
				gpointer user_data)
{
	net_nfc_error_e result;

	result = net_nfc_client_sys_handler_set_state_sync(NET_NFC_LAUNCH_APP_SELECT);

	if(result != NET_NFC_OK)
	{
		g_print("System handler set launch popup state failed: %d\n", result);
		return;
	}
	else
	{
		g_print("System handler set launch popup state success: %d\n", result);
	}

	run_next_callback(user_data);
}

void net_nfc_test_sys_handler_set_launch_popup_state_force(gpointer data,
				gpointer user_data)
{
	net_nfc_error_e result;
	int enable = 1;

	result = net_nfc_client_sys_handler_set_state_force(enable,sys_handler_cb,user_data);

	if(result != NET_NFC_OK)
	{
		g_print("net_nfc_test_sys_handler_set_launch_popup_state_force failed: %d\n", result);
		return;
	}

	g_print("net_nfc_test_sys_handler_set_launch_popup_state_force success: %d\n", result);
}


void net_nfc_test_sys_handler_set_launch_popup_state_force_sync(gpointer data,
				gpointer user_data)
{
	net_nfc_error_e result;

	result = net_nfc_client_sys_handler_set_state_force_sync(NET_NFC_LAUNCH_APP_SELECT);

	if(result != NET_NFC_OK)
	{
		g_print("System handler set launch popup state failed: %d\n", result);
		return;
	}
	else
	{
		g_print("System handler set launch popup state success: %d\n", result);
	}

	run_next_callback(user_data);
}


void net_nfc_test_sys_handler_get_launch_popup_state(gpointer data,
		gpointer user_data)
{
	net_nfc_error_e result;
	int state = 0;

	result = net_nfc_client_sys_handler_get_launch_popup_state(&state);

	if(result != NET_NFC_OK)
	{
		g_print("System handler get launch popup state failed: %d\n", result);
	}
	else
	{
		g_print("System handler get launch popup state success: %d\n", result);
		g_print("System handler get launch popup state value: %d\n", state);
	}

	run_next_callback(user_data);
}
