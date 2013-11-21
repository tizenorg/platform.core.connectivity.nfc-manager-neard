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
#include <vconf.h>

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_client.h"
#include "net_nfc_client_manager.h"
#include "net_nfc_client_se.h"

typedef struct _SeFuncData SeFuncData;

struct _SeFuncData
{
	gpointer se_callback;
	gpointer se_data;
};

typedef struct _SeEventHandler SeEventHandler;

struct _SeEventHandler
{
	net_nfc_client_se_event se_event_cb;
	gpointer se_event_data;
};

typedef struct _SeTransEventHandler SeTransEventHandler;

struct _SeTransEventHandler
{
	net_nfc_se_type_e se_type;
	net_nfc_client_se_transaction_event eSE_transaction_event_cb;
	net_nfc_client_se_transaction_event UICC_transaction_event_cb;
	gpointer eSE_transaction_event_data;
	gpointer UICC_transaction_event_data;
};

typedef struct _SeESEDetectedHandler SeESEDetectedHandler;

struct _SeESEDetectedHandler
{
	net_nfc_client_se_ese_detected_event se_ese_detected_cb;
	gpointer se_ese_detected_data;
};


static NetNfcGDbusSecureElement *se_proxy = NULL;

static SeEventHandler se_eventhandler;
static SeTransEventHandler se_transeventhandler;
static SeESEDetectedHandler se_esedetecthandler;

static void se_ese_detected(GObject *source_object, guint arg_handle,
		gint arg_se_type, GVariant *arg_data)
{
	data_s buffer_data = { NULL, 0 };
	net_nfc_client_se_ese_detected_event callback;

	NFC_INFO(">>> SIGNAL arrived");

	RET_IF(NULL == se_esedetecthandler.se_ese_detected_cb);

	net_nfc_util_gdbus_variant_to_data_s(arg_data, &buffer_data);

	callback = se_esedetecthandler.se_ese_detected_cb;
	callback((net_nfc_target_handle_s*)arg_handle, arg_se_type, &buffer_data,
			se_esedetecthandler.se_ese_detected_data);

	net_nfc_util_free_data(&buffer_data);
}


static void se_type_changed(GObject *source_object, gint arg_se_type)
{
	net_nfc_client_se_event callback;

	NFC_INFO(">>> SIGNAL arrived");

	RET_IF(NULL == se_eventhandler.se_event_cb);

	callback = se_eventhandler.se_event_cb;
	callback((net_nfc_message_e)NET_NFC_MESSAGE_SE_TYPE_CHANGED,
		se_eventhandler.se_event_data);
}


static void se_transaction_event(GObject *source_object,
		gint arg_se_type,
		GVariant *arg_aid,
		GVariant *arg_param,
		gint fg_dispatch,
		gint focus_app_pid)
{
	void *user_data = NULL;
	data_s aid = { NULL, 0 };
	data_s param = { NULL, 0 };
	net_nfc_client_se_transaction_event callback;

	NFC_INFO(">>> SIGNAL arrived");

	RET_IF(NULL == se_transeventhandler.eSE_transaction_event_cb);
	RET_IF(NULL == se_transeventhandler.UICC_transaction_event_cb);

	if (se_transeventhandler.se_type == arg_se_type)
	{
		pid_t mypid = getpid();
		if(fg_dispatch == false ||
				(fg_dispatch == true && focus_app_pid == (getpgid(mypid))))
		{
			net_nfc_util_gdbus_variant_to_data_s(arg_aid, &aid);
			net_nfc_util_gdbus_variant_to_data_s(arg_param, &param);

			if(arg_se_type == NET_NFC_SE_TYPE_ESE)
			{
				callback = se_transeventhandler.eSE_transaction_event_cb;
				user_data = se_transeventhandler.eSE_transaction_event_data;
			}
			else if(arg_se_type == NET_NFC_SE_TYPE_UICC)
			{
				callback = se_transeventhandler.UICC_transaction_event_cb;
				user_data = se_transeventhandler.UICC_transaction_event_data;
			}

			callback(arg_se_type, &aid, &param, user_data);

			net_nfc_util_free_data(&param);
			net_nfc_util_free_data(&aid);
		}
	}
}

static void se_card_emulation_mode_changed(GObject *source_object,
		gint arg_se_type)
{
	net_nfc_client_se_event callback;
	NFC_DBG(">>> SIGNAL arrived");

	RET_IF(NULL == se_eventhandler.se_event_cb);

	callback = se_eventhandler.se_event_cb;
	callback((net_nfc_message_e)NET_NFC_MESSAGE_SE_CARD_EMULATION_CHANGED,
			se_eventhandler.se_event_data);
}

static void set_secure_element(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	net_nfc_se_set_se_cb se_callback;
	SeFuncData *func_data = user_data;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_secure_element_call_set_finish(se_proxy, &result, res, &error);

	if(FALSE == ret)
	{
		NFC_ERR("Could not set secure element: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->se_callback != NULL)
	{
		se_callback = (net_nfc_se_set_se_cb)func_data->se_callback;

		se_callback(result, func_data->se_data);
	}

	g_free(func_data);
}

static void get_secure_element(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	gint type = 0;
	GError *error = NULL;
	net_nfc_error_e result;
	net_nfc_se_get_se_cb se_callback;
	NetNfcCallback *func_data = user_data;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_secure_element_call_get_finish(se_proxy,
			&result, &type, res, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Could not set secure element: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		se_callback = (net_nfc_se_get_se_cb)func_data->callback;

		se_callback(result, type, func_data->user_data);
	}

	g_free(func_data);
}

static void _set_card_emulation_cb(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	net_nfc_se_set_se_cb se_callback;
	NetNfcCallback *func_data = user_data;

	g_assert(user_data != NULL);


	ret = net_nfc_gdbus_secure_element_call_set_card_emulation_finish(
			se_proxy, &result, res, &error);

	if(FALSE == ret)
	{
		NFC_ERR("Could not set card emulation: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		se_callback = (net_nfc_se_set_se_cb)func_data->callback;

		se_callback(result, func_data->user_data);
	}

	g_free(func_data);
}


static void open_secure_element(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	guint out_handle = 0;
	net_nfc_error_e result;
	SeFuncData *func_data = user_data;
	net_nfc_se_open_se_cb se_callback;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_secure_element_call_open_secure_element_finish(
			se_proxy, &result, &out_handle, res, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Could not open secure element: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->se_callback != NULL)
	{
		se_callback = (net_nfc_se_open_se_cb)func_data->se_callback;

		se_callback(result, (net_nfc_target_handle_s*)out_handle, func_data->se_data);
	}

	g_free(func_data);
}


static void close_secure_element(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	SeFuncData *func_data = user_data;
	net_nfc_se_close_se_cb se_callback;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_secure_element_call_close_secure_element_finish(
			se_proxy, &result, res, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Could not close secure element: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->se_callback != NULL)
	{
		se_callback = (net_nfc_se_close_se_cb)func_data->se_callback;

		se_callback(result, func_data->se_data);
	}

	g_free(func_data);
}


static void send_apdu_secure_element(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	GVariant *out_response = NULL;
	SeFuncData *func_data = user_data;
	net_nfc_se_send_apdu_cb se_callback;

	g_assert(user_data != NULL);

	ret = net_nfc_gdbus_secure_element_call_send_apdu_finish(
			se_proxy, &result, &out_response, res, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Could not send apdu: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->se_callback != NULL)
	{
		se_callback = (net_nfc_se_send_apdu_cb)func_data->se_callback;
		data_s data = { NULL, };

		net_nfc_util_gdbus_variant_to_data_s(out_response, &data);

		se_callback(result, &data, func_data->se_data);

		net_nfc_util_free_data(&data);
	}

	g_free(func_data);
}


static void get_atr_secure_element(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result;
	GVariant *out_atr = NULL;
	SeFuncData *func_data = user_data;
	net_nfc_se_get_atr_cb se_callback;

	g_assert(user_data != NULL);

	RET_IF(NULL == func_data->se_callback);

	ret = net_nfc_gdbus_secure_element_call_get_atr_finish(
			se_proxy, &result, &out_atr, res, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Could not get atr: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->se_callback != NULL)
	{
		se_callback = (net_nfc_se_get_atr_cb)func_data->se_callback;
		data_s data = { NULL, };

		net_nfc_util_gdbus_variant_to_data_s(out_atr, &data);

		se_callback(result, &data, func_data->se_data);

		net_nfc_util_free_data(&data);
	}

	g_free(func_data);
}


API net_nfc_error_e net_nfc_client_se_set_secure_element_type(
		net_nfc_se_type_e se_type, net_nfc_se_set_se_cb callback, void *user_data)
{
	SeFuncData *func_data;

	RETV_IF(NULL == se_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	func_data = g_try_new0(SeFuncData, 1);
	if (NULL == func_data)
		return NET_NFC_ALLOC_FAIL;

	func_data->se_callback = (gpointer)callback;
	func_data->se_data = user_data;

	net_nfc_gdbus_secure_element_call_set(se_proxy, (gint)se_type,
			net_nfc_client_gdbus_get_privilege(), NULL, set_secure_element, func_data);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_se_set_secure_element_type_sync(
		net_nfc_se_type_e se_type)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == se_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	ret = net_nfc_gdbus_secure_element_call_set_sync(se_proxy, (gint)se_type,
			net_nfc_client_gdbus_get_privilege(), &result, NULL, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Set secure element failed: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_se_get_secure_element_type(
		net_nfc_se_get_se_cb callback, void *user_data)
{
	NetNfcCallback *func_data;

	RETV_IF(NULL == se_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	func_data = g_try_new0(NetNfcCallback, 1);
	if (NULL == func_data)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_secure_element_call_get(se_proxy, net_nfc_client_gdbus_get_privilege(),
			NULL, get_secure_element, func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_se_get_secure_element_type_sync(
		net_nfc_se_type_e *se_type)
{
	gint type;
	net_nfc_error_e result = NET_NFC_OK;

#if 0
	gboolean ret;
	GError *error = NULL;
#endif
	RETV_IF(NULL == se_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

#if 1
	if (vconf_get_int(VCONFKEY_NFC_SE_TYPE, &type) == 0)
		*se_type = type;
	else
		result = NET_NFC_OPERATION_FAIL;
#else
	ret = net_nfc_gdbus_secure_element_call_get_sync(se_proxy,
			net_nfc_client_gdbus_get_privilege(), &result, (gint)&type, NULL, &error);

	if (TRUE == ret)
	{
		*se_type = type;
	}
	else
	{
		NFC_ERR("Set secure element failed: %s", error->message);

		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}
#endif
	return result;
}

API net_nfc_error_e net_nfc_set_card_emulation_mode(
		net_nfc_card_emulation_mode_t mode,
		net_nfc_se_set_card_emulation_cb callback,
		void *user_data)
{
	NetNfcCallback *func_data;

	RETV_IF(NULL == se_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	func_data = g_try_new0(NetNfcCallback, 1);
	if (NULL == func_data)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_secure_element_call_set_card_emulation(se_proxy, (gint)mode,
			net_nfc_client_gdbus_get_privilege(), NULL, _set_card_emulation_cb, func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_get_card_emulation_mode(net_nfc_se_type_e *type)
{
	int ret;
	int se_type;
	net_nfc_error_e result = NET_NFC_OK;

	ret = vconf_get_int(VCONFKEY_NFC_SE_TYPE, &se_type);

	if (0 == ret)
	{
		switch(se_type)
		{
			case VCONFKEY_NFC_SE_TYPE_UICC:
				*type = NET_NFC_SE_TYPE_UICC;
				break;

			case VCONFKEY_NFC_SE_TYPE_ESE:
				*type = NET_NFC_SE_TYPE_ESE;
				break;

			default :
				*type = NET_NFC_SE_TYPE_NONE;
				break;
		}
	}
	else
	{
		result = NET_NFC_UNKNOWN_ERROR;
	}

	return result;
}

API net_nfc_error_e net_nfc_set_card_emulation_mode_sync(
		net_nfc_card_emulation_mode_t mode)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == se_proxy, NET_NFC_NOT_INITIALIZED);

	/* prevent executing daemon when nfc is off */
	RETV_IF(net_nfc_client_manager_is_activated() == false, NET_NFC_INVALID_STATE);

	ret = net_nfc_gdbus_secure_element_call_set_card_emulation_sync(se_proxy, (gint)mode,
			net_nfc_client_gdbus_get_privilege(), &result, NULL, &error);

	if (FALSE == ret)
	{
		NFC_ERR("Set card emulation failed: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	return result;
}


API net_nfc_error_e net_nfc_client_se_open_internal_secure_element(
		net_nfc_se_type_e se_type, net_nfc_se_open_se_cb callback, void *user_data)
{
	SeFuncData *func_data;

	RETV_IF(NULL == se_proxy, NET_NFC_NOT_INITIALIZED);

	/* allow this function even nfc is off */

	func_data = g_try_new0(SeFuncData, 1);
	if (NULL == func_data)
		return NET_NFC_ALLOC_FAIL;

	func_data->se_callback = (gpointer)callback;
	func_data->se_data = user_data;

	net_nfc_gdbus_secure_element_call_open_secure_element(se_proxy, (gint)se_type,
			net_nfc_client_gdbus_get_privilege(), NULL, open_secure_element, func_data);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_se_open_internal_secure_element_sync(
		net_nfc_se_type_e se_type, net_nfc_target_handle_s **handle)
{
	gboolean ret;
	guint out_handle = 0;
	GError *error =  NULL;
	net_nfc_error_e result = NET_NFC_OK;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == se_proxy, NET_NFC_NOT_INITIALIZED);

	/* allow this function even nfc is off */

	ret = net_nfc_gdbus_secure_element_call_open_secure_element_sync(se_proxy, se_type,
			net_nfc_client_gdbus_get_privilege(), &result, &out_handle, NULL, &error);

	if (TRUE == ret)
	{
		*handle = GUINT_TO_POINTER(out_handle);
	}
	else
	{
		NFC_ERR("Open internal secure element failed: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}


API net_nfc_error_e net_nfc_client_se_close_internal_secure_element(
		net_nfc_target_handle_s *handle, net_nfc_se_close_se_cb callback, void *user_data)
{
	GError *error = NULL;
	SeFuncData *func_data;
	NetNfcGDbusSecureElement* auto_proxy;

	if(se_proxy != NULL)
	{
		auto_proxy = se_proxy;
	}
	else
	{
		auto_proxy = net_nfc_gdbus_secure_element_proxy_new_for_bus_sync(
				G_BUS_TYPE_SYSTEM,
				G_DBUS_PROXY_FLAGS_NONE,
				"org.tizen.NetNfcService",
				"/org/tizen/NetNfcService/SecureElement",
				NULL,
				&error);
		if(NULL == auto_proxy)
		{
			NFC_ERR("Can not create proxy : %s", error->message);
			g_error_free(error);

			return NET_NFC_UNKNOWN_ERROR;
		}
	}

	/* allow this function even nfc is off */

	func_data = g_try_new0(SeFuncData, 1);
	if (NULL == func_data)
	{
		g_object_unref(auto_proxy);
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->se_callback = (gpointer)callback;
	func_data->se_data = user_data;

	net_nfc_gdbus_secure_element_call_close_secure_element(
			auto_proxy,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			close_secure_element,
			func_data);

	g_object_unref(auto_proxy);
	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_se_close_internal_secure_element_sync(
		net_nfc_target_handle_s *handle)
{
	gboolean ret;
	GError *error = NULL;
	net_nfc_error_e result = NET_NFC_OK;
	NetNfcGDbusSecureElement* auto_proxy;

	if(se_proxy != NULL)
	{
		auto_proxy = se_proxy;
	}
	else
	{
		auto_proxy = net_nfc_gdbus_secure_element_proxy_new_for_bus_sync(
				G_BUS_TYPE_SYSTEM,
				G_DBUS_PROXY_FLAGS_NONE,
				"org.tizen.NetNfcService",
				"/org/tizen/NetNfcService/SecureElement",
				NULL,
				&error);
		if (NULL == auto_proxy)
		{
			NFC_ERR("Can not create proxy : %s", error->message);
			g_error_free(error);

			return NET_NFC_UNKNOWN_ERROR;
		}
	}

	/* allow this function even nfc is off */

	ret = net_nfc_gdbus_secure_element_call_close_secure_element_sync(
			auto_proxy,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			&result,
			NULL,
			&error);

	if (FALSE == ret)
	{
		NFC_ERR("close internal secure element failed: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	g_object_unref(auto_proxy);
	return result;
}


API net_nfc_error_e net_nfc_client_se_get_atr(net_nfc_target_handle_s *handle,
		net_nfc_se_get_atr_cb callback, void *user_data)
{
	GError *error = NULL;
	SeFuncData *func_data;
	NetNfcGDbusSecureElement* auto_proxy;

	if(se_proxy != NULL)
	{
		auto_proxy = se_proxy;
	}
	else
	{
		auto_proxy = net_nfc_gdbus_secure_element_proxy_new_for_bus_sync(
				G_BUS_TYPE_SYSTEM,
				G_DBUS_PROXY_FLAGS_NONE,
				"org.tizen.NetNfcService",
				"/org/tizen/NetNfcService/SecureElement",
				NULL,
				&error);
		if (NULL == auto_proxy)
		{
			NFC_ERR("Can not create proxy : %s", error->message);
			g_error_free(error);

			return NET_NFC_UNKNOWN_ERROR;
		}
	}

	/* allow this function even nfc is off */

	func_data = g_try_new0(SeFuncData, 1);
	if (NULL == func_data)
	{
		g_object_unref(auto_proxy);
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->se_callback = (gpointer)callback;
	func_data->se_data = user_data;

	net_nfc_gdbus_secure_element_call_get_atr(
			auto_proxy,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			get_atr_secure_element,
			func_data);

	g_object_unref(auto_proxy);
	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_se_get_atr_sync(
		net_nfc_target_handle_s *handle, data_s **atr)
{
	gboolean ret;
	GError *error = NULL;
	GVariant *out_atr = NULL;
	net_nfc_error_e result = NET_NFC_OK;
	NetNfcGDbusSecureElement* auto_proxy;

	RETV_IF(NULL == atr, NET_NFC_NULL_PARAMETER);

	if(se_proxy != NULL)
	{
		auto_proxy = se_proxy;
	}
	else
	{
		auto_proxy = net_nfc_gdbus_secure_element_proxy_new_for_bus_sync(
				G_BUS_TYPE_SYSTEM,
				G_DBUS_PROXY_FLAGS_NONE,
				"org.tizen.NetNfcService",
				"/org/tizen/NetNfcService/SecureElement",
				NULL,
				&error);
		if (NULL == auto_proxy)
		{
			NFC_DBG("Can not create proxy : %s", error->message);
			g_error_free(error);

			return NET_NFC_UNKNOWN_ERROR;
		}
	}

	*atr = NULL;

	/* allow this function even nfc is off */

	ret = net_nfc_gdbus_secure_element_call_get_atr_sync(
			auto_proxy,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&out_atr,
			NULL,
			&error);

	if (TRUE == ret)
	{
		*atr = net_nfc_util_gdbus_variant_to_data(out_atr);
	}
	else
	{
		NFC_ERR("Get attributes failed: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	g_object_unref(auto_proxy);
	return result;
}


API net_nfc_error_e net_nfc_client_se_send_apdu(net_nfc_target_handle_s *handle,
		data_s *apdu_data, net_nfc_se_send_apdu_cb callback, void *user_data)
{
	GVariant *arg_data;
	GError *error = NULL;
	SeFuncData *func_data;
	NetNfcGDbusSecureElement* auto_proxy;

	if(se_proxy != NULL)
	{
		auto_proxy = se_proxy;
	}
	else
	{
		auto_proxy = net_nfc_gdbus_secure_element_proxy_new_for_bus_sync(
				G_BUS_TYPE_SYSTEM,
				G_DBUS_PROXY_FLAGS_NONE,
				"org.tizen.NetNfcService",
				"/org/tizen/NetNfcService/SecureElement",
				NULL,
				&error);
		if (NULL == auto_proxy)
		{
			NFC_ERR("Can not create proxy : %s", error->message);
			g_error_free(error);

			return NET_NFC_UNKNOWN_ERROR;
		}
	}

	/* allow this function even nfc is off */

	arg_data = net_nfc_util_gdbus_data_to_variant(apdu_data);
	if (arg_data == NULL)
	{
		g_object_unref(auto_proxy);
		return NET_NFC_INVALID_PARAM;
	}

	func_data = g_try_new0(SeFuncData, 1);
	if (NULL == func_data)
	{
		g_variant_unref(arg_data);
		g_object_unref(auto_proxy);
		return NET_NFC_ALLOC_FAIL;
	}

	func_data->se_callback = (gpointer)callback;
	func_data->se_data = user_data;

	net_nfc_gdbus_secure_element_call_send_apdu(
			auto_proxy,
			GPOINTER_TO_UINT(handle),
			arg_data,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			send_apdu_secure_element,
			func_data);

	g_object_unref(auto_proxy);
	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_se_send_apdu_sync(
		net_nfc_target_handle_s *handle, data_s *apdu_data, data_s **response)
{
	gboolean ret;
	GVariant *arg_data;
	GError *error = NULL;
	GVariant *out_data = NULL;
	net_nfc_error_e result = NET_NFC_OK;
	NetNfcGDbusSecureElement* auto_proxy;

	RETV_IF(NULL == response, NET_NFC_NULL_PARAMETER);

	if(se_proxy != NULL)
	{
		auto_proxy = se_proxy;
	}
	else
	{
		GError *error = NULL;

		auto_proxy = net_nfc_gdbus_secure_element_proxy_new_for_bus_sync(
				G_BUS_TYPE_SYSTEM,
				G_DBUS_PROXY_FLAGS_NONE,
				"org.tizen.NetNfcService",
				"/org/tizen/NetNfcService/SecureElement",
				NULL,
				&error);
		if (NULL == auto_proxy)
		{
			NFC_ERR("Can not create proxy : %s", error->message);
			g_error_free(error);

			return NET_NFC_UNKNOWN_ERROR;
		}
	}

	*response = NULL;

	/* allow this function even nfc is off */

	arg_data = net_nfc_util_gdbus_data_to_variant(apdu_data);
	if (NULL == arg_data)
	{
		g_object_unref(auto_proxy);
		return NET_NFC_INVALID_PARAM;
	}

	ret = net_nfc_gdbus_secure_element_call_send_apdu_sync(
			auto_proxy,
			GPOINTER_TO_UINT(handle),
			arg_data,
			net_nfc_client_gdbus_get_privilege(),
			&result,
			&out_data,
			NULL,
			&error);

	if (TRUE == ret)
	{
		*response = net_nfc_util_gdbus_variant_to_data(out_data);
	}
	else
	{
		NFC_ERR("Send APDU failed: %s", error->message);
		g_error_free(error);
		result = NET_NFC_IPC_FAIL;
	}

	g_object_unref(auto_proxy);
	return result;
}


API void net_nfc_client_se_set_ese_detection_cb(
		net_nfc_client_se_ese_detected_event callback, void *user_data)
{
	se_esedetecthandler.se_ese_detected_cb = callback;
	se_esedetecthandler.se_ese_detected_data = user_data;
}


API void net_nfc_client_se_unset_ese_detection_cb(void)
{
	net_nfc_client_se_set_ese_detection_cb(NULL, NULL);
}


API void net_nfc_client_se_set_transaction_event_cb(
		net_nfc_se_type_e se_type,
		net_nfc_client_se_transaction_event callback,
		void *user_data)
{
	se_transeventhandler.se_type = se_type;

	if(se_type == NET_NFC_SE_TYPE_ESE)
	{
		se_transeventhandler.eSE_transaction_event_cb = callback;
		se_transeventhandler.eSE_transaction_event_data = user_data;
	}
	else if(se_type == NET_NFC_SE_TYPE_UICC)
	{
		se_transeventhandler.UICC_transaction_event_cb = callback;
		se_transeventhandler.UICC_transaction_event_data = user_data;
	}
}


API void net_nfc_client_se_unset_transaction_event_cb(net_nfc_se_type_e type)
{
	net_nfc_client_se_set_transaction_event_cb(type, NULL, NULL);
	net_nfc_client_se_set_transaction_event_cb(NET_NFC_SE_TYPE_NONE, NULL, NULL);
}


API void net_nfc_client_se_set_event_cb(net_nfc_client_se_event callback,
		void *user_data)
{
	se_eventhandler.se_event_cb = callback;
	se_eventhandler.se_event_data = user_data;
}


API void net_nfc_client_se_unset_event_cb(void)
{
	net_nfc_client_se_set_event_cb(NULL, NULL);
}


net_nfc_error_e net_nfc_client_se_init(void)
{
	GError *error = NULL;

	if (se_proxy)
	{
		NFC_WARN("Already initialized");
		return NET_NFC_OK;
	}

	se_proxy = net_nfc_gdbus_secure_element_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/SecureElement",
			NULL,
			&error);
	if (NULL == se_proxy)
	{
		NFC_ERR("Can not create proxy : %s", error->message);

		g_error_free(error);

		return NET_NFC_UNKNOWN_ERROR;
	}

	g_signal_connect(se_proxy, "se-type-changed", G_CALLBACK(se_type_changed), NULL);
	g_signal_connect(se_proxy, "ese-detected", G_CALLBACK(se_ese_detected), NULL);

	g_signal_connect(se_proxy, "transaction-event",
			G_CALLBACK(se_transaction_event), NULL);

	g_signal_connect(se_proxy, "card-emulation-mode-changed",
			G_CALLBACK(se_card_emulation_mode_changed), NULL);

	return NET_NFC_OK;
}


void net_nfc_client_se_deinit(void)
{
	if (se_proxy)
	{
		g_object_unref(se_proxy);
		se_proxy = NULL;
	}
}
