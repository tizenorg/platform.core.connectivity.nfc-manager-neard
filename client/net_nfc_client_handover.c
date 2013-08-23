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

typedef struct _HandoverFuncData HandoverFuncData;

struct _HandoverFuncData
{
	gpointer handover_callback;
	gpointer handover_data;
};

static NetNfcGDbusHandover *handover_proxy = NULL;

static void p2p_connection_handover(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);

static void p2p_connection_handover(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	HandoverFuncData *func_data;
	GVariant *data;
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_exchanger_event_e event;
	net_nfc_conn_handover_carrier_type_e type;
	data_s arg_data;

	net_nfc_p2p_connection_handover_completed_cb callback;

	if(net_nfc_gdbus_handover_call_request_finish (handover_proxy,
				(gint32 *)&event,
				(gint32 *)&type,
				&data,
				res,
				&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish"
				" connection handover: %s", error->message);
		g_error_free(error);
		return;
	}

	func_data = user_data;
	if(func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get HandoverFuncData");
		return;
	}

	if(func_data->handover_callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not avaiilable");
		g_free(func_data);
		return;
	}

	net_nfc_util_gdbus_variant_to_data_s(data, &arg_data);

	callback = (net_nfc_p2p_connection_handover_completed_cb)
		func_data->handover_callback;

	callback(result,
			type,
			&arg_data,
			func_data->handover_data);

	net_nfc_util_free_data(&arg_data);

	g_free(func_data);
}


API net_nfc_error_e net_nfc_client_handover_free_alternative_carrier_data(
		net_nfc_connection_handover_info_h info_handle)
{
	net_nfc_error_e result = NET_NFC_UNKNOWN_ERROR;
	net_nfc_connection_handover_info_s *info = NULL;

	if (info_handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	info = (net_nfc_connection_handover_info_s *)info_handle;

	if (info->data.buffer != NULL)
	{
		_net_nfc_util_free_mem(info->data.buffer);
	}

	_net_nfc_util_free_mem(info);

	return result;
}


API net_nfc_error_e net_nfc_client_handover_get_alternative_carrier_type(
		net_nfc_connection_handover_info_h info_handle,
		net_nfc_conn_handover_carrier_type_e *type)
{
	net_nfc_connection_handover_info_s *info = NULL;

	if (info_handle == NULL || type == NULL)
		return NET_NFC_NULL_PARAMETER;

	info = (net_nfc_connection_handover_info_s *)info_handle;

	*type = info->type;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_handover_get_alternative_carrier_data(
		net_nfc_connection_handover_info_h info_handle,
		data_h *data)
{
	net_nfc_error_e result = NET_NFC_UNKNOWN_ERROR;
	net_nfc_connection_handover_info_s *info = NULL;

	if (info_handle == NULL || data == NULL)
		return NET_NFC_NULL_PARAMETER;

	info = (net_nfc_connection_handover_info_s *)info_handle;

	result = net_nfc_create_data(data, info->data.buffer, info->data.length);

	return result;
}


API net_nfc_error_e net_nfc_client_p2p_connection_handover(
		net_nfc_target_handle_h handle,
		net_nfc_conn_handover_carrier_type_e arg_type,
		net_nfc_p2p_connection_handover_completed_cb callback,
		void *cb_data)
{

	HandoverFuncData *funcdata = NULL;
	net_nfc_target_handle_s *tag_handle = (net_nfc_target_handle_s *)handle;

	if(handover_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get handover Proxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	funcdata = g_new0(HandoverFuncData, 1);
	if (funcdata == NULL)
		return NET_NFC_UNKNOWN_ERROR;

	funcdata->handover_callback = (gpointer)callback;
	funcdata->handover_data = cb_data;

	net_nfc_gdbus_handover_call_request(handover_proxy,
			GPOINTER_TO_UINT(tag_handle),
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

	net_nfc_target_handle_s *tag_handle = (net_nfc_target_handle_s *)handle;
	GError *error = NULL;
	GVariant *out_data;
	net_nfc_exchanger_event_e out_event;
	net_nfc_conn_handover_carrier_type_e out_type;

	if(handover_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get handover Proxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if(net_nfc_gdbus_handover_call_request_sync(handover_proxy,
				GPOINTER_TO_UINT(tag_handle),
				arg_type,
				net_nfc_client_gdbus_get_privilege(),
				(gint32 *)&out_event,
				(gint32 *)&out_type,
				&out_data,
				NULL,
				&error) == FALSE)
	{
		DEBUG_ERR_MSG("handover (sync call) failed: %s",error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (out_carrier)
	{
		*out_carrier = out_type;
	}

	if (out_ac_data)
	{
		*out_ac_data = net_nfc_util_gdbus_variant_to_data(out_data);
	}

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_handover_init(void)
{
	GError *error = NULL;

	if (handover_proxy)
	{
		DEBUG_CLIENT_MSG("Already initialized");
		return NET_NFC_OK;
	}

	handover_proxy = net_nfc_gdbus_handover_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Handover",
			NULL,
			&error);

	if(handover_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not create proxy : %s", error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	return NET_NFC_OK;
}

API void net_nfc_client_handover_deinit(void)
{
	if(handover_proxy)
	{
		g_object_unref(handover_proxy);
		handover_proxy = NULL;
	}
}
