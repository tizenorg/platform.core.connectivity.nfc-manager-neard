/*
 * Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <vconf.h>
#include <tapi_common.h>
#include <ITapiSim.h>

#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_gdbus.h"
#include "net_nfc_server_common.h"
#include "net_nfc_server_context.h"
#include "net_nfc_server_manager.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_server_util.h"
#include "net_nfc_server_se.h"

enum
{
	SE_UICC_UNAVAILABLE = -1,
	SE_UICC_ON_PROGRESS = 0,
	SE_UICC_READY = 1,
};

typedef struct _nfc_se_setting_t
{
	bool busy;
	uint8_t type;
	uint8_t mode;
}
net_nfc_server_se_setting_t;


static NetNfcGDbusSecureElement *se_skeleton = NULL;
#if 0
static uint8_t gdbus_se_prev_type = SECURE_ELEMENT_TYPE_INVALID;
static uint8_t gdbus_se_prev_mode = SECURE_ELEMENT_OFF_MODE;
#endif
static net_nfc_server_se_setting_t gdbus_se_setting;

/* TODO : make a list for handles */
static TapiHandle *gdbus_uicc_handle;
static net_nfc_target_handle_s *gdbus_ese_handle;

static int gdbus_uicc_ready;

/* server_side */
typedef struct _ServerSeData ServerSeData;

struct _ServerSeData
{
	data_s aid;
	data_s param;
	guint event;
};

typedef struct _SeSetCardEmul SeSetCardEmul;

struct _SeSetCardEmul
{
	NetNfcGDbusSecureElement *object;
	GDBusMethodInvocation *invocation;
	gint mode;
};

typedef struct _SeDataSeType SeDataSeType;

struct _SeDataSeType
{
	NetNfcGDbusSecureElement *object;
	GDBusMethodInvocation *invocation;
	gint se_type;
};


typedef struct _SeDataHandle SeDataHandle;

struct _SeDataHandle
{
	NetNfcGDbusSecureElement *object;
	GDBusMethodInvocation *invocation;
	net_nfc_target_handle_s *handle;
};

typedef struct _SeDataApdu SeDataApdu;

struct _SeDataApdu
{
	NetNfcGDbusSecureElement *object;
	GDBusMethodInvocation *invocation;
	net_nfc_target_handle_s *handle;
	GVariant *data;
};

static void se_close_secure_element_thread_func(gpointer user_data);

static void se_get_atr_thread_func(gpointer user_data);

static void se_open_secure_element_thread_func(gpointer user_data);

static void se_send_apdu_thread_func(gpointer user_data);

static void se_set_data_thread_func(gpointer user_data);

static gboolean se_handle_close_secure_element(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		guint arg_handle,
		GVariant *smack_privilege);

static gboolean se_handle_get_atr(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		guint arg_handle,
		GVariant *smack_privilege);


static gboolean se_handle_open_secure_element(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		gint arg_type,
		GVariant *smack_privilege);


static gboolean se_handle_send_apdu(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		guint arg_handle,
		GVariant* apdudata,
		GVariant *smack_privilege);

static gboolean se_handle_set(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		gint arg_type,
		GVariant *smack_privilege);


uint8_t net_nfc_server_se_get_se_type()
{
	return gdbus_se_setting.type;
}

uint8_t net_nfc_server_se_get_se_mode()
{
	return gdbus_se_setting.mode;
}

static void net_nfc_server_se_set_se_type(uint8_t type)
{
	gdbus_se_setting.type = type;
}

static void net_nfc_server_se_set_se_mode(uint8_t mode)
{
	gdbus_se_setting.mode = mode;
}


/* eSE functions */
static bool net_nfc_server_se_is_ese_handle(net_nfc_target_handle_s *handle)
{
	return (gdbus_ese_handle != NULL && gdbus_ese_handle == handle);
}

static void net_nfc_server_se_set_current_ese_handle(
		net_nfc_target_handle_s *handle)
{
	gdbus_ese_handle = handle;
}

static net_nfc_target_handle_s *net_nfc_server_se_open_ese()
{
	if (gdbus_ese_handle == NULL) {
		net_nfc_error_e result = NET_NFC_OK;
		net_nfc_target_handle_s *handle = NULL;

		if (net_nfc_controller_secure_element_open(
					SECURE_ELEMENT_TYPE_ESE,
					&handle, &result) == true)
		{
			net_nfc_server_se_set_current_ese_handle(handle);

			NFC_DBG("handle [%p]", handle);
		}
		else
		{
			NFC_ERR("net_nfc_controller_secure_element_open failed [%d]",
					result);
		}
	}

	return gdbus_ese_handle;
}

static net_nfc_error_e net_nfc_server_se_close_ese()
{
	net_nfc_error_e result = NET_NFC_OK;

	if (gdbus_ese_handle != NULL &&
			net_nfc_server_gdbus_is_server_busy() == false) {
		if (net_nfc_controller_secure_element_close(
					gdbus_ese_handle,
					&result) == false)
		{
			net_nfc_controller_exception_handler();
		}
		net_nfc_server_se_set_current_ese_handle(NULL);
	}

	return result;
}

static void _se_uicc_enable_card_emulation()
{
	net_nfc_error_e result;

	/*turn off ESE*/
	net_nfc_controller_set_secure_element_mode(
			SECURE_ELEMENT_TYPE_ESE,
			SECURE_ELEMENT_OFF_MODE,
			&result);

	/*turn on UICC*/
	net_nfc_controller_set_secure_element_mode(
			SECURE_ELEMENT_TYPE_UICC,
			SECURE_ELEMENT_VIRTUAL_MODE,
			&result);
	if (result == NET_NFC_OK) {
		NFC_INFO("card emulation changed to SECURE_ELEMENT_TYPE_UICC");

		net_nfc_server_se_set_se_type(SECURE_ELEMENT_TYPE_UICC);
		net_nfc_server_se_set_se_mode(SECURE_ELEMENT_VIRTUAL_MODE);

		if (vconf_set_int(VCONFKEY_NFC_SE_TYPE,
					VCONFKEY_NFC_SE_TYPE_UICC) < 0) {
			NFC_ERR("vconf_set_int failed");
		}
	} else {
		NFC_ERR("net_nfc_controller_set_secure_element_mode failed, [%d]",
				result);
	}
}

static void _se_uicc_prepare(void)
{
	char **cpList;

	cpList = tel_get_cp_name_list();
	if (cpList != NULL) {
		gdbus_uicc_handle = tel_init(cpList[0]);
		if (gdbus_uicc_handle != NULL) {
		} else {
			NFC_ERR("tel_init() failed");
		}
	} else {
		NFC_ERR("tel_get_cp_name_list() failed");
	}
}

static void _se_uicc_status_noti_cb(TapiHandle *handle,
		const char *noti_id,
		void *data,
		void *user_data)
{
	TelSimCardStatus_t *status = (TelSimCardStatus_t *)data;

	NFC_DBG("_se_uicc_status_noti_cb");

	switch (*status) {
	case TAPI_SIM_STATUS_SIM_INIT_COMPLETED :
		gdbus_uicc_ready = SE_UICC_READY;

		_se_uicc_prepare();

		if (gdbus_se_setting.busy == true &&
				net_nfc_server_se_get_se_type() == SECURE_ELEMENT_TYPE_UICC) {

			gdbus_se_setting.busy = false;

			_se_uicc_enable_card_emulation();
		}
		break;

	case TAPI_SIM_STATUS_CARD_REMOVED :
		NFC_DBG("TAPI_SIM_STATUS_CARD_REMOVED");
		gdbus_uicc_ready = SE_UICC_UNAVAILABLE;

		if (net_nfc_server_se_get_se_type() == SECURE_ELEMENT_TYPE_UICC) {
			net_nfc_error_e result;

			/*turn off UICC*/
			net_nfc_controller_set_secure_element_mode(
					SECURE_ELEMENT_TYPE_UICC,
					SECURE_ELEMENT_OFF_MODE,
					&result);

			net_nfc_server_se_set_se_type(SECURE_ELEMENT_TYPE_INVALID);
			net_nfc_server_se_set_se_mode(SECURE_ELEMENT_OFF_MODE);
		}
		break;

	default:
		break;
	}
}

static void _se_uicc_init(void)
{
	_se_uicc_prepare();
	tel_register_noti_event(gdbus_uicc_handle,
			TAPI_NOTI_SIM_STATUS,
			_se_uicc_status_noti_cb,
			NULL);
}

static void _se_uicc_deinit()
{
	if (gdbus_uicc_handle != NULL) {
		tel_deregister_noti_event(gdbus_uicc_handle,
				TAPI_NOTI_SIM_STATUS);

		tel_deinit(gdbus_uicc_handle);

		gdbus_uicc_handle = NULL;
	}
}

static net_nfc_target_handle_s* _se_uicc_open()
{
	net_nfc_target_handle_s *result = NULL;

	if (gdbus_uicc_ready == SE_UICC_READY && gdbus_uicc_handle != NULL) {
		result = (net_nfc_target_handle_s *)gdbus_uicc_handle;
	}

	return result;
}

static bool _se_is_uicc_handle(net_nfc_target_handle_s *handle)
{
	return (gdbus_uicc_ready == SE_UICC_READY &&
			gdbus_uicc_handle != NULL &&
			(TapiHandle *)handle == gdbus_uicc_handle);
}

static void _se_uicc_close(net_nfc_target_handle_s *handle)
{
}

/* SE Functions */
net_nfc_error_e net_nfc_server_se_disable_card_emulation()
{
	net_nfc_error_e result;

	net_nfc_server_se_set_se_type(SECURE_ELEMENT_TYPE_INVALID);
	net_nfc_server_se_set_se_mode(SECURE_ELEMENT_OFF_MODE);

	/*turn off ESE*/
	net_nfc_controller_set_secure_element_mode(
			SECURE_ELEMENT_TYPE_ESE,
			SECURE_ELEMENT_OFF_MODE,
			&result);

	/*turn off UICC*/
	net_nfc_controller_set_secure_element_mode(
			SECURE_ELEMENT_TYPE_UICC,
			SECURE_ELEMENT_OFF_MODE,
			&result);

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_server_se_change_se(uint8_t type)
{
	net_nfc_error_e result = NET_NFC_OK;

	switch (type) {
	case SECURE_ELEMENT_TYPE_UICC :
		if (gdbus_se_setting.busy == false) {
			if (gdbus_uicc_ready == SE_UICC_READY) {
				_se_uicc_enable_card_emulation();
			} else if (gdbus_uicc_ready == SE_UICC_ON_PROGRESS) {
				NFC_INFO("waiting for uicc initializing complete...");

				net_nfc_server_se_set_se_type(SECURE_ELEMENT_TYPE_UICC);
				net_nfc_server_se_set_se_mode(SECURE_ELEMENT_VIRTUAL_MODE);

				gdbus_se_setting.busy = true;
			} else {
				result = NET_NFC_NOT_SUPPORTED;
			}
		} else {
			NFC_DBG("Previous request is processing.");

			result = NET_NFC_BUSY;
		}
		break;

	case SECURE_ELEMENT_TYPE_ESE :
		/*turn off UICC*/
		net_nfc_controller_set_secure_element_mode(
				SECURE_ELEMENT_TYPE_UICC,
				SECURE_ELEMENT_OFF_MODE,
				&result);

		/*turn on ESE*/
		net_nfc_controller_set_secure_element_mode(
				SECURE_ELEMENT_TYPE_ESE,
				SECURE_ELEMENT_VIRTUAL_MODE,
				&result);

		if (result == NET_NFC_OK) {
			NFC_INFO("card emulation changed to SECURE_ELEMENT_TYPE_ESE");

			net_nfc_server_se_set_se_type(SECURE_ELEMENT_TYPE_ESE);
			net_nfc_server_se_set_se_mode(SECURE_ELEMENT_VIRTUAL_MODE);

			if (vconf_set_int(VCONFKEY_NFC_SE_TYPE,
						VCONFKEY_NFC_SE_TYPE_ESE) != 0) {
				NFC_ERR("vconf_set_int failed");
			}
		} else {
			NFC_ERR("net_nfc_controller_set_secure_element_mode failed, [%d]", result);
		}
		break;

	default:
		result = net_nfc_server_se_disable_card_emulation();
		if (result == NET_NFC_OK){
			NFC_INFO("card emulation turned off");

			if (vconf_set_int(VCONFKEY_NFC_SE_TYPE,
						VCONFKEY_NFC_SE_TYPE_NONE) != 0) {
				NFC_ERR("vconf_set_int failed");
			}
		}
		break;
	}

	return result;
}

static void se_close_secure_element_thread_func(gpointer user_data)
{
	SeDataHandle *detail = (SeDataHandle *)user_data;
	net_nfc_error_e result;

	g_assert(detail != NULL);
	g_assert(detail->object != NULL);
	g_assert(detail->invocation != NULL);

	if (_se_is_uicc_handle(detail->handle) == true)
	{
		_se_uicc_close(detail->handle);

		result = NET_NFC_OK;
	}
	else if (net_nfc_server_se_is_ese_handle(detail->handle) == true)
	{
		/* decrease client reference count */
		net_nfc_server_gdbus_decrease_se_count(
				g_dbus_method_invocation_get_sender(
					detail->invocation));

		result = net_nfc_server_se_close_ese();
	}
	else
	{
		result = NET_NFC_INVALID_HANDLE;
	}
#if 0
	if ((gdbus_se_prev_type != net_nfc_server_se_get_se_type()) ||
			(gdbus_se_prev_mode != net_nfc_server_se_get_se_mode()))
	{
		/*return back se mode*/
		net_nfc_controller_set_secure_element_mode(gdbus_se_prev_type,
				gdbus_se_prev_mode, &result);

		net_nfc_server_se_set_se_type(gdbus_se_prev_type);
		net_nfc_server_se_set_se_mode(gdbus_se_prev_mode);
	}
#endif
	net_nfc_gdbus_secure_element_complete_close_secure_element(
			detail->object, detail->invocation, result);

	g_object_unref(detail->invocation);
	g_object_unref(detail->object);

	g_free(detail);

	/* shutdown process if it doesn't need */
	if (net_nfc_server_manager_get_active() == false &&
			net_nfc_server_gdbus_is_server_busy() == false) {
		net_nfc_server_controller_deinit();
	}
}

static gboolean se_handle_close_secure_element(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		guint arg_handle,
		GVariant *smack_privilege)
{
	SeDataHandle *data;
	gboolean result;

	NFC_INFO(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager",
				"rw") == false) {
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(SeDataHandle, 1);
	if (data == NULL)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");

		return FALSE;
	}

	data->object = g_object_ref(object);
	data->invocation = g_object_ref(invocation);
	data->handle = GUINT_TO_POINTER(arg_handle);

	result = net_nfc_server_controller_async_queue_push(
			se_close_secure_element_thread_func, data);
	if (result == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Se.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->object);
		g_object_unref(data->invocation);

		g_free(data);
	}

	return result;
}

static void se_get_atr_thread_func(gpointer user_data)
{
	SeDataHandle *detail = (SeDataHandle *)user_data;
	net_nfc_error_e result = NET_NFC_OK;
	data_s *atr = NULL;
	GVariant *data;

	g_assert(detail != NULL);
	g_assert(detail->object != NULL);
	g_assert(detail->invocation != NULL);

	if (_se_is_uicc_handle(detail->handle) == true)
	{
		result = NET_NFC_NOT_SUPPORTED;
	}
	else if (net_nfc_server_se_is_ese_handle(detail->handle) == true)
	{
		net_nfc_controller_secure_element_get_atr(detail->handle, &atr,
				&result);
	}
	else
	{
		NFC_ERR("invalid se handle");

		result = NET_NFC_INVALID_HANDLE;
	}

	data = net_nfc_util_gdbus_data_to_variant(atr);

	net_nfc_gdbus_secure_element_complete_get_atr(
			detail->object,
			detail->invocation,
			result,
			data);

	if (atr != NULL) {
		net_nfc_util_free_data(atr);
		g_free(atr);
	}

	g_object_unref(detail->invocation);
	g_object_unref(detail->object);

	g_free(detail);
}

static gboolean se_handle_get_atr(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		guint arg_handle,
		GVariant *smack_privilege)
{
	SeDataHandle *data;
	gboolean result;

	NFC_INFO(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager",
				"r") == false) {
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(SeDataHandle, 1);
	if (data == NULL)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");

		return FALSE;
	}

	data->object = g_object_ref(object);
	data->invocation = g_object_ref(invocation);
	data->handle = GUINT_TO_POINTER(arg_handle);

	result = net_nfc_server_controller_async_queue_push(
			se_get_atr_thread_func, data);
	if (result == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Se.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->object);
		g_object_unref(data->invocation);

		g_free(data);
	}

	return result;
}

static void se_open_secure_element_thread_func(gpointer user_data)
{
	SeDataSeType *detail = (SeDataSeType *)user_data;
	net_nfc_target_handle_s *handle = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	g_assert(detail != NULL);
	g_assert(detail->object != NULL);
	g_assert(detail->invocation != NULL);

#if 0 /* opening SE doesn't affect card emulation */
	gdbus_se_prev_type = net_nfc_server_se_get_se_type();
	gdbus_se_prev_mode = net_nfc_server_se_get_se_mode();
#endif

	if (detail->se_type == SECURE_ELEMENT_TYPE_UICC)
	{
#if 0 /* opening SE doesn't affect card emulation */
		/*off ESE*/
		net_nfc_controller_set_secure_element_mode(
				SECURE_ELEMENT_TYPE_ESE,
				SECURE_ELEMENT_OFF_MODE, &result);

		/*Off UICC. UICC SHOULD not be detected by external reader when
		  being communicated in internal process*/
		net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_UICC,
				SECURE_ELEMENT_OFF_MODE, &result);

		net_nfc_server_se_set_se_type(SECURE_ELEMENT_TYPE_UICC);
		net_nfc_server_se_set_se_mode(SECURE_ELEMENT_OFF_MODE);
#endif
		handle = _se_uicc_open();
		if (handle != NULL)
		{
			result = NET_NFC_OK;
		}
		else
		{
			result = NET_NFC_INVALID_STATE;
			handle = NULL;
		}
	}
	else if (detail->se_type == SECURE_ELEMENT_TYPE_ESE)
	{
#if 0 /* opening SE doesn't affect card emulation */
		/*off UICC*/
		net_nfc_controller_set_secure_element_mode(
				SECURE_ELEMENT_TYPE_UICC,
				SECURE_ELEMENT_OFF_MODE, &result);
#endif
		handle = net_nfc_server_se_open_ese();
		if (handle != NULL)
		{
			result = NET_NFC_OK;
#if 0 /* opening SE doesn't affect card emulation */
			net_nfc_server_se_set_se_type(SECURE_ELEMENT_TYPE_ESE);
			net_nfc_server_se_set_se_mode(SECURE_ELEMENT_WIRED_MODE);

			net_nfc_server_se_set_current_ese_handle(handle);
#endif
			NFC_DBG("handle [%p]", handle);

			/* increase client reference count */
			net_nfc_server_gdbus_increase_se_count(
					g_dbus_method_invocation_get_sender(
						detail->invocation));
		}
		else
		{
			result = NET_NFC_INVALID_STATE;
			handle = NULL;
		}
	}
	else
	{
		result = NET_NFC_INVALID_PARAM;
		handle = NULL;
	}

	net_nfc_gdbus_secure_element_complete_open_secure_element(
			detail->object,
			detail->invocation,
			result,
			(guint)handle);

	g_object_unref(detail->invocation);
	g_object_unref(detail->object);

	g_free(detail);
}

static gboolean se_handle_open_secure_element(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		gint arg_type,
		GVariant *smack_privilege)
{
	SeDataSeType *data;
	gboolean result;

	NFC_INFO(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager",
				"rw") == false) {
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(SeDataSeType, 1);
	if (data == NULL)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");

		return FALSE;
	}

	data->object = g_object_ref(object);
	data->invocation = g_object_ref(invocation);
	data->se_type= arg_type;

	result = net_nfc_server_controller_async_queue_push(
			se_open_secure_element_thread_func, data);
	if (result == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Se.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->object);
		g_object_unref(data->invocation);

		g_free(data);
	}

	return result;
}

static void se_send_apdu_thread_func(gpointer user_data)
{
	SeDataApdu *detail = (SeDataApdu *)user_data;
	data_s apdu_data = { NULL, 0 };
	data_s *response = NULL;
	net_nfc_error_e result = NET_NFC_OK;
	GVariant *rspdata = NULL;

	g_assert(detail != NULL);
	g_assert(detail->object != NULL);
	g_assert(detail->invocation != NULL);

	net_nfc_util_gdbus_variant_to_data_s(detail->data, &apdu_data);

	if (_se_is_uicc_handle(detail->handle) == true)
	{
		result = NET_NFC_NOT_SUPPORTED;
	}
	else if (net_nfc_server_se_is_ese_handle(detail->handle) == true)
	{
		net_nfc_controller_secure_element_send_apdu(detail->handle,
				&apdu_data, &response, &result);
	}
	else
	{
		result = NET_NFC_INVALID_HANDLE;
	}

	rspdata = net_nfc_util_gdbus_data_to_variant(response);

	net_nfc_gdbus_secure_element_complete_send_apdu(
			detail->object,
			detail->invocation,
			result,
			rspdata);

	if (response != NULL)
	{
		net_nfc_util_free_data(response);
		g_free(response);
	}

	net_nfc_util_free_data(&apdu_data);

	g_variant_unref(detail->data);

	g_object_unref(detail->invocation);
	g_object_unref(detail->object);

	g_free(detail);
}

static gboolean se_handle_send_apdu(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		guint arg_handle,
		GVariant *apdudata,
		GVariant *smack_privilege)
{
	SeDataApdu *data;
	gboolean result;

	NFC_INFO(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager",
				"rw") == false) {
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(SeDataApdu, 1);
	if (data == NULL)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");

		return FALSE;
	}

	data->object = g_object_ref(object);
	data->invocation = g_object_ref(invocation);
	data->handle = GUINT_TO_POINTER(arg_handle);
	data->data = g_variant_ref(apdudata);

	result = net_nfc_server_controller_async_queue_push(
			se_send_apdu_thread_func, data);
	if (result == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Se.ThreadError",
				"can not push to controller thread");

		g_variant_unref(data->data);

		g_object_unref(data->object);
		g_object_unref(data->invocation);

		g_free(data);
	}

	return result;
}

static void _se_set_card_emulation_thread_func(gpointer user_data)
{
	SeSetCardEmul *data = (SeSetCardEmul *)user_data;
	net_nfc_error_e result = NET_NFC_OK;

	g_assert(data != NULL);
	g_assert(data->object != NULL);
	g_assert(data->invocation != NULL);

	if (data->mode == NET_NFC_CARD_EMELATION_ENABLE)
	{
		net_nfc_controller_set_secure_element_mode(
				net_nfc_server_se_get_se_mode(),
				SECURE_ELEMENT_VIRTUAL_MODE,
				&result);
		if (result == NET_NFC_OK)
		{
			NFC_DBG("changed to CARD EMULATION ON");

			net_nfc_server_se_set_se_mode(
					SECURE_ELEMENT_VIRTUAL_MODE);
		}
		else
		{
			NFC_ERR("CARD EMULATION ON fail [%d]", result);
		}
	}
	else if (data->mode == NET_NFC_CARD_EMULATION_DISABLE)
	{
		net_nfc_controller_set_secure_element_mode(
				net_nfc_server_se_get_se_mode(),
				SECURE_ELEMENT_OFF_MODE,
				&result);
		if (result == NET_NFC_OK)
		{
			NFC_DBG("changed to CARD EMULATION OFF");

			net_nfc_server_se_set_se_mode(SECURE_ELEMENT_OFF_MODE);
		}
		else
		{
			NFC_ERR("CARD EMULATION OFF fail [%d]", result);
		}
	}
	else
	{
		result = NET_NFC_INVALID_PARAM;
	}

	net_nfc_gdbus_secure_element_complete_set_card_emulation(data->object,
			data->invocation, result);

	g_object_unref(data->invocation);
	g_object_unref(data->object);

	g_free(data);
}

static gboolean _se_handle_set_card_emulation(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		gint arg_mode,
		GVariant *smack_privilege)
{
	SeSetCardEmul *data;
	gboolean result;

	NFC_INFO(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager",
				"w") == false) {
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(SeSetCardEmul, 1);
	if (data == NULL)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");

		return FALSE;
	}

	data->object = g_object_ref(object);
	data->invocation = g_object_ref(invocation);
	data->mode = arg_mode;

	result = net_nfc_server_controller_async_queue_push(
			_se_set_card_emulation_thread_func, data);
	if (result == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Se.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->object);
		g_object_unref(data->invocation);

		g_free(data);
	}

	return result;
}

static void se_set_data_thread_func(gpointer user_data)
{
	SeDataSeType *data = (SeDataSeType *)user_data;
	gboolean isTypeChanged = FALSE;
	net_nfc_error_e result = NET_NFC_OK;

	g_assert(data != NULL);
	g_assert(data->object != NULL);
	g_assert(data->invocation != NULL);

	if (data->se_type != net_nfc_server_se_get_se_type())
	{
		result = net_nfc_server_se_change_se(data->se_type);
		isTypeChanged = TRUE;
	}

	net_nfc_gdbus_secure_element_complete_set(data->object,
			data->invocation, result);

	if (isTypeChanged)
		net_nfc_gdbus_secure_element_emit_se_type_changed(data->object,
				data->se_type);

	g_object_unref(data->invocation);
	g_object_unref(data->object);

	g_free(data);
}

static gboolean se_handle_set(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		gint arg_type,
		GVariant *smack_privilege)
{
	SeDataSeType *data;
	gboolean result;

	NFC_INFO(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager",
				"w") == false) {
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(SeDataSeType, 1);
	if (data == NULL)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");

		return FALSE;
	}

	data->object = g_object_ref(object);
	data->invocation = g_object_ref(invocation);
	data->se_type = arg_type;

	result = net_nfc_server_controller_async_queue_push(
			se_set_data_thread_func, data);
	if (result == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Se.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->object);
		g_object_unref(data->invocation);

		g_free(data);
	}

	return result;
}

static void se_get_data_thread_func(gpointer user_data)
{
	SeDataSeType *data = (SeDataSeType *)user_data;
	net_nfc_error_e result = NET_NFC_OK;

	g_assert(data != NULL);
	g_assert(data->object != NULL);
	g_assert(data->invocation != NULL);

	net_nfc_gdbus_secure_element_complete_get(data->object,
			data->invocation,
			net_nfc_server_se_get_se_type(),
			result);

	g_object_unref(data->invocation);
	g_object_unref(data->object);

	g_free(data);
}

static gboolean se_handle_get(
		NetNfcGDbusSecureElement *object,
		GDBusMethodInvocation *invocation,
		GVariant *smack_privilege)
{
	SeDataSeType *data;
	gboolean result;

	NFC_INFO(">>> REQUEST from [%s]",
			g_dbus_method_invocation_get_sender(invocation));

	/* check privilege and update client context */
	if (net_nfc_server_gdbus_check_privilege(invocation,
				smack_privilege,
				"nfc-manager",
				"r") == false) {
		NFC_ERR("permission denied, and finished request");

		return FALSE;
	}

	data = g_try_new0(SeDataSeType, 1);
	if (data == NULL)
	{
		NFC_ERR("Memory allocation failed");
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.AllocationError",
				"Can not allocate memory");

		return FALSE;
	}

	data->object = g_object_ref(object);
	data->invocation = g_object_ref(invocation);

	result = net_nfc_server_controller_async_queue_push(
			se_get_data_thread_func, data);
	if (result == FALSE)
	{
		g_dbus_method_invocation_return_dbus_error(invocation,
				"org.tizen.NetNfcService.Se.ThreadError",
				"can not push to controller thread");

		g_object_unref(data->object);
		g_object_unref(data->invocation);

		g_free(data);
	}

	return result;
}

gboolean net_nfc_server_se_init(GDBusConnection *connection)
{
	GError *error = NULL;
	gboolean result;

	if (se_skeleton)
		g_object_unref(se_skeleton);

	/* initialize UICC */
	_se_uicc_init();

	se_skeleton = net_nfc_gdbus_secure_element_skeleton_new();

	g_signal_connect(se_skeleton,
			"handle-set",
			G_CALLBACK(se_handle_set),
			NULL);

	g_signal_connect(se_skeleton,
			"handle-get",
			G_CALLBACK(se_handle_get),
			NULL);

	g_signal_connect(se_skeleton,
			"handle-set-card-emulation",
			G_CALLBACK(_se_handle_set_card_emulation),
			NULL);

	g_signal_connect(se_skeleton,
			"handle-open-secure-element",
			G_CALLBACK(se_handle_open_secure_element),
			NULL);

	g_signal_connect(se_skeleton,
			"handle-close-secure-element",
			G_CALLBACK(se_handle_close_secure_element),
			NULL);

	g_signal_connect(se_skeleton,
			"handle-get-atr",
			G_CALLBACK(se_handle_get_atr),
			NULL);

	g_signal_connect(se_skeleton,
			"handle-send-apdu",
			G_CALLBACK(se_handle_send_apdu),
			NULL);

	result = g_dbus_interface_skeleton_export(
			G_DBUS_INTERFACE_SKELETON(se_skeleton),
			connection,
			"/org/tizen/NetNfcService/SecureElement",
			&error);
	if (result == FALSE)
	{
		NFC_ERR("can not skeleton_export %s", error->message);

		g_error_free(error);

		net_nfc_server_se_deinit();
	}

	return result;
}

void net_nfc_server_se_deinit(void)
{
	if (se_skeleton)
	{
		g_object_unref(se_skeleton);
		se_skeleton = NULL;

		/* de-initialize UICC */
		_se_uicc_deinit();
	}
}

static void se_detected_thread_func(gpointer user_data)
{
	net_nfc_target_handle_s *handle = NULL;
	uint32_t devType = 0;
	GVariant *data = NULL;
	net_nfc_error_e result = NET_NFC_OK;

	g_assert(user_data != NULL);

	if (se_skeleton == NULL)
	{
		NFC_ERR("se skeleton is not initialized");

		g_variant_unref((GVariant *)user_data);

		return;
	}

	g_variant_get((GVariant *)user_data,
			"uu@a(y)",
			(guint *)&handle,
			&devType,
			&data);

	net_nfc_server_se_set_current_ese_handle(handle);

	NFC_DBG("trying to connect to ESE = [0x%p]", handle);

	if (!net_nfc_controller_connect(handle, &result))
	{
		NFC_DBG("connect failed = [%d]", result);
	}

	net_nfc_gdbus_secure_element_emit_ese_detected(
			se_skeleton,
			GPOINTER_TO_UINT(handle),
			devType,
			data);

	g_variant_unref((GVariant *)user_data);
}

static void se_transcation_thread_func(gpointer user_data)
{
	ServerSeData *detail = (ServerSeData *)user_data;

	g_assert(user_data != NULL);

	if (detail->event == NET_NFC_MESSAGE_SE_START_TRANSACTION)
	{
		NFC_DBG("launch se app");

		net_nfc_app_util_launch_se_transaction_app(
				detail->aid.buffer,
				detail->aid.length,
				detail->param.buffer,
				detail->param.length);

		NFC_DBG("launch se app end");
	}

	net_nfc_util_free_data(&detail->param);
	net_nfc_util_free_data(&detail->aid);

	g_free(detail);
}

void net_nfc_server_se_detected(void *info)
{
	net_nfc_request_target_detected_t *se_target =
		(net_nfc_request_target_detected_t *)info;
	GVariant *parameter;
	GVariant *data;

	data = net_nfc_util_gdbus_buffer_to_variant(
			se_target->target_info_values.buffer,
			se_target->target_info_values.length);

	parameter = g_variant_new("uu@a(y)",
			GPOINTER_TO_UINT(se_target->handle),
			se_target->devType,
			data);
	if (parameter != NULL) {
		if (net_nfc_server_controller_async_queue_push(
					se_detected_thread_func,
					parameter) == FALSE) {
			NFC_ERR("can not push to controller thread");

			g_variant_unref(parameter);
		}
	} else {
		NFC_ERR("g_variant_new failed");
	}

	/* FIXME : should be removed when plugins would be fixed*/
	_net_nfc_util_free_mem(info);
}

void net_nfc_server_se_transaction_received(void *info)
{
	net_nfc_request_se_event_t *se_event =
		(net_nfc_request_se_event_t *)info;
	ServerSeData *detail;

	detail = g_try_new0(ServerSeData, 1);
	if (detail != NULL) {
		detail->event = se_event->request_type;

		if (se_event->aid.buffer != NULL && se_event->aid.length > 0) {
			if (net_nfc_util_alloc_data(&detail->aid,
						se_event->aid.length) == true) {
				memcpy(detail->aid.buffer, se_event->aid.buffer,
						se_event->aid.length);
			}
		}

		if (se_event->param.buffer != NULL &&
				se_event->param.length > 0) {
			if (net_nfc_util_alloc_data(&detail->param,
						se_event->param.length) == true) {
				memcpy(detail->param.buffer,
						se_event->param.buffer,
						se_event->param.length);
			}
		}

		if (net_nfc_server_controller_async_queue_push(
					se_transcation_thread_func, detail) == FALSE) {
			NFC_ERR("can not push to controller thread");

			net_nfc_util_free_data(&detail->param);
			net_nfc_util_free_data(&detail->aid);

			g_free(detail);
		}
	} else {
		NFC_ERR("g_new0 failed");
	}

	/* FIXME : should be removed when plugins would be fixed*/
	net_nfc_util_free_data(&se_event->param);
	net_nfc_util_free_data(&se_event->aid);

	_net_nfc_util_free_mem(info);
}
