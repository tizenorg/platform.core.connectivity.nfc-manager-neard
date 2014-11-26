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


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "net_nfc.h"
#include "net_nfc_sign_record.h"
#include "ndef-tool.h"

typedef struct _net_nfc_sub_field_s
{
	uint16_t length;
	uint8_t value[0];
}
__attribute__((packed)) net_nfc_sub_field_s;

typedef struct _net_nfc_signature_record_s
{
	uint8_t version;
	uint8_t sign_type : 7;
	uint8_t uri_present : 1;
	net_nfc_sub_field_s signature;
}
__attribute__((packed)) net_nfc_signature_record_s;

typedef struct _net_nfc_certificate_chain_s
{
	uint8_t num_of_certs : 4;
	uint8_t cert_format : 3;
	uint8_t uri_present : 1;
	uint8_t cert_store[0];
}
__attribute__((packed)) net_nfc_certificate_chain_s;

void _display_ndef_message(ndef_message_s *msg);

static void _display_buffer(char *title, uint8_t *buffer, uint32_t length)
{
	int32_t i;

	if (0 < length)
	{
		fprintf(stdout, " %s[%d] = {", title, length);

		for (i = 0; i < length; i++)
		{
			if ((i % 16) == 0)
				fprintf(stdout, "\n");

			if ((i % 8) == 0)
				fprintf(stdout, "  ");

			fprintf(stdout, "%02X ", buffer[i]);
		}

		fprintf(stdout, "\n };\n");
	}
	else
	{
		fprintf(stdout, " %s[%d] = Empty", title, length);
	}
}

static void _display_id(ndef_record_s *record)
{
	data_s *data = NULL;

	net_nfc_get_record_id(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		char temp_buffer[1024] = { 0, };

		memcpy(temp_buffer, (void *)net_nfc_get_data_buffer(data), net_nfc_get_data_length(data));
		fprintf(stdout, " ID string : %s\n", temp_buffer);
	}
}

static void _display_signature(ndef_record_s *record)
{
	data_s *data = NULL;

	_display_id(record);

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		int32_t i;
		char temp_buffer[1024];
		net_nfc_signature_record_s *sign = (net_nfc_signature_record_s *)net_nfc_get_data_buffer(data);

		fprintf(stdout, " Version : %d\n", sign->version);
		fprintf(stdout, " Signature URI present : %s\n", sign->uri_present ? "present" : "not present");

		switch (sign->sign_type)
		{
		case NET_NFC_SIGN_TYPE_NO_SIGN :
			fprintf(stdout, " Signing method : Not signed (%d)\n", sign->sign_type);
			break;

		case NET_NFC_SIGN_TYPE_PKCS_1 :
			fprintf(stdout, " Signing method : RSASSA-PSS with SHA1 (%d)\n", sign->sign_type);
			break;

		case NET_NFC_SIGN_TYPE_PKCS_1_V_1_5 :
			fprintf(stdout, " Signing method : PKCS #1 v1.5 with SHA1 (%d)\n", sign->sign_type);
			break;

		case NET_NFC_SIGN_TYPE_DSA :
			fprintf(stdout, " Signing method : DSA (%d)\n", sign->sign_type);
			break;

		case NET_NFC_SIGN_TYPE_ECDSA :
			fprintf(stdout, " Signing method : ECDSA (%d)\n", sign->sign_type);
			break;

		default :
			fprintf(stdout, " Signing method : Unknown (%d)\n", sign->sign_type);
			break;
		}

		if (sign->uri_present)
		{
			memset(temp_buffer, 0, sizeof(temp_buffer));
			memcpy(temp_buffer, sign->signature.value, sign->signature.length);
			fprintf(stdout, " URI : %s\n", temp_buffer);
		}
		else
		{
			_display_buffer("Signature", sign->signature.value, sign->signature.length);
		}

		net_nfc_certificate_chain_s *chain = (net_nfc_certificate_chain_s *)(sign->signature.value + sign->signature.length);
		fprintf(stdout, " Cert. URI : %s\n", chain->uri_present ? "present" : "not present");

		switch (chain->cert_format)
		{
		case NET_NFC_CERT_FORMAT_X_509 :
			fprintf(stdout, " Cert. format : X.509 (%d)\n", chain->cert_format);
			break;

		case NET_NFC_CERT_FORMAT_X9_86 :
			fprintf(stdout, " Cert. format : X9.86 (%d)\n", chain->cert_format);
			break;

		default :
			fprintf(stdout, " Cert. format : Unknown (%d)\n", chain->cert_format);
			break;
		}

		fprintf(stdout, " Cert. count : %d\n", chain->num_of_certs);

		net_nfc_sub_field_s *field = NULL;

		for (i = 0, field = (net_nfc_sub_field_s *)chain->cert_store; i < chain->num_of_certs; i++, field = (net_nfc_sub_field_s *)(field->value + field->length))
		{
			memset(temp_buffer, 0, sizeof(temp_buffer));
			snprintf(temp_buffer, sizeof(temp_buffer), "Certificate %d", i);
			_display_buffer(temp_buffer, field->value, field->length);
		}

		if (chain->uri_present)
		{
			memset(temp_buffer, 0, sizeof(temp_buffer));
			memcpy(temp_buffer, field->value, field->length);
			fprintf(stdout, " URI : %s\n", temp_buffer);
		}
	}
}

static void _display_smart_poster(ndef_record_s *record)
{
	data_s *data = NULL;

	_display_id(record);

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		char temp_buffer[1024] = { 0, };

		memcpy(temp_buffer, (void *)net_nfc_get_data_buffer(data), net_nfc_get_data_length(data));
		fprintf(stdout, " Type string : %s (Signature)\n", temp_buffer);
	}
}

static void _display_text(ndef_record_s *record)
{
	data_s *data = NULL;
	net_nfc_encode_type_e encoding = 0;

	_display_id(record);

	net_nfc_get_encoding_type_from_text_record(record, &encoding);
	switch (encoding)
	{
	case NET_NFC_ENCODE_UTF_8 :
		fprintf(stdout, " Encoding : UTF-8\n");
		break;

	case NET_NFC_ENCODE_UTF_16 :
		fprintf(stdout, " Encoding : UTF-16\n");
		break;
	}

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);
		uint32_t length = net_nfc_get_data_length(data);
		char temp_buffer[1024] = { 0, };
		int code_length = buffer[0] & 0x3F;

		memcpy(temp_buffer, buffer + 1, code_length);
		fprintf(stdout, " Language code[%d] : %s\n", code_length, temp_buffer);

		memset(temp_buffer, 0, sizeof(temp_buffer));

		memcpy(temp_buffer, buffer + code_length + 1, length - code_length - 1);
		fprintf(stdout, " Text[%d] : %s\n", length - code_length - 1, temp_buffer);
	}
}

static void _display_uri(ndef_record_s *record)
{
	data_s *data = NULL;

	_display_id(record);

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);
		uint32_t length = net_nfc_get_data_length(data);
		char temp_buffer[1024] = { 0, };
		char *uri = NULL;

		fprintf(stdout, " URI scheme : 0x%02X\n", buffer[0]);

		memcpy(temp_buffer, buffer + 1, length - 1);
		fprintf(stdout, " Raw URI payload[%d] : %s\n", length - 1, temp_buffer);

		net_nfc_create_uri_string_from_uri_record(record, &uri);
		if (uri != NULL)
		{
			fprintf(stdout, " Resolved URI[%d] : %s\n", strlen(uri), uri);
			free(uri);
		}
	}
}

static void _display_hs(ndef_record_s *record)
{
	data_s *data = NULL;

	_display_id(record);

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);
		uint32_t length = net_nfc_get_data_length(data);

		fprintf(stdout, " Version : %d.%d\n", ((*buffer) >> 4) & 0x0F,
				(*buffer) & 0x0F);
		fprintf(stdout, " -- Containing NDEF message -- \n");

		ndef_message_s *msg;
		data_s *temp;

		net_nfc_create_data(&temp, buffer + 1, length - 1);
		net_nfc_create_ndef_message_from_rawdata(&msg, temp);

		_display_ndef_message(msg);

		net_nfc_free_ndef_message(msg);
		net_nfc_free_data(temp);
	}
}

static void _display_hr(ndef_record_s *record)
{
	data_s *data = NULL;

	_display_id(record);

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);
		uint32_t length = net_nfc_get_data_length(data);

		fprintf(stdout, " Version : %d.%d\n", ((*buffer) >> 4) & 0x0F,
				(*buffer) & 0x0F);
		fprintf(stdout, " -- Containing NDEF message -- \n");

		ndef_message_s *msg;
		data_s *temp;

		net_nfc_create_data(&temp, buffer + 1, length - 1);
		net_nfc_create_ndef_message_from_rawdata(&msg, temp);

		_display_ndef_message(msg);

		net_nfc_free_ndef_message(msg);
		net_nfc_free_data(temp);
	}
}

static void _display_ac(ndef_record_s *record)
{
	data_s *data = NULL;

	_display_id(record);

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);
		int i, len;
		const char *flag = "unknown";

		switch (*buffer)
		{
		case 0 :
			flag = "inactive";
			break;

		case 1 :
			flag = "active";
			break;

		case 2 :
			flag = "activating";
			break;

		default :
			break;
		}

		fprintf(stdout, " Carrier Flags : 0x%x (%s)\n", *buffer++, flag);
		len = *buffer++;
		fprintf(stdout, " Carrier Data Ref. len : %d byte(s)\n", len);

		for (i = 0; i < len; i++)
		{
			fprintf(stdout, "  Carrier Data Ref. [%d] : %c\n", i, *buffer++);
		}

		len = *buffer++;
		fprintf(stdout, " Aux. Data Ref. len : %d byte(s)\n", len);

		for (i = 0; i < len; i++)
		{
			fprintf(stdout, "  Aux. Data Ref. [%d] : %c\n", i, *buffer++);
		}
	}
}

static void _display_hc(ndef_record_s *record)
{
	data_s *data = NULL;

	_display_id(record);

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);
		int i, len;
		const char *flag = "unknown";
		uint8_t ctf = (*buffer) & 0x07;

		switch (ctf)
		{
		case 1 :
			flag = "Well-known type";
			break;

		case 2 :
			flag = "MIME type";
			break;

		case 3 :
			flag = "Absolute URI";
			break;

		case 4 :
			flag = "External";
			break;

		default :
			break;
		}

		fprintf(stdout, " Carrier type format : 0x%02x (%s)\n", ctf, flag);
		buffer++;

		len = *buffer++;
		fprintf(stdout, " Carrier type length : %d byte(s)\n", len);

		for (i = 0; i < len; i++)
		{
			fprintf(stdout, "  Carrier type [%d] : 0x%02x\n", i, *buffer++);
		}

		len = net_nfc_get_data_length(data) - len - 2;

		for (i = 0; i < len; i++)
		{
			fprintf(stdout, "  Carrier data [%d] : 0x%02x\n", i, *buffer++);
		}
	}
}

static void _display_cr(ndef_record_s *record)
{
	data_s *data = NULL;

	_display_id(record);

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);

		fprintf(stdout, " Random number : %02X %02X\n", buffer[0], buffer[1]);
	}
}

static void _display_err(ndef_record_s *record)
{
	data_s *data = NULL;

	_display_id(record);

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);
		const char *reason = "unknown";

		switch (buffer[0])
		{
		case 1 :
			reason = "temporary memory constraint";
			break;

		case 2 :
			reason = "permanent memory constraint";
			break;

		case 3 :
			reason = "carrier-specific constraint";
			break;

		default :
			break;
		}

		fprintf(stdout, " Error reason : %s\n", reason);
		fprintf(stdout, " Error data : %d ms\n", buffer[1]);
	}
}

static void _display_well_known(ndef_record_s *record)
{
	data_s *data = NULL;

	net_nfc_get_record_type(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);
		uint32_t length = net_nfc_get_data_length(data);
		char temp_buffer[1024] = { 0, };

		memcpy(temp_buffer, buffer, length);

		if (strncmp(temp_buffer, "Sig", 3) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (Signature)\n", length, temp_buffer);
			_display_signature(record);
		}
		else if (strncmp(temp_buffer, "Sp", 2) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (Smart poster)\n", length, temp_buffer);
			_display_smart_poster(record);
		}
		else if (strncmp(temp_buffer, "Gc", 2) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (Generic control)\n", length, temp_buffer);
		}
		else if (strncmp(temp_buffer, "U", 1) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (URI)\n", length, temp_buffer);
			_display_uri(record);
		}
		else if (strncmp(temp_buffer, "T", 1) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (Text)\n", length, temp_buffer);
			_display_text(record);
		}
		else if (strncmp(temp_buffer, "Hc", 2) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (Handover carrier)\n", length, temp_buffer);
			_display_hc(record);
		}
		else if (strncmp(temp_buffer, "Hr", 2) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (Handover request)\n", length, temp_buffer);
			_display_hr(record);
		}
		else if (strncmp(temp_buffer, "Hs", 2) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (Handover select)\n", length, temp_buffer);
			_display_hs(record);
		}
		else if (strncmp(temp_buffer, "ac", 2) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (alternative carrier)\n", length, temp_buffer);
			_display_ac(record);
		}
		else if (strncmp(temp_buffer, "cr", 2) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (Collision Resolution)\n", length, temp_buffer);
			_display_cr(record);
		}
		else if (strncmp(temp_buffer, "err", 3) == 0)
		{
			fprintf(stdout, " Type string[%d] : %s (Error record)\n", length, temp_buffer);
			_display_err(record);
		}
		else
		{
			fprintf(stdout, " Type string[%d] : %s (Unknown)\n", length, temp_buffer);
		}
	}
}

static void _display_general_record(ndef_record_s *record)
{
	data_s *data = NULL;

	net_nfc_get_record_type(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);
		uint32_t length = net_nfc_get_data_length(data);
		char temp_buffer[1024] = { 0, };

		memcpy(temp_buffer, buffer, length);

		fprintf(stdout, " Type string[%d] : %s\n", length, temp_buffer);
	}

	_display_id(record);

	net_nfc_get_record_payload(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint32_t length = net_nfc_get_data_length(data);
		uint8_t temp_buffer[512] = { 0, };

		if (length > sizeof(temp_buffer))
		{
			memcpy(temp_buffer, net_nfc_get_data_buffer(data), sizeof(temp_buffer));

			fprintf(stdout, " Real payload length : %d\n", length);
			_display_buffer("Abb. payload", temp_buffer, sizeof(temp_buffer));
		}
		else
		{
			_display_buffer("Payload", net_nfc_get_data_buffer(data), length);
		}
	}
}

static void _display_absolute_uri(ndef_record_s *record)
{
	data_s *data = NULL;

	_display_id(record);

	net_nfc_get_record_type(record, &data);
	if (net_nfc_get_data_length(data) > 0)
	{
		uint8_t *buffer = net_nfc_get_data_buffer(data);
		uint32_t length = net_nfc_get_data_length(data);
		char temp_buffer[2048] = { 0, };

		memcpy(temp_buffer, buffer, length);

		fprintf(stdout, " URI[%d] : %s\n", length, temp_buffer);
	}
#if 0
	nfc_ndef_record_get_payload(record, &buffer, &length);
	if (0 < length)
	{
		char temp_buffer[512] = { 0, };

		if (length > sizeof(temp_buffer))
		{
			memcpy(temp_buffer, buffer, sizeof(temp_buffer));

			fprintf(stdout, " Real payload length : %d\n", length);
			_display_buffer("Abb. payload", temp_buffer, sizeof(temp_buffer));
		}
		else
		{
			_display_buffer("Payload", temp_buffer, length);
		}
	}
#endif
}

static void _display_tnf(ndef_record_s *record)
{
	net_nfc_record_tnf_e tnf = NET_NFC_RECORD_UNKNOWN;

	net_nfc_get_record_tnf(record, &tnf);

	switch (tnf)
	{
	case NET_NFC_RECORD_EMPTY :
		fprintf(stdout, " TNF : Empty (%d)\n", tnf);
		break;

	case NET_NFC_RECORD_WELL_KNOWN_TYPE :
		fprintf(stdout, " TNF : Well-known (%d)\n", tnf);
		_display_well_known(record);
		break;

	case NET_NFC_RECORD_MIME_TYPE :
		fprintf(stdout, " TNF : MIME (%d)\n", tnf);
		_display_general_record(record);
		break;

	case NET_NFC_RECORD_URI :
		fprintf(stdout, " TNF : Absolute URI (%d)\n", tnf);
		_display_absolute_uri(record);
		break;

	case NET_NFC_RECORD_EXTERNAL_RTD :
		fprintf(stdout, " TNF : External (%d)\n", tnf);
		_display_general_record(record);
		break;

	case NET_NFC_RECORD_UNKNOWN :
		fprintf(stdout, " TNF : Unknown (%d)\n", tnf);
		_display_general_record(record);
		break;

	case NET_NFC_RECORD_UNCHAGNED :
		fprintf(stdout, " TNF : Unchanged (%d)\n", tnf);
		_display_general_record(record);
		break;

	default :
		fprintf(stdout, " Invalid TNF\n");
		break;
	}
}

static void _display_record_length(ndef_record_s *record)
{
	int length = 2; /* header : 1 byte, type length : 1 byte */

	net_nfc_record_tnf_e tnf = NET_NFC_RECORD_UNKNOWN;

	net_nfc_get_record_tnf(record, &tnf);
	if (tnf != NET_NFC_RECORD_EMPTY &&
			tnf != NET_NFC_RECORD_UNKNOWN &&
			tnf != NET_NFC_RECORD_UNCHAGNED)
	{
		data_s *data;

		net_nfc_get_record_type(record, &data);
		length += net_nfc_get_data_length(data);
	}

	uint8_t header;

	net_nfc_get_record_flags(record, &header);
	/* to check if payloadlength is 8bit or 32bit*/
	if (net_nfc_get_record_sr(header) == 1)
	{
		/* ++ is for the Payload Length Byte */
		length++;/* for short record*/
	}
	else
	{
		/* + NET_NFC_NDEF_NORMAL_RECORD_BYTE is for the Payload Length Byte */
		length += 4;/* for normal record*/
	}

	/* for non empty record */
	if (tnf != NET_NFC_RECORD_EMPTY)
	{
		data_s *data;

		net_nfc_get_record_payload(record, &data);
		length += net_nfc_get_data_length(data);
	}

	/* ID and IDlength are present only if IL flag is set*/
	if (net_nfc_get_record_il(header) == 1)
	{
		data_s *data;

		/* ++ is for the ID Length Byte */
		length++;

		net_nfc_get_record_id(record, &data);
		length += net_nfc_get_data_length(data);
	}

	fprintf(stdout, " Length : %d\n", length);
}

static void _display_record(ndef_record_s *record, int index)
{
	uint8_t header;

	fprintf(stdout, "------------------ NDEF record %02d ------------------\n", index);

	_display_record_length(record);

	net_nfc_get_record_flags(record, &header);

	fprintf(stdout, " MB[%d], ME[%d], CF[%d], SR[%d], IL[%d]\n", net_nfc_get_record_mb(header), net_nfc_get_record_me(header),
			net_nfc_get_record_cf(header), net_nfc_get_record_sr(header), net_nfc_get_record_il(header));

	_display_tnf(record);

	fprintf(stdout, "----------------------------------------------------\n");
}

void _display_ndef_message(ndef_message_s *msg)
{
	uint32_t length;
	int count = 0;
	int32_t i = 0;
	ndef_record_s *record = NULL;

	net_nfc_get_ndef_message_byte_length(msg, &length);

	fprintf(stdout, "================ NDEF message begin ================\n");
	fprintf(stdout, "Length : %d\n", length);

	net_nfc_get_ndef_message_record_count(msg, &count);

	for (i = 0; i < count; i++)
	{
		net_nfc_get_record_by_index(msg, i, &record);

		_display_record(record, i);
	}

	fprintf(stdout, "================= NDEF message end =================\n");
}


void ndef_tool_display_ndef_message_from_file(const char *file_name)
{
	int length = 0;
	ndef_message_s *msg = NULL;

	if ((length = ndef_tool_read_ndef_message_from_file(file_name, &msg)) > 0)
	{
		fprintf(stdout, "\n");

		_display_ndef_message(msg);

		fprintf(stdout, "\n");

		net_nfc_free_ndef_message(msg);
	}
	else
	{
		fprintf(stdout, "No file exists : %s\n\n", file_name);
	}
}

void _display_target_type(net_nfc_target_type_e type)
{
	char *text = NULL;

	switch (type)
	{
	case NET_NFC_GENERIC_PICC :
		text = "Generic";
		break;

	case NET_NFC_ISO14443_A_PICC :
		text = "ISO 14443 A";
		break;

	case NET_NFC_ISO14443_4A_PICC :
		text = "ISO 14443 4A";
		break;

	case NET_NFC_ISO14443_3A_PICC :
		text = "ISO 14443 3A";
		break;

	case NET_NFC_MIFARE_MINI_PICC :
		text = "Mifare Mini";
		break;

	case NET_NFC_MIFARE_1K_PICC :
		text = "Mifare 1k";
		break;

	case NET_NFC_MIFARE_4K_PICC :
		text = "Mifare 4k";
		break;

	case NET_NFC_MIFARE_ULTRA_PICC :
		text = "Mifare Ultralight";
		break;

	case NET_NFC_MIFARE_DESFIRE_PICC :
		text = "Mifare Desfire";
		break;

	case NET_NFC_ISO14443_B_PICC :
		text = "ISO 14443 B";
		break;

	case NET_NFC_ISO14443_4B_PICC :
		text = "ISO 14443 4B";
		break;

	case NET_NFC_ISO14443_BPRIME_PICC :
		text = "ISO 14443 B Prime";
		break;

	case NET_NFC_FELICA_PICC :
		text = "Felica";
		break;

	case NET_NFC_JEWEL_PICC :
		text = "Jewel";
		break;

	case NET_NFC_ISO15693_PICC :
		text = "ISO 15693";
		break;

	case NET_NFC_NFCIP1_TARGET :
		text = "P2P Target (NFCIP1)";
		break;

	case NET_NFC_NFCIP1_INITIATOR :
		text = "P2P Initiator (NFCIP1)";
		break;

	default :
		text = "Unknown";
		break;
	}

	fprintf(stdout, "Target Type : %s (%d)\n", text, type);
}

void _display_tag_info(net_nfc_target_info_s *target)
{
	int count, i;
	char **keys;
	data_s *data;

	fprintf(stdout, "Target information\n");
	net_nfc_get_tag_info_keys(target, &keys, &count);
	for (i = 0; i < count; i++)
	{
		if (keys[i] != NULL)
		{
			if (net_nfc_get_tag_info_value(target, keys[i], &data) == 0)
			{
				_display_buffer(keys[i], net_nfc_get_data_buffer(data), net_nfc_get_data_length(data));
			}
		}
	}
}

void ndef_tool_display_discovered_tag(net_nfc_target_info_s *target)
{
	net_nfc_target_type_e type;
	bool is_ndef;
	uint32_t size;

	if (target == NULL)
	{
		return;
	}

	fprintf(stdout, "\n************* Tag discovered *************\n");

	net_nfc_get_tag_type(target, &type);
	_display_target_type(type);

	net_nfc_get_tag_ndef_support(target, &is_ndef);
	fprintf(stdout, "Including NDEF? : %s\n", is_ndef ? "Yes" : "No");

	net_nfc_get_tag_max_data_size(target, &size);
	fprintf(stdout, "Maximum size : %d\n", size);

	net_nfc_get_tag_actual_data_size(target, &size);
	fprintf(stdout, "Actual size : %d\n", size);

	_display_tag_info(target);

	fprintf(stdout, "******************************************\n");
}

void ndef_tool_display_discovered_target(net_nfc_target_handle_s *handle)
{
	if (handle == NULL)
		return;

	fprintf(stdout, "\n************* Target discovered *************\n");
	fprintf(stdout, "Target Type : P2P Target (NFCIP1) (%d)\n", NET_NFC_NFCIP1_TARGET);
	fprintf(stdout, "*********************************************\n");
}

