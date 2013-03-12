/*
 * Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Flora License, Version 1.0 (the "License");
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

#include <unistd.h>
#include <glib.h>

#include "vconf.h"

#include "net_nfc_debug_private.h"
#include "net_nfc_util_private.h"
#include "net_nfc_util_defines.h"
#include "net_nfc_server_context_private.h"

static GList *g_client_contexts = NULL;
static pthread_mutex_t g_client_context_lock = PTHREAD_MUTEX_INITIALIZER;

static gint _client_context_compare_func(gconstpointer a, gconstpointer b)
{
	gint result = -1;
	net_nfc_client_info_t *info = (net_nfc_client_info_t *)a;

	if (info->socket == (int)b)
		result = 0;
	else if (info->socket > (int)b)
		result = 1;

	return result;
}

static void _cleanup_client_context(gpointer data)
{
	net_nfc_client_info_t *info = data;

	if (info != NULL)
	{
		if (info->channel != NULL)
		{
			g_io_channel_unref(info->channel);
		}

		/* need to check . is it necessary to remove g_source_id */
		if (info->src_id > 0)
		{
			g_source_remove(info->src_id);
		}

		if (info->socket > 0)
		{
			shutdown(info->socket, SHUT_RDWR);
			close(info->socket);
		}

		if (vconf_set_bool(NET_NFC_DISABLE_LAUNCH_POPUP_KEY, net_nfc_server_is_set_launch_state()) != 0)
			DEBUG_ERR_MSG("SERVER :set launch vconf fail");

		DEBUG_SERVER_MSG("cleanup success : client [%d]", info->socket);

		_net_nfc_util_free_mem(info);
	}
}

void net_nfc_server_deinit_client_context()
{
	pthread_mutex_lock(&g_client_context_lock);

	g_list_free_full(g_client_contexts, _cleanup_client_context);

	pthread_mutex_unlock(&g_client_context_lock);
}

int net_nfc_server_get_client_count()
{
	int result = 0;

	pthread_mutex_lock(&g_client_context_lock);

	result = g_list_length(g_client_contexts);

	pthread_mutex_unlock(&g_client_context_lock);

	return result;
}

net_nfc_client_info_t *net_nfc_server_get_client_context(int socket)
{
	net_nfc_client_info_t *result = NULL;
	GList *item = NULL;

	pthread_mutex_lock(&g_client_context_lock);

	item = g_list_find_custom(g_client_contexts, (gconstpointer)socket, _client_context_compare_func);
	if (item != NULL)
	{
		result = item->data;
	}

	pthread_mutex_unlock(&g_client_context_lock);

	return result;
}

void net_nfc_server_add_client_context(int socket, GIOChannel* channel, uint32_t src_id, client_state_e state)
{
	DEBUG_SERVER_MSG("add client context");

	if (net_nfc_server_get_client_context(socket) == NULL)
	{
		net_nfc_client_info_t *info = NULL;

		pthread_mutex_lock(&g_client_context_lock);

		_net_nfc_util_alloc_mem(info, sizeof(net_nfc_client_info_t));
		if (info != NULL)
		{
			info->socket = socket;
			info->channel = channel;
			info->src_id = src_id;
			info->state = state;
			info->is_set_launch_popup = TRUE;

			g_client_contexts = g_list_append(g_client_contexts, info);
		}
		else
		{
			DEBUG_ERR_MSG("alloc failed");
		}

		pthread_mutex_unlock(&g_client_context_lock);
	}
	else
	{
		DEBUG_ERR_MSG("client exists already [%d]", socket);
	}

	DEBUG_SERVER_MSG("current client count = [%d]", g_list_length(g_client_contexts));
}

void net_nfc_server_cleanup_client_context(int socket)
{
	GList *item = NULL;

	DEBUG_SERVER_MSG("clean up client context");

	pthread_mutex_lock(&g_client_context_lock);

	item = g_list_find_custom(g_client_contexts, (gconstpointer)socket, _client_context_compare_func);
	if (item != NULL)
	{
		_cleanup_client_context(item->data);

		g_client_contexts = g_list_delete_link(g_client_contexts, item);
	}

	pthread_mutex_unlock(&g_client_context_lock);

	DEBUG_SERVER_MSG("current client count = [%d]", g_list_length(g_client_contexts));
}

void net_nfc_server_for_each_client_context(net_nfc_server_for_each_client_cb cb, void *user_param)
{
	GList *item = NULL;

	pthread_mutex_lock(&g_client_context_lock);
	item = g_list_first(g_client_contexts);
	while (item != NULL)
	{
		if (cb != NULL)
		{
			cb(item->data, user_param);
		}
		item = g_list_next(item);
	}
	pthread_mutex_unlock(&g_client_context_lock);
}

#ifndef BROADCAST_MESSAGE
net_nfc_target_handle_s* net_nfc_server_get_current_client_target_handle(int socket_fd)
{
	int i = 0;

	pthread_mutex_lock(&g_server_socket_lock);

	net_nfc_target_handle_s* handle = NULL;

	for(; i < NET_NFC_CLIENT_MAX; i++)
	{
		if(g_client_info[i].socket == socket_fd)
		{
			handle = g_client_info[i].target_handle;
			break;
		}
	}

	pthread_mutex_unlock(&g_server_socket_lock);

	return handle;
}

bool net_nfc_server_set_current_client_target_handle(int socket_fd, net_nfc_target_handle_s* handle)
{
	int i = 0;

	pthread_mutex_lock(&g_server_socket_lock);

	for(; i < NET_NFC_CLIENT_MAX; i++)
	{
		if(g_client_info[i].socket == socket_fd)
		{
			g_client_info[i].target_handle = handle;
			pthread_mutex_unlock(&g_server_socket_lock);
			return true;
		}
	}

	pthread_mutex_unlock(&g_server_socket_lock);
	return false;
}
#endif

bool net_nfc_server_check_client_is_running(int socket)
{
#ifdef BROADCAST_MESSAGE
	return (net_nfc_server_get_client_context(socket) != NULL);
#else
	int client_fd = *((int *)client_context);

	if(client_fd > 0)
	return true;
	else
	return false;
#endif
}

client_state_e net_nfc_server_get_client_state(int socket)
{
	GList *item = NULL;
	client_state_e state = NET_NFC_CLIENT_INACTIVE_STATE;

	pthread_mutex_lock(&g_client_context_lock);

	item = g_list_find_custom(g_client_contexts, (gconstpointer)socket, _client_context_compare_func);
	if (item != NULL)
	{
		state = ((net_nfc_client_info_t *)item->data)->state;
	}

	pthread_mutex_unlock(&g_client_context_lock);

	return state;
}

void net_nfc_server_set_client_state(int socket, client_state_e state)
{
	GList *item = NULL;

	pthread_mutex_lock(&g_client_context_lock);

	item = g_list_find_custom(g_client_contexts, (gconstpointer)socket, _client_context_compare_func);
	if (item != NULL)
	{
		((net_nfc_client_info_t *)item->data)->state = state;
	}

	pthread_mutex_unlock(&g_client_context_lock);
}

static void _enable_launch_state(gpointer data, gpointer user_data)
{
	net_nfc_client_info_t *info = (net_nfc_client_info_t *)data;

	if (info != NULL)
	{
		info->state = (bool)user_data;
	}
}

static gint _check_launch_state(gconstpointer a, gconstpointer b)
{
	gint result = 1;
	net_nfc_client_info_t *info = (net_nfc_client_info_t *)a;

	if (info->is_set_launch_popup == false)
		result = 0;

	return result;
}

void net_nfc_server_set_launch_state(int socket, bool enable)
{
	if (vconf_set_bool(NET_NFC_DISABLE_LAUNCH_POPUP_KEY, enable) != 0)
	{
		DEBUG_ERR_MSG("SERVER : launch state set vconf fail");
	}

	if (enable == true)
	{
		pthread_mutex_lock(&g_client_context_lock);
		g_list_foreach(g_client_contexts, _enable_launch_state, (gpointer)true);
		pthread_mutex_unlock(&g_client_context_lock);
	}
	else
	{
		net_nfc_client_info_t *context = net_nfc_server_get_client_context(socket);
		pthread_mutex_lock(&g_client_context_lock);
		if (context != NULL)
		{
			context->state = enable;
		}
		pthread_mutex_unlock(&g_client_context_lock);
	}
}

bool net_nfc_server_is_set_launch_state()
{
	bool result = true;
	GList *item = NULL;

	item = g_list_find_custom(g_client_contexts, NULL, _check_launch_state);
	if (item != NULL)
	{
		result = false;
	}

	return result;
}
