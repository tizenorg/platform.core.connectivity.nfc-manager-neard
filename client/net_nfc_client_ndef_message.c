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

#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_ndef_message.h"
#include "net_nfc_data.h"


/* public functions */
API net_nfc_error_e net_nfc_create_ndef_message(ndef_message_h *ndef_message)
{
	return net_nfc_util_create_ndef_message((ndef_message_s **)ndef_message);
}

API net_nfc_error_e net_nfc_create_rawdata_from_ndef_message(ndef_message_h ndef_message, data_h *rawdata)
{
	uint32_t count;
	net_nfc_error_e result;
	data_h data;

	if (ndef_message == NULL || rawdata == NULL) {
		return NET_NFC_NULL_PARAMETER;
	}

	*rawdata = NULL;

	result = net_nfc_get_ndef_message_byte_length(ndef_message, &count);
	if (result != NET_NFC_OK) {
		return result;
	}

	result = net_nfc_create_data(&data ,NULL, count);
	if (result != NET_NFC_OK) {
		return result;
	}

	result = net_nfc_util_convert_ndef_message_to_rawdata(
			(ndef_message_s *)ndef_message, (data_s *)data);
	if (result == NET_NFC_OK) {
		*rawdata = data;
	} else {
		net_nfc_free_data(data);
	}

	return result;
}

API net_nfc_error_e net_nfc_create_ndef_message_from_rawdata(ndef_message_h *ndef_message, data_h rawdata)
{
	net_nfc_error_e result;
	ndef_message_h msg;

	if (ndef_message == NULL || rawdata == NULL) {
		return NET_NFC_NULL_PARAMETER;
	}

	*ndef_message = NULL;

	result = net_nfc_create_ndef_message(&msg);
	if (result != NET_NFC_OK) {
		return result;
	}

	result = net_nfc_util_convert_rawdata_to_ndef_message(
			(data_s *)rawdata, (ndef_message_s *)msg);
	if (result == NET_NFC_OK) {
		*ndef_message = msg;
	} else {
		net_nfc_free_ndef_message(msg);
	}

	return result;
}

API net_nfc_error_e net_nfc_get_ndef_message_byte_length(ndef_message_h ndef_message, uint32_t *length)
{
	net_nfc_error_e result;

	if (ndef_message == NULL || length == NULL){
		return NET_NFC_NULL_PARAMETER;
	}

	*length = net_nfc_util_get_ndef_message_length((ndef_message_s *)ndef_message);
	if (*length > 0) {
		result = NET_NFC_OK;
	} else {
		result = NET_NFC_INVALID_PARAM;
	}

	return result;
}

API net_nfc_error_e net_nfc_append_record_to_ndef_message(ndef_message_h ndef_message, ndef_record_h  record)
{
	if (ndef_message == NULL || record == NULL){
		return NET_NFC_NULL_PARAMETER;
	}

	return net_nfc_util_append_record((ndef_message_s*)ndef_message, (ndef_record_s *)record);
}

API net_nfc_error_e net_nfc_free_ndef_message(ndef_message_h ndef_message)
{
	if (ndef_message == NULL) {
		return NET_NFC_NULL_PARAMETER;
	}

	return net_nfc_util_free_ndef_message((ndef_message_s *)ndef_message);
}

API net_nfc_error_e net_nfc_get_ndef_message_record_count(ndef_message_h ndef_message, int *count)
{
	if (ndef_message == NULL || count == NULL) {
		return NET_NFC_NULL_PARAMETER;
	}

	ndef_message_s *msg = (ndef_message_s *)ndef_message;

	*count = msg->recordCount;

	return NET_NFC_OK;
}

API void net_nfc_ndef_print_message (ndef_message_h ndef_message )
{
	net_nfc_util_print_ndef_message ((ndef_message_s *)(ndef_message) );
}


API net_nfc_error_e net_nfc_search_record_by_type (ndef_message_h ndef_message, net_nfc_record_tnf_e tnf, data_h type, ndef_record_h* record)
{
	return net_nfc_util_search_record_by_type ((ndef_message_s*)ndef_message, tnf, (data_s *)type, (ndef_record_s**)record);
}

API net_nfc_error_e net_nfc_append_record_by_index (ndef_message_h ndef_message, int index, ndef_record_h record)
{
	return net_nfc_util_append_record_by_index ((ndef_message_s *) ndef_message, index, (ndef_record_s *) record);
}

API net_nfc_error_e net_nfc_get_record_by_index (ndef_message_h ndef_message, int index, ndef_record_h*  record)
{
	return net_nfc_util_get_record_by_index ((ndef_message_s*) ndef_message, index, (ndef_record_s**) record);
}

API net_nfc_error_e net_nfc_remove_record_by_index (ndef_message_h ndef_message, int index)
{
	return net_nfc_util_remove_record_by_index ((ndef_message_s*)ndef_message, index);
}

API net_nfc_error_e net_nfc_retrieve_current_ndef_message(ndef_message_h* ndef_message)
{
	net_nfc_error_e result = NET_NFC_UNKNOWN_ERROR;
	char file_path[1024] = { 0, };
	FILE *fp = NULL;

	if (ndef_message == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	snprintf(file_path, sizeof(file_path), "%s/%s/%s", NET_NFC_MANAGER_DATA_PATH, NET_NFC_MANAGER_DATA_PATH_MESSAGE, NET_NFC_MANAGER_NDEF_FILE_NAME);

	if ((fp = fopen(file_path, "r")) != NULL)
	{
		long int size = 0;

		/* rewind to start of file */
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		DEBUG_CLIENT_MSG("message length = [%ld]", size);

		if (size > 0)
		{
			uint8_t *buffer = NULL;

			_net_nfc_util_alloc_mem(buffer, size);
			if (buffer != NULL)
			{
				/* read fully */
				if ((size = fread(buffer, 1, size, fp)) > 0)
				{
					data_h data = NULL;
					if ((result = net_nfc_create_data(&data, buffer, size)) == NET_NFC_OK)
					{
						result = net_nfc_create_ndef_message_from_rawdata(ndef_message, data);

						net_nfc_free_data(data);
					}
				}

				_net_nfc_util_free_mem(buffer);
			}
		}
		else
		{
			result = NET_NFC_ALLOC_FAIL;
		}

		fclose(fp);
	}
	else
	{
		result = NET_NFC_NO_NDEF_MESSAGE;
	}

	return result;
}
