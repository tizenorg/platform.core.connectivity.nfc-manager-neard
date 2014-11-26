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
#ifndef __NET_NFC_SERVER_HANDOVER_BSS_H__
#define __NET_NFC_SERVER_HANDOVER_BSS_H__

#include <wifi.h>
#ifdef TARGET
#include <wifi-direct.h>
#endif

#include "net_nfc_typedef_internal.h"
#include "net_nfc_server_handover_bt.h"

typedef struct _net_nfc_handover_bss_process_context_t
{
	int step;
	net_nfc_error_e result;
	net_nfc_server_handover_process_carrier_record_cb cb;
	ndef_record_s *carrier;
	data_s data;
	void *user_param;
	wifi_ap_h ap_handle;
	net_nfc_carrier_config_s *config;
}
net_nfc_handover_bss_process_context_t;

net_nfc_error_e net_nfc_server_handover_bss_process_carrier_record(
		ndef_record_s *record,
		net_nfc_server_handover_process_carrier_record_cb cb,
		void *user_param);
net_nfc_error_e net_nfc_server_handover_bss_get_carrier_record(
		net_nfc_server_handover_get_carrier_record_cb cb,
		void *user_param);
typedef struct _net_nfc_handover_bss_create_context_t
{
	int step;
	net_nfc_error_e result;
	net_nfc_server_handover_get_carrier_record_cb cb;
	net_nfc_conn_handover_carrier_state_e cps;
	ndef_record_s *carrier;
	data_s data;
	void *user_param;
	wifi_ap_h ap_handle;
	net_nfc_carrier_config_s *config;
}
net_nfc_handover_bss_create_context_t;

typedef struct _net_nfc_handover_bss_get_context_t
{
	int step;
	net_nfc_error_e result;
	net_nfc_conn_handover_carrier_state_e cps;
	net_nfc_server_handover_get_carrier_record_cb cb;
	ndef_record_s *carrier;
	uint32_t aux_data_count;
	ndef_record_s *aux_data;
	void *user_param;
}
net_nfc_handover_bss_get_context_t;

net_nfc_error_e net_nfc_server_handover_bss_process_carrier_record(
		ndef_record_s *record,
		net_nfc_server_handover_process_carrier_record_cb cb,
		void *user_param);

#ifdef TARGET
net_nfc_error_e net_nfc_server_handover_bss_wfd_get_carrier_record(
		net_nfc_server_handover_get_carrier_record_cb cb,
		void *user_param);
#endif
#endif //__NET_NFC_SERVER_HANDOVER_BSS_H__
