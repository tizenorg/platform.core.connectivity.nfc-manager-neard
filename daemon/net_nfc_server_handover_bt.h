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
#ifndef __NET_NFC_SERVER_HANDOVER_BT_H__
#define __NET_NFC_SERVER_HANDOVER_BT_H__

#include "net_nfc_typedef_internal.h"

typedef void (*net_nfc_server_handover_get_carrier_record_cb)(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_state_e cps,
		ndef_record_s *carrier,
		uint32_t aux_data_count,
		ndef_record_s *aux_data,
		void *user_param);

typedef void (*net_nfc_server_handover_process_carrier_record_cb)(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_s *data,
		void *user_param);

/* alternative carrier functions */
net_nfc_error_e net_nfc_server_handover_bt_get_carrier_record(
		net_nfc_server_handover_get_carrier_record_cb cb, void *user_param);

net_nfc_error_e net_nfc_server_handover_bt_process_carrier_record(
		ndef_record_s *record, net_nfc_server_handover_process_carrier_record_cb cb,
		void *user_param);

net_nfc_error_e net_nfc_server_handover_bt_post_process(data_s *data,
		net_nfc_server_handover_process_carrier_record_cb cb,
		void *user_param);

#endif //__NET_NFC_SERVER_HANDOVER_BT_H__
