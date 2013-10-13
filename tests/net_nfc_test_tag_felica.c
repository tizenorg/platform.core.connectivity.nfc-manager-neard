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

#include "net_nfc_test_tag_felica.h"
#include "net_nfc_typedef_internal.h"
#include "net_nfc_test_tag.h"
#include "net_nfc_target_info.h"
#include "net_nfc_test_util.h"
#include "net_nfc_client_tag_felica.h"


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

static void felica_cb(net_nfc_error_e result, data_s *resp_data, void *user_data)
{
	g_print("felica_cb Completed %d\n", result);

	print_received_data(resp_data);

	run_next_callback(user_data);
}

void net_nfc_test_felica_poll(gpointer data, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;
	net_nfc_felica_poll_request_code_e req_code = 0x00;
	uint8_t time_slot = 2;

	handle = get_handle();
	if (handle == NULL)
		return ;

	result = net_nfc_client_felica_poll(handle, req_code, time_slot, felica_cb, user_data);

	g_print("net_nfc_client_felica_poll() : %d\n", result);
}

void net_nfc_test_felica_request_service(gpointer data, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;
	uint8_t number_of_area_service = 4;
	uint16_t area_service_list[10] = { 0,};
	uint8_t number_of_services = 5;

	handle = get_handle();
	if (handle == NULL)
		return ;

	result = net_nfc_client_felica_request_service(handle,
			number_of_area_service,
			area_service_list,
			number_of_services,
			felica_cb,
			user_data);
	g_print("net_nfc_client_felica_request_service() : %d\n", result);
}

void net_nfc_test_felica_request_response(gpointer data, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;

	handle = get_handle();
	if (handle == NULL)
		return ;

	result = net_nfc_client_felica_request_response(handle, felica_cb, user_data);
	g_print("net_nfc_client_felica_request_response() : %d\n", result);
}

void net_nfc_test_felica_read_without_encryption(gpointer data,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;
	uint8_t number_of_services = 10;
	uint16_t service_list[10] = {0,};
	uint8_t number_of_blocks = 1;
	uint8_t block_list[3] = {0,};

	handle = get_handle();
	if (handle == NULL)
		return ;

	result = net_nfc_client_felica_read_without_encryption(handle,
			number_of_services,
			service_list,
			number_of_blocks,
			block_list,
			felica_cb,
			user_data);
	g_print("net_nfc_client_felica_read_without_encryption() : %d\n", result);
}

void net_nfc_test_felica_write_without_encryption(gpointer data,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;
	uint8_t number_of_services = 10;
	uint16_t service_list[10] = {0,};
	uint8_t number_of_blocks = 1;
	uint8_t block_list[3] = {0,};
	data_s *data_to_write = NULL;

	handle = get_handle();
	if (handle == NULL)
		return ;

	result = net_nfc_client_felica_write_without_encryption(handle,
			number_of_services,
			service_list,
			number_of_blocks,
			block_list,
			data_to_write,
			felica_cb,
			user_data);
	g_print("net_nfc_client_felica_write_without_encryption() : %d\n", result);
}

void net_nfc_test_felica_request_system_code(gpointer data, gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_handle_s *handle = NULL;

	handle = get_handle();
	if (handle == NULL)
		return ;

	result = net_nfc_client_felica_request_system_code(handle, felica_cb, user_data);
	g_print("net_nfc_client_felica_request_system_code() : %d\n", result);
}
