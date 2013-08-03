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

#include <string.h>

#include "net_nfc_data.h"
#include "net_nfc_typedef_internal.h"
#include "net_nfc_util_internal.h"

#ifndef NET_NFC_EXPORT_API
#define NET_NFC_EXPORT_API __attribute__((visibility("default")))
#endif

NET_NFC_EXPORT_API net_nfc_error_e net_nfc_create_data_only(data_h* data)
{
	return net_nfc_create_data(data, NULL, 0);
}

NET_NFC_EXPORT_API net_nfc_error_e net_nfc_create_data(data_h* data, const uint8_t* bytes, const uint32_t length)
{
	data_s *tmp_data = NULL;

	if (data == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	_net_nfc_util_alloc_mem(tmp_data, sizeof(data_s));
	if (tmp_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	if (length > 0)
	{
		_net_nfc_util_alloc_mem(tmp_data->buffer, length);
		if (tmp_data->buffer == NULL)
		{
			_net_nfc_util_free_mem(tmp_data);
			return NET_NFC_ALLOC_FAIL;
		}

		tmp_data->length = length;

		if (bytes != NULL)
		{
			memcpy(tmp_data->buffer, bytes, length);
		}
	}

	*data = (data_h)tmp_data;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API net_nfc_error_e net_nfc_get_data(const data_h data, uint8_t** bytes, uint32_t * length)
{
	if (data == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}
	data_s * tmp_data = (data_s *)data;

	*bytes = tmp_data->buffer;
	*length = tmp_data->length;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API net_nfc_error_e net_nfc_set_data(const data_h data, const uint8_t* bytes, const uint32_t length)
{
	if (data == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}
	data_s * tmp_data = (data_s *)data;

	if (tmp_data->buffer == bytes && tmp_data->length == length)
	{
		return NET_NFC_OK;
	}

	if (tmp_data->buffer != NULL)
	{
		_net_nfc_util_free_mem(tmp_data->buffer);
	}

	if (length <= 0)
	{
		tmp_data->buffer = NULL;
		tmp_data->length = 0;
		return NET_NFC_OK;
	}

	if (length > 0)
	{
		_net_nfc_util_alloc_mem((tmp_data)->buffer, length);
	}

	if (bytes != NULL)
	{
		memcpy(tmp_data->buffer, bytes, length);
	}

	tmp_data->length = length;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API uint32_t net_nfc_get_data_length(const data_h data)
{
	if (data == NULL)
	{
		return 0;
	}
	data_s * tmp_data = (data_s *)data;

	return tmp_data->length;
}

NET_NFC_EXPORT_API uint8_t * net_nfc_get_data_buffer(const data_h data)
{
	if (data == NULL)
	{
		return NULL;
	}
	data_s * tmp_data = (data_s *)data;

	return tmp_data->buffer;
}

NET_NFC_EXPORT_API net_nfc_error_e net_nfc_free_data(data_h data)
{
	if (data == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}
	data_s * tmp_data = (data_s *)data;

	if (tmp_data->buffer != NULL)
	{
		_net_nfc_util_free_mem(tmp_data->buffer);
	}
	_net_nfc_util_free_mem(tmp_data);

	return NET_NFC_OK;
}
