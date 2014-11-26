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
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_data.h"
#include "net_nfc_ndef_message.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_ndef.h"
#include "net_nfc_client_tag_internal.h"
#include "net_nfc_neard.h"

static NetNfcGDbusNdef *ndef_proxy = NULL;

static gboolean ndef_is_supported_tag(void)
{
	net_nfc_target_info_s *target_info = NULL;

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
	{
		NFC_ERR("target_info does not exist");
		return FALSE;
	}

	switch (target_info->devType)
	{
	case NET_NFC_ISO14443_A_PICC :
	case NET_NFC_MIFARE_MINI_PICC :
	case NET_NFC_MIFARE_1K_PICC :
	case NET_NFC_MIFARE_4K_PICC :
	case NET_NFC_MIFARE_ULTRA_PICC :
	case NET_NFC_JEWEL_PICC :
		return TRUE;
		break;
	default:
		NFC_ERR("not supported tag(%d) for read only tag", target_info->devType);
		return FALSE;
	}
}

static void ndef_call_read(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	GVariant *out_data = NULL;
	net_nfc_error_e out_result;
	NetNfcCallback *func_data = user_data;
	net_nfc_client_ndef_read_completed callback;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_ndef_call_read_finish(NET_NFC_GDBUS_NDEF(source_object),
			(gint *)&out_result, &out_data, res, &error);

	if (FALSE == ret)
	{

		NFC_ERR("Can not finish read: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		ndef_message_s *message;

		callback = (net_nfc_client_ndef_read_completed)func_data->callback;

		message = net_nfc_util_gdbus_variant_to_ndef_message(out_data);

		callback(out_result, message, func_data->user_data);

		net_nfc_util_free_ndef_message(message);
	}

	g_free(func_data);
}

static void ndef_call_write(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e out_result;
	NetNfcCallback *func_data = user_data;
	net_nfc_client_ndef_write_completed callback;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_ndef_call_write_finish(NET_NFC_GDBUS_NDEF(source_object),
			(gint *)&out_result, res, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Can not finish write: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		callback = (net_nfc_client_ndef_write_completed)func_data->callback;

		callback(out_result, func_data->user_data);
	}

	g_free(func_data);
}

static void ndef_call_make_read_only(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e out_result;
	NetNfcCallback *func_data = user_data;
	net_nfc_client_ndef_make_read_only_completed callback;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_ndef_call_make_read_only_finish(NET_NFC_GDBUS_NDEF(source_object),
			(gint *)&out_result, res, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Can not finish make read only: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		callback = (net_nfc_client_ndef_make_read_only_completed)func_data->callback;

		callback(out_result, func_data->user_data);
	}

	g_free(func_data);
}

static void ndef_call_format(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e out_result;
	NetNfcCallback *func_data = user_data;
	net_nfc_client_ndef_format_completed callback;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_ndef_call_format_finish(NET_NFC_GDBUS_NDEF(source_object),
				(gint *)&out_result, res, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Can not finish format: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		callback = (net_nfc_client_ndef_format_completed)func_data->callback;

		callback(out_result, func_data->user_data);
	}

	g_free(func_data);
}

API net_nfc_error_e net_nfc_client_ndef_read(net_nfc_target_handle_s *handle,
		net_nfc_client_ndef_read_completed callback, void *user_data)
{
	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_NOT_CONNECTED);

	NFC_DBG("send request :: read ndef = [%p]", handle);

	return net_nfc_neard_read_tag(handle, callback, user_data);
}

API net_nfc_error_e net_nfc_client_ndef_read_sync(net_nfc_target_handle_s *handle,
		ndef_message_s **message)
{
	gboolean ret;
	GError *error = NULL;
	GVariant *out_data = NULL;
	net_nfc_error_e out_result = NET_NFC_OK;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == ndef_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_NOT_CONNECTED);

	NFC_DBG("send request :: read ndef = [%p]", handle);

	ret = net_nfc_gdbus_ndef_call_read_sync(ndef_proxy,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			(gint *)&out_result,
			&out_data,
			NULL,
			&error);

	if (TRUE == ret)
	{
		*message = net_nfc_util_gdbus_variant_to_ndef_message(out_data);
	}
	else
	{
		NFC_ERR("can not call read: %s", error->message);
		g_error_free(error);
		out_result = NET_NFC_IPC_FAIL;
	}

	return out_result;
}

API net_nfc_error_e net_nfc_client_ndef_write(net_nfc_target_handle_s *handle,
		ndef_message_s *message,
		net_nfc_client_ndef_write_completed callback,
		void *user_data)
{
	size_t length;
	net_nfc_error_e ret;
	data_s data = {NULL, 0};

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == message, NET_NFC_NULL_PARAMETER);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_NOT_CONNECTED);

	length = net_nfc_util_get_ndef_message_length((ndef_message_s *)message);
	if (length > 0) {
		if (net_nfc_util_alloc_data(&data, length) == true) {
			ret = net_nfc_util_convert_ndef_message_to_rawdata((ndef_message_s *)message,
						&data);
			if (NET_NFC_OK != ret) {
				NFC_ERR("can not convert ndef_message to rawdata");

				net_nfc_util_free_data(&data);
			}
		} else {
			NFC_ERR("net_nfc_util_alloc_data failed");

			return NET_NFC_ALLOC_FAIL;
		}
	} else {
		NFC_ERR("message length is 0");

		return NET_NFC_INVALID_PARAM;
	}

	ret = net_nfc_neard_write_ndef(handle, &data, callback, user_data);

	net_nfc_util_free_data(&data);

	return ret;
}

API net_nfc_error_e net_nfc_client_ndef_write_sync(net_nfc_target_handle_s *handle,
		ndef_message_s *message)
{
	gboolean ret;
	GVariant *arg_data;
	GError *error = NULL;
	net_nfc_error_e out_result = NET_NFC_OK;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == message, NET_NFC_NULL_PARAMETER);

	RETV_IF(NULL == ndef_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_NOT_CONNECTED);

	arg_data = net_nfc_util_gdbus_ndef_message_to_variant(message);

	ret = net_nfc_gdbus_ndef_call_write_sync(ndef_proxy ,
			GPOINTER_TO_UINT(handle),
			arg_data,
			net_nfc_client_gdbus_get_privilege(),
			(gint *)&out_result,
			NULL,
			&error);

	if (FALSE == ret)
	{
		NFC_ERR("can not call write: %s", error->message);
		g_error_free(error);
		out_result = NET_NFC_IPC_FAIL;
	}

	return out_result;
}

API net_nfc_error_e net_nfc_client_ndef_make_read_only(
		net_nfc_target_handle_s *handle,
		net_nfc_client_ndef_make_read_only_completed callback,
		void *user_data)
{
	NetNfcCallback *func_data;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == ndef_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_NOT_CONNECTED);
	RETV_IF(ndef_is_supported_tag() == FALSE, NET_NFC_NOT_SUPPORTED);

	func_data = g_try_new0(NetNfcCallback, 1);
	if (NULL == func_data)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_ndef_call_make_read_only(ndef_proxy,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			ndef_call_make_read_only,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_ndef_make_read_only_sync(
		net_nfc_target_handle_s *handle)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e out_result = NET_NFC_OK;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == ndef_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_NOT_CONNECTED);
	RETV_IF(ndef_is_supported_tag() == FALSE, NET_NFC_NOT_SUPPORTED);

	ret = net_nfc_gdbus_ndef_call_make_read_only_sync(ndef_proxy,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			(gint *)&out_result,
			NULL,
			&error);

	if (FALSE == ret)
	{
		NFC_ERR("can not make read only: %s", error->message);
		g_error_free(error);
		out_result = NET_NFC_IPC_FAIL;
	}

	return out_result;
}

API net_nfc_error_e net_nfc_client_ndef_format(net_nfc_target_handle_s *handle,
		data_s *key, net_nfc_client_ndef_format_completed callback, void *user_data)
{
	GVariant *arg_data;
	NetNfcCallback *func_data;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == ndef_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_NOT_CONNECTED);

	func_data = g_try_new0(NetNfcCallback, 1);
	if (NULL == func_data)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	arg_data = net_nfc_util_gdbus_data_to_variant((data_s *)key);

	net_nfc_gdbus_ndef_call_format(ndef_proxy,
			GPOINTER_TO_UINT(handle),
			arg_data,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			ndef_call_format,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_ndef_format_sync(
		net_nfc_target_handle_s *handle, data_s *key)
{
	gboolean ret;
	GVariant *arg_data;
	GError *error = NULL;
	net_nfc_error_e out_result = NET_NFC_OK;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == ndef_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);
	RETV_IF(net_nfc_client_tag_is_connected() == FALSE, NET_NFC_NOT_CONNECTED);

	arg_data = net_nfc_util_gdbus_data_to_variant((data_s *)key);

	ret = net_nfc_gdbus_ndef_call_format_sync(ndef_proxy,
			GPOINTER_TO_UINT(handle),
			arg_data,
			net_nfc_client_gdbus_get_privilege(),
			(gint *)&out_result,
			NULL,
			&error);

	if (FALSE == ret)
	{
		NFC_ERR("can not call format: %s", error->message);
		g_error_free(error);
		out_result = NET_NFC_IPC_FAIL;
	}

	return out_result;
}

net_nfc_error_e net_nfc_client_ndef_init(void)
{
	GError *error = NULL;

	if (ndef_proxy)
	{
		NFC_WARN("Already initialized");
		return NET_NFC_OK;
	}

	ndef_proxy = net_nfc_gdbus_ndef_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Ndef",
			NULL,
			&error);

	if (NULL == ndef_proxy)
	{
		NFC_ERR("Can not create proxy : %s", error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	return NET_NFC_OK;
}

void net_nfc_client_ndef_deinit(void)
{
	if (ndef_proxy)
	{
		g_object_unref(ndef_proxy);
		ndef_proxy = NULL;
	}
}
