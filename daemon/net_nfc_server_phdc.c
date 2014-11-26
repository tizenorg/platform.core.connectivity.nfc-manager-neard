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

#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_server_common.h"
#include "net_nfc_server_tag.h"
#include "net_nfc_server_context.h"
#include "net_nfc_server_process_phdc.h"
#include "net_nfc_server_phdc.h"
#include "net_nfc_server_llcp.h"

typedef struct _PhdcSendData PhdcSendData;

struct _PhdcSendData
{
	NetNfcGDbusPhdc *phdc;
	GDBusMethodInvocation *invocation;
	guint32 phdc_handle;
	data_s data;
};

static NetNfcGDbusPhdc *phdc_skeleton = NULL;


static gboolean _phdc_send_request_cb_(net_nfc_phdc_handle_h handle,
		net_nfc_error_e result, net_nfc_server_phdc_indication_type indication,
		gpointer *user_data)
{

	NFC_DBG("phdc_send_request_cb_result [%d]",result);

	GVariant *parameter = (GVariant *)user_data;

	g_assert(parameter != NULL);

	NetNfcGDbusPhdc *object;
	GDBusMethodInvocation *invocation;
	net_nfc_phdc_handle_h phdc_handle;
	GVariant *phdc_data ;

	g_variant_get(parameter,
		"(uuu@a(y))",
		(guint *)&object,
		(guint *)&invocation,
		(guint *)&phdc_handle,
		(guint *)&phdc_data);

	net_nfc_gdbus_phdc_complete_send(object, invocation, (gint)result);

	g_variant_unref(phdc_data);
	g_object_unref(invocation);
	g_object_unref(object);
	g_variant_unref(parameter);

	result = NET_NFC_OK;
	return result;

}

static void phdc_send_data_thread_func(gpointer user_data)
{
	NetNfcGDbusPhdc *object;
	GDBusMethodInvocation *invocation;
	net_nfc_error_e result;
	net_nfc_phdc_handle_h handle = NULL;
	GVariant *phdc_data;
	data_s data = { NULL, };

	if (NULL == user_data)
	{
		NFC_ERR("cannot get PHDC client data");
		return;
	}

	NFC_DBG(">>> phdc_send_data_thread_func");

	g_variant_get((GVariant *)user_data,
		"(uuu@a(y))",
		(guint *)&object,
		(guint *)&invocation,
		(guint *)&handle,
		&phdc_data);

	g_assert(object != NULL);
	g_assert(invocation != NULL);

	net_nfc_util_gdbus_variant_to_data_s(phdc_data, &data);

	result = net_nfc_server_phdc_agent_request(handle, &data,
			_phdc_send_request_cb_, user_data);

	if (result != NET_NFC_OK)
	{
		net_nfc_gdbus_phdc_complete_send(object, invocation, (gint)result);

		g_object_unref(invocation);
		g_object_unref(object);
		g_variant_unref(user_data);
	}

	g_variant_unref(phdc_data);
}

static gboolean phdc_handle_send(NetNfcGDbusPhdc *phdc,
	GDBusMethodInvocation *invocation, //object for handling remote calls,provides a way to asynly return data
	guint handle,
	GVariant* user_data,
	GVariant *smack_privilege)
{
	bool ret;
	gboolean result;
	GVariant *parameter = NULL;

	NFC_INFO(">>> REQUEST from [%s]",g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
			"nfc-manager::p2p", "w");

	if(FALSE == ret)
	{
		NFC_ERR("permission denied, and finished request");
		result = NET_NFC_SECURITY_FAIL;
		goto ERROR;
	}

	parameter = g_variant_new("(uuu@a(y))",
		GPOINTER_TO_UINT(g_object_ref(phdc)),
		GPOINTER_TO_UINT(g_object_ref(invocation)),
		handle,
		user_data);

	if (NULL == parameter)
	{
		NFC_ERR("Memory allocation failed");
		result = NET_NFC_ALLOC_FAIL;

		goto ERROR;
	}

	result = net_nfc_server_controller_async_queue_push(phdc_send_data_thread_func, parameter);

	if (FALSE == result)
	{
		/* return error if queue was blocked */
		NFC_ERR("controller is processing important message..");
		result = NET_NFC_BUSY;

		goto ERROR;

	}

	return TRUE;

	ERROR :

	if (parameter != NULL)
	{
		g_object_unref(invocation);
		g_object_unref(phdc);
		g_variant_unref(parameter);
	}

	net_nfc_gdbus_phdc_complete_send(phdc, invocation, result);
	return TRUE;
}

void net_nfc_server_phdc_data_received_indication(data_s *arg_data)
{
	NFC_INFO("=net_nfc_server_phdc_transport_data_received_indication=");

	GVariant *data;
	data = net_nfc_util_gdbus_data_to_variant(arg_data);

	if (NULL == phdc_skeleton)
	{
		NFC_ERR("phdc_skeleton is not initialized");
		return;
	}

	net_nfc_gdbus_phdc_emit_phdc_received(phdc_skeleton, data);
}

static void _emit_phdc_event_signal(GVariant *parameter,
		net_nfc_phdc_handle_h handle, net_nfc_error_e result, uint32_t type)
{
	gboolean ret;
	char *client_id = NULL;
	void *user_data = NULL;
	GError *error = NULL;
	GDBusConnection *connection;

	g_variant_get(parameter, "(usu)", (guint *)&connection, &client_id,
			(guint *)&user_data);

	ret = g_dbus_connection_emit_signal(
			connection,
			client_id,
			"/org/tizen/NetNfcService/Phdc",
			"org.tizen.NetNfcService.Phdc",
			"PhdcEvent",
			g_variant_new("(iuu)", (gint)result, type,
				GPOINTER_TO_UINT(user_data)), &error);

	if (FALSE == ret)
	{
		if (error != NULL && error->message != NULL)
			NFC_ERR("g_dbus_connection_emit_signal failed : %s", error->message);
		else
			NFC_ERR("g_dbus_connection_emit_signal failed");
	}

	g_free(client_id);
}

static void _server_phdc_agent_cb_(net_nfc_phdc_handle_h handle,
		net_nfc_error_e result, net_nfc_server_phdc_indication_type indication, data_s *data)
{
	if(NET_NFC_OK != result)
	{
		net_nfc_server_phdc_transport_disconnect_indication();
		return;
	}

	RET_IF(NULL == handle);

	NFC_DBG(" handle [%p], result[%d]", handle, result);

	if( NET_NFC_PHDC_TARGET_CONNECTED == indication)
		net_nfc_server_phdc_transport_connect_indication(handle);
	else if(NET_NFC_PHDC_DATA_RECEIVED == indication)
		net_nfc_server_phdc_data_received_indication(data);

	return;
}


static void _phdc_agent_activate_cb(int event, net_nfc_target_handle_s *handle,
		uint32_t sap, const char *san, void *user_param)
{
	net_nfc_error_e result = NET_NFC_OK;
	GVariant *parameter = (GVariant *)user_param;
	char *client_id = NULL;
	void *user_data = NULL;
	GVariant *param = NULL;
	GDBusConnection *connection = NULL;

	NFC_DBG("event [%d], handle [%p], sap [%d], san [%s]", event, handle, sap, san);

	if (NET_NFC_LLCP_START == event)
	{
		g_variant_get(parameter, "(usu)", (guint *)&connection, &client_id, (guint *)&user_data);

		param = g_variant_new("(usu)", GPOINTER_TO_UINT(g_object_ref(connection)),client_id,
				GPOINTER_TO_UINT(user_data));

		g_free(client_id);

		/* start phdc agent service*/
		result = net_nfc_server_phdc_agent_start(handle, (char *)san, sap,
				_server_phdc_agent_cb_, param);

		if (NET_NFC_OK == result)
		{
			_emit_phdc_event_signal(parameter, handle, result, event);
		}
		else
		{
			NFC_ERR("net_nfc_server_phdc_manager_start failed, [%d]", result);
			g_variant_unref(param);
		}
	}
	else
	{
		_emit_phdc_event_signal(parameter, handle, result, NET_NFC_LLCP_UNREGISTERED);
		/* unregister server */
		g_variant_unref(parameter);
	}

}

static void _server_phdc_manager_cb_(net_nfc_phdc_handle_h handle,
		net_nfc_error_e result, net_nfc_server_phdc_indication_type indication, data_s *data)
{

	if(NET_NFC_OK != result)
	{
		net_nfc_server_phdc_transport_disconnect_indication();
		return;
	}

	RET_IF(NULL == handle);

	NFC_DBG("result [%d], data [%p]", result, data);

	if( NET_NFC_PHDC_TARGET_CONNECTED == indication)
		net_nfc_server_phdc_transport_connect_indication(handle);
	else if(NET_NFC_PHDC_DATA_RECEIVED == indication)
		net_nfc_server_phdc_data_received_indication(data);

	return ;
}


static void _phdc_manager_activate_cb(int event, net_nfc_target_handle_s *handle,
		uint32_t sap, const char *san, void *user_param)
{
	net_nfc_error_e result = NET_NFC_OK;
	GVariant *parameter = (GVariant *)user_param;
	char *client_id = NULL;
	void *user_data = NULL;
	GVariant *param = NULL;
	GDBusConnection *connection = NULL;

	NFC_DBG("event [%d], handle [%p], sap [%d], san [%s]", event, handle, sap, san);

	if (NET_NFC_LLCP_START == event)
	{
		g_variant_get(parameter, "(usu)", (guint *)&connection, &client_id, (guint *)&user_data);

		param = g_variant_new("(usu)", GPOINTER_TO_UINT(g_object_ref(connection)),client_id,
				GPOINTER_TO_UINT(user_data));

		g_free(client_id);

		/* start phdc manager service*/
		result = net_nfc_server_phdc_manager_start(handle, (char *)san, sap,
				_server_phdc_manager_cb_, param);

		if (NET_NFC_OK == result)
		{
			_emit_phdc_event_signal(parameter, handle, result, event);
		}
		else
		{
			NFC_ERR("net_nfc_server_phdc_manager_start failed, [%d]", result);
			g_variant_unref(param);
		}
	}
	else
	{
		_emit_phdc_event_signal(parameter, handle, result, NET_NFC_LLCP_UNREGISTERED);
		/* unregister server */
		g_variant_unref(parameter);
	}

}

static void phdc_register_server_thread_func(gpointer user_data)
{
	gchar *arg_san = NULL;
	guint arg_role;
	guint arg_sap;
	guint arg_user_data;
	net_nfc_error_e result = NET_NFC_OK;
	NetNfcGDbusPhdc *object = NULL;
	g_assert(user_data != NULL);
	GVariant *parameter = NULL;
	GDBusConnection *connection = NULL;
	GDBusMethodInvocation *invocation = NULL;

	g_variant_get((GVariant *)user_data, "(uuusu)", (guint *)&object, (guint *)&invocation,
			&arg_role, &arg_san, &arg_user_data);

	g_assert(object != NULL);
	g_assert(invocation != NULL);

	connection = g_dbus_method_invocation_get_connection(invocation);

	parameter = g_variant_new("(usu)", GPOINTER_TO_UINT(g_object_ref(connection)),
			g_dbus_method_invocation_get_sender(invocation), arg_user_data);

	if (parameter != NULL)
	{
		if(!strcmp(arg_san,PHDC_SAN))
		{
			arg_san = PHDC_SAN;
			arg_sap = PHDC_SAP;
		}
		else if (!strcmp(arg_san,PHDS_SAN))
		{
			arg_san = PHDS_SAN;
			arg_sap = PHDS_SAP;
		}
		else
		{
			// anything else, as of now,defaulting to PHDC default server
			arg_san = PHDC_SAN;
			arg_sap = PHDC_SAP;
		}

		if(NET_NFC_PHDC_MANAGER == arg_role)
		{
			result = net_nfc_server_llcp_register_service(
					g_dbus_method_invocation_get_sender(invocation), arg_sap,
					arg_san, _phdc_manager_activate_cb, 	parameter);
		}
		else if(NET_NFC_PHDC_AGENT == arg_role)
		{
			result = net_nfc_server_llcp_register_service(
					g_dbus_method_invocation_get_sender(invocation), arg_sap,
					arg_san, _phdc_agent_activate_cb, 	parameter);
		}

		if (result != NET_NFC_OK)
		{
			NFC_ERR("net_nfc_service_llcp_register_service failed, [%d]", result);
			g_object_unref(connection);
			g_variant_unref(parameter);
		}
	}
	else
	{
		result = NET_NFC_ALLOC_FAIL;
		g_object_unref(connection);
	}

	net_nfc_gdbus_phdc_complete_register_role(object, invocation, result);


	g_variant_unref(user_data);


}

static gboolean phdc_handle_register(
		NetNfcGDbusPhdc *object,
		GDBusMethodInvocation *invocation,
		guint arg_role,
		const gchar *arg_san,
		guint arg_user_data,
		GVariant *arg_privilege)
{
	bool ret;
	gboolean result;
	GVariant *parameter = NULL;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, arg_privilege,
			"nfc-manager::p2p", "rw");
	if (FALSE == ret)
	{
		NFC_ERR("permission denied, and finished request");
		result = NET_NFC_SECURITY_FAIL;
		goto ERROR;
	}

	parameter = g_variant_new("(uuusu)", GPOINTER_TO_UINT(g_object_ref(object)),
			GPOINTER_TO_UINT(g_object_ref(invocation)), arg_role, arg_san, arg_user_data);

	if (parameter == NULL)
	{
		NFC_ERR("Memory allocation failed");
		result = NET_NFC_ALLOC_FAIL;
		goto ERROR;
	}
	if(net_nfc_server_controller_async_queue_push(
			phdc_register_server_thread_func, parameter) == FALSE)
	{
		NFC_ERR("controller is processing important message.");
		result = NET_NFC_ALLOC_FAIL;
		goto ERROR;
	}

	return TRUE;

	ERROR :
	if (parameter != NULL)
	{
		g_object_unref(invocation);
		g_object_unref(object);
		g_variant_unref(parameter);
	}

	net_nfc_gdbus_phdc_complete_register_role(object,
		invocation,
		result);

	return TRUE;
}

static void phdc_unregister_server_thread_func(gpointer user_data)
{
	guint arg_role;
	guint arg_sap;
	gchar *arg_san = NULL;
	net_nfc_error_e result;
	NetNfcGDbusSnep *object = NULL;
	GDBusMethodInvocation *invocation = NULL;

	g_assert(user_data != NULL);

	g_variant_get((GVariant *)user_data, "(uuus)", (guint *)&object, (guint *)&invocation,
			&arg_role, &arg_san);

	g_assert(object != NULL);
	g_assert(invocation != NULL);

	if(!strcmp(arg_san,PHDS_SAN))
		arg_sap = PHDS_SAP;
	else
		arg_sap = PHDC_SAP;

	result = net_nfc_server_llcp_unregister_service(
			g_dbus_method_invocation_get_sender(invocation), arg_sap, arg_san);

	net_nfc_gdbus_snep_complete_server_unregister(object, invocation, result);

	g_free(arg_san);

	g_object_unref(invocation);
	g_object_unref(object);

	g_variant_unref(user_data);
}

static gboolean phdc_handle_unregister(
		NetNfcGDbusPhdc *object,
		GDBusMethodInvocation *invocation,
		guint arg_role,
		const gchar *arg_san,
		GVariant *arg_privilege)
{
	bool ret;
	gboolean result;
	GVariant *parameter = NULL;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, arg_privilege,
			"nfc-manager::p2p", "rw");

	if (FALSE == ret)
	{
		NFC_ERR("permission denied, and finished request");
		result = NET_NFC_SECURITY_FAIL;
		goto ERROR;
	}

	parameter = g_variant_new("(uuus)", GPOINTER_TO_UINT(g_object_ref(object)),
			GPOINTER_TO_UINT(g_object_ref(invocation)), arg_role, arg_san);

	if (parameter == NULL)
	{
		NFC_ERR("Memory allocation failed");
		result = NET_NFC_ALLOC_FAIL;
		goto ERROR;
	}
	if(net_nfc_server_controller_async_queue_push(
			phdc_unregister_server_thread_func, parameter) == FALSE)
	{
		NFC_ERR("controller is processing important message.");
		result = NET_NFC_ALLOC_FAIL;
		goto ERROR;
	}

	return TRUE;

	ERROR :
	if (parameter != NULL)
	{
			g_object_unref(invocation);
			g_object_unref(object);

			g_variant_unref(parameter);
	}

	net_nfc_gdbus_phdc_complete_register_role(object,
		invocation,
		result);

	return TRUE;
}


gboolean net_nfc_server_phdc_init(GDBusConnection *connection)
{
	gboolean result;
	GError *error = NULL;

	if (phdc_skeleton)
		net_nfc_server_phdc_deinit();

	phdc_skeleton = net_nfc_gdbus_phdc_skeleton_new();

	g_signal_connect(phdc_skeleton,"handle-send", G_CALLBACK(phdc_handle_send), NULL);

	g_signal_connect(phdc_skeleton,"handle-register-role", G_CALLBACK(phdc_handle_register), NULL);

	g_signal_connect(phdc_skeleton,"handle-unregister-role", G_CALLBACK(phdc_handle_unregister), NULL);

	result = g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(phdc_skeleton),
			connection, "/org/tizen/NetNfcService/Phdc", &error);

	if (FALSE == result)
	{
		g_error_free(error);
		net_nfc_server_phdc_deinit();
	}

	return result;
}

void net_nfc_server_phdc_deinit(void)
{
	if (phdc_skeleton)
	{
		g_object_unref(phdc_skeleton);
		phdc_skeleton = NULL;
	}
}

void net_nfc_server_phdc_transport_disconnect_indication(void)
{
	NFC_INFO("====== phdc target disconnected ======");

	/* release target information */
	net_nfc_server_free_target_info();

	if (phdc_skeleton != NULL)
		net_nfc_gdbus_phdc_emit_phdc_disconnect(phdc_skeleton);
}


void net_nfc_server_phdc_transport_connect_indication(net_nfc_phdc_handle_h handle)
{
	NFC_INFO("====== phdc target connected ======");

	if (NULL == phdc_skeleton)
	{
		NFC_ERR("phdc_skeleton is not initialized");
		return;
	}

	net_nfc_gdbus_phdc_emit_phdc_connect(phdc_skeleton, GPOINTER_TO_UINT(handle));
}

void net_nfc_server_phdc_data_sent(net_nfc_error_e result, gpointer user_data)
{
	PhdcSendData *data = (PhdcSendData *)user_data;

	g_assert(data != NULL);
	g_assert(data->phdc != NULL);
	g_assert(data->invocation != NULL);

	net_nfc_gdbus_phdc_complete_send(data->phdc, data->invocation, (gint)result);

	net_nfc_util_free_data(&data->data);

	g_object_unref(data->invocation);
	g_object_unref(data->phdc);

	g_free(data);
}

