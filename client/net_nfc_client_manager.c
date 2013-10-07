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

#include "net_nfc_debug_internal.h"

#include "net_nfc_gdbus.h"
#include "net_nfc_client.h"
#include "net_nfc_client_context.h"
#include "net_nfc_client_manager.h"

#define DEACTIVATE_DELAY	500 /* ms */

typedef struct _ManagerFuncData ManagerFuncData;

struct _ManagerFuncData
{
	gpointer callback;
	gpointer user_data;
	net_nfc_error_e result;
};

static NetNfcGDbusManager *manager_proxy = NULL;
static gboolean activation_is_running = FALSE;
static ManagerFuncData activated_func_data;
static int is_activated = -1;
static guint timeout_id[2];

static void manager_call_set_active_callback(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);

static void manager_call_get_server_state_callback(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);


static void manager_activated(NetNfcGDbusManager *manager,
		gboolean activated,
		gpointer user_data);


static gboolean _set_activate_time_elapsed_callback(gpointer user_data)
{
	ManagerFuncData *func_data = (ManagerFuncData *)user_data;
	net_nfc_client_manager_set_active_completed callback;

	g_assert(func_data != NULL);

	timeout_id[0] = 0;

	callback = (net_nfc_client_manager_set_active_completed)func_data->callback;

	callback(func_data->result, func_data->user_data);

	g_free(func_data);

	return false;
}

static void manager_call_set_active_callback(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	ManagerFuncData *func_data = (ManagerFuncData *)user_data;
	net_nfc_error_e result;
	GError *error = NULL;

	g_assert(user_data != NULL);

	activation_is_running = FALSE;

	if (net_nfc_gdbus_manager_call_set_active_finish(NET_NFC_GDBUS_MANAGER(source_object),
				&result, res, &error) == FALSE)
	{
		NFC_ERR("Can not finish call_set_active: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	func_data->result = result;
	net_nfc_client_get_nfc_state(&is_activated);

	if (is_activated == false)
	{
		//TODO : wait several times
		timeout_id[0] = g_timeout_add(DEACTIVATE_DELAY,
				_set_activate_time_elapsed_callback,
				func_data);
	}
	else
	{
		g_main_context_invoke(NULL,
				_set_activate_time_elapsed_callback,
				func_data);
	}
}

static void manager_call_get_server_state_callback(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NetNfcCallback *func_data = (NetNfcCallback *)user_data;
	net_nfc_error_e result;
	guint out_state = 0;
	GError *error = NULL;

	g_assert(user_data != NULL);

	if (net_nfc_gdbus_manager_call_get_server_state_finish(
				NET_NFC_GDBUS_MANAGER(source_object),
				&result,
				&out_state,
				res,
				&error) == FALSE)
	{
		NFC_ERR("Can not finish get_server_state: %s",
				error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_manager_get_server_state_completed callback =
			(net_nfc_client_manager_get_server_state_completed)func_data->callback;

		callback(result, out_state, func_data->user_data);
	}

	g_free(func_data);
}

static gboolean _activated_time_elapsed_callback(gpointer user_data)
{
	net_nfc_client_manager_activated callback =
		(net_nfc_client_manager_activated)activated_func_data.callback;

	timeout_id[1] = 0;

	callback(is_activated, activated_func_data.user_data);

	return false;
}

static void manager_activated(NetNfcGDbusManager *manager, gboolean activated,
		gpointer user_data)
{
	NFC_INFO(">>> SIGNAL arrived : activated %d", activated);

	/* update current state */
	is_activated = (int)activated;

	if (activated_func_data.callback != NULL)
	{
		if (is_activated == false)
		{
			/* FIXME : wait several times */
			timeout_id[1] = g_timeout_add(DEACTIVATE_DELAY,
					_activated_time_elapsed_callback, NULL);
		}
		else
		{
			g_main_context_invoke(NULL, _activated_time_elapsed_callback, NULL);
		}
	}
}

API void net_nfc_client_manager_set_activated(
		net_nfc_client_manager_activated callback, void *user_data)
{
	if (callback == NULL)
		return;

	activated_func_data.callback = callback;
	activated_func_data.user_data = user_data;
}

API void net_nfc_client_manager_unset_activated(void)
{
	activated_func_data.callback = NULL;
	activated_func_data.user_data = NULL;
}

API net_nfc_error_e net_nfc_client_manager_set_active(int state,
		net_nfc_client_manager_set_active_completed callback, void *user_data)
{
	gboolean active = FALSE;
	ManagerFuncData *func_data;

	if (manager_proxy == NULL)
		return NET_NFC_NOT_INITIALIZED;

	/* allow this function even nfc is off */

	if (activation_is_running == TRUE)
		return NET_NFC_BUSY;

	activation_is_running = TRUE;

	func_data = g_try_new0(ManagerFuncData, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	if (state == true)
		active = TRUE;

	net_nfc_gdbus_manager_call_set_active(manager_proxy,
			active,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			manager_call_set_active_callback,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_manager_set_active_sync(int state)
{
	net_nfc_error_e out_result = NET_NFC_OK;
	GError *error = NULL;

	if (manager_proxy == NULL)
		return NET_NFC_NOT_INITIALIZED;

	/* allow this function even nfc is off */

	if (net_nfc_gdbus_manager_call_set_active_sync(manager_proxy,
				(gboolean)state,
				net_nfc_client_gdbus_get_privilege(),
				&out_result,
				NULL,
				&error) == FALSE)
	{
		NFC_ERR("can not call SetActive: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	return out_result;
}

API net_nfc_error_e net_nfc_client_manager_get_server_state(
		net_nfc_client_manager_get_server_state_completed callback, void *user_data)
{
	NetNfcCallback *func_data;

	if (manager_proxy == NULL)
		return NET_NFC_NOT_INITIALIZED;

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_try_new0(NetNfcCallback, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer) callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_manager_call_get_server_state(manager_proxy,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			manager_call_get_server_state_callback,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_manager_get_server_state_sync(
		unsigned int *state)
{
	net_nfc_error_e out_result = NET_NFC_OK;
	guint out_state = 0;
	GError *error = NULL;

	if (state == NULL)
		return NET_NFC_NULL_PARAMETER;

	*state = 0;

	if (manager_proxy == NULL)
		return NET_NFC_NOT_INITIALIZED;

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_manager_call_get_server_state_sync(manager_proxy,
				net_nfc_client_gdbus_get_privilege(),
				&out_result,
				&out_state,
				NULL,
				&error) == TRUE)
	{
		*state = out_state;
	}
	else
	{
		NFC_ERR("can not call GetServerState: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	return out_result;
}

net_nfc_error_e net_nfc_client_manager_init(void)
{
	GError *error = NULL;

	if (manager_proxy)
	{
		NFC_WARN("Already initialized");
		return NET_NFC_OK;
	}

	manager_proxy = net_nfc_gdbus_manager_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Manager",
			NULL,
			&error);

	if (manager_proxy == NULL)
	{
		NFC_ERR("Can not create proxy : %s", error->message);
		g_error_free(error);

		return NET_NFC_UNKNOWN_ERROR;
	}

	g_signal_connect(manager_proxy, "activated",
			G_CALLBACK(manager_activated), NULL);

	return NET_NFC_OK;
}

void net_nfc_client_manager_deinit(void)
{
	if (manager_proxy)
	{
		int i;

		for (i = 0; i < 2; i++) {
			if (timeout_id[i] > 0) {
				g_source_remove(timeout_id[i]);
				timeout_id[i] = 0;
			}
		}

		g_object_unref(manager_proxy);
		manager_proxy = NULL;
	}
}

/* internal function */
bool net_nfc_client_manager_is_activated()
{
	if (is_activated < 0) {
		net_nfc_client_get_nfc_state(&is_activated);
	}

	return is_activated;
}
