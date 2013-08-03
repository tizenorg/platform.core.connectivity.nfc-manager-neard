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
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_data.h"
#include "net_nfc_ndef_message.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_ndef.h"
#include "net_nfc_client_tag_internal.h"

#ifndef NET_NFC_EXPORT_API
#define NET_NFC_EXPORT_API __attribute__((visibility("default")))
#endif

typedef struct _NdefFuncData NdefFuncData;

struct _NdefFuncData
{
	gpointer callback;
	gpointer user_data;
};

static NetNfcGDbusNdef *ndef_proxy = NULL;

static ndef_message_h ndef_variant_to_message(GVariant *variant);

static GVariant *ndef_message_to_variant(ndef_message_h message);

static gboolean ndef_is_supported_tag(void);

static void ndef_call_read(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data);

static void ndef_call_write(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data);

static void ndef_call_make_read_only(GObject *source_object,
				GAsyncResult *res,
				gpointer user_data);

static void ndef_call_format(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data);

static ndef_message_h ndef_variant_to_message(GVariant *variant)
{
	data_s data;
	ndef_message_h message = NULL;

	net_nfc_util_gdbus_variant_to_data_s(variant, &data);

	if (data.buffer && data.length > 0)
	{
		if (net_nfc_create_ndef_message_from_rawdata(&message, &data)
			!= NET_NFC_OK)
		{
			DEBUG_ERR_MSG("memory alloc fail...");
		}

		net_nfc_util_free_data(&data);
	}

	return message;
}

static GVariant *ndef_message_to_variant(ndef_message_h message)
{
	guint length;
	data_s data;
	GVariant *variant = NULL;

	length = net_nfc_util_get_ndef_message_length(
						(ndef_message_s *)message);

	if (length == 0)
	{
		DEBUG_ERR_MSG("message length is 0");
		return NULL;
	}

	data.length = length;
	data.buffer = g_new0(guint8, length);

	if(net_nfc_util_convert_ndef_message_to_rawdata(
						(ndef_message_s *)message,
						&data) != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("can not convert ndef_message to rawdata");
		return NULL;
	}

	variant = net_nfc_util_gdbus_data_to_variant(&data);

	g_free(data.buffer);

	return variant;
}

static gboolean ndef_is_supported_tag(void)
{
	net_nfc_target_info_s *target_info = NULL;

	target_info = net_nfc_client_tag_get_client_target_info();

	if (target_info == NULL)
	{
		DEBUG_ERR_MSG("target_info does not exist");
		return TRUE;
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
			DEBUG_CLIENT_MSG(
				"not supported tag for read only tag");
			return FALSE;
	}
}

static void ndef_call_read(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	NdefFuncData *func_data;

	net_nfc_error_e out_result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_ndef_read_completed callback;

	GVariant *out_data = NULL;
	ndef_message_h message = NULL;

	if (net_nfc_gdbus_ndef_call_read_finish(
				NET_NFC_GDBUS_NDEF(source_object),
				(gint *)&out_result,
				&out_data,
				res,
				&error) == FALSE)
	{
		out_result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish read: %s", error->message);
		g_error_free(error);
	}

	func_data = (NdefFuncData *)user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get NdefFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not avaiilable");
		g_free(func_data);
		return;
	}

	if (out_result == NET_NFC_OK)
		message = ndef_variant_to_message(out_data);

	callback = (net_nfc_client_ndef_read_completed)func_data->callback;
	callback(out_result, message, func_data->user_data);

	if (message)
		net_nfc_util_free_ndef_message(message);

	g_free(func_data);
}

static void ndef_call_write(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	NdefFuncData *func_data;

	net_nfc_error_e out_result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_ndef_write_completed callback;

	if (net_nfc_gdbus_ndef_call_write_finish(
				NET_NFC_GDBUS_NDEF(source_object),
				(gint *)&out_result,
				res,
				&error) == FALSE)
	{
		out_result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish write: %s", error->message);
		g_error_free(error);
	}

	func_data = (NdefFuncData *)user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get NdefFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not avaiilable");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_ndef_write_completed)func_data->callback;
	callback(out_result, func_data->user_data);

	g_free(func_data);
}

static void ndef_call_make_read_only(GObject *source_object,
				GAsyncResult *res,
				gpointer user_data)
{
	NdefFuncData *func_data;

	net_nfc_error_e out_result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_ndef_make_read_only_completed callback;

	if (net_nfc_gdbus_ndef_call_make_read_only_finish(
				NET_NFC_GDBUS_NDEF(source_object),
				(gint *)&out_result,
				res,
				&error) == FALSE)
	{
		out_result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish make read only: %s",
				error->message);
		g_error_free(error);
	}

	func_data = (NdefFuncData *)user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get NdefFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not avaiilable");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_ndef_make_read_only_completed)
							func_data->callback;
	callback(out_result, func_data->user_data);

	g_free(func_data);
}

static void ndef_call_format(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	NdefFuncData *func_data;

	net_nfc_error_e out_result = NET_NFC_OK;
	GError *error = NULL;

	net_nfc_client_ndef_format_completed callback;

	if (net_nfc_gdbus_ndef_call_format_finish(
				NET_NFC_GDBUS_NDEF(source_object),
				(gint *)&out_result,
				res,
				&error) == FALSE)
	{
		out_result = NET_NFC_UNKNOWN_ERROR;

		DEBUG_ERR_MSG("Can not finish format: %s", error->message);
		g_error_free(error);
	}

	func_data = (NdefFuncData *)user_data;
	if (func_data == NULL)
	{
		DEBUG_ERR_MSG("can not get NdefFuncData");
		return;
	}

	if (func_data->callback == NULL)
	{
		DEBUG_CLIENT_MSG("callback function is not avaiilable");
		g_free(func_data);
		return;
	}

	callback = (net_nfc_client_ndef_format_completed) func_data->callback;
	callback(out_result, func_data->user_data);

	g_free(func_data);
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_ndef_read(net_nfc_target_handle_h handle,
				net_nfc_client_ndef_read_completed callback,
				void *user_data)
{
	NdefFuncData *func_data;

	if (ndef_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get NdefProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	DEBUG_CLIENT_MSG("send reqeust :: read ndef = [%p]", handle);

	if (handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (net_nfc_client_tag_is_connected() == FALSE)
		return NET_NFC_OPERATION_FAIL;

	func_data = g_new0(NdefFuncData, 1);

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_ndef_call_read(ndef_proxy,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			ndef_call_read,
			func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_ndef_read_sync(net_nfc_target_handle_h handle,
					ndef_message_h *message)
{
	GVariant *out_data = NULL;
	GError *error = NULL;

	net_nfc_error_e out_result = NET_NFC_OK;

	if (ndef_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get NdefProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	DEBUG_CLIENT_MSG("send reqeust :: read ndef = [%p]", handle);

	if (handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (net_nfc_client_tag_is_connected() == FALSE)
		return NET_NFC_OPERATION_FAIL;

	if (net_nfc_gdbus_ndef_call_read_sync(ndef_proxy,
					GPOINTER_TO_UINT(handle),
					net_nfc_client_gdbus_get_privilege(),
					(gint *)&out_result,
					&out_data,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not call read: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	*message = ndef_variant_to_message(out_data);

	return out_result;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_ndef_write(net_nfc_target_handle_h handle,
				ndef_message_h message,
				net_nfc_client_ndef_write_completed callback,
				void *user_data)
{
	NdefFuncData *func_data;

	GVariant *arg_data = NULL;

	if (ndef_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get NdefProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (message == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (net_nfc_client_tag_is_connected() == FALSE)
		return NET_NFC_OPERATION_FAIL;

	func_data = g_new0(NdefFuncData, 1);

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	arg_data = ndef_message_to_variant(message);
	if (arg_data == NULL)
		return NET_NFC_INVALID_PARAM;

	net_nfc_gdbus_ndef_call_write(ndef_proxy,
				GPOINTER_TO_UINT(handle),
				arg_data,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				ndef_call_write,
				func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_ndef_write_sync(net_nfc_target_handle_h handle,
					ndef_message_h message)
{
	GVariant *arg_data = NULL;
	GError *error = NULL;

	net_nfc_error_e out_result = NET_NFC_OK;

	if (ndef_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get NdefProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (message == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (net_nfc_client_tag_is_connected() == FALSE)
		return NET_NFC_OPERATION_FAIL;

	arg_data = ndef_message_to_variant(message);
	if (arg_data == NULL)
		return NET_NFC_INVALID_PARAM;

	if (net_nfc_gdbus_ndef_call_write_sync(ndef_proxy ,
					GPOINTER_TO_UINT(handle),
					arg_data,
					net_nfc_client_gdbus_get_privilege(),
					(gint *)&out_result,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not call write: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	return out_result;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_ndef_make_read_only(
			net_nfc_target_handle_h handle,
			net_nfc_client_ndef_make_read_only_completed callback,
			void *user_data)
{
	NdefFuncData *func_data;

	if (ndef_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get NdefProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (net_nfc_client_tag_is_connected() == FALSE)
		return NET_NFC_OPERATION_FAIL;

	if (ndef_is_supported_tag() == FALSE)
		return NET_NFC_NOT_SUPPORTED;

	func_data = g_new0(NdefFuncData, 1);

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

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_ndef_make_read_only_sync(
					net_nfc_target_handle_h handle)
{
	GError *error = NULL;

	net_nfc_error_e out_result = NET_NFC_OK;

	if (ndef_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get NdefProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (net_nfc_client_tag_is_connected() == FALSE)
		return NET_NFC_OPERATION_FAIL;

	if (ndef_is_supported_tag() == FALSE)
		return NET_NFC_NOT_SUPPORTED;

	if (net_nfc_gdbus_ndef_call_make_read_only_sync(ndef_proxy,
						GPOINTER_TO_UINT(handle),
						net_nfc_client_gdbus_get_privilege(),
						(gint *)&out_result,
						NULL,
						&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not make read only: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	return out_result;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_ndef_format(net_nfc_target_handle_h handle,
				data_h key,
				net_nfc_client_ndef_format_completed callback,
				void *user_data)
{
	NdefFuncData *func_data;
	GVariant *arg_data = NULL;


	if (ndef_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get NdefProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (key == NULL)
		arg_data = net_nfc_util_gdbus_buffer_to_variant(NULL, 0);
	else
	{
		data_s *key_s;

		key_s = (data_s *)key;
		arg_data = net_nfc_util_gdbus_data_to_variant(key_s);
	}

	if (arg_data == NULL)
		return NET_NFC_INVALID_PARAM;

	func_data = g_new0(NdefFuncData, 1);

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_ndef_call_format(ndef_proxy ,
				GPOINTER_TO_UINT(handle),
				arg_data,
				net_nfc_client_gdbus_get_privilege(),
				NULL,
				ndef_call_format,
				func_data);

	return NET_NFC_OK;
}

NET_NFC_EXPORT_API
net_nfc_error_e net_nfc_client_ndef_format_sync(
					net_nfc_target_handle_h handle,
					data_h key)
{
	GVariant *arg_data = NULL;
	GError *error = NULL;

	net_nfc_error_e out_result = NET_NFC_OK;

	if (ndef_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get NdefProxy");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (handle == NULL)
		return NET_NFC_NULL_PARAMETER;

	if (key == NULL)
		arg_data = net_nfc_util_gdbus_buffer_to_variant(NULL, 0);
	else
	{
		data_s *key_s;

		key_s = (data_s *)key;
		arg_data = net_nfc_util_gdbus_data_to_variant(key_s);
	}

	if (arg_data == NULL)
		return NET_NFC_INVALID_PARAM;

	if (net_nfc_gdbus_ndef_call_format_sync(ndef_proxy ,
					GPOINTER_TO_UINT(handle),
					arg_data,
					net_nfc_client_gdbus_get_privilege(),
					(gint *)&out_result,
					NULL,
					&error) == FALSE)
	{
		DEBUG_ERR_MSG("can not call format: %s",
				error->message);
		g_error_free(error);
		return NET_NFC_UNKNOWN_ERROR;
	}

	return out_result;
}

net_nfc_error_e net_nfc_client_ndef_init(void)
{
	GError *error = NULL;

	if (ndef_proxy)
	{
		DEBUG_CLIENT_MSG("Already initialized");
		return NET_NFC_OK;
	}

	ndef_proxy = net_nfc_gdbus_ndef_proxy_new_for_bus_sync(
				G_BUS_TYPE_SYSTEM,
				G_DBUS_PROXY_FLAGS_NONE,
				"org.tizen.NetNfcService",
				"/org/tizen/NetNfcService/Ndef",
				NULL,
				&error);

	if (ndef_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not create proxy : %s", error->message);
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
