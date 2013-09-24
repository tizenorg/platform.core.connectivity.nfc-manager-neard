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

#include <vconf.h>

#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_server.h"
#include "net_nfc_server_util.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_server_common.h"
#include "net_nfc_server_tag.h"
#include "net_nfc_server_llcp.h"
#include "net_nfc_server_se.h"


typedef struct _ControllerFuncData ControllerFuncData;

struct _ControllerFuncData
{
	net_nfc_server_controller_func func;
	gpointer data;
};

static gpointer controller_thread_func(gpointer user_data);

static void controller_async_queue_free_func(gpointer user_data);

static void controller_thread_deinit_thread_func(gpointer user_data);

static void controller_target_detected_cb(void *info,
		void *user_context);

static void controller_se_transaction_cb(void *info,
		void *user_context);

static void controller_llcp_event_cb(void *info,
		void *user_context);

static void controller_init_thread_func(gpointer user_data);

#ifndef ESE_ALWAYS_ON
static void controller_deinit_thread_func(gpointer user_data);
#endif

static void restart_polling_loop_thread_func(gpointer user_data);

static GAsyncQueue *controller_async_queue = NULL;

static GThread *controller_thread = NULL;

static gboolean controller_is_running = FALSE;

static guint32 server_state = NET_NFC_SERVER_IDLE;


static gpointer controller_thread_func(gpointer user_data)
{
	if (controller_async_queue == NULL)
	{
		g_thread_exit(NULL);
		return NULL;
	}

	controller_is_running = TRUE;
	while(controller_is_running)
	{
		ControllerFuncData *func_data;

		func_data = g_async_queue_pop(controller_async_queue);
		if (func_data->func)
			func_data->func(func_data->data);

		g_free(func_data);
	}

	g_thread_exit(NULL);
	return NULL;
}

static void controller_async_queue_free_func(gpointer user_data)
{
	g_free(user_data);
}

static void controller_thread_deinit_thread_func(gpointer user_data)
{
	controller_is_running = FALSE;
}

/* FIXME: it works as broadcast only now */
static void controller_target_detected_cb(void *info,
		void *user_context)
{
	net_nfc_request_target_detected_t *req =
		(net_nfc_request_target_detected_t *)info;

	g_assert(info != NULL);

	if (req->request_type == NET_NFC_MESSAGE_SERVICE_RESTART_POLLING_LOOP)
	{
		net_nfc_server_restart_polling_loop();
	}
	else
	{
		net_nfc_server_set_target_info(info);

		if (req->devType != NET_NFC_UNKNOWN_TARGET) {
			if (req->devType == NET_NFC_NFCIP1_TARGET ||
					req->devType == NET_NFC_NFCIP1_INITIATOR) {
				/* llcp target detected */
				net_nfc_server_llcp_target_detected(info);
			} else {
				/* tag target detected */
				net_nfc_server_tag_target_detected(info);
			}
		}

		/* If target detected, sound should be played. */
		net_nfc_manager_util_play_sound(NET_NFC_TASK_START);
	}

	/* FIXME : should be removed when plugins would be fixed*/
	_net_nfc_util_free_mem(info);
}

static void controller_se_transaction_cb(void *info,
		void *user_context)
{
	net_nfc_request_se_event_t *req = (net_nfc_request_se_event_t *)info;

	g_assert(info != NULL);

	req->user_param = (uint32_t)user_context;

	switch(req->request_type)
	{
	case NET_NFC_MESSAGE_SERVICE_SLAVE_ESE_DETECTED :
		net_nfc_server_se_detected(req);
		break;

	case NET_NFC_MESSAGE_SE_START_TRANSACTION :
		net_nfc_server_se_transaction_received(req);
		break;

	default :
		break;
	}
}

static void _controller_llcp_event_cb(gpointer user_data)
{
	net_nfc_request_llcp_msg_t *req_msg =
		(net_nfc_request_llcp_msg_t *)user_data;

	if (req_msg == NULL)
	{
		NFC_ERR("can not get llcp_event info");

		return;
	}

	switch (req_msg->request_type)
	{
	case NET_NFC_MESSAGE_SERVICE_LLCP_DEACTIVATED:
		net_nfc_server_llcp_deactivated(NULL);
		break;

	case NET_NFC_MESSAGE_SERVICE_LLCP_LISTEN:
		{
			net_nfc_request_listen_socket_t *msg =
				(net_nfc_request_listen_socket_t *)user_data;

			net_nfc_controller_llcp_incoming_cb(msg->client_socket,
					msg->result, NULL, (void *)req_msg->user_param);
		}
		break;

	case NET_NFC_MESSAGE_SERVICE_LLCP_SOCKET_ERROR:
	case NET_NFC_MESSAGE_SERVICE_LLCP_SOCKET_ACCEPTED_ERROR:
		net_nfc_controller_llcp_socket_error_cb(
				req_msg->llcp_socket,
				req_msg->result,
				NULL,
				(void *)req_msg->user_param);
		break;

	case NET_NFC_MESSAGE_SERVICE_LLCP_SEND:
	case NET_NFC_MESSAGE_SERVICE_LLCP_SEND_TO:
		net_nfc_controller_llcp_sent_cb(
				req_msg->llcp_socket,
				req_msg->result,
				NULL,
				(void *)req_msg->user_param);
		break;

	case NET_NFC_MESSAGE_SERVICE_LLCP_RECEIVE:
		{
			net_nfc_request_receive_socket_t *msg =
				(net_nfc_request_receive_socket_t *)user_data;
			data_s data = { msg->data.buffer, msg->data.length };

			net_nfc_controller_llcp_received_cb(msg->client_socket,
					msg->result,
					&data,
					(void *)req_msg->user_param);
		}
		break;

	case NET_NFC_MESSAGE_SERVICE_LLCP_RECEIVE_FROM:
		{
			net_nfc_request_receive_from_socket_t *msg =
				(net_nfc_request_receive_from_socket_t *)user_data;
			data_s data = { msg->data.buffer, msg->data.length };

			/* FIXME : pass sap */
			net_nfc_controller_llcp_received_cb(msg->client_socket,
					msg->result,
					&data,
					(void *)req_msg->user_param);
		}
		break;

	case NET_NFC_MESSAGE_SERVICE_LLCP_CONNECT:
	case NET_NFC_MESSAGE_SERVICE_LLCP_CONNECT_SAP:
		net_nfc_controller_llcp_connected_cb(
				req_msg->llcp_socket,
				req_msg->result,
				NULL,
				(void *)req_msg->user_param);
		break;

	case NET_NFC_MESSAGE_SERVICE_LLCP_DISCONNECT:
		net_nfc_controller_llcp_disconnected_cb(
				req_msg->llcp_socket,
				req_msg->result,
				NULL,
				(void *)req_msg->user_param);
		break;

	default:
		break;
	}

	/* FIXME : should be removed when plugins would be fixed*/
	_net_nfc_util_free_mem(req_msg);
}

/* FIXME : net_nfc_dispatcher_queue_push() need to be removed */
static void controller_llcp_event_cb(void *info, void *user_context)
{
	if(net_nfc_server_controller_async_queue_push(
				_controller_llcp_event_cb, info) == FALSE)
	{
		NFC_ERR("Failed to push onto the queue");
	}
}

static void controller_init_thread_func(gpointer user_data)
{
	net_nfc_error_e result;

	if (net_nfc_controller_init(&result) == false)
	{
		NFC_ERR("net_nfc_controller_init failed, [%d]", result);

		net_nfc_manager_quit();
		return;
	}

	NFC_INFO("net_nfc_controller_init success, [%d]", result);

	if (net_nfc_controller_register_listener(controller_target_detected_cb,
				controller_se_transaction_cb,
				controller_llcp_event_cb,
				&result) == false)
	{
		NFC_ERR("net_nfc_contorller_register_listener failed [%d]",
				result);
	}

	NFC_INFO("net_nfc_contorller_register_listener success");

	result = net_nfc_server_llcp_set_config(NULL);
	if (result != NET_NFC_OK)
	{
		NFC_ERR("net_nfc_server_llcp_set config failed, [%d]",
				result);
	}

	NFC_INFO("net_nfc_server_llcp_set_config success");
}

#ifndef ESE_ALWAYS_ON
static void controller_deinit_thread_func(gpointer user_data)
{
	net_nfc_error_e result;

	if (net_nfc_controller_configure_discovery(NET_NFC_DISCOVERY_MODE_CONFIG,
				NET_NFC_ALL_DISABLE,
				&result) == false) {

		NFC_ERR("net_nfc_controller_configure_discovery failed, [%d]", result);
	}

	net_nfc_server_free_target_info();

	if (net_nfc_controller_deinit() == false)
	{
		NFC_ERR("net_nfc_controller_deinit failed, [%d]", result);
		return;
	}

	NFC_INFO("net_nfc_controller_deinit success");

	net_nfc_manager_quit();
}
#endif

static void restart_polling_loop_thread_func(gpointer user_data)
{

	gint state = 0;
	gint pm_state = 0;

	net_nfc_error_e result;

	if (vconf_get_bool(VCONFKEY_NFC_STATE, &state) != 0)
		NFC_ERR("%s does not exist", "VCONFKEY_NFC_STATE");

	if (state == 0)
		return;

	if (vconf_get_int(VCONFKEY_PM_STATE, &pm_state) != 0)
		NFC_ERR("%s does not exist", "VCONFKEY_PM_STATE");

	NFC_DBG("net_nfc_service_restart_polling, state = [%d]", pm_state);

	if (pm_state == VCONFKEY_PM_STATE_NORMAL)
	{
		if (net_nfc_controller_configure_discovery(
					NET_NFC_DISCOVERY_MODE_CONFIG,
					NET_NFC_ALL_ENABLE,
					&result) == true)
		{
			NFC_DBG("polling enable");
		}

		return;
	}

	if (pm_state == VCONFKEY_PM_STATE_LCDOFF)
	{
		if (net_nfc_controller_configure_discovery(
					NET_NFC_DISCOVERY_MODE_CONFIG,
					NET_NFC_ALL_DISABLE,
					&result) == true)
		{
			NFC_DBG("polling disabled");
		}

		return;
	}
}

gboolean net_nfc_server_controller_thread_init(void)
{
	GError *error = NULL;

	controller_async_queue = g_async_queue_new_full(
			controller_async_queue_free_func);

	controller_thread = g_thread_try_new("controller_thread",
			controller_thread_func,
			NULL,
			&error);

	if (controller_thread == NULL)
	{
		NFC_ERR("can not create controller thread: %s",
				error->message);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

void net_nfc_server_controller_thread_deinit(void)
{
	if(net_nfc_server_controller_async_queue_push(
				controller_thread_deinit_thread_func,
				NULL)==FALSE)
	{
		NFC_ERR("Failed to push onto the queue");
	}

	g_thread_join(controller_thread);
	controller_thread = NULL;

	g_async_queue_unref(controller_async_queue);
	controller_async_queue = NULL;
}

void net_nfc_server_controller_init(void)
{
	if(net_nfc_server_controller_async_queue_push(
				controller_init_thread_func,
				NULL)==FALSE)
	{
		NFC_ERR("Failed to push onto the queue");
	}
}

#ifndef ESE_ALWAYS_ON
void net_nfc_server_controller_deinit(void)
{
	int ret;

	ret = net_nfc_server_controller_async_queue_push(controller_deinit_thread_func, NULL);
	if (FALSE == ret)
	{
		NFC_ERR("Failed to push onto the queue");
	}
}
#endif

gboolean net_nfc_server_controller_async_queue_push(
		net_nfc_server_controller_func func,
		gpointer user_data)
{
	ControllerFuncData *func_data;

	if(controller_async_queue == NULL)
	{
		NFC_ERR("controller_async_queue is not initialized");

		return FALSE;
	}

	func_data = g_new0(ControllerFuncData, 1);
	func_data->func = func;
	func_data->data = user_data;

	g_async_queue_push(controller_async_queue, func_data);

	return TRUE;
}

void net_nfc_server_restart_polling_loop(void)
{
	if(net_nfc_server_controller_async_queue_push(
				restart_polling_loop_thread_func,
				NULL) == FALSE)
	{
		NFC_ERR("Failed to push onto the queue");
	}
}

void net_nfc_server_set_state(guint32 state)
{
	if (state == NET_NFC_SERVER_IDLE)
		server_state &= NET_NFC_SERVER_IDLE;
	else
		server_state |= state;
}

void net_nfc_server_unset_state(guint32 state)
{
	server_state &= ~state;
}

guint32 net_nfc_server_get_state(void)
{
	return server_state;
}
