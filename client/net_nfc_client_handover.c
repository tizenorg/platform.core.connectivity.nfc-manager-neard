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

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_data.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_handover.h"
#include "net_nfc_neard.h"


static NetNfcGDbusHandover *handover_proxy = NULL;

static void p2p_connection_handover(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	data_s arg_data;
	GError *error = NULL;
	GVariant *data = NULL;
	net_nfc_error_e result;
	NetNfcCallback *func_data = user_data;
	net_nfc_p2p_connection_handover_completed_cb callback;
	net_nfc_conn_handover_carrier_type_e type = NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_handover_call_request_finish(handover_proxy,
			(gint *)&result, (gint *)&type, &data, res, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Can not finish connection handover: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		callback = (net_nfc_p2p_connection_handover_completed_cb)func_data->callback;

		net_nfc_util_gdbus_variant_to_data_s(data, &arg_data);

		callback(result, type, &arg_data, func_data->user_data);

		net_nfc_util_free_data(&arg_data);
	}

	g_free(func_data);
}


API net_nfc_error_e net_nfc_client_handover_free_alternative_carrier_data(
		net_nfc_connection_handover_info_s *info)
{
	RETV_IF(NULL == info, NET_NFC_NULL_PARAMETER);

	if (info->data.buffer != NULL)
		_net_nfc_util_free_mem(info->data.buffer);

	_net_nfc_util_free_mem(info);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_handover_get_alternative_carrier_type(
		net_nfc_connection_handover_info_s *info,
		net_nfc_conn_handover_carrier_type_e *type)
{
	RETV_IF(NULL == info, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == type, NET_NFC_NULL_PARAMETER);

	*type = info->type;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_handover_get_alternative_carrier_data(
		net_nfc_connection_handover_info_s *info, data_s **data)
{
	RETV_IF(NULL == info, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == data, NET_NFC_NULL_PARAMETER);

	return net_nfc_create_data(data, info->data.buffer, info->data.length);
}


API net_nfc_error_e net_nfc_client_p2p_connection_handover(
		net_nfc_target_handle_s *handle,
		net_nfc_conn_handover_carrier_type_e arg_type,
		net_nfc_p2p_connection_handover_completed_cb callback,
		void *cb_data)
{
	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	return net_nfc_neard_p2p_connection_handover(handle, arg_type, callback, cb_data);
}


API net_nfc_error_e net_nfc_client_p2p_connection_handover_sync(
		net_nfc_target_handle_s *handle,
		net_nfc_conn_handover_carrier_type_e arg_type,
		net_nfc_conn_handover_carrier_type_e *out_carrier,
		data_s **out_ac_data)
{
	gboolean ret;
	GError *error = NULL;
	GVariant *out_data = NULL;
	net_nfc_error_e out_result = NET_NFC_OK;
	net_nfc_conn_handover_carrier_type_e out_type = NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;

	RETV_IF(NULL == handover_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	ret = net_nfc_gdbus_handover_call_request_sync(handover_proxy,
			GPOINTER_TO_UINT(handle),
			arg_type,
			net_nfc_client_gdbus_get_privilege(),
			(gint32 *)&out_result,
			(gint32 *)&out_type,
			&out_data,
			NULL,
			&error);

	if (TRUE == ret)
	{
		if (out_carrier)
			*out_carrier = out_type;

		if (out_ac_data)
			*out_ac_data = net_nfc_util_gdbus_variant_to_data(out_data);
	}
	else
	{
		NFC_ERR("handover (sync call) failed: %s",error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	return out_result;
}


net_nfc_error_e net_nfc_client_handover_init(void)
{
	GError *error = NULL;

	if (handover_proxy)
	{
		NFC_WARN("Already initialized");
		return NET_NFC_OK;
	}

	handover_proxy = net_nfc_gdbus_handover_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Handover",
			NULL,
			&error);

	if (NULL == handover_proxy)
	{
		NFC_ERR("Can not create proxy : %s", error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	return NET_NFC_OK;
}


void net_nfc_client_handover_deinit(void)
{
	if (handover_proxy)
	{
		g_object_unref(handover_proxy);
		handover_proxy = NULL;
	}
}
