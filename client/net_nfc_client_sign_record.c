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
#include "net_nfc_util_sign_record.h"

API net_nfc_error_e net_nfc_sign_records(ndef_message_s *msg, int begin_index,
		int end_index, char *cert_file, char *password)
{
	return net_nfc_util_sign_records((ndef_message_s *)msg,
			begin_index, end_index, cert_file, password);
}

API net_nfc_error_e net_nfc_sign_ndef_message(ndef_message_s *msg,
		char *cert_file, char *password)
{
	return net_nfc_util_sign_ndef_message((ndef_message_s *)msg, cert_file, password);
}

API net_nfc_error_e net_nfc_verify_signature_ndef_message(ndef_message_s *msg)
{
	return net_nfc_util_verify_signature_ndef_message((ndef_message_s *)msg);
}

API net_nfc_error_e net_nfc_verify_signature_records(ndef_record_s *begin_record,
		ndef_record_s *sign_record)
{
	return net_nfc_util_verify_signature_records(begin_record, sign_record);
}
