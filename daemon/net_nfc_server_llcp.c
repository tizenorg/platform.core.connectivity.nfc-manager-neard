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
#include <pmapi.h>/*for pm lock*/

#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_gdbus.h"

#include "net_nfc_server_controller.h"
#include "net_nfc_server_common.h"
#include "net_nfc_server_context.h"
#include "net_nfc_server_tag.h"
#include "net_nfc_server_llcp.h"
#include "net_nfc_server_p2p.h"
#include "net_nfc_server_process_snep.h"
#include "net_nfc_server_process_npp.h"
#include "net_nfc_server_process_handover.h"

/* default llcp configurations */
#define NET_NFC_LLCP_MIU	128
#define NET_NFC_LLCP_WKS	1
#define NET_NFC_LLCP_LTO	10
#define NET_NFC_LLCP_OPT	0

static NetNfcGDbusLlcp *llcp_skeleton = NULL;

static net_nfc_llcp_config_info_s llcp_config =
{
	NET_NFC_LLCP_MIU,
	NET_NFC_LLCP_WKS,
	NET_NFC_LLCP_LTO,
	NET_NFC_LLCP_OPT,
};


typedef struct _llcp_client_data
{
	GDBusConnection *connection;
	char *id;
	net_nfc_target_handle_s *handle;
	net_nfc_llcp_socket_t socket;
	void *user_data;
}
llcp_client_data;


typedef struct _LlcpData LlcpData;

struct _LlcpData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;
};

typedef struct _LlcpConfigData LlcpConfigData;

struct _LlcpConfigData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint16 miu;
	guint16 wks;
	guint8 lto;
	guint8 option;
};

typedef struct _LlcpListenData LlcpListenData;

struct _LlcpListenData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
	guint16 miu;
	guint8 rw;
	guint type;
	guint sap;
	gchar *service_name;
};

typedef struct _LlcpAcceptData LlcpAcceptData;

struct _LlcpAcceptData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint handle;
	guint client_socket;
};

typedef struct _LlcpConnectData LlcpConnectData;

struct _LlcpConnectData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
	guint16 miu;
	guint8 rw;
	guint type;
	gchar *service_name;
};

typedef struct _LlcpConnectSapData LlcpConnectSapData;

struct _LlcpConnectSapData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
	guint16 miu;
	guint8 rw;
	guint type;
	guint sap;
};

typedef struct _LlcpSendData LlcpSendData;

struct _LlcpSendData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;

	data_s data;
};

typedef struct _LlcpSendToData LlcpSendToData;

struct _LlcpSendToData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
	guint8 sap;

	data_s data;
};

typedef struct _LlcpReceiveData LlcpReceiveData;

struct _LlcpReceiveData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
	guint32 req_length;
};

typedef struct _LlcpCloseData LlcpCloseData;

struct _LlcpCloseData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
};

typedef struct _LlcpDisconnectData LlcpDisconnectData;

struct _LlcpDisconnectData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
};

typedef struct _LlcpSimpleData LlcpSimpleData;

struct _LlcpSimpleData
{
	net_nfc_target_handle_s *handle;
	net_nfc_llcp_socket_t socket;
	guint32 miu;
	net_nfc_server_llcp_callback callback;
	net_nfc_server_llcp_callback error_callback;
	gpointer user_data;
};

static void llcp_socket_error_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	gboolean ret;
	GError *error = NULL;
	llcp_client_data *client_data = user_param;

	ret = g_dbus_connection_emit_signal(
				client_data->connection,
				client_data->id,
			   "/org/tizen/NetNfcService/Llcp",
				"org.tizen.NetNfcService.Llcp",
				"Error",
				g_variant_new("(uui)",
				GPOINTER_TO_UINT(client_data->handle),
				socket,
				result),
				&error);

	if (FALSE == ret)
	{
		if (error != NULL && error->message != NULL)
			NFC_ERR("g_dbus_connection_emit_signal failed : %s", error->message);
		else
			NFC_ERR("g_dbus_connection_emit_signal failed");
	}
}

static void llcp_incoming_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	gboolean ret;
	GError *error = NULL;
	llcp_client_data *client_data = user_param;

	ret = g_dbus_connection_emit_signal(
				client_data->connection,
				client_data->id,
				"/org/tizen/NetNfcService/Llcp",
				"org.tizen.NetNfcService.Llcp",
				"Incoming",
				g_variant_new("(uuu)",
					GPOINTER_TO_UINT(client_data->handle),
					client_data->socket,
					socket),
				&error);

	if (FALSE == ret)
	{
		if (error != NULL && error->message != NULL)
			NFC_ERR("g_dbus_connection_emit_signal failed : %s", error->message);
		else
			NFC_ERR("g_dbus_connection_emit_signal failed");
	}
}

static void llcp_connect_by_url_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	LlcpConnectData *llcp_data = user_param;

	g_assert(llcp_data != NULL);
	g_assert(llcp_data->llcp != NULL);
	g_assert(llcp_data->invocation != NULL);

	net_nfc_gdbus_llcp_complete_connect(llcp_data->llcp, llcp_data->invocation,
			result, socket);

	g_free(llcp_data->service_name);

	g_object_unref(llcp_data->invocation);
	g_object_unref(llcp_data->llcp);

	g_free(llcp_data);
}

static void llcp_connect_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	LlcpConnectSapData *llcp_data = user_param;

	g_assert(llcp_data != NULL);
	g_assert(llcp_data->llcp != NULL);
	g_assert(llcp_data->invocation != NULL);

	net_nfc_gdbus_llcp_complete_connect_sap(llcp_data->llcp, llcp_data->invocation,
			result, socket);

	g_object_unref(llcp_data->invocation);
	g_object_unref(llcp_data->llcp);

	g_free(llcp_data);
}

static void llcp_send_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	LlcpSendData *llcp_data = user_param;

	g_assert(llcp_data != NULL);
	g_assert(llcp_data->llcp != NULL);
	g_assert(llcp_data->invocation != NULL);

	net_nfc_gdbus_llcp_complete_send(llcp_data->llcp, llcp_data->invocation,
			result, socket);

	net_nfc_util_free_data(&llcp_data->data);

	g_object_unref(llcp_data->invocation);
	g_object_unref(llcp_data->llcp);

	g_free(llcp_data);
}

static void llcp_send_to_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	LlcpSendToData *llcp_data = user_param;

	g_assert(llcp_data != NULL);
	g_assert(llcp_data->llcp != NULL);
	g_assert(llcp_data->invocation != NULL);

	net_nfc_gdbus_llcp_complete_send_to(llcp_data->llcp, llcp_data->invocation,
			result, socket);

	net_nfc_util_free_data(&llcp_data->data);

	g_object_unref(llcp_data->invocation);
	g_object_unref(llcp_data->llcp);

	g_free(llcp_data);
}

static void llcp_receive_cb(net_nfc_llcp_socket_t socket, net_nfc_error_e result,
		data_s *data, void *extra, void *user_param)
{
	GVariant *variant;
	LlcpReceiveData *llcp_data = user_param;

	g_assert(llcp_data != NULL);
	g_assert(llcp_data->llcp != NULL);
	g_assert(llcp_data->invocation != NULL);

	variant = net_nfc_util_gdbus_data_to_variant(data);
	net_nfc_gdbus_llcp_complete_receive(llcp_data->llcp, llcp_data->invocation,
			result, variant);

	g_object_unref(llcp_data->invocation);
	g_object_unref(llcp_data->llcp);

	g_free(llcp_data);
}

static void llcp_receive_from_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	GVariant *variant;
	LlcpReceiveData *llcp_data = user_param;

	g_assert(llcp_data != NULL);
	g_assert(llcp_data->llcp != NULL);
	g_assert(llcp_data->invocation != NULL);

	variant = net_nfc_util_gdbus_data_to_variant(data);
	net_nfc_gdbus_llcp_complete_receive_from(llcp_data->llcp, llcp_data->invocation,
			result, (guint8) (int)extra, variant);

	g_object_unref(llcp_data->invocation);
	g_object_unref(llcp_data->llcp);

	g_free(llcp_data);
}


static void llcp_disconnect_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	LlcpDisconnectData *llcp_data = user_param;

	g_assert(llcp_data != NULL);
	g_assert(llcp_data->llcp != NULL);
	g_assert(llcp_data->invocation != NULL);

	net_nfc_gdbus_llcp_complete_disconnect(llcp_data->llcp, llcp_data->invocation,
			result, socket);

	g_object_unref(llcp_data->invocation);
	g_object_unref(llcp_data->llcp);

	g_free(llcp_data);
}

static void llcp_handle_listen_thread_func(gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	llcp_client_data *client_data;
	LlcpListenData *data = user_data;
	net_nfc_llcp_socket_t socket = -1;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	client_data = g_try_new0(llcp_client_data, 1);
	if (NULL == client_data)
	{
		result = NET_NFC_ALLOC_FAIL;

		goto ERROR;
	}

	client_data->connection = g_dbus_method_invocation_get_connection(data->invocation);
	client_data->id = g_strdup(g_dbus_method_invocation_get_sender(data->invocation));
	client_data->handle = (net_nfc_target_handle_s*)data->handle;

	ret = net_nfc_controller_llcp_create_socket(&socket, data->type, data->miu, data->rw,
				&result, llcp_socket_error_cb, client_data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_create_socket failed [%d]", result);

		goto ERROR;
	}

	client_data->socket = socket;

	if (net_nfc_controller_llcp_bind(socket, data->sap, &result) == false)
	{
		NFC_ERR("net_nfc_controller_llcp_bind failed [%d]", result);

		goto ERROR;
	}

	ret = net_nfc_controller_llcp_listen(GUINT_TO_POINTER(data->handle),
				(uint8_t *)data->service_name,
				socket,
				&result,
				llcp_incoming_cb,
				client_data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_listen failed [%d]", result);

		goto ERROR;
	}

	net_nfc_gdbus_llcp_complete_listen(data->llcp, data->invocation, result,
			GPOINTER_TO_UINT(socket));

	g_object_unref(data->invocation);
	g_object_unref(data->llcp);

	g_free(data);

	return;

ERROR :
	net_nfc_gdbus_llcp_complete_listen(data->llcp, data->invocation, result, -1);

	if (socket != -1)
		net_nfc_controller_llcp_socket_close(socket, &result);

	g_free(client_data);

	g_free(data->service_name);

	g_object_unref(data->invocation);
	g_object_unref(data->llcp);

	g_free(data);
}

static void llcp_handle_accept_thread_func(gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	llcp_client_data *client_data;
	LlcpAcceptData *data = user_data;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	client_data = g_try_new0(llcp_client_data, 1);
	if (NULL == client_data)
	{
		result = NET_NFC_ALLOC_FAIL;

		goto ERROR;
	}

	client_data->connection = g_dbus_method_invocation_get_connection(data->invocation);
	client_data->id = g_strdup(g_dbus_method_invocation_get_sender(data->invocation));
	client_data->handle = (net_nfc_target_handle_s*)data->handle;
	client_data->socket = data->client_socket;

	ret = net_nfc_controller_llcp_accept(data->client_socket, &result,
				llcp_socket_error_cb, client_data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_accept failed [%d]", result);

		goto ERROR;
	}

	net_nfc_gdbus_llcp_complete_accept(data->llcp, data->invocation, result);

	g_object_unref(data->invocation);
	g_object_unref(data->llcp);

	g_free(data);

	return;

ERROR :
	net_nfc_gdbus_llcp_complete_accept(data->llcp, data->invocation, result);

	g_free(client_data);

	g_object_unref(data->invocation);
	g_object_unref(data->llcp);

	g_free(data);
}

static void llcp_handle_reject_thread_func(gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	LlcpAcceptData *data = user_data;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	ret = net_nfc_controller_llcp_reject(GUINT_TO_POINTER(data->handle),
				data->client_socket, &result);

	if (false == ret)
		NFC_ERR("net_nfc_controller_llcp_reject failed [%d]", result);

	net_nfc_gdbus_llcp_complete_reject(data->llcp, data->invocation, result);

	g_object_unref(data->invocation);
	g_object_unref(data->llcp);

	g_free(data);
}


static void llcp_handle_connect_thread_func(gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	llcp_client_data *client_data;
	LlcpConnectData *data = user_data;
	net_nfc_llcp_socket_t socket = -1;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	client_data = g_try_new0(llcp_client_data, 1);

	if (NULL == client_data)
	{
		result = NET_NFC_ALLOC_FAIL;

		goto ERROR;
	}

	client_data->connection = g_dbus_method_invocation_get_connection(data->invocation);
	client_data->id = g_strdup(g_dbus_method_invocation_get_sender(data->invocation));
	client_data->handle = (net_nfc_target_handle_s*)data->handle;

	ret = net_nfc_controller_llcp_create_socket(&socket, data->type, data->miu, data->rw,
				&result, llcp_socket_error_cb, client_data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_create_socket failed [%d]", result);

		goto ERROR;
	}

	client_data->socket = socket;

	ret = net_nfc_controller_llcp_connect_by_url(
				GUINT_TO_POINTER(data->handle),
				socket,
				(uint8_t *)data->service_name,
				&result,
				llcp_connect_by_url_cb,
				data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_listen failed [%d]", result);

		goto ERROR;
	}

	g_object_unref(data->invocation);
	g_object_unref(data->llcp);

	g_free(data);

	return;

ERROR :
	net_nfc_gdbus_llcp_complete_connect(data->llcp, data->invocation, result, -1);

	if (socket != -1)
		net_nfc_controller_llcp_socket_close(socket, &result);

	g_free(client_data);

	g_free(data->service_name);

	g_object_unref(data->invocation);
	g_object_unref(data->llcp);

	g_free(data);
}

static void llcp_handle_connect_sap_thread_func(gpointer user_data)
{
	bool ret;
	llcp_client_data *client_data;
	net_nfc_llcp_socket_t socket = -1;
	net_nfc_error_e result = NET_NFC_OK;
	LlcpConnectSapData *data = user_data;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	client_data = g_try_new0(llcp_client_data, 1);
	if (NULL == client_data)
	{
		result = NET_NFC_ALLOC_FAIL;

		goto ERROR;
	}

	client_data->connection = g_dbus_method_invocation_get_connection(data->invocation);
	client_data->id = g_strdup(g_dbus_method_invocation_get_sender(data->invocation));
	client_data->handle = (net_nfc_target_handle_s*)data->handle;

	ret = net_nfc_controller_llcp_create_socket(&socket, data->type, data->miu, data->rw,
				&result, llcp_socket_error_cb, client_data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_create_socket failed [%d]", result);

		goto ERROR;
	}

	client_data->socket = socket;

	ret = net_nfc_controller_llcp_connect(GUINT_TO_POINTER(data->handle), socket,
				data->sap, &result, llcp_connect_cb, data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_listen failed [%d]", result);

		goto ERROR;
	}

	g_object_unref(data->invocation);
	g_object_unref(data->llcp);

	g_free(data);

	return;

ERROR :
	net_nfc_gdbus_llcp_complete_connect_sap(data->llcp, data->invocation, result, -1);

	if (socket != -1)
		net_nfc_controller_llcp_socket_close(socket, &result);

	g_free(client_data);

	g_object_unref(data->invocation);
	g_object_unref(data->llcp);

	g_free(data);
}

static void llcp_handle_send_thread_func(gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	LlcpSendData *data = user_data;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	ret = net_nfc_controller_llcp_send(GUINT_TO_POINTER(data->handle),
				data->client_socket, &data->data, &result, llcp_send_cb, data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_send failed [%d]", result);

		net_nfc_gdbus_llcp_complete_send(data->llcp, data->invocation, result,
				data->client_socket);

		net_nfc_util_free_data(&data->data);

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}
}

static void llcp_handle_send_to_thread_func(gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	LlcpSendToData *data = user_data;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	ret = net_nfc_controller_llcp_send_to(GUINT_TO_POINTER(data->handle),
				data->client_socket,
				&data->data,
				data->sap,
				&result,
				llcp_send_to_cb,
				data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_send_to failed [%d]", result);

		net_nfc_gdbus_llcp_complete_send_to(data->llcp, data->invocation, result,
				data->client_socket);

		net_nfc_util_free_data(&data->data);

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}
}

static void llcp_handle_receive_thread_func(gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	LlcpReceiveData *data = user_data;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	ret = net_nfc_controller_llcp_recv(GUINT_TO_POINTER(data->handle),
				data->client_socket,
				data->req_length,
				&result,
				llcp_receive_cb,
				data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_recv failed [%d]", result);

		net_nfc_gdbus_llcp_complete_receive(data->llcp, data->invocation, result,
				net_nfc_util_gdbus_buffer_to_variant(NULL, 0));

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}
}

static void llcp_handle_receive_from_thread_func(gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	LlcpReceiveData *data = user_data;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	ret = net_nfc_controller_llcp_recv_from(GUINT_TO_POINTER(data->handle),
				data->client_socket,
				data->req_length,
				&result,
				llcp_receive_from_cb,
				data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_recv_from failed [%d]", result);

		net_nfc_gdbus_llcp_complete_receive_from(data->llcp, data->invocation, result, -1,
				net_nfc_util_gdbus_buffer_to_variant(NULL, 0));

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}
}

static void llcp_handle_close_thread_func(gpointer user_data)
{
	net_nfc_error_e result;
	LlcpCloseData *data = user_data;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	net_nfc_controller_llcp_socket_close(data->client_socket, &result);

	net_nfc_gdbus_llcp_complete_close(data->llcp, data->invocation, result,
			data->client_socket);
}

static void llcp_handle_disconnect_thread_func(gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	LlcpDisconnectData *data = user_data;

	g_assert(data != NULL);
	g_assert(data->llcp != NULL);
	g_assert(data->invocation != NULL);

	if (GUINT_TO_POINTER(data->handle) == 0)
	{
		int ret_val;

		net_nfc_server_free_target_info();
		ret_val = pm_unlock_state(LCD_NORMAL, PM_RESET_TIMER);
		NFC_DBG("net_nfc_controller_disconnect pm_unlock_state[%d]!!", ret_val);
	}

	ret = net_nfc_controller_llcp_disconnect(GUINT_TO_POINTER(data->handle),
				data->client_socket, &result, llcp_disconnect_cb, data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_disconnect failed [%d]", result);

		net_nfc_gdbus_llcp_complete_disconnect(data->llcp, data->invocation, result,
				data->client_socket);

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}
}

static gboolean llcp_handle_listen(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint16 arg_miu,
		guint8 arg_rw,
		gint32 arg_type,
		guint8 arg_sap,
		const gchar *arg_service_name,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpListenData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
			"nfc-manager::p2p", "rw");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpListenData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->miu = arg_miu;
	data->rw = arg_rw;
	data->type = arg_type;
	data->sap = arg_sap;
	data->service_name = g_strdup(arg_service_name);

	result = net_nfc_server_controller_async_queue_push(llcp_handle_listen_thread_func,
			data);

	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		g_free(data->service_name);

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}

	return result;
}

static gboolean llcp_handle_accept(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpAcceptData *data;

	NFC_INFO(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
			"nfc-manager::p2p", "rw");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpAcceptData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.MemoryError", "Out of memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;

	result = net_nfc_server_controller_async_queue_push(llcp_handle_accept_thread_func,
			data);

	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}

	return result;
}

static gboolean llcp_handle_reject(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpAcceptData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
			"nfc-manager::p2p", "rw");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpAcceptData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.MemoryError", "Out of memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;

	result = net_nfc_server_controller_async_queue_push(llcp_handle_reject_thread_func,
			data);

	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}

	return result;
}

static gboolean llcp_handle_connect(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint16 arg_miu,
		guint8 arg_rw,
		gint32 arg_type,
		const gchar *arg_service_name,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpConnectData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
			"nfc-manager::p2p", "rw");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpConnectData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->miu = arg_miu;
	data->rw = arg_rw;
	data->type = arg_type;
	data->service_name = g_strdup(arg_service_name);

	result = net_nfc_server_controller_async_queue_push(llcp_handle_connect_thread_func,
			data);

	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		g_free(data->service_name);

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}

	return result;
}

static gboolean llcp_handle_connect_sap(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint16 arg_miu,
		guint8 arg_rw,
		gint32 arg_type,
		guint8 arg_sap,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpConnectSapData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
			"nfc-manager::p2p", "rw");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpConnectSapData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->miu = arg_miu;
	data->rw = arg_rw;
	data->type = arg_type;
	data->sap = arg_sap;

	result = net_nfc_server_controller_async_queue_push(
			llcp_handle_connect_sap_thread_func, data);

	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}

	return result;
}

static gboolean llcp_handle_send(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		GVariant *arg_data,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpSendData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::p2p", "w");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpSendData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;

	net_nfc_util_gdbus_variant_to_data_s(arg_data, &data->data);

	result = net_nfc_server_controller_async_queue_push(llcp_handle_send_thread_func,
			data);

	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		net_nfc_util_free_data(&data->data);

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}

	return result;
}

static gboolean llcp_handle_send_to(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint8 arg_sap,
		GVariant *arg_data,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpSendToData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::p2p", "rw");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpSendToData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->sap = arg_sap;

	net_nfc_util_gdbus_variant_to_data_s(arg_data, &data->data);

	result = net_nfc_server_controller_async_queue_push(llcp_handle_send_to_thread_func,
			data);

	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		net_nfc_util_free_data(&data->data);

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}

	return result;
}

static gboolean llcp_handle_receive(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_req_length,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpReceiveData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::p2p", "w");

	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpReceiveData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->req_length = arg_req_length;

	result = net_nfc_server_controller_async_queue_push(llcp_handle_receive_thread_func,
			data);
	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}

	return result;
}

static gboolean llcp_handle_receive_from(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_req_length,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpReceiveData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::p2p", "w");

	/* check privilege and update client context */
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpReceiveData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->req_length = arg_req_length;

	result = net_nfc_server_controller_async_queue_push(
			llcp_handle_receive_from_thread_func, data);
	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}

	return result;
}

static gboolean llcp_handle_close(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpCloseData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::p2p", "w");
	/* check privilege and update client context */
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpCloseData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;

	result = net_nfc_server_controller_async_queue_push(llcp_handle_close_thread_func,
			data);

	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		g_free(data);
	}

	return result;
}

static gboolean llcp_handle_disconnect(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	LlcpDisconnectData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::p2p", "w");
	/* check privilege and update client context */
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(LlcpDisconnectData, 1);
	if (NULL == data)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;

	result = net_nfc_server_controller_async_queue_push(llcp_handle_disconnect_thread_func,
			data);

	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Llcp.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->invocation);
		g_object_unref(data->llcp);

		g_free(data);
	}

	return result;
}

void net_nfc_server_llcp_deactivated(gpointer user_data)
{
	net_nfc_target_handle_s *handle = user_data;

	if (handle != NULL)
	{
		net_nfc_error_e result = NET_NFC_OK;

		if (net_nfc_controller_disconnect(handle, &result) == false)
		{
			if (result != NET_NFC_NOT_CONNECTED)
				net_nfc_controller_exception_handler();
			else
				NFC_ERR("target was not connected.");
		}

		net_nfc_server_set_state(NET_NFC_SERVER_IDLE);
	}
	else
	{
		NFC_ERR("the target was disconnected");
	}

	/* send p2p detatch */
	net_nfc_server_p2p_detached();
}

static void llcp_simple_socket_error_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	LlcpSimpleData *simple_data = user_param;

	g_assert(simple_data != NULL);

	if (simple_data->error_callback)
	{
		simple_data->error_callback(result, simple_data->handle, socket, data,
				simple_data->user_data);
	}

	g_free(simple_data);
}

static void llcp_simple_listen_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	LlcpSimpleData *simple_data = user_param;

	g_assert(simple_data != NULL);

	if (result != NET_NFC_OK)
		NFC_ERR("listen socket failed, [%d]", result);

	if (simple_data->callback)
	{
		simple_data->callback(result, simple_data->handle, socket, data,
				simple_data->user_data);
	}

	/* 'simple_data' will be freed in socket error callback */
}

static void llcp_simple_connect_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	LlcpSimpleData *simple_data = user_param;

	g_assert(simple_data != NULL);

	if (result != NET_NFC_OK)
		NFC_ERR("connect socket failed, [%d]", result);

	if (simple_data->callback)
	{
		simple_data->callback(result, simple_data->handle, socket, data,
				simple_data->user_data);
	}

	/* 'simple_data' will be freed in socket error callback */
}

static void llcp_simple_send_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	LlcpSimpleData *simple_data = user_param;

	g_assert(simple_data != NULL);

	if (simple_data->callback)
	{
		simple_data->callback(result, simple_data->handle, socket, data,
				simple_data->user_data);
	}

	g_free(simple_data);
}

static void llcp_simple_receive_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	LlcpSimpleData *simple_data = user_param;

	g_assert(simple_data != NULL);

	if (simple_data->callback)
	{
		simple_data->callback(result, simple_data->handle, socket, data,
				simple_data->user_data);
	}

	g_free(simple_data);
}

/* Public Function */
gboolean net_nfc_server_llcp_init(GDBusConnection *connection)
{
	gboolean result;
	GError *error = NULL;

	if (llcp_skeleton)
		g_object_unref(llcp_skeleton);

	llcp_skeleton = net_nfc_gdbus_llcp_skeleton_new();

	g_signal_connect(llcp_skeleton, "handle-listen",
			G_CALLBACK(llcp_handle_listen), NULL);

	g_signal_connect(llcp_skeleton, "handle-accept",
			G_CALLBACK(llcp_handle_accept), NULL);

	g_signal_connect(llcp_skeleton, "handle-reject",
			G_CALLBACK(llcp_handle_reject), NULL);

	g_signal_connect(llcp_skeleton, "handle-connect",
			G_CALLBACK(llcp_handle_connect), NULL);

	g_signal_connect(llcp_skeleton, "handle-connect-sap",
			G_CALLBACK(llcp_handle_connect_sap), NULL);

	g_signal_connect(llcp_skeleton, "handle-send", G_CALLBACK(llcp_handle_send), NULL);

	g_signal_connect(llcp_skeleton, "handle-send-to",
			G_CALLBACK(llcp_handle_send_to), NULL);

	g_signal_connect(llcp_skeleton, "handle-receive",
			G_CALLBACK(llcp_handle_receive), NULL);

	g_signal_connect(llcp_skeleton, "handle-receive-from",
			G_CALLBACK(llcp_handle_receive_from), NULL);

	g_signal_connect(llcp_skeleton, "handle-close", G_CALLBACK(llcp_handle_close), NULL);

	g_signal_connect(llcp_skeleton, "handle-disconnect",
			G_CALLBACK(llcp_handle_disconnect), NULL);

	result = g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(llcp_skeleton),
			connection, "/org/tizen/NetNfcService/Llcp", &error);
	if (FALSE == result)
	{
		g_error_free(error);

		net_nfc_server_llcp_deinit();
	}

	return result;
}

void net_nfc_server_llcp_deinit(void)
{
	if (llcp_skeleton)
	{
		g_object_unref(llcp_skeleton);
		llcp_skeleton = NULL;
	}
}

guint16 net_nfc_server_llcp_get_miu(void)
{
	return llcp_config.miu;
}

guint16 net_nfc_server_llcp_get_wks(void)
{
	return llcp_config.wks;
}

guint8 net_nfc_server_llcp_get_lto(void)
{
	return llcp_config.lto;
}

guint8 net_nfc_server_llcp_get_option(void)
{
	return llcp_config.option;
}

net_nfc_error_e net_nfc_server_llcp_simple_server(net_nfc_target_handle_s *handle,
		const char *san,
		sap_t sap,
		net_nfc_server_llcp_callback callback,
		net_nfc_server_llcp_callback error_callback,
		gpointer user_data)
{
	bool ret;
	net_nfc_llcp_socket_t socket = -1;
	net_nfc_llcp_config_info_s config;
	net_nfc_error_e result = NET_NFC_OK;
	LlcpSimpleData *simple_data = NULL;

	if (NULL == handle)
		return NET_NFC_NULL_PARAMETER;

	if (net_nfc_controller_llcp_get_remote_config(handle, &config, &result) == false)
	{
		NFC_ERR("net_nfc_controller_llcp_get_remote_config failed [%d]", result);

		goto ERROR;
	}

	simple_data = g_try_new0(LlcpSimpleData, 1);
	if (NULL == simple_data)
	{
		NFC_ERR("g_try_new0 failed");

		result = NET_NFC_ALLOC_FAIL;

		goto ERROR;
	}

	simple_data->handle = handle;
	simple_data->callback = callback;
	simple_data->error_callback = error_callback;
	simple_data->user_data = user_data;

	simple_data->miu = MIN(config.miu, net_nfc_server_llcp_get_miu());

	ret = net_nfc_controller_llcp_create_socket(&socket,
				NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONORIENTED,
				simple_data->miu,
				1,
				&result,
				llcp_simple_socket_error_cb,
				simple_data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_create_socket failed [%d]", result);

		goto ERROR;
	}

	simple_data->socket = socket;

	if (net_nfc_controller_llcp_bind(socket, sap, &result) == false)
	{
		NFC_ERR("net_nfc_controller_llcp_bind failed [%d]", result);

		goto ERROR;
	}

	ret = net_nfc_controller_llcp_listen(handle, (uint8_t *)san, socket, &result,
				llcp_simple_listen_cb, simple_data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_listen failed [%d]", result);

		goto ERROR;
	}

	NFC_DBG("result [%d]", result);

	if (result == NET_NFC_BUSY)
		result = NET_NFC_OK;

	return result;

ERROR :
	if (socket != -1)
	{
		net_nfc_error_e temp;

		net_nfc_controller_llcp_socket_close(socket, &temp);
	}

	if (simple_data != NULL)
		g_free(simple_data);

	return result;
}

net_nfc_error_e net_nfc_server_llcp_simple_client(
		net_nfc_target_handle_s *handle,
		const char *san,
		sap_t sap,
		net_nfc_server_llcp_callback callback,
		net_nfc_server_llcp_callback error_callback,
		gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	net_nfc_llcp_socket_t socket = -1;
	net_nfc_llcp_config_info_s config;
	LlcpSimpleData *simple_data = NULL;

	if (NULL == handle)
		return NET_NFC_NULL_PARAMETER;

	if (net_nfc_controller_llcp_get_remote_config(handle, &config, &result) == false)
	{
		NFC_ERR("net_nfc_controller_llcp_get_remote_config failed [%d]", result);

		goto ERROR;
	}

	simple_data = g_try_new0(LlcpSimpleData, 1);
	if (NULL == simple_data)
	{
		NFC_ERR("g_try_new0 failed");

		result = NET_NFC_ALLOC_FAIL;

		goto ERROR;
	}

	simple_data->handle = handle;
	simple_data->callback = callback;
	simple_data->error_callback = error_callback;
	simple_data->user_data = user_data;

	simple_data->miu = MIN(config.miu, net_nfc_server_llcp_get_miu());

	ret = net_nfc_controller_llcp_create_socket(&socket,
				NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONORIENTED,
				simple_data->miu,
				1,
				&result,
				llcp_simple_socket_error_cb,
				simple_data);

	if (false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_create_socket failed [%d]", result);

		goto ERROR;
	}

	simple_data->socket = socket;

	if (NULL == san)
	{
		ret = net_nfc_controller_llcp_connect(handle, simple_data->socket, sap,
					&result, llcp_simple_connect_cb, simple_data);
		if (false == ret)
		{
			NFC_ERR("net_nfc_controller_llcp_connect failed [%d]", result);

			goto ERROR;
		}
	}
	else
	{
		ret = net_nfc_controller_llcp_connect_by_url(handle, simple_data->socket,
					(uint8_t *)san, &result, llcp_simple_connect_cb, simple_data);
		if (false == ret)
		{
			NFC_ERR("net_nfc_controller_llcp_connect_by_url failed [%d]", result);

			goto ERROR;
		}
	}

	NFC_DBG("result [%d]", result);

	if (result == NET_NFC_BUSY)
		result = NET_NFC_OK;

	return result;

ERROR :
	if (socket != -1)
	{
		net_nfc_error_e temp;

		net_nfc_controller_llcp_socket_close(socket, &temp);
	}

	if (simple_data != NULL)
		g_free(simple_data);

	return result;
}

net_nfc_error_e net_nfc_server_llcp_simple_accept(
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		net_nfc_server_llcp_callback error_callback,
		gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	LlcpSimpleData *simple_data;

	simple_data = g_try_new0(LlcpSimpleData, 1);
	if (simple_data != NULL)
	{
		simple_data->handle = handle;
		simple_data->socket = socket;
		simple_data->error_callback = error_callback;
		simple_data->user_data = user_data;

		ret = net_nfc_controller_llcp_accept(socket, &result,
					llcp_simple_socket_error_cb, simple_data);
		if (false == ret)
			NFC_ERR("net_nfc_controller_llcp_accept failed [%d]", result);
	}
	else
	{
		NFC_ERR("g_try_new0 failed");

		result = NET_NFC_ALLOC_FAIL;
	}

	if (result == NET_NFC_BUSY)
		result = NET_NFC_OK;

	return result;
}

net_nfc_error_e net_nfc_server_llcp_simple_send(
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		net_nfc_server_llcp_callback callback,
		gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	LlcpSimpleData *simple_data;

	simple_data = g_try_new0(LlcpSimpleData, 1);
	if (simple_data != NULL)
	{
		simple_data->handle = handle;
		simple_data->socket = socket;
		simple_data->callback = callback;
		simple_data->user_data = user_data;

		ret = net_nfc_controller_llcp_send(handle, socket, data, &result,
					llcp_simple_send_cb, simple_data);

		if (false == ret)
			NFC_ERR("net_nfc_controller_llcp_send failed [%d]", result);
	}
	else
	{
		NFC_ERR("g_try_new0 failed");

		result = NET_NFC_ALLOC_FAIL;
	}

	if (NET_NFC_BUSY == result)
		result = NET_NFC_OK;

	return result;
}

net_nfc_error_e net_nfc_server_llcp_simple_receive(
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		net_nfc_server_llcp_callback callback,
		gpointer user_data)
{
	bool ret;
	net_nfc_error_e result;
	LlcpSimpleData *simple_data;

	simple_data = g_try_new0(LlcpSimpleData, 1);
	if (simple_data != NULL)
	{
		simple_data->handle = handle;
		simple_data->socket = socket;
		simple_data->callback = callback;
		simple_data->user_data = user_data;

		ret = net_nfc_controller_llcp_recv(handle, socket, net_nfc_server_llcp_get_miu(),
					&result, llcp_simple_receive_cb, simple_data);
		if (false == ret)
		{
			NFC_ERR("net_nfc_controller_llcp_recv failed [%d]", result);

			g_free(simple_data);
		}
	}
	else
	{
		NFC_ERR("g_try_new0 failed");

		result = NET_NFC_ALLOC_FAIL;
	}

	if (NET_NFC_BUSY == result)
		result = NET_NFC_OK;

	return result;
}

typedef struct _service_t
{
	uint32_t sap;
	char *san;
	char *id;
	net_nfc_server_llcp_activate_cb cb;
	void *user_data;
}
service_t;

static GHashTable *service_table;

static void _llcp_init()
{
	if (NULL == service_table)
		service_table = g_hash_table_new(NULL, NULL);
}

inline static service_t *_llcp_find_service(uint32_t sap)
{
	return (service_t *)g_hash_table_lookup(service_table, (gconstpointer)sap);
}

static net_nfc_error_e _llcp_add_service(const char *id, uint32_t sap,
		const char *san, net_nfc_server_llcp_activate_cb cb, void *user_data)
{
	net_nfc_error_e result;
	service_t *service = NULL;

	RETV_IF(NULL == cb, NET_NFC_NULL_PARAMETER);

	_llcp_init();

	if (_llcp_find_service(sap) == NULL)
	{
		NFC_DBG("new service, sap [%d]", sap);

		service = g_try_new0(service_t, 1);
		if (service != NULL)
		{
			service->sap = sap;
			if (san != NULL && strlen(san) > 0)
				service->san = g_strdup(san);

			if (id != NULL && strlen(id) > 0)
				service->id = g_strdup(id);

			service->cb = cb;
			service->user_data = user_data;

			g_hash_table_insert(service_table, (gpointer)sap, (gpointer)service);

			result = NET_NFC_OK;
		}
		else
		{
			NFC_ERR("alloc failed");

			result = NET_NFC_ALLOC_FAIL;
		}
	} else {
		NFC_ERR("already registered");

		result = NET_NFC_ALREADY_REGISTERED;
	}

	return result;
}

static void _llcp_remove_service(uint32_t sap)
{
	service_t *service = NULL;

	service = _llcp_find_service(sap);
	if (service != NULL)
	{
		g_free(service->san);
		g_free(service->id);
		g_free(service);

		g_hash_table_remove(service_table, (gconstpointer)sap);
	}
}

static void _llcp_remove_services(const char *id)
{
	gpointer key;
	service_t *service;
	GHashTableIter iter;

	if (NULL == service_table)
		return;

	g_hash_table_iter_init (&iter, service_table);

	while (g_hash_table_iter_next (&iter, &key, (gpointer)&service))
	{
		if (id == NULL || strcmp(service->id, id) == 0)
		{
			g_free(service->san);
			g_free(service->id);
			g_free(service);

			g_hash_table_iter_remove(&iter);
		}
	}
}

static void _llcp_start_services_cb(gpointer key, gpointer value,
		gpointer user_data)
{
	service_t *service = (service_t *)value;

	/* TODO : start service */
	if (service != NULL && service->cb != NULL)
	{
		service->cb(NET_NFC_LLCP_START, (net_nfc_target_handle_s*)user_data, service->sap,
				service->san, service->user_data);
	}
}

static void _llcp_start_services(net_nfc_target_handle_s *handle)
{
	g_hash_table_foreach(service_table, _llcp_start_services_cb, handle);
}

net_nfc_error_e net_nfc_server_llcp_register_service(const char *id, sap_t sap,
		const char *san, net_nfc_server_llcp_activate_cb cb, void *user_param)
{
	return _llcp_add_service(id, sap, san, cb, user_param);
}

net_nfc_error_e net_nfc_server_llcp_unregister_service(const char *id,
		sap_t sap, const char *san)
{
	service_t *service;
	net_nfc_error_e result;

	service = _llcp_find_service(sap);
	if (service != NULL)
	{
		/* invoke callback */
		service->cb(NET_NFC_LLCP_UNREGISTERED, NULL, service->sap,
				service->san, service->user_data);

		_llcp_remove_service(sap);

		result = NET_NFC_OK;
	}
	else
	{
		NFC_ERR("service is not registered");

		result = NET_NFC_NOT_REGISTERED;
	}

	return result;
}

net_nfc_error_e net_nfc_server_llcp_unregister_services(const char *id)
{
	_llcp_remove_services(id);

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_server_llcp_unregister_all()
{
	gpointer key;
	service_t *service;
	GHashTableIter iter;

	if (NULL == service_table)
		return NET_NFC_OK;

	g_hash_table_iter_init(&iter, service_table);

	while (g_hash_table_iter_next(&iter, &key, (gpointer)&service))
	{
		service->cb(NET_NFC_LLCP_UNREGISTERED, NULL, service->sap,
				service->san, service->user_data);

		g_free(service->san);
		g_free(service->id);
		g_free(service);

		g_hash_table_iter_remove(&iter);
	}

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_server_llcp_start_registered_services(
		net_nfc_target_handle_s *handle)
{
	_llcp_start_services(handle);

	return NET_NFC_OK;
}

static void net_nfc_server_llcp_process(gpointer user_data)
{
	bool ret;
	net_nfc_target_handle_s *handle;
	net_nfc_current_target_info_s *target;
#if 0
	net_nfc_error_e result;
	net_nfc_target_type_e dev_type;
#endif

	target = net_nfc_server_get_target_info();

	g_assert(target != NULL); /* raise exception!!! what;s wrong?? */

	handle = target->handle;

	NFC_DBG("connection type = [%d]", handle->connection_type);
#if 0
	dev_type = target->devType;

	if (NET_NFC_NFCIP1_TARGET == dev_type)
	{
		NFC_DBG("LLCP : target, try to connect");

		if (net_nfc_controller_connect(handle, &result) == false)
		{
			NFC_ERR("net_nfc_controller_connect is failed, [%d]", result);

			ret = net_nfc_controller_configure_discovery(NET_NFC_DISCOVERY_MODE_RESUME,
						NET_NFC_ALL_ENABLE, &result);
			if (false == ret)
			{
				NFC_ERR("net_nfc_controller_configure_discovery is failed [%d]", result);
				net_nfc_controller_exception_handler();
			}

			return;
		}
	}

	NFC_DBG("check LLCP");

	if (net_nfc_controller_llcp_check_llcp(handle, &result) == false)
	{
		NFC_ERR("net_nfc_controller_llcp_check_llcp is failed [%d]", result);
		return;
	}

	NFC_DBG("activate LLCP");

	if (net_nfc_controller_llcp_activate_llcp(handle, &result) == false)
	{
		NFC_ERR("%s is failed [%d]", "net_nfc_controller_llcp_activate_llcp", result);

		return;
	}
#endif
	net_nfc_server_llcp_start_registered_services(handle);

	net_nfc_server_p2p_discovered(handle);
}

void net_nfc_server_llcp_target_detected(void *info)
{
	gboolean ret;

	ret = net_nfc_server_controller_async_queue_push(
				net_nfc_server_llcp_process, NULL);

	if (FALSE == ret)
		NFC_ERR("can not push to controller thread");

}
