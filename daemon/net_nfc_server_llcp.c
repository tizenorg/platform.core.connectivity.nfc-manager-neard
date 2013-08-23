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
#include <pmapi.h>/*for pm lock*/

#include "net_nfc_gdbus.h"
#include "net_nfc_server_common.h"
#include "net_nfc_server_p2p.h"
#include "net_nfc_server_llcp.h"

#include "net_nfc_server_process_snep.h"
#include "net_nfc_server_process_npp.h"
#include "net_nfc_server_process_handover.h"
#include "net_nfc_server_tag.h"

#include "net_nfc_debug_internal.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_server_context.h"

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
	guint32 oal_socket;
	guint16 miu;
	guint8 rw;
	guint type;
	guint sap;
	gchar *service_name;
};

typedef struct _LlcpConnectData LlcpConnectData;

struct _LlcpConnectData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
	guint32 oal_socket;
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
	guint32 oal_socket;
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
	guint32 oal_socket;

	data_s *data;
};

typedef struct _LlcpSendToData LlcpSendToData;

struct _LlcpSendToData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
	guint32 oal_socket;
	guint8 sap;

	data_s *data;
};

typedef struct _LlcpReceiveData LlcpReceiveData;

struct _LlcpReceiveData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
	guint32 oal_socket;
	guint32 req_length;
};

typedef struct _LlcpCloseData LlcpCloseData;

struct _LlcpCloseData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
	guint32 oal_socket;
};

typedef struct _LlcpDisconnectData LlcpDisconnectData;

struct _LlcpDisconnectData
{
	NetNfcGDbusLlcp *llcp;
	GDBusMethodInvocation *invocation;

	guint32 handle;
	guint32 client_socket;
	guint32 oal_socket;
};

/* server_side */
typedef struct _ServerLlcpData ServerLlcpData;

struct _ServerLlcpData
{
	NetNfcGDbusLlcp *llcp;
	net_nfc_request_msg_t *req_msg;
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


static void llcp_add_async_queue(net_nfc_request_msg_t *req_msg,
		net_nfc_server_controller_func func);

static void llcp_socket_error_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_listen_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_connect_by_url_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_connect_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_send_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_receive_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_receive_from_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_disconnect_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

/* client method */
static void llcp_handle_config_thread_func(gpointer user_data);

static void llcp_handle_listen_thread_func(gpointer user_data);

static void llcp_handle_connect_thread_func(gpointer user_data);

static void llcp_handle_connect_sap_thread_func(gpointer user_data);

static void llcp_handle_send_thread_func(gpointer user_data);

static void llcp_handle_send_to_thread_func(gpointer user_data);

static void llcp_handle_receive_thread_func(gpointer user_data);

static void llcp_handle_receive_from_thread_func(gpointer user_data);

static void llcp_handle_close_thread_func(gpointer user_data);

static void llcp_handle_disconnect_thread_func(gpointer user_data);

/* methods */
static gboolean llcp_handle_config(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		GVariant *arg_config,
		GVariant *smack_privilege,
		gpointer user_data);

static gboolean llcp_handle_listen(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint16 arg_miu,
		guint8 arg_rw,
		gint32 arg_type,
		guint8 arg_sap,
		const gchar *arg_service_name,
		GVariant *smack_privilege,
		gpointer user_data);

static gboolean llcp_handle_connect(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint16 arg_miu,
		guint8 arg_rw,
		gint32 arg_type,
		const gchar *arg_service_name,
		GVariant *smack_privilege,
		gpointer user_data);

static gboolean llcp_handle_connect_sap(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint16 arg_miu,
		guint8 arg_rw,
		gint32 arg_type,
		guint8 arg_sap,
		GVariant *smack_privilege,
		gpointer user_data);

static gboolean llcp_handle_send(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		GVariant *arg_data,
		GVariant *smack_privilege,
		gpointer user_data);

static gboolean llcp_handle_send_to(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint8 arg_sap,
		GVariant *arg_data,
		GVariant *smack_privilege,
		gpointer user_data);

static gboolean llcp_handle_receive(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint32 arg_req_length,
		GVariant *smack_privilege,
		gpointer user_data);

static gboolean llcp_handle_receive_from(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint32 arg_req_length,
		GVariant *smack_privilege,
		gpointer user_data);

static gboolean llcp_handle_close(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		GVariant *smack_privilege,
		gpointer user_data);

static gboolean llcp_handle_disconnect(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		GVariant *smack_privilege,
		gpointer user_data);


/* server side */
static void llcp_deactivated_thread_func(gpointer user_data);

static void llcp_listen_thread_func(gpointer user_data);

static void llcp_socket_error_thread_func(gpointer user_data);

static void llcp_send_thread_func(gpointer user_data);

static void llcp_receive_thread_func(gpointer user_data);

static void llcp_receive_from_thread_func(gpointer user_data);

static void llcp_connect_thread_func(gpointer user_data);

static void llcp_disconnect_thread_func(gpointer user_data);

/* simple */
static void llcp_simple_socket_error_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_simple_listen_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_simple_connect_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_simple_server_error_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_simple_send_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void llcp_simple_receive_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);


static void llcp_add_async_queue(net_nfc_request_msg_t *req_msg,
		net_nfc_server_controller_func func)
{
	ServerLlcpData *data = NULL;

	if (llcp_skeleton == NULL)
	{
		DEBUG_ERR_MSG("%s is not initialized",
				"net_nfc_server_llcp");
		return;
	}

	if (req_msg == NULL)
	{
		DEBUG_ERR_MSG("req msg is null");
	}

	data = g_new0(ServerLlcpData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		return;
	}
	data->llcp = g_object_ref(llcp_skeleton);
	data->req_msg = req_msg;

	if (net_nfc_server_controller_async_queue_push(func, data) == FALSE)
	{
		DEBUG_ERR_MSG("can not push to controller thread");

		if (data)
		{
			g_object_unref(data->llcp);
			g_free(data->req_msg);
			g_free(data);
		}
	}
}

static void llcp_socket_error_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpData *llcp_data;

	llcp_data = (LlcpData *)user_param;

	if (llcp_data->invocation)
	{
		g_dbus_method_invocation_return_dbus_error(
				llcp_data->invocation,
				"org.tizen.NetNfcService.SocketError",
				"socket error");

		g_object_unref(llcp_data->invocation);
	}

	if (llcp_data->llcp)
		g_object_unref(llcp_data->llcp);

	g_free(llcp_data);
}

static void llcp_listen_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpListenData *llcp_data;

	llcp_data = (LlcpListenData *)user_param;

	if (llcp_data == NULL)
		return;

	if (llcp_data->llcp)
	{

		if (llcp_data->invocation)
		{
			net_nfc_gdbus_llcp_complete_listen(llcp_data->llcp,
					llcp_data->invocation,
					llcp_data->client_socket,
					socket);
			g_object_unref(llcp_data->invocation);
		}

		g_object_unref(llcp_data->llcp);
	}

	g_free(llcp_data->service_name);
	g_free(llcp_data);
}

static void llcp_connect_by_url_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpConnectData *llcp_data;

	llcp_data = (LlcpConnectData *)user_param;

	if (llcp_data == NULL)
		return;

	if (llcp_data->llcp)
	{

		if (llcp_data->invocation)
		{
			net_nfc_gdbus_llcp_complete_connect(llcp_data->llcp,
					llcp_data->invocation,
					llcp_data->client_socket,
					socket);
			g_object_unref(llcp_data->invocation);
		}

		g_object_unref(llcp_data->llcp);
	}

	g_free(llcp_data->service_name);
	g_free(llcp_data);
}

static void llcp_connect_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpConnectSapData *llcp_data;

	llcp_data = (LlcpConnectSapData *)user_param;

	if (llcp_data == NULL)
		return;

	if (llcp_data->llcp)
	{

		if (llcp_data->invocation)
		{
			net_nfc_gdbus_llcp_complete_connect_sap(
					llcp_data->llcp,
					llcp_data->invocation,
					llcp_data->client_socket,
					socket);
			g_object_unref(llcp_data->invocation);
		}

		g_object_unref(llcp_data->llcp);
	}

	g_free(llcp_data);
}

static void llcp_send_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpData *llcp_data;

	llcp_data = (LlcpData *)user_param;

	if (llcp_data == NULL)
		return;

	if (llcp_data->llcp)
	{

		if (llcp_data->invocation)
		{
			net_nfc_gdbus_llcp_complete_send(
					llcp_data->llcp,
					llcp_data->invocation,
					socket);
			g_object_unref(llcp_data->invocation);
		}

		g_object_unref(llcp_data->llcp);
	}

	g_free(llcp_data);
}

static void llcp_receive_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpData *llcp_data;

	llcp_data = (LlcpData *)user_param;

	if (llcp_data == NULL)
		return;

	if (llcp_data->llcp)
	{
		if (llcp_data->invocation)
		{
			GVariant *variant;

			variant = net_nfc_util_gdbus_data_to_variant(data);
			net_nfc_gdbus_llcp_complete_receive(
					llcp_data->llcp,
					llcp_data->invocation,
					variant);

			g_object_unref(llcp_data->invocation);
		}

		g_object_unref(llcp_data->llcp);
	}

	g_free(llcp_data);
}

static void llcp_receive_from_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpData *llcp_data;

	llcp_data = (LlcpData *)user_param;

	if (llcp_data == NULL)
		return;

	if (llcp_data->llcp)
	{

		if (llcp_data->invocation)
		{
			GVariant *variant;

			variant = net_nfc_util_gdbus_data_to_variant(data);
			net_nfc_gdbus_llcp_complete_receive_from(
					llcp_data->llcp,
					llcp_data->invocation,
					(guint8) (int)extra,
					variant);

			g_object_unref(llcp_data->invocation);
		}

		g_object_unref(llcp_data->llcp);
	}

	g_free(llcp_data);
}


static void llcp_disconnect_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpData *llcp_data;

	llcp_data = (LlcpData *)user_param;

	if (llcp_data == NULL)
		return;

	if (llcp_data->llcp)
	{

		if (llcp_data->invocation)
		{
			net_nfc_gdbus_llcp_complete_disconnect(
					llcp_data->llcp,
					llcp_data->invocation,
					socket);

			g_object_unref(llcp_data->invocation);
		}

		g_object_unref(llcp_data->llcp);
	}

	g_free(llcp_data);
}


static void llcp_handle_config_thread_func(gpointer user_data)
{
	LlcpConfigData *data;

	net_nfc_llcp_config_info_s config;

	data = (LlcpConfigData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpConfigData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ConfigError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}
		g_free(data);

		return;
	}

	config.miu = data->miu;
	config.wks = data->wks;
	config.lto = data->lto;
	config.option = data->option;

	if (net_nfc_server_llcp_set_config(&config) != NET_NFC_OK)
	{
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ConfigError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);
		g_free(data);
		return;
	}

	if (data->invocation)
	{
		net_nfc_gdbus_llcp_complete_config(data->llcp,
				data->invocation);

		g_object_unref(data->invocation);
	}

	g_object_unref(data->llcp);
	g_free(data);

	return;
}

static void llcp_handle_listen_thread_func(gpointer user_data)
{
	LlcpListenData *data;
	LlcpData *error_data;

	net_nfc_llcp_socket_t socket = -1;
	net_nfc_error_e result = NET_NFC_OK;

	data = (LlcpListenData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpListenData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ListenError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}

		g_free(data->service_name);
		g_free(data);

		return;
	}

	error_data = g_new0(LlcpData, 1);
	if(error_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(data->invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return;
	}
	error_data->llcp = g_object_ref(data->llcp);
	error_data->invocation = g_object_ref(data->invocation);

	if (net_nfc_controller_llcp_create_socket(&socket,
				data->type,
				data->miu,
				data->rw,
				&result,
				llcp_socket_error_cb,
				error_data) == false)
	{
		DEBUG_ERR_MSG("%s fiailed [%d]",
				"net_nfc_controller_llcp_create_socket" ,result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ListenError",
					"can not create socket");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);

		g_free(data->service_name);
		g_free(data);

		g_free(error_data);

		return;
	}

	if (net_nfc_controller_llcp_bind(socket,
				data->sap,
				&result) == false)
	{
		DEBUG_ERR_MSG("%s fiailed [%d]",
				"net_nfc_controller_llcp_create_socket" ,result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ListenError",
					"can not bind socket");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);

		g_free(data->service_name);
		g_free(data);

		g_free(error_data);

		if (socket != -1)
			net_nfc_controller_llcp_socket_close(socket, &result);

		return;
	}

	DEBUG_SERVER_MSG("OAL socket in Listen : %d\n", socket);

	if (net_nfc_controller_llcp_listen(GUINT_TO_POINTER(data->handle),
				(uint8_t *)data->service_name,
				socket,
				&result,
				llcp_listen_cb,
				data) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_controller_llcp_listen",
				result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ListenError",
					"can not listen socket");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);

		g_free(data->service_name);
		g_free(data);

		g_free(error_data);

		if (socket != -1)
			net_nfc_controller_llcp_socket_close(socket, &result);
	}
}

static void llcp_handle_connect_thread_func(gpointer user_data)
{
	LlcpConnectData *data;
	LlcpData *error_data;

	net_nfc_llcp_socket_t socket = -1;
	net_nfc_error_e result = NET_NFC_OK;

	data = (LlcpConnectData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpListenData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ConnectError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}

		g_free(data->service_name);
		g_free(data);

		return;
	}

	error_data = g_new0(LlcpData, 1);
	if(error_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(data->invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return;
	}
	error_data->llcp = g_object_ref(data->llcp);
	error_data->invocation = g_object_ref(data->invocation);

	if (net_nfc_controller_llcp_create_socket(&socket,
				data->type,
				data->miu,
				data->rw,
				&result,
				llcp_socket_error_cb,
				error_data) == false)
	{
		DEBUG_ERR_MSG("%s fiailed [%d]",
				"net_nfc_controller_llcp_create_socket" ,result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ConnectError",
					"can not create socket");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);

		g_free(data->service_name);
		g_free(data);

		g_free(error_data);

		return;
	}

	DEBUG_SERVER_MSG("OAL socket in Listen :%d", socket);

	if (net_nfc_controller_llcp_connect_by_url(
				GUINT_TO_POINTER(data->handle),
				socket,
				(uint8_t *)data->service_name,
				&result,
				llcp_connect_by_url_cb,
				data) == false)
	{
		DEBUG_ERR_MSG("%s failed, [%d]",
				"net_nfc_controller_llcp_connect_by_url",
				result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ConnectError",
					"can not listen socket");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);

		g_free(data->service_name);
		g_free(data);

		g_free(error_data);

		if (socket != -1)
			net_nfc_controller_llcp_socket_close(socket, &result);
	}
}

static void llcp_handle_connect_sap_thread_func(gpointer user_data)
{
	LlcpConnectSapData *data;
	LlcpData *error_data;

	net_nfc_llcp_socket_t socket = -1;
	net_nfc_error_e result = NET_NFC_OK;

	data = (LlcpConnectSapData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpConnectSapData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ConnectSapError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}

		g_free(data);

		return;
	}

	error_data = g_new0(LlcpData, 1);
	if(error_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(data->invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return;
	}
	error_data->llcp = g_object_ref(data->llcp);
	error_data->invocation = g_object_ref(data->invocation);

	if (net_nfc_controller_llcp_create_socket(&socket,
				data->type,
				data->miu,
				data->rw,
				&result,
				llcp_socket_error_cb,
				error_data) == false)
	{
		DEBUG_ERR_MSG("%s fiailed [%d]",
				"net_nfc_controller_llcp_create_socket" ,result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ConnectSapError",
					"can not create socket");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);

		g_free(data);

		g_free(error_data);

		return;
	}

	DEBUG_SERVER_MSG("OAL socket in Listen :%d", socket);

	if (net_nfc_controller_llcp_connect(GUINT_TO_POINTER(data->handle),
				socket,
				data->sap,
				&result,
				llcp_connect_cb,
				data) == false)
	{
		DEBUG_ERR_MSG("%s failed, [%d]",
				"net_nfc_controller_llcp_connect",
				result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ConnectSapError",
					"can not connect socket");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);

		g_free(data);

		g_free(error_data);

		if (socket != -1)
			net_nfc_controller_llcp_socket_close(socket, &result);

	}
}

static void llcp_handle_send_thread_func(gpointer user_data)
{
	LlcpSendData *data;
	LlcpData *llcp_data;

	net_nfc_error_e result = NET_NFC_OK;

	data = (LlcpSendData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpSendData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.SendError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}

		if (data->data)
		{
			g_free(data->data->buffer);
			g_free(data->data);
		}

		g_free(data);

		return;
	}

	llcp_data = g_new0(LlcpData, 1);
	if(llcp_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(data->invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return;
	}
	llcp_data->llcp = g_object_ref(data->llcp);
	llcp_data->invocation = g_object_ref(data->invocation);

	if (net_nfc_controller_llcp_send(GUINT_TO_POINTER(data->handle),
				data->oal_socket,
				data->data,
				&result,
				llcp_send_cb,
				llcp_data) == false)
	{
		DEBUG_ERR_MSG("%s failed, [%d]",
				"net_nfc_controller_llcp_send",
				result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.SendError",
					"can not send socket");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);

		if (data->data)
		{
			g_free(data->data->buffer);
			g_free(data->data);
		}

		g_free(data);

		g_free(llcp_data);

		return;
	}

	g_object_unref(data->llcp);
	g_object_unref(data->invocation);

	if (data->data)
	{
		g_free(data->data->buffer);
		g_free(data->data);
	}

	g_free(data);

	g_free(llcp_data);
}

static void llcp_handle_send_to_thread_func(gpointer user_data)
{
	LlcpSendToData *data;
	LlcpData *llcp_data;

	net_nfc_error_e result = NET_NFC_OK;

	data = (LlcpSendToData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpSendToData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.SendToError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}

		if (data->data)
		{
			g_free(data->data->buffer);
			g_free(data->data);
		}

		g_free(data);

		return;
	}

	llcp_data = g_new0(LlcpData, 1);
	if(llcp_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(data->invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return;
	}
	llcp_data->llcp = g_object_ref(data->llcp);
	llcp_data->invocation = g_object_ref(data->invocation);

	if (net_nfc_controller_llcp_send_to(GUINT_TO_POINTER(data->handle),
				data->oal_socket,
				data->data,
				data->sap,
				&result,
				llcp_send_cb,
				llcp_data) == false)
	{
		DEBUG_ERR_MSG("%s failed, [%d]",
				"net_nfc_controller_llcp_send_to",
				result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.SendToError",
					"can not send socket");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);

		if (data->data)
		{
			g_free(data->data->buffer);
			g_free(data->data);
		}

		g_free(data);

		g_free(llcp_data);

		return;
	}

	g_object_unref(data->llcp);
	g_object_unref(data->invocation);

	if (data->data)
	{
		g_free(data->data->buffer);
		g_free(data->data);
	}

	g_free(data);

	g_free(llcp_data);
}

static void llcp_handle_receive_thread_func(gpointer user_data)
{
	LlcpReceiveData *data;
	LlcpData *llcp_data;

	net_nfc_error_e result = NET_NFC_OK;

	data = (LlcpReceiveData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpReceiveData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ReceiveError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}

		g_free(data);

		return;
	}

	llcp_data = g_new0(LlcpData, 1);
	if(llcp_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(data->invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return;
	}
	llcp_data->llcp = g_object_ref(data->llcp);
	llcp_data->invocation = g_object_ref(data->invocation);

	if (net_nfc_controller_llcp_recv(GUINT_TO_POINTER(data->handle),
				data->oal_socket,
				data->req_length,
				&result,
				llcp_receive_cb,
				llcp_data) == false)
	{
		DEBUG_ERR_MSG("%s failed, [%d]",
				"net_nfc_controller_llcp_receive",
				result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ReceiveError",
					"can not receive");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);
		g_free(data);

		g_free(llcp_data);

		return;
	}

	g_object_unref(data->llcp);
	g_object_unref(data->invocation);

	g_free(data);

	g_free(llcp_data);
}

static void llcp_handle_receive_from_thread_func(gpointer user_data)
{
	LlcpReceiveData *data;
	LlcpData *llcp_data;

	net_nfc_error_e result = NET_NFC_OK;

	data = (LlcpReceiveData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpReceiveData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ReceiveFromError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}

		g_free(data);

		return;
	}

	llcp_data = g_new0(LlcpData, 1);
	if(llcp_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(data->invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return;
	}
	llcp_data->llcp = g_object_ref(data->llcp);
	llcp_data->invocation = g_object_ref(data->invocation);

	if (net_nfc_controller_llcp_recv_from(GUINT_TO_POINTER(data->handle),
				data->oal_socket,
				data->req_length,
				&result,
				llcp_receive_from_cb,
				llcp_data) == false)
	{
		DEBUG_ERR_MSG("%s failed, [%d]",
				"net_nfc_controller_llcp_receive",
				result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.ReceiveFromError",
					"can not receive");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);
		g_free(data);

		g_free(llcp_data);

		return;
	}

	g_object_unref(data->llcp);
	g_object_unref(data->invocation);

	g_free(data);

	g_free(llcp_data);
}

static void llcp_handle_close_thread_func(gpointer user_data)
{
	LlcpCloseData *data;

	net_nfc_error_e result = NET_NFC_OK;

	data = (LlcpCloseData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpCloseData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.CloseError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}

		g_free(data);

		return;
	}

	net_nfc_controller_llcp_socket_close(data->oal_socket,
			&result);

	net_nfc_gdbus_llcp_complete_close(data->llcp,
			data->invocation,
			data->client_socket);
}

static void llcp_handle_disconnect_thread_func(gpointer user_data)
{
	LlcpDisconnectData *data;
	LlcpData *llcp_data;
	int ret_val = 0;

	net_nfc_error_e result = NET_NFC_OK;

	data = (LlcpDisconnectData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpDisconnectData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");
		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.DisconnectError",
					"can not get llcp");

			g_object_unref(data->invocation);
		}

		g_free(data);

		return;
	}

	if(GUINT_TO_POINTER(data->handle) == 0)
	{
		net_nfc_server_free_target_info();
		ret_val = pm_unlock_state(LCD_NORMAL, PM_RESET_TIMER);
		DEBUG_SERVER_MSG("net_nfc_controller_disconnect pm_unlock_state"
				"[%d]!!", ret_val);
	}

	llcp_data = g_new0(LlcpData, 1);
	if(llcp_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		return;
	}
	llcp_data->llcp = g_object_ref(data->llcp);
	llcp_data->invocation = g_object_ref(data->invocation);

	if (net_nfc_controller_llcp_disconnect(GUINT_TO_POINTER(data->handle),
				data->oal_socket,
				&result,
				llcp_disconnect_cb,
				llcp_data) == false)
	{
		DEBUG_ERR_MSG("%s failed, [%d]",
				"net_nfc_controller_llcp_receive",
				result);

		if (data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					data->invocation,
					"org.tizen.NetNfcService.DisconnectError",
					"can not disconnect socket");

			g_object_unref(data->invocation);
		}

		g_object_unref(data->llcp);
		g_free(data);

		g_free(llcp_data);

		return;
	}

	g_object_unref(data->llcp);
	g_object_unref(data->invocation);

	g_free(data);

	g_free(llcp_data);
}


static gboolean llcp_handle_config(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		GVariant *arg_config,
		GVariant *smack_privilege,
		gpointer user_data)
{
	LlcpConfigData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"w") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(LlcpConfigData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}
	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);

	g_variant_get(arg_config,
			"(qqyy)",
			&data->miu,
			&data->wks,
			&data->lto,
			&data->option);

	if (net_nfc_server_controller_async_queue_push(
				llcp_handle_config_thread_func,
				data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");
		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		g_free(data);

		return FALSE;
	}

	return TRUE;
}

static gboolean llcp_handle_listen(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint16 arg_miu,
		guint8 arg_rw,
		gint32 arg_type,
		guint8 arg_sap,
		const gchar *arg_service_name,
		GVariant *smack_privilege,
		gpointer user_data)
{
	LlcpListenData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"rw") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(LlcpListenData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}
	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->oal_socket = arg_oal_socket;
	data->miu = arg_miu;
	data->rw = arg_rw;
	data->type = arg_type;
	data->sap = arg_sap;
	data->service_name = g_strdup(arg_service_name);

	if (net_nfc_server_controller_async_queue_push(
				llcp_handle_listen_thread_func,
				data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		g_free(data->service_name);
		g_free(data);

		return FALSE;
	}

	return TRUE;
}

static gboolean llcp_handle_connect(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint16 arg_miu,
		guint8 arg_rw,
		gint32 arg_type,
		const gchar *arg_service_name,
		GVariant *smack_privilege,
		gpointer user_data)
{
	LlcpConnectData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"rw") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(LlcpConnectData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}
	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->oal_socket = arg_oal_socket;
	data->miu = arg_miu;
	data->rw = arg_rw;
	data->type = arg_type;
	data->service_name = g_strdup(arg_service_name);

	if (net_nfc_server_controller_async_queue_push(
				llcp_handle_connect_thread_func,
				data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		g_free(data->service_name);
		g_free(data);

		return FALSE;
	}

	return TRUE;
}

static gboolean llcp_handle_connect_sap(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint16 arg_miu,
		guint8 arg_rw,
		gint32 arg_type,
		guint8 arg_sap,
		GVariant *smack_privilege,
		gpointer user_data)
{
	LlcpConnectSapData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"rw") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(LlcpConnectSapData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}
	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->oal_socket = arg_oal_socket;
	data->miu = arg_miu;
	data->rw = arg_rw;
	data->type = arg_type;
	data->sap = arg_sap;

	if(net_nfc_server_controller_async_queue_push(
				llcp_handle_connect_sap_thread_func,
				data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		g_free(data);

		return FALSE;
	}

	return TRUE;
}

static gboolean llcp_handle_send(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		GVariant *arg_data,
		GVariant *smack_privilege,
		gpointer user_data)
{
	LlcpSendData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"w") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(LlcpSendData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}
	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->oal_socket = arg_oal_socket;

	data->data = net_nfc_util_gdbus_variant_to_data(arg_data);

	if (net_nfc_server_controller_async_queue_push(
				llcp_handle_send_thread_func,
				data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		if (data->data != NULL)
		{
			g_free(data->data->buffer);
			g_free(data->data);
		}

		g_free(data);

		return FALSE;
	}

	return TRUE;
}

static gboolean llcp_handle_send_to(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint8 arg_sap,
		GVariant *arg_data,
		GVariant *smack_privilege,
		gpointer user_data)
{
	LlcpSendToData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"rw") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(LlcpSendToData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}
	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->oal_socket = arg_oal_socket;
	data->sap = arg_sap;

	data->data = net_nfc_util_gdbus_variant_to_data(arg_data);

	if(net_nfc_server_controller_async_queue_push(
				llcp_handle_send_to_thread_func,
				data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		if (data->data != NULL)
		{
			g_free(data->data->buffer);
			g_free(data->data);
		}

		g_free(data);

		return FALSE;
	}

	return TRUE;
}

static gboolean llcp_handle_receive(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint32 arg_req_length,
		GVariant *smack_privilege,
		gpointer user_data)
{
	LlcpReceiveData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"w") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(LlcpReceiveData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}
	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->oal_socket = arg_oal_socket;
	data->req_length = arg_req_length;

	if(net_nfc_server_controller_async_queue_push(
				llcp_handle_receive_thread_func,
				data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		g_free(data);

		return FALSE;
	}

	return TRUE;
}

static gboolean llcp_handle_receive_from(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		guint32 arg_req_length,
		GVariant *smack_privilege,
		gpointer user_data)
{
	LlcpReceiveData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"w") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(LlcpReceiveData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}
	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->oal_socket = arg_oal_socket;
	data->req_length = arg_req_length;

	if(net_nfc_server_controller_async_queue_push(
				llcp_handle_receive_from_thread_func,
				data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		g_free(data);

		return FALSE;
	}

	return TRUE;
}

static gboolean llcp_handle_close(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		GVariant *smack_privilege,
		gpointer user_data)
{
	LlcpCloseData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"w") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(LlcpCloseData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}
	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->oal_socket = arg_oal_socket;

	if (net_nfc_server_controller_async_queue_push(
				llcp_handle_close_thread_func,
				data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		g_free(data);

		return FALSE;
	}

	return TRUE;
}

static gboolean llcp_handle_disconnect(NetNfcGDbusLlcp *llcp,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		guint32 arg_client_socket,
		guint32 arg_oal_socket,
		GVariant *smack_privilege,
		gpointer user_data)
{
	LlcpDisconnectData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"w") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(LlcpDisconnectData, 1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}
	data->llcp = g_object_ref(llcp);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	data->client_socket = arg_client_socket;
	data->oal_socket = arg_oal_socket;

	if (net_nfc_server_controller_async_queue_push(
				llcp_handle_disconnect_thread_func,
				data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->llcp);
		g_object_unref(data->invocation);

		g_free(data);

		return FALSE;
	}

	return TRUE;
}


static void llcp_deactivated_thread_func(gpointer user_data)
{
	ServerLlcpData *data;

	net_nfc_request_llcp_msg_t *req_llcp_msg;
	net_nfc_target_handle_s *handle;

	data = (ServerLlcpData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get ServerLlcpData");

		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");

		g_free(data->req_msg);
		g_free(data);

		return;
	}

	if (data->req_msg == NULL)
	{
		DEBUG_ERR_MSG("can not get request msg");

		g_object_unref(data->llcp);
		g_free(data);

		return;
	}

	req_llcp_msg = (net_nfc_request_llcp_msg_t *)data->req_msg;

	handle = (net_nfc_target_handle_s *)req_llcp_msg->user_param;
	if (handle == NULL)
	{
		DEBUG_SERVER_MSG(
				"the target ID = [0x%p] was not connected before."
				"current device may be a TARGET", handle);
	}
	else
	{
		net_nfc_error_e result = NET_NFC_OK;

		if (net_nfc_controller_disconnect(handle, &result) == false)
		{
			if (result != NET_NFC_NOT_CONNECTED)
			{
				net_nfc_controller_exception_handler();
			}
			else
			{
				DEBUG_SERVER_MSG("target was not connected.");
			}
		}

		net_nfc_server_set_state(NET_NFC_SERVER_IDLE);
	}

	/* send p2p detatch */
	net_nfc_server_p2p_detached();

	g_object_unref(data->llcp);

	g_free(data->req_msg);
	g_free(data);
}

static void llcp_listen_thread_func(gpointer user_data)
{
	ServerLlcpData *data;

	socket_info_t *info = NULL;
	net_nfc_request_listen_socket_t *listen_socket;

	data = (ServerLlcpData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get ServerLlcpData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");

		g_free(data->req_msg);
		g_free(data);

		return;
	}

	if (data->req_msg == NULL)
	{
		DEBUG_ERR_MSG("can not get request msg");

		g_object_unref(data->llcp);
		g_free(data);

		return;
	}

	listen_socket = (net_nfc_request_listen_socket_t *)data->req_msg;

	info = (socket_info_t *)listen_socket->user_param;
	info = _get_socket_info(info->socket);

	if (info != NULL)
	{
		if (_add_socket_info(listen_socket->client_socket) != NULL)
		{
			if (info->work_cb != NULL)
			{
				info->work_cb(listen_socket->client_socket,
						listen_socket->result, NULL, NULL,info->work_param);
			}
		}
		else
		{
			DEBUG_ERR_MSG("_net_nfc_util_alloc_mem failed");
		}
	}

	g_object_unref(data->llcp);

	g_free(data->req_msg);
	g_free(data);
}

static void llcp_socket_error_thread_func(gpointer user_data)
{
	ServerLlcpData *data;

	net_nfc_request_llcp_msg_t *req_llcp_msg;
	net_nfc_llcp_param_t *param;

	data = (ServerLlcpData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get ServerLlcpData");
		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");

		g_free(data->req_msg);
		g_free(data);

		return;
	}

	if (data->req_msg == NULL)
	{
		DEBUG_ERR_MSG("can not get request msg");

		g_object_unref(data->llcp);
		g_free(data);

		return;
	}

	req_llcp_msg = (net_nfc_request_llcp_msg_t *)data->req_msg;
	param = (net_nfc_llcp_param_t *)req_llcp_msg->user_param;

	if (param)
	{
		if (param->cb)
		{
			param->cb(req_llcp_msg->llcp_socket,
					req_llcp_msg->result,
					NULL,
					NULL,
					param->user_param);
		}

		g_free(param);
	}

	g_object_unref(data->llcp);

	g_free(data->req_msg);
	g_free(data);
}

static void llcp_send_thread_func(gpointer user_data)
{
	ServerLlcpData *data;

	net_nfc_request_llcp_msg_t *req_llcp_msg;
	net_nfc_llcp_param_t *param;

	data = (ServerLlcpData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get ServerLlcpData");

		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");

		g_free(data->req_msg);
		g_free(data);

		return;
	}

	if (data->req_msg == NULL)
	{
		DEBUG_ERR_MSG("can not get request msg");

		g_object_unref(data->llcp);
		g_free(data);

		return;
	}

	req_llcp_msg = (net_nfc_request_llcp_msg_t *)data->req_msg;
	param = (net_nfc_llcp_param_t *)req_llcp_msg->user_param;

	if (param)
	{
		if (param->cb)
		{
			param->cb(param->socket,
					req_llcp_msg->result,
					NULL,
					NULL,
					param->user_param);
		}

		g_free(param);
	}

	g_object_unref(data->llcp);

	g_free(data->req_msg);
	g_free(data);
}

static void llcp_receive_thread_func(gpointer user_data)
{
	ServerLlcpData *data;

	net_nfc_request_receive_socket_t *req_receive_socket;
	net_nfc_llcp_param_t *param;

	data = (ServerLlcpData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get ServerLlcpData");

		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");

		g_free(data->req_msg);
		g_free(data);

		return;
	}

	if (data->req_msg == NULL)
	{
		DEBUG_ERR_MSG("can not get request msg");

		g_object_unref(data->llcp);
		g_free(data);

		return;
	}

	req_receive_socket = (net_nfc_request_receive_socket_t *)data->req_msg;
	param = (net_nfc_llcp_param_t *)req_receive_socket->user_param;

	if (param)
	{
		if (param->cb)
		{
			param->cb(param->socket,
					req_receive_socket->result,
					&param->data,
					NULL,
					param->user_param);
		}

		if (param->data.buffer)
			g_free(param->data.buffer);

		g_free(param);
	}

	g_object_unref(data->llcp);

	g_free(data->req_msg);
	g_free(data);
}

static void llcp_receive_from_thread_func(gpointer user_data)
{
	ServerLlcpData *data;

	net_nfc_request_receive_from_socket_t *req_receive_from_socket;
	net_nfc_llcp_param_t *param;

	data = (ServerLlcpData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get ServerLlcpData");

		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");

		g_free(data->req_msg);
		g_free(data);

		return;
	}

	if (data->req_msg == NULL)
	{
		DEBUG_ERR_MSG("can not get request msg");

		g_object_unref(data->llcp);
		g_free(data);

		return;
	}

	req_receive_from_socket = (net_nfc_request_receive_from_socket_t *)
		data->req_msg;
	param = (net_nfc_llcp_param_t *)req_receive_from_socket->user_param;

	if (param)
	{
		if (param->cb)
		{
			param->cb(param->socket,
					req_receive_from_socket->result,
					&param->data,
					(void *)(int)req_receive_from_socket->sap,
					param->user_param);
		}

		if (param->data.buffer)
			g_free(param->data.buffer);

		g_free(param);
	}

	g_object_unref(data->llcp);

	g_free(data->req_msg);
	g_free(data);
}

static void llcp_connect_thread_func(gpointer user_data)
{
	ServerLlcpData *data;

	net_nfc_request_llcp_msg_t *req_llcp_msg;
	net_nfc_llcp_param_t *param;

	data = (ServerLlcpData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get ServerLlcpData");

		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");

		g_free(data->req_msg);
		g_free(data);

		return;
	}

	if (data->req_msg == NULL)
	{
		DEBUG_ERR_MSG("can not get request msg");

		g_object_unref(data->llcp);
		g_free(data);

		return;
	}

	req_llcp_msg = (net_nfc_request_llcp_msg_t *)data->req_msg;
	param = (net_nfc_llcp_param_t *)req_llcp_msg->user_param;

	if (param)
	{
		if (param->cb)
		{
			param->cb(param->socket,
					req_llcp_msg->result,
					NULL,
					NULL,
					param->user_param);
		}

		g_free(param);
	}

	g_object_unref(data->llcp);

	g_free(data->req_msg);
	g_free(data);
}

static void llcp_disconnect_thread_func(gpointer user_data)
{
	ServerLlcpData *data;

	net_nfc_request_llcp_msg_t *req_llcp_msg;
	net_nfc_llcp_param_t *param;

	data = (ServerLlcpData *)user_data;

	if (data == NULL)
	{
		DEBUG_ERR_MSG("can not get ServerLlcpData");

		return;
	}

	if (data->llcp == NULL)
	{
		DEBUG_ERR_MSG("can not get llcp");

		g_free(data->req_msg);
		g_free(data);

		return;
	}

	if (data->req_msg == NULL)
	{
		DEBUG_ERR_MSG("can not get request msg");

		g_object_unref(data->llcp);
		g_free(data);

		return;
	}

	req_llcp_msg = (net_nfc_request_llcp_msg_t *)data->req_msg;
	param = (net_nfc_llcp_param_t *)req_llcp_msg->user_param;

	if (param)
	{
		if (param->cb)
		{
			param->cb(param->socket,
					req_llcp_msg->result,
					NULL,
					NULL,
					param->user_param);
		}

		g_free(param);
	}

	g_object_unref(data->llcp);

	g_free(data->req_msg);
	g_free(data);
}

static void llcp_simple_socket_error_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpSimpleData *simple_data;

	simple_data = (LlcpSimpleData *)user_param;

	if (simple_data->error_callback)
	{
		simple_data->error_callback(result,
				simple_data->handle,
				socket,
				data,
				simple_data->user_data);
	}

	g_free(simple_data);
}

static void llcp_simple_listen_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpSimpleData *simple_data;

	simple_data = (LlcpSimpleData *)user_param;

	if (result != NET_NFC_OK) {
		DEBUG_ERR_MSG("listen socket failed, [%d]", result);
	}

	if (simple_data->callback)
	{
		simple_data->callback(result,
				simple_data->handle,
				socket,
				data,
				simple_data->user_data);
	}

	g_free(simple_data);
}

static void llcp_simple_connect_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpSimpleData *simple_data;

	simple_data = (LlcpSimpleData *)user_param;

	if (result != NET_NFC_OK) {
		DEBUG_ERR_MSG("connect socket failed, [%d]", result);
	}

	if (simple_data->callback)
	{
		simple_data->callback(result,
				simple_data->handle,
				socket,
				data,
				simple_data->user_data);
	}

	g_free(simple_data);
}

static void llcp_simple_server_error_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpSimpleData *simple_data;

	simple_data = (LlcpSimpleData *)user_param;

	if (simple_data->error_callback)
	{
		simple_data->error_callback(result,
				simple_data->handle,
				socket,
				data,
				simple_data->user_data);
	}

	g_free(simple_data);
}

static void llcp_simple_send_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpSimpleData *simple_data;

	simple_data = (LlcpSimpleData *)user_param;

	if (simple_data->callback)
	{
		simple_data->callback(result,
				simple_data->handle,
				socket,
				data,
				simple_data->user_data);
	}

	g_free(simple_data);
}

static void llcp_simple_receive_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	LlcpSimpleData *simple_data;

	simple_data = (LlcpSimpleData *)user_param;

	if (simple_data->callback)
	{
		simple_data->callback(result,
				simple_data->handle,
				socket,
				data,
				simple_data->user_data);
	}

	g_free(simple_data);
}

/* Public Function */
gboolean net_nfc_server_llcp_init(GDBusConnection *connection)
{
	GError *error = NULL;

	if (llcp_skeleton)
		g_object_unref(llcp_skeleton);

	llcp_skeleton = net_nfc_gdbus_llcp_skeleton_new();

	g_signal_connect(llcp_skeleton,
			"handle-config",
			G_CALLBACK(llcp_handle_config),
			NULL);

	g_signal_connect(llcp_skeleton,
			"handle-listen",
			G_CALLBACK(llcp_handle_listen),
			NULL);

	g_signal_connect(llcp_skeleton,
			"handle-connect",
			G_CALLBACK(llcp_handle_connect),
			NULL);

	g_signal_connect(llcp_skeleton,
			"handle-connect-sap",
			G_CALLBACK(llcp_handle_connect_sap),
			NULL);

	g_signal_connect(llcp_skeleton,
			"handle-send",
			G_CALLBACK(llcp_handle_send),
			NULL);

	g_signal_connect(llcp_skeleton,
			"handle-send-to",
			G_CALLBACK(llcp_handle_send_to),
			NULL);

	g_signal_connect(llcp_skeleton,
			"handle-receive",
			G_CALLBACK(llcp_handle_receive),
			NULL);

	g_signal_connect(llcp_skeleton,
			"handle-receive-from",
			G_CALLBACK(llcp_handle_receive_from),
			NULL);

	g_signal_connect(llcp_skeleton,
			"handle-close",
			G_CALLBACK(llcp_handle_close),
			NULL);

	g_signal_connect(llcp_skeleton,
			"handle-disconnect",
			G_CALLBACK(llcp_handle_disconnect),
			NULL);

	if(g_dbus_interface_skeleton_export(
				G_DBUS_INTERFACE_SKELETON(llcp_skeleton),
				connection,
				"/org/tizen/NetNfcService/Llcp",
				&error) == FALSE)
	{
		g_error_free(error);
		g_object_unref(llcp_skeleton);
		llcp_skeleton = NULL;

		return FALSE;
	}

	return TRUE;
}

void net_nfc_server_llcp_deinit(void)
{
	if (llcp_skeleton)
	{
		g_object_unref(llcp_skeleton);
		llcp_skeleton = NULL;
	}
}

void net_nfc_server_llcp_deactivated(net_nfc_request_msg_t *req_msg)
{
	llcp_add_async_queue(req_msg, llcp_deactivated_thread_func);
}

void net_nfc_server_llcp_listen(net_nfc_request_msg_t *req_msg)
{
	llcp_add_async_queue(req_msg, llcp_listen_thread_func);
}

void net_nfc_server_llcp_socket_error(net_nfc_request_msg_t *req_msg)
{
	llcp_add_async_queue(req_msg, llcp_socket_error_thread_func);
}

void net_nfc_server_llcp_send(net_nfc_request_msg_t *req_msg)
{
	llcp_add_async_queue(req_msg, llcp_send_thread_func);
}

void net_nfc_server_llcp_receive(net_nfc_request_msg_t *req_msg)
{
	llcp_add_async_queue(req_msg, llcp_receive_thread_func);
}

void net_nfc_server_llcp_receive_from(net_nfc_request_msg_t *req_msg)
{
	llcp_add_async_queue(req_msg, llcp_receive_from_thread_func);
}

void net_nfc_server_llcp_connect(net_nfc_request_msg_t *req_msg)
{
	llcp_add_async_queue(req_msg, llcp_connect_thread_func);
}

void net_nfc_server_llcp_disconnect(net_nfc_request_msg_t *req_msg)
{
	llcp_add_async_queue(req_msg, llcp_disconnect_thread_func);
}

net_nfc_error_e net_nfc_server_llcp_set_config(
		net_nfc_llcp_config_info_s *config)
{
	net_nfc_error_e result;

	if (config == NULL)
	{
		net_nfc_controller_llcp_config(&llcp_config, &result);
		return result;
	}

	net_nfc_controller_llcp_config(config, &result);
	memcpy(&llcp_config, config, sizeof(llcp_config));

	return result;
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

net_nfc_error_e net_nfc_server_llcp_simple_server(
		net_nfc_target_handle_s *handle,
		const char *san,
		sap_t sap,
		net_nfc_server_llcp_callback callback,
		net_nfc_server_llcp_callback error_callback,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;

	net_nfc_llcp_socket_t socket = -1;
	net_nfc_llcp_config_info_s config;

	LlcpSimpleData *simple_data = NULL;

	if (handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (san == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (net_nfc_controller_llcp_get_remote_config(handle,
				&config,
				&result) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_controller_llcp_get_remote_config",
				result);
		return result;
	}

	simple_data = g_new0(LlcpSimpleData, 1);
	if(simple_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");

		return NET_NFC_ALLOC_FAIL;
	}
	simple_data->handle = handle;
	simple_data->callback = callback;
	simple_data->error_callback = error_callback;
	simple_data->user_data = user_data;

	simple_data->miu = MIN(config.miu, net_nfc_server_llcp_get_miu());

	if (net_nfc_controller_llcp_create_socket(&socket,
				NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONORIENTED,
				simple_data->miu,
				1,
				&result,
				llcp_simple_socket_error_cb,
				simple_data) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_controller_llcp_create_socket",
				result);

		g_free(simple_data);
		return result;
	}

	simple_data->socket = socket;

	if (net_nfc_controller_llcp_bind(socket,
				sap,
				&result) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"nte_nfc_controller_llcp_bind",
				result);

		if (simple_data->socket != 1)
			net_nfc_controller_llcp_socket_close(socket, &result);

		g_free(simple_data);

		return result;
	}

	if (net_nfc_controller_llcp_listen(handle,
				(uint8_t *)san,
				socket,
				&result,
				llcp_simple_listen_cb,
				simple_data) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_controller_llcp_listen",
				result);

		if (simple_data->socket != 1)
		{
			net_nfc_controller_llcp_socket_close(
					simple_data->socket,
					&result);
		}

		g_free(simple_data);

		return result;

	}

	DEBUG_SERVER_MSG("result [%d]", result);

	if (result == NET_NFC_BUSY)
		result = NET_NFC_OK;

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
	net_nfc_error_e result = NET_NFC_OK;

	net_nfc_llcp_socket_t socket = -1;
	net_nfc_llcp_config_info_s config;

	LlcpSimpleData *simple_data = NULL;

	if (handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (net_nfc_controller_llcp_get_remote_config(handle,
				&config,
				&result) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_controller_llcp_get_remote_config",
				result);
		return result;
	}

	simple_data = g_new0(LlcpSimpleData, 1);

	simple_data->handle = handle;
	simple_data->callback = callback;
	simple_data->error_callback = error_callback;
	simple_data->user_data = user_data;

	simple_data->miu = MIN(config.miu, net_nfc_server_llcp_get_miu());

	if (net_nfc_controller_llcp_create_socket(&socket,
				NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONORIENTED,
				simple_data->miu,
				1,
				&result,
				llcp_simple_socket_error_cb,
				simple_data) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_controller_llcp_create_socket",
				result);

		g_free(simple_data);
		return result;
	}

	simple_data->socket = socket;

	if (san == NULL)
	{
		if (net_nfc_controller_llcp_connect(handle,
					simple_data->socket,
					sap,
					&result,
					llcp_simple_connect_cb,
					simple_data) == false)
		{
			DEBUG_ERR_MSG("%s failed [%d]",
					"net_nfc_controller_llcp_connect",
					result);

			if (simple_data->socket != -1)
			{
				net_nfc_controller_llcp_socket_close(
						simple_data->socket,
						&result);
			}

			g_free(simple_data);

			return result;
		}

	}
	else
	{
		if (net_nfc_controller_llcp_connect_by_url(handle,
					simple_data->socket,
					(uint8_t *)san,
					&result,
					llcp_simple_connect_cb,
					simple_data) == false)
		{
			DEBUG_ERR_MSG("%s failed [%d]",
					"net_nfc_controller_llcp_connect_by_url",
					result);

			if (simple_data->socket != -1)
			{
				net_nfc_controller_llcp_socket_close(
						simple_data->socket,
						&result);
			}

			g_free(simple_data);

			return result;
		}
	}

	DEBUG_SERVER_MSG("result [%d]", result);

	if (result == NET_NFC_BUSY)
		result = NET_NFC_OK;

	return result;
}

net_nfc_error_e net_nfc_server_llcp_simple_accept(
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		net_nfc_server_llcp_callback error_callback,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;

	LlcpSimpleData *simple_data;

	simple_data = g_new0(LlcpSimpleData, 1);
	if(simple_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");

		return NET_NFC_ALLOC_FAIL;
	}
	simple_data->handle = handle;
	simple_data->socket = socket;
	simple_data->error_callback = error_callback;
	simple_data->user_data = user_data;

	if (net_nfc_controller_llcp_accept(socket,
				&result,
				llcp_simple_server_error_cb,
				simple_data) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_controller_llcp_accept",
				result);
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
	net_nfc_error_e result = NET_NFC_OK;

	LlcpSimpleData *simple_data;

	simple_data = g_new0(LlcpSimpleData, 1);
	if(simple_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");

		return NET_NFC_ALLOC_FAIL;
	}
	simple_data->handle = handle;
	simple_data->socket = socket;
	simple_data->callback = callback;
	simple_data->user_data = user_data;

	if (net_nfc_controller_llcp_send(handle,
				socket,
				data,
				&result,
				llcp_simple_send_cb,
				simple_data) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_controller_llcp_send",
				result);
	}

	if (result == NET_NFC_BUSY)
		result = NET_NFC_OK;

	return result;
}

net_nfc_error_e net_nfc_server_llcp_simple_receive(
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		net_nfc_server_llcp_callback callback,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;

	LlcpSimpleData *simple_data;

	simple_data = g_new0(LlcpSimpleData, 1);
	if(simple_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");

		return NET_NFC_ALLOC_FAIL;
	}
	simple_data->handle = handle;
	simple_data->socket = socket;
	simple_data->callback = callback;
	simple_data->user_data = user_data;

	if (net_nfc_controller_llcp_recv(handle,
				socket,
				net_nfc_server_llcp_get_miu(),
				&result,
				llcp_simple_receive_cb,
				simple_data) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_controller_llcp_send",
				result);
	}

	if (result == NET_NFC_BUSY)
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
	if (service_table == NULL)
		service_table = g_hash_table_new(NULL, NULL);
}

inline static service_t *_llcp_find_service(uint32_t sap)
{
	return (service_t *)g_hash_table_lookup(service_table,
			(gconstpointer)sap);
}

static net_nfc_error_e _llcp_add_service(const char *id, uint32_t sap,
		const char *san, net_nfc_server_llcp_activate_cb cb, void *user_data)
{
	service_t *service = NULL;
	net_nfc_error_e result;

	if (cb == NULL) {
		DEBUG_ERR_MSG("callback is mandatory");

		return NET_NFC_NULL_PARAMETER;
	}

	_llcp_init();

	if (_llcp_find_service(sap) == NULL) {
		DEBUG_SERVER_MSG("new service, sap [%d]", sap);

		service = g_new0(service_t, 1);
		if (service != NULL) {
			service->sap = sap;
			if (san != NULL && strlen(san) > 0) {
				service->san = g_strdup(san);
			}
			if (id != NULL && strlen(id) > 0) {
				service->id = g_strdup(id);
			}
			service->cb = cb;
			service->user_data = user_data;

			g_hash_table_insert(service_table, (gpointer)sap,
					(gpointer)service);

			result = NET_NFC_OK;
		} else {
			DEBUG_ERR_MSG("alloc failed");

			result = NET_NFC_ALLOC_FAIL;
		}
	} else {
		DEBUG_ERR_MSG("already registered");

		result = NET_NFC_ALREADY_REGISTERED;
	}

	return result;
}

static void _llcp_remove_service(uint32_t sap)
{
	service_t *service = NULL;

	service = _llcp_find_service(sap);
	if (service != NULL) {
		g_free(service->san);
		g_free(service->id);
		g_free(service);

		g_hash_table_remove(service_table, (gconstpointer)sap);
	}
}

static void _llcp_remove_services(const char *id)
{
	GHashTableIter iter;
	gpointer key;
	service_t *service;

	if (service_table == NULL)
		return;

	g_hash_table_iter_init (&iter, service_table);

	while (g_hash_table_iter_next (&iter, &key, (gpointer)&service)) {
		if (id == NULL || strcmp(service->id, id) == 0) {
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
	if (service != NULL && service->cb != NULL) {
		service->cb(NET_NFC_LLCP_START,
				(net_nfc_target_handle_s *)user_data,
				service->sap,
				service->san, service->user_data);
	}
}

static void _llcp_start_services(net_nfc_target_handle_s *handle)
{
	g_hash_table_foreach(service_table, _llcp_start_services_cb, handle);
}

net_nfc_error_e net_nfc_server_llcp_register_service(const char *id,
		sap_t sap, const char *san, net_nfc_server_llcp_activate_cb cb,
		void *user_param)
{
	return _llcp_add_service(id, sap, san, cb, user_param);
}

net_nfc_error_e net_nfc_server_llcp_unregister_service(const char *id,
		sap_t sap, const char *san)
{
	net_nfc_error_e result;
	service_t *service;

	service = _llcp_find_service(sap);
	if (service != NULL) {
		/* invoke callback */
		service->cb(NET_NFC_LLCP_UNREGISTERED, NULL, service->sap,
				service->san, service->user_data);

		_llcp_remove_service(sap);

		result = NET_NFC_OK;
	} else {
		DEBUG_ERR_MSG("service is not registered");

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
	GHashTableIter iter;
	gpointer key;
	service_t *service;

	if (service_table == NULL)
		return NET_NFC_OK;

	g_hash_table_iter_init(&iter, service_table);

	while (g_hash_table_iter_next(&iter, &key, (gpointer)&service)) {
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
	net_nfc_current_target_info_s *target;
#if 0
	net_nfc_error_e result;
#endif
	net_nfc_target_handle_s *handle;
	net_nfc_target_type_e dev_type;

	target = net_nfc_server_get_target_info();

	g_assert(target != NULL); /* raise exception!!! what;s wrong?? */

	handle = target->handle;
	dev_type = target->devType;

	DEBUG_SERVER_MSG("connection type = [%d]", handle->connection_type);
#if 0
	if (dev_type == NET_NFC_NFCIP1_TARGET)
	{
		DEBUG_SERVER_MSG("LLCP : target, try to connect");

		if (net_nfc_controller_connect(handle, &result) == false)
		{
			DEBUG_SERVER_MSG("%s is failed, [%d]",
					"net_nfc_controller_connect",
					result);

			if (net_nfc_controller_configure_discovery(
						NET_NFC_DISCOVERY_MODE_RESUME,
						NET_NFC_ALL_ENABLE,
						&result) == false)
			{
				DEBUG_ERR_MSG("%s is failed [%d]",
						"net_nfc_controller_configure_discovery",
						result);

				net_nfc_controller_exception_handler();
			}

			return;
		}
	}

	DEBUG_SERVER_MSG("check LLCP");

	if (net_nfc_controller_llcp_check_llcp(handle, &result) == false)
	{
		DEBUG_ERR_MSG("%s is failed [%d]",
				"net_nfc_controller_llcp_check_llcp",
				result);

		return;
	}

	DEBUG_SERVER_MSG("activate LLCP");

	if (net_nfc_controller_llcp_activate_llcp(handle, &result) == false)
	{
		DEBUG_ERR_MSG("%s is failed [%d]",
				"net_nfc_controller_llcp_activate_llcp",
				result);

		return;
	}
#endif
	net_nfc_server_llcp_start_registered_services(handle);

	net_nfc_server_p2p_discovered(handle);
}

void net_nfc_server_llcp_target_detected(void *info)
{
	if (net_nfc_server_controller_async_queue_push(
				net_nfc_server_llcp_process, NULL) == FALSE)
	{
		DEBUG_ERR_MSG("can not push to controller thread");
	}
}
