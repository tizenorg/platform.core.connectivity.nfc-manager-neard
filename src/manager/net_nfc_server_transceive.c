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

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_controller_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_server_common.h"
#include "net_nfc_server_tag.h"
#include "net_nfc_server_context_internal.h"
#include "net_nfc_server_transceive.h"


static NetNfcGDbusTransceive *transceive_skeleton = NULL;

static void transceive_thread_func(gpointer user_data);

static void transceive_data_thread_func(gpointer user_data);

static gboolean transceive_handle(NetNfcGDbusTransceive *transceive,
	GDBusMethodInvocation *invocation,
	guint handle,
	guint dev_type,
	GVariant *arg_data,
	GVariant *smack_privilege,
	gpointer user_data);

static gboolean transceive_data_handle(NetNfcGDbusTransceive *transceive,
	GDBusMethodInvocation *invocation,
	guint handle,
	guint dev_type,
	GVariant *arg_data,
	GVariant *smack_privilege,
	gpointer user_data);


typedef struct _TransceiveSendData TransceiveSendData;

struct _TransceiveSendData
{
	NetNfcGDbusTransceive *transceive;
	GDBusMethodInvocation *invocation;
	guint transceive_handle;
	net_nfc_transceive_info_s transceive_info;
};

static void transceive_data_thread_func(gpointer user_data)
{
	TransceiveSendData *transceive_data = (TransceiveSendData*)user_data;
	net_nfc_target_handle_s *handle =
		(net_nfc_target_handle_s *)transceive_data->transceive_handle;
	net_nfc_error_e result;
	data_s *data = NULL;
	GVariant *resp_data = NULL;

	/* use assert because it was checked in handle function */
	g_assert(transceive_data != NULL);
	g_assert(transceive_data->transceive != NULL);
	g_assert(transceive_data->invocation != NULL);

	if (net_nfc_server_target_connected(handle) == true)
	{
		DEBUG_SERVER_MSG("call transceive");

		if (net_nfc_controller_transceive(handle,
			&transceive_data->transceive_info,
			&data,
			&result) == true)
		{
			if (data != NULL)
			{
				DEBUG_SERVER_MSG("Transceive data received [%d]",
					data->length);
			}
		}
	}
	else
	{
		result = NET_NFC_TARGET_IS_MOVED_AWAY;
	}

	DEBUG_SERVER_MSG("transceive result : %d", result);

	resp_data = net_nfc_util_gdbus_data_to_variant(data);

	net_nfc_gdbus_transceive_complete_transceive_data(
		transceive_data->transceive,
		transceive_data->invocation,
		(gint)result,
		resp_data);

	if (data)
	{
		g_free(data->buffer);
		g_free(data);
	}

	net_nfc_util_free_data(&transceive_data->transceive_info.trans_data);

	g_object_unref(transceive_data->invocation);
	g_object_unref(transceive_data->transceive);

	g_free(transceive_data);
}

static gboolean transceive_data_handle(NetNfcGDbusTransceive *transceive,
	GDBusMethodInvocation *invocation,
	guint handle,
	guint dev_type,
	GVariant *arg_data,
	GVariant *smack_privilege,
	gpointer user_data)
{
	TransceiveSendData *data;
	gboolean result;

	INFO_MSG(">>> REQUEST from [%s]",
		g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
		smack_privilege,
		"nfc-manager",
		"rw") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return FALSE;
	}

	data = g_new0(TransceiveSendData, 1);
	if (data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
			"org.tizen.NetNfcService.AllocationError",
			"Can not allocate memory");

		return FALSE;
	}

	data->transceive = g_object_ref(transceive);
	data->invocation = g_object_ref(invocation);
	data->transceive_handle = handle;
	data->transceive_info.dev_type = dev_type;
	net_nfc_util_gdbus_variant_to_data_s(arg_data,
		&data->transceive_info.trans_data);

	result = net_nfc_server_controller_async_queue_push(
		transceive_data_thread_func, data);
	if (result == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
			"org.tizen.NetNfcService.Transceive.ThreadError",
			"can not push to controller thread");

		net_nfc_util_free_data(&data->transceive_info.trans_data);

		g_object_unref(data->transceive);
		g_object_unref(data->invocation);

		g_free(data);
	}

	return result;
}

static void transceive_thread_func(gpointer user_data)
{
	TransceiveSendData *transceive_data = (TransceiveSendData *)user_data;
	net_nfc_target_handle_s *handle =
		(net_nfc_target_handle_s *)transceive_data->transceive_handle;
	net_nfc_error_e result = NET_NFC_OK;
	data_s *data = NULL;

	/* use assert because it was checked in handle function */
	g_assert(transceive_data != NULL);
	g_assert(transceive_data->transceive != NULL);
	g_assert(transceive_data->invocation != NULL);

	if (net_nfc_server_target_connected(handle) == true)
	{
		DEBUG_MSG("call transceive");

		if (net_nfc_controller_transceive(handle,
			&transceive_data->transceive_info,
			&data,
			&result) == true)
		{
			if (data != NULL)
			{
				DEBUG_SERVER_MSG(
					"Transceive data received [%d]",
					data->length);

				/* free resource because it doesn't need */
				g_free(data->buffer);
				g_free(data);
			}
		}
	}
	else
	{
		DEBUG_SERVER_MSG("target is not connected");

		result = NET_NFC_TARGET_IS_MOVED_AWAY;
	}

	DEBUG_SERVER_MSG("transceive result : %d", result);

	net_nfc_gdbus_transceive_complete_transceive(
		transceive_data->transceive,
		transceive_data->invocation,
		(gint)result);

	net_nfc_util_free_data(&transceive_data->transceive_info.trans_data);

	g_object_unref(transceive_data->invocation);
	g_object_unref(transceive_data->transceive);

	g_free(transceive_data);
}

static gboolean transceive_handle(NetNfcGDbusTransceive *transceive,
	GDBusMethodInvocation *invocation,
	guint handle,
	guint dev_type,
	GVariant *arg_data,
	GVariant *smack_privilege,
	gpointer user_data)
{
	TransceiveSendData *data;
	gboolean result;

	INFO_MSG(">>> REQUEST from [%s]",
		g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
		smack_privilege,
		"nfc-manager",
		"rw") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return FALSE;
	}

	data = g_new0(TransceiveSendData, 1);
	if (data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
		"org.tizen.NetNfcService.AllocationError",
		"Can not allocate memory");

		return FALSE;
	}

	data->transceive = g_object_ref(transceive);
	data->invocation = g_object_ref(invocation);
	data->transceive_handle = handle;
	data->transceive_info.dev_type = dev_type;
	net_nfc_util_gdbus_variant_to_data_s(arg_data,
		&data->transceive_info.trans_data);

	result = net_nfc_server_controller_async_queue_push(
		transceive_thread_func, data);
	if (result == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
			"org.tizen.NetNfcService.Transceive.ThreadError",
			"can not push to controller thread");

		net_nfc_util_free_data(&data->transceive_info.trans_data);

		g_object_unref(data->transceive);
		g_object_unref(data->invocation);

		g_free(data);
	}

	return result;
}


gboolean net_nfc_server_transceive_init(GDBusConnection *connection)
{
	GError *error = NULL;
	gboolean result;

	if (transceive_skeleton)
		g_object_unref(transceive_skeleton);

	transceive_skeleton = net_nfc_gdbus_transceive_skeleton_new();

	g_signal_connect(transceive_skeleton,
			"handle-transceive-data",
			G_CALLBACK(transceive_data_handle),
			NULL);

	g_signal_connect(transceive_skeleton,
			"handle-transceive",
			G_CALLBACK(transceive_handle),
			NULL);

	result = g_dbus_interface_skeleton_export(
		G_DBUS_INTERFACE_SKELETON(transceive_skeleton),
		connection,
		"/org/tizen/NetNfcService/Transceive",
		&error);
	if (result == FALSE)
	{
		g_error_free(error);
		g_object_unref(transceive_skeleton);
		transceive_skeleton = NULL;
	}

	return result;
}

void net_nfc_server_transceive_deinit(void)
{
	if (transceive_skeleton)
	{
		g_object_unref(transceive_skeleton);
		transceive_skeleton = NULL;
	}
}
