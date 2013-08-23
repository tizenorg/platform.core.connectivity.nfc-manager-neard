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

#include "Ecore_X.h"

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_system_handler.h"

typedef struct _PopupFuncData PopupFuncData;

struct _PopupFuncData
{
	gpointer callback;
	gpointer user_data;
};

static NetNfcGDbusPopup *popup_proxy = NULL;
static int popup_state = 0;

static void popup_set_active_callback(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);

static void popup_set_active_callback(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	PopupFuncData *func_data;

	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_popup_set_state_callback callback;
	gpointer data;

	if (net_nfc_gdbus_popup_call_set_finish(
				NET_NFC_GDBUS_POPUP(source_object),
				res,
				&error) == FALSE)
	{

		result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish popup_set_active: %s",
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

	callback = (net_nfc_client_popup_set_state_callback)
		func_data->callback;
	data = func_data->user_data;

	callback(result, data);

	g_free(func_data);
}

API net_nfc_error_e net_nfc_client_sys_handler_set_state(int state,
		net_nfc_client_popup_set_state_callback callback,
		void *user_data)
{
	gboolean active = FALSE;
	PopupFuncData *func_data;
	net_nfc_launch_popup_check_e focus_state = CHECK_FOREGROUND;

	if (popup_proxy == NULL )
		return NET_NFC_NOT_INITIALIZED;

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(PopupFuncData, 1);
	if (func_data == NULL )
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	if (state == true)
		active = TRUE;

	net_nfc_gdbus_popup_call_set(popup_proxy,
			active,
			focus_state,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			popup_set_active_callback,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_sys_handler_set_state_sync(int state)
{
	GError *error = NULL;
	net_nfc_launch_popup_check_e focus_state = CHECK_FOREGROUND;

	if (popup_proxy == NULL)
		return NET_NFC_NOT_INITIALIZED;

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_popup_call_set_sync(popup_proxy,
				(gboolean)state,
				focus_state,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				&error) == FALSE)
	{
		DEBUG_CLIENT_MSG("can not call SetActive: %s",
				error->message);
		g_error_free(error);

		return NET_NFC_IPC_FAIL;
	}

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_sys_handler_set_state_force(int state,
		net_nfc_client_popup_set_state_callback callback,
		void *user_data)
{
	gboolean active = FALSE;
	PopupFuncData *func_data;
	net_nfc_launch_popup_check_e focus_state = NO_CHECK_FOREGROUND;

	if (popup_proxy == NULL)
		return NET_NFC_NOT_INITIALIZED;

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_new0(PopupFuncData, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	if (state == true)
		active = TRUE;

	net_nfc_gdbus_popup_call_set(popup_proxy,
			active,
			focus_state,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			popup_set_active_callback,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_sys_handler_set_state_force_sync(int state)
{
	GError *error = NULL;
	net_nfc_launch_popup_check_e focus_state = NO_CHECK_FOREGROUND;

	if (popup_proxy == NULL)
		return NET_NFC_NOT_INITIALIZED;

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_popup_call_set_sync(popup_proxy,
				(gboolean)state,
				focus_state,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				&error) == FALSE)
	{
		DEBUG_CLIENT_MSG("can not call SetActive: %s",
				error->message);
		g_error_free(error);

		return NET_NFC_IPC_FAIL;
	}

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_sys_handler_set_launch_popup_state(
		int enable)
{
	net_nfc_error_e ret;

	popup_state = enable;

	if (enable)
	{
		ret = net_nfc_client_sys_handler_set_state_sync(
				NET_NFC_LAUNCH_APP_SELECT);
	}
	else
	{
		ret = net_nfc_client_sys_handler_set_state_sync(
				NET_NFC_NO_LAUNCH_APP_SELECT);
	}

	return ret;
}

API net_nfc_error_e net_nfc_client_sys_handler_set_launch_popup_state_force(
		int enable)
{
	net_nfc_error_e ret;

	popup_state = enable;

	if (enable)
	{
		ret = net_nfc_client_sys_handler_set_state_force_sync(
				NET_NFC_LAUNCH_APP_SELECT);
	}
	else
	{
		ret = net_nfc_client_sys_handler_set_state_force_sync(
				NET_NFC_NO_LAUNCH_APP_SELECT);
	}

	return ret;
}

API net_nfc_error_e net_nfc_client_sys_handler_get_launch_popup_state(
		int *state)
{
	if (state == NULL)
		return NET_NFC_NULL_PARAMETER;
#if 1
	*state = popup_state;
#else
	/* TODO : get state from server */
	GError *error = NULL;

	if (popup_proxy == NULL) {
		DEBUG_ERR_MSG("popup_proxy is null");

		return NET_NFC_NOT_INITIALIZED;
	}

	if (net_nfc_gdbus_popup_call_get_sync(popup_proxy,
				net_nfc_client_gdbus_get_privilege(),
				state,
				NULL,
				&error) == false) {
		DEBUG_CLIENT_MSG("net_nfc_gdbus_popup_call_get_sync failed: %s",
				error->message);
		g_error_free(error);

		return NET_NFC_IPC_FAIL;
	}
#endif
	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_client_sys_handler_init(void)
{
	GError *error = NULL;

	if (popup_proxy)
	{
		DEBUG_CLIENT_MSG("Already initialized");

		return NET_NFC_OK;
	}

	popup_proxy = net_nfc_gdbus_popup_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Popup",
			NULL,
			&error);
	if (popup_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not create proxy : %s", error->message);
		g_error_free(error);

		return NET_NFC_OPERATION_FAIL;
	}

	return NET_NFC_OK;
}

void net_nfc_client_sys_handler_deinit(void)
{
	if (popup_proxy)
	{
		g_object_unref(popup_proxy);
		popup_proxy = NULL;
	}
}
