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
#include "net_nfc_ndef_message.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_snep.h"

static NetNfcGDbusSnep *snep_proxy = NULL;

static void snep_send_client_request(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	void *user_param;
	GError *error = NULL;
	GVariant *out_data = NULL;
	net_nfc_error_e out_result;
	net_nfc_snep_handle_h handle;
	net_nfc_client_snep_event_cb callback;
	GVariant *parameter = (GVariant *)user_data;
	net_nfc_snep_type_t out_type = NET_NFC_SNEP_GET;

	g_assert(parameter != NULL);

	if (net_nfc_gdbus_snep_call_client_request_finish(NET_NFC_GDBUS_SNEP(source_object),
				(gint *)&out_result, (guint *)&out_type, &out_data, res, &error) == FALSE)
	{
		NFC_ERR("Can not finish send client request %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	g_variant_get(parameter, "(uuu)", (guint *)&callback, (guint *)&user_param,
			(guint *)&handle);

	if (callback != NULL) {
		ndef_message_s *message = NULL;

		message = net_nfc_util_gdbus_variant_to_ndef_message(out_data);

		callback(handle, out_type, out_result,message, user_param);
		net_nfc_free_ndef_message(message);
	}

	g_variant_unref(parameter);
}

API net_nfc_error_e net_nfc_client_snep_start_server(
		net_nfc_target_handle_s *target,
		const char *san,
		sap_t sap,
		net_nfc_client_snep_event_cb callback,
		void *user_data)
{
	GVariant *parameter;
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == snep_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	parameter = g_variant_new("(uu)",
			GPOINTER_TO_UINT(callback),
			GPOINTER_TO_UINT(user_data));

	if (net_nfc_gdbus_snep_call_server_start_sync(snep_proxy,
				GPOINTER_TO_UINT(target),
				sap,
				san,
				GPOINTER_TO_UINT(parameter),
				net_nfc_client_gdbus_get_privilege(),
				(gint *)&result,
				NULL,
				&error) == FALSE)
	{
		NFC_ERR("snep server(sync call) failed: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;

		g_variant_unref(parameter);
	}

	return result;
}

API net_nfc_error_e net_nfc_client_snep_start_client(
		net_nfc_target_handle_s *target,
		const char *san,
		sap_t sap,
		net_nfc_client_snep_event_cb callback,
		void *user_data)
{
	GVariant *parameter;
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == snep_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	parameter = g_variant_new("(uu)",
			GPOINTER_TO_UINT(callback),
			GPOINTER_TO_UINT(user_data));

	if (net_nfc_gdbus_snep_call_client_start_sync(snep_proxy,
				GPOINTER_TO_UINT(target),
				sap,
				san,
				GPOINTER_TO_UINT(parameter),
				net_nfc_client_gdbus_get_privilege(),
				(gint *)&result,
				NULL,
				&error) == FALSE)
	{
		NFC_ERR("snep client(sync call) failed: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;

		g_variant_unref(parameter);
	}

	return result;
}

API net_nfc_error_e net_nfc_client_snep_send_client_request(
		net_nfc_snep_handle_h target,
		net_nfc_snep_type_t snep_type,
		ndef_message_s *msg,
		net_nfc_client_snep_event_cb callback,
		void *user_data)
{
	GVariant *parameter;
	GVariant *ndef_msg = NULL;

	RETV_IF(NULL == target, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == msg, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == snep_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	parameter = g_variant_new("(uuu)",
			GPOINTER_TO_UINT(callback),
			GPOINTER_TO_UINT(user_data),
			GPOINTER_TO_UINT(target));

	ndef_msg = net_nfc_util_gdbus_ndef_message_to_variant(msg);

	net_nfc_gdbus_snep_call_client_request(snep_proxy,
			GPOINTER_TO_UINT(target),
			snep_type,
			ndef_msg,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			snep_send_client_request,
			parameter);

	return NET_NFC_OK;
}
#if 0
API net_nfc_error_e net_nfc_client_snep_send_client_request_sync(
		net_nfc_target_handle_s *target,
		net_nfc_snep_type_t snep_type,
		ndef_message_s *msg,
		net_nfc_snep_type_t *resp_type,
		ndef_message_s **response)
{
	GVariant *resp_msg = NULL;
	GVariant *arg_msg = NULL;
	GError *error = NULL;
	net_nfc_error_e result;
	guint type;

	if (target == NULL || msg == NULL
			|| resp_type == NULL || response == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	*response = NULL;

	if (snep_proxy == NULL)
	{
		NFC_ERR("Can not get Snep Proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	arg_msg = net_nfc_util_gdbus_ndef_message_to_variant(msg);

	if (net_nfc_gdbus_snep_call_client_request_sync(snep_proxy,
				GPOINTER_TO_UINT(target),
				snep_type,
				arg_msg,
				net_nfc_client_gdbus_get_privilege(),
				&result,
				resp_type,
				&resp_msg,
				NULL,
				&error) == TRUE)
	{
		data_s ndef_data = { NULL, };

		net_nfc_util_gdbus_variant_to_data_s(resp_msg, &ndef_data);

		if (ndef_data.buffer != NULL && ndef_data.length > 0)
		{
			result = net_nfc_create_ndef_message_from_rawdata(
					response,
					&ndef_data);

			net_nfc_util_free_data(&ndef_data);
		}
	}
	else
	{
		NFC_ERR(" send client request (sync call) failed: %s",
				error->message);
		g_error_free(error);

		return NET_NFC_IPC_FAIL;
	}

	return result;
}
#endif
API net_nfc_error_e net_nfc_client_snep_stop_service_sync(
		net_nfc_target_handle_s *target, net_nfc_snep_handle_h service)
{
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == target, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == service, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == snep_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	if (net_nfc_gdbus_snep_call_stop_snep_sync(
				snep_proxy,
				GPOINTER_TO_UINT(target),
				GPOINTER_TO_UINT(service),
				net_nfc_client_gdbus_get_privilege(),
				(gint *)&result,
				NULL,
				&error) == FALSE)
	{
		NFC_ERR("snep stop service(sync call) failed: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

static void _snep_event_cb(NetNfcGDbusSnep *object, guint arg_handle,
		guint arg_event, gint arg_result, GVariant *arg_ndef_msg, guint arg_user_data)
{
	GVariant *parameter = (GVariant *)GUINT_TO_POINTER(arg_user_data);

	NFC_DBG("handle [%p], event [%d], result [%d], user_data [%p]",
			GUINT_TO_POINTER(arg_handle), arg_event, arg_result, parameter);

	if (parameter != NULL)
	{
		void *user_data;
		net_nfc_client_snep_event_cb callback;

		g_variant_get(parameter, "(uu)", &callback, &user_data);

		if (callback != NULL)
		{
			ndef_message_s *message;
			message = net_nfc_util_gdbus_variant_to_ndef_message(arg_ndef_msg);

			callback(GUINT_TO_POINTER(arg_handle), arg_event, arg_result,
					message, user_data);

			net_nfc_free_ndef_message(message);
		}

		if (arg_event == NET_NFC_LLCP_UNREGISTERED)
			g_variant_unref(parameter);
	}
}

API net_nfc_error_e net_nfc_client_snep_register_server(const char *san,
		sap_t sap, net_nfc_client_snep_event_cb callback, void *user_data)
{
	GVariant *parameter;
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == snep_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	parameter = g_variant_new("(uu)",
			GPOINTER_TO_UINT(callback),
			GPOINTER_TO_UINT(user_data));

	if (net_nfc_gdbus_snep_call_server_register_sync(snep_proxy,
				sap,
				san,
				GPOINTER_TO_UINT(parameter),
				net_nfc_client_gdbus_get_privilege(),
				(gint *)&result,
				NULL,
				&error) == FALSE)
	{
		NFC_ERR("snep register server(sync call) failed: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;

		g_variant_unref(parameter);
	}

	return result;
}

API net_nfc_error_e net_nfc_client_snep_unregister_server(const char *san,
		sap_t sap)
{
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == snep_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	if (net_nfc_gdbus_snep_call_server_unregister_sync(snep_proxy,
				sap,
				san,
				net_nfc_client_gdbus_get_privilege(),
				(gint *)&result,
				NULL,
				&error) == FALSE)
	{
		NFC_ERR("snep unregister server(sync call) failed: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

net_nfc_error_e net_nfc_client_snep_init(void)
{
	GError *error = NULL;

	if (snep_proxy)
	{
		NFC_WARN("Already initialized");
		return NET_NFC_OK;
	}

	snep_proxy = net_nfc_gdbus_snep_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Snep",
			NULL,
			&error);
	if (snep_proxy == NULL)
	{
		NFC_ERR("Can not create proxy : %s", error->message);
		g_error_free(error);

		return NET_NFC_UNKNOWN_ERROR;
	}

	g_signal_connect(snep_proxy, "snep-event", G_CALLBACK(_snep_event_cb), NULL);

	return NET_NFC_OK;
}

void net_nfc_client_snep_deinit(void)
{
	if (snep_proxy)
	{
		g_object_unref(snep_proxy);
		snep_proxy = NULL;
	}
}
