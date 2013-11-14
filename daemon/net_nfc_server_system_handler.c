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
#include "net_nfc_server_common.h"
#include "net_nfc_server_context.h"
#include "net_nfc_server_system_handler.h"


static NetNfcGDbusPopup *popup_skeleton = NULL;

static gboolean popup_handle_set(NetNfcGDbusPopup *popup_manager,
		GDBusMethodInvocation *invocation,
		gint state,
		gint focus_state,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager", "w");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	net_nfc_server_gdbus_set_launch_state(g_dbus_method_invocation_get_sender(invocation),
			state, focus_state);

	net_nfc_gdbus_popup_complete_set(popup_manager, invocation, NET_NFC_OK);

	return TRUE;
}

static gboolean popup_handle_get(NetNfcGDbusPopup *popup_manager,
		GDBusMethodInvocation *invocation, GVariant *smack_privilege, gpointer user_data)
{
	bool ret;
	gboolean state;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager", "r");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	state = net_nfc_server_gdbus_get_launch_state(
			g_dbus_method_invocation_get_sender(invocation));

	net_nfc_gdbus_popup_complete_get(popup_manager, invocation, NET_NFC_OK, state);

	return TRUE;
}

gboolean net_nfc_server_system_handler_init(GDBusConnection *connection)
{
	gboolean result;
	GError *error = NULL;

	if (popup_skeleton)
		g_object_unref(popup_skeleton);

	popup_skeleton = net_nfc_gdbus_popup_skeleton_new();
	if (NULL == popup_skeleton)
	{
		NFC_ERR("Failed to allocate popup skeleton");

		return FALSE;
	}

	g_signal_connect(popup_skeleton, "handle-set", G_CALLBACK(popup_handle_set), NULL);

	g_signal_connect(popup_skeleton, "handle-get", G_CALLBACK(popup_handle_get), NULL);

	result = g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(popup_skeleton),
			connection, "/org/tizen/NetNfcService/Popup", &error);
	if (FALSE == result)
	{
		NFC_ERR("Can not skeleton_export %s", error->message);

		g_error_free(error);

		net_nfc_server_system_handler_deinit();
	}

	return result;
}

void net_nfc_server_system_handler_deinit(void)
{
	if (popup_skeleton)
	{
		g_object_unref(popup_skeleton);
		popup_skeleton = NULL;
	}
}
