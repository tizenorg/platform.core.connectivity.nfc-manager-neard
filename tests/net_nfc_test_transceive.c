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

#include "net_nfc_client_transceive.h"
#include "net_nfc_test_transceive.h"
#include "net_nfc_test_util.h"
#include "net_nfc_target_info.h"
#include "net_nfc_typedef_internal.h"
#include "net_nfc_test_tag.h"


static void call_transceive_data_cb(net_nfc_error_e result,
		data_s *resp_data,
		void *user_data);

static void call_transceive_cb(net_nfc_error_e result,
		void* user_data);

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


static void call_transceive_cb(net_nfc_error_e result,
		void* user_data)
{
	g_print("call_transceive_cb Completed %d\n", result);

	run_next_callback(user_data);
}

static void call_transceive_data_cb(net_nfc_error_e result,
		data_s *resp_data,
		void *user_data)
{
	g_print("call_transceive_data_cb Completed %d\n", result);
	print_received_data(resp_data);

	run_next_callback(user_data);
}

void net_nfc_test_transceive(gpointer data, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	data_s raw_data = {NULL,};
	net_nfc_target_info_s *info = NULL;
	net_nfc_target_handle_s *handle = NULL;

	info = net_nfc_test_tag_get_target_info();

	net_nfc_get_tag_handle(info, &handle);

	result =net_nfc_client_transceive(handle,
			&raw_data,
			call_transceive_cb,
			user_data);
	g_print("net_nfc_client_transceive() : %d\n", result);
}

void net_nfc_test_transceive_sync(gpointer data, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	data_s raw_data = {NULL,};
	net_nfc_target_info_s *info = NULL;
	net_nfc_target_handle_s *handle = NULL;

	info = net_nfc_test_tag_get_target_info();

	net_nfc_get_tag_handle(info, &handle);

	result = net_nfc_client_transceive_sync(handle, &raw_data);

	g_print("Transceive Sync is completed(%d)\n", result);
}

void net_nfc_test_transceive_data(gpointer data, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	data_s raw_data = {NULL,};
	net_nfc_target_info_s *info = NULL;
	net_nfc_target_handle_s *handle = NULL;

	info = net_nfc_test_tag_get_target_info();

	net_nfc_get_tag_handle(info, &handle);

	result = net_nfc_client_transceive_data(handle,
			&raw_data,
			call_transceive_data_cb,
			user_data);
	g_print("net_nfc_client_transceive_data() : %d\n", result);
}

void net_nfc_test_transceive_data_sync(gpointer data, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	data_s raw_data = {NULL};
	data_s *response = NULL;
	net_nfc_target_info_s *info = NULL;
	net_nfc_target_handle_s *handle = NULL;

	info = net_nfc_test_tag_get_target_info();

	net_nfc_get_tag_handle(info, &handle);

	result = net_nfc_client_transceive_data_sync(handle, &raw_data, &response);
	g_print("net_nfc_client_transceive_data_sync() : %d\n", result);

	if (NET_NFC_OK == result)
		print_received_data(response);
}
