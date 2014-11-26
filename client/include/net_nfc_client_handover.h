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
#ifndef __NET_NFC_CLIENT_HANDOVER_H__
#define __NET_NFC_CLIENT_HANDOVER_H__

#include "net_nfc_typedef.h"

typedef void (*net_nfc_p2p_connection_handover_completed_cb)(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e carrier,
		data_s *ac_data,
		void *user_data);


net_nfc_error_e net_nfc_client_handover_free_alternative_carrier_data(
		net_nfc_connection_handover_info_s *info_handle);


net_nfc_error_e net_nfc_client_handover_get_alternative_carrier_type(
		net_nfc_connection_handover_info_s *info_handle,
		net_nfc_conn_handover_carrier_type_e *type);


net_nfc_error_e net_nfc_client_handover_get_alternative_carrier_data(
		net_nfc_connection_handover_info_s *info_handle, data_s **data);


net_nfc_error_e net_nfc_client_p2p_connection_handover(
		net_nfc_target_handle_s *handle,
		net_nfc_conn_handover_carrier_type_e arg_type,
		net_nfc_p2p_connection_handover_completed_cb callback,
		void *cb_data);


net_nfc_error_e net_nfc_client_p2p_connection_handover_sync(
		net_nfc_target_handle_s *handle,
		net_nfc_conn_handover_carrier_type_e arg_type,
		net_nfc_conn_handover_carrier_type_e *out_carrier,
		data_s **out_ac_data);

/* TODO : move to internal header */
net_nfc_error_e net_nfc_client_handover_init(void);

void net_nfc_client_handover_deinit(void);

#endif //__NET_NFC_CLIENT_HANDOVER_H__
