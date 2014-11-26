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
#ifndef __NET_NFC_CLIENT_P2P_H__
#define __NET_NFC_CLIENT_P2P_H__

#include "net_nfc_typedef.h"

/* p2p callbacks */

typedef void (*net_nfc_client_p2p_send_completed)(net_nfc_error_e result,
		void *user_data);

typedef void (*net_nfc_client_p2p_device_discovered)(
		net_nfc_target_handle_s *handle_info,void *user_data);

typedef void (*net_nfc_client_p2p_device_detached)(void *user_data);

typedef void (*net_nfc_client_p2p_data_received)(data_s *p2p_data, void *user_data);

/* P2P client API's*/
net_nfc_error_e net_nfc_client_p2p_send(net_nfc_target_handle_s *handle,
		data_s *data, net_nfc_client_p2p_send_completed callback, void *user_data);

net_nfc_error_e net_nfc_client_p2p_send_sync(net_nfc_target_handle_s *handle,
		data_s *data);


/* P2P client API's - used for registering callbacks*/
void net_nfc_client_p2p_set_data_received(
		net_nfc_client_p2p_data_received callback, void *user_data);

void net_nfc_client_p2p_set_device_detached(
		net_nfc_client_p2p_device_detached callback, void *user_data);

void net_nfc_client_p2p_set_device_discovered(
		net_nfc_client_p2p_device_discovered callback, void *user_data);

/* P2P client API's - used for unregistering callbacks*/
void net_nfc_client_p2p_unset_data_received(void);

void net_nfc_client_p2p_unset_device_detached(void);

void net_nfc_client_p2p_unset_device_discovered(void);

/* TODO : move to internal header */
/* Init/Deint function calls*/
net_nfc_error_e net_nfc_client_p2p_init(void);

void net_nfc_client_p2p_deinit(void);

#endif //__NET_NFC_CLIENT_P2P_H__
