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


#include "net_nfc_server_common.h"
#include "net_nfc_server_process_handover.h"
#include "net_nfc_server_handover.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_server_context.h"

static NetNfcGDbusHandover *handover_skeleton = NULL;

static void handover_request_thread_func(gpointer user_data);

static gboolean handover_handle_request(NetNfcGDbusHandover *hdover,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		gint32 arg_type,
		GVariant *smack_privilege,
		gpointer user_data);

static void handover_request_thread_func(gpointer user_data)
{
	HandoverRequestData *handover_data;
	net_nfc_target_handle_s *handle;
	net_nfc_error_e error = NET_NFC_OK;

	handover_data = (HandoverRequestData *)user_data;

	if (handover_data == NULL)
	{
		DEBUG_ERR_MSG("cannot send Handover data");

		return;
	}

	if (handover_data->handoverobj == NULL)
	{
		DEBUG_ERR_MSG("can not get Handover object");

		if (handover_data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					handover_data->invocation,
					"org.tizen.NetNfcService.Handover.DataError",
					"Handover invocation is NULL");

			g_object_unref(handover_data->invocation);
		}

		g_free(handover_data);

		return;
	}

	handle = GUINT_TO_POINTER(handover_data->handle);

	if ((error = net_nfc_server_handover_default_client_start(
					handle,
					(void *)handover_data)) != NET_NFC_OK)
	{
		if (handover_data->invocation)
		{
			g_dbus_method_invocation_return_dbus_error(
					handover_data->invocation,
					"org.tizen.NetNfcService.Handover.SendError",
					"handover operation unsuccessfull");

			g_object_unref(handover_data->invocation);
		}

		g_object_unref(handover_data->handoverobj);
		g_free(handover_data);
	}
}

static gboolean handover_handle_request(NetNfcGDbusHandover *hdover,
		GDBusMethodInvocation *invocation,
		guint32 arg_handle,
		gint32 arg_type,
		GVariant *smack_privilege,
		gpointer user_data)
{
	HandoverRequestData *data;

	INFO_MSG(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager::p2p",
				"rw") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return TRUE;
	}

	data = g_new0(HandoverRequestData,1);
	if(data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");
		return FALSE;
	}

	data->handoverobj = g_object_ref(hdover);
	data->handle = arg_handle;
	data->type = arg_type;
	data->invocation = g_object_ref(invocation);

	if (net_nfc_server_controller_async_queue_push(
				handover_request_thread_func, data) == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Handover.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->handoverobj);
		g_object_unref(data->invocation);
		g_free(data);

		return FALSE;
	}

	return TRUE;
}

gboolean net_nfc_server_handover_init(GDBusConnection *connection)
{
	GError *error = NULL;

	if (handover_skeleton)
		g_object_unref(handover_skeleton);

	handover_skeleton = net_nfc_gdbus_handover_skeleton_new();

	g_signal_connect(handover_skeleton,
			"handle-request",
			G_CALLBACK(handover_handle_request),
			NULL);

	if (g_dbus_interface_skeleton_export(
				G_DBUS_INTERFACE_SKELETON(handover_skeleton),
				connection,
				"/org/tizen/NetNfcService/Handover",
				&error) == FALSE)
	{
		g_error_free(error);
		g_object_unref(handover_skeleton);
		handover_skeleton = NULL;

		return FALSE;
	}

	return TRUE;
}

void net_nfc_server_handover_deinit(void)
{
	if (handover_skeleton)
	{
		g_object_unref(handover_skeleton);
		handover_skeleton = NULL;
	}
}
