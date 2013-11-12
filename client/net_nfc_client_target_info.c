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

#include <stdbool.h>

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_data.h"
#include "net_nfc_target_info.h"
#include "net_nfc_util_internal.h"

API net_nfc_error_e net_nfc_get_tag_type(net_nfc_target_info_s *target_info,
		net_nfc_target_type_e *type)
{
	RETV_IF(NULL == type, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == target_info, NET_NFC_INVALID_HANDLE);

	*type = NET_NFC_UNKNOWN_TARGET;
	*type = target_info->devType;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_get_tag_handle(net_nfc_target_info_s *target_info,
		net_nfc_target_handle_s **handle)
{
	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == target_info, NET_NFC_INVALID_HANDLE);

	*handle = NULL;
	*handle = target_info->handle;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_get_tag_ndef_support(
		net_nfc_target_info_s *target_info, bool *is_support)
{
	RETV_IF(NULL == target_info, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == is_support, NET_NFC_NULL_PARAMETER);

	*is_support = (bool)target_info->is_ndef_supported;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_get_tag_max_data_size(
		net_nfc_target_info_s *target_info, uint32_t *max_size)
{
	RETV_IF(NULL == target_info, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == max_size, NET_NFC_NULL_PARAMETER);

	*max_size = target_info->maxDataSize;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_get_tag_actual_data_size(
		net_nfc_target_info_s *target_info, uint32_t *actual_data)
{
	RETV_IF(NULL == target_info, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == actual_data, NET_NFC_NULL_PARAMETER);

	*actual_data = target_info->actualDataSize;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_get_tag_info_keys(net_nfc_target_info_s *target_info,
		char ***keys, int *number_of_keys)
{
	int i = 0;

	RETV_IF(NULL == keys, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == target_info, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == number_of_keys, NET_NFC_NULL_PARAMETER);

	RETV_IF(NULL == target_info->tag_info_list, NET_NFC_NO_DATA_FOUND);
	RETV_IF(target_info->number_of_keys <= 0, NET_NFC_NO_DATA_FOUND);

	if (target_info->keylist != NULL)
	{
		*keys = target_info->keylist;
		return NET_NFC_OK;
	}

	_net_nfc_util_alloc_mem(*keys, target_info->number_of_keys * sizeof(char *));
	if (NULL == *keys)
		return NET_NFC_ALLOC_FAIL;

	net_nfc_tag_info_s *tag_info = target_info->tag_info_list;

	for (; i < target_info->number_of_keys; i++, tag_info++)
	{
		(*keys)[i] = tag_info->key;
	}

	*number_of_keys = target_info->number_of_keys;

	NFC_DBG("number of keys = [%d]", target_info->number_of_keys);

	target_info->keylist = *keys;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_get_tag_info_value(net_nfc_target_info_s *target_info,
		const char *key, data_s **value)
{
	int i;
	net_nfc_tag_info_s *tag_info;

	RETV_IF(NULL == key, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == value, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == target_info, NET_NFC_NULL_PARAMETER);

	RETV_IF(NULL == target_info->tag_info_list, NET_NFC_NO_DATA_FOUND);

	tag_info = target_info->tag_info_list;

	for (i = 0; i < target_info->number_of_keys; i++, tag_info++)
	{
		if (strcmp(key, tag_info->key) == 0)
		{
			if (NULL == tag_info->value)
			{
				return NET_NFC_NO_DATA_FOUND;
			}
			else
			{
				*value = tag_info->value;
				break;
			}
		}
	}

	if (i == target_info->number_of_keys)
		return NET_NFC_NO_DATA_FOUND;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_duplicate_target_info(net_nfc_target_info_s *origin,
		net_nfc_target_info_s **result)
{
	net_nfc_target_info_s *temp = NULL;

	RETV_IF(NULL == origin, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == result, NET_NFC_NULL_PARAMETER);

	_net_nfc_util_alloc_mem(temp, sizeof(net_nfc_target_info_s));
	if (NULL == temp)
		return NET_NFC_ALLOC_FAIL;

	temp->ndefCardState = origin->ndefCardState;
	temp->actualDataSize = origin->actualDataSize;
	temp->maxDataSize = origin->maxDataSize;
	temp->devType = origin->devType;
	temp->handle = origin->handle;
	temp->is_ndef_supported = origin->is_ndef_supported;
	temp->number_of_keys = origin->number_of_keys;

	if (0 < temp->number_of_keys)
	{
		int i;

		_net_nfc_util_alloc_mem(temp->tag_info_list,
				temp->number_of_keys * sizeof(net_nfc_tag_info_s));

		if (NULL == temp->tag_info_list)
		{
			_net_nfc_util_free_mem(temp);
			return NET_NFC_ALLOC_FAIL;
		}

		for (i = 0; i < origin->number_of_keys; i++)
		{
			if (origin->tag_info_list[i].key != NULL)
			{
				_net_nfc_util_strdup(temp->tag_info_list[i].key,
					origin->tag_info_list[i].key);
			}

			if (origin->tag_info_list[i].value != NULL)
			{
				data_s *data = origin->tag_info_list[i].value;

				net_nfc_create_data(&temp->tag_info_list[i].value,
					data->buffer, data->length);
			}
		}
	}

	if (0 < origin->raw_data.length)
	{
		net_nfc_util_alloc_data(&temp->raw_data, origin->raw_data.length);
		memcpy(temp->raw_data.buffer, origin->raw_data.buffer, temp->raw_data.length);
	}

	*result = temp;

	return NET_NFC_OK;
}

static net_nfc_error_e _release_tag_info(net_nfc_target_info_s *info)
{
	net_nfc_tag_info_s *list = NULL;

	if (NULL == info)
		return NET_NFC_NULL_PARAMETER;

	list = info->tag_info_list;
	if (list != NULL)
	{
		int i;

		for (i = 0; i < info->number_of_keys; i++, list++)
		{
			if (list->key != NULL)
				_net_nfc_util_free_mem(list->key);

			if (list->value != NULL)
				net_nfc_free_data(list->value);
		}

		_net_nfc_util_free_mem(info->tag_info_list);
	}

	if (info->keylist != NULL)
	{
		_net_nfc_util_free_mem(info->keylist);
	}

	net_nfc_util_free_data(&info->raw_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_release_tag_info(net_nfc_target_info_s *target_info)
{
	net_nfc_error_e result;

	if (NULL == target_info)
		return NET_NFC_NULL_PARAMETER;

	result = _release_tag_info(target_info);

	_net_nfc_util_free_mem(target_info);

	return result;
}
