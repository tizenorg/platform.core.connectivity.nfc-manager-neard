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
#include "net_nfc_util_internal.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_controller_internal.h"
#include "net_nfc_app_util_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_server_common.h"
#include "net_nfc_server_context_internal.h"
#include "net_nfc_server_tag.h"
#include "net_nfc_server_p2p.h"


typedef struct _CurrentTagInfoData CurrentTagInfoData;

struct _CurrentTagInfoData
{
	NetNfcGDbusTag *tag;
	GDBusMethodInvocation *invocation;
};

typedef struct _WatchDogData WatchDogData;

struct _WatchDogData
{
	net_nfc_target_type_e dev_type;
	net_nfc_target_handle_s *handle;
};


static gboolean tag_is_isp_dep_ndef_formatable(net_nfc_target_handle_s *handle,
					int dev_type);

static gboolean tag_read_ndef_message(net_nfc_target_handle_s *handle,
				int dev_type,
				data_s **read_ndef);

static void tag_watchdog_thread_func(gpointer user_data);

static void tag_get_current_tag_info_thread_func(gpointer user_data);

static void tag_slave_target_detected_thread_func(gpointer user_data);


/* methods */
static gboolean tag_handle_is_tag_connected(NetNfcGDbusTag *tag,
					GDBusMethodInvocation *invocation,
					GVariant *smack_privilege,
					gpointer user_data);

static gboolean tag_handle_get_current_tag_info(NetNfcGDbusTag *tag,
					GDBusMethodInvocation *invocation,
					GVariant *smack_privilege,
					gpointer user_data);

static gboolean tag_handle_get_current_target_handle(NetNfcGDbusTag *tag,
					GDBusMethodInvocation *invocation,
					GVariant *smack_privilege,
					gpointer user_data);

static NetNfcGDbusTag *tag_skeleton = NULL;

static net_nfc_current_target_info_s *current_target_info = NULL;

static gboolean tag_is_isp_dep_ndef_formatable(net_nfc_target_handle_s *handle,
					int dev_type)
{
	uint8_t cmd[] = { 0x90, 0x60, 0x00, 0x00, 0x00 };

	net_nfc_transceive_info_s info;
	data_s *response = NULL;
	net_nfc_error_e error = NET_NFC_OK;
	gboolean result = false;

	info.dev_type = dev_type;
	info.trans_data.buffer = cmd;
	info.trans_data.length = sizeof(cmd);

	if (net_nfc_controller_transceive(handle,
				&info,
				&response,
				&error) == false)
	{
		DEBUG_ERR_MSG("net_nfc_controller_transceive is failed");

		return result;
	}

	if (response != NULL)
	{
		if (response->length == 9 &&
			response->buffer[7] == (uint8_t)0x91 &&
			response->buffer[8] == (uint8_t)0xAF)
		{
			result =  TRUE;
		}

		net_nfc_util_free_data(response);
		g_free(response);
	}
	else
	{
		DEBUG_ERR_MSG("response is NULL");
	}

	return result;
}

static gboolean tag_read_ndef_message(net_nfc_target_handle_s *handle,
				int dev_type,
				data_s **read_ndef)
{
	net_nfc_error_e result = NET_NFC_OK;
	data_s *temp = NULL;

	if (handle == NULL)
		return FALSE;

	if (read_ndef == NULL)
		return FALSE;

	*read_ndef = NULL;

	if (dev_type == NET_NFC_MIFARE_DESFIRE_PICC)
	{
		if (tag_is_isp_dep_ndef_formatable(handle, dev_type) == FALSE)
		{
			DEBUG_ERR_MSG(
				"DESFIRE : ISO-DEP ndef not formatable");
			return FALSE;
		}

		DEBUG_SERVER_MSG("DESFIRE : ISO-DEP ndef formatable");

		if (net_nfc_controller_connect(handle, &result) == false)
		{
			DEBUG_ERR_MSG("%s failed, & retry polling!!",
					"net_nfc_controller_connect");

			if (net_nfc_controller_configure_discovery(
						NET_NFC_DISCOVERY_MODE_RESUME,
						NET_NFC_ALL_ENABLE,
						&result) == false)
			{
				net_nfc_controller_exception_handler();
			}
			return FALSE;
		}
	}

	if (net_nfc_controller_read_ndef(handle, &temp, &result) == false)
	{
		DEBUG_ERR_MSG("%s failed",
				"net_nfc_controller_read_ndef");
		return FALSE;
	}

	DEBUG_SERVER_MSG("%s success",
			"net_nfc_controller_read_ndef");

	if (dev_type == NET_NFC_MIFARE_DESFIRE_PICC)
	{
		if (net_nfc_controller_connect(handle, &result) == false)
		{
			DEBUG_ERR_MSG("%s failed, & retry polling!!",
					"net_nfc_controller_connect");

			if (net_nfc_controller_configure_discovery(
						NET_NFC_DISCOVERY_MODE_RESUME,
						NET_NFC_ALL_ENABLE,
						&result) == false)
			{
				net_nfc_controller_exception_handler();
			}

			if (temp)
			{
				g_free(temp->buffer);
				g_free(temp);
			}

			return FALSE;
		}
	}

	*read_ndef = temp;

	return TRUE;
}

static void tag_watchdog_thread_func(gpointer user_data)
{
	WatchDogData *watch_dog = (WatchDogData *)user_data;
	net_nfc_target_handle_s *handle;
	net_nfc_error_e result = NET_NFC_OK;
	bool is_present_target = false;

	if (watch_dog == NULL)
	{
		DEBUG_ERR_MSG("can not get WatchDogData");
		return;
	}

	if (watch_dog->handle == NULL)
	{
		DEBUG_ERR_MSG("can not get WatchDogData->handle");
		return;
	}


	/* IMPORTANT, TEMPORARY : switching context to another thread
	   for give CPU time */
	g_usleep(10000);

	handle = watch_dog->handle;
	if (handle->connection_type == NET_NFC_P2P_CONNECTION_TARGET ||
			handle->connection_type == NET_NFC_TAG_CONNECTION)
	{
		is_present_target = net_nfc_controller_check_target_presence(
					handle, &result);
	}

	if (is_present_target == true)
	{
		if(net_nfc_server_controller_async_queue_push(
					tag_watchdog_thread_func,
					watch_dog) == FALSE)
		{
			DEBUG_ERR_MSG("can not create watch dog");
			g_free(watch_dog);
		}
		return;
	}

	if (result != NET_NFC_NOT_INITIALIZED &&
			result != NET_NFC_INVALID_HANDLE)
	{
		if(net_nfc_controller_disconnect(handle, &result) == false)
		{
			DEBUG_SERVER_MSG("try to disconnect result = [%d]",
					result);
			net_nfc_controller_exception_handler();
		}
	}

	net_nfc_server_set_state(NET_NFC_SERVER_IDLE);

	net_nfc_gdbus_tag_emit_tag_detached(tag_skeleton,
					GPOINTER_TO_UINT(handle),
					watch_dog->dev_type);

	g_free(watch_dog);
}

static void tag_get_current_tag_info_thread_func(gpointer user_data)
{
	CurrentTagInfoData *info_data =
		(CurrentTagInfoData *)user_data;

	/* FIXME : net_nfc_current_target_info_s should be removed */
	net_nfc_current_target_info_s *target_info;
	net_nfc_error_e result;
	net_nfc_target_handle_s *handle = NULL;
	net_nfc_target_type_e dev_type = NET_NFC_UNKNOWN_TARGET;
	gboolean is_ndef_supported = FALSE;
	guint8 ndef_card_state = 0;
	guint32 max_data_size = 0;
	guint32 actual_data_size = 0;
	gint number_of_keys = 0;
	data_s target_info_values = { NULL, 0 };
	data_s *raw_data = NULL;

	g_assert(info_data != NULL);
	g_assert(info_data->tag != NULL);
	g_assert(info_data->invocation != NULL);

	target_info = net_nfc_server_get_target_info();
	if (target_info != NULL &&
		target_info->devType != NET_NFC_NFCIP1_TARGET &&
		target_info->devType != NET_NFC_NFCIP1_INITIATOR)
	{
		handle = target_info->handle;
		number_of_keys = target_info->number_of_keys;

		target_info_values.buffer = target_info->target_info_values.buffer;
		target_info_values.length = target_info->target_info_values.length;

		dev_type = target_info->devType ;

		if (net_nfc_controller_check_ndef(target_info->handle,
					&ndef_card_state,
					(int *)&max_data_size,
					(int *)&actual_data_size,
					&result) == true)
		{
			is_ndef_supported = TRUE;
		}

		if (is_ndef_supported)
		{
			if (net_nfc_controller_read_ndef(target_info->handle,
					&raw_data, &result) == true)
			{
				DEBUG_SERVER_MSG("%s is success",
						"net_nfc_controller_read_ndef");
			}
		}
	}

	net_nfc_gdbus_tag_complete_get_current_tag_info(info_data->tag,
		info_data->invocation,
		(dev_type != NET_NFC_UNKNOWN_TARGET),
		GPOINTER_TO_UINT(handle),
		dev_type,
		is_ndef_supported,
		ndef_card_state,
		max_data_size,
		actual_data_size,
		number_of_keys,
		net_nfc_util_gdbus_data_to_variant(&target_info_values),
		net_nfc_util_gdbus_data_to_variant(raw_data));

	if (raw_data != NULL) {
		net_nfc_util_free_data(raw_data);
		g_free(raw_data);
	}

	g_object_unref(info_data->invocation);
	g_object_unref(info_data->tag);

	g_free(info_data);
}

static void tag_slave_target_detected_thread_func(gpointer user_data)
{
	net_nfc_current_target_info_s *target;
	net_nfc_error_e result = NET_NFC_OK;

	guint32 max_data_size = 0;
	guint32 actual_data_size = 0;
	guint8 ndef_card_state = 0;
	gboolean is_ndef_supported = FALSE;

	GVariant *target_info_values = NULL;
	GVariant *raw_data = NULL;

	WatchDogData *watch_dog = NULL;

	target = net_nfc_server_get_target_info();

	g_assert(target != NULL); /* raise exception!!! what;s wrong?? */

	if (tag_skeleton == NULL)
	{
		DEBUG_ERR_MSG("tag skeleton is not initialized");

		return;
	}

	if (net_nfc_controller_connect(target->handle, &result) == false)
	{
		DEBUG_ERR_MSG("connect failed & Retry Polling!!");

		if (net_nfc_controller_configure_discovery(
					NET_NFC_DISCOVERY_MODE_RESUME,
					NET_NFC_ALL_ENABLE,
					&result) == false)
		{
			net_nfc_controller_exception_handler();
		}

		return;
	}

	net_nfc_server_set_state(NET_NFC_TAG_CONNECTED);

	DEBUG_SERVER_MSG("tag is connected");

	target_info_values = net_nfc_util_gdbus_buffer_to_variant(
			target->target_info_values.buffer,
			target->target_info_values.length);

	if (net_nfc_controller_check_ndef(target->handle,
				&ndef_card_state,
				(int *)&max_data_size,
				(int *)&actual_data_size,
				&result) == true)
	{
		is_ndef_supported = TRUE;
	}

	if (is_ndef_supported)
	{
		data_s *recv_data = NULL;

		DEBUG_SERVER_MSG("support NDEF");

		if (tag_read_ndef_message(target->handle,
					target->devType,
					&recv_data) == TRUE)
		{
			net_nfc_app_util_process_ndef(recv_data);
			raw_data = net_nfc_util_gdbus_data_to_variant(recv_data);
		}
		else
		{
			DEBUG_ERR_MSG("net_nfc_controller_read_ndef failed");
			raw_data = net_nfc_util_gdbus_buffer_to_variant(NULL, 0);
		}
	}
	else
	{
		/* raw-data of empty ndef msseages */
		uint8_t empty[] = { 0xd0, 0x00, 0x00 };
		data_s empty_data = { empty, sizeof(empty) };

		DEBUG_SERVER_MSG("not support NDEF");

		net_nfc_app_util_process_ndef(&empty_data);
		raw_data = net_nfc_util_gdbus_data_to_variant(&empty_data);
	}

	/* send TagDiscoverd signal */
	net_nfc_gdbus_tag_emit_tag_discovered(tag_skeleton,
					GPOINTER_TO_UINT(target->handle),
					target->devType,
					is_ndef_supported,
					ndef_card_state,
					max_data_size,
					actual_data_size,
					target->number_of_keys,
					target_info_values,
					raw_data);

	/* turn on watch dog */
	DEBUG_SERVER_MSG("turn on watch dog");

	watch_dog = g_new0(WatchDogData, 1);
	if(watch_dog == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		return;
	}

	watch_dog->dev_type = target->devType;
	watch_dog->handle = target->handle;

	if (net_nfc_server_controller_async_queue_push(
					tag_watchdog_thread_func,
					watch_dog) == FALSE)
	{
		DEBUG_ERR_MSG("can not create watch dog");
		g_free(watch_dog);
		return;
	}
}


static gboolean tag_handle_is_tag_connected(NetNfcGDbusTag *tag,
					GDBusMethodInvocation *invocation,
					GVariant *smack_privilege,
					gpointer user_data)
{
	/* FIXME : net_nfc_current_target_info_s should be removed */
	net_nfc_current_target_info_s *target_info;
	net_nfc_target_type_e dev_type = NET_NFC_UNKNOWN_TARGET;
	gboolean is_connected = FALSE;

	INFO_MSG(">>> REQUEST from [%s]",
		g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
		smack_privilege,
		"nfc-manager::tag",
		"r") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return FALSE;
	}

	target_info = net_nfc_server_get_target_info();
	if (target_info != NULL)
	{
		dev_type = target_info->devType;
		is_connected = TRUE;
	}

	net_nfc_gdbus_tag_complete_is_tag_connected(tag,
		invocation,
		is_connected,
		(gint32)dev_type);

	return TRUE;
}

static gboolean tag_handle_get_current_tag_info(NetNfcGDbusTag *tag,
				GDBusMethodInvocation *invocation,
				GVariant *smack_privilege,
				gpointer user_data)
{
	CurrentTagInfoData *info_data;
	gboolean result;

	INFO_MSG(">>> REQUEST from [%s]",
		g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
		smack_privilege,
		"nfc-manager::tag",
		"r") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return FALSE;
	}

	info_data = g_new0(CurrentTagInfoData, 1);
	if (info_data == NULL)
	{
		DEBUG_ERR_MSG("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
		"org.tizen.NetNfcService.AllocationError",
		"Can not allocate memory");

		return FALSE;
	}

	info_data->tag = g_object_ref(tag);
	info_data->invocation = g_object_ref(invocation);

	result = net_nfc_server_controller_async_queue_push(
		tag_get_current_tag_info_thread_func,
		info_data);
	if (result == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.ThreadError",
				"can not push to controller thread");

		g_object_unref(info_data->invocation);
		g_object_unref(info_data->tag);

		g_free(info_data);
	}

	return result;
}

static gboolean tag_handle_get_current_target_handle(NetNfcGDbusTag *tag,
	GDBusMethodInvocation *invocation,
	GVariant *smack_privilege,
	gpointer user_data)
{
	/* FIXME : net_nfc_current_target_info_s should be removed */
	net_nfc_current_target_info_s *target_info;
	net_nfc_target_handle_s *handle = NULL;
	uint32_t devType = NET_NFC_UNKNOWN_TARGET;

	INFO_MSG(">>> REQUEST from [%s]",
		g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
		smack_privilege,
		"nfc-manager::tag",
		"r") == false) {
		DEBUG_ERR_MSG("permission denied, and finished request");

		return FALSE;
	}

	target_info = net_nfc_server_get_target_info();
	if (target_info != NULL)
	{
		handle = target_info->handle;
		devType = target_info->devType;
	}

	net_nfc_gdbus_tag_complete_get_current_target_handle(tag,
		invocation,
		(handle != NULL),
		GPOINTER_TO_UINT(handle),
		devType);

	return TRUE;
}


gboolean net_nfc_server_tag_init(GDBusConnection *connection)
{
	GError *error = NULL;
	gboolean result;

	if (tag_skeleton)
		net_nfc_server_tag_deinit();

	tag_skeleton = net_nfc_gdbus_tag_skeleton_new();

	g_signal_connect(tag_skeleton,
			"handle-is-tag-connected",
			G_CALLBACK(tag_handle_is_tag_connected),
			NULL);

	g_signal_connect(tag_skeleton,
			"handle-get-current-tag-info",
			G_CALLBACK(tag_handle_get_current_tag_info),
			NULL);

	g_signal_connect(tag_skeleton,
			"handle-get-current-target-handle",
			G_CALLBACK(tag_handle_get_current_target_handle),
			NULL);

	result = g_dbus_interface_skeleton_export(
		G_DBUS_INTERFACE_SKELETON(tag_skeleton),
		connection,
		"/org/tizen/NetNfcService/Tag",
		&error);
	if (result == FALSE)
	{
		DEBUG_ERR_MSG("can not skeleton_export %s", error->message);

		g_error_free(error);

		net_nfc_server_tag_deinit();
	}

	return result;
}

void net_nfc_server_tag_deinit(void)
{
	if (tag_skeleton)
	{
		g_object_unref(tag_skeleton);
		tag_skeleton = NULL;
	}
}

void net_nfc_server_set_target_info(void *info)
{
	net_nfc_request_target_detected_t *target;

	if (current_target_info)
		g_free(current_target_info);

	target = (net_nfc_request_target_detected_t *)info;

	current_target_info = g_malloc0(
				sizeof(net_nfc_current_target_info_s) +
				target->target_info_values.length);

	current_target_info->handle = target->handle;
	current_target_info->devType = target->devType;

	if (current_target_info->devType != NET_NFC_NFCIP1_INITIATOR &&
			current_target_info->devType != NET_NFC_NFCIP1_TARGET)
	{
		current_target_info->number_of_keys = target->number_of_keys;
		current_target_info->target_info_values.length =
				target->target_info_values.length;

		memcpy(&current_target_info->target_info_values,
			&target->target_info_values,
			current_target_info->target_info_values.length);
	}
}

net_nfc_current_target_info_s *net_nfc_server_get_target_info(void)
{
	return current_target_info;
}

gboolean net_nfc_server_target_connected(net_nfc_target_handle_s *handle)
{
	if (current_target_info == NULL)
		return FALSE;

	if (current_target_info->handle != handle)
		return FALSE;

	return TRUE;
}

void net_nfc_server_free_target_info(void)
{
	g_free(current_target_info);
	current_target_info = NULL;
}

void net_nfc_server_tag_target_detected(void *info)
{
	if (net_nfc_server_controller_async_queue_push(
		tag_slave_target_detected_thread_func,
		NULL) == FALSE)
	{
		DEBUG_ERR_MSG("can not push to controller thread");
	}
}
