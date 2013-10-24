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

#include "net_nfc_ndef_record.h"
#include "net_nfc_ndef_message.h"
#include "net_nfc_data.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_ndef_record.h"

API net_nfc_error_e net_nfc_create_record(ndef_record_s **record,
		net_nfc_record_tnf_e tnf,
		const data_s *typeName,
		const data_s *id,
		const data_s *payload)
{
	return net_nfc_util_create_record(tnf, typeName, id, payload, record);
}

API net_nfc_error_e net_nfc_create_text_type_record(ndef_record_s **record,
		const char* text, const char* language_code_str, net_nfc_encode_type_e encode)
{
	return net_nfc_util_create_text_type_record(text, language_code_str, encode, record);
}

API net_nfc_error_e net_nfc_create_uri_type_record(ndef_record_s **record,
		const char* uri, net_nfc_schema_type_e protocol_schema)
{
	return net_nfc_util_create_uri_type_record(uri, protocol_schema, record);
}

API net_nfc_error_e net_nfc_free_record(ndef_record_s *record)
{
	return net_nfc_util_free_record(record);
}

API net_nfc_error_e net_nfc_get_record_payload(ndef_record_s *record,
		data_s **payload)
{
	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == payload, NET_NFC_NULL_PARAMETER);

	*payload = &(record->payload_s);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_get_record_type(ndef_record_s *record, data_s **type)
{
	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == type, NET_NFC_NULL_PARAMETER);

	*type = &(record->type_s);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_get_record_id(ndef_record_s *record, data_s **id)
{
	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == id, NET_NFC_NULL_PARAMETER);

	*id = &(record->id_s);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_get_record_tnf(ndef_record_s *record,
		net_nfc_record_tnf_e *TNF)
{
	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == TNF, NET_NFC_NULL_PARAMETER);

	*TNF = (net_nfc_record_tnf_e)record->TNF;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_set_record_id(ndef_record_s *record, data_s *id)
{
	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == id, NET_NFC_NULL_PARAMETER);

	return net_nfc_util_set_record_id(record, id->buffer, id->length);
}

API net_nfc_error_e net_nfc_get_record_flags(ndef_record_s *record, uint8_t *flag)
{
	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == flag, NET_NFC_NULL_PARAMETER);

	*flag = record->MB;
	*flag <<= 1;
	*flag += record->ME;
	*flag <<= 1;
	*flag += record->CF;
	*flag <<= 1;
	*flag += record->SR;
	*flag <<= 1;
	*flag += record->IL;
	*flag <<= 3;
	*flag += record->TNF;

	return NET_NFC_OK;
}

API uint8_t net_nfc_get_record_mb(uint8_t flag)
{
	return ((flag >> 7) & 0x01);
}

API uint8_t net_nfc_get_record_me(uint8_t flag)
{
	return ((flag >> 6) & 0x01);
}

API uint8_t net_nfc_get_record_cf(uint8_t flag)
{
	return ((flag >> 5) & 0x01);
}

API uint8_t net_nfc_get_record_sr(uint8_t flag)
{
	return ((flag >> 4) & 0x01);
}

API uint8_t net_nfc_get_record_il(uint8_t flag)
{
	return ((flag >> 3) & 0x01);
}

static bool _is_text_record(ndef_record_s *record)
{
	data_s *type;
	bool result = false;

	if ((net_nfc_get_record_type(record, &type) == NET_NFC_OK)
			&& (strncmp((char *)net_nfc_get_data_buffer(type),
					TEXT_RECORD_TYPE,
					net_nfc_get_data_length(type)) == 0))
	{
		result = true;
	}

	return result;
}

API net_nfc_error_e net_nfc_create_text_string_from_text_record(
		ndef_record_s *record, char **buffer)
{
	data_s *payload;
	net_nfc_error_e result;

	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == buffer, NET_NFC_NULL_PARAMETER);

	*buffer = NULL;

	if (_is_text_record(record) == false)
	{
		NFC_ERR("record type is not matched");
		return NET_NFC_NDEF_RECORD_IS_NOT_EXPECTED_TYPE;
	}

	result = net_nfc_get_record_payload(record, &payload);
	if (result == NET_NFC_OK)
	{
		uint8_t *buffer_temp = net_nfc_get_data_buffer(payload);
		uint32_t buffer_length = net_nfc_get_data_length(payload);

		if (NULL == buffer_temp)
			return NET_NFC_NO_DATA_FOUND;

		int controllbyte = buffer_temp[0];
		int lang_code_length = controllbyte & 0x3F;
		int index = lang_code_length + 1;
		int text_length = buffer_length - (lang_code_length + 1);

		char *temp = NULL;

		_net_nfc_util_alloc_mem(temp, text_length + 1);
		if (temp != NULL)
		{
			memcpy(temp, &(buffer_temp[index]), text_length);
			NFC_DBG("text = [%s]", temp);

			*buffer = temp;
		}
		else
		{
			result = NET_NFC_ALLOC_FAIL;
		}
	}

	return result;
}

API net_nfc_error_e net_nfc_get_languange_code_string_from_text_record(
		ndef_record_s *record, char **lang_code_str)
{
	data_s *payload;
	net_nfc_error_e result;

	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == lang_code_str, NET_NFC_NULL_PARAMETER);

	*lang_code_str = NULL;

	if (_is_text_record(record) == false)
	{
		NFC_ERR("record type is not matched");
		return NET_NFC_NDEF_RECORD_IS_NOT_EXPECTED_TYPE;
	}

	result = net_nfc_get_record_payload(record, &payload);
	if (result == NET_NFC_OK)
	{
		int index = 1;
		char *buffer = NULL;
		uint8_t *buffer_temp = net_nfc_get_data_buffer(payload);

		if (NULL == buffer_temp)
			return NET_NFC_NO_DATA_FOUND;

		int controllbyte = buffer_temp[0];
		int lang_code_length = controllbyte & 0x3F;

		_net_nfc_util_alloc_mem(buffer, lang_code_length + 1);
		if (buffer != NULL)
		{
			memcpy(buffer, &(buffer_temp[index]), lang_code_length);
			NFC_DBG("language code = [%s]", buffer);

			*lang_code_str = buffer;
		}
		else
		{
			result = NET_NFC_ALLOC_FAIL;
		}
	}

	return result;
}

API net_nfc_error_e net_nfc_get_encoding_type_from_text_record(
		ndef_record_s *record, net_nfc_encode_type_e *encoding)
{
	data_s *payload;
	net_nfc_error_e result;

	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == encoding, NET_NFC_NULL_PARAMETER);

	*encoding = NET_NFC_ENCODE_UTF_8;

	if (_is_text_record(record) == false)
	{
		NFC_ERR("record type is not matched");
		return NET_NFC_NDEF_RECORD_IS_NOT_EXPECTED_TYPE;
	}

	result = net_nfc_get_record_payload(record, &payload);
	if (result == NET_NFC_OK)
	{
		uint8_t *buffer_temp = net_nfc_get_data_buffer(payload);

		if (NULL == buffer_temp)
			return NET_NFC_NO_DATA_FOUND;

		int controllbyte = buffer_temp[0];

		if ((controllbyte & 0x80) == 0x80)
			*encoding = NET_NFC_ENCODE_UTF_16;
	}

	return result;
}

API net_nfc_error_e net_nfc_create_uri_string_from_uri_record(
		ndef_record_s *record, char **uri)
{
	return net_nfc_util_create_uri_string_from_uri_record(record, uri);
}
