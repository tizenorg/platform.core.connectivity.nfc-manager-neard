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


#include <sys/param.h>

#include "net_nfc_debug_internal.h"
#include "net_nfc_util_defines.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_ndef_record.h"
#include "net_nfc_util_openssl_internal.h"
#include "net_nfc_util_sign_record.h"

#define IS_SIGN_RECORD(__x) \
	(((__x)->TNF == NET_NFC_RECORD_WELL_KNOWN_TYPE) && \
	 ((__x)->type_s.length == 3) && \
	 (memcmp((__x)->type_s.buffer, "Sig", 3) == 0))

#define IS_EMPTY_RECORD(__x) \
	((__x->TNF == NET_NFC_RECORD_EMPTY))

#define __FILL_SUB_FIELD(__dst, __buf, __len) \
	(__dst)->length = (__len); \
	memcpy((__dst)->value, (__buf), (__dst)->length);

#define __NEXT_SUB_FIELD(__dst) ((__dst)->value + (__dst)->length)

bool _get_records_data_buffer(ndef_record_s *begin_record,
	ndef_record_s *end_record, uint8_t **buffer, uint32_t *length)
{
	uint32_t len = 0;
	bool result = false;
	ndef_record_s *current_record = NULL;

	RETV_IF(NULL == begin_record, result);
	RETV_IF(begin_record == end_record, result);

	/* count total buffer length */
	current_record = begin_record;
	len = 0;

	while (current_record != NULL && current_record != end_record)
	{
		/* type length */
		if (current_record->type_s.buffer != NULL && current_record->type_s.length > 0)
			len += current_record->type_s.length;

		/* ID length */
		if (current_record->id_s.buffer != NULL && current_record->id_s.length > 0)
			len += current_record->id_s.length;

		/* payload length */
		if (current_record->payload_s.buffer != NULL &&
				current_record->payload_s.length > 0)
		{
			len += current_record->payload_s.length;
		}

		current_record = current_record->next;
	}

	if (len > 0)
	{
		uint8_t *buf = NULL;

		_net_nfc_util_alloc_mem(buf, len);
		if (buf != NULL)
		{
			uint32_t offset = 0;

			current_record = begin_record;

			while (offset < len && current_record != NULL && current_record != end_record)
			{
				/* type length */
				if (current_record->type_s.buffer != NULL &&
					current_record->type_s.length > 0)
				{
					memcpy(buf + offset, current_record->type_s.buffer,
						MIN(current_record->type_s.length, len - offset));
					offset += MIN(current_record->type_s.length, len - offset);
				}

				/* ID length */
				if (current_record->id_s.buffer != NULL && current_record->id_s.length > 0)
				{
					memcpy(buf + offset, current_record->id_s.buffer,
						MIN(current_record->id_s.length, len - offset));
					offset += MIN(current_record->id_s.length, len - offset);
				}

				/* payload length */
				if (current_record->payload_s.buffer != NULL &&
					current_record->payload_s.length > 0)
				{
					memcpy(buf + offset, current_record->payload_s.buffer,
						MIN(current_record->payload_s.length, len - offset));
					offset += MIN(current_record->payload_s.length, len - offset);
				}

				current_record = current_record->next;
			}

			*buffer = buf;
			*length = offset;

			result = true;
		}
	}

	return result;
}

net_nfc_error_e net_nfc_util_verify_signature_records(ndef_record_s *begin_record,
	ndef_record_s *sign_record)
{
	int ret;
	uint32_t length = 0;
	uint8_t *buffer = NULL;
	net_nfc_error_e result = NET_NFC_UNKNOWN_ERROR;

	RETV_IF(NULL == sign_record, NET_NFC_INVALID_PARAM);
	RETV_IF(NULL == begin_record, NET_NFC_INVALID_PARAM);
	RETV_IF(begin_record == sign_record, NET_NFC_INVALID_PARAM);

	/* get signed data */
	if (_get_records_data_buffer(begin_record, sign_record, &buffer, &length) == true)
	{
		net_nfc_signature_record_s *sign_info = NULL;
		net_nfc_certificate_chain_s *chain_info = NULL;

		/* parse signature info */
		sign_info = (net_nfc_signature_record_s *)sign_record->payload_s.buffer;

		NFC_DBG("record version : %d", sign_info->version);
		NFC_DBG("signature URI present? : %s", sign_info->uri_present ? "true" : "false");
		NFC_DBG("signature type : %d", sign_info->sign_type);
		NFC_DBG("signature length : %d", sign_info->signature.length);

		if (true == sign_info->uri_present)
		{
			/* TODO */
			/* receive the signature data directed by uri */
			NFC_ERR("NOT IMPLEMENTED (sign_info->uri_present == true)");
			_net_nfc_util_free_mem(buffer);
			return result;
		}

		/* parse certificate chain info */
		chain_info =
			(net_nfc_certificate_chain_s *)__NEXT_SUB_FIELD(&(sign_info->signature));

		SECURE_LOGD("certificate URI present? : %s",
			chain_info->uri_present ? "true" : "false");
		NFC_DBG("certificate format : %d", chain_info->cert_format);
		NFC_DBG("number of certificates : %d", chain_info->num_of_certs);

		if (chain_info->num_of_certs > 0)
		{
			net_nfc_sub_field_s *data_info = NULL;

			data_info = (net_nfc_sub_field_s *)chain_info->cert_store;
			NFC_DBG("certficate length : %d", data_info->length);

			//		DEBUG_MSG_PRINT_BUFFER(data_info->value, data_info->length);

			/* the first certificate is signer's one
			 * verify signature of content */
			ret = net_nfc_util_openssl_verify_signature(sign_info->sign_type,
						buffer,
						length,
						data_info->value,
						data_info->length,
						sign_info->signature.value,
						sign_info->signature.length);

			if (true == ret)
			{
				if (chain_info->num_of_certs > 1)
				{
					int32_t i = 0;
					net_nfc_openssl_verify_context_h context = NULL;

					/* initialize context of verifying certificate */
					context = net_nfc_util_openssl_init_verify_certificate();

					/* add signer's certificate */
					net_nfc_util_openssl_add_certificate_of_signer(context, data_info->value,
						data_info->length);

					/* verify certificate using certificate chain */
					for (i = 1, data_info = (net_nfc_sub_field_s *)__NEXT_SUB_FIELD(data_info);
							i < chain_info->num_of_certs;
							i++, data_info = (net_nfc_sub_field_s *)__NEXT_SUB_FIELD(data_info))
					{
						NFC_DBG("certficate length : %d", data_info->length);
						//DEBUG_MSG_PRINT_BUFFER(data_info->value, data_info->length);

						net_nfc_util_openssl_add_certificate_of_ca(context, data_info->value,
							data_info->length);
					}

					/* if the CA_Uri is present, continue adding certificate from uri */
					if (true == chain_info->uri_present)
					{
						/* TODO : Need to implement */
						NFC_ERR("NOT IMPLEMENTED (found_root == false && chain_info->uri_present == true)");
						net_nfc_util_openssl_release_verify_certificate(context);
						_net_nfc_util_free_mem(buffer);
						return result;

						//NFC_DBG("certficate length : %d", data_info->length);
						//DEBUG_MSG_PRINT_BUFFER(data_info->value, data_info->length);
					}

					/* verify buffer with cert chain and signature bytes */
					if (net_nfc_util_openssl_verify_certificate(context) == true)
						result = NET_NFC_OK;

					net_nfc_util_openssl_release_verify_certificate(context);
				}
				else
				{
					/* TODO : test certificate??? */
					result = NET_NFC_OK;
				}

				NFC_DBG("verifying signature %d", result);
			}
			else
			{
				NFC_ERR("verifying signature failed");
			}
		}
		else
		{
			NFC_ERR("certificate not found");
		}

		_net_nfc_util_free_mem(buffer);
	}
	else
	{
		if(buffer != NULL)
			_net_nfc_util_free_mem(buffer);

		NFC_ERR("_get_records_data_buffer failed");
	}

	return result;
}

net_nfc_error_e net_nfc_util_verify_signature_ndef_message(ndef_message_s *msg)
{
	ndef_record_s *begin_record = NULL;
	ndef_record_s *current_record = NULL;
	net_nfc_error_e result = NET_NFC_UNKNOWN_ERROR;


	begin_record = msg->records;
	current_record = msg->records;

	while (current_record != NULL)
	{
		if (NULL == begin_record)
			begin_record = current_record;

		if (IS_EMPTY_RECORD(current_record))
		{
			begin_record = NULL;
		}
		else if (IS_SIGN_RECORD(current_record))
		{
			result = net_nfc_util_verify_signature_records(begin_record, current_record);

			begin_record = NULL;
		}

		current_record = current_record->next;
	}

	return result;
}

/*
 * sign method
 */
net_nfc_error_e net_nfc_util_sign_records(ndef_message_s *msg, int begin_index,
	int end_index, char *cert_file, char *password)
{
	uint32_t cert_len = 0;
	uint32_t data_len = 0;
	uint32_t cert_count = 0;
	uint8_t *data_buffer = NULL;
	uint8_t *cert_buffer = NULL;
	data_s payload = { NULL, 0 };
	uint8_t signature[1024] = { 0, };
	uint32_t sign_len = sizeof(signature);
	net_nfc_error_e result = NET_NFC_UNKNOWN_ERROR;
	ndef_record_s *begin_record = NULL, *end_record = NULL, *record = NULL;


	net_nfc_util_get_record_by_index(msg, begin_index, &begin_record);
	net_nfc_util_get_record_by_index(msg, end_index, &end_record);

	NFC_DBG("total record count : %d, begin_index : %d, end_index : %d", msg->recordCount,
		begin_index, end_index);

	/* get target data */
	_get_records_data_buffer(begin_record, end_record->next, &data_buffer, &data_len);

	DEBUG_MSG_PRINT_BUFFER(data_buffer, data_len);

	net_nfc_util_openssl_sign_buffer(NET_NFC_SIGN_TYPE_PKCS_1, data_buffer, data_len,
		cert_file, password, signature, &sign_len);

	/* get cert chain */
	net_nfc_util_get_cert_list_from_file(cert_file, password, &cert_buffer, &cert_len,
		&cert_count);

	/* create payload */
	payload.length = sizeof(net_nfc_signature_record_s) + sign_len +
		sizeof(net_nfc_certificate_chain_s) + cert_len;

	_net_nfc_util_alloc_mem(payload.buffer, payload.length);

	net_nfc_signature_record_s *sign_record =
		(net_nfc_signature_record_s *)payload.buffer;
	sign_record->version = 1;
	sign_record->uri_present = 0;
	sign_record->sign_type = NET_NFC_SIGN_TYPE_PKCS_1;

	if (sign_record->uri_present)
	{
		/* TODO */
	}
	else
	{
		__FILL_SUB_FIELD(&(sign_record->signature), signature, sign_len);
	}

	net_nfc_certificate_chain_s *chain =
		(net_nfc_certificate_chain_s *)__NEXT_SUB_FIELD(&(sign_record->signature));

	if (cert_count < 16)
		chain->uri_present = 0;
	else
		chain->uri_present = 1;

	chain->cert_format = NET_NFC_CERT_FORMAT_X_509;
	chain->num_of_certs = cert_count;
	memcpy(chain->cert_store, cert_buffer, cert_len);

	if (chain->uri_present)
	{
		/* TODO */
		NFC_ERR("num_of_certs is greater than 15 [%d]", cert_count);
	}

	/* create record */
	data_s type = { (uint8_t *)"Sig", 3 };

	net_nfc_util_create_record(NET_NFC_RECORD_WELL_KNOWN_TYPE, &type, NULL,
		&payload, &record);

	/* get last record index */
	net_nfc_util_append_record_by_index(msg, end_index + 1, record);

	_net_nfc_util_free_mem(payload.buffer);
	_net_nfc_util_free_mem(cert_buffer);
	_net_nfc_util_free_mem(data_buffer);

	return result;
}

net_nfc_error_e net_nfc_util_sign_ndef_message(ndef_message_s *msg,
	char *cert_file, char *password)
{
	net_nfc_error_e result = NET_NFC_UNKNOWN_ERROR;

	if (msg->recordCount > 0)
	{
		net_nfc_util_sign_records(msg, 0, msg->recordCount - 1, cert_file, password);

		result = NET_NFC_OK;
	}

	return result;
}
