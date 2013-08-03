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

#ifndef NET_NFC_EXPORT_API
#define NET_NFC_EXPORT_API __attribute__((visibility("default")))
#endif

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


void llcp_socket_data_append(net_nfc_llcp_internal_socket_s *socket_data);

void llcp_socket_data_remove(net_nfc_llcp_internal_socket_s *socket_data);

net_nfc_llcp_internal_socket_s *llcp_socket_data_find
						(net_nfc_llcp_socket_t socket);

/* aysnc callback */
static void llcp_call_config(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data);

static void llcp_call_listen(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data);

static void llcp_call_connect(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data);

static void llcp_call_connect_sap(GObject *source_object,
				GAsyncResult *res,
				gpointer user_data);

static void llcp_call_send(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data);

static void llcp_call_send_to(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data);

static void llcp_call_receive(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data);

static void llcp_call_receive_from(GObject *source_object,
				GAsyncResult *res,
				gpointer user_data);

static void llcp_call_close(GObject *source_object,
				GAsyncResult *res,
				gpointer user_data);

static void llcp_call_disconnect(GObject *source_object,
				GAsyncResult *res,
				gpointer user_data);

/* signal callback */
static void llcp_error(NetNfcGDbusLlcp *object,
		guint arg_handle,
		guint client_socket,
		guint oal_socket,
		gint error,
		gpointer user_data);


void llcp_socket_data_append(net_nfc_llcp_internal_socket_s *socket_data)
{
	if (socket_data_list == NULL)
		return;

	if (socket_data)
	{
		socket_data_list = g_list_append(socket_data_list,
						socket_data);
	}
}

void llcp_socket_data_remove(net_nfc_llcp_internal_socket_s *socket_data)
{
	if (socket_data_list == NULL)
		return;

	if (socket_data)
	{
		socket_data_list = g_list_remove(socket_data_list,
						socket_data);

		g_free(socket_data->service_name);
		g_free(socket_data);
	}
}

net_nfc_llcp_internal_socket_s *llcp_socket_data_find
						(net_nfc_llcp_socket_t socket)
{
	GList *pos;

	if (socket_data_list == NULL)
		return NULL;


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

static void llcp_call_config(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	LlcpFuncData *func_data;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_llcp_config_completed callback;

	if (net_nfc_gdbus_llcp_call_config_finish(
					NET_NFC_GDBUS_LLCP(source_object),
					res,
					&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish config: %s",
				error->message);
		g_error_free(error);
	}

	func_data = user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not available");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_llcp_config_completed)func_data->callback;
	callback(result, func_data->user_data);
}

static void llcp_call_listen(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	LlcpFuncData *func_data;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	guint32 out_client_socket;
	guint32 out_oal_socket;

	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	net_nfc_client_llcp_listen_completed callback;

	if (net_nfc_gdbus_llcp_call_listen_finish(
					NET_NFC_GDBUS_LLCP(source_object),
					&out_client_socket,
					&out_oal_socket,
					res,
					&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish listen: %s",
				error->message);
		g_error_free(error);
	}

	socket_data = llcp_socket_data_find(out_client_socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("Wrong client socket is returned");
		return;
	}

	socket_data->oal_socket = out_oal_socket;

	func_data = user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not available");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_llcp_listen_completed)func_data->callback;
	callback(result, out_client_socket, func_data->user_data);
}

static void llcp_call_connect(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	LlcpFuncData *func_data;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	guint32 out_client_socket;
	guint32 out_oal_socket;

	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	net_nfc_client_llcp_connect_completed callback;

	if (net_nfc_gdbus_llcp_call_connect_finish(
					NET_NFC_GDBUS_LLCP(source_object),
					&out_client_socket,
					&out_oal_socket,
					res,
					&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish connect: %s",
				error->message);
		g_error_free(error);
	}

	socket_data = llcp_socket_data_find(out_client_socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("Wrong client socket is returned");
		return;
	}

	socket_data->oal_socket = out_oal_socket;

	func_data = user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not available");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_llcp_connect_completed)func_data->callback;
	callback(result, out_client_socket, func_data->user_data);
}

static void llcp_call_connect_sap(GObject *source_object,
				GAsyncResult *res,
				gpointer user_data)
{
	LlcpFuncData *func_data;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	guint32 out_client_socket;
	guint32 out_oal_socket;

	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	net_nfc_client_llcp_connect_sap_completed callback;

	if (net_nfc_gdbus_llcp_call_connect_sap_finish(
					NET_NFC_GDBUS_LLCP(source_object),
					&out_client_socket,
					&out_oal_socket,
					res,
					&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish connect sap: %s",
				error->message);
		g_error_free(error);
	}

	socket_data = llcp_socket_data_find(out_client_socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("Wrong client socket is returned");
		return;
	}

	socket_data->oal_socket = out_oal_socket;

	func_data = user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not available");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_llcp_connect_sap_completed)
			func_data->callback;
	callback(result, out_client_socket, func_data->user_data);
}


static void llcp_call_send(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	LlcpFuncData *func_data;

	guint32 out_client_socket;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_llcp_send_completed callback;

	if (net_nfc_gdbus_llcp_call_send_finish(
					NET_NFC_GDBUS_LLCP(source_object),
					&out_client_socket,
					res,
					&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish send: %s",
				error->message);

		g_error_free(error);
	}

	func_data = user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not available");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_llcp_send_completed)func_data->callback;
	callback(result, out_client_socket, func_data->user_data);
}

static void llcp_call_send_to(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	LlcpFuncData *func_data;

	guint32 out_client_socket;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_llcp_send_to_completed callback;

	if (net_nfc_gdbus_llcp_call_send_to_finish(
					NET_NFC_GDBUS_LLCP(source_object),
					&out_client_socket,
					res,
					&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish send to: %s",
				error->message);

		g_error_free(error);
	}
	func_data = user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not available");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_llcp_send_to_completed)func_data->callback;
	callback(result, out_client_socket, func_data->user_data);
}

static void llcp_call_receive(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	LlcpFuncData *func_data;

	GVariant *variant = NULL;
	data_s data;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_llcp_receive_completed callback;

	if (net_nfc_gdbus_llcp_call_receive_finish(
					NET_NFC_GDBUS_LLCP(source_object),
					&variant,
					res,
					&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish receive: %s",
				error->message);

		g_error_free(error);
	}

	func_data = user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not available");
		g_free(func_data);
		return;
	}

	net_nfc_util_gdbus_variant_to_data_s(variant, &data);

	callback = (net_nfc_client_llcp_receive_completed)func_data->callback;
	callback(result, &data, func_data->user_data);

	net_nfc_util_free_data(&data);

	g_free(func_data);
}

static void llcp_call_receive_from(GObject *source_object,
				GAsyncResult *res,
				gpointer user_data)
{
	LlcpFuncData *func_data;

	GVariant *variant = NULL;

	sap_t sap;
	data_s data;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_llcp_receive_from_completed callback;

	if (net_nfc_gdbus_llcp_call_receive_from_finish(
					NET_NFC_GDBUS_LLCP(source_object),
					&sap,
					&variant,
					res,
					&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish receive from: %s",
				error->message);

		g_error_free(error);
	}

	func_data = user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not available");
		g_free(func_data);
		return;
	}

	net_nfc_util_gdbus_variant_to_data_s(variant, &data);

	callback = (net_nfc_client_llcp_receive_from_completed)
			func_data->callback;
	callback(result, sap, &data, func_data->user_data);

	net_nfc_util_free_data(&data);

	g_free(func_data);
}

static void llcp_call_close(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	LlcpFuncData *func_data;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	guint32 out_client_socket;

	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	net_nfc_client_llcp_close_completed callback;

	if (net_nfc_gdbus_llcp_call_close_finish(
					NET_NFC_GDBUS_LLCP(source_object),
					&out_client_socket,
					res,
					&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish close: %s",
				error->message);
		g_error_free(error);
	}

	socket_data = llcp_socket_data_find(out_client_socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("Wrong client socket is returned");
		return;
	}

	func_data = user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not available");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_llcp_close_completed)func_data->callback;
	callback(result, out_client_socket, func_data->user_data);
}

static void llcp_call_disconnect(GObject *source_object,
				GAsyncResult *res,
				gpointer user_data)
{
	LlcpFuncData *func_data;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	guint32 out_client_socket;

	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	net_nfc_client_llcp_disconnect_completed callback;

	if (net_nfc_gdbus_llcp_call_disconnect_finish(
					NET_NFC_GDBUS_LLCP(source_object),
					&out_client_socket,
					res,
					&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish disconnect: %s",
				error->message);
		g_error_free(error);
	}

	socket_data = llcp_socket_data_find(out_client_socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("Wrong client socket is returned");
		return;
	}

	func_data = user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get LlcpFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not available");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_llcp_disconnect_completed)
			func_data->callback;
	callback(result, out_client_socket, func_data->user_data);
}


static void llcp_error(NetNfcGDbusLlcp *object,
		guint arg_handle,
		guint client_socket,
		guint oal_socket,
		gint error,
		gpointer user_data)
{
	INFO_MSG(">>> SIGNAL arrived");
}

/* Public APIs */
NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_config(net_nfc_llcp_config_info_h config,
				net_nfc_client_llcp_config_completed callback,
				void *user_data)
{
	LlcpFuncData *func_data;
	GVariant *variant = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (config == NULL)
	{
		DEBUG_ERR_MSG("Config is empty");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(LlcpFuncData, 1);
	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	memcpy(&llcp_config, config, sizeof(net_nfc_llcp_config_info_s));

	variant = g_variant_new("(qqyy)",
				config->miu,
				config->wks,
				config->lto,
				config->option);

	net_nfc_gdbus_llcp_call_config(llcp_proxy,
				variant,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				llcp_call_config,
				func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_config_sync
					(net_nfc_llcp_config_info_h config)
{
	GVariant *variant = NULL;
	GError *error = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	memcpy(&llcp_config, config, sizeof(net_nfc_llcp_config_info_s));

	variant = g_variant_new("(qqyy)",
				config->miu,
				config->wks,
				config->lto,
				config->option);

	if (net_nfc_gdbus_llcp_call_config_sync(llcp_proxy,
						variant,
						net_nfc_client_gdbus_get_privilege(),
						NULL,
						&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not config: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_get_config
					(net_nfc_llcp_config_info_h *config)
{
	if (config == NULL)
		return NET_NFC_NULL_PARAMETER;

	*config = (net_nfc_llcp_config_info_h)&llcp_config;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_listen(net_nfc_llcp_socket_t socket,
				const char *service_name,
				sap_t sap,
				net_nfc_client_llcp_listen_completed callback,
				void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (service_name == NULL)
	{
		DEBUG_ERR_MSG("service_name is empty");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* FIXME: temporary typecast to (uint8_t *) */
	socket_data->service_name = (uint8_t *)g_strdup(service_name);
	socket_data->sap = sap;

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(LlcpFuncData, 1);
	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_listen(llcp_proxy,
				GPOINTER_TO_UINT(llcp_handle),
				socket_data->client_socket,
				socket_data->oal_socket,
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

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_listen_sync(net_nfc_llcp_socket_t socket,
					const char *service_name,
					sap_t sap,
					net_nfc_llcp_socket_t *out_socket)
{
	GError *error = NULL;
	guint32 out_client_socket;
	guint32 out_oal_socket;

	net_nfc_llcp_internal_socket_s *socket_data = NULL;
	net_nfc_llcp_internal_socket_s *out_socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (service_name == NULL)
	{
		DEBUG_ERR_MSG("service_name is empty");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* FIXME: temporary typecast to (uint8_t *) */
	socket_data->service_name = (uint8_t *)g_strdup(service_name);
	socket_data->sap = sap;

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_llcp_call_listen_sync(llcp_proxy,
					GPOINTER_TO_UINT(llcp_handle),
					socket_data->client_socket,
					socket_data->oal_socket,
					socket_data->miu,
					socket_data->rw,
					socket_data->type,
					socket_data->sap,
					service_name,
					net_nfc_client_gdbus_get_privilege(),
					&out_client_socket,
					&out_oal_socket,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not listen: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	out_socket_data = llcp_socket_data_find(out_client_socket);
	if (out_socket_data == NULL || out_socket_data !=  socket_data)
	{
		DEBUG_ERR_MSG("Wrong client socket is returned");
		return NET_NFC_UNKNOWN_ERROR;
	}

	out_socket_data->oal_socket = out_oal_socket;

	if (out_socket)
		*out_socket = out_client_socket;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_connect(net_nfc_llcp_socket_t socket,
				const char *service_name,
				net_nfc_client_llcp_connect_completed callback,
				void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (service_name == NULL)
	{
		DEBUG_ERR_MSG("service_name is empty");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(LlcpFuncData, 1);
	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_connect(llcp_proxy,
				GPOINTER_TO_UINT(llcp_handle),
				socket_data->client_socket,
				socket_data->oal_socket,
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

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_connect_sync(net_nfc_llcp_socket_t socket,
					const char *service_name,
					net_nfc_llcp_socket_t *out_socket)
{
	GError *error = NULL;
	guint32 out_client_socket;
	guint32 out_oal_socket;

	net_nfc_llcp_internal_socket_s *socket_data = NULL;
	net_nfc_llcp_internal_socket_s *out_socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (service_name == NULL)
	{
		DEBUG_ERR_MSG("service_name is empty");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_llcp_call_connect_sync(llcp_proxy,
					GPOINTER_TO_UINT(llcp_handle),
					socket_data->client_socket,
					socket_data->oal_socket,
					socket_data->miu,
					socket_data->rw,
					socket_data->type,
					service_name,
					net_nfc_client_gdbus_get_privilege(),
					&out_client_socket,
					&out_oal_socket,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not connect: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	out_socket_data = llcp_socket_data_find(out_client_socket);
	if (out_socket_data == NULL || out_socket_data !=  socket_data)
	{
		DEBUG_ERR_MSG("Wrong client socket is returned");
		return NET_NFC_UNKNOWN_ERROR;
	}

	out_socket_data->oal_socket = out_oal_socket;

	if (out_socket)
		*out_socket = out_client_socket;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_connect_sap(net_nfc_llcp_socket_t socket,
			sap_t sap,
			net_nfc_client_llcp_connect_sap_completed callback,
			void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(LlcpFuncData, 1);
	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_connect_sap(llcp_proxy,
				GPOINTER_TO_UINT(llcp_handle),
				socket_data->client_socket,
				socket_data->oal_socket,
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

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_connect_sap_sync(
					net_nfc_llcp_socket_t socket,
					sap_t sap,
					net_nfc_llcp_socket_t *out_socket)
{
	GError *error = NULL;
	guint32 out_client_socket;
	guint32 out_oal_socket;

	net_nfc_llcp_internal_socket_s *socket_data = NULL;
	net_nfc_llcp_internal_socket_s *out_socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_llcp_call_connect_sap_sync(llcp_proxy,
					GPOINTER_TO_UINT(llcp_handle),
					socket_data->client_socket,
					socket_data->oal_socket,
					socket_data->miu,
					socket_data->rw,
					socket_data->type,
					sap,
					net_nfc_client_gdbus_get_privilege(),
					&out_client_socket,
					&out_oal_socket,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not connect: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	out_socket_data = llcp_socket_data_find(out_client_socket);
	if (out_socket_data == NULL || out_socket_data !=  socket_data)
	{
		DEBUG_ERR_MSG("Wrong client socket is returned");
		return NET_NFC_UNKNOWN_ERROR;
	}

	out_socket_data->oal_socket = out_oal_socket;

	if (out_socket)
		*out_socket = out_client_socket;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_send(net_nfc_llcp_socket_t socket,
				data_h data,
				net_nfc_client_llcp_send_completed callback,
				void *user_data)
{
	LlcpFuncData *func_data;
	GVariant *variant = NULL;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	if (data == NULL)
	{
		DEBUG_ERR_MSG("data is empty");
		return NET_NFC_INVALID_PARAM;
	}

	variant = net_nfc_util_gdbus_data_to_variant(data);
	if (variant == NULL)
	{
		DEBUG_ERR_MSG("Can not convert to variant");
		return NET_NFC_INVALID_PARAM;
	}


	func_data = g_new0(LlcpFuncData, 1);
	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_send(llcp_proxy,
				GPOINTER_TO_UINT(llcp_handle),
				socket_data->client_socket,
				socket_data->oal_socket,
				variant,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				llcp_call_send,
				func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_send_sync(net_nfc_llcp_socket_t socket,
					data_h data,
					net_nfc_llcp_socket_t *out_socket)
{
	GVariant *variant;
	GError *error = NULL;
	guint32 out_client_socket;

	net_nfc_llcp_internal_socket_s *socket_data;
	net_nfc_llcp_internal_socket_s *out_socket_data;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	if (data == NULL)
	{
		DEBUG_ERR_MSG("data is empty");
		return NET_NFC_INVALID_PARAM;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	variant = net_nfc_util_gdbus_data_to_variant(data);
	if (variant == NULL)
	{
		DEBUG_ERR_MSG("Can not convert to variant");
		return NET_NFC_INVALID_PARAM;
	}

	if (net_nfc_gdbus_llcp_call_send_sync(llcp_proxy,
					GPOINTER_TO_UINT(llcp_handle),
					socket_data->client_socket,
					socket_data->oal_socket,
					variant,
					net_nfc_client_gdbus_get_privilege(),
					&out_client_socket,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not call send: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	out_socket_data = llcp_socket_data_find(out_client_socket);
	if (out_socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not find socket_data");
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (out_socket)
		*out_socket = out_client_socket;

	return NET_NFC_OK;
}


NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_send_to(net_nfc_llcp_socket_t socket,
				sap_t sap,
				data_h data,
				net_nfc_client_llcp_send_to_completed callback,
				void *user_data)
{
	LlcpFuncData *func_data;
	GVariant *variant = NULL;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	if (data == NULL)
	{
		DEBUG_ERR_MSG("data is empty");
		return NET_NFC_INVALID_PARAM;
	}

	variant = net_nfc_util_gdbus_data_to_variant(data);
	if (variant == NULL)
	{
		DEBUG_ERR_MSG("Can not convert to variant");
		return NET_NFC_INVALID_PARAM;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(LlcpFuncData, 1);
	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_send_to(llcp_proxy,
				GPOINTER_TO_UINT(llcp_handle),
				socket_data->client_socket,
				socket_data->oal_socket,
				sap,
				variant,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				llcp_call_send_to,
				func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_send_to_sync(net_nfc_llcp_socket_t socket,
					sap_t sap,
					data_h data,
					net_nfc_llcp_socket_t *out_socket)
{
	GVariant *variant;
	GError *error = NULL;
	guint32 out_client_socket;

	net_nfc_llcp_internal_socket_s *socket_data;
	net_nfc_llcp_internal_socket_s *out_socket_data;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	if (data == NULL)
	{
		DEBUG_ERR_MSG("data is empty");
		return NET_NFC_INVALID_PARAM;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	variant = net_nfc_util_gdbus_data_to_variant(data);
	if (variant == NULL)
	{
		DEBUG_ERR_MSG("Can not convert to variant");
		return NET_NFC_INVALID_PARAM;
	}

	if (net_nfc_gdbus_llcp_call_send_to_sync(llcp_proxy,
					GPOINTER_TO_UINT(llcp_handle),
					socket_data->client_socket,
					socket_data->oal_socket,
					sap,
					variant,
					net_nfc_client_gdbus_get_privilege(),
					&out_client_socket,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not call send to: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	out_socket_data = llcp_socket_data_find(out_client_socket);
	if (out_socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not find socket_data");
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (out_socket)
		*out_socket = out_client_socket;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_receive(net_nfc_llcp_socket_t socket,
				size_t request_length,
				net_nfc_client_llcp_receive_completed callback,
				void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(LlcpFuncData, 1);
	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_receive(llcp_proxy,
				GPOINTER_TO_UINT(llcp_handle),
				socket_data->client_socket,
				socket_data->oal_socket,
				request_length,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				llcp_call_receive,
				func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_receive_sync(net_nfc_llcp_socket_t socket,
						size_t request_length,
						data_h *out_data)
{
	GVariant *variant = NULL;
	GError *error = NULL;
	net_nfc_llcp_internal_socket_s *socket_data;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_llcp_call_receive_sync(llcp_proxy,
					GPOINTER_TO_UINT(llcp_handle),
					socket_data->client_socket,
					socket_data->oal_socket,
					request_length,
					net_nfc_client_gdbus_get_privilege(),
					&variant,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not call receive: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (out_data)
	{
		*out_data = net_nfc_util_gdbus_variant_to_data(variant);
	}

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_receive_from(net_nfc_llcp_socket_t socket,
			size_t request_length,
			net_nfc_client_llcp_receive_from_completed callback,
			void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	func_data = g_new0(LlcpFuncData, 1);
	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_receive_from(llcp_proxy,
				GPOINTER_TO_UINT(llcp_handle),
				socket_data->client_socket,
				socket_data->oal_socket,
				request_length,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				llcp_call_receive_from,
				func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_receive_from_sync
						(net_nfc_llcp_socket_t socket,
						 size_t request_length,
						 sap_t *out_sap,
						 data_h *out_data)
{
	GVariant *variant = NULL;
	GError *error = NULL;
	sap_t sap;

	net_nfc_llcp_internal_socket_s *socket_data;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_llcp_call_receive_from_sync(llcp_proxy,
					GPOINTER_TO_UINT(llcp_handle),
					socket_data->client_socket,
					socket_data->oal_socket,
					request_length,
					net_nfc_client_gdbus_get_privilege(),
					&sap,
					&variant,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not call receive from: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (*out_sap)
		*out_sap = sap;

	if (out_data)
	{
		*out_data = net_nfc_util_gdbus_variant_to_data(variant);
	}

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_close(net_nfc_llcp_socket_t socket,
				net_nfc_client_llcp_close_completed callback,
				void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(LlcpFuncData, 1);
	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_close(llcp_proxy,
				GPOINTER_TO_UINT(llcp_handle),
				socket_data->client_socket,
				socket_data->oal_socket,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				llcp_call_close,
				func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_close_sync(net_nfc_llcp_socket_t socket,
					net_nfc_llcp_socket_t *out_socket)
{
	GError *error = NULL;
	guint32 out_client_socket;

	net_nfc_llcp_internal_socket_s *socket_data = NULL;
	net_nfc_llcp_internal_socket_s *out_socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_llcp_call_close_sync(llcp_proxy,
					GPOINTER_TO_UINT(llcp_handle),
					socket_data->client_socket,
					socket_data->oal_socket,
					net_nfc_client_gdbus_get_privilege(),
					&out_client_socket,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not close: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	out_socket_data = llcp_socket_data_find(out_client_socket);
	if (out_socket_data == NULL || out_socket_data !=  socket_data)
	{
		DEBUG_ERR_MSG("Wrong client socket is returned");
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (out_socket)
		*out_socket = out_client_socket;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_disconnect(net_nfc_llcp_socket_t socket,
			net_nfc_client_llcp_disconnect_completed callback,
			void *user_data)
{
	LlcpFuncData *func_data;
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(LlcpFuncData, 1);
	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_llcp_call_disconnect(llcp_proxy,
				GPOINTER_TO_UINT(llcp_handle),
				socket_data->client_socket,
				socket_data->oal_socket,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				llcp_call_disconnect,
				func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_disconnect_sync(
					net_nfc_llcp_socket_t socket,
					net_nfc_llcp_socket_t *out_socket)
{
	GError *error = NULL;
	guint32 out_client_socket;

	net_nfc_llcp_internal_socket_s *socket_data = NULL;
	net_nfc_llcp_internal_socket_s *out_socket_data = NULL;

	if (llcp_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get LlcpProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
	{
		DEBUG_ERR_MSG("can not get socket_data");
		return NET_NFC_LLCP_INVALID_SOCKET;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_llcp_call_disconnect_sync(llcp_proxy,
					GPOINTER_TO_UINT(llcp_handle),
					socket_data->client_socket,
					socket_data->oal_socket,
					net_nfc_client_gdbus_get_privilege(),
					&out_client_socket,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not disconnect: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	out_socket_data = llcp_socket_data_find(out_client_socket);
	if (out_socket_data == NULL || out_socket_data !=  socket_data)
	{
		DEBUG_ERR_MSG("Wrong client socket is returned");
		return NET_NFC_UNKNOWN_ERROR;
	}

	if (out_socket)
		*out_socket = out_client_socket;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
void net_nfc_client_llcp_create_socket(net_nfc_llcp_socket_t *socket,
				net_nfc_llcp_socket_option_h option)
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
		socket_data->type =
			NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONORIENTED;
	}

	socket_data->device_id = llcp_handle;
	socket_data->close_requested = false;


	if (socket)
		*socket = socket_data->client_socket;
}


NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_get_local_config(
					net_nfc_llcp_config_info_h *config)
{
	if (config == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	*config = (net_nfc_llcp_config_info_h)&llcp_config;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_get_local_socket_option(
					net_nfc_llcp_socket_t socket,
					net_nfc_llcp_socket_option_h *info)
{
	net_nfc_llcp_internal_socket_s *socket_data = NULL;

	DEBUG_CLIENT_MSG("function %s is called", __func__);

	socket_data = llcp_socket_data_find(socket);
	if (socket_data == NULL)
		return NET_NFC_LLCP_INVALID_SOCKET;

	*info = (net_nfc_llcp_socket_option_h)socket_data;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_create_socket_option(
					net_nfc_llcp_socket_option_h *option,
					uint16_t miu,
					uint8_t rw,
					net_nfc_socket_type_e type)
{
	net_nfc_llcp_socket_option_s *struct_option = NULL;

	if (option == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	if (miu < 128 || miu > 1152 ||
		rw < 1 || rw > 15 ||
		type < NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONORIENTED ||
		type > NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONLESS)
	{
		return NET_NFC_OUT_OF_BOUND;
	}

	_net_nfc_util_alloc_mem(struct_option, sizeof(net_nfc_llcp_socket_option_s));
	if (struct_option != NULL)
	{
		struct_option->miu = miu;
		struct_option->rw = rw;
		struct_option->type = type;

		*option = (net_nfc_llcp_socket_option_h)struct_option;

		return NET_NFC_OK;
	}
	else
	{
		return NET_NFC_ALLOC_FAIL;
	}
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_create_socket_option_default(
					net_nfc_llcp_socket_option_h *option)
{
	return net_nfc_client_llcp_create_socket_option(
					option,
					128,
					1,
					NET_NFC_LLCP_SOCKET_TYPE_CONNECTIONORIENTED);
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_get_socket_option_miu(
					net_nfc_llcp_socket_option_h option,
					uint16_t *miu)
{
	if (option == NULL || miu == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}
	net_nfc_llcp_socket_option_s *struct_option = (net_nfc_llcp_socket_option_s *)option;

	*miu = struct_option->miu;
	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_set_socket_option_miu(
					net_nfc_llcp_socket_option_h option,
					uint16_t miu)
{
	if (option == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}
	net_nfc_llcp_socket_option_s *struct_option = (net_nfc_llcp_socket_option_s *)option;

	struct_option->miu = miu;
	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_get_socket_option_rw(
					net_nfc_llcp_socket_option_h option,
					uint8_t *rw)
{
	if (option == NULL || rw == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}
	net_nfc_llcp_socket_option_s *struct_option = (net_nfc_llcp_socket_option_s *)option;

	*rw = struct_option->rw;
	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_set_socket_option_rw(
					net_nfc_llcp_socket_option_h option,
					uint8_t rw)
{
	if (option == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}
	net_nfc_llcp_socket_option_s *struct_option = (net_nfc_llcp_socket_option_s *)option;

	struct_option->rw = rw;
	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_get_socket_option_type(
					net_nfc_llcp_socket_option_h option,
					net_nfc_socket_type_e * type)
{
	if (option == NULL || type == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}
	net_nfc_llcp_socket_option_s *struct_option = (net_nfc_llcp_socket_option_s *)option;

	*type = struct_option->type;
	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_set_socket_option_type(
					net_nfc_llcp_socket_option_h option,
					net_nfc_socket_type_e type)
{
	if (option == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}
	net_nfc_llcp_socket_option_s *struct_option = (net_nfc_llcp_socket_option_s *)option;

	struct_option->type = type;
	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_free_socket_option(
					net_nfc_llcp_socket_option_h option)
{
	if (option == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	_net_nfc_util_free_mem(option);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_create_config(
					net_nfc_llcp_config_info_h *config,
					uint16_t miu,
					uint16_t wks,
					uint8_t lto,
					uint8_t option)
{
	if (config == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	net_nfc_llcp_config_info_s **tmp_config =
						(net_nfc_llcp_config_info_s **)config;

	_net_nfc_util_alloc_mem(*tmp_config,
					sizeof(net_nfc_llcp_config_info_s));

	if (*tmp_config == NULL)
		return NET_NFC_ALLOC_FAIL;

	(*tmp_config)->miu = miu;
	(*tmp_config)->wks = wks;
	(*tmp_config)->lto = lto;
	(*tmp_config)->option = option;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_create_config_default(net_nfc_llcp_config_info_h *config)
{
	return net_nfc_client_llcp_create_config(config, 128, 1, 10, 0);
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_get_config_miu(
					net_nfc_llcp_config_info_h config,
					uint16_t *miu)
{
	if (config == NULL || miu == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	net_nfc_llcp_config_info_s *tmp_config =
						(net_nfc_llcp_config_info_s *)config;

	*miu = tmp_config->miu;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_get_config_wks(
					net_nfc_llcp_config_info_h config,
					uint16_t *wks)
{
	if (config == NULL || wks == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	net_nfc_llcp_config_info_s *tmp_config =
						(net_nfc_llcp_config_info_s *)config;

	*wks = tmp_config->wks;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_get_config_lto(
						net_nfc_llcp_config_info_h config,
						uint8_t *lto)
{
	if (config == NULL || lto == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	net_nfc_llcp_config_info_s *tmp_config =
						(net_nfc_llcp_config_info_s *)config;

	*lto = tmp_config->lto;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_get_config_option(
					net_nfc_llcp_config_info_h config,
					uint8_t *option)
{
	if (config == NULL || option == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	net_nfc_llcp_config_info_s *tmp_config =
						(net_nfc_llcp_config_info_s *)config;

	*option = tmp_config->option;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_set_config_miu(
					net_nfc_llcp_config_info_h config,
					uint16_t miu)
{
	if (config == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	if (miu < 128 || miu > 1152)
	{
		return NET_NFC_OUT_OF_BOUND;
	}

	net_nfc_llcp_config_info_s * tmp_config =
						(net_nfc_llcp_config_info_s *)config;

	tmp_config->miu = miu;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_set_config_wks(
					net_nfc_llcp_config_info_h config,
					uint16_t wks)
{
	if (config == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	net_nfc_llcp_config_info_s *tmp_config =
						(net_nfc_llcp_config_info_s *)config;

	tmp_config->wks = wks;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_set_config_lto(
					net_nfc_llcp_config_info_h config,
					uint8_t lto)
{
	if (config == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	net_nfc_llcp_config_info_s *tmp_config =
						(net_nfc_llcp_config_info_s *)config;

	tmp_config->lto = lto;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_set_config_option(
					net_nfc_llcp_config_info_h config,
					uint8_t option)
{
	if (config == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	net_nfc_llcp_config_info_s * tmp_config =
						(net_nfc_llcp_config_info_s *)config;

	tmp_config->option = option;

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_llcp_free_config(net_nfc_llcp_config_info_h config)
{
	if (config == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	_net_nfc_util_free_mem(config);
	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_client_llcp_init(void)
{
	GError *error = NULL;

	if (llcp_proxy)
	{
		DEBUG_CLIENT_MSG("Already initialized");
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
		DEBUG_ERR_MSG("Can not create proxy : %s", error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	g_signal_connect(llcp_proxy, "error",
			G_CALLBACK(llcp_error), NULL);

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
