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

#include <string.h>

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_tag_internal.h"
#include "net_nfc_client_transceive.h"

static NetNfcGDbusTransceive *transceive_proxy = NULL;

static GVariant *transceive_data_to_transceive_variant(
		net_nfc_target_type_e devType, data_s *data)
{
	GVariant *variant;
	data_s transceive_info = { NULL, };

	RETV_IF(NULL == data, NULL);

	switch (devType)
	{
	case NET_NFC_MIFARE_MINI_PICC :
	case NET_NFC_MIFARE_1K_PICC :
	case NET_NFC_MIFARE_4K_PICC :
	case NET_NFC_MIFARE_ULTRA_PICC :
		if (net_nfc_util_alloc_data(&transceive_info, data->length + 2) == true)
		{
			memcpy(transceive_info.buffer, data->buffer, data->length);

			net_nfc_util_compute_CRC(CRC_A, transceive_info.buffer, transceive_info.length);
		}
		break;

	case NET_NFC_JEWEL_PICC :
		if (data->length > 9)
		{
			NFC_ERR("data length is larger than 9");
			return NULL;
		}

		if (net_nfc_util_alloc_data(&transceive_info, 9) == true)
		{
			memcpy(transceive_info.buffer, data->buffer, data->length);

			net_nfc_util_compute_CRC(CRC_B, transceive_info.buffer, transceive_info.length);
		}
		break;

	default :
		if(net_nfc_util_alloc_data(&transceive_info, data->length) == true)
			memcpy(transceive_info.buffer, data->buffer, data->length);

		break;
	}

	variant = net_nfc_util_gdbus_data_to_variant(&transceive_info);

	net_nfc_util_free_data(&transceive_info);

	return variant;
}

static void transceive_data_call(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	data_s resp = { NULL, };
	GVariant *out_data = NULL;
	net_nfc_error_e out_result;
	nfc_transceive_data_callback callback;
	NetNfcCallback *func_data = user_data;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_transceive_call_transceive_data_finish(
				NET_NFC_GDBUS_TRANSCEIVE(source_object),
				(gint *)&out_result,
				&out_data,
				res,
				&error);

	if (FALSE == ret)
	{
		NFC_ERR("Can not finish transceive: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		data_s resp = { NULL, };

		net_nfc_util_gdbus_variant_to_data_s(out_data, &resp);

		((nfc_transceive_data_callback)func_data->callback)(
			out_result,
			&resp,
			func_data->user_data);

		net_nfc_util_free_data(&resp);
	}

	g_free(func_data);
}

static void transceive_call(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e out_result;
	nfc_transceive_callback callback;
	NetNfcCallback *func_data = user_data;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_transceive_call_transceive_finish(
				NET_NFC_GDBUS_TRANSCEIVE(source_object),
				(gint *)&out_result,
				res,
				&error);

	if (FALSE == ret)
	{
		NFC_ERR("Can not finish transceive: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback)
	{
		callback = (nfc_transceive_callback)func_data->callback;
		callback(out_result, func_data->user_data);
	}

	g_free(func_data);
}

API net_nfc_error_e net_nfc_client_transceive(net_nfc_target_handle_s *handle,
		data_s *rawdata, nfc_transceive_callback callback, void *user_data)
{
	GVariant *arg_data;
	NetNfcCallback *funcdata;
	net_nfc_target_info_s *target_info;

	if (NULL == transceive_proxy)
	{
		NFC_ERR("Can not get TransceiveProxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == rawdata, NET_NFC_NULL_PARAMETER);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	target_info = net_nfc_client_tag_get_client_target_info();

	if (NULL == target_info)
	{
		NFC_ERR("target_info is NULL");
		return NET_NFC_NOT_CONNECTED;
	}

	if (NULL == target_info->handle)
	{
		NFC_ERR("target_info->handle is NULL");
		return NET_NFC_NOT_CONNECTED;
	}

	NFC_DBG("send request :: transceive = [%p]", handle);

	arg_data = transceive_data_to_transceive_variant(target_info->devType, rawdata);
	if (NULL == arg_data)
		return NET_NFC_INVALID_PARAM;

	funcdata = g_try_new0(NetNfcCallback, 1);
	if (NULL == funcdata)
	{
		g_variant_unref(arg_data);

		return NET_NFC_ALLOC_FAIL;
	}

	funcdata->callback = (gpointer)callback;
	funcdata->user_data = user_data;

	net_nfc_gdbus_transceive_call_transceive(transceive_proxy,
			GPOINTER_TO_UINT(handle),
			target_info->devType,
			arg_data,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			transceive_call,
			funcdata);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_transceive_data(net_nfc_target_handle_s *handle,
		data_s *rawdata, nfc_transceive_data_callback callback, void *user_data)
{
	GVariant *arg_data;
	NetNfcCallback *funcdata;
	net_nfc_target_info_s *target_info;

	RETV_IF(NULL == transceive_proxy, NET_NFC_NOT_INITIALIZED);

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == rawdata, NET_NFC_NULL_PARAMETER);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	target_info = net_nfc_client_tag_get_client_target_info();

	if (NULL == target_info)
	{
		NFC_ERR("target_info is NULL");
		return NET_NFC_NOT_CONNECTED;
	}

	if (NULL == target_info->handle)
	{
		NFC_ERR("target_info->handle is NULL");
		return NET_NFC_NOT_CONNECTED;
	}

	NFC_DBG("send request :: transceive = [%p]", handle);

	arg_data = transceive_data_to_transceive_variant(target_info->devType, rawdata);
	if (NULL == arg_data)
		return NET_NFC_INVALID_PARAM;

	funcdata = g_try_new0(NetNfcCallback, 1);
	if (NULL == funcdata)
	{
		g_variant_unref(arg_data);

		return NET_NFC_ALLOC_FAIL;
	}

	funcdata->callback = (gpointer)callback;
	funcdata->user_data = user_data;

	net_nfc_gdbus_transceive_call_transceive_data(transceive_proxy,
			GPOINTER_TO_UINT(handle),
			target_info->devType,
			arg_data,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			transceive_data_call,
			funcdata);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_transceive_sync(net_nfc_target_handle_s *handle,
		data_s *rawdata)
{
	gboolean ret;
	GVariant *arg_data;
	GError *error = NULL;
	net_nfc_target_info_s *target_info;
	net_nfc_error_e out_result = NET_NFC_OK;

	RETV_IF(NULL == transceive_proxy, NET_NFC_NOT_INITIALIZED);

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == rawdata, NET_NFC_NULL_PARAMETER);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	target_info = net_nfc_client_tag_get_client_target_info();

	if (NULL == target_info)
	{
		NFC_ERR("target_info is NULL");
		return NET_NFC_NOT_CONNECTED;
	}

	if (NULL == target_info->handle)
	{
		NFC_ERR("target_info->handle is NULL");
		return NET_NFC_NOT_CONNECTED;
	}

	NFC_DBG("send request :: transceive = [%p]", handle);

	arg_data = transceive_data_to_transceive_variant(target_info->devType, rawdata);
	if (NULL == arg_data)
		return NET_NFC_ALLOC_FAIL;

	ret = net_nfc_gdbus_transceive_call_transceive_sync(transceive_proxy,
				GPOINTER_TO_UINT(handle),
				target_info->devType,
				arg_data,
				net_nfc_client_gdbus_get_privilege(),
				(gint *)&out_result,
				NULL,
				&error);

	if (FALSE == ret)
	{
		NFC_ERR("Transceive (sync call) failed: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	return out_result;
}

API net_nfc_error_e net_nfc_client_transceive_data_sync(
		net_nfc_target_handle_s *handle, data_s *rawdata, data_s **response)
{
	gboolean ret;
	GVariant *arg_data;
	GError *error = NULL;
	GVariant *out_data = NULL;
	net_nfc_target_info_s *target_info;
	net_nfc_error_e out_result = NET_NFC_OK;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == rawdata, NET_NFC_NULL_PARAMETER);

	RETV_IF(NULL == transceive_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	target_info = net_nfc_client_tag_get_client_target_info();
	if (NULL == target_info)
	{
		NFC_ERR("target_info is NULL");
		return NET_NFC_NOT_CONNECTED;
	}

	if (NULL == target_info->handle)
	{
		NFC_ERR("target_info->handle is NULL");
		return NET_NFC_NOT_CONNECTED;
	}

	NFC_DBG("send request :: transceive = [%p]", handle);

	arg_data = transceive_data_to_transceive_variant(target_info->devType, rawdata);
	if (NULL == arg_data)
		return NET_NFC_ALLOC_FAIL;

	ret = net_nfc_gdbus_transceive_call_transceive_data_sync(
				transceive_proxy,
				GPOINTER_TO_UINT(handle),
				target_info->devType,
				arg_data,
				net_nfc_client_gdbus_get_privilege(),
				(gint *)&out_result,
				&out_data,
				NULL,
				&error);

	if (FALSE == ret)
	{
		NFC_ERR("Transceive (sync call) failed: %s", error->message);
		g_error_free(error);

		out_result = NET_NFC_IPC_FAIL;
	}

	if (response && out_data != NULL)
		*response = net_nfc_util_gdbus_variant_to_data(out_data);

	return out_result;
}


net_nfc_error_e net_nfc_client_transceive_init(void)
{
	GError *error = NULL;

	if (transceive_proxy)
	{
		NFC_WARN("Already initialized");
		return NET_NFC_OK;
	}

	transceive_proxy = net_nfc_gdbus_transceive_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/Transceive",
			NULL,
			&error);
	if (NULL == transceive_proxy)
	{
		NFC_ERR("Can not create proxy : %s", error->message);
		g_error_free(error);

		return NET_NFC_UNKNOWN_ERROR;
	}

	return NET_NFC_OK;
}

void net_nfc_client_transceive_deinit(void)
{
	if (transceive_proxy)
	{
		g_object_unref(transceive_proxy);
		transceive_proxy = NULL;
	}
}
