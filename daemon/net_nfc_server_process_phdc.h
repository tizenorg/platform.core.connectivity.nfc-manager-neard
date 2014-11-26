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

#ifndef __NET_NFC_SERVER_PROCESS_PHDC_H__
#define __NET_NFC_SERVER_PROCESS_PHDC_H__

#include "net_nfc_gdbus.h"
#include "net_nfc_typedef_internal.h"

#define PHDC_SAP 0x16
#define PHDS_SAP 0x17
#define PHDC_SAN "urn:nfc:sn:phdc"
#define PHDS_SAN "urn:nfc:sn:phds"
#define PHDC_HEADER_LEN 2

typedef enum
{
	NET_NFC_PHDC_OPERATION_FAILED = 0,
	NET_NFC_PHDC_TARGET_CONNECTED ,
	NET_NFC_PHDC_DATA_RECEIVED
}net_nfc_server_phdc_indication_type;


//callback for manager connection accept / agent connect
typedef void (*net_nfc_server_phdc_cb)(net_nfc_phdc_handle_h handle,
		net_nfc_error_e result, net_nfc_server_phdc_indication_type indication, data_s *data);

//callback for data send
typedef gboolean (*net_nfc_server_phdc_send_cb)(net_nfc_phdc_handle_h handle,
		net_nfc_error_e result, net_nfc_server_phdc_indication_type indication, gpointer *user_data);

typedef struct _net_nfc_server_phdc_context_t net_nfc_server_phdc_context_t;

struct _net_nfc_server_phdc_context_t
{
	net_nfc_target_handle_s *handle;
	net_nfc_error_e result;
	net_nfc_llcp_socket_t socket;
	uint32_t state;
	data_s data;
	net_nfc_server_phdc_cb cb;
	void *user_param;
	GQueue queue;
};

net_nfc_error_e net_nfc_server_phdc_manager_start(net_nfc_target_handle_s *handle,
		const char *san, sap_t sap, net_nfc_server_phdc_cb cb, 	void *user_param);

net_nfc_error_e net_nfc_server_phdc_agent_start(net_nfc_target_handle_s *handle,
		const char *san, sap_t sap, net_nfc_server_phdc_cb cb, void *user_param);

net_nfc_error_e net_nfc_server_phdc_agent_request(net_nfc_phdc_handle_h handle,
		data_s *data, net_nfc_server_phdc_send_cb cb, void *user_param);


#endif//__NET_NFC_SERVER_PROCESS_PHDC_H__

