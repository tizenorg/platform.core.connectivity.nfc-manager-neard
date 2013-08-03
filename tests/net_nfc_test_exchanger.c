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

#include "net_nfc_test_p2p.h"
#include "net_nfc_target_info.h"
#include "net_nfc_client_exchanger.h"
#include "net_nfc_test_exchanger.h"
#include "net_nfc_ndef_message.h"
#include "net_nfc_test_util.h"

static void run_next_callback(gpointer user_data);

static net_nfc_exchanger_data_h _net_nfc_test_create_exchgr_data();


static void run_next_callback(gpointer user_data)
{
	if (user_data)
	{
		GCallback callback;

		callback = (GCallback)(user_data);
		callback();
	}
}

static net_nfc_exchanger_data_h _net_nfc_test_create_exchgr_data()
{
	net_nfc_error_e result = NET_NFC_OK;
	ndef_message_h msg = NULL;
	data_h rawdata = NULL;
	net_nfc_exchanger_data_h exch_data = NULL;

	net_nfc_create_ndef_message(&msg);
	net_nfc_create_rawdata_from_ndef_message(msg, &rawdata);
	if((result = net_nfc_client_create_exchanger_data(&exch_data, rawdata)) != NET_NFC_OK)
	{
		net_nfc_free_data(rawdata);
		g_printerr(" Exchanger data creation failed \n");
		return NULL;
	}
	net_nfc_free_data(rawdata);
	return exch_data;
}

void net_nfc_test_create_exchanger_data(gpointer data,
								gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	ndef_message_h msg = NULL;
	data_h rawdata = NULL;
	net_nfc_exchanger_data_h exch_data = NULL;

	net_nfc_create_ndef_message(&msg);
	net_nfc_create_rawdata_from_ndef_message(msg, &rawdata);
	if((result = net_nfc_client_create_exchanger_data(&exch_data, rawdata)) != NET_NFC_OK)
	{
		net_nfc_free_data(rawdata);
		g_printerr(" Exchanger data creation failed \n");
		return;
	}
	net_nfc_free_data(rawdata);
	return;
}

void net_nfc_test_send_exchanger_data(gpointer data,
                                gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_h handle = NULL;
	net_nfc_exchanger_data_h exch_data = NULL;

	exch_data = _net_nfc_test_create_exchgr_data();
	handle = net_nfc_test_device_get_target_handle();

	result = net_nfc_client_send_exchanger_data(exch_data,
				handle,
				user_data);
	g_print("Send exchanger result : %d\n", result);
	run_next_callback(user_data);
}

void net_nfc_test_exchanger_request_connection_handover(gpointer data,
                                gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_conn_handover_carrier_type_e type = NET_NFC_CONN_HANDOVER_CARRIER_BT;
	net_nfc_target_handle_h handle = NULL;

	handle = net_nfc_test_device_get_target_handle();
	result = net_nfc_client_exchanger_request_connection_handover(handle, type);

	g_print(" Exchanger request connection handover : %d\n", result);
	run_next_callback(user_data);
}

void net_nfc_test_exchanger_get_alternative_carrier_type(gpointer data,
                                gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_connection_handover_info_h info = NULL;
	net_nfc_conn_handover_carrier_type_e type = NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;

	result = net_nfc_client_exchanger_get_alternative_carrier_type(info, &type);
	g_print(" Exchanger alternative carrier type and result : %d \n %d",type, result);
}

void net_nfc_test_exchanger_get_alternative_carrier_data(gpointer data,
                                gpointer user_data)
{
        net_nfc_error_e result = NET_NFC_OK;
        net_nfc_connection_handover_info_h info = NULL;
	data_h data_info = NULL;

        result = net_nfc_client_exchanger_get_alternative_carrier_data(info, &data_info);
        g_print(" Exchanger alternative carrier  result : %d \n",result);
	print_received_data(data_info);
}

