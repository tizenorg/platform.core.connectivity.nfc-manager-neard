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
#ifndef __NET_NFC_CLIENT_PHDC_H__
#define __NET_NFC_CLIENT_PHDC_H__

#include "net_nfc_typedef.h"

/* PHDC callbacks */

typedef void (*net_nfc_client_phdc_send_completed)(net_nfc_error_e result,
		void *user_data);

typedef void (*net_nfc_client_phdc_transport_connect_indication)(
		net_nfc_phdc_handle_h handle_info,void *user_data);

typedef void (*net_nfc_client_phdc_transport_disconnect_indication)(void *user_data);

typedef void (*net_nfc_client_phdc_data_received)(data_s *phdc_data,
		void *user_data);

typedef void (*net_nfc_client_phdc_event_cb)( net_nfc_error_e result,
		net_nfc_llcp_state_t event, void *user_data);

/* PHDC client API's*/
net_nfc_error_e net_nfc_client_phdc_send(net_nfc_phdc_handle_h handle,
		data_s *data, net_nfc_client_phdc_send_completed callback, void *user_data);

net_nfc_error_e net_nfc_client_phdc_send_sync(net_nfc_phdc_handle_h handle,
		data_s *data);

/* PHDC client API's - used for registering callbacks*/
void net_nfc_client_phdc_set_transport_connect_indication(
		net_nfc_client_phdc_transport_connect_indication callback, void *user_data);

void net_nfc_client_phdc_set_transport_disconnect_indication(
		net_nfc_client_phdc_transport_disconnect_indication callback, void *user_data);

void net_nfc_client_phdc_set_data_received(
		net_nfc_client_phdc_data_received callback, void *user_data);

net_nfc_error_e net_nfc_client_phdc_register(net_nfc_phdc_role_e role,
		const char *san,net_nfc_client_phdc_event_cb callback, void *user_data);

net_nfc_error_e net_nfc_client_phdc_unregister( net_nfc_phdc_role_e role,
		const char *san);


/* PHDC client API's - used for unregistering callbacks*/
void net_nfc_client_phdc_unset_transport_connect_indication(void);

void net_nfc_client_phdc_unset_transport_disconnect_indication(void);

void net_nfc_client_phdc_unset_data_received(void);

/* Init/Deint function calls*/
net_nfc_error_e net_nfc_client_phdc_init(void);

void net_nfc_client_phdc_deinit(void);

#endif //__NET_NFC_CLIENT_PHDC_H__


