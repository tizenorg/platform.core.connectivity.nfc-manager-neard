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
	net_nfc_client_se_transaction_event se_transaction_event_cb;
	gpointer se_transaction_event_data;
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

static void se_ese_detected(GObject *source_object,
		guint arg_handle,
		gint arg_se_type,
		GVariant *arg_data);

static void se_type_changed(GObject *source_object,
		gint arg_se_type);

static void set_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);

static void open_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);

static void close_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);

static void send_apdu_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);

static void get_atr_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);


static void se_ese_detected(GObject *source_object,
		guint arg_handle,
		gint arg_se_type,
		GVariant *arg_data)
{
	INFO_MSG(">>> SIGNAL arrived");

	if (se_esedetecthandler.se_ese_detected_cb != NULL) {
		data_s buffer_data = { NULL, 0 };
		net_nfc_client_se_ese_detected_event callback =
			(net_nfc_client_se_ese_detected_event)se_esedetecthandler.se_ese_detected_cb;

		net_nfc_util_gdbus_variant_to_data_s(arg_data, &buffer_data);

		callback((net_nfc_target_handle_h)arg_handle, arg_se_type, &buffer_data,
				se_esedetecthandler.se_ese_detected_data);

		net_nfc_util_free_data(&buffer_data);
	}
}


static void se_type_changed(GObject *source_object,
		gint arg_se_type)
{
	INFO_MSG(">>> SIGNAL arrived");

	if (se_eventhandler.se_event_cb != NULL)
	{
		net_nfc_client_se_event callback =
			(net_nfc_client_se_event)se_eventhandler.se_event_cb;

		callback((net_nfc_message_e)arg_se_type,
				se_eventhandler.se_event_data);
	}
}


static void se_transaction_event(GObject *source_object,
		gint arg_se_type,
		GVariant *arg_aid,
		GVariant *arg_param)
{
	INFO_MSG(">>> SIGNAL arrived");

	if (se_transeventhandler.se_transaction_event_cb != NULL) {
		net_nfc_client_se_transaction_event callback =
			(net_nfc_client_se_transaction_event)se_transeventhandler.se_transaction_event_cb;
		data_s aid = { NULL, 0 };
		data_s param = { NULL, 0 };

		net_nfc_util_gdbus_variant_to_data_s(arg_aid, &aid);
		net_nfc_util_gdbus_variant_to_data_s(arg_param, &param);

		callback(&aid, &param,
				se_transeventhandler.se_transaction_event_data);

		net_nfc_util_free_data(&param);
		net_nfc_util_free_data(&aid);
	}
}


static void set_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	SeFuncData *func_data = (SeFuncData *)user_data;
	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	g_assert(user_data != NULL);

	if (net_nfc_gdbus_secure_element_call_set_finish(se_proxy,
				&result,
				res,
				&error) == FALSE)
	{
		result = NET_NFC_IPC_FAIL;

		DEBUG_ERR_MSG("Could not set secure element: %s",
				error->message);

		g_error_free(error);
	}

	if (func_data->se_callback != NULL)
	{
		net_nfc_se_set_se_cb se_callback =
			(net_nfc_se_set_se_cb)func_data->se_callback;

		se_callback(result, func_data->se_data);
	}

	g_free(func_data);
}

static void get_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NetNfcCallback *func_data = (NetNfcCallback *)user_data;
	net_nfc_error_e result = NET_NFC_OK;
	gint type = 0;
	GError *error = NULL;

	g_assert(user_data != NULL);

	if (net_nfc_gdbus_secure_element_call_get_finish(se_proxy,
				&result,
				&type,
				res,
				&error) == FALSE)
	{
		DEBUG_ERR_MSG("Could not set secure element: %s",
				error->message);

		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_se_get_se_cb se_callback =
			(net_nfc_se_get_se_cb)func_data->callback;

		se_callback(result, type, func_data->user_data);
	}

	g_free(func_data);
}

static void _set_card_emulation_cb(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NetNfcCallback *func_data = (NetNfcCallback *)user_data;
	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	g_assert(user_data != NULL);

	if (net_nfc_gdbus_secure_element_call_set_card_emulation_finish(
				se_proxy,
				&result,
				res,
				&error) == FALSE)
	{
		DEBUG_ERR_MSG("Could not set card emulation: %s",
				error->message);

		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	if (func_data->callback != NULL)
	{
		net_nfc_se_set_se_cb se_callback =
			(net_nfc_se_set_se_cb)func_data->callback;

		se_callback(result, func_data->user_data);
	}

	g_free(func_data);
}


static void open_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	SeFuncData *func_data = (SeFuncData *)user_data;
	net_nfc_error_e result = NET_NFC_OK;
	guint out_handle = 0;
	GError *error = NULL;

	g_assert(user_data != NULL);

	if (net_nfc_gdbus_secure_element_call_open_secure_element_finish(
				se_proxy,
				&result,
				&out_handle,
				res,
				&error) == FALSE)
	{
		result = NET_NFC_IPC_FAIL;

		DEBUG_ERR_MSG("Could not open secure element: %s",
				error->message);

		g_error_free(error);
	}

	if (func_data->se_callback != NULL)
	{
		net_nfc_se_open_se_cb se_callback =
			(net_nfc_se_open_se_cb)func_data->se_callback;

		se_callback(result, (net_nfc_target_handle_h)out_handle, func_data->se_data);
	}

	g_free(func_data);
}


static void close_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	SeFuncData *func_data = (SeFuncData *)user_data;
	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	g_assert(user_data != NULL);

	if (net_nfc_gdbus_secure_element_call_close_secure_element_finish(
				se_proxy,
				&result,
				res,
				&error) == FALSE)
	{
		result = NET_NFC_IPC_FAIL;

		DEBUG_ERR_MSG("Could not close secure element: %s", error->message);

		g_error_free(error);
	}

	if (func_data->se_callback != NULL)
	{
		net_nfc_se_close_se_cb se_callback =
			(net_nfc_se_close_se_cb)func_data->se_callback;

		se_callback(result, func_data->se_data);
	}

	g_free(func_data);
}


static void send_apdu_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	SeFuncData *func_data = (SeFuncData *)user_data;
	net_nfc_error_e result = NET_NFC_OK;
	GVariant *out_response = NULL;
	GError *error = NULL;

	g_assert(user_data != NULL);

	if (net_nfc_gdbus_secure_element_call_send_apdu_finish(
				se_proxy,
				&result,
				&out_response,
				res,
				&error) == FALSE)
	{
		result = NET_NFC_IPC_FAIL;

		DEBUG_ERR_MSG("Could not send apdu: %s", error->message);

		g_error_free(error);
	}

	if (func_data->se_callback != NULL)
	{
		net_nfc_se_send_apdu_cb se_callback =
			(net_nfc_se_send_apdu_cb)func_data->se_callback;
		data_s data = { NULL, };

		net_nfc_util_gdbus_variant_to_data_s(out_response, &data);

		se_callback(result, &data, func_data->se_data);

		net_nfc_util_free_data(&data);
	}

	g_free(func_data);
}


static void get_atr_secure_element(GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	SeFuncData *func_data = (SeFuncData *)user_data;
	net_nfc_error_e result = NET_NFC_OK;
	GVariant *out_atr = NULL;
	GError *error = NULL;

	g_assert(user_data != NULL);

	if (net_nfc_gdbus_secure_element_call_get_atr_finish(
				se_proxy,
				&result,
				&out_atr,
				res,
				&error) == FALSE)
	{
		result = NET_NFC_IPC_FAIL;

		DEBUG_ERR_MSG("Could not get atr: %s", error->message);

		g_error_free(error);
	}

	if (func_data->se_callback != NULL)
	{
		net_nfc_se_get_atr_cb se_callback =
			(net_nfc_se_get_atr_cb)func_data->se_callback;
		data_s data = { NULL, };

		net_nfc_util_gdbus_variant_to_data_s(out_atr, &data);

		se_callback(result, &data, func_data->se_data);

		net_nfc_util_free_data(&data);
	}

	g_free(func_data);
}


API net_nfc_error_e net_nfc_client_se_set_secure_element_type(
		net_nfc_se_type_e se_type,
		net_nfc_se_set_se_cb callback,
		void *user_data)
{
	SeFuncData *func_data;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_try_new0(SeFuncData, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->se_callback = (gpointer)callback;
	func_data->se_data = user_data;

	net_nfc_gdbus_secure_element_call_set(
			se_proxy,
			(gint)se_type,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			set_secure_element,
			func_data);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_se_set_secure_element_type_sync(
		net_nfc_se_type_e se_type)
{
	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_secure_element_call_set_sync(
				se_proxy,
				(gint)se_type,
				net_nfc_client_gdbus_get_privilege(),
				&result,
				NULL,
				&error) == FALSE)
	{
		DEBUG_ERR_MSG("Set secure element failed: %s", error->message);

		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}

API net_nfc_error_e net_nfc_client_se_get_secure_element_type(
		net_nfc_se_get_se_cb callback,
		void *user_data)
{
	NetNfcCallback *func_data;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_try_new0(NetNfcCallback, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_secure_element_call_get(
			se_proxy,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			get_secure_element,
			func_data);

	return NET_NFC_OK;
}

API net_nfc_error_e net_nfc_client_se_get_secure_element_type_sync(
		net_nfc_se_type_e *se_type)
{
	net_nfc_error_e result = NET_NFC_OK;
	gint type;
#if 0
	GError *error = NULL;
#endif
	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}
#if 1
	if (vconf_get_int(VCONFKEY_NFC_SE_TYPE, &type) == 0) {
		*se_type = type;
	} else {
		result = NET_NFC_OPERATION_FAIL;
	}
#else
	if (net_nfc_gdbus_secure_element_call_get_sync(
				se_proxy,
				net_nfc_client_gdbus_get_privilege(),
				&result,
				(gint)&type,
				NULL,
				&error) == true) {
		*se_type = type;
	} else {
		DEBUG_ERR_MSG("Set secure element failed: %s", error->message);

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

	if (se_proxy == NULL) {
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	func_data = g_try_new0(NetNfcCallback, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->callback = (gpointer)callback;
	func_data->user_data = user_data;

	net_nfc_gdbus_secure_element_call_set_card_emulation(
			se_proxy,
			(gint)mode,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			_set_card_emulation_cb,
			func_data);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_set_card_emulation_mode_sync(
		net_nfc_card_emulation_mode_t mode)
{
	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* prevent executing daemon when nfc is off */
	if (net_nfc_client_manager_is_activated() == false) {
		return NET_NFC_INVALID_STATE;
	}

	if (net_nfc_gdbus_secure_element_call_set_card_emulation_sync(
				se_proxy,
				(gint)mode,
				net_nfc_client_gdbus_get_privilege(),
				&result,
				NULL,
				&error) == FALSE)
	{
		DEBUG_ERR_MSG("Set card emulation failed: %s", error->message);

		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}


API net_nfc_error_e net_nfc_client_se_open_internal_secure_element(
		net_nfc_se_type_e se_type,
		net_nfc_se_open_se_cb callback,
		void *user_data)
{
	SeFuncData *func_data;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* allow this function even nfc is off */

	func_data = g_try_new0(SeFuncData, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->se_callback = (gpointer)callback;
	func_data->se_data = user_data;

	net_nfc_gdbus_secure_element_call_open_secure_element(
			se_proxy,
			(gint)se_type,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			open_secure_element,
			func_data);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_se_open_internal_secure_element_sync(
		net_nfc_se_type_e se_type, net_nfc_target_handle_h *handle)
{
	net_nfc_error_e result = NET_NFC_OK;
	guint out_handle = 0;
	GError *error =  NULL;

	if (handle == NULL) {
		return NET_NFC_NULL_PARAMETER;
	}

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* allow this function even nfc is off */

	if (net_nfc_gdbus_secure_element_call_open_secure_element_sync(
				se_proxy,
				se_type,
				net_nfc_client_gdbus_get_privilege(),
				&result,
				&out_handle,
				NULL,
				&error) == true) {
		*handle = GUINT_TO_POINTER(out_handle);
	} else {
		DEBUG_ERR_MSG("Open internal secure element failed: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}


API net_nfc_error_e net_nfc_client_se_close_internal_secure_element(
		net_nfc_target_handle_h handle, net_nfc_se_close_se_cb callback, void *user_data)
{
	SeFuncData *func_data;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* allow this function even nfc is off */

	func_data = g_try_new0(SeFuncData, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->se_callback = (gpointer)callback;
	func_data->se_data = user_data;

	net_nfc_gdbus_secure_element_call_close_secure_element(
			se_proxy,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			close_secure_element,
			func_data);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_se_close_internal_secure_element_sync(
		net_nfc_target_handle_h handle)
{
	net_nfc_error_e result = NET_NFC_OK;
	GError *error = NULL;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* allow this function even nfc is off */

	if (net_nfc_gdbus_secure_element_call_close_secure_element_sync(
				se_proxy,
				GPOINTER_TO_UINT(handle),
				net_nfc_client_gdbus_get_privilege(),
				&result,
				NULL,
				&error) == FALSE)
	{
		DEBUG_ERR_MSG("close internal secure element failed: %s",
				error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}


API net_nfc_error_e net_nfc_client_se_get_atr(net_nfc_target_handle_h handle,
		net_nfc_se_get_atr_cb callback, void *user_data)
{
	SeFuncData *func_data;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* allow this function even nfc is off */

	func_data = g_try_new0(SeFuncData, 1);
	if (func_data == NULL)
		return NET_NFC_ALLOC_FAIL;

	func_data->se_callback = (gpointer)callback;
	func_data->se_data = user_data;

	net_nfc_gdbus_secure_element_call_get_atr(
			se_proxy,
			GPOINTER_TO_UINT(handle),
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			get_atr_secure_element,
			func_data);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_se_get_atr_sync(
		net_nfc_target_handle_h handle, data_h *atr)
{
	net_nfc_error_e result = NET_NFC_OK;
	GVariant *out_atr = NULL;
	GError *error = NULL;

	if (atr == NULL)
		return NET_NFC_NULL_PARAMETER;

	*atr = NULL;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");
		return NET_NFC_NOT_INITIALIZED;
	}

	/* allow this function even nfc is off */

	if (net_nfc_gdbus_secure_element_call_get_atr_sync(
				se_proxy,
				GPOINTER_TO_UINT(handle),
				net_nfc_client_gdbus_get_privilege(),
				&result,
				&out_atr,
				NULL,
				&error) == true) {
		*atr = net_nfc_util_gdbus_variant_to_data(out_atr);
	} else {
		DEBUG_ERR_MSG("Get attributes failed: %s", error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}


API net_nfc_error_e net_nfc_client_se_send_apdu(net_nfc_target_handle_h handle,
		data_h apdu_data, net_nfc_se_send_apdu_cb callback, void *user_data)
{
	SeFuncData *func_data;
	GVariant *arg_data;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* allow this function even nfc is off */

	arg_data = net_nfc_util_gdbus_data_to_variant((data_s *)apdu_data);
	if (arg_data == NULL)
		return NET_NFC_INVALID_PARAM;

	func_data = g_try_new0(SeFuncData, 1);
	if (func_data == NULL) {
		g_variant_unref(arg_data);

		return NET_NFC_ALLOC_FAIL;
	}

	func_data->se_callback = (gpointer)callback;
	func_data->se_data = user_data;

	net_nfc_gdbus_secure_element_call_send_apdu(
			se_proxy,
			GPOINTER_TO_UINT(handle),
			arg_data,
			net_nfc_client_gdbus_get_privilege(),
			NULL,
			send_apdu_secure_element,
			func_data);

	return NET_NFC_OK;
}


API net_nfc_error_e net_nfc_client_se_send_apdu_sync(
		net_nfc_target_handle_h handle, data_h apdu_data, data_h *response)
{
	net_nfc_error_e result = NET_NFC_OK;
	GVariant *out_data = NULL;
	GError *error = NULL;
	GVariant *arg_data;

	if (response == NULL)
		return NET_NFC_NULL_PARAMETER;

	*response = NULL;

	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not get se_proxy");

		return NET_NFC_NOT_INITIALIZED;
	}

	/* allow this function even nfc is off */

	arg_data = net_nfc_util_gdbus_data_to_variant((data_s *)apdu_data);
	if (arg_data == NULL)
		return NET_NFC_INVALID_PARAM;

	if (net_nfc_gdbus_secure_element_call_send_apdu_sync(
				se_proxy,
				GPOINTER_TO_UINT(handle),
				arg_data,
				net_nfc_client_gdbus_get_privilege(),
				&result,
				&out_data,
				NULL,
				&error) == true) {
		*response = net_nfc_util_gdbus_variant_to_data(out_data);
	} else {
		DEBUG_ERR_MSG("Send APDU failed: %s",
				error->message);
		g_error_free(error);

		result = NET_NFC_IPC_FAIL;
	}

	return result;
}


API void net_nfc_client_se_set_ese_detection_cb(
		net_nfc_client_se_ese_detected_event callback,
		void *user_data)
{
	se_esedetecthandler.se_ese_detected_cb = callback;
	se_esedetecthandler.se_ese_detected_data = user_data;
}


API void net_nfc_client_se_unset_ese_detection_cb(void)
{
	net_nfc_client_se_set_ese_detection_cb(NULL, NULL);
}


API void net_nfc_client_se_set_transaction_event_cb(
		net_nfc_client_se_transaction_event callback,
		void *user_data)
{
	se_transeventhandler.se_transaction_event_cb = callback;
	se_transeventhandler.se_transaction_event_data = user_data;
}


API void net_nfc_client_se_unset_transaction_event_cb(void)
{
	net_nfc_client_se_set_transaction_event_cb(NULL, NULL);
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
		DEBUG_CLIENT_MSG("Already initialized");

		return NET_NFC_OK;
	}

	se_proxy = net_nfc_gdbus_secure_element_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.tizen.NetNfcService",
			"/org/tizen/NetNfcService/SecureElement",
			NULL,
			&error);
	if (se_proxy == NULL)
	{
		DEBUG_ERR_MSG("Can not create proxy : %s", error->message);

		g_error_free(error);

		return NET_NFC_UNKNOWN_ERROR;
	}

	g_signal_connect(se_proxy, "se-type-changed",
			G_CALLBACK(se_type_changed), NULL);

	g_signal_connect(se_proxy, "ese-detected",
			G_CALLBACK(se_ese_detected), NULL);

	g_signal_connect(se_proxy, "transaction-event",
			G_CALLBACK(se_transaction_event), NULL);

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
