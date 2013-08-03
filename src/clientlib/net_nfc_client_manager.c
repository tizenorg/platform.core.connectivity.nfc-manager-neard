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

#ifndef NET_NFC_EXPORT_API
#define NET_NFC_EXPORT_API __attribute__((visibility("default")))
#endif

typedef struct _ManagerFuncData ManagerFuncData;

struct _ManagerFuncData
{
	gpointer callback;
	gpointer user_data;
};

static NetNfcGDbusManager *manager_proxy = NULL;
static gboolean activation_is_running = FALSE;

static ManagerFuncData *activated_func_data = NULL;

static int is_activated = -1;

static void manager_call_set_active_callback(GObject *source_object,
					GAsyncResult *res,
					gpointer user_data);

static void manager_call_get_server_state_callback(GObject *source_object,
						GAsyncResult *res,
						gpointer user_data);


static void manager_activated(NetNfcGDbusManager *manager,
				gboolean activated,
				gpointer user_data);


static void manager_call_set_active_callback(GObject *source_object,
					GAsyncResult *res,
					gpointer user_data)
{
	ManagerFuncData *func_data;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_manager_set_active_completed callback;
	gpointer data;

	activation_is_running = FALSE;

	if (net_nfc_gdbus_manager_call_set_active_finish(
				NET_NFC_GDBUS_MANAGER(source_object),
				res,
				&error) == FALSE)
	{
		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish call_set_active: %s",
			error->message);
		g_error_free(error);
	}

	func_data = user_data;
	if (func_data == NULL)
		return;

	if (func_data->callback == NULL)
	{
		g_free(func_data);
		return;
	}


	callback = (net_nfc_client_manager_set_active_completed)
			func_data->callback;
	data = func_data->user_data;

	callback(result, data);

	g_free(func_data);
}

static void manager_call_get_server_state_callback(GObject *source_object,
						GAsyncResult *res,
						gpointer user_data)
{
	ManagerFuncData *func_data;

	net_nfc_error_e result = NET_NFC_OK;
	guint out_state;
	GError *error = NULL;

	net_nfc_client_manager_get_server_state_completed callback;
	gpointer data;

	if (net_nfc_gdbus_manager_call_get_server_state_finish(
				NET_NFC_GDBUS_MANAGER(source_object),
				&out_state,
				res,
				&error) == FALSE)
	{

		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish get_server_state: %s",
			error->message);
		g_error_free(error);
	}

	func_data = user_data;
	if (func_data == NULL)
		return;

	if (func_data->callback == NULL)
	{
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_manager_get_server_state_completed)
			func_data->callback;
	data = func_data->user_data;

	callback(result, out_state, data);

	g_free(func_data);
}


static void manager_activated(NetNfcGDbusManager *manager,
				gboolean activated,
				gpointer user_data)
{
	bool state = false;

	INFO_MSG(">>> SIGNAL arrived");
	DEBUG_CLIENT_MSG("activated %d", activated);

	/* update current state */
	is_activated = (int)activated;

	if (activated_func_data == NULL)
		return;

	if (activated == TRUE)
		state = true;

	if (activated_func_data->callback)
	{
		net_nfc_client_manager_activated callback;
		gpointer user_data;

		callback = (net_nfc_client_manager_activated)
				(activated_func_data->callback);
		user_data = activated_func_data->user_data;

		callback(state, user_data);
	}
}

NET_NFC_EXPORT_API
void net_nfc_client_manager_set_activated(
			net_nfc_client_manager_activated callback,
			void *user_data)
{
	if (activated_func_data == NULL)
		activated_func_data = g_new0(ManagerFuncData, 1);

	activated_func_data->callback = (gpointer)callback;
	activated_func_data->user_data = user_data;
}

NET_NFC_EXPORT_API
void net_nfc_client_manager_unset_activated(void)
{
	if (activated_func_data == NULL)
	{
		DEBUG_ERR_MSG("manager_func_data is not initialized");
		return;
	}

	g_free(activated_func_data);
	activated_func_data = NULL;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_manager_set_active(int state,
			net_nfc_client_manager_set_active_completed callback,
			void *user_data)
{
	gboolean active = FALSE;
	ManagerFuncData *func_data;

	if (manager_proxy == NULL)
		return NET_NFC_UNKNOWN_ERROR;

	/* allow this function even nfc is off */

	if (activation_is_running == TRUE)
		return NET_NFC_BUSY;

	activation_is_running = TRUE;

	func_data = g_new0(ManagerFuncData, 1);
	if (func_data == NULL)
		return NET_NFC_UNKNOWN_ERROR;

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

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_manager_set_active_sync(int state)
{
	GError *error = NULL;

	if (manager_proxy == NULL)
		return NET_NFC_UNKNOWN_ERROR;

	/* allow this function even nfc is off */

	if (net_nfc_gdbus_manager_call_set_active_sync(manager_proxy,
						(gboolean)state,
						net_nfc_client_gdbus_get_privilege(),
						NULL,
						&error) == FALSE)
	{
		DEBUG_CLIENT_MSG("can not call SetActive: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_manager_get_server_state(
		net_nfc_client_manager_get_server_state_completed callback,
		void *user_data)
{
	ManagerFuncData *func_data;

	if (manager_proxy == NULL)
		return NET_NFC_UNKNOWN_ERROR;

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(ManagerFuncData, 1);
	if (func_data == NULL)
		return NET_NFC_UNKNOWN_ERROR;

	func_data->callback = (gpointer) callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_manager_call_get_server_state(manager_proxy,
					net_nfc_client_gdbus_get_privilege(),
					NULL,
					manager_call_get_server_state_callback,
					func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_manager_get_server_state_sync(
							unsigned int *state)
{
	GError *error = NULL;
	guint out_state;

	if (manager_proxy == NULL)
		return NET_NFC_UNKNOWN_ERROR;

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_manager_call_get_server_state_sync(manager_proxy,
							net_nfc_client_gdbus_get_privilege(),
							&out_state,
							NULL,
							&error) == FALSE)
	{
		DEBUG_CLIENT_MSG("can not call GetServerState: %s",
				error->message);
		g_error_free(error);

		return NET_NFC_UNKNOWN_ERROR;
	}

	*state = out_state;
	return NET_NFC_OK;

}

net_nfc_error_e net_nfc_client_manager_init(void)
{
	GError *error = NULL;

	if (manager_proxy)
	{
		DEBUG_CLIENT_MSG("Already initialized");
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
		DEBUG_ERR_MSG("Can not create proxy : %s", error->message);
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
		g_object_unref(manager_proxy);
		manager_proxy = NULL;
	}

	if (activated_func_data)
	{
		g_free(activated_func_data);
		activated_func_data = NULL;
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
