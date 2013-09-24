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
#include <glib.h>
#include <gio/gio.h>
#include <vconf.h>

#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_server.h"
#include "net_nfc_server_common.h"
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

static gboolean use_daemon = FALSE;
static GMainLoop *loop = NULL;

static GDBusConnection *connection = NULL;
static guint subscribe_id;

GOptionEntry option_entries[] = {
	{ "daemon", 'd', 0, G_OPTION_ARG_NONE, &use_daemon,
		"Use Daemon mode", NULL },
	{ NULL }
};

pid_t net_nfc_server_gdbus_get_pid(const char *name)
{
	guint pid = 0;
	GError *error = NULL;
	GVariant *_ret;

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
	if (_ret != NULL) {
		g_variant_get(_ret, "(u)", &pid);
		g_variant_unref(_ret);
	}

	return pid;
}

static void _name_owner_changed(GDBusProxy *proxy,
		const gchar *name, const gchar *old_owner,
		const gchar *new_owner, void *user_data)
{
	if (name == NULL || old_owner == NULL || new_owner == NULL) {
		NFC_ERR("invalid parameter");

		return;
	}

	if (strlen(new_owner) == 0) {
		if (net_nfc_server_gdbus_check_client_is_running(old_owner)) {
			/* unregister service */
			net_nfc_server_llcp_unregister_services(old_owner);

			/* remove client context */
			net_nfc_server_gdbus_cleanup_client_context(old_owner);
		}
	}
}

static void _on_name_owner_changed(GDBusConnection *connection,
		const gchar *sender_name, const gchar *object_path,
		const gchar *interface_name, const gchar *signal_name,
		GVariant *parameters, gpointer user_data)
{
	gchar *name;
	gchar *old_owner;
	gchar *new_owner;

	g_variant_get(parameters,
			"(sss)",
			&name,
			&old_owner,
			&new_owner);

	_name_owner_changed((GDBusProxy *)connection,
			name, old_owner, new_owner, user_data);
}

static void _subscribe_name_owner_changed_event()
{
	if (connection == NULL)
		return;

	/* subscribe signal */
	subscribe_id = g_dbus_connection_signal_subscribe(connection,
			"org.freedesktop.DBus", /* bus name */
			"org.freedesktop.DBus", /* interface */
			"NameOwnerChanged", /* member */
			"/org/freedesktop/DBus", /* path */
			NULL, /* arg0 */
			G_DBUS_SIGNAL_FLAGS_NONE,
			_on_name_owner_changed,
			NULL, NULL);
}

static void _unsubscribe_name_owner_changed_event()
{
	if (connection == NULL)
		return;

	/* subscribe signal */
	if (subscribe_id > 0) {
		g_dbus_connection_signal_unsubscribe(connection, subscribe_id);
	}
}

static void net_nfc_server_gdbus_init(void)
{
	GError *error = NULL;

	if (connection)
		g_object_unref(connection);

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (connection == NULL)
	{
		NFC_ERR("Can not get connection %s", error->message);
		g_error_free (error);
		return;
	}

	net_nfc_server_gdbus_init_client_context();

	if (net_nfc_server_manager_init(connection) == FALSE)
	{
		NFC_ERR("Can not init manager");
		return;
	}

	if (net_nfc_server_tag_init(connection) == FALSE)
	{
		NFC_ERR("Can not init tag");
		return;
	}

	if (net_nfc_server_ndef_init(connection) == FALSE)
	{
		NFC_ERR("Can not init ndef");
		return;
	}

	if (net_nfc_server_llcp_init(connection) == FALSE)
	{
		NFC_ERR("Can not init llcp");
		return;
	}

	if (net_nfc_server_p2p_init(connection) == FALSE)
	{
		NFC_ERR("Can not init tag");
		return;
	}

	if (net_nfc_server_transceive_init(connection) == FALSE)
	{
		NFC_ERR("Can not initialize transceive");
		return;
	}

	if (net_nfc_server_handover_init(connection) == FALSE)
	{
		NFC_ERR("Can not initialize transceive");
		return;
	}

	if (net_nfc_server_se_init(connection) == FALSE)
	{
		NFC_ERR("Can not init Test");
		return;
	}

	if (net_nfc_server_snep_init(connection) == FALSE)
	{
		NFC_ERR("Can not init controller thread");
		return;
	}

	if (net_nfc_server_system_handler_init(connection) == FALSE)
	{
		NFC_ERR("Can not init controller thread");
		return;
	}

	if (net_nfc_server_controller_thread_init() == FALSE)
	{
		NFC_ERR("Can not init controller thread");
		return;
	}

	_subscribe_name_owner_changed_event();
}

static void net_nfc_server_gdbus_deinit(void)
{
	_unsubscribe_name_owner_changed_event();

	net_nfc_server_manager_deinit();
	net_nfc_server_tag_deinit();
	net_nfc_server_ndef_deinit();
	net_nfc_server_llcp_deinit();
	net_nfc_server_transceive_deinit();
	net_nfc_server_handover_deinit();
	net_nfc_server_se_deinit();
	net_nfc_server_snep_deinit();
	net_nfc_server_system_handler_deinit();

	net_nfc_server_gdbus_deinit_client_context();

	net_nfc_server_controller_thread_deinit();

	if (connection)
	{
		g_object_unref(connection);
		connection = NULL;
	}
}

void net_nfc_manager_quit()
{
	NFC_DBG("net_nfc_manager_quit kill the nfc-manager daemon!!");

	if (loop != NULL)
		g_main_loop_quit(loop);
}

static void on_bus_acquired(GDBusConnection *connection, const gchar *path,
		gpointer user_data)
{
	gint state;

	NFC_DBG("bus path : %s", path);

	net_nfc_server_gdbus_init();

	net_nfc_server_controller_init();

	if (vconf_get_bool(VCONFKEY_NFC_STATE, &state) != 0)
	{
		NFC_DBG("VCONFKEY_NFC_STATE is not exist");
		net_nfc_manager_quit();

		return;
	}

	net_nfc_server_vconf_init();

	if (state == 1)
		net_nfc_server_manager_set_active(TRUE);
#ifndef ESE_ALWAYS_ON
	else if (use_daemon == TRUE)
		net_nfc_server_controller_deinit();
#endif
}

static void on_name_acquired(GDBusConnection *connection, const gchar *name,
		gpointer user_data)
{
	NFC_INFO("name : %s", name);
}

static void on_name_lost(GDBusConnection *connnection, const gchar *name,
		gpointer user_data)
{
	NFC_INFO("name : %s", name);

	net_nfc_manager_quit();
}


int main(int argc, char *argv[])
{

	void *handle = NULL;
	guint id = 0;
	gboolean use_daemon = FALSE;
	GOptionContext *option_context;
	GError *error = NULL;

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

	handle = net_nfc_controller_onload();
	if (handle == NULL)
	{
		NFC_ERR("load plugin library is failed");

		if (vconf_set_bool(VCONFKEY_NFC_FEATURE, VCONFKEY_NFC_FEATURE_OFF) != 0)
			NFC_ERR("VCONFKEY_NFC_FEATURE set to %d failed", VCONFKEY_NFC_FEATURE_OFF);

		if (vconf_set_bool(VCONFKEY_NFC_STATE, 0) != 0)
			NFC_ERR("VCONFKEY_NFC_STATE set to %d failed", 0);

		goto EXIT;
	}

	if (vconf_set_bool(VCONFKEY_NFC_FEATURE, VCONFKEY_NFC_FEATURE_ON) != 0)
		NFC_ERR("VCONFKEY_NFC_FEATURE set to %d failed", VCONFKEY_NFC_FEATURE_ON);

	id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
			"org.tizen.NetNfcService",
			G_BUS_NAME_OWNER_FLAGS_NONE,
			on_bus_acquired,
			on_name_acquired,
			on_name_lost,
			NULL,
			NULL);

	loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);

EXIT :
	net_nfc_server_vconf_deinit();
	net_nfc_server_controller_deinit();
	net_nfc_server_gdbus_deinit();

	if (id)
		g_bus_unown_name(id);

	net_nfc_controller_unload(handle);

	g_option_context_free(option_context);

	return 0;
}