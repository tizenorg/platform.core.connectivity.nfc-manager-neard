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
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_llcp.h"

typedef struct _LlcpFuncData LlcpFuncData;

struct _LlcpFuncData
{
	gpointer callback;
	gpointer user_data;
};

static NetNfcGDbusLlcp *llcp_proxy = NULL;
static net_nfc_llcp_config_info_s llcp_config = { 128, 0, 0, 1 };
static net_nfc_target_handle_s *llcp_handle = NULL;
static GList *socket_data_list = NULL;
static guint socket_handle = 0;

/* signal callback */
static void llcp_error(NetNfcGDbusLlcp *object, guint arg_handle, guint client_socket,
		gint error, gpointer user_data)
{
	NFC_INFO(">>> SIGNAL arrived");
}

static void llcp_incoming(NetNfcGDbusLlcp *object, guint arg_handle,
		guint client_socket, guint incoming_socket, gpointer user_data)
{
	NFC_INFO(">>> SIGNAL arrived");
}

void llcp_socket_data_append(net_nfc_llcp_internal_socket_s *socket_data)
{
	RET_IF(NULL == socket_data_list);

	if (socket_data)
		socket_data_list = g_list_append(socket_data_list, socket_data);
}

void llcp_socket_data_remove(net_nfc_llcp_internal_socket_s *socket_data)
{
	RET_IF(NULL == socket_data_list);

	if (socket_data)
	{
		socket_data_list = g_list_remove(socket_data_list, socket_data);

		g_free(socket_data->service_name);
		g_free(socket_data);
	}
}

net_nfc_llcp_internal_socket_s *llcp_socket_data_find(net_nfc_llcp_socket_t socket)
{
	GList *pos;

	RETV_IF(NULL == socket_data_list, NULL);

	for (pos = g_list_first(socket_data_list); pos ; pos = pos->data)
	{
		net_nfc_llcp_internal_socket_s *data;

		data = pos->data;
		if (data == NULL)
			continue;

		if (data->client_socket == socket)
			break;
	}

	if (pos == NULL)
		return NULL;

	return pos->data;
}

/* aysnc callback */
static void llcp_call_config(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	LlcpFuncData *func_data = user_data;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_config_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish config: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_llcp_config_completed callback =
			(net_nfc_client_llcp_config_completed)func_data->callback;

		callback(result, func_data->user_data);
	}

	g_free(func_data);
}

static void llcp_call_listen(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket = -1;
	LlcpFuncData *func_data = user_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_listen_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, &out_client_socket, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish listen: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	socket_data = llcp_socket_data_find(out_client_socket);
	if (socket_data == NULL)
	{
		NFC_ERR("Wrong client socket is returned");
		return;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_llcp_listen_completed callback =
			(net_nfc_client_llcp_listen_completed)func_data->callback;

		callback(result, out_client_socket, func_data->user_data);
	}

	/* TODO : release resource when socket is closed */
	//	g_free(func_data);
}

static void llcp_call_accept(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	LlcpFuncData *func_data = user_data;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_accept_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish accept: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_llcp_accept_completed callback =
			(net_nfc_client_llcp_accept_completed)func_data->callback;

		callback(result, func_data->user_data);
	}

	g_free(func_data);
}

static void llcp_call_reject(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	LlcpFuncData *func_data = user_data;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_reject_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish reject: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_llcp_reject_completed callback =
			(net_nfc_client_llcp_reject_completed)func_data->callback;

		callback(result, func_data->user_data);
	}

	g_free(func_data);
}

static void llcp_call_connect(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket = -1;
	LlcpFuncData *func_data = user_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_connect_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, &out_client_socket, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish connect: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	socket_data = llcp_socket_data_find(out_client_socket);
	if (socket_data == NULL)
	{
		NFC_ERR("Wrong client socket is returned");
		return;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_llcp_connect_completed callback =
			(net_nfc_client_llcp_connect_completed)func_data->callback;

		callback(result, out_client_socket, func_data->user_data);
	}

	/* TODO : release resource when socket is closed */
	//	g_free(func_data);
}

static void llcp_call_connect_sap(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket = -1;
	LlcpFuncData *func_data = user_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_connect_sap_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, &out_client_socket, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish connect sap: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	socket_data = llcp_socket_data_find(out_client_socket);
	if (socket_data == NULL)
	{
		NFC_ERR("Wrong client socket is returned");
		return;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_llcp_connect_sap_completed callback =
			(net_nfc_client_llcp_connect_sap_completed)func_data->callback;

		callback(result, out_client_socket, func_data->user_data);
	}

	/* TODO : release resource when socket is closed */
	//	g_free(func_data);
}

static void llcp_call_send(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket = -1;
	LlcpFuncData *func_data = user_data;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_send_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, &out_client_socket, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish send: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_llcp_send_completed callback =
			(net_nfc_client_llcp_send_completed)func_data->callback;

		callback(result, func_data->user_data);
	}

	g_free(func_data);
}

static void llcp_call_send_to(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket = -1;
	LlcpFuncData *func_data = user_data;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_send_to_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, &out_client_socket, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish send to: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_llcp_send_to_completed callback =
			(net_nfc_client_llcp_send_to_completed)func_data->callback;

		callback(result, func_data->user_data);
	}

	g_free(func_data);
}

static void llcp_call_receive(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GVariant *variant;
	GError *error = NULL;
	net_nfc_error_e result;
	LlcpFuncData *func_data = user_data;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_receive_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, &variant, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish receive: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		data_s data = { NULL, 0 };
		net_nfc_client_llcp_receive_completed callback =
			(net_nfc_client_llcp_receive_completed)func_data->callback;

		net_nfc_util_gdbus_variant_to_data_s(variant, &data);

		callback(result, &data, func_data->user_data);

		net_nfc_util_free_data(&data);
	}

	g_free(func_data);
}

static void llcp_call_receive_from(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	sap_t sap = 0;
	GError *error = NULL;
	net_nfc_error_e result;
	GVariant *variant = NULL;
	LlcpFuncData *func_data = user_data;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_receive_from_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, &sap, &variant, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish receive from: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		data_s data = { NULL, 0 };
		net_nfc_client_llcp_receive_from_completed callback =
			(net_nfc_client_llcp_receive_from_completed)func_data->callback;

		net_nfc_util_gdbus_variant_to_data_s(variant, &data);

		callback(result, sap, &data, func_data->user_data);

		net_nfc_util_free_data(&data);
	}

	g_free(func_data);
}

static void llcp_call_close(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket = -1;
	LlcpFuncData *func_data = user_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_close_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, &out_client_socket, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish close: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	socket_data = llcp_socket_data_find(out_client_socket);
	if (socket_data == NULL)
	{
		NFC_ERR("Wrong client socket is returned");
		return;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_llcp_close_completed callback =
			(net_nfc_client_llcp_close_completed)func_data->callback;

		callback(result, func_data->user_data);
	}

	g_free(func_data);
}

static void llcp_call_disconnect(GObject *source_object, 	GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket = -1;
	LlcpFuncData *func_data = user_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	g_assert(func_data != NULL);

	ret = net_nfc_gdbus_llcp_call_disconnect_finish(NET_NFC_GDBUS_LLCP(source_object),
			&result, &out_client_socket, res, &error);
	if (FALSE == ret)
	{
		NFC_ERR("Can not finish disconnect: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	socket_data = llcp_socket_data_find(out_client_socket);
	if (socket_data == NULL)
	{
		NFC_ERR("Wrong client socket is returned");
		return;
	}

	if (func_data->callback != NULL) {
		net_nfc_client_llcp_disconnect_completed callback =
			(net_nfc_client_llcp_disconnect_completed)func_data->callback;

		callback(result, func_data->user_data);
	}

	g_free(func_data);
}

API net_nfc_error_e net_nfc_client_llcp_get_config(
		net_nfc_llcp_config_info_s **config)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);

	*config = &llcp_config;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_listen(net_nfc_llcp_socket_t socket,
		const char *service_name,
		sap_t sap,
		net_nfc_client_llcp_listen_completed callback,
		void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* FIXME: temporary typecast to (uint8_t *) */
	socket_data->service_name = (uint8_t *)g_strdup(service_name);
	socket_data->sap = sap;

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL) {
		NFC_ERR("g_new0 failed");

		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_listen(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			socket_data->miu,
			socket_data->rw,
			socket_data->type,
			socket_data->sap,
			service_name,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_listen,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_listen_sync(net_nfc_llcp_socket_t socket,
		const char *service_name, sap_t sap, net_nfc_llcp_socket_t *out_socket)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;
	net_nfc_llcp_internal_socket_s *out_socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(NULL == out_socket, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == service_name, NET_NFC_NULL_PARAMETER);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* FIXME: temporary typecast to (uint8_t *) */
	socket_data->service_name = (uint8_t *)g_strdup(service_name);
	socket_data->sap = sap;

	ret = net_nfc_gdbus_llcp_call_listen_sync(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			socket_data->miu,
			socket_data->rw,
			socket_data->type,
			socket_data->sap,
			service_name,
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&out_client_socket,
			NULL,
			&error);
	if (TRUE == ret)
	{
		out_socket_data = llcp_socket_data_find(out_client_socket);
		if (out_socket_data == NULL || out_socket_data != socket_data)
		{
			NFC_ERR("out_socket_data(%p), socket_data(%p)", out_socket_data, socket_data);
			return NET_NFC_UNKNOWN_ERROR;
		}

		if (out_socket)
			*out_socket = out_client_socket;
	}
	else
	{
		NFC_ERR("can not listen: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_llcp_accept(net_nfc_llcp_socket_t socket,
		net_nfc_client_llcp_accept_completed callback, void *user_data)
{
	LlcpFuncData *func_data;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL)
	{
		NFC_ERR("g_new0 failed");
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_accept(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_accept,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_accept_sync(net_nfc_llcp_socket_t socket)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	ret = net_nfc_gdbus_llcp_call_accept_sync(llcp_proxy, GPOINTER_TO_UINT(llcp_handle),
			socket, net_nfc_client_gdbus_get_privilege(), &result, NULL, &error);
	if (FALSE == ret)
	{
		NFC_ERR("can not connect: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_llcp_reject(net_nfc_llcp_socket_t socket,
		net_nfc_client_llcp_reject_completed callback, void *user_data)
{
	LlcpFuncData *func_data;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL)
	{
		NFC_ERR("g_new0 failed");
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_reject(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_reject,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_reject_sync(net_nfc_llcp_socket_t socket)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	ret = net_nfc_gdbus_llcp_call_reject_sync(llcp_proxy, GPOINTER_TO_UINT(llcp_handle),
			socket, net_nfc_client_gdbus_get_privilege(), &result, NULL, &error);
	if (FALSE == ret)
	{
		NFC_ERR("can not connect: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_llcp_connect(net_nfc_llcp_socket_t socket,
		const char *service_name,
		net_nfc_client_llcp_connect_completed callback,
		void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(NULL == service_name, NET_NFC_NULL_PARAMETER);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL)
	{
		NFC_ERR("g_new0 failed");

		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_connect(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			socket_data->miu,
			socket_data->rw,
			socket_data->type,
			service_name,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_connect,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_connect_sync(net_nfc_llcp_socket_t socket,
		const char *service_name, net_nfc_llcp_socket_t *out_socket)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;
	net_nfc_llcp_internal_socket_s *out_socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(NULL == service_name, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == out_socket, NET_NFC_NULL_PARAMETER);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	ret = net_nfc_gdbus_llcp_call_connect_sync(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			socket_data->miu,
			socket_data->rw,
			socket_data->type,
			service_name,
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&out_client_socket,
			NULL,
			&error);
	if (TRUE == ret)
	{
		out_socket_data = llcp_socket_data_find(out_client_socket);
		if (out_socket_data == NULL || out_socket_data !=  socket_data)
		{
			NFC_ERR("out_socket_data(%p), socket_data(%p)", out_socket_data, socket_data);
			return NET_NFC_UNKNOWN_ERROR;
		}

		if (out_socket)
			*out_socket = out_client_socket;
	}
	else
	{
		NFC_ERR("can not connect: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_llcp_connect_sap(net_nfc_llcp_socket_t socket,
		sap_t sap, net_nfc_client_llcp_connect_sap_completed callback, void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(0 == sap, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL)
	{
		NFC_ERR("g_new0 failed");
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_connect_sap(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			socket_data->miu,
			socket_data->rw,
			socket_data->type,
			sap,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_connect_sap,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_connect_sap_sync(
		net_nfc_llcp_socket_t socket, sap_t sap, net_nfc_llcp_socket_t *out_socket)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;
	net_nfc_llcp_internal_socket_s *out_socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(NULL == out_socket, NET_NFC_NULL_PARAMETER);
	RETV_IF(0 == sap, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	ret = net_nfc_gdbus_llcp_call_connect_sap_sync(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			socket_data->miu,
			socket_data->rw,
			socket_data->type,
			sap,
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&out_client_socket,
			NULL,
			&error);
	if (TRUE == ret)
	{
		out_socket_data = llcp_socket_data_find(out_client_socket);
		if (out_socket_data == NULL || out_socket_data !=  socket_data)
		{
			NFC_ERR("out_socket_data(%p), socket_data(%p)", out_socket_data, socket_data);
			return NET_NFC_UNKNOWN_ERROR;
		}

		if (out_socket)
			*out_socket = out_client_socket;
	}
	else
	{
		NFC_ERR("can not connect: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_llcp_send(net_nfc_llcp_socket_t socket,
		data_s *data, net_nfc_client_llcp_send_completed callback, void *user_data)
{
	GVariant *variant;
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(NULL == data, NET_NFC_NULL_PARAMETER);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL)
	{
		NFC_ERR("g_new0 failed");
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	variant = net_nfc_util_gdbus_data_to_variant(data);

	net_nfc_gdbus_llcp_call_send(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			variant,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_send,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_send_sync(net_nfc_llcp_socket_t socket,
		data_s *data)
{
	gboolean ret;
	GVariant *variant;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket;
	net_nfc_llcp_internal_socket_s *socket_data;
	net_nfc_llcp_internal_socket_s *out_socket_data;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(NULL == data, NET_NFC_NULL_PARAMETER);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	variant = net_nfc_util_gdbus_data_to_variant(data);

	ret = net_nfc_gdbus_llcp_call_send_sync(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			variant,
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&out_client_socket,
			NULL,
			&error);
	if (TRUE == ret)
	{
		out_socket_data = llcp_socket_data_find(out_client_socket);
		if (out_socket_data == NULL)
		{
			NFC_ERR("out_socket_data is NULL");
			return NET_NFC_UNKNOWN_ERROR;
		}
	}
	else
	{
		NFC_ERR("can not call send: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_llcp_send_to(net_nfc_llcp_socket_t socket,
		sap_t sap,
		data_s *data,
		net_nfc_client_llcp_send_to_completed callback,
		void *user_data)
{
	GVariant *variant;
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(NULL == data, NET_NFC_NULL_PARAMETER);
	RETV_IF(0 == sap, NET_NFC_INVALID_PARAM);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL)
	{
		NFC_ERR("g_new0 failed");
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	variant = net_nfc_util_gdbus_data_to_variant(data);

	net_nfc_gdbus_llcp_call_send_to(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			sap,
			variant,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_send_to,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_send_to_sync(net_nfc_llcp_socket_t socket,
		sap_t sap, data_s *data)
{
	gboolean ret;
	GVariant *variant;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket;
	net_nfc_llcp_internal_socket_s *socket_data;
	net_nfc_llcp_internal_socket_s *out_socket_data;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(NULL == data, NET_NFC_NULL_PARAMETER);
	RETV_IF(0 == sap, NET_NFC_INVALID_PARAM);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	variant = net_nfc_util_gdbus_data_to_variant(data);

	ret = net_nfc_gdbus_llcp_call_send_to_sync(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			sap,
			variant,
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&out_client_socket,
			NULL,
			&error);
	if (TRUE == ret)
	{
		out_socket_data = llcp_socket_data_find(out_client_socket);
		if (out_socket_data == NULL)
		{
			NFC_ERR("out_socket_data is NULL");
			return NET_NFC_UNKNOWN_ERROR;
		}
	}
	else
	{
		NFC_ERR("can not call send to: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_llcp_receive(net_nfc_llcp_socket_t socket,
		size_t request_length,
		net_nfc_client_llcp_receive_completed callback,
		void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(0 == request_length, NET_NFC_INVALID_PARAM);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL)
	{
		NFC_ERR("g_new0 failed");
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_receive(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			request_length,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_receive,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_receive_sync(net_nfc_llcp_socket_t socket,
		size_t request_length, data_s **out_data)
{
	gboolean ret;
	GVariant *variant;
	GError *error = NULL;
	net_nfc_error_e result;
	net_nfc_llcp_internal_socket_s *socket_data;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(NULL == out_data, NET_NFC_NULL_PARAMETER);
	RETV_IF(0 == request_length, NET_NFC_INVALID_PARAM);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	*out_data = NULL;

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	ret = net_nfc_gdbus_llcp_call_receive_sync(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			request_length,
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&variant,
			NULL,
			&error);
	if (TRUE == ret)
	{
		*out_data = net_nfc_util_gdbus_variant_to_data(variant);
	}
	else
	{
		NFC_ERR("can not call receive: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_llcp_receive_from(net_nfc_llcp_socket_t socket,
		size_t request_length,
		net_nfc_client_llcp_receive_from_completed callback,
		void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(0 == request_length, NET_NFC_INVALID_PARAM);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL)
	{
		NFC_ERR("g_new0 failed");
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_receive_from(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			request_length,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_receive_from,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_receive_from_sync(
		net_nfc_llcp_socket_t socket,
		size_t request_length,
		sap_t *out_sap,
		data_s **out_data)
{
	sap_t sap;
	gboolean ret;
	GVariant *variant;
	GError *error = NULL;
	net_nfc_error_e result;
	net_nfc_llcp_internal_socket_s *socket_data;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(NULL == out_sap, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == out_data, NET_NFC_NULL_PARAMETER);
	RETV_IF(0 == request_length, NET_NFC_INVALID_PARAM);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	*out_data = NULL;
	*out_sap = 0;

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	ret = net_nfc_gdbus_llcp_call_receive_from_sync(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			request_length,
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&sap,
			&variant,
			NULL,
			&error);
	if (TRUE == ret)
	{
		*out_sap = sap;
		*out_data = net_nfc_util_gdbus_variant_to_data(variant);
	}
	else
	{
		NFC_ERR("can not call receive from: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_llcp_close(net_nfc_llcp_socket_t socket,
		net_nfc_client_llcp_close_completed callback, void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL)
	{
		NFC_ERR("g_new0 failed");
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_close(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_close,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_close_sync(net_nfc_llcp_socket_t socket)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;
	net_nfc_llcp_internal_socket_s *out_socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	ret = net_nfc_gdbus_llcp_call_close_sync(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&out_client_socket,
			NULL,
			&error);
	if (TRUE == ret)
	{
		out_socket_data = llcp_socket_data_find(out_client_socket);
		if (out_socket_data == NULL || out_socket_data !=  socket_data)
		{
			NFC_ERR("out_socket_data(%p), socket_data(%p)", out_socket_data, socket_data);
			return NET_NFC_UNKNOWN_ERROR;
		}
	}
	else
	{
		NFC_ERR("can not close: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_llcp_disconnect(net_nfc_llcp_socket_t socket,
		net_nfc_client_llcp_disconnect_completed callback, void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	func_data = g_new0(LlcpFuncData, 1);
	if (func_data == NULL)
	{
		NFC_ERR("g_new0 failed");
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_disconnect(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			llcp_call_disconnect,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_disconnect_sync(
		net_nfc_llcp_socket_t socket)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	guint32 out_client_socket;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;
	net_nfc_llcp_internal_socket_s *out_socket_data = NULL;

	RETV_IF(NULL == llcp_proxy, NET_NFC_NOT_INITIALIZED);
	RETV_IF(socket <= 0, NET_NFC_INVALID_PARAM);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		NFC_ERR("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	ret = net_nfc_gdbus_llcp_call_disconnect_sync(llcp_proxy,
			GPOINTER_TO_UINT(llcp_handle),
			socket_data->client_socket,
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&out_client_socket,
			NULL,
			&error);
	if (TRUE == ret)
	{
		out_socket_data = llcp_socket_data_find(out_client_socket);
		if (out_socket_data == NULL || out_socket_data !=  socket_data)
		{
			NFC_ERR("out_socket_data(%p), socket_data(%p)", out_socket_data, socket_data);
			return NET_NFC_UNKNOWN_ERROR;
		}
	}
	else
	{
		NFC_ERR("can not disconnect: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API void net_nfc_client_llcp_create_socket(net_nfc_llcp_socket_t *socket,
		net_nfc_llcp_socket_option_s *option)
{
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	socket_data = g_new0(net_nfc_llcp_internal_socket_s, 1);

	socket_data->client_socket = socket_handle++;

	if (option)
	{
		socket_data->miu = option->miu;
		socket_data->rw = option->rw;
		socket_data->type = option->type;
	}
	else
	{
		socket_data->miu = 128;
		socket_data->rw = 1;
		socket_data->type = NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONORIENTED;
	}

	socket_data->device_id = llcp_handle;
	socket_data->close_requested = false;


	if (socket)
		*socket = socket_data->client_socket;
}


API net_nfc_error_e net_nfc_client_llcp_get_local_config(
		net_nfc_llcp_config_info_s **config)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);

	*config = &llcp_config;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_get_local_socket_option(
		net_nfc_llcp_socket_t socket, net_nfc_llcp_socket_option_s **info)
{
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
		return NET_NFC_LLCP_INVALID_SOCKET;

	*info = (net_nfc_llcp_socket_option_s*)socket_data;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_create_socket_option(
		net_nfc_llcp_socket_option_s **option,
		uint16_t miu,
		uint8_t rw,
		net_nfc_socket_type_e type)
{
	net_nfc_llcp_socket_option_s *tmp_option = NULL;

	RETV_IF(NULL == option, NET_NFC_NULL_PARAMETER);
	RETVM_IF(miu < 128 || miu > 1152, NET_NFC_OUT_OF_BOUND, "miu = %d", miu);
	RETVM_IF(rw < 1 || rw > 15, NET_NFC_OUT_OF_BOUND, "rw = %d", rw);
	RETVM_IF((type < NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONORIENTED
				|| type > NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONLESS),
			NET_NFC_OUT_OF_BOUND,
			"type = %d", type);

	_net_nfc_util_alloc_mem(tmp_option, sizeof(net_nfc_llcp_socket_option_s));
	if (tmp_option != NULL)
	{
		tmp_option->miu = miu;
		tmp_option->rw = rw;
		tmp_option->type = type;

		*option = tmp_option;

		return NET_NFC_OK;
	}
	else
	{
		return NET_NFC_ALLOC_FAIL;
	}
}

API net_nfc_error_e net_nfc_client_llcp_create_socket_option_default(
		net_nfc_llcp_socket_option_s **option)
{
	return net_nfc_client_llcp_create_socket_option(option, 128, 1,
			NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONORIENTED);
}

API net_nfc_error_e net_nfc_client_llcp_get_socket_option_miu(
		net_nfc_llcp_socket_option_s *option, uint16_t *miu)
{
	RETV_IF(NULL == option, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == miu, NET_NFC_NULL_PARAMETER);

	*miu = option->miu;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_set_socket_option_miu(
		net_nfc_llcp_socket_option_s *option, uint16_t miu)
{
	RETV_IF(NULL == option, NET_NFC_NULL_PARAMETER);

	option->miu = miu;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_get_socket_option_rw(
		net_nfc_llcp_socket_option_s *option, uint8_t *rw)
{
	RETV_IF(NULL == option, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == rw, NET_NFC_NULL_PARAMETER);

	*rw = option->rw;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_set_socket_option_rw(
		net_nfc_llcp_socket_option_s *option, uint8_t rw)
{
	RETV_IF(NULL == option, NET_NFC_NULL_PARAMETER);

	option->rw = rw;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_get_socket_option_type(
		net_nfc_llcp_socket_option_s *option, net_nfc_socket_type_e *type)
{
	RETV_IF(NULL == type, NET_NFC_NULL_PARAMETER);

	*type = option->type;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_set_socket_option_type(
		net_nfc_llcp_socket_option_s *option, net_nfc_socket_type_e type)
{
	RETV_IF(NULL == option, NET_NFC_NULL_PARAMETER);

	option->type = type;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_free_socket_option(
		net_nfc_llcp_socket_option_s *option)
{
	RETV_IF(NULL == option, NET_NFC_NULL_PARAMETER);

	_net_nfc_util_free_mem(option);
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_create_config(
		net_nfc_llcp_config_info_s **config,
		uint16_t miu,
		uint16_t wks,
		uint8_t lto,
		uint8_t option)
{
	net_nfc_llcp_config_info_s *tmp_config;

	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);

	_net_nfc_util_alloc_mem(tmp_config, sizeof(net_nfc_llcp_config_info_s));
	if (tmp_config == NULL)
		return NET_NFC_ALLOC_FAIL;

	tmp_config->miu = miu;
	tmp_config->wks = wks;
	tmp_config->lto = lto;
	tmp_config->option = option;

	*config = tmp_config;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_create_config_default(
		net_nfc_llcp_config_info_s **config)
{
	return net_nfc_client_llcp_create_config(config, 128, 1, 10, 0);
}

API net_nfc_error_e net_nfc_client_llcp_get_config_miu(
		net_nfc_llcp_config_info_s *config, uint16_t *miu)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == miu, NET_NFC_NULL_PARAMETER);

	*miu = config->miu;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_get_config_wks(
		net_nfc_llcp_config_info_s *config, uint16_t *wks)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == wks, NET_NFC_NULL_PARAMETER);

	*wks = config->wks;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_get_config_lto(
		net_nfc_llcp_config_info_s *config, uint8_t *lto)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == lto, NET_NFC_NULL_PARAMETER);

	*lto = config->lto;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_get_config_option(
		net_nfc_llcp_config_info_s *config, uint8_t *option)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == option, NET_NFC_NULL_PARAMETER);

	*option = config->option;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_set_config_miu(
		net_nfc_llcp_config_info_s *config, uint16_t miu)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);
	RETVM_IF(miu < 128 || miu > 1152, NET_NFC_OUT_OF_BOUND, "miu = %d", miu);

	config->miu = miu;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_set_config_wks(
		net_nfc_llcp_config_info_s *config, uint16_t wks)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);

	config->wks = wks;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_set_config_lto(
		net_nfc_llcp_config_info_s *config, uint8_t lto)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);

	config->lto = lto;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_set_config_option(
		net_nfc_llcp_config_info_s *config, uint8_t option)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);

	config->option = option;
	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_llcp_free_config(
		net_nfc_llcp_config_info_s *config)
{
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);

	_net_nfc_util_free_mem(config);
	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_client_llcp_init(void)
{
	GError *error = NULL;

	if (llcp_proxy)
	{
		NFC_WARN("Already initialized");
		return NET_NFC_OK;
	}

	llcp_proxy = net_nfc_gdbus_llcp_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Llcp",
			NULL,
			&error);

	if (llcp_proxy == NULL)
	{
		NFC_ERR("Can not create proxy : %s", error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	g_signal_connect(llcp_proxy, "error",
			G_CALLBACK(llcp_error), NULL);

	g_signal_connect(llcp_proxy, "incoming",
			G_CALLBACK(llcp_incoming), NULL);

	return NET_NFC_OK;
}

void net_nfc_client_llcp_deinit(void)
{
	if (llcp_proxy)
	{
		g_object_unref(llcp_proxy);
		llcp_proxy = NULL;
	}
}
