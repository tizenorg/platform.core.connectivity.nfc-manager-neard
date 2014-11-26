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


#include "net_nfc_test_jewel.h"
#include "net_nfc_typedef_internal.h"
#include "net_nfc_test_util.h"
#include "net_nfc_client_tag_jewel.h"
#include "net_nfc_target_info.h"
#include "net_nfc_test_tag.h"


static net_nfc_target_handle_s* get_handle()
{
	net_nfc_target_info_s *info = NULL;
	net_nfc_target_handle_s *handle = NULL;

	info = net_nfc_test_tag_get_target_info();

	net_nfc_get_tag_handle(info, &handle);

	return handle;
}

static void run_next_callback(gpointer user_data)
{
	if (user_data)
	{
		GCallback callback;

		callback = (GCallback)(user_data);

		callback();
	}
}


/*********************************** Callbacks *************************************/

static void jewel_read_cb(net_nfc_error_e result,
		data_s *resp_data,
		void *user_data)
{

	g_print("jewel_read_cb Completed %d\n", result);

	print_received_data(resp_data);

	run_next_callback(user_data);
}

static void jewel_write_cb(net_nfc_error_e result,
		void* user_data)
{
	g_print("jewel_write_cb Completed %d\n", result);

	run_next_callback(user_data);
}


/*********************************** Function Calls *************************************/


void net_nfc_test_tag_jewel_read_id(gpointer data,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;

	handle = get_handle();

	result = net_nfc_client_jewel_read_id(handle,
			jewel_read_cb,
			user_data);
	g_print("net_nfc_client_jewel_read_id() : %d\n", result);
}

void net_nfc_test_tag_jewel_read_byte(gpointer data,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;
	uint8_t block = 0x01;
	uint8_t byte = 1;

	handle = get_handle();

	result = net_nfc_client_jewel_read_byte(handle,
			block,
			byte,
			jewel_read_cb,
			user_data);
	g_print("net_nfc_client_jewel_read_byte() : %d\n", result);
}

void net_nfc_test_tag_jewel_read_all(gpointer data,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;

	handle = get_handle();

	result = net_nfc_client_jewel_read_all(handle,
			jewel_read_cb,
			user_data);
	g_print("net_nfc_client_jewel_read_all() : %d\n", result);
}

void net_nfc_test_tag_jewel_write_with_erase(gpointer data,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;
	uint8_t block = 0x01;
	uint8_t byte = 1;
	uint8_t data_to_write = 'A';

	handle = get_handle();

	result = net_nfc_client_jewel_write_with_erase(handle,
			block,
			byte,
			data_to_write,
			jewel_write_cb,
			user_data);
	g_print("net_nfc_client_jewel_write_with_erase() : %d\n", result);
}

void net_nfc_test_tag_jewel_write_with_no_erase(gpointer data,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;
	uint8_t block = 0x01;
	uint8_t byte = 1;
	uint8_t data_to_write = 'A';

	handle = get_handle();

	result = net_nfc_client_jewel_write_with_erase(handle,
			block,
			byte,
			data_to_write,
			jewel_write_cb,
			user_data);
	g_print("net_nfc_client_jewel_write_with_erase() : %d\n", result);
}
