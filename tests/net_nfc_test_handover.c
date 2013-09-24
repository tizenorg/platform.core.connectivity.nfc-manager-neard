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

#include "net_nfc_client_handover.h"
#include "net_nfc_test_handover.h"
#include "net_nfc_test_util.h"
#include "net_nfc_target_info.h"
#include "net_nfc_typedef_internal.h"
#include "net_nfc_typedef.h"
#include "net_nfc_test_p2p.h"
#include "net_nfc_util_internal.h"


static void run_next_callback(gpointer user_data);

static void p2p_connection_handover_cb(net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_h data,
		void *user_data);

static net_nfc_connection_handover_info_h global_info = NULL;


static void run_next_callback(gpointer user_data)
{
	if (user_data)
	{
		GCallback callback;
		callback = (GCallback)(user_data);
		callback();
	}
}

static void p2p_connection_handover_cb(net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_h data,
		void *user_data)
{
	g_print("Connection handover completed\n");

	print_received_data(data);

	global_info->type = type;

	if(data->length > 0)
	{
		g_print("Callback has valid data \n");
		global_info->data.buffer = data->buffer;
		global_info->data.length = data->length;
		print_received_data(&global_info->data);
	}

	run_next_callback(user_data);
}

static void _p2p_connection_handover(
		net_nfc_conn_handover_carrier_type_e type, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_h handle = NULL;

	handle = net_nfc_test_device_get_target_handle();

	g_print("handle for handover : %p \n", handle);

	result = net_nfc_client_p2p_connection_handover(
		handle,
		type,
		p2p_connection_handover_cb,
		user_data);
	g_print("net_nfc_client_p2p_connection_handover() : %d\n", result);
}

static void _p2p_connection_handover_sync(
		net_nfc_conn_handover_carrier_type_e type, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_conn_handover_carrier_type_e out_carrier;
	data_h out_data = NULL;
	net_nfc_target_handle_h handle = NULL;

	handle = net_nfc_test_device_get_target_handle();

	result = net_nfc_client_p2p_connection_handover_sync(
			handle,
			type,
			&out_carrier,
			&out_data);
	g_print("net_nfc_client_p2p_connection_handover_sync() : %d\n", result);

	g_print("Received out carrier type & carrier type  %d, %d\n", out_carrier, type);
	print_received_data(out_data);
	run_next_callback(user_data);
}

void net_nfc_test_p2p_connection_handover_with_BT(gpointer data,
		gpointer user_data)
{
	_p2p_connection_handover(NET_NFC_CONN_HANDOVER_CARRIER_BT, user_data);
}

void net_nfc_test_p2p_connection_handover_with_WIFI(gpointer data,
		gpointer user_data)
{
	_p2p_connection_handover(NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS, user_data);
}

void net_nfc_test_p2p_connection_handover_with_BT_sync(gpointer data,
		gpointer user_data)
{
	_p2p_connection_handover_sync(NET_NFC_CONN_HANDOVER_CARRIER_BT, user_data);
}

void net_nfc_test_p2p_connection_handover_with_WIFI_sync(gpointer data,
		gpointer user_data)
{
	_p2p_connection_handover_sync(NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS, user_data);
}

void net_nfc_test_handover_get_alternative_carrier_type(gpointer data,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_conn_handover_carrier_type_e type;

	result = net_nfc_client_handover_get_alternative_carrier_type(global_info, &type);
	g_print("net_nfc_client_handover_get_alternative_carrier_type() : %d\n", result);
	g_print("Handover alternative carrier type -> %d", type);
}

void net_nfc_test_handover_handle_alternative_carrier_data(gpointer data,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	data_h out_data = NULL;
	net_nfc_connection_handover_info_h hand_info = NULL;

	result = net_nfc_client_handover_get_alternative_carrier_data(global_info, &out_data);
	g_print(" Get alternative carrier data  %d", result);
	print_received_data(out_data);

	hand_info = global_info;

	result = net_nfc_client_handover_free_alternative_carrier_data(hand_info);
	g_print("Free alternative carrier data	%d", result);
}
