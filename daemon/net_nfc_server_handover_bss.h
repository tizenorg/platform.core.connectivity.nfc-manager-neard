/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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
#ifndef __NET_NFC_SERVER_HANDOVER_BSS_H__
#define __NET_NFC_SERVER_HANDOVER_BSS_H__

#include <wifi.h>

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

#endif //__NET_NFC_SERVER_HANDOVER_BSS_H__
