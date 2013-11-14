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

#include <unistd.h>
#include <glib.h>

#include "vconf.h"
#ifdef SECURITY_SERVER
#include "security-server.h"
#endif

#include "net_nfc_server.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_defines.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_server_context.h"


static GHashTable *client_contexts;
static pthread_mutex_t context_lock = PTHREAD_MUTEX_INITIALIZER;

static void _cleanup_client_context(gpointer data)
{
	net_nfc_client_context_info_t *info = data;

	if (info != NULL)
	{
		g_free(info->id);
		g_free(info);
	}
}

void net_nfc_server_gdbus_init_client_context()
{
	pthread_mutex_lock(&context_lock);

	if (client_contexts == NULL)
		client_contexts = g_hash_table_new(g_str_hash, g_str_equal);

	pthread_mutex_unlock(&context_lock);
}

void net_nfc_server_gdbus_deinit_client_context()
{
	pthread_mutex_lock(&context_lock);

	if (client_contexts != NULL) {
		g_hash_table_destroy(client_contexts);
		client_contexts = NULL;
	}

	pthread_mutex_unlock(&context_lock);
}

/* TODO */
bool net_nfc_server_gdbus_check_privilege(GDBusMethodInvocation *invocation,
		GVariant *privilege, const char *object, const char *right)
{
#ifdef SECURITY_SERVER
	int result;
	data_s priv = { NULL, 0 };

	RETV_IF(NULL == right, false);
	RETV_IF(NULL == object, false);
	RETV_IF(NULL == privilege, false);

	net_nfc_util_gdbus_variant_to_data_s(privilege, &priv);

	result = security_server_check_privilege_by_cookie((char *)priv.buffer, object, right);

	net_nfc_util_free_data(&priv);

	if (result < 0)
	{
		NFC_ERR("permission denied : \"%s\", \"%s\"", object, right);
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Privilege",
				"Permission denied");

		return false;
	}
#endif
	const char *id = g_dbus_method_invocation_get_sender(invocation);

	net_nfc_server_gdbus_add_client_context(id, NET_NFC_CLIENT_ACTIVE_STATE);

	return true;
}

size_t net_nfc_server_gdbus_get_client_count_no_lock()
{
	return g_hash_table_size(client_contexts);
}

size_t net_nfc_server_gdbus_get_client_count()
{
	size_t result;

	pthread_mutex_lock(&context_lock);

	result = net_nfc_server_gdbus_get_client_count_no_lock();

	pthread_mutex_unlock(&context_lock);

	return result;
}

net_nfc_client_context_info_t *net_nfc_server_gdbus_get_client_context_no_lock(
		const char *id)
{
	net_nfc_client_context_info_t *result;

	result = g_hash_table_lookup(client_contexts, id);

	return result;
}

net_nfc_client_context_info_t *net_nfc_server_gdbus_get_client_context(
		const char *id)
{
	net_nfc_client_context_info_t *result;

	pthread_mutex_lock(&context_lock);

	result = net_nfc_server_gdbus_get_client_context_no_lock(id);

	pthread_mutex_unlock(&context_lock);

	return result;
}

void net_nfc_server_gdbus_add_client_context(const char *id,
		client_state_e state)
{
	pthread_mutex_lock(&context_lock);

	if (net_nfc_server_gdbus_get_client_context_no_lock(id) == NULL)
	{
		net_nfc_client_context_info_t *info = NULL;

		info = g_new0(net_nfc_client_context_info_t, 1);
		if (info != NULL)
		{
			pid_t pid;

			pid = net_nfc_server_gdbus_get_pid(id);
			NFC_DBG("added client id : [%s], pid [%d]", id, pid);

			info->id = g_strdup(id);
			info->pid = pid;
			info->pgid = getpgid(pid);
			info->state = state;
			info->launch_popup_state = NET_NFC_LAUNCH_APP_SELECT;
			info->launch_popup_state_no_check = NET_NFC_LAUNCH_APP_SELECT;

			g_hash_table_insert(client_contexts, (gpointer)info->id, (gpointer)info);

			NFC_DBG("current client count = [%d]",
					net_nfc_server_gdbus_get_client_count_no_lock());
		}
		else
		{
			NFC_ERR("alloc failed");
		}
	}

	pthread_mutex_unlock(&context_lock);
}

void net_nfc_server_gdbus_cleanup_client_context(const char *id)
{
	net_nfc_client_context_info_t *info;

	pthread_mutex_lock(&context_lock);

	info = net_nfc_server_gdbus_get_client_context_no_lock(id);
	if (info != NULL)
	{
		NFC_DBG("clean up client context, [%s, %d]", info->id, info->pid);

		g_hash_table_remove(client_contexts, id);

		_cleanup_client_context(info);

		NFC_DBG("current client count = [%d]",
				net_nfc_server_gdbus_get_client_count_no_lock());

		///* TODO : exit when no client */
		//if (net_nfc_server_gdbus_get_client_count_no_lock() == 0)
		//{
		//	/* terminate service */
		//	net_nfc_manager_quit();
		//}
	}

	pthread_mutex_unlock(&context_lock);
}

void net_nfc_server_gdbus_for_each_client_context(
		net_nfc_server_gdbus_for_each_client_cb cb, void *user_param)
{
	char *id;
	GHashTableIter iter;
	net_nfc_client_context_info_t *info;

	RET_IF(NULL == cb);

	pthread_mutex_lock(&context_lock);

	g_hash_table_iter_init(&iter, client_contexts);

	while (g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&info) == true)
	{
		cb(info, user_param);
	}

	pthread_mutex_unlock(&context_lock);
}

bool net_nfc_server_gdbus_check_client_is_running(const char *id)
{
	return (net_nfc_server_gdbus_get_client_context(id) != NULL);
}

client_state_e net_nfc_server_gdbus_get_client_state(const char *id)
{
	net_nfc_client_context_info_t *info;
	client_state_e state = NET_NFC_CLIENT_INACTIVE_STATE;

	pthread_mutex_lock(&context_lock);

	info = net_nfc_server_gdbus_get_client_context_no_lock(id);
	if (info != NULL)
		state = info->state;

	pthread_mutex_unlock(&context_lock);

	return state;
}

void net_nfc_server_gdbus_set_client_state(const char *id, client_state_e state)
{
	net_nfc_client_context_info_t *info;

	pthread_mutex_lock(&context_lock);

	info = net_nfc_server_gdbus_get_client_context_no_lock(id);
	if (info != NULL)
		info->state = state;

	pthread_mutex_unlock(&context_lock);
}

void net_nfc_server_gdbus_set_launch_state(const char *id,
		net_nfc_launch_popup_state_e popup_state,
		net_nfc_launch_popup_check_e check_foreground)
{
	net_nfc_client_context_info_t *info;

	pthread_mutex_lock(&context_lock);

	info = net_nfc_server_gdbus_get_client_context_no_lock(id);
	if (info != NULL)
	{
		if (check_foreground == CHECK_FOREGROUND)
			info->launch_popup_state = popup_state;
		else
			info->launch_popup_state_no_check = popup_state;
	}

	pthread_mutex_unlock(&context_lock);
}

net_nfc_launch_popup_state_e net_nfc_server_gdbus_get_launch_state(
		const char *id)
{
	net_nfc_client_context_info_t *info;
	net_nfc_launch_popup_state_e result = NET_NFC_LAUNCH_APP_SELECT;

	pthread_mutex_lock(&context_lock);

	info = net_nfc_server_gdbus_get_client_context_no_lock(id);
	if (info != NULL)
	{
		if (info->launch_popup_state_no_check  == NET_NFC_NO_LAUNCH_APP_SELECT)
			result = NET_NFC_NO_LAUNCH_APP_SELECT;
		else
			result = info->launch_popup_state;
	}

	pthread_mutex_unlock(&context_lock);

	return result;
}

net_nfc_launch_popup_state_e net_nfc_server_gdbus_get_client_popup_state(
		pid_t pid)
{
	char *id;
	GHashTableIter iter;
	net_nfc_client_context_info_t *info = NULL, *temp;
	net_nfc_launch_popup_state_e state = NET_NFC_LAUNCH_APP_SELECT;

	pthread_mutex_lock(&context_lock);

	g_hash_table_iter_init(&iter, client_contexts);
	while (g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&temp) == true)
	{
		if (NET_NFC_NO_LAUNCH_APP_SELECT == temp->launch_popup_state_no_check)
		{
			state = NET_NFC_NO_LAUNCH_APP_SELECT;
			break;
		}

		if (pid == temp->pgid)
		{
			info = temp;
			break;
		}
	}

	if (info != NULL)
		state = info->launch_popup_state;

	pthread_mutex_unlock(&context_lock);

	return state;
}

void net_nfc_server_gdbus_increase_se_count(const char *id)
{
	net_nfc_client_context_info_t *info;

	pthread_mutex_lock(&context_lock);

	info = net_nfc_server_gdbus_get_client_context_no_lock(id);
	if (info != NULL)
		info->ref_se++;

	pthread_mutex_unlock(&context_lock);
}

void net_nfc_server_gdbus_decrease_se_count(const char *id)
{
	net_nfc_client_context_info_t *info;

	pthread_mutex_lock(&context_lock);

	info = net_nfc_server_gdbus_get_client_context_no_lock(id);
	if (info != NULL)
		info->ref_se--;

	pthread_mutex_unlock(&context_lock);
}

bool net_nfc_server_gdbus_is_server_busy()
{
	bool result = false;

	pthread_mutex_lock(&context_lock);

	if (g_hash_table_size(client_contexts) > 0)
	{
		char *id;
		GHashTableIter iter;
		net_nfc_client_context_info_t *info;

		g_hash_table_iter_init(&iter, client_contexts);
		while (g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&info) == true)
		{
			if (info->ref_se > 0)
			{
				result = true;
				break;
			}
		}
	}

	pthread_mutex_unlock(&context_lock);

	return result;
}
