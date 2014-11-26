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
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_data.h"
#include "net_nfc_target_info.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_tag.h"
#include "net_nfc_client_tag_internal.h"
#include "net_nfc_neard.h"


static NetNfcGDbusTag *tag_proxy = NULL;

static NetNfcCallback tag_discovered_func_data;
static NetNfcCallback tag_detached_func_data;

static net_nfc_target_info_s *client_target_info = NULL;
static net_nfc_event_filter_e client_filter = NET_NFC_ALL_ENABLE;

static gboolean tag_check_filter(net_nfc_target_type_e type)
{
	net_nfc_event_filter_e converted = NET_NFC_ALL_ENABLE;

	NFC_DBG("client filter =  %d", client_filter);

	if (type >= NET_NFC_ISO14443_A_PICC
			&& type <= NET_NFC_MIFARE_DESFIRE_PICC)
	{
		converted = NET_NFC_ISO14443A_ENABLE;
	}
	else if (type >= NET_NFC_ISO14443_B_PICC
			&& type <= NET_NFC_ISO14443_BPRIME_PICC)
	{
		converted = NET_NFC_ISO14443B_ENABLE;
	}
	else if (type == NET_NFC_FELICA_PICC)
	{
		converted = NET_NFC_FELICA_ENABLE;
	}
	else if (type == NET_NFC_JEWEL_PICC)
	{
		converted = NET_NFC_FELICA_ENABLE;
	}
	else if (type == NET_NFC_ISO15693_PICC)
	{
		converted = NET_NFC_ISO15693_ENABLE;
	}

	if ((converted & client_filter) == 0)
		return FALSE;

	return TRUE;
}

static void tag_get_info_list(guint8 *buffer, gint number_of_keys,
		net_nfc_tag_info_s **list)
{
	gint i = 0;
	gint length;
	guint8 *pos = buffer;
	net_nfc_tag_info_s *current = NULL;
	net_nfc_tag_info_s *tmp_list = NULL;

	RET_IF(NULL == buffer);

	tmp_list = g_new0(net_nfc_tag_info_s, number_of_keys);
	current = tmp_list;

	while (i < number_of_keys)
	{
		gchar *str = NULL;
		data_s *value = NULL;

		/* key */
		length = *pos;	/* first values is length of key */
		pos++;

		str = g_new0(gchar, length + 1);
		memcpy(str, pos, length);

		NFC_DBG("key = [%s]", str);

		pos += length;

		current->key = str;

		/* value */
		length = *pos; /* first value is length of value */
		pos++;

		value = NULL;
		if (length > 0)
		{
			net_nfc_create_data(&value, pos, length);
			pos += length;
		}

		current->value = value;

		current++;
		i++;
	}

	*list = tmp_list;
}

static void tag_get_target_info(guint handle,
		guint dev_type,
		gboolean is_ndef_supported,
		guchar ndef_card_state,
		guint max_data_size,
		guint actual_data_size,
		guint number_of_keys,
		GVariant *target_info_values,
		GVariant *raw_data,
		net_nfc_target_info_s **info)
{
	guint8 *buffer = NULL;
	net_nfc_tag_info_s *list = NULL;
	net_nfc_target_info_s *info_data = NULL;

	RET_IF(NULL == info);

	net_nfc_util_gdbus_variant_to_buffer(target_info_values, &buffer, NULL);

	tag_get_info_list(buffer, number_of_keys, &list);

	info_data = g_new0(net_nfc_target_info_s, 1);

	info_data->ndefCardState = ndef_card_state;
	info_data->actualDataSize = actual_data_size;
	info_data->maxDataSize = max_data_size;
	info_data->devType = dev_type;
	info_data->handle = GUINT_TO_POINTER(handle);
	info_data->is_ndef_supported = (uint8_t)is_ndef_supported;
	info_data->number_of_keys = number_of_keys;
	info_data->tag_info_list = list;

	net_nfc_util_gdbus_variant_to_data_s(raw_data, &info_data->raw_data);

	*info = info_data;
}
#if 0
static void tag_is_tag_connected(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e out_result;
	gboolean out_is_connected = false;
	NetNfcCallback *func_data = (NetNfcCallback *)user_data;
	net_nfc_target_type_e out_dev_type = NET_NFC_UNKNOWN_TARGET;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_tag_call_is_tag_connected_finish(
			NET_NFC_GDBUS_TAG(source_object),
			&out_result,
			&out_is_connected,
			(gint32 *)&out_dev_type,
			res,
			&error);

	if (FALSE == ret)
	{
		NFC_ERR("Can not finish is_tag_connected: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_tag_is_tag_connected_completed callback =
			(net_nfc_client_tag_is_tag_connected_completed)func_data->callback;

		if (out_is_connected == FALSE)
			out_result = NET_NFC_NOT_CONNECTED;

		callback(out_result, out_dev_type, func_data->user_data);
	}

	g_free(func_data);
}

static void tag_get_current_tag_info(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	guint out_handle = 0;
	GError *error = NULL;
	guint out_max_data_size = 0;
	guint out_number_of_keys = 0;
	GVariant *out_raw_data = NULL;
	guchar out_ndef_card_state = 0;
	guint out_actual_data_size = 0;
	gboolean out_is_connected = FALSE;
	gboolean out_is_ndef_supported = FALSE;
	GVariant *out_target_info_values = NULL;
	net_nfc_error_e out_result = NET_NFC_OK;
	NetNfcCallback *func_data = (NetNfcCallback *)user_data;
	net_nfc_target_type_e out_dev_type = NET_NFC_UNKNOWN_TARGET;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_tag_call_get_current_tag_info_finish (
			NET_NFC_GDBUS_TAG(source_object),
			&out_result,
			&out_is_connected,
			&out_handle,
			(gint *)&out_dev_type,
			&out_is_ndef_supported,
			&out_ndef_card_state,
			&out_max_data_size,
			&out_actual_data_size,
			&out_number_of_keys,
			&out_target_info_values,
			&out_raw_data,
			res,
			&error);

	if (FALSE == ret)
	{
		out_result = NET_NFC_IPC_FAIL;

		NFC_ERR("Can not finish get_current_tag_info: %s", error->message);
		g_error_free(error);
	}

	if (out_result == NET_NFC_OK && out_is_connected == true)
	{
		net_nfc_release_tag_info(client_target_info);
		client_target_info = NULL;

		if (tag_check_filter(out_dev_type) == true)
		{
			tag_get_target_info(out_handle,
					out_dev_type,
					out_is_ndef_supported,
					out_ndef_card_state,
					out_max_data_size,
					out_actual_data_size,
					out_number_of_keys,
					out_target_info_values,
					out_raw_data,
					&client_target_info);
		}
		else
		{
			NFC_INFO("The detected target is filtered out, type [%d]", out_dev_type);

			out_is_connected = false;
		}
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_tag_get_current_tag_info_completed callback =
			(net_nfc_client_tag_get_current_tag_info_completed)func_data->callback;

		if (out_result == NET_NFC_OK && out_is_connected == false)
			out_result = NET_NFC_NOT_CONNECTED;

		callback(out_result, client_target_info, func_data->user_data);
	}

	g_free(func_data);
}

static void tag_get_current_target_handle(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	gboolean out_is_connected = false;
	net_nfc_error_e out_result = NET_NFC_OK;
	net_nfc_target_handle_s *out_handle = NULL;
	NetNfcCallback *func_data = (NetNfcCallback *)user_data;
	net_nfc_target_type_e out_dev_type = NET_NFC_UNKNOWN_TARGET;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_tag_call_get_current_target_handle_finish(
			NET_NFC_GDBUS_TAG(source_object),
			&out_result,
			&out_is_connected,
			(guint *)&out_handle,
			(gint *)&out_dev_type,
			res,
			&error);

	if (FALSE == ret)
	{
		NFC_ERR("Can not finish get_current_target_handle: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_client_tag_get_current_target_handle_completed callback =
			(net_nfc_client_tag_get_current_target_handle_completed)func_data->callback;

		if (out_result == NET_NFC_OK && out_is_connected == FALSE)
			out_result = NET_NFC_NOT_CONNECTED;

		callback(out_result, GUINT_TO_POINTER(out_handle), func_data->user_data);
	}

	g_free(func_data);
}
#endif
static void tag_tag_discovered(NetNfcGDbusTag *object,
		guint arg_handle,
		gint arg_dev_type,
		gboolean arg_is_ndef_supported,
		guchar arg_ndef_card_state,
		guint arg_max_data_size,
		guint arg_actual_data_size,
		guint arg_number_of_keys,
		GVariant *arg_target_info_values,
		GVariant *arg_raw_data,
		gpointer user_data)
{
	net_nfc_client_tag_tag_discovered callback;

	NFC_INFO(">>> SIGNAL arrived");

	net_nfc_release_tag_info(client_target_info);
	client_target_info = NULL;

	if (tag_check_filter(arg_dev_type) == FALSE)
	{
		NFC_INFO("The detected target is filtered out, type [%d]", arg_dev_type);

		return;
	}

	tag_get_target_info(arg_handle,
			arg_dev_type,
			arg_is_ndef_supported,
			arg_ndef_card_state,
			arg_max_data_size,
			arg_actual_data_size,
			arg_number_of_keys,
			arg_target_info_values,
			arg_raw_data,
			&client_target_info);

	if (tag_discovered_func_data.callback != NULL)
	{
		callback = (net_nfc_client_tag_tag_discovered)tag_discovered_func_data.callback;

		callback(client_target_info, tag_discovered_func_data.user_data);
	}
}

static void tag_tag_detached(NetNfcGDbusTag *object, guint arg_handle,
		gint arg_dev_type, gpointer user_data)
{
	NFC_INFO(">>> SIGNAL arrived");
	net_nfc_client_tag_tag_detached callback;
	if (tag_check_filter(arg_dev_type) == TRUE)
	{
		if (tag_detached_func_data.callback != NULL)
		{
			callback = (net_nfc_client_tag_tag_detached)tag_detached_func_data.callback;

			callback(tag_detached_func_data.user_data);
		}
	}
	else
	{
		NFC_INFO("The detected target is filtered out, type [%d]", arg_dev_type);
	}

	net_nfc_release_tag_info(client_target_info);
	client_target_info = NULL;
}

/* internal function */
gboolean net_nfc_client_tag_is_connected(void)
{
	return net_nfc_neard_is_tag_connected();
}

net_nfc_target_info_s *net_nfc_client_tag_get_client_target_info(void)
{
	net_nfc_neard_get_current_tag_info(&client_target_info);
	return client_target_info;
}

/* public APIs */
#if 0
API net_nfc_error_e net_nfc_client_tag_is_tag_connected(
		net_nfc_client_tag_is_tag_connected_completed callback, void *user_data)
{
	NetNfcCallback *func_data;

	RETV_IF(NULL == tag_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	func_data = g_try_new0(NetNfcCallback, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_tag_call_is_tag_connected(tag_proxy,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			tag_is_tag_connected,
			func_data);

	return NET_NFC_OK;
}
#endif

API net_nfc_error_e net_nfc_client_tag_is_tag_connected_sync(
		net_nfc_target_type_e *dev_type)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_target_info_s *info;
	gboolean out_is_connected = FALSE;
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_target_type_e out_dev_type = NET_NFC_UNKNOWN_TARGET;

	RETV_IF(NULL == tag_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	info = net_nfc_client_tag_get_client_target_info();
	if (NULL == info)
	{
		/* try to request target information from server */
		ret = net_nfc_gdbus_tag_call_is_tag_connected_sync(tag_proxy,
				net_nfc_client_gdbus_get_privilege(),
				&result,
				&out_is_connected,
				(gint *)&out_dev_type,
				NULL,
				&error);

		if (FALSE == ret)
		{
			NFC_ERR("Can not get is_tag_connected result: %s", error->message);
			g_error_free(error);

			return NET_NFC_IPC_FAIL;
		}

		if (TRUE == out_is_connected)
		{
			if (dev_type)
				*dev_type = out_dev_type;

			result = NET_NFC_OK;
		}
		else
		{
			result = NET_NFC_NOT_CONNECTED;
		}
	}
	else
	{
		/* target was connected */
		if (dev_type != NULL)
			*dev_type = info->devType;

		result = NET_NFC_OK;
	}

	return result;
}

#if 0
API net_nfc_error_e net_nfc_client_tag_get_current_tag_info(
		net_nfc_client_tag_get_current_tag_info_completed callback, void *user_data)
{
	NetNfcCallback *func_data;

	RETV_IF(NULL == tag_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	func_data = g_try_new0(NetNfcCallback, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_tag_call_get_current_tag_info(tag_proxy,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			tag_get_current_tag_info,
			func_data);

	return NET_NFC_OK;
}
#endif

API net_nfc_error_e net_nfc_client_tag_get_current_tag_info_sync(
		net_nfc_target_info_s **info)
{
	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	return net_nfc_neard_get_current_tag_info(info);
}

#if 0
API net_nfc_error_e net_nfc_client_tag_get_current_target_handle(
		net_nfc_client_tag_get_current_target_handle_completed callback, void *user_data)
{
	NetNfcCallback *func_data;

	RETV_IF(NULL == tag_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	func_data = g_try_new0(NetNfcCallback, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_tag_call_get_current_target_handle(tag_proxy,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			tag_get_current_target_handle,
			func_data);

	return NET_NFC_OK;
}
#endif

API net_nfc_error_e net_nfc_client_tag_get_current_target_handle_sync(
		net_nfc_target_handle_s **handle)
{
	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	return net_nfc_neard_get_current_target_handle(handle);
}

API void net_nfc_client_tag_set_tag_discovered(
		net_nfc_client_tag_tag_discovered callback, void *user_data)
{
	RET_IF(NULL == callback);

	net_nfc_neard_set_tag_discovered(callback, user_data);
}

API void net_nfc_client_tag_unset_tag_discovered(void)
{
	net_nfc_neard_unset_tag_discovered();
}

API void net_nfc_client_tag_set_tag_detached(
		net_nfc_client_tag_tag_detached callback, void *user_data)
{
	RET_IF(NULL == callback);

	net_nfc_neard_set_tag_detached(callback, user_data);
}

API void net_nfc_client_tag_unset_tag_detached(void)
{
	net_nfc_neard_unset_tag_detached();
}

API void net_nfc_client_tag_set_filter(net_nfc_event_filter_e filter)
{
	client_filter = filter;
}

API net_nfc_event_filter_e net_nfc_client_tag_get_filter(void)
{
	return client_filter;
}

net_nfc_error_e net_nfc_client_tag_init(void)
{
	GError *error = NULL;

	if (tag_proxy)
	{
		NFC_WARN("Alrady initialized");
		return NET_NFC_OK;
	}

	if (client_target_info)
	{
		net_nfc_release_tag_info(client_target_info);
		client_target_info = NULL;
	}

	client_filter = NET_NFC_ALL_ENABLE;

	tag_proxy = net_nfc_gdbus_tag_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Tag",
			NULL,
			&error);
	if (NULL == tag_proxy)
	{
		NFC_ERR("Can not create proxy : %s", error->message);
		g_error_free(error);

		return NET_NFC_UNKNOWN_ERROR;
	}

	g_signal_connect(tag_proxy, "tag-discovered", G_CALLBACK(tag_tag_discovered), NULL);
	g_signal_connect(tag_proxy, "tag-detached", G_CALLBACK(tag_tag_detached), NULL);

	return NET_NFC_OK;
}

void net_nfc_client_tag_deinit(void)
{
	client_filter = NET_NFC_ALL_ENABLE;

	net_nfc_release_tag_info(client_target_info);
	client_target_info = NULL;

	net_nfc_client_tag_unset_tag_discovered();
	net_nfc_client_tag_unset_tag_detached();

	if (tag_proxy)
	{
		g_object_unref(tag_proxy);
		tag_proxy = NULL;
	}
}
