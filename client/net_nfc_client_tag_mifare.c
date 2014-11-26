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

#include "net_nfc_client_transceive.h"
#include "net_nfc_client_tag_internal.h"
#include "net_nfc_client_tag_mifare.h"
#include "net_nfc_data.h"
#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_target_info.h"
#include "net_nfc_util_internal.h"

#define MIFARE_CMD_RAW			0x00U
#define MIFARE_CMD_AUTH_A		0x60U /**< authenticate with key A */
#define MIFARE_CMD_AUTH_B		0x61U /**< authenticate with key B */
#define MIFARE_CMD_READ			0x30U /**< read 16 bytes */
#define MIFARE_CMD_WRITE_BLOCK		0xA0U /**< write 16 bytes */
#define MIFARE_CMD_WRITE_PAGE		0xA2U /**< write 4 bytes */
#define MIFARE_CMD_INCREMENT		0xC1U /**< Increment. */
#define MIFARE_CMD_DECREMENT		0xC0U /**< Decrement. */
#define MIFARE_CMD_TRANSFER		0xB0U /**< Tranfer.   */
#define MIFARE_CMD_RESTORE		0xC2U /**< Restore.   */
#define MIFARE_TAG_KEY	"UID"

#define MIFARE_CMD_READ_SECTOR	0x38U /* read sector */
#define MIFARE_CMD_WRITE_SECTOR	0xA8U /* write sector */

static uint8_t default_key[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static uint8_t mad_key[6] = { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5 };
static uint8_t net_nfc_forum_key[6] = { 0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7 };

#define MIFARE_BLOCK_4 4
#define MIFARE_BLOCK_16 16

#define MIFARE_MINI_SECTORS 5
#define MIFARE_1K_SECTORS 16
#define MIFARE_4K_SECTORS 40

#define MIFARE_BLOCK_SIZE 16	/* 1 block is 16 byte */
#define MIFARE_PAGE_SIZE 4	/* 1 page is 4 byte */


API net_nfc_error_e net_nfc_client_mifare_authenticate_with_keyA(
		net_nfc_target_handle_s *handle,
		uint8_t sector,
		data_s *auth_key,
		void *callback,
		void *user_data)
{
	data_s rawdata;
	data_s *UID = NULL;
	uint8_t* temp = NULL;
	net_nfc_error_e result;
	int send_buffer_length = 0;
	uint8_t* send_buffer = NULL;
	uint8_t sector_to_block = 0;
	net_nfc_target_info_s* target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == auth_key, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);
	RETVM_IF(auth_key->length != 6, NET_NFC_OUT_OF_BOUND,
		"auth_key->length(%d) != 6", auth_key->length);

	target_info = net_nfc_client_tag_get_client_target_info();

	if (target_info)
	{
		if (target_info->devType < NET_NFC_MIFARE_MINI_PICC ||
				target_info->devType > NET_NFC_MIFARE_4K_PICC)
		{
			NFC_ERR("not a MIFARE Classic TAG(%d)", target_info->devType);
			return NET_NFC_NOT_SUPPORTED;
		}
	}

	if (net_nfc_get_tag_info_value(target_info, MIFARE_TAG_KEY, &UID) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	switch (target_info->devType)
	{
	case NET_NFC_MIFARE_MINI_PICC :
		/* 0 ~ 4 : 5 sector and 4 block with 16 bytes */
		if (sector > MIFARE_MINI_SECTORS - 1)
			return NET_NFC_OUT_OF_BOUND;

		sector_to_block = sector * MIFARE_BLOCK_4 + 3;
		break;
	case NET_NFC_MIFARE_1K_PICC :
		/* 0 ~ 15 : 16 sector and 4 block with 16 bytes */
		if (sector > MIFARE_1K_SECTORS)
			return NET_NFC_OUT_OF_BOUND;

		sector_to_block = sector * MIFARE_BLOCK_4 + 3;
		break;
	case NET_NFC_MIFARE_4K_PICC :
		/* 0 ~ 31 : 32 sector and 4 block with 16 bytes
		 * 32 ~ 39 : 8 sector and 16 block with 16 bytes
		 */
		if (sector > MIFARE_4K_SECTORS)
			return NET_NFC_OUT_OF_BOUND;

		if (sector < 32)
			sector_to_block = sector * MIFARE_BLOCK_4 + 3;
		else
		{
			sector_to_block = (31 * MIFARE_BLOCK_4 + 3)
				+ (sector - 32)
				* MIFARE_BLOCK_16 + 15;
		}
		break;
	default :
		break;
	}


	/* cmd + addr + UID + AUTH_KEY + CRC_A */
	send_buffer_length = 1 + 1 + UID->length + auth_key->length + 2;

	_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
	if (NULL == send_buffer)
	{
		net_nfc_free_data(UID);
		return NET_NFC_ALLOC_FAIL;
	}

	temp = send_buffer;

	*temp = MIFARE_CMD_AUTH_A;
	temp++;

	*temp = sector_to_block;
	temp++;

	memcpy(temp, UID->buffer, UID->length);
	temp = temp + UID->length;

	memcpy(temp, auth_key->buffer, auth_key->length);

	net_nfc_util_compute_CRC(CRC_A, send_buffer, send_buffer_length);


	rawdata.buffer = send_buffer;
	rawdata.length = send_buffer_length;

	result = net_nfc_client_transceive(handle, &rawdata, callback, user_data);

	if (send_buffer != NULL)
		_net_nfc_util_free_mem(send_buffer);

	return result;
}

API net_nfc_error_e net_nfc_client_mifare_authenticate_with_keyB(
		net_nfc_target_handle_s *handle,
		uint8_t sector,
		data_s *auth_key,
		void *callback,
		void *user_data)
{
	data_s rawdata;
	data_s *UID = NULL;
	uint8_t* temp = NULL;
	net_nfc_error_e result;
	int send_buffer_length = 0;
	uint8_t* send_buffer = NULL;
	uint8_t sector_to_block = 0;
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == auth_key, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);
	RETVM_IF(auth_key->length != 6, NET_NFC_OUT_OF_BOUND,
		"auth_key->length(%d) != 6", auth_key->length);


	target_info = net_nfc_client_tag_get_client_target_info();

	if (target_info != NULL)
	{
		if (target_info->devType < NET_NFC_MIFARE_MINI_PICC ||
				target_info->devType > NET_NFC_MIFARE_4K_PICC)
		{
			NFC_ERR("not a MIFARE Classic TAG(%d)", target_info->devType);
			return NET_NFC_NOT_SUPPORTED;
		}
	}

	if (net_nfc_get_tag_info_value(target_info, MIFARE_TAG_KEY, &UID) != NET_NFC_OK)
		return NET_NFC_NO_DATA_FOUND;

	switch (target_info->devType)
	{
	case NET_NFC_MIFARE_MINI_PICC :
		/* 0 ~ 4 : 5 sector and 4 block with 16 bytes */
		if (sector > MIFARE_MINI_SECTORS)
			return NET_NFC_OUT_OF_BOUND;

		sector_to_block = sector * MIFARE_BLOCK_4 + 3;
		break;
	case NET_NFC_MIFARE_1K_PICC :
		/* 0 ~ 15 : 16 sector and 4 block with 16 bytes */
		if (sector > MIFARE_1K_SECTORS)
			return NET_NFC_OUT_OF_BOUND;

		sector_to_block = sector * MIFARE_BLOCK_4 + 3;
		break;
	case NET_NFC_MIFARE_4K_PICC :
		/* 0 ~ 31 : 32 sector and 4 block with 16 bytes
		 * 32 ~ 39 : 8 sector and 16 block with 16 bytes
		 */
		if (sector > MIFARE_4K_SECTORS)
			return NET_NFC_OUT_OF_BOUND;

		if (sector < 32)
			sector_to_block = sector * MIFARE_BLOCK_4 + 3;
		else
		{
			sector_to_block = (31 * MIFARE_BLOCK_4 + 3) + (sector - 32)
				* MIFARE_BLOCK_16 + 15;
		}
		break;
	default :
		break;
	}

	/* cmd + addr + UID + AUTH_KEY + CRC_A */
	send_buffer_length = 1 + 1 + UID->length + auth_key->length + 2;

	_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
	if (NULL == send_buffer)
	{
		net_nfc_free_data(UID);
		return NET_NFC_ALLOC_FAIL;
	}

	temp = send_buffer;

	*temp = MIFARE_CMD_AUTH_B;
	temp++;

	*temp = sector_to_block;
	temp++;

	memcpy(temp, UID->buffer, UID->length);
	temp = temp + UID->length;

	memcpy(temp, auth_key->buffer, auth_key->length);

	net_nfc_util_compute_CRC(CRC_A, send_buffer, send_buffer_length);

	rawdata.buffer = send_buffer;
	rawdata.length = send_buffer_length;

	result = net_nfc_client_transceive(handle, &rawdata, callback, user_data);

	if (send_buffer != NULL)
		_net_nfc_util_free_mem(send_buffer);

	return result;
}

API net_nfc_error_e net_nfc_client_mifare_read(net_nfc_target_handle_s *handle,
		uint8_t addr, void *callback, void *user_data)
{
	data_s rawdata;
	uint8_t send_buffer[4] = {0};
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();

	if (target_info != NULL)
	{
		if (target_info->devType < NET_NFC_MIFARE_MINI_PICC ||
				target_info->devType > NET_NFC_MIFARE_ULTRA_PICC)
		{
			NFC_ERR("not a MIFARE Classic TAG(%d)", target_info->devType);
			return NET_NFC_NOT_SUPPORTED;
		}

		if (target_info->devType == NET_NFC_MIFARE_ULTRA_PICC)
		{
			if (addr > 7)
				return NET_NFC_OUT_OF_BOUND;
		}
	}


	send_buffer[0] = MIFARE_CMD_READ;
	send_buffer[1] = addr;

	net_nfc_util_compute_CRC(CRC_A, send_buffer, 4);

	rawdata.buffer = send_buffer;
	rawdata.length = 4;

	return net_nfc_client_transceive_data(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_mifare_write_block(
		net_nfc_target_handle_s *handle,
		uint8_t addr,
		data_s *data,
		void *callback,
		void *user_data)
{
	data_s rawdata;
	net_nfc_error_e result;
	uint8_t* send_buffer = NULL;
	uint32_t send_buffer_length = 0;
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
		return NET_NFC_NOT_INITIALIZED;

	if (target_info->devType < NET_NFC_MIFARE_MINI_PICC ||
			target_info->devType > NET_NFC_MIFARE_ULTRA_PICC)
	{
		NFC_ERR("not a MIFARE Classic TAG(%d)", target_info->devType);
		return NET_NFC_NOT_SUPPORTED;
	}

	if (target_info->devType == NET_NFC_MIFARE_ULTRA_PICC)
	{
		if (addr > 7)
			return NET_NFC_OUT_OF_BOUND;
	}

	if (target_info->devType == NET_NFC_MIFARE_ULTRA_PICC)
	{
		if (data->length > MIFARE_PAGE_SIZE)
		{
			uint8_t* temp = NULL;

			/* cmd + addr + page + CRC */
			send_buffer_length = 1 + 1 + MIFARE_PAGE_SIZE + 2;

			_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
			if (NULL == send_buffer)
				return NET_NFC_ALLOC_FAIL;

			temp = send_buffer;

			*temp = MIFARE_CMD_WRITE_PAGE;
			temp++;

			*temp = addr;
			temp++;

			memcpy(temp, data->buffer, MIFARE_PAGE_SIZE);

		}
		else
		{
			uint8_t* temp = NULL;

			/* cmd + addr + page + CRC */
			send_buffer_length = 1 + 1 + data->length + 2;

			_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
			if (NULL == send_buffer)
				return NET_NFC_ALLOC_FAIL;

			temp = send_buffer;

			*temp = MIFARE_CMD_WRITE_PAGE;
			temp++;

			*temp = addr;
			temp++;

			memcpy(temp, data->buffer, data->length);
		}
	}
	else
	{
		if (data->length > MIFARE_BLOCK_SIZE)
		{
			uint8_t* temp = NULL;

			/* cmd + addr + page + CRC */
			send_buffer_length = 1 + 1 + MIFARE_BLOCK_SIZE + 2;

			_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
			if (NULL == send_buffer)
				return NET_NFC_ALLOC_FAIL;

			temp = send_buffer;

			*temp = MIFARE_CMD_WRITE_BLOCK;
			temp++;

			*temp = addr;
			temp++;

			memcpy(temp, data->buffer, MIFARE_BLOCK_SIZE);
		}
		else
		{
			uint8_t* temp = NULL;

			/* cmd + addr + page + CRC */
			send_buffer_length = 1 + 1 + data->length + 2;

			_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
			if (NULL == send_buffer)
				return NET_NFC_ALLOC_FAIL;

			temp = send_buffer;

			*temp = MIFARE_CMD_WRITE_BLOCK;
			temp++;

			*temp = addr;
			temp++;

			memcpy(temp, data->buffer, data->length);
		}
	}

	net_nfc_util_compute_CRC(CRC_A, send_buffer, send_buffer_length);

	rawdata.buffer = send_buffer;
	rawdata.length = send_buffer_length;

	result = net_nfc_client_transceive(handle, &rawdata, callback, user_data);

	if (send_buffer != NULL)
		_net_nfc_util_free_mem(send_buffer);

	return result;
}

API net_nfc_error_e net_nfc_client_mifare_write_page(
		net_nfc_target_handle_s *handle,
		uint8_t addr,
		data_s *data,
		void *callback,
		void *user_data)
{
	data_s rawdata;
	net_nfc_error_e result;
	uint8_t* send_buffer = NULL;
	uint32_t send_buffer_length = 0;
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();

	if (NULL == target_info)
		return NET_NFC_NOT_INITIALIZED;

	if (target_info->devType < NET_NFC_MIFARE_MINI_PICC ||
			target_info->devType > NET_NFC_MIFARE_ULTRA_PICC)
	{
		NFC_ERR("not a MIFARE Classic TAG(%d)", target_info->devType);
		return NET_NFC_NOT_SUPPORTED;
	}

	if (target_info->devType == NET_NFC_MIFARE_ULTRA_PICC)
	{
		if (addr > 7)
			return NET_NFC_OUT_OF_BOUND;
	}

	if (target_info->devType == NET_NFC_MIFARE_ULTRA_PICC)
	{
		if (data->length > MIFARE_PAGE_SIZE)
		{
			uint8_t* temp = NULL;

			/* cmd + addr + page + CRC */
			send_buffer_length = 1 + 1 + MIFARE_PAGE_SIZE + 2;

			_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
			if (NULL == send_buffer)
				return NET_NFC_ALLOC_FAIL;

			temp = send_buffer;

			*temp = MIFARE_CMD_WRITE_PAGE;
			temp++;

			*temp = addr;
			temp++;

			memcpy(temp, data->buffer, MIFARE_PAGE_SIZE);
		}
		else
		{
			uint8_t* temp = NULL;

			/* cmd + addr + page + CRC */
			send_buffer_length = 1 + 1 + data->length + 2;

			_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
			if (NULL == send_buffer)
				return NET_NFC_ALLOC_FAIL;

			temp = send_buffer;

			*temp = MIFARE_CMD_WRITE_PAGE;
			temp++;

			*temp = addr;
			temp++;

			memcpy(temp, data->buffer, data->length);
		}

	}
	else
	{
		if (data->length > MIFARE_PAGE_SIZE)
		{
			uint8_t* temp = NULL;

			/* cmd + addr + page + CRC */
			send_buffer_length = 1 + 1 + MIFARE_PAGE_SIZE + 2;

			_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
			if (NULL == send_buffer)
				return NET_NFC_ALLOC_FAIL;

			temp = send_buffer;

			*temp = MIFARE_CMD_WRITE_BLOCK;
			temp++;

			*temp = addr;
			temp++;

			memcpy(temp, data->buffer, MIFARE_PAGE_SIZE);

		}
		else
		{
			uint8_t* temp = NULL;

			/* cmd + addr + page + CRC */
			send_buffer_length = 1 + 1 + data->length + 2;

			_net_nfc_util_alloc_mem(send_buffer, send_buffer_length * sizeof(uint8_t));
			if (NULL == send_buffer)
				return NET_NFC_ALLOC_FAIL;

			temp = send_buffer;

			*temp = MIFARE_CMD_WRITE_BLOCK;
			temp++;

			*temp = addr;
			temp++;

			memcpy(temp, data->buffer, data->length);
		}

	}

	net_nfc_util_compute_CRC(CRC_A, send_buffer, send_buffer_length);

	rawdata.buffer = send_buffer;
	rawdata.length = send_buffer_length;

	result = net_nfc_client_transceive(handle, &rawdata, callback, user_data);

	if (send_buffer != NULL)
		_net_nfc_util_free_mem(send_buffer);

	return result;
}

API net_nfc_error_e net_nfc_client_mifare_increment(
		net_nfc_target_handle_s *handle,
		uint8_t addr,
		int value,
		void *callback,
		void *user_data)
{
	data_s rawdata;
	uint8_t send_buffer[8] = { 0 };
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();

	if (target_info)
	{
		if (target_info->devType < NET_NFC_MIFARE_MINI_PICC ||
				target_info->devType > NET_NFC_MIFARE_4K_PICC)
		{
			NFC_ERR("not a MIFARE Classic TAG(%d)", target_info->devType);
			return NET_NFC_NOT_SUPPORTED;
		}
	}


	send_buffer[0] = MIFARE_CMD_INCREMENT;
	send_buffer[1] = addr;

	/* little endian.
	   little value of byte array will be saved first in memory */
	send_buffer[5] = (value & 0xFF000000) >> 24;
	send_buffer[4] = (value & 0x00FF0000) >> 16;
	send_buffer[3] = (value & 0x0000FF00) >> 8;
	send_buffer[2] = (value & 0x000000FF);

	net_nfc_util_compute_CRC(CRC_A, send_buffer, 8);

	rawdata.buffer = send_buffer;
	rawdata.length = 8;

	return net_nfc_client_transceive(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_mifare_decrement(
		net_nfc_target_handle_s *handle,
		uint8_t addr,
		int value,
		void *callback,
		void *user_data)
{
	data_s rawdata;
	uint8_t send_buffer[8] = { 0 };
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();

	if (target_info)
	{
		if (target_info->devType < NET_NFC_MIFARE_MINI_PICC ||
				target_info->devType > NET_NFC_MIFARE_4K_PICC)
		{
			NFC_ERR("not a MIFARE Classic TAG(%d)", target_info->devType);
			return NET_NFC_NOT_SUPPORTED;
		}
	}

	send_buffer[0] = MIFARE_CMD_DECREMENT;
	send_buffer[1] = addr;

	/* little endian.
	   little value of byte array will be saved first in memory */
	send_buffer[5] = (value & 0xFF000000) >> 24;
	send_buffer[4] = (value & 0x00FF0000) >> 16;
	send_buffer[3] = (value & 0x0000FF00) >> 8;
	send_buffer[2] = (value & 0x000000FF);

	net_nfc_util_compute_CRC(CRC_A, send_buffer, 8);

	DEBUG_MSG_PRINT_BUFFER(send_buffer, 8);

	rawdata.buffer = send_buffer;
	rawdata.length = 8;

	return net_nfc_client_transceive(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_mifare_transfer(net_nfc_target_handle_s *handle,
		uint8_t addr, void *callback, void *user_data)
{
	data_s rawdata;
	uint8_t send_buffer[4] = { 0 };
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();

	if (target_info)
	{
		if (target_info->devType < NET_NFC_MIFARE_MINI_PICC ||
				target_info->devType > NET_NFC_MIFARE_4K_PICC)
		{
			NFC_ERR("not a MIFARE Classic TAG(%d)", target_info->devType);
			return NET_NFC_NOT_SUPPORTED;
		}
	}

	send_buffer[0] = MIFARE_CMD_TRANSFER;
	send_buffer[1] = addr;

	net_nfc_util_compute_CRC(CRC_A, send_buffer, 4);

	rawdata.buffer = send_buffer;
	rawdata.length = 4;

	return net_nfc_client_transceive(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_mifare_restore(net_nfc_target_handle_s *handle,
		uint8_t addr, void *callback, void *user_data)
{
	data_s rawdata;
	uint8_t send_buffer[4] = { 0 };
	net_nfc_target_info_s *target_info = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_OPERATION_FAIL);

	target_info = net_nfc_client_tag_get_client_target_info();

	if (target_info)
	{
		if (target_info->devType < NET_NFC_MIFARE_MINI_PICC ||
				target_info->devType > NET_NFC_MIFARE_4K_PICC)
		{
			NFC_ERR("not a MIFARE Classic TAG(%d)", target_info->devType);
			return NET_NFC_NOT_SUPPORTED;
		}
	}

	send_buffer[0] = MIFARE_CMD_RESTORE;
	send_buffer[1] = addr;

	net_nfc_util_compute_CRC(CRC_A, send_buffer, 4);

	rawdata.buffer = send_buffer;
	rawdata.length = 4;

	return net_nfc_client_transceive(handle, &rawdata, callback, user_data);
}

API net_nfc_error_e net_nfc_client_mifare_create_default_key(data_s** key)
{
	RETV_IF(NULL == key, NET_NFC_NULL_PARAMETER);

	return net_nfc_create_data(key, default_key, 6);
}

API net_nfc_error_e net_nfc_client_mifare_create_application_directory_key(
		data_s** key)
{
	RETV_IF(NULL == key, NET_NFC_NULL_PARAMETER);

	return net_nfc_create_data(key, mad_key, 6);
}

API net_nfc_error_e net_nfc_client_mifare_create_net_nfc_forum_key(
		data_s** key)
{
	RETV_IF(NULL == key, NET_NFC_NULL_PARAMETER);

	return net_nfc_create_data(key, net_nfc_forum_key, 6);
}

