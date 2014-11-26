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

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_gdbus.h"

#include "net_nfc_server_common.h"
#include "net_nfc_server_context.h"
#include "net_nfc_server_tag.h"
#include "net_nfc_server_ndef.h"

typedef struct _ReadData ReadData;

struct _ReadData
{
	NetNfcGDbusNdef *ndef;
	GDBusMethodInvocation *invocation;
	guint32 handle;
};

typedef struct _WriteData WriteData;

struct _WriteData
{
	NetNfcGDbusNdef *ndef;
	GDBusMethodInvocation *invocation;
	guint32 handle;
	data_s data;
};

typedef struct _MakeReadOnlyData MakeReadOnlyData;

struct _MakeReadOnlyData
{
	NetNfcGDbusNdef *ndef;
	GDBusMethodInvocation *invocation;
	guint32 handle;
};

typedef struct _FormatData FormatData;

struct _FormatData
{
	NetNfcGDbusNdef *ndef;
	GDBusMethodInvocation *invocation;
	guint32 handle;
	data_s key;
};


static NetNfcGDbusNdef *ndef_skeleton = NULL;


static void ndef_read_thread_func(gpointer user_data)
{
	net_nfc_error_e result;
	GVariant *data_variant;
	data_s *read_data = NULL;
	ReadData *data = user_data;
	net_nfc_target_handle_s *handle;

	g_assert(data != NULL);
	g_assert(data->ndef != NULL);
	g_assert(data->invocation != NULL);

	handle = GUINT_TO_POINTER(data->handle);

	if (net_nfc_server_target_connected(handle) == true)
		net_nfc_controller_read_ndef(handle, &read_data, &result);
	else
		result = NET_NFC_TARGET_IS_MOVED_AWAY;

	data_variant = net_nfc_util_gdbus_data_to_variant(read_data);

	net_nfc_gdbus_ndef_complete_read(data->ndef, data->invocation, (gint)result,
			data_variant);

	if (read_data)
	{
		net_nfc_util_free_data(read_data);
		g_free(read_data);
	}

	g_object_unref(data->invocation);
	g_object_unref(data->ndef);

	g_free(data);
}

static void ndef_write_thread_func(gpointer user_data)
{
	net_nfc_error_e result;
	WriteData *data = user_data;
	net_nfc_target_handle_s *handle;

	g_assert(data != NULL);
	g_assert(data->ndef != NULL);
	g_assert(data->invocation != NULL);

	handle = GUINT_TO_POINTER(data->handle);

	if (net_nfc_server_target_connected(handle) == true)
		net_nfc_controller_write_ndef(handle, &data->data, &result);
	else
		result = NET_NFC_TARGET_IS_MOVED_AWAY;

	net_nfc_gdbus_ndef_complete_write(data->ndef, data->invocation, (gint)result);

	net_nfc_util_free_data(&data->data);

	g_object_unref(data->invocation);
	g_object_unref(data->ndef);

	g_free(data);
}

static void ndef_make_read_only_thread_func(gpointer user_data)
{
	net_nfc_error_e result;
	net_nfc_target_handle_s *handle;
	MakeReadOnlyData *data = user_data;

	g_assert(data != NULL);
	g_assert(data->ndef != NULL);
	g_assert(data->invocation != NULL);

	handle = GUINT_TO_POINTER(data->handle);

	if (net_nfc_server_target_connected(handle) == true)
		net_nfc_controller_make_read_only_ndef(handle, &result);
	else
		result = NET_NFC_TARGET_IS_MOVED_AWAY;

	net_nfc_gdbus_ndef_complete_make_read_only(data->ndef, data->invocation, (gint)result);

	g_object_unref(data->invocation);
	g_object_unref(data->ndef);

	g_free(data);
}

static void ndef_format_thread_func(gpointer user_data)
{
	net_nfc_error_e result;
	FormatData *data = user_data;
	net_nfc_target_handle_s *handle;

	g_assert(data != NULL);
	g_assert(data->ndef != NULL);
	g_assert(data->invocation != NULL);

	handle = GUINT_TO_POINTER(data->handle);

	if (net_nfc_server_target_connected(handle) == true)
		net_nfc_controller_format_ndef(handle, &data->key, &result);
	else
		result = NET_NFC_TARGET_IS_MOVED_AWAY;

	net_nfc_gdbus_ndef_complete_format(data->ndef, data->invocation, (gint)result);

	net_nfc_util_free_data(&data->key);

	g_object_unref(data->invocation);
	g_object_unref(data->ndef);

	g_free(data);
}

static gboolean ndef_handle_read(NetNfcGDbusNdef *ndef,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	ReadData *data;
	gboolean result;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::tag", "r");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_new0(ReadData, 1);
	if (NULL == data)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->ndef = g_object_ref(ndef);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;

	result = net_nfc_server_controller_async_queue_push(ndef_read_thread_func, data);
	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Ndef.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->invocation);
		g_object_unref(data->ndef);

		g_free(data);
	}

	return result;
}

static gboolean ndef_handle_write(NetNfcGDbusNdef *ndef,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		GVariant *arg_data,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	WriteData *data;
	gboolean result;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
			"nfc-manager::tag", "w");

	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_new0(WriteData, 1);
	if (NULL == data)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->ndef = g_object_ref(ndef);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;

	net_nfc_util_gdbus_variant_to_data_s(arg_data, &data->data);

	result = net_nfc_server_controller_async_queue_push(ndef_write_thread_func, data);
	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Ndef.ThreadError",
				"can not push to controller thread");

		net_nfc_util_free_data(&data->data);

		g_object_unref(data->invocation);
		g_object_unref(data->ndef);

		g_free(data);
	}

	return result;
}

static gboolean ndef_handle_make_read_only(NetNfcGDbusNdef *ndef,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	MakeReadOnlyData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::tag", "w");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_new0(MakeReadOnlyData, 1);
	if (NULL == data)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->ndef = g_object_ref(ndef);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;

	result = net_nfc_server_controller_async_queue_push(ndef_make_read_only_thread_func,
			data);
	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Ndef.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->invocation);
		g_object_unref(data->ndef);

		g_free(data);
	}

	return result;
}

static gboolean ndef_handle_format(NetNfcGDbusNdef *ndef,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		GVariant *arg_key,
		GVariant *smack_privilege,
		gpointer user_data)
{
	bool ret;
	gboolean result;
	FormatData *data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::tag", "w");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_new0(FormatData, 1);
	if (NULL == data)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	data->ndef = g_object_ref(ndef);
	data->invocation = g_object_ref(invocation);
	data->handle = arg_handle;
	net_nfc_util_gdbus_variant_to_data_s(arg_key, &data->key);

	result = net_nfc_server_controller_async_queue_push(ndef_format_thread_func, data);
	if (FALSE == result)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Ndef.ThreadError",
				"can not push to controller thread");

		net_nfc_util_free_data(&data->key);

		g_object_unref(data->invocation);
		g_object_unref(data->ndef);

		g_free(data);
	}

	return result;
}

gboolean net_nfc_server_ndef_init(GDBusConnection *connection)
{
	gboolean result;
	GError *error = NULL;

	if (ndef_skeleton)
		net_nfc_server_ndef_deinit();

	ndef_skeleton = net_nfc_gdbus_ndef_skeleton_new();

	g_signal_connect(ndef_skeleton, "handle-read", G_CALLBACK(ndef_handle_read), NULL);

	g_signal_connect(ndef_skeleton, "handle-write", G_CALLBACK(ndef_handle_write), NULL);

	g_signal_connect(ndef_skeleton, "handle-make-read-only",
			G_CALLBACK(ndef_handle_make_read_only), NULL);

	g_signal_connect(ndef_skeleton, "handle-format",
			G_CALLBACK(ndef_handle_format), NULL);

	result = g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(ndef_skeleton),
			connection, "/org/tizen/NetNfcService/Ndef", &error);
	if (FALSE == result)
	{
		g_error_free(error);

		net_nfc_server_ndef_deinit();
	}

	return TRUE;
}

void net_nfc_server_ndef_deinit(void)
{
	if (ndef_skeleton)
	{
		g_object_unref(ndef_skeleton);
		ndef_skeleton = NULL;
	}
}
