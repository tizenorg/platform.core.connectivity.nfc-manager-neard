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

#include <glib.h>
#include <string.h>

#include "net_nfc_client_tag_felica.h"
#include "net_nfc_client_tag_internal.h"

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_target_info.h"

#define FELICA_CMD_POLL 0x00
#define FELICA_CMD_REQ_SERVICE 0x02
#define FELICA_CMD_REQ_RESPONSE 0x04
#define FELICA_CMD_READ_WITHOUT_ENC 0x06
#define FELICA_CMD_WRITE_WITHOUT_ENC 0x08
#define FELICA_CMD_REQ_SYSTEM_CODE 0x0C
#define FELICA_TAG_KEY	"IDm"

API net_nfc_error_e net_nfc_client_felica_poll(net_nfc_target_handle_s *handle,
		net_nfc_felica_poll_request_code_e req_code,
		uint8_t time_slote,
		nfc_transceive_data_callback callback,
		void *user_data)
{
	data_s rawdata;
	uint8_t send_buffer[6] = { 0x00, };
	net_nfc_target_info_s* target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;

	if (target_info->devType != NET_NFC_FELICA_PICC)
	{
		NFC_ERR("only felica tag is available(TAG = %d)", target_info->devType);
		return NET_NFC_NOT_ALLOWED_OPERATION;
	}

	/* total size of requet command */
	send_buffer[0] = 0x06;
	send_buffer[1] = FELICA_CMD_POLL;

	/* use wild card for system code */
	send_buffer[2] = 0xff;
	send_buffer[3] = 0xff;

	send_buffer[4] = req_code;
	send_buffer[5] = time_slote;

	DEBUG_MSG_PRINT_BUFFER(send_buffer, 6);

	rawdata.buffer = send_buffer;
	rawdata.length = 6;

	return net_nfc_client_transceive_data(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_felica_request_service(
		net_nfc_target_handle_s *handle,
		uint8_t number_of_area_service,
		uint16_t area_service_list[],
		uint8_t number_of_services,
		nfc_transceive_data_callback callback,
		void *user_data)
{
	int i;
	data_s rawdata;
	data_s *IDm = NULL;
	uint8_t* temp = NULL;
	uint8_t* send_buffer = NULL;
	uint32_t send_buffer_length;
	net_nfc_target_info_s* target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == area_service_list, NET_NFC_NULL_PARAMETER);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;

	if (target_info->devType != NET_NFC_FELICA_PICC)
	{
		NFC_ERR("only Felica tag is available(TAG=%d)", target_info->devType);
		return NET_NFC_NOT_ALLOWED_OPERATION;
	}

	if (net_nfc_get_tag_info_value(target_info, FELICA_TAG_KEY, &IDm) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	if (IDm->length != 8)
		return NET_NFC_OUT_OF_BOUND;

	if (number_of_area_service > 32)
		return NET_NFC_OUT_OF_BOUND;

	/* size + cmd + UID + number of service service count + service list */
	send_buffer_length = 1 + 1 + 8 + 1 + (2 * number_of_services);

	_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
	if (NULL == send_buffer)
		return NET_NFC_ALLOC_FAIL;

	temp = send_buffer;

	/* set cmd length */
	*send_buffer = send_buffer_length;
	send_buffer++;

	/* set cmd */
	*send_buffer = FELICA_CMD_REQ_SERVICE;
	send_buffer++;

	/* set IDm */
	memcpy(send_buffer, IDm->buffer, IDm->length);
	send_buffer = send_buffer + IDm->length;

	/* set the number of service codes */
	*send_buffer = number_of_area_service;
	send_buffer++;

	for (i = 0; i < number_of_services; i++)
	{
		memcpy(send_buffer, &area_service_list[i], sizeof(uint16_t));
		send_buffer = send_buffer + 2;
	}

	DEBUG_MSG_PRINT_BUFFER(temp, send_buffer_length);

	rawdata.buffer = send_buffer;
	rawdata.length = send_buffer_length;

	net_nfc_error_e result = NET_NFC_OK;
	result = net_nfc_client_transceive_data(handle, &rawdata, callback, user_data);

	if (temp != NULL)
		_net_nfc_util_free_mem(temp);

	return result;
}

API net_nfc_error_e net_nfc_client_felica_request_response(
		net_nfc_target_handle_s *handle,
		nfc_transceive_data_callback callback,
		void *user_data)
{
	data_s rawdata;
	data_s *IDm = NULL;
	uint8_t send_buffer[10] = { 0x00, };
	net_nfc_target_info_s* target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;

	if (target_info->devType != NET_NFC_FELICA_PICC)
	{
		NFC_ERR("only Felica tag is available(TAG=%d)", target_info->devType);
		return NET_NFC_NOT_ALLOWED_OPERATION;
	}

	if (net_nfc_get_tag_info_value(target_info, FELICA_TAG_KEY, &IDm) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	if (IDm->length != 8)
		return NET_NFC_OUT_OF_BOUND;

	send_buffer[0] = 0xA;
	send_buffer[1] = FELICA_CMD_REQ_RESPONSE;

	memcpy(send_buffer + 2, IDm->buffer, IDm->length);

	DEBUG_MSG_PRINT_BUFFER(send_buffer, 10);

	rawdata.buffer = send_buffer;
	rawdata.length = 10;

	return net_nfc_client_transceive_data(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_felica_read_without_encryption(
		net_nfc_target_handle_s *handle,
		uint8_t number_of_services,
		uint16_t service_list[],
		uint8_t number_of_blocks,
		uint8_t block_list[],
		nfc_transceive_data_callback callback,
		void *user_data)
{
	int i;
	data_s rawdata;
	data_s *IDm = NULL;
	uint8_t* temp = NULL;
	uint8_t* send_buffer = NULL;
	uint32_t send_buffer_length;
	net_nfc_target_info_s* target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == service_list, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == block_list, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;

	if (target_info->devType != NET_NFC_FELICA_PICC)
	{
		NFC_ERR("only Felica tag is available(TAG=%d)", target_info->devType);
		return NET_NFC_NOT_ALLOWED_OPERATION;
	}

	if (net_nfc_get_tag_info_value(target_info, FELICA_TAG_KEY, &IDm) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	if (IDm->length != 8)
		return NET_NFC_OUT_OF_BOUND;

	if (number_of_services > 16)
		return NET_NFC_OUT_OF_BOUND;

	send_buffer_length = 1 + 1 + 8 + 1 + (2 * number_of_services) + 1 + number_of_blocks;

	_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));

	if (send_buffer == NULL)
		return NET_NFC_ALLOC_FAIL;

	temp = send_buffer;

	*send_buffer = send_buffer_length;
	send_buffer++;

	*send_buffer = FELICA_CMD_READ_WITHOUT_ENC;
	send_buffer++;

	memcpy(send_buffer, IDm->buffer, IDm->length);
	send_buffer = send_buffer + IDm->length;

	*send_buffer = number_of_services;
	send_buffer++;

	for (i = 0; i < number_of_services; i++)
	{
		memcpy(send_buffer, &service_list[i], sizeof(uint16_t));
		send_buffer = send_buffer + 2;
	}

	*send_buffer = number_of_blocks;
	send_buffer++;

	for (i = 0; i < number_of_blocks; i++)
	{
		memcpy(send_buffer, &block_list[i], sizeof(uint8_t));
		send_buffer++;
	}

	DEBUG_MSG_PRINT_BUFFER(temp, send_buffer_length);

	rawdata.buffer = temp;
	rawdata.length = send_buffer_length;

	net_nfc_error_e result = NET_NFC_OK;
	result = net_nfc_client_transceive_data(handle, &rawdata, callback, user_data);

	if (temp != NULL)
		_net_nfc_util_free_mem(temp);

	return result;
}

API net_nfc_error_e net_nfc_client_felica_write_without_encryption(
		net_nfc_target_handle_s *handle,
		uint8_t number_of_services,
		uint16_t service_list[],
		uint8_t number_of_blocks,
		uint8_t block_list[],
		data_s *data,
		nfc_transceive_data_callback callback,
		void *user_data)
{
	int i;
	data_s rawdata;
	data_s *IDm = NULL;
	uint8_t* temp = NULL;
	uint8_t* send_buffer = NULL;
	uint32_t send_buffer_length;
	net_nfc_target_info_s* target_info = NULL;

	RETV_IF(NULL == data, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == block_list, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == service_list, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;

	if (target_info->devType != NET_NFC_FELICA_PICC)
	{
		NFC_ERR("only Felica tag is available(TAG=%d)", target_info->devType);
		return NET_NFC_NOT_ALLOWED_OPERATION;
	}

	if (net_nfc_get_tag_info_value(target_info, FELICA_TAG_KEY, &IDm) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	if (IDm->length != 8)
		return NET_NFC_OUT_OF_BOUND;

	if (number_of_services > 16)
		return NET_NFC_OUT_OF_BOUND;

	if (data->length > 16 * number_of_blocks)
		return NET_NFC_OUT_OF_BOUND;

	send_buffer_length = 1 + 1 + 8 + 1 + (2 * number_of_services)
		+ 1 + number_of_blocks + data->length;

	_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
	if (NULL == send_buffer)
		return NET_NFC_ALLOC_FAIL;

	temp = send_buffer;

	*send_buffer = send_buffer_length;
	send_buffer++;

	*send_buffer = FELICA_CMD_WRITE_WITHOUT_ENC;
	send_buffer++;

	memcpy(send_buffer, IDm->buffer, IDm->length);
	send_buffer = send_buffer + IDm->length;

	*send_buffer = number_of_services;
	send_buffer++;

	for (i = 0; i < number_of_services; i++)
	{
		memcpy(send_buffer, &service_list[i], sizeof(uint16_t));
		send_buffer = send_buffer + 2;
	}

	*send_buffer = number_of_blocks;
	send_buffer++;

	for (i = 0; i < number_of_blocks; i++)
	{
		memcpy(send_buffer, &block_list[i], sizeof(uint8_t));
		send_buffer++;
	}

	memcpy(send_buffer, data->buffer, data->length);

	DEBUG_MSG_PRINT_BUFFER(temp, send_buffer_length);

	rawdata.buffer = temp;
	rawdata.length = send_buffer_length;

	net_nfc_error_e result = NET_NFC_OK;

	result = net_nfc_client_transceive_data(handle, &rawdata, callback, user_data);

	if (temp != NULL)
		_net_nfc_util_free_mem(temp);

	return result;
}

API net_nfc_error_e net_nfc_client_felica_request_system_code(
		net_nfc_target_handle_s *handle,
		nfc_transceive_data_callback callback,
		void *user_data)
{
	data_s rawdata;
	data_s *IDm = NULL;
	uint8_t send_buffer[10] = { 0x00, };
	net_nfc_target_info_s* target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;

	if (target_info->devType != NET_NFC_FELICA_PICC)
	{
		NFC_ERR("only Felica tag is available(TAG=%d)", target_info->devType);
		return NET_NFC_NOT_ALLOWED_OPERATION;
	}

	if (net_nfc_get_tag_info_value(target_info, FELICA_TAG_KEY, &IDm) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	if (IDm->length != 8)
		return NET_NFC_OUT_OF_BOUND;

	send_buffer[0] = 0xA;
	send_buffer[1] = FELICA_CMD_REQ_SYSTEM_CODE;

	memcpy(send_buffer + 2, IDm->buffer, IDm->length);

	DEBUG_MSG_PRINT_BUFFER(send_buffer, 10);

	rawdata.buffer = send_buffer;
	rawdata.length = 10;

	return net_nfc_client_transceive_data(handle, &rawdata, callback, user_data);
}
