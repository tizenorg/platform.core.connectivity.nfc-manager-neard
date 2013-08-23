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
#include <arpa/inet.h>

#include "net_nfc_debug_internal.h"
#include "net_nfc_server_p2p.h"
#include "net_nfc_server_llcp.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_server_util.h"
#include "net_nfc_server_process_npp.h"

typedef struct _net_nfc_npp_entity_t
{
	uint8_t op;
	uint32_t length;
	uint8_t data[0];
}
__attribute__ ((packed)) net_nfc_npp_entity_t;

typedef struct _net_nfc_npp_msg_t
{
	uint8_t version;
	uint32_t entity_count;
	net_nfc_npp_entity_t entity[0];
}
__attribute__ ((packed)) net_nfc_npp_msg_t;

typedef struct _NppData NppData;

struct _NppData
{
	net_nfc_target_handle_s *handle;
	net_nfc_llcp_socket_t socket;
	uint32_t type;
	data_s data;
	net_nfc_server_npp_callback callback;
	gpointer user_data;
};

typedef struct _NppClientStartData NppClientStartData;

struct _NppClientStartData
{
	net_nfc_target_handle_s *handle;
	int client;
	gpointer user_data;
};

#define NPP_SAN				"com.android.npp"
#define NPP_SAP				0x10

#define NPP_HEADER_LEN			(sizeof(net_nfc_npp_msg_t))
#define NPP_ENTITY_HEADER_LEN		(sizeof(net_nfc_npp_entity_t))

#define NPP_MAJOR_VER			0
#define NPP_MINOR_VER			1
#define NPP_VERSION			((NPP_MAJOR_VER << 4) | NPP_MINOR_VER)

#define NPP_NDEF_ENTRY			0x00000001
#define NPP_ACTION_CODE			0x01

static net_nfc_error_e npp_create_message(data_s *data,
		data_s *message);

/* server */
static void npp_server_receive_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		gpointer user_data);

static void npp_server_process(NppData *npp_data);

static void npp_listen_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		gpointer user_data);

/* client */
static void npp_client_disconnected_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void npp_client_send_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		gpointer user_data);

static void npp_client_process(NppData *npp_data);

static void npp_connected_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		gpointer user_data);

static void npp_socket_error_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		gpointer user_data);

static void npp_default_server_cb(net_nfc_error_e result,
		data_s *data,
		gpointer user_data);

static void npp_default_client_cb(net_nfc_error_e result,
		data_s *data,
		gpointer user_data);

static net_nfc_error_e npp_create_message(data_s *data,
		data_s *message)
{
	uint32_t length = NPP_HEADER_LEN;
	net_nfc_npp_msg_t *msg;

	if (data != NULL)
		length += NPP_ENTITY_HEADER_LEN + data->length;

	message->buffer = g_new0(uint8_t, length);
	message->length = length;

	msg = (net_nfc_npp_msg_t *)message->buffer;
	msg->version = NPP_VERSION;

	if (data != NULL)
	{
		net_nfc_npp_entity_t *entity;

		DEBUG_SERVER_MSG("data->length [%d]", data->length);

		msg->entity_count = htonl(1);

		entity = msg->entity;

		entity->op = NPP_ACTION_CODE;
		entity->length = htonl(data->length);
		/* copy ndef information to response msg */
		memcpy(entity->data, data->buffer, data->length);
	}
	else
	{
		msg->entity_count = 0;
	}

	return NET_NFC_OK;
}

static void npp_server_receive_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		gpointer user_data)
{
	NppData *npp_data;

	net_nfc_npp_msg_t *message;
	net_nfc_npp_entity_t *entity;

	data_s ndef_msg = { NULL, 0 };

	uint32_t length;
	uint32_t entity_count;
	uint32_t i;

	npp_data = (NppData *)user_data;

	if (npp_data == NULL)
		return;

	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("error [%d]", result);

		if(npp_data->callback)
			npp_data->callback(result, NULL, npp_data->user_data);

		g_free(npp_data);
		return;
	}

	if (data == NULL)
	{
		DEBUG_ERR_MSG("data is NULL");

		if(npp_data->callback)
		{
			npp_data->callback(NET_NFC_INVALID_PARAM,
					NULL,
					npp_data->user_data);
		}

		g_free(npp_data);
		return;
	}

	if (data->buffer == NULL || data->length == 0)
	{
		DEBUG_ERR_MSG("Wrong data");

		if(npp_data->callback)
		{
			npp_data->callback(NET_NFC_INVALID_PARAM,
					NULL,
					npp_data->user_data);
		}

		g_free(npp_data);
		return;
	}

	message = (net_nfc_npp_msg_t *)data->buffer;

	if (data->length < NPP_HEADER_LEN)
	{
		DEBUG_ERR_MSG("too short data, length [%d]",
				data->length);
		/* FIXME!!! what should I do. */
		if(npp_data->callback)
		{
			npp_data->callback(NET_NFC_INVALID_PARAM,
					NULL,
					npp_data->user_data);
		}

		g_free(npp_data);
		return;
	}

	if (GET_MAJOR_VER(message->version) > NPP_MAJOR_VER ||
			GET_MINOR_VER(message->version > NPP_MINOR_VER))
	{
		DEBUG_ERR_MSG("not supported version, version [0x%02x]",
				message->version);

		if(npp_data->callback)
		{
			npp_data->callback(NET_NFC_NOT_SUPPORTED,
					NULL,
					npp_data->user_data);
		}

		g_free(npp_data);
		return;
	}

	entity_count = htonl(message->entity_count);

	if (entity_count > NPP_NDEF_ENTRY)
	{
		DEBUG_ERR_MSG("too many entities, [%d]",
				message->entity_count);

		if(npp_data->callback)
		{
			npp_data->callback(NET_NFC_INVALID_PARAM,
					NULL,
					npp_data->user_data);
		}

		g_free(npp_data);
		return;
	}

	for (i = 0; i < entity_count; i++)
	{
		entity = (message->entity + i);

		if (entity->op != NPP_ACTION_CODE) {
			DEBUG_ERR_MSG("not supported action code, [0x%02x]",
					entity->op);

			if(npp_data->callback)
			{
				npp_data->callback(NET_NFC_INVALID_PARAM,
						NULL,
						npp_data->user_data);
			}

			g_free(npp_data);
			return;
		}

		length = htonl(entity->length);

		DEBUG_SERVER_MSG("action code [0x%02x], length [%d]",
				entity->op, length);

		if (length > 0)
		{
			/* buffer create */
			ndef_msg.buffer = g_new0(uint8_t, length);
			ndef_msg.length = length;

			memcpy(ndef_msg.buffer, entity->data, length);
		}
	}

	if (npp_data->callback)
		npp_data->callback(result, &ndef_msg, npp_data->user_data);

	g_free(ndef_msg.buffer);
	g_free(npp_data);
}


static void npp_server_process(NppData *npp_data)
{
	net_nfc_error_e result;
	if (npp_data == NULL)
		return;

	/* receive request */
	result = net_nfc_server_llcp_simple_receive(npp_data->handle,
			npp_data->socket,
			npp_server_receive_cb,
			npp_data);
	if (result != NET_NFC_OK)
	{
		if (npp_data->callback)
			npp_data->callback(result, NULL, npp_data->user_data);

		g_free(npp_data);
	}
}

static void npp_listen_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		gpointer user_data)
{
	NppData *npp_data;
	NppData *accept_data;

	npp_data = (NppData *)user_data;
	if (npp_data == NULL)
		return;

	DEBUG_SERVER_MSG("npp_listen_cb, incoming socket [%#x], result [%d]",
			socket, result);

	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("listen failed [%d]", result);

		if (npp_data->callback)
			npp_data->callback(result, NULL, npp_data->user_data);

		g_free(npp_data);
		return;
	}

	/* start npp server */
	accept_data = g_new0(NppData, 1);

	accept_data->handle = npp_data->handle;
	accept_data->socket = socket;
	accept_data->callback = npp_data->callback;
	accept_data->user_data = npp_data->user_data;

	result = net_nfc_server_llcp_simple_accept(handle,
			socket,
			npp_socket_error_cb,
			accept_data);
	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("%s failed",
				"net_nfc_server_llcp_simple_accept");

		if (npp_data->callback)
			npp_data->callback(result, NULL, npp_data->user_data);

		g_free(npp_data);
		g_free(accept_data);

		return;
	}

	DEBUG_SERVER_MSG("socket [%x] accepted.. waiting for request message",
			socket);

	npp_server_process(accept_data);
	g_free(npp_data);
}

/* client */
static void npp_client_disconnected_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param)
{
	DEBUG_SERVER_MSG("disconnected! [%d]", result);
}

static void npp_client_send_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		gpointer user_data)
{
	NppData *npp_data;

	npp_data = (NppData *)user_data;
	if (npp_data == NULL)
		return;

	DEBUG_SERVER_MSG("send complete... [%d]", result);

	if (npp_data->callback)
	{
		npp_data->callback(result, NULL, npp_data->user_data);
	}

	net_nfc_controller_llcp_disconnect(npp_data->handle,
			npp_data->socket,
			&result,
			npp_client_disconnected_cb,
			NULL);

	g_free(npp_data->data.buffer);
	g_free(npp_data);

}

static void npp_client_process(NppData *npp_data)
{
	net_nfc_error_e result;
	data_s data;

	if (npp_data == NULL)
		return;

	result = npp_create_message(&npp_data->data, &data);
	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("%s failed [%d]", "npp_create_message", result);

		if (npp_data->callback)
			npp_data->callback(result, NULL, npp_data->user_data);

		g_free(npp_data->data.buffer);
		g_free(npp_data);

		return;
	}

	/* send request */
	result = net_nfc_server_llcp_simple_send(npp_data->handle,
			npp_data->socket,
			&data,
			npp_client_send_cb,
			npp_data);
	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_server_llcp_simple_send",
				result);

		if (npp_data->callback)
			npp_data->callback(result, NULL, npp_data->user_data);

		g_free(npp_data->data.buffer);
		g_free(npp_data);

	}

	g_free(data.buffer);
}

static void npp_connected_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		gpointer user_data)
{
	NppData *npp_data;

	npp_data = (NppData *)user_data;
	if (npp_data == NULL)
		return;

	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("connect socket failed, [%d]", result);

		if (npp_data->callback)
			npp_data->callback(result, NULL, npp_data->user_data);

		g_free(npp_data->data.buffer);
		g_free(npp_data);

		return;
	}

	/*start npp client */
	DEBUG_SERVER_MSG("socket [%x] connected, send request message.",
			socket);
	npp_data->socket = socket;

	npp_client_process(npp_data);
	return;
}

static void npp_socket_error_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		gpointer user_data)
{
	NppData *npp_data;

	DEBUG_SERVER_MSG("socket [%x], result [%d]", socket, result);

	npp_data = (NppData *)user_data;
	if (npp_data == NULL)
		return;

	if (npp_data->callback)
		npp_data->callback(result, NULL, npp_data->user_data);

	g_free(npp_data->data.buffer);
	g_free(npp_data);
}

static void npp_default_server_cb(net_nfc_error_e result,
		data_s *data,
		gpointer user_data)
{
	DEBUG_SERVER_MSG("result [%d], data [%p], user_data [%p]",
			result, data, user_data);

	if (data == NULL)
	{
		DEBUG_ERR_MSG("npp server receive failed, [%d]", result);
		return;
	}

	if (data->buffer == NULL)
	{
		DEBUG_ERR_MSG("npp server receive failed, [%d]", result);
		return;
	}

	net_nfc_server_p2p_received(data);
	net_nfc_app_util_process_ndef(data);
}

static void npp_default_client_cb(net_nfc_error_e result,
		data_s *data,
		gpointer user_data)
{
	NppClientStartData *npp_client_data;

	DEBUG_SERVER_MSG("result [%d], data [%p], user_data [%p]",
			result, data, user_data);

	if (user_data == NULL)
		return;

	npp_client_data = (NppClientStartData *)user_data;

	net_nfc_server_p2p_data_sent(result, npp_client_data->user_data);

	g_free(npp_client_data);
}

/* public apis */
net_nfc_error_e net_nfc_server_npp_server(net_nfc_target_handle_s *handle,
		char *san,
		sap_t sap,
		net_nfc_server_npp_callback callback,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;

	NppData *npp_data = NULL;

	if (handle == NULL)
	{
		DEBUG_ERR_MSG("handle is NULL");
		return FALSE;
	}

	if (san == NULL)
	{
		DEBUG_ERR_MSG("san is NULL");
		return FALSE;
	}

	npp_data = g_new0(NppData, 1);

	npp_data->handle = handle;
	npp_data->callback = callback;
	npp_data->user_data = user_data;

	result = net_nfc_server_llcp_simple_server(handle,
			san,
			sap,
			npp_listen_cb,
			npp_socket_error_cb,
			npp_data);
	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("%s failed",
				"net_nfc_server_llcp_simple_server");

		if (npp_data->callback)
			npp_data->callback(result, NULL, npp_data->user_data);

		g_free(npp_data);

		return FALSE;
	}

	DEBUG_SERVER_MSG("start npp server, san [%s], sap [%d]", san, sap);

	return result;
}

net_nfc_error_e net_nfc_server_npp_client(net_nfc_target_handle_s *handle,
		char *san,
		sap_t sap,
		data_s *data,
		net_nfc_server_npp_callback callback,
		gpointer user_data)
{
	net_nfc_error_e result = NET_NFC_OK;;
	net_nfc_llcp_config_info_s config;

	NppData *npp_data;

	if (handle == NULL)
	{
		DEBUG_ERR_MSG("handle is NULL");
		return NET_NFC_NULL_PARAMETER;
	}

	if(net_nfc_controller_llcp_get_remote_config(handle,
				&config,
				&result) == false)
	{
		DEBUG_ERR_MSG("%s failed [%d]",
				"net_nfc_controller_llcp_get_remote_config",
				result);

		return result;
	}

	if (config.miu <
			data->length + NPP_HEADER_LEN + NPP_ENTITY_HEADER_LEN)
	{
		DEBUG_ERR_MSG("too large message, max [%d], request [%d]",
				config.miu - (NPP_HEADER_LEN + NPP_ENTITY_HEADER_LEN),
				data->length);

		return NET_NFC_INSUFFICIENT_STORAGE;
	}

	npp_data = g_new0(NppData, 1);

	npp_data->handle = handle;
	npp_data->callback = callback;
	npp_data->user_data = user_data;

	npp_data->data.buffer = g_new0(uint8_t, data->length);
	npp_data->data.length = data->length;

	result = net_nfc_server_llcp_simple_client(handle,
			san,
			sap,
			npp_connected_cb,
			npp_socket_error_cb,
			npp_data);
	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("%s failed",
				"net_nfc_server_llcp_simple_client");

		if (npp_data->callback)
		{
			npp_data->callback(NET_NFC_UNKNOWN_ERROR,
					NULL,
					npp_data->user_data);
		}

		g_free(npp_data->data.buffer);
		g_free(npp_data);

		return result;
	}

	if (san != NULL)
	{
		DEBUG_SERVER_MSG("start npp client, san [%s], result [%d]",
				san, result);
	}
	else
	{
		DEBUG_SERVER_MSG("start npp client, sap [%d], result [%d]",
				sap, result);
	}

	return result;
}

net_nfc_error_e net_nfc_server_npp_default_server_start(
		net_nfc_target_handle_s *handle)
{
	/* start default npp server */
	return net_nfc_server_npp_server(handle,
			NPP_SAN,
			NPP_SAP,
			npp_default_server_cb,
			(gpointer)1234);
}

static void _npp_default_activate_cb(int event,
		net_nfc_target_handle_s *handle,
		uint32_t sap, const char *san, void *user_param)
{
	net_nfc_error_e result;

	DEBUG_SERVER_MSG("event [%d], handle [%p], sap [%d], san [%s]",
			event, handle, sap, san);

	if (event == NET_NFC_LLCP_START) {
		/* start default npp server */
		result = net_nfc_server_npp_server(handle, (char *)san, sap,
				npp_default_server_cb, user_param);
		if (result != NET_NFC_OK) {
			DEBUG_ERR_MSG("net_nfc_server_npp_server failed, [%d]",
					result);
		}
	} else if (event == NET_NFC_LLCP_UNREGISTERED) {
		/* unregister server, do nothing */
	}
}

net_nfc_error_e net_nfc_server_npp_default_server_register()
{
	char id[20];

	/* TODO : make id, */
	snprintf(id, sizeof(id), "%d", getpid());

	/* start default npp server */
	return net_nfc_server_llcp_register_service(id,
			NPP_SAP,
			NPP_SAN,
			_npp_default_activate_cb,
			NULL);
}

net_nfc_error_e net_nfc_server_npp_default_server_unregister()
{
	char id[20];

	/* TODO : make id, */
	snprintf(id, sizeof(id), "%d", getpid());

	/* start default npp server */
	return net_nfc_server_llcp_unregister_service(id,
			NPP_SAP,
			NPP_SAN);
}

net_nfc_error_e net_nfc_server_npp_default_client_start(
		net_nfc_target_handle_s *handle,
		data_s *data,
		int client,
		gpointer user_data)
{
	NppClientStartData *npp_client_data;

	net_nfc_error_e result = NET_NFC_OK;

	if (handle == NULL)
	{
		DEBUG_ERR_MSG("handle is NULL");
		return NET_NFC_NULL_PARAMETER;
	}

	if (data == NULL)
	{
		DEBUG_ERR_MSG("data is NULL");
		return NET_NFC_NULL_PARAMETER;
	}

	if (data->buffer == NULL)
	{
		DEBUG_ERR_MSG("data->buffer is NULL");
		return NET_NFC_NULL_PARAMETER;
	}

	npp_client_data = g_new0(NppClientStartData, 1);

	npp_client_data->handle = handle;
	npp_client_data->client = client;
	npp_client_data->user_data = user_data;

	result = net_nfc_server_npp_client(handle,
			NPP_SAN,
			NPP_SAP,
			data,
			npp_default_client_cb,
			npp_client_data);

	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("net_nfc_server_client failed");
		g_free(npp_client_data);
	}

	return result;
}
