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

#include "net_nfc_debug_internal.h"
#include "net_nfc_util_defines.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_ndef_record.h"

net_nfc_error_e net_nfc_util_free_record(ndef_record_s *record)
{
	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);

	if (record->type_s.buffer != NULL)
		_net_nfc_util_free_mem(record->type_s.buffer);
	if (record->id_s.buffer != NULL)
		_net_nfc_util_free_mem(record->id_s.buffer);
	if (record->payload_s.buffer != NULL)
		_net_nfc_util_free_mem(record->payload_s.buffer);

	_net_nfc_util_free_mem(record);

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_util_create_record(net_nfc_record_tnf_e recordType,
		const data_s *typeName, const data_s *id, const data_s *payload,
		ndef_record_s **record)
{
	ndef_record_s *record_temp = NULL;

	RETV_IF(NULL == typeName, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == payload, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);

	RETV_IF(recordType < NET_NFC_RECORD_EMPTY, NET_NFC_OUT_OF_BOUND);
	RETV_IF(recordType > NET_NFC_RECORD_UNCHAGNED, NET_NFC_OUT_OF_BOUND);

	/* empty_tag */
	if (NET_NFC_RECORD_EMPTY == recordType)
	{
		if ((typeName->buffer != NULL) || (payload->buffer != NULL)
				|| (id->buffer != NULL) || (typeName->length != 0) || (payload->length != 0)
				|| (id->length != 0))
		{
			return NET_NFC_NULL_PARAMETER;
		}
	}

	_net_nfc_util_alloc_mem(record_temp, sizeof(ndef_record_s));
	if (NULL == record_temp)
		return NET_NFC_ALLOC_FAIL;

	// set type name and length and  TNF field
	record_temp->TNF = recordType;
	record_temp->type_s.length = typeName->length;

	if(record_temp->type_s.length > 0)
	{
		_net_nfc_util_alloc_mem(record_temp->type_s.buffer, record_temp->type_s.length);
		if (NULL == record_temp->type_s.buffer)
		{
			_net_nfc_util_free_mem(record_temp);

			return NET_NFC_ALLOC_FAIL;
		}

		memcpy(record_temp->type_s.buffer, typeName->buffer, record_temp->type_s.length);
	}
	else
	{
		record_temp->type_s.buffer = NULL;
		record_temp->type_s.length = 0;
	}

	// set payload
	record_temp->payload_s.length = payload->length;
	if(payload->length >0)
	{
		_net_nfc_util_alloc_mem(record_temp->payload_s.buffer,
			record_temp->payload_s.length);

		if (NULL == record_temp->payload_s.buffer)
		{
			_net_nfc_util_free_mem(record_temp->type_s.buffer);
			_net_nfc_util_free_mem(record_temp);

			return NET_NFC_ALLOC_FAIL;
		}

		memcpy(record_temp->payload_s.buffer, payload->buffer,
			record_temp->payload_s.length);
	}
	else
	{
		record_temp->payload_s.buffer = NULL;
		record_temp->payload_s.length = 0;
	}

	if (payload->length < 256)
		record_temp->SR = 1;
	else
		record_temp->SR = 0;

	// set id and id length and IL field
	if (id != NULL && id->buffer != NULL && id->length > 0)
	{
		record_temp->id_s.length = id->length;
		_net_nfc_util_alloc_mem(record_temp->id_s.buffer, record_temp->id_s.length);
		if (NULL == record_temp->id_s.buffer)
		{
			_net_nfc_util_free_mem(record_temp->payload_s.buffer);
			_net_nfc_util_free_mem(record_temp->type_s.buffer);
			_net_nfc_util_free_mem(record_temp);

			return NET_NFC_ALLOC_FAIL;
		}

		memcpy(record_temp->id_s.buffer, id->buffer, record_temp->id_s.length);
		record_temp->IL = 1;
	}
	else
	{
		record_temp->IL = 0;
		record_temp->id_s.buffer = NULL;
		record_temp->id_s.length = 0;
	}

	// this is default value
	record_temp->MB = 1;
	record_temp->ME = 1;

	record_temp->next = NULL;

	*record = record_temp;

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_util_create_uri_type_record(const char *uri,
		net_nfc_schema_type_e protocol_schema, ndef_record_s **record)
{
	data_s type_data;
	net_nfc_error_e error;
	data_s payload_data = { NULL, 0 };

	RETV_IF(NULL == uri, NET_NFC_NULL_PARAMETER);

	payload_data.length = strlen((char *)uri) + 1;

	RETV_IF(1 == payload_data.length, NET_NFC_INVALID_PARAM);

	_net_nfc_util_alloc_mem(payload_data.buffer, payload_data.length);
	if (NULL == payload_data.buffer)
		return NET_NFC_ALLOC_FAIL;

	payload_data.buffer[0] = protocol_schema;	/* first byte of payload is protocol scheme */
	memcpy(payload_data.buffer + 1, uri, payload_data.length - 1);

	type_data.length = 1;
	type_data.buffer = (uint8_t *)URI_RECORD_TYPE;

	error = net_nfc_util_create_record(NET_NFC_RECORD_WELL_KNOWN_TYPE,
			&type_data, NULL, &payload_data, record);

	_net_nfc_util_free_mem(payload_data.buffer);

	return error;
}

net_nfc_error_e net_nfc_util_create_text_type_record(const char *text,
	const char *lang_code_str, net_nfc_encode_type_e encode, ndef_record_s **record)
{
	int offset = 0;
	data_s type_data;
	int controll_byte;
	data_s payload_data;

	RETV_IF(NULL == text, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == lang_code_str, NET_NFC_NULL_PARAMETER);

	RETVM_IF(encode < NET_NFC_ENCODE_UTF_8 || NET_NFC_ENCODE_UTF_16 < encode ,
		NET_NFC_OUT_OF_BOUND, "encode(%d) is invalid", encode);

	payload_data.length = strlen((char *)text) + strlen(lang_code_str) + 1;

	_net_nfc_util_alloc_mem(payload_data.buffer, payload_data.length);
	if (NULL == payload_data.buffer)
	{
		return NET_NFC_ALLOC_FAIL;
	}

	controll_byte = strlen(lang_code_str) & 0x3F;
	if (NET_NFC_ENCODE_UTF_16 == encode)
	{
		controll_byte = controll_byte | 0x80;
	}

	payload_data.buffer[0] = controll_byte;

	offset = 1;
	memcpy(payload_data.buffer + offset, lang_code_str, strlen(lang_code_str));

	offset = offset + strlen(lang_code_str);
	memcpy(payload_data.buffer + offset, (char *)text, strlen((char *)text));

	type_data.length = 1;
	type_data.buffer = (uint8_t *)TEXT_RECORD_TYPE;

	net_nfc_util_create_record(NET_NFC_RECORD_WELL_KNOWN_TYPE, &type_data, NULL,
		&payload_data, record);

	_net_nfc_util_free_mem(payload_data.buffer);

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_util_set_record_id(ndef_record_s *record,
	uint8_t *data, int length)
{
	RETV_IF(NULL == data, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);
	RETV_IF(length < 1, NET_NFC_OUT_OF_BOUND);

	if (record->id_s.buffer != NULL && record->id_s.length > 0)
		_net_nfc_util_free_mem(record->id_s.buffer);

	_net_nfc_util_alloc_mem(record->id_s.buffer, length);

	if (record->id_s.buffer == NULL)
		return NET_NFC_ALLOC_FAIL;

	memcpy(record->id_s.buffer, data, length);
	record->id_s.length = length;
	record->IL = 1;

	return NET_NFC_OK;
}

uint32_t net_nfc_util_get_record_length(ndef_record_s *Record)
{
	uint32_t RecordLength = 1;

	RETV_IF(NULL == Record, 0);

	/* Type length is present only for following TNF
	   NET_NFC_TNF_NFCWELLKNOWN
	   NET_NFC_TNF_MEDIATYPE
	   SLP_FRINET_NFC_NDEFRECORD_TNF_ABSURI
	   SLP_FRINET_NFC_NDEFRECORD_TNF_NFCEXT
	   */

	/* ++ is for the Type Length Byte */
	RecordLength++;
	if (Record->TNF != NET_NFC_NDEF_TNF_EMPTY &&
			Record->TNF != NET_NFC_NDEF_TNF_UNKNOWN &&
			Record->TNF != NET_NFC_NDEF_TNF_UNCHANGED)
	{
		RecordLength += Record->type_s.length;
	}

	/* to check if payloadlength is 8bit or 32bit*/
	if (Record->SR != 0)
	{
		/* ++ is for the Payload Length Byte */
		RecordLength++;/* for short record*/
	}
	else
	{
		/* + NET_NFC_NDEF_NORMAL_RECORD_BYTE is for the Payload Length Byte */
		RecordLength += 4;
	}

	/* for non empty record */
	if (Record->TNF != NET_NFC_NDEF_TNF_EMPTY)
		RecordLength += Record->payload_s.length;

	/* ID and IDlength are present only if IL flag is set*/
	if (Record->IL != 0)
	{
		RecordLength += Record->id_s.length;
		/* ++ is for the ID Length Byte */
		RecordLength++;
	}

	return RecordLength;
}

net_nfc_error_e net_nfc_util_create_uri_string_from_uri_record(
	ndef_record_s *record, char **uri)
{
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == uri, NET_NFC_INVALID_PARAM);
	RETV_IF(NULL == record, NET_NFC_INVALID_PARAM);

	*uri = NULL;

	if (NET_NFC_RECORD_WELL_KNOWN_TYPE == record->TNF &&
			(record->type_s.length == 1 && record->type_s.buffer[0] == 'U'))
	{
		data_s *payload = &record->payload_s;

		if (payload->length > 0)
		{
			int length = 0;
			const char *scheme = NULL;

			/* buffer length include a schema byte.
			 * so it does not need to allocate one more byte for string. */
			if ((scheme = net_nfc_util_get_schema_string(payload->buffer[0])) != NULL)
				length = strlen(scheme);

			*uri = (char *)calloc(1, length + payload->length);
			if (*uri != NULL)
			{
				if (length > 0)
					memcpy(*uri, scheme, length);
				memcpy(*uri + length, payload->buffer + 1, payload->length - 1);
			}
			else
			{
				result = NET_NFC_ALLOC_FAIL;
			}
		}
		else
		{
			NFC_ERR("invalid payload in record");
		}
	}
	else if (NET_NFC_RECORD_URI == record->TNF)
	{
		data_s *type = &record->type_s;

		if (type->length > 0)
		{
			*uri = (char *)calloc(1, type->length + 1);

			if (*uri != NULL)
				memcpy(*uri, type->buffer, type->length);
			else
				result = NET_NFC_ALLOC_FAIL;
		}
	}
	else
	{
		NFC_ERR("no uri record");
		result = NET_NFC_NDEF_RECORD_IS_NOT_EXPECTED_TYPE;
	}

	return result;
}
