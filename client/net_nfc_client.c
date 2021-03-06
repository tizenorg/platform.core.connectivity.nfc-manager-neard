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

#include <glib.h>

#include "net_nfc_typedef.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_client_se.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_tag.h"
#include "net_nfc_client_ndef.h"
#include "net_nfc_client_transceive.h"
#include "net_nfc_client_llcp.h"
#include "net_nfc_client_snep.h"
#include "net_nfc_client_p2p.h"
#include "net_nfc_client_phdc.h"
#include "net_nfc_client_system_handler.h"
#include "net_nfc_client_handover.h"

GVariant *net_nfc_client_gdbus_get_privilege()
{
	return net_nfc_util_gdbus_buffer_to_variant(NULL, 0);
}

void net_nfc_client_gdbus_init(void)
{
	if (net_nfc_client_manager_init() != NET_NFC_OK)
		return;
	if (net_nfc_client_tag_init() != NET_NFC_OK)
		return;
	if (net_nfc_client_ndef_init() != NET_NFC_OK)
		return;
	if (net_nfc_client_transceive_init() != NET_NFC_OK)
		return;
	if (net_nfc_client_llcp_init() != NET_NFC_OK)
		return;
	if (net_nfc_client_snep_init() != NET_NFC_OK)
		return;
	if (net_nfc_client_p2p_init() != NET_NFC_OK)
		return;
	if (net_nfc_client_sys_handler_init() != NET_NFC_OK)
		return;
	if (net_nfc_client_se_init() != NET_NFC_OK)
		return;
	if(net_nfc_client_handover_init() != NET_NFC_OK)
		return;
	if(net_nfc_client_phdc_init() != NET_NFC_OK)
		return;
}

void net_nfc_client_gdbus_deinit(void)
{
	net_nfc_client_handover_deinit();
	net_nfc_client_se_deinit();
	net_nfc_client_sys_handler_deinit();
	net_nfc_client_p2p_deinit();
	net_nfc_client_snep_deinit();
	net_nfc_client_llcp_deinit();
	net_nfc_client_transceive_deinit();
	net_nfc_client_ndef_deinit();
	net_nfc_client_tag_deinit();
	net_nfc_client_phdc_deinit();
	net_nfc_client_manager_deinit();
}
