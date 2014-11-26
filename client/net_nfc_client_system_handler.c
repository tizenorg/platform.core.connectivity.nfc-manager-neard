/*
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 				 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef USE_X11
#include <Ecore_X.h>
#endif
#ifdef USE_WAYLAND
#include <Ecore.h>
#include <Ecore_Wayland.h>
#endif

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_system_handler.h"



static NetNfcGDbusPopup *popup_proxy = NULL;
static int popup_state = 1;

static void popup_set_active_callback(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	net_nfc_client_popup_set_state_callback callback;
	NetNfcCallback *func_data = (NetNfcCallback *)user_data;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_popup_call_set_finish(NET_NFC_GDBUS_POPUP(source_object),
				&result, res, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Can not finish popup_set_active: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		callback = (net_nfc_client_popup_set_state_callback)func_data->callback;

		callback(result, func_data->user_data);
	}

	g_free(func_data);
}

API net_nfc_error_e net_nfc_client_sys_handler_set_state(int state,
		net_nfc_client_popup_set_state_callback callback, void *user_data)
{
	NetNfcCallback *func_data;

	RETV_IF(NULL == popup_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	func_data = g_try_new0(NetNfcCallback, 1);
	if (NULL == func_data)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_popup_call_set(popup_proxy,
			state,
			CHECK_FOREGROUND,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			popup_set_active_callback,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_sys_handler_set_state_sync(int state)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == popup_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	ret = net_nfc_gdbus_popup_call_set_sync(popup_proxy,
				state,
				CHECK_FOREGROUND,
				net_nfc_client_gdbus_get_privilege(),
				&result,
				NULL,
				&error);

	if (FALSE == ret)
	{
		NFC_ERR("can not call SetActive: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_sys_handler_set_state_force(int state,
		net_nfc_client_popup_set_state_callback callback, void *user_data)
{
	NetNfcCallback *func_data;

	RETV_IF(NULL == popup_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	func_data = g_try_new0(NetNfcCallback, 1);
	if (NULL == func_data)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_popup_call_set(popup_proxy,
			state,
			NO_CHECK_FOREGROUND,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			popup_set_active_callback,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_sys_handler_set_state_force_sync(int state)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;

	RETV_IF(NULL == popup_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	ret = net_nfc_gdbus_popup_call_set_sync(popup_proxy,
				state,
				NO_CHECK_FOREGROUND,
				net_nfc_client_gdbus_get_privilege(),
				&result,
				NULL,
				&error);

	if (FALSE == ret)
	{
		NFC_ERR("can not call SetActive: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_sys_handler_set_launch_popup_state(
		int enable)
{
	popup_state = enable;

	/*
	 * In previous version we have vconf key to manintain the popup state
	 * now it is deprecated, new implement is not finished, just skip it now
	 * previous operation vconf_set_bool(NET_NFC_DISABLE_LAUNCH_POPUP_KEY, enable)
	 */

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_sys_handler_set_launch_popup_state_force(
		int enable)
{
	popup_state = enable;

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_sys_handler_get_launch_popup_state(
		int *state)
{
	RETV_IF(NULL == state, NET_NFC_NULL_PARAMETER);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	if (popup_state == true)
		*state = NET_NFC_LAUNCH_APP_SELECT;
	else
		*state = NET_NFC_NO_LAUNCH_APP_SELECT;

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_client_sys_handler_init(void)
{
	GError *error = NULL;

	if (popup_proxy)
	{
		NFC_WARN("Already initialized");
		return NET_NFC_OK;
	}

	popup_proxy = net_nfc_gdbus_popup_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Popup",
			NULL,
			&error);
	if (NULL == popup_proxy)
	{
		NFC_ERR("Can not create proxy : %s", error->message);
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
