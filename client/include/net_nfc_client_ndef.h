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
#ifndef __NET_NFC_CLIENT_NDEF_H__
#define __NET_NFC_CLIENT_NDEF_H__

#include "net_nfc_typedef.h"

typedef void (*net_nfc_client_ndef_read_completed) (net_nfc_error_e result,
		ndef_message_s *message, void *user_data);

typedef void (*net_nfc_client_ndef_write_completed) (net_nfc_error_e result,
		void *user_data);

typedef void (*net_nfc_client_ndef_make_read_only_completed) (
		net_nfc_error_e result,
		void *user_data);

typedef void (*net_nfc_client_ndef_format_completed) (net_nfc_error_e result,
		void *user_data);

net_nfc_error_e net_nfc_client_ndef_read(net_nfc_target_handle_s *handle,
		net_nfc_client_ndef_read_completed callback,
		void *user_data);

net_nfc_error_e net_nfc_client_ndef_read_sync(net_nfc_target_handle_s *handle,
		ndef_message_s **message);

net_nfc_error_e net_nfc_client_ndef_write(net_nfc_target_handle_s *handle,
		ndef_message_s *message, net_nfc_client_ndef_write_completed callback, void *user_data);

net_nfc_error_e net_nfc_client_ndef_write_sync(net_nfc_target_handle_s *handle,
		ndef_message_s *message);

net_nfc_error_e net_nfc_client_ndef_make_read_only(
		net_nfc_target_handle_s *handle,
		net_nfc_client_ndef_make_read_only_completed callback,
		void *user_data);

net_nfc_error_e net_nfc_client_ndef_make_read_only_sync(
		net_nfc_target_handle_s *handle);

net_nfc_error_e net_nfc_client_ndef_format(net_nfc_target_handle_s *handle,
		data_s *key, net_nfc_client_ndef_format_completed callback, void *user_data);

net_nfc_error_e net_nfc_client_ndef_format_sync(net_nfc_target_handle_s *handle,
		data_s *key);

/* TODO : move to internal header */
net_nfc_error_e net_nfc_client_ndef_init(void);

void net_nfc_client_ndef_deinit(void);

#endif //__NET_NFC_CLIENT_NDEF_H__
