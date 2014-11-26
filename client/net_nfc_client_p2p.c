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
#include "net_nfc_client_p2p.h"
#include "net_nfc_neard.h"


typedef struct _P2p_SignalHandler P2pSignalHandler;

struct _P2p_SignalHandler
{
	net_nfc_client_p2p_device_discovered p2p_device_discovered_cb;
	net_nfc_client_p2p_device_detached p2p_device_detached_cb;
	net_nfc_client_p2p_data_received p2p_data_received_cb;

	gpointer p2p_device_discovered_data;
	gpointer p2p_device_detached_data;
	gpointer p2p_data_received_data;
};

static NetNfcGDbusP2p *p2p_proxy = NULL;
static P2pSignalHandler p2p_signal_handler;

static void p2p_device_detached(GObject *source_object,
		gpointer user_data)
{
	NFC_INFO(">>> SIGNAL arrived");

	RET_IF(NULL == p2p_signal_handler.p2p_device_detached_cb);

	/*llcp client function to set/unset the current target id needs to be implemented*/
	/*net_nfc_client_llcp_current_target_id(NULL);*/

	p2p_signal_handler.p2p_device_detached_cb(p2p_signal_handler.p2p_device_detached_data);

	/*llcp client function to close all socket needs to be implemented*/
	/*net_nfc_client_llcp_close_all_socket();*/
}

static void p2p_device_discovered(GObject *source_object, guint arg_handle,
		gpointer user_data)
{
	net_nfc_target_handle_s *handle_info = NULL;

	NFC_INFO(">>> SIGNAL arrived");

	RET_IF(NULL == p2p_signal_handler.p2p_device_discovered_cb);

	handle_info = GUINT_TO_POINTER(arg_handle);


	p2p_signal_handler.p2p_device_discovered_cb(handle_info,
				p2p_signal_handler.p2p_device_discovered_data);

}

static void p2p_device_data_received(GObject *source_object, GVariant *arg_data,
		gpointer user_data)
{
	data_s p2p_data = { NULL, };

	NFC_INFO(">>> SIGNAL arrived");

	RET_IF(NULL == p2p_signal_handler.p2p_data_received_cb);

	net_nfc_util_gdbus_variant_to_data_s(arg_data, &p2p_data);
	p2p_signal_handler.p2p_data_received_cb(&p2p_data,
		p2p_signal_handler.p2p_data_received_data);

	net_nfc_util_free_data(&p2p_data);
}

static void p2p_call_send(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e out_result;
	NetNfcCallback *func_data = user_data;
	net_nfc_client_p2p_send_completed callback;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_p2p_call_send_finish(NET_NFC_GDBUS_P2P(source_object),
			(gint *)&out_result, res, &error);

	if (FALSE == ret)
	{
		out_result = NET_NFC_IPC_FAIL;

		NFC_ERR("Can not finish p2p send: %s", error->message);
		g_error_free(error);
	}

	if (func_data->callback != NULL)
	{
		callback = (net_nfc_client_p2p_send_completed)func_data->callback;

		callback(out_result, func_data->user_data);
	}

	g_free(func_data);
}


API net_nfc_error_e net_nfc_client_p2p_send(net_nfc_target_handle_s *handle,
		data_s *data, net_nfc_client_p2p_send_completed callback, void *user_data)
{
	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	return net_nfc_neard_send_p2p(handle, data, callback, user_data);
}



API net_nfc_error_e net_nfc_client_p2p_send_sync(net_nfc_target_handle_s *handle,
		data_s *data)
{
	gboolean ret;
	GVariant *arg_data;
	GError *error = NULL;
	net_nfc_error_e out_result;

	RETV_IF(NULL == p2p_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	arg_data = net_nfc_util_gdbus_data_to_variant(data);

	ret = net_nfc_gdbus_p2p_call_send_sync(p2p_proxy,
			0 /* FIXME */,
			arg_data,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			(gint *)&out_result,
			NULL,
			&error);

	if (FALSE == ret)
	{
		NFC_ERR("p2p send (sync call) failed: %s", error->message);

		g_error_free(error);
		out_result = NET_NFC_IPC_FAIL;
	}

	return out_result;
}


API void net_nfc_client_p2p_set_device_discovered(
		net_nfc_client_p2p_device_discovered callback, void *user_data)
{
	RET_IF(NULL == callback);

	net_nfc_neard_set_p2p_discovered(callback, user_data);
}


API void net_nfc_client_p2p_set_device_detached(
		net_nfc_client_p2p_device_detached callback, void *user_data)
{
	RET_IF(NULL == callback);

	net_nfc_neard_set_p2p_detached(callback, user_data);
}


API void net_nfc_client_p2p_set_data_received(
		net_nfc_client_p2p_data_received callback, void *user_data)
{
	RET_IF(NULL == callback);

	net_nfc_neard_set_p2p_data_received(callback, user_data);
}


API void net_nfc_client_p2p_unset_device_discovered(void)
{
	net_nfc_neard_unset_p2p_discovered();
}


API void net_nfc_client_p2p_unset_device_detached(void)
{
	net_nfc_neard_unset_p2p_detached();
}


API void net_nfc_client_p2p_unset_data_received(void)
{
	net_nfc_neard_unset_p2p_data_received();
}

net_nfc_error_e net_nfc_client_p2p_init(void)
{
	GError *error = NULL;

	if (p2p_proxy)
	{
		NFC_WARN("Already initialized");
		return NET_NFC_OK;
	}

	p2p_proxy = net_nfc_gdbus_p2p_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/P2p",
			NULL,
			&error);

	if (NULL == p2p_proxy)
	{
		NFC_ERR("Can not create proxy : %s", error->message);
		g_error_free(error);

		return NET_NFC_UNKNOWN_ERROR;
	}

	g_signal_connect(p2p_proxy, "detached", G_CALLBACK(p2p_device_detached), NULL);
	g_signal_connect(p2p_proxy, "discovered", G_CALLBACK(p2p_device_discovered), NULL);
	g_signal_connect(p2p_proxy, "received", G_CALLBACK(p2p_device_data_received), NULL);

	return NET_NFC_OK;
}

void net_nfc_client_p2p_deinit(void)
{
	if (p2p_proxy)
	{
		g_object_unref(p2p_proxy);
		p2p_proxy = NULL;
	}
}
