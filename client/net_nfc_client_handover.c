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


static NetNfcGDbusHandover *handover_proxy = NULL;

static void p2p_connection_handover(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);

static void p2p_connection_handover(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NetNfcCallback *func_data = (NetNfcCallback *)user_data;
	GVariant *data = NULL;
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_conn_handover_carrier_type_e type =
		NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;
	GError *error = NULL;
	data_s arg_data;

	g_assert(user_data != NULL);

	if (net_nfc_gdbus_handover_call_request_finish(handover_proxy,
				(gint *)&result,
				(gint *)&type,
				&data,
				res,
				&error) == FALSE)
	{
		NFC_ERR("Can not finish"
				" connection handover: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_p2p_connection_handover_completed_cb callback =
			(net_nfc_p2p_connection_handover_completed_cb)func_data->callback;

		net_nfc_util_gdbus_variant_to_data_s(data, &arg_data);

		callback(result, type, &arg_data, func_data->user_data);

		net_nfc_util_free_data(&arg_data);
	}

	g_free(func_data);
}


API net_nfc_error_e net_nfc_client_handover_free_alternative_carrier_data(
		net_nfc_connection_handover_info_h info_handle)
{
	net_nfc_connection_handover_info_s *info =
		(net_nfc_connection_handover_info_s *)info_handle;

	if (info_handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (info->data.buffer != NULL)
	{
		_net_nfc_util_free_mem(info->data.buffer);
	}

	_net_nfc_util_free_mem(info);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_handover_get_alternative_carrier_type(
		net_nfc_connection_handover_info_h info_handle,
		net_nfc_conn_handover_carrier_type_e *type)
{
	net_nfc_connection_handover_info_s *info =
		(net_nfc_connection_handover_info_s *)info_handle;

	if (info_handle == NULL || type == NULL)
		return NET_NFC_NULL_PARAMETER;

	*type = info->type;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_handover_get_alternative_carrier_data(
		net_nfc_connection_handover_info_h info_handle, data_h *data)
{
	net_nfc_connection_handover_info_s *info =
		(net_nfc_connection_handover_info_s *)info_handle;

	if (info_handle == NULL || data == NULL)
		return NET_NFC_NULL_PARAMETER;

	return net_nfc_create_data(data, info->data.buffer, info->data.length);
}


API net_nfc_error_e net_nfc_client_p2p_connection_handover(
		net_nfc_target_handle_h handle,
		net_nfc_conn_handover_carrier_type_e arg_type,
		net_nfc_p2p_connection_handover_completed_cb callback,
		void *cb_data)
{
	NetNfcCallback *funcdata;

	if (handover_proxy == NULL)
	{
		NFC_ERR("Can not get handover Proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	funcdata = g_try_new0(NetNfcCallback, 1);
	if (funcdata == NULL) {
		return NET_NFC_ALLOC_FAIL;
	}

	funcdata->callback = (gpointer)callback;
	funcdata->user_data = cb_data;

	net_nfc_gdbus_handover_call_request(handover_proxy,
			GPOINTER_TO_UINT(handle),
			arg_type,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			p2p_connection_handover,
			funcdata);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_p2p_connection_handover_sync(
		net_nfc_target_handle_h handle,
		net_nfc_conn_handover_carrier_type_e arg_type,
		net_nfc_conn_handover_carrier_type_e *out_carrier,
		data_h *out_ac_data)
{
	GVariant *out_data = NULL;
	net_nfc_error_e out_result = NET_NFC_OK;
	net_nfc_conn_handover_carrier_type_e out_type =
		NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;
	GError *error = NULL;

	if (handover_proxy == NULL)
	{
		NFC_ERR("Can not get handover Proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_handover_call_request_sync(handover_proxy,
				GPOINTER_TO_UINT(handle),
				arg_type,
				net_nfc_client_gdbus_get_privilege(),
				(gint32 *)&out_result,
				(gint32 *)&out_type,
				&out_data,
				NULL,
				&error) == TRUE) {
		if (out_carrier) {
			*out_carrier = out_type;
		}

		if (out_ac_data) {
			*out_ac_data = net_nfc_util_gdbus_variant_to_data(out_data);
		}
	} else {
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

	if (handover_proxy == NULL)
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
