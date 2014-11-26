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
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_phdc.h"

typedef struct _Phdc_SignalHandler PhdcSignalHandler;

static NetNfcGDbusPhdc *phdc_proxy = NULL;
static PhdcSignalHandler phdc_signal_handler;

struct _Phdc_SignalHandler
{
	net_nfc_client_phdc_transport_connect_indication phdc_transport_connect_indication_cb;
	net_nfc_client_phdc_transport_disconnect_indication phdc_transport_disconnect_indication_cb;
	net_nfc_client_phdc_data_received phdc_data_received_cb;

	gpointer phdc_transport_connect_indication_data;
	gpointer phdc_transport_disconnect_indication_data;
	gpointer phdc_data_received_data;
};


static void _phdc_event_cb(NetNfcGDbusPhdc *object, gint arg_result,
		guint arg_event,guint arg_user_data)
{
	GVariant *parameter = (GVariant *)GUINT_TO_POINTER(arg_user_data);

	NFC_DBG(" result [%d], event [%d], user_data [%p]",
			 arg_result, arg_event, parameter);

	if (parameter != NULL)
	{
		void *user_data;
		net_nfc_client_phdc_event_cb callback;

		g_variant_get(parameter, "(uu)",
			(guint *)&callback,
			(guint *)&user_data);

		if (callback != NULL)
		{
			callback( arg_result, arg_event, user_data);
		}
	}

	if (arg_event == NET_NFC_LLCP_UNREGISTERED)
	{
		g_variant_unref(parameter);
	}

}


static void phdc_transport_disconnect_indication(GObject *source_object, gpointer user_data)
{
	NFC_INFO(">>> SIGNAL arrived");

	RET_IF(NULL == phdc_signal_handler.phdc_transport_disconnect_indication_cb);

	phdc_signal_handler.phdc_transport_disconnect_indication_cb(
			phdc_signal_handler.phdc_transport_disconnect_indication_data);
}

static void phdc_transport_connect_indication(GObject *source_object,
		guint arg_handle, gpointer user_data)
{
	net_nfc_phdc_handle_h handle_info = NULL;

	NFC_INFO(">>> SIGNAL arrived");

	handle_info = GUINT_TO_POINTER(arg_handle);

	RET_IF(NULL == phdc_signal_handler.phdc_transport_connect_indication_cb);

	phdc_signal_handler.phdc_transport_connect_indication_cb(handle_info,
			phdc_signal_handler.phdc_transport_connect_indication_data);
}

static void phdc_call_send(GObject *source_object,GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GVariant *parameter = (GVariant *)user_data;
	GError *error = NULL;
	net_nfc_error_e out_result;
	net_nfc_client_phdc_send_completed callback;
	net_nfc_phdc_handle_h handle;
	void *user_param;

	NFC_INFO(">>> phdc_call_send \n");

	ret = net_nfc_gdbus_phdc_call_send_finish(NET_NFC_GDBUS_PHDC(source_object),
			(gint *)&out_result, res, &error);

	if (FALSE == ret)
	{
		out_result = NET_NFC_IPC_FAIL;

		NFC_ERR("Can not finish phdc send: %s", error->message);
		g_error_free(error);
	}

	g_variant_get(parameter, "(uuu)",
	(guint *)&callback,
	(guint *)&user_param,
	(guint *)&handle);

	NFC_INFO(">>> phdc_call_send  %p\n", handle);

	if (callback != NULL)
	{
		callback( out_result, user_param);
	}

	g_variant_unref(parameter);
}

static void phdc_device_data_received(GObject *source_object,GVariant *arg_data,
		gpointer user_data)
{
	data_s phdc_data = { NULL, };

	NFC_INFO(">>> SIGNAL arrived");

	RET_IF(NULL == phdc_signal_handler.phdc_data_received_cb);

	net_nfc_util_gdbus_variant_to_data_s(arg_data, &phdc_data);
	phdc_signal_handler.phdc_data_received_cb(&phdc_data,
			phdc_signal_handler.phdc_data_received_data);

	net_nfc_util_free_data(&phdc_data);
}

API net_nfc_error_e net_nfc_client_phdc_send(net_nfc_phdc_handle_h handle,
		data_s *data, net_nfc_client_phdc_send_completed callback, void *user_data)
{
	GVariant *arg_data;
	GVariant *parameter;

	NFC_INFO(">>> net_nfc_client_phdc_send \n");

	RETV_IF(NULL == phdc_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	parameter = g_variant_new("(uuu)",
		GPOINTER_TO_UINT(callback),
		GPOINTER_TO_UINT(user_data),
		GPOINTER_TO_UINT(handle));


	arg_data = net_nfc_util_gdbus_data_to_variant(data);

	net_nfc_gdbus_phdc_call_send(phdc_proxy,
		GPOINTER_TO_UINT(handle),
		arg_data,
		net_nfc_client_gdbus_get_privilege(),
		NULL,
		phdc_call_send,
		parameter);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_phdc_send_sync(net_nfc_phdc_handle_h handle,
		data_s *data)
{
	gboolean ret;
	GVariant *arg_data;
	GError *error = NULL;

	net_nfc_error_e out_result;

	RETV_IF(NULL == phdc_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	arg_data = net_nfc_util_gdbus_data_to_variant(data);

	ret = net_nfc_gdbus_phdc_call_send_sync(phdc_proxy,
			GPOINTER_TO_UINT(handle),
			arg_data,
			net_nfc_client_gdbus_get_privilege(),
			(gint *)&out_result,
			NULL,
			&error);

	if(FALSE == ret)
	{
		NFC_ERR("phdc send (sync call) failed: %s", error->message);

		g_error_free(error);
		out_result = NET_NFC_IPC_FAIL;
	}

	return out_result;
}

API net_nfc_error_e net_nfc_client_phdc_register(net_nfc_phdc_role_e role,
		const char *san,net_nfc_client_phdc_event_cb callback, void *user_data)
{
	GError *error = NULL;
	GVariant *parameter;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == phdc_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	parameter = g_variant_new("(uu)",
			GPOINTER_TO_UINT(callback),
			GPOINTER_TO_UINT(user_data));

	if (net_nfc_gdbus_phdc_call_register_role_sync(phdc_proxy,
				role,
				san,
				GPOINTER_TO_UINT(parameter),
				net_nfc_client_gdbus_get_privilege(),
				(gint *)&result,
				NULL,
				&error) == FALSE)
	{
		NFC_ERR("phdc register role(sync call) failed: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
		g_variant_unref(parameter);
	}

	return result;
}

API net_nfc_error_e net_nfc_client_phdc_unregister( net_nfc_phdc_role_e role,
		const char *san)
{
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == phdc_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	if (net_nfc_gdbus_phdc_call_unregister_role_sync(phdc_proxy,
				role,
				san,
				net_nfc_client_gdbus_get_privilege(),
				(gint *)&result,
				NULL,
				&error) == FALSE)
	{
		NFC_ERR("phdc unregister role(sync call) failed: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}


API void net_nfc_client_phdc_set_transport_connect_indication(
		net_nfc_client_phdc_transport_connect_indication callback, void *user_data)
{
	RET_IF(NULL == callback);

	phdc_signal_handler.phdc_transport_connect_indication_cb = callback;
	phdc_signal_handler.phdc_transport_connect_indication_data = user_data;
}

API void net_nfc_client_phdc_set_transport_disconnect_indication(
		net_nfc_client_phdc_transport_disconnect_indication callback, void *user_data)
{
	RET_IF(NULL == callback);

	phdc_signal_handler.phdc_transport_disconnect_indication_cb = callback;
	phdc_signal_handler.phdc_transport_disconnect_indication_data = user_data;
}

API void net_nfc_client_phdc_set_data_received(
		net_nfc_client_phdc_data_received callback, void *user_data)
{
	RET_IF(NULL == callback);

	phdc_signal_handler.phdc_data_received_cb = callback;
	phdc_signal_handler.phdc_data_received_data = user_data;
}

API void net_nfc_client_phdc_unset_transport_connect_indication(void)
{
	phdc_signal_handler.phdc_transport_connect_indication_cb = NULL;
	phdc_signal_handler.phdc_transport_connect_indication_data= NULL;
}


API void net_nfc_client_phdc_unset_transport_disconnect_indication(void)
{
	phdc_signal_handler.phdc_transport_disconnect_indication_cb = NULL;
	phdc_signal_handler.phdc_transport_disconnect_indication_data = NULL;
}

API void net_nfc_client_phdc_unset_data_received(void)
{
	phdc_signal_handler.phdc_data_received_cb = NULL;
	phdc_signal_handler.phdc_data_received_data = NULL;
}

net_nfc_error_e net_nfc_client_phdc_init(void)
{
	GError *error = NULL;

	if (phdc_proxy)
	{
		NFC_WARN("Already initialized");
		return NET_NFC_OK;
	}

	phdc_proxy = net_nfc_gdbus_phdc_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Phdc",
			NULL,
			&error);
	if (NULL == phdc_proxy)
	{
		NFC_ERR("Can not create proxy : %s", error->message);
		g_error_free(error);

		return NET_NFC_UNKNOWN_ERROR;
	}

	net_nfc_client_phdc_unset_transport_connect_indication();
	net_nfc_client_phdc_unset_transport_disconnect_indication();
	net_nfc_client_phdc_unset_data_received();

	g_signal_connect(phdc_proxy, "phdc_connect",
			G_CALLBACK(phdc_transport_connect_indication), NULL);

	g_signal_connect(phdc_proxy, "phdc_disconnect",
			G_CALLBACK(phdc_transport_disconnect_indication), NULL);

	g_signal_connect(phdc_proxy, "phdc_received",
			G_CALLBACK(phdc_device_data_received), NULL);

	g_signal_connect(phdc_proxy, "phdc-event", G_CALLBACK(_phdc_event_cb), NULL);


	return NET_NFC_OK;
}

void net_nfc_client_phdc_deinit(void)
{
	if (phdc_proxy)
	{
		g_object_unref(phdc_proxy);
		phdc_proxy = NULL;
	}

	net_nfc_client_phdc_unset_transport_connect_indication();
	net_nfc_client_phdc_unset_transport_disconnect_indication();
	net_nfc_client_phdc_unset_data_received();

}

