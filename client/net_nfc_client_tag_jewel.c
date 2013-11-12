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

#include <glib.h>
#include <string.h>

#include "net_nfc_client_tag_jewel.h"
#include "net_nfc_client_tag_internal.h"

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_target_info.h"


#define JEWEL_CMD_REQA 0x26
#define JEWEL_CMD_WUPA 0x52
#define JEWEL_CMD_RID 0x78
#define JEWEL_CMD_RALL 0x00
#define JEWEL_CMD_READ 0x01
#define JEWEL_CMD_WRITE_E 0x53
#define JEWEL_CMD_WRITE_NE 0x1A
#define JEWEL_TAG_KEY	"UID"

API net_nfc_error_e net_nfc_client_jewel_read_id(net_nfc_target_handle_s *handle,
		nfc_transceive_data_callback callback, void *user_data)
{
	data_s rawdata;
	uint8_t send_buffer[9] = {0x00, };
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;

	if(target_info->devType != NET_NFC_JEWEL_PICC)
	{
		NFC_ERR("only Jewel tag is available(TAG=%d)", target_info->devType);
		return NET_NFC_NOT_ALLOWED_OPERATION;
	}

	send_buffer[0] = JEWEL_CMD_RID;

	net_nfc_util_compute_CRC(CRC_B, send_buffer, 9);

	DEBUG_MSG_PRINT_BUFFER(send_buffer, 9);

	rawdata.buffer = send_buffer;
	rawdata.length = 9;

	return net_nfc_client_transceive_data(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_jewel_read_byte(net_nfc_target_handle_s *handle,
		uint8_t block, uint8_t byte, nfc_transceive_data_callback callback, void *user_data)
{
	data_s rawdata;
	data_s *UID = NULL;
	uint8_t send_buffer[9] = {0x00, };
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETVM_IF(block > 0xE || byte > 0x7, NET_NFC_OUT_OF_BOUND,
		"block value is = [0x%x], byte value is = [0x%x]", block, byte);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;

	if(target_info->devType != NET_NFC_JEWEL_PICC)
	{
		NFC_ERR("only Jewel tag is available(TAG=%d)", target_info->devType);
		return NET_NFC_NOT_ALLOWED_OPERATION;
	}

	if(net_nfc_get_tag_info_value(target_info, JEWEL_TAG_KEY, &UID) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	if(UID->length != 4)
		return NET_NFC_OUT_OF_BOUND;

	/* command */
	send_buffer[0] = JEWEL_CMD_READ;

	/* addr */
	send_buffer[1] = (((block << 3) & 0x78) | (byte & 0x7));

	/* data */
	send_buffer[2] = 0x00;

	/* UID0 ~ 3 */
	memcpy(&(send_buffer[3]), UID->buffer, UID->length);

	net_nfc_util_compute_CRC(CRC_B, send_buffer, 9);

	DEBUG_MSG_PRINT_BUFFER(send_buffer, 9);

	rawdata.buffer = send_buffer;
	rawdata.length = 9;

	return net_nfc_client_transceive_data(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_jewel_read_all(net_nfc_target_handle_s *handle,
		nfc_transceive_data_callback callback, void *user_data)
{
	data_s rawdata;
	data_s *UID = NULL;
	uint8_t send_buffer[9] = {0x00, };
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;

	if(target_info->devType != NET_NFC_JEWEL_PICC)
	{
		NFC_ERR("only Jewel tag is available(TAG=%d)", target_info->devType);
		return NET_NFC_NOT_ALLOWED_OPERATION;
	}

	if(net_nfc_get_tag_info_value(target_info, JEWEL_TAG_KEY, &UID) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	if(UID->length != 4)
		return NET_NFC_OUT_OF_BOUND;

	/* command */
	send_buffer[0] = JEWEL_CMD_RALL;

	/* addr */
	send_buffer[1] = 0x00;

	/* data */
	send_buffer[2] = 0x00;

	/* UID0 ~ 3 */
	memcpy(&(send_buffer[3]), UID->buffer, UID->length);

	net_nfc_util_compute_CRC(CRC_B, send_buffer, 9);

	DEBUG_MSG_PRINT_BUFFER(send_buffer, 9);

	rawdata.buffer = send_buffer;
	rawdata.length = 9;

	return net_nfc_client_transceive_data(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_jewel_write_with_erase(
		net_nfc_target_handle_s *handle,
		uint8_t block,
		uint8_t byte,
		uint8_t data,
		nfc_transceive_callback callback,
		void *user_data)
{
	data_s rawdata;
	data_s *UID = NULL;
	uint8_t send_buffer[9] = {0x00, };
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETVM_IF(block > 0xE || byte > 0x7, NET_NFC_OUT_OF_BOUND,
		"block value is = [0x%x], byte value is = [0x%x]", block, byte);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;

	if(net_nfc_get_tag_info_value(target_info, JEWEL_TAG_KEY, &UID) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	if(UID->length != 4)
		return NET_NFC_OUT_OF_BOUND;

	/* command */
	send_buffer[0] = JEWEL_CMD_WRITE_E;

	/* addr */
	send_buffer[1] = (((block << 3) & 0x78) | (byte & 0x7));

	/* data */
	send_buffer[2] = data;

	/* UID0 ~ 3 */
	memcpy(&(send_buffer[3]), UID->buffer, UID->length);

	net_nfc_util_compute_CRC(CRC_B, send_buffer, 9);

	DEBUG_MSG_PRINT_BUFFER(send_buffer, 9);

	rawdata.buffer = send_buffer;
	rawdata.length = 9;

	return net_nfc_client_transceive(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_jewel_write_with_no_erase(
		net_nfc_target_handle_s *handle,
		uint8_t block,
		uint8_t byte,
		uint8_t data,
		nfc_transceive_callback callback,
		void *user_data)
{
	data_s rawdata;
	data_s *UID = NULL;
	uint8_t send_buffer[9] = {0x00, };
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETVM_IF(block > 0xE || byte > 0x7, NET_NFC_OUT_OF_BOUND,
		"block value is = [0x%x], byte value is = [0x%x]", block, byte);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NO_DATA_FOUND;


	if(target_info->devType != NET_NFC_JEWEL_PICC)
	{
		NFC_ERR("only Jewel tag is available(TAG=%d)", target_info->devType);
		return NET_NFC_NOT_ALLOWED_OPERATION;
	}

	if(net_nfc_get_tag_info_value(target_info, JEWEL_TAG_KEY, &UID) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	if(UID->length != 4)
		return NET_NFC_OUT_OF_BOUND;

	/* command */
	send_buffer[0] = JEWEL_CMD_WRITE_NE;

	/* addr */
	send_buffer[1] = (((block << 3) & 0x78) | (byte & 0x7));

	/* data */
	send_buffer[2] = data;

	/* UID0 ~ 3 */
	memcpy(&(send_buffer[3]), UID->buffer, UID->length);

	net_nfc_util_compute_CRC(CRC_B, send_buffer, 9);

	DEBUG_MSG_PRINT_BUFFER(send_buffer, 9);

	rawdata.buffer = send_buffer;
	rawdata.length = 9;

	return net_nfc_client_transceive(handle, &rawdata, callback, user_data);
}
