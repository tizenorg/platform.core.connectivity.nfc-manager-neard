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


#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_server_common.h"
#include "net_nfc_server_context.h"
#include "net_nfc_server_util.h"
#include "net_nfc_server_p2p.h"
#include "net_nfc_server_process_handover.h"
#include "net_nfc_util_ndef_record.h"
#include "net_nfc_server_tag.h"

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

static NetNfcGDbusTag *tag_skeleton = NULL;

static net_nfc_current_target_info_s *current_target_info = NULL;

static gboolean tag_is_isp_dep_ndef_formatable(net_nfc_target_handle_s *handle,
		int dev_type)
{

	gboolean result = false;
	data_s *response = NULL;
	net_nfc_transceive_info_s info;
	net_nfc_error_e error = NET_NFC_OK;
	uint8_t cmd[] = { 0x90, 0x60, 0x00, 0x00, 0x00 };

	info.dev_type = dev_type;
	info.trans_data.buffer = cmd;
	info.trans_data.length = sizeof(cmd);

	if (net_nfc_controller_transceive(handle, &info, &response, &error) == false)
	{
		NFC_ERR("net_nfc_controller_transceive is failed");

		return result;
	}

	if (response != NULL)
	{
		if (response->length == 9 && response->buffer[7] == (uint8_t)0x91 &&
				response->buffer[8] == (uint8_t)0xAF)
		{
			result =  TRUE;
		}

		net_nfc_util_free_data(response);
		g_free(response);
	}
	else
	{
		NFC_ERR("response is NULL");
	}

	return result;
}

static gboolean tag_read_ndef_message(net_nfc_target_handle_s *handle,
		int dev_type, data_s **read_ndef)
{
	data_s *temp = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == handle, FALSE);
	RETV_IF(NULL == read_ndef, FALSE);

	*read_ndef = NULL;

	if (NET_NFC_MIFARE_DESFIRE_PICC == dev_type)
	{
		if (tag_is_isp_dep_ndef_formatable(handle, dev_type) == FALSE)
		{
			NFC_ERR("DESFIRE : ISO-DEP ndef not formatable");
			return FALSE;
		}

		NFC_DBG("DESFIRE : ISO-DEP ndef formatable");

		if (net_nfc_controller_connect(handle, &result) == false)
		{
			NFC_ERR("net_nfc_controller_connect failed & retry polling!!");

			if (net_nfc_controller_configure_discovery(NET_NFC_DISCOVERY_MODE_RESUME,
						NET_NFC_ALL_ENABLE, &result) == false)
			{
				net_nfc_controller_exception_handler();
			}
			return FALSE;
		}
	}

	if (net_nfc_controller_read_ndef(handle, &temp, &result) == false)
	{
		NFC_ERR("net_nfc_controller_read_ndef failed");
		return FALSE;
	}

	NFC_DBG("net_nfc_controller_read_ndef success");

	if (NET_NFC_MIFARE_DESFIRE_PICC == dev_type)
	{
		if (net_nfc_controller_connect(handle, &result) == false)
		{
			NFC_ERR("net_nfc_controller_connect failed, & retry polling!!");

			if (net_nfc_controller_configure_discovery(NET_NFC_DISCOVERY_MODE_RESUME,
						NET_NFC_ALL_ENABLE, &result) == false)
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
	gboolean ret;
	net_nfc_target_handle_s *handle;
	bool is_present_target = false;
	net_nfc_error_e result = NET_NFC_OK;
	WatchDogData *watch_dog = user_data;

	RET_IF(NULL == watch_dog);
	RET_IF(NULL == watch_dog->handle);

	/* IMPORTANT, TEMPORARY : switching context to another thread
	   for give CPU time */
	g_usleep(10000);

	handle = watch_dog->handle;
	if (handle->connection_type == NET_NFC_P2P_CONNECTION_TARGET ||
			handle->connection_type == NET_NFC_TAG_CONNECTION)
	{
		is_present_target = net_nfc_controller_check_target_presence(handle, &result);
	}

	if (true == is_present_target)
	{
		ret = net_nfc_server_controller_async_queue_push(tag_watchdog_thread_func,
					watch_dog);
		if(FALSE == ret)
		{
			NFC_ERR("can not create watch dog");
			g_free(watch_dog);
		}
		return;
	}

	if (result != NET_NFC_NOT_INITIALIZED && result != NET_NFC_INVALID_HANDLE)
	{
		if(net_nfc_controller_disconnect(handle, &result) == false)
		{
			NFC_ERR("try to disconnect result = [%d]", result);
			net_nfc_controller_exception_handler();
		}
	}

	net_nfc_server_set_state(NET_NFC_SERVER_IDLE);

	net_nfc_gdbus_tag_emit_tag_detached(tag_skeleton, GPOINTER_TO_UINT(handle),
			watch_dog->dev_type);

	g_free(watch_dog);
}

static void tag_get_current_tag_info_thread_func(gpointer user_data)
{

	/* FIXME : net_nfc_current_target_info_s should be removed */
	bool ret;
	data_s *raw_data = NULL;
	gint number_of_keys = 0;
	guint32 max_data_size = 0;
	guint8 ndef_card_state = 0;
	guint32 actual_data_size = 0;
	gboolean is_ndef_supported = FALSE;
	net_nfc_target_handle_s *handle = NULL;
	data_s target_info_values = { NULL, 0 };
	CurrentTagInfoData *info_data = user_data;
	net_nfc_current_target_info_s *target_info;
	net_nfc_error_e result = NET_NFC_OPERATION_FAIL;
	net_nfc_target_type_e dev_type = NET_NFC_UNKNOWN_TARGET;

	g_assert(info_data != NULL);
	g_assert(info_data->tag != NULL);
	g_assert(info_data->invocation != NULL);

	target_info = net_nfc_server_get_target_info();
	if (target_info != NULL && target_info->devType != NET_NFC_NFCIP1_TARGET &&
			target_info->devType != NET_NFC_NFCIP1_INITIATOR)
	{
		handle = target_info->handle;
		number_of_keys = target_info->number_of_keys;

		target_info_values.buffer = target_info->target_info_values.buffer;
		target_info_values.length = target_info->target_info_values.length;

		dev_type = target_info->devType ;

		ret = net_nfc_controller_check_ndef(target_info->handle, &ndef_card_state,
					(int *)&max_data_size, (int *)&actual_data_size, &result);
		if (true == ret)
			is_ndef_supported = TRUE;

		if (is_ndef_supported)
		{
			ret = net_nfc_controller_read_ndef(target_info->handle, &raw_data, &result);
			if (true == ret)
				NFC_DBG("net_nfc_controller_read_ndef is success");
		}
	}

	net_nfc_gdbus_tag_complete_get_current_tag_info(info_data->tag,
			info_data->invocation,
			result,
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

	if (raw_data != NULL)
	{
		net_nfc_util_free_data(raw_data);
		g_free(raw_data);
	}

	g_object_unref(info_data->invocation);
	g_object_unref(info_data->tag);

	g_free(info_data);
}

static void tag_slave_target_detected_thread_func(gpointer user_data)
{
	bool ret;
	GVariant *raw_data = NULL;
	guint32 max_data_size = 0;
	guint8 ndef_card_state = 0;
	guint32 actual_data_size = 0;
	WatchDogData *watch_dog = NULL;
	bool isHandoverMessage = false;
	net_nfc_error_e result = NET_NFC_OK;
	gboolean is_ndef_supported = FALSE;
	net_nfc_current_target_info_s *target;
	GVariant *target_info_values = NULL;

	target = net_nfc_server_get_target_info();

	g_assert(target != NULL); /* raise exception!!! what;s wrong?? */

	RET_IF(NULL == tag_skeleton);

	if (net_nfc_controller_connect(target->handle, &result) == false)
	{
		NFC_ERR("connect failed & Retry Polling!!");

		ret = net_nfc_controller_configure_discovery(NET_NFC_DISCOVERY_MODE_RESUME,
					NET_NFC_ALL_ENABLE, &result);
		if (false == ret)
			net_nfc_controller_exception_handler();

		return;
	}

	net_nfc_server_set_state(NET_NFC_TAG_CONNECTED);

	NFC_DBG("tag is connected");

	target_info_values = net_nfc_util_gdbus_buffer_to_variant(
			target->target_info_values.buffer, target->target_info_values.length);

	ret = net_nfc_controller_check_ndef(target->handle, &ndef_card_state,
				(int *)&max_data_size, (int *)&actual_data_size, &result);
	if (true == ret)
		is_ndef_supported = TRUE;

	if (is_ndef_supported)
	{
		data_s *recv_data = NULL;

		NFC_DBG("support NDEF");

		if (tag_read_ndef_message(target->handle, target->devType, &recv_data) == TRUE)
		{
			ndef_record_s *record;
			ndef_message_s *selector;
			ndef_record_s *recordasperpriority;

			result = net_nfc_server_handover_create_selector_from_rawdata(&selector,
					recv_data);

			if (NET_NFC_OK == result)
			{
				result = net_nfc_server_handover_get_carrier_record_by_priority_order(
							selector, &record);
				isHandoverMessage = true;
				if (NET_NFC_OK == result)
				{
					net_nfc_util_create_record(record->TNF, &record->type_s, &record->id_s,
							&record->payload_s, &recordasperpriority);

					net_nfc_server_handover_process_carrier_record(recordasperpriority,
							NULL, NULL);
				}
				else
				{
					NFC_ERR("_get_carrier_record_by_priority_order failed, [%d]",result);
				}
			}
			else
			{
				net_nfc_app_util_process_ndef(recv_data);
				raw_data = net_nfc_util_gdbus_data_to_variant(recv_data);
			}
		}
		else
		{
			NFC_ERR("net_nfc_controller_read_ndef failed");
			raw_data = net_nfc_util_gdbus_buffer_to_variant(NULL, 0);
		}
	}
	else
	{
		/* raw-data of empty ndef msseages */
		uint8_t empty[] = { 0xd0, 0x00, 0x00 };
		data_s empty_data = { empty, sizeof(empty) };

		NFC_DBG("not support NDEF");

		net_nfc_app_util_process_ndef(&empty_data);
		raw_data = net_nfc_util_gdbus_data_to_variant(&empty_data);
	}

	if(isHandoverMessage == false)
	{
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
	}

	/* turn on watch dog */
	NFC_DBG("turn on watch dog");

	watch_dog = g_new0(WatchDogData, 1);
	if(NULL == watch_dog)
	{
		NFC_ERR("Memory allocation failed");
		return;
	}

	watch_dog->dev_type = target->devType;
	watch_dog->handle = target->handle;

	if (net_nfc_server_controller_async_queue_push(
				tag_watchdog_thread_func, watch_dog) == FALSE)
	{
		NFC_ERR("can not create watch dog");
		g_free(watch_dog);
		return;
	}
}


static gboolean tag_handle_is_tag_connected(NetNfcGDbusTag *tag,
		GDBusMethodInvocation *invocation, GVariant *smack_privilege, gpointer user_data)
{
	/* FIXME : net_nfc_current_target_info_s should be removed */
	bool ret;
	gint result;
	gboolean is_connected = FALSE;
	net_nfc_current_target_info_s *target_info;
	net_nfc_target_type_e dev_type = NET_NFC_UNKNOWN_TARGET;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::tag", "r");

	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");
		result = NET_NFC_SECURITY_FAIL;

		goto END;
	}

	target_info = net_nfc_server_get_target_info();
	if (target_info != NULL)
	{
		dev_type = target_info->devType;
		is_connected = TRUE;
	}

	result = NET_NFC_OK;

END :
	net_nfc_gdbus_tag_complete_is_tag_connected(tag, invocation, result, is_connected,
			(gint32)dev_type);

	return TRUE;
}

static gboolean tag_handle_get_current_tag_info(NetNfcGDbusTag *tag,
		GDBusMethodInvocation *invocation, GVariant *smack_privilege, gpointer user_data)
{
	bool ret;
	gboolean result;
	CurrentTagInfoData *info_data;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::tag", "r");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	info_data = g_new0(CurrentTagInfoData, 1);
	if (NULL == info_data)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError", "Can not allocate memory");

		return FALSE;
	}

	info_data->tag = g_object_ref(tag);
	info_data->invocation = g_object_ref(invocation);

	result = net_nfc_server_controller_async_queue_push(
			tag_get_current_tag_info_thread_func, info_data);
	if (FALSE == result)
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
		GDBusMethodInvocation *invocation, GVariant *smack_privilege, gpointer user_data)
{
	/* FIXME : net_nfc_current_target_info_s should be removed */
	gint result;
	gboolean ret;
	net_nfc_target_handle_s *handle = NULL;
	uint32_t devType = NET_NFC_UNKNOWN_TARGET;
	net_nfc_current_target_info_s *target_info;

	NFC_INFO(">>> REQUEST from [%s]", g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	ret = net_nfc_server_gdbus_check_privilege(invocation, smack_privilege,
				"nfc-manager::p2p", "r");
	if (false == ret)
	{
		NFC_ERR("permission denied, and finished request");
		result = NET_NFC_SECURITY_FAIL;

		goto END;
	}

	target_info = net_nfc_server_get_target_info();
	if (target_info != NULL)
	{
		handle = target_info->handle;
		devType = target_info->devType;
	}

	result = NET_NFC_OK;

END :
	net_nfc_gdbus_tag_complete_get_current_target_handle(tag, invocation, result,
			(handle != NULL), GPOINTER_TO_UINT(handle), devType);

	return TRUE;
}


gboolean net_nfc_server_tag_init(GDBusConnection *connection)
{
	gboolean result;
	GError *error = NULL;

	if (tag_skeleton)
		net_nfc_server_tag_deinit();

	tag_skeleton = net_nfc_gdbus_tag_skeleton_new();

	g_signal_connect(tag_skeleton, "handle-is-tag-connected",
			G_CALLBACK(tag_handle_is_tag_connected), NULL);

	g_signal_connect(tag_skeleton, "handle-get-current-tag-info",
			G_CALLBACK(tag_handle_get_current_tag_info), NULL);

	g_signal_connect(tag_skeleton, "handle-get-current-target-handle",
			G_CALLBACK(tag_handle_get_current_target_handle), NULL);

	result = g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(tag_skeleton),
			connection, "/org/tizen/NetNfcService/Tag", &error);
	if (FALSE == result)
	{
		NFC_ERR("can not skeleton_export %s", error->message);

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

	current_target_info = g_malloc0(sizeof(net_nfc_current_target_info_s) +
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
	if (NULL == current_target_info)
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
	gboolean ret;

	ret = net_nfc_server_controller_async_queue_push(
				tag_slave_target_detected_thread_func, NULL);

	if (FALSE == ret)
		NFC_ERR("can not push to controller thread");
}
