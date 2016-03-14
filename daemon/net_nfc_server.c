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
#include <glib.h>
#include <gio/gio.h>
#include <vconf.h>

#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_server.h"
#include "net_nfc_server_common.h"
#include "net_nfc_server_phdc.h"
#include "net_nfc_server_vconf.h"
#include "net_nfc_server_manager.h"
#include "net_nfc_server_util.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_server_tag.h"
#include "net_nfc_server_ndef.h"
#include "net_nfc_server_llcp.h"
#include "net_nfc_server_p2p.h"
#include "net_nfc_server_transceive.h"
#include "net_nfc_server_handover.h"
#include "net_nfc_server_se.h"
#include "net_nfc_server_snep.h"
#include "net_nfc_server_system_handler.h"
#include "net_nfc_server_context.h"

#include "neardal.h"

static gboolean use_daemon = FALSE;
static GMainLoop *loop = NULL;

static GDBusConnection *connection = NULL;

GOptionEntry option_entries[] = {
	{ "daemon", 'd', 0, G_OPTION_ARG_NONE, &use_daemon,
		"Use Daemon mode", NULL },
	{ NULL }
};

pid_t net_nfc_server_gdbus_get_pid(const char *name)
{
	GVariant *_ret;
	guint pid = 0;
	GError *error = NULL;

	_ret = g_dbus_connection_call_sync(connection,
			"org.freedesktop.DBus",
			"/org/freedesktop/DBus",
			"org.freedesktop.DBus",
			"GetConnectionUnixProcessID",
			g_variant_new("(s)", name),
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			&error);

	if (_ret != NULL)
	{
		g_variant_get(_ret, "(u)", &pid);
		g_variant_unref(_ret);
	}

	return pid;
}

void net_nfc_manager_quit()
{
	NFC_DBG("net_nfc_manager_quit kill the nfc-manager daemon!!");

	if (loop != NULL)
		g_main_loop_quit(loop);
}

static void _adapter_property_changed_cb(char *name, char *property,
					void *value, void *user_data)
{
	bool powered;

	if (!g_strcmp0(property, "Powered")) {
		if ((int *) value == 0)
			powered = false;
		else
			powered = true;

		NFC_DBG("power changed %d", powered);
		if (vconf_set_bool(VCONFKEY_NFC_STATE, powered) != 0)
			NFC_DBG("vconf_set_bool failed ");
	}
}

static bool net_nfc_neard_nfc_support(void)
{
	char **adapters = NULL;
	neardal_adapter *neard_adapter = NULL;
	int len;
	errorCode_t err;

	NFC_INFO("checking nfc support");
	err = neardal_get_adapters(&adapters, &len);
	if (err != NEARDAL_SUCCESS)
		return false;

	if (!(len > 0 && adapters != NULL))
		return false;

	err = neardal_get_adapter_properties(adapters[0], &neard_adapter);
	if (err == NEARDAL_SUCCESS && neard_adapter != NULL) {
		if (vconf_set_bool(VCONFKEY_NFC_STATE,
				neard_adapter->powered) != 0)
			NFC_ERR("VCONFKEY_NFC_STATE set to %d failed",
						neard_adapter->powered);

		if (neardal_set_cb_adapter_property_changed(
			_adapter_property_changed_cb, NULL) != NEARDAL_SUCCESS)
			NFC_ERR("Failed to register property changed cb");
	}

	if (adapters)
		neardal_free_array(&adapters);

	if (neard_adapter)
		neardal_free_adapter(neard_adapter);

	adapters = NULL;
	neard_adapter = NULL;

	return true;
}

int main(int argc, char *argv[])
{
	GError *error = NULL;
	gboolean use_daemon = FALSE;
	GOptionContext *option_context;

	net_nfc_change_log_tag();

	option_context = g_option_context_new("Nfc manager");
	g_option_context_add_main_entries(option_context, option_entries, NULL);

	if (g_option_context_parse(option_context, &argc, &argv, &error) == FALSE)
	{
		NFC_ERR("can not parse option: %s", error->message);
		g_error_free(error);

		g_option_context_free(option_context);
		return 0;
	}

	NFC_DBG("start nfc manager");
	NFC_INFO("use_daemon : %d", use_daemon);

	net_nfc_app_util_clean_storage(MESSAGE_STORAGE);

	if (net_nfc_neard_nfc_support() == false)
	{
		NFC_ERR("failed to detect NFC devices");

		if (vconf_set_bool(VCONFKEY_NFC_FEATURE, VCONFKEY_NFC_FEATURE_OFF) != 0)
			NFC_ERR("VCONFKEY_NFC_FEATURE set to %d failed", VCONFKEY_NFC_FEATURE_OFF);

		goto EXIT;
	}

	if (vconf_set_bool(VCONFKEY_NFC_FEATURE, VCONFKEY_NFC_FEATURE_ON) != 0)
		NFC_ERR("VCONFKEY_NFC_FEATURE set to %d failed", VCONFKEY_NFC_FEATURE_ON);

	net_nfc_server_vconf_init();

	loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);

EXIT :
	net_nfc_server_vconf_deinit();

	g_option_context_free(option_context);

	return 0;
}
