/*
 * Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Flora License, Version 1.0 (the "License");
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

#include <pthread.h>
#include <glib.h>
#include <malloc.h>

#include "vconf.h"
#include "tapi_common.h"
#include "ITapiSim.h"

#include "net_nfc_controller_private.h"
#include "net_nfc_util_private.h"
#include "net_nfc_typedef.h"
#include "net_nfc_debug_private.h"
#include "net_nfc_service_private.h"
#include "net_nfc_app_util_private.h"
#include "net_nfc_server_ipc_private.h"
#include "net_nfc_server_dispatcher_private.h"
#include "net_nfc_service_se_private.h"

/* define */
/* For ESE*/
static net_nfc_se_setting_t g_se_setting;

/* For UICC */
static struct tapi_handle *uicc_handle = NULL;
static bool net_nfc_service_check_sim_state(void);
static void _uicc_transmit_apdu_cb(TapiHandle *handle, int result, void *data, void *user_data);
static void _uicc_get_atr_cb(TapiHandle *handle, int result, void *data, void *user_data);
static void _uicc_status_noti_cb(TapiHandle *handle, const char *noti_id, void *data, void *user_data);

net_nfc_se_setting_t *net_nfc_service_se_get_se_setting()
{
	return &g_se_setting;
}

net_nfc_target_handle_s *net_nfc_service_se_get_current_ese_handle()
{
	return g_se_setting.current_ese_handle;
}

void net_nfc_service_se_set_current_ese_handle(net_nfc_target_handle_s *handle)
{
	g_se_setting.current_ese_handle = handle;
}

uint8_t net_nfc_service_se_get_se_type()
{
	return g_se_setting.type;
}

void net_nfc_service_se_set_se_type(uint8_t type)
{
	g_se_setting.type = type;
}

uint8_t net_nfc_service_se_get_se_mode()
{
	return g_se_setting.mode;
}

void net_nfc_service_se_set_se_mode(uint8_t mode)
{
	g_se_setting.mode = mode;
}

net_nfc_error_e net_nfc_service_se_change_se(uint8_t type)
{
	net_nfc_error_e result = NET_NFC_OK;

	switch (type)
	{
	case SECURE_ELEMENT_TYPE_UICC :
		/*turn off ESE*/
		net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_ESE, SECURE_ELEMENT_OFF_MODE, &result);

		/*turn on UICC*/
		net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_UICC, SECURE_ELEMENT_VIRTUAL_MODE, &result);
		if (result == NET_NFC_OK)
		{
			DEBUG_SERVER_MSG("changed to SECURE_ELEMENT_TYPE_UICC");

			net_nfc_service_se_set_se_type(SECURE_ELEMENT_TYPE_UICC);
			net_nfc_service_se_set_se_mode(SECURE_ELEMENT_VIRTUAL_MODE);

			if (vconf_set_int(VCONFKEY_NFC_SE_TYPE, VCONFKEY_NFC_SE_TYPE_UICC) != 0)
			{
				DEBUG_ERR_MSG("vconf_set_int failed");
			}
		}
		else
		{
			DEBUG_ERR_MSG("SECURE_ELEMENT_TYPE_UICC, SECURE_ELEMENT_VIRTUAL_MODE failed [%d]", result);
		}
		break;

	case SECURE_ELEMENT_TYPE_ESE :
		/*turn off UICC*/
		net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_UICC, SECURE_ELEMENT_OFF_MODE, &result);

		/*turn on ESE*/
		net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_ESE, SECURE_ELEMENT_VIRTUAL_MODE, &result);
		if (result == NET_NFC_OK)
		{
			DEBUG_SERVER_MSG("changed to SECURE_ELEMENT_TYPE_ESE");

			net_nfc_service_se_set_se_type(SECURE_ELEMENT_TYPE_ESE);
			net_nfc_service_se_set_se_mode(SECURE_ELEMENT_VIRTUAL_MODE);

			if (vconf_set_int(VCONFKEY_NFC_SE_TYPE, VCONFKEY_NFC_SE_TYPE_ESE) != 0)
			{
				DEBUG_ERR_MSG("vconf_set_int failed");
			}
		}
		else
		{
			DEBUG_ERR_MSG("SECURE_ELEMENT_TYPE_ESE, SECURE_ELEMENT_VIRTUAL_MODE failed [%d]", result);
		}
		break;

	default :
		{
			net_nfc_error_e result_ese, result_uicc;

			net_nfc_service_se_set_se_type(SECURE_ELEMENT_TYPE_INVALID);
			net_nfc_service_se_set_se_mode(SECURE_ELEMENT_OFF_MODE);

			/*turn off ESE*/
			net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_ESE, SECURE_ELEMENT_OFF_MODE, &result_ese);

			/*turn off UICC*/
			net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_UICC, SECURE_ELEMENT_OFF_MODE, &result_uicc);
			if (result_ese != NET_NFC_INVALID_HANDLE && result_uicc != NET_NFC_INVALID_HANDLE)
			{
				DEBUG_SERVER_MSG("SE off all");
				if (vconf_set_int(VCONFKEY_NFC_SE_TYPE, VCONFKEY_NFC_SE_TYPE_NONE) != 0)
				{
					DEBUG_ERR_MSG("vconf_set_int failed");
				}
				result = NET_NFC_OK;
			}
			else
			{
				DEBUG_ERR_MSG("ALL OFF failed, ese [%d], uicc [%d]", result_ese, result_uicc);
				result = NET_NFC_INVALID_HANDLE;
			}
		}
		break;
	}

	return result;
}

void net_nfc_service_se_detected(net_nfc_request_msg_t *msg)
{
	net_nfc_request_target_detected_t *detail_msg = (net_nfc_request_target_detected_t *)(msg);
	net_nfc_target_handle_s *handle = NULL;
	net_nfc_error_e state = NET_NFC_OK;
	net_nfc_response_open_internal_se_t resp = { 0 };

	if (detail_msg == NULL)
	{
		return;
	}

	handle = detail_msg->handle;
	g_se_setting.current_ese_handle = handle;

	DEBUG_SERVER_MSG("trying to connect to ESE = [0x%p]", handle);

	if (!net_nfc_controller_connect(handle, &state))
	{
		DEBUG_SERVER_MSG("connect failed = [%d]", state);
		resp.result = state;
	}

#ifdef BROADCAST_MESSAGE
	net_nfc_server_set_server_state(NET_NFC_SE_CONNECTED);
#endif

	resp.handle = handle;
	resp.trans_param = g_se_setting.open_request_trans_param;
	resp.se_type = SECURE_ELEMENT_TYPE_ESE;

	DEBUG_SERVER_MSG("trans param = [%p]", resp.trans_param);

	net_nfc_send_response_msg(detail_msg->client_fd, NET_NFC_MESSAGE_OPEN_INTERNAL_SE, &resp, sizeof(net_nfc_response_open_internal_se_t), NULL);

	g_se_setting.open_request_trans_param = NULL;
}

net_nfc_error_e net_nfc_service_se_close_ese()
{
	net_nfc_error_e result = NET_NFC_OK;

	if (g_se_setting.current_ese_handle != NULL)
	{
		if (net_nfc_controller_disconnect(g_se_setting.current_ese_handle, &result) == false)
		{
			net_nfc_controller_exception_handler();
		}
		net_nfc_service_se_set_current_ese_handle(NULL);
	}

	return result;
}

bool net_nfc_service_tapi_init(void)
{
	char **cpList = NULL;

	DEBUG_SERVER_MSG("tapi init");

	cpList = tel_get_cp_name_list();

	uicc_handle = tel_init(cpList[0]);
	if (uicc_handle == NULL)
	{
		int error;

		error = tel_register_noti_event(uicc_handle, TAPI_NOTI_SIM_STATUS, _uicc_status_noti_cb, NULL);
	}
	else
	{
		DEBUG_SERVER_MSG("tel_init() failed");
		return false;
	}

	DEBUG_SERVER_MSG("tel_init() is success");

	return net_nfc_service_check_sim_state();
}

void net_nfc_service_tapi_deinit(void)
{
	DEBUG_SERVER_MSG("deinit tapi");

	tel_deregister_noti_event(uicc_handle, TAPI_NOTI_SIM_STATUS);

	tel_deinit(uicc_handle);
}

bool net_nfc_service_transfer_apdu(int client_fd, data_s *apdu, void *trans_param)
{
	net_nfc_request_msg_t *param = NULL;
	TelSimApdu_t apdu_data = { 0 };
	int result;

	DEBUG_SERVER_MSG("tranfer apdu");

	if (apdu == NULL)
		return false;

	apdu_data.apdu = apdu->buffer;
	apdu_data.apdu_len = apdu->length;

	/* make param */
	_net_nfc_util_alloc_mem(param, sizeof(net_nfc_request_msg_t));
	if (param != NULL)
	{
		param->client_fd = client_fd;
		param->user_param = (uint32_t)trans_param;

		result = tel_req_sim_apdu(uicc_handle, &apdu_data, _uicc_transmit_apdu_cb, param);
		if (result == 0)
		{
			DEBUG_SERVER_MSG("sim apdu request is success");
		}
		else
		{
			DEBUG_ERR_MSG("request sim apdu is failed with error = [%d]", result);
			return false;
		}
	}
	else
	{
		DEBUG_ERR_MSG("_net_nfc_util_alloc_mem failed");
		return false;
	}

	return true;
}

bool net_nfc_service_request_atr(int client_fd, void *trans_param)
{
	int result;
	net_nfc_request_msg_t *param = NULL;

	/* make param */
	_net_nfc_util_alloc_mem(param, sizeof(net_nfc_request_msg_t));
	if (param != NULL)
	{
		param->client_fd = client_fd;
		param->user_param = (uint32_t)trans_param;

		result = tel_req_sim_atr(uicc_handle, _uicc_get_atr_cb, param);
		if (result == 0)
		{
			DEBUG_SERVER_MSG("request is success");
		}
		else
		{
			DEBUG_SERVER_MSG("failed to request ATR  = [%d]", result);
			return false;
		}
	}
	else
	{
		DEBUG_ERR_MSG("_net_nfc_manager_util_alloc_mem failed");
		return false;
	}

	return true;
}

static bool net_nfc_service_check_sim_state(void)
{
	TelSimCardStatus_t state = (TelSimCardStatus_t)0;
	int b_card_changed = 0;
	int error;

	DEBUG_SERVER_MSG("check sim state");

	error = tel_get_sim_init_info(uicc_handle, &state, &b_card_changed);

	DEBUG_SERVER_MSG("current sim init state = [%d]", state);

	if (error != 0)
	{
		DEBUG_SERVER_MSG("error = [%d]", error);
		return false;
	}
	else if (state == TAPI_SIM_STATUS_SIM_INIT_COMPLETED || state == TAPI_SIM_STATUS_SIM_INITIALIZING)
	{
		DEBUG_SERVER_MSG("sim is initialized");
	}
	else
	{
		DEBUG_SERVER_MSG("sim is not initialized");
		return false;
	}

	return true;
}

void _uicc_transmit_apdu_cb(TapiHandle *handle, int result, void *data, void *user_data)
{
	TelSimApduResp_t *apdu = (TelSimApduResp_t *)data;
	net_nfc_response_send_apdu_t resp = { 0 };
	net_nfc_request_msg_t *param = (net_nfc_request_msg_t *)user_data;

	DEBUG_SERVER_MSG("_uicc_transmit_apdu_cb");

	if (result == 0)
	{
		resp.result = NET_NFC_OK;
	}
	else
	{
		resp.result = NET_NFC_OPERATION_FAIL;
	}

	resp.trans_param = (void *)param->user_param;

	if (apdu != NULL && apdu->apdu_resp_len > 0)
	{
		resp.data.length = apdu->apdu_resp_len;

		DEBUG_MSG("send response send apdu msg");
		net_nfc_send_response_msg(param->client_fd, NET_NFC_MESSAGE_SEND_APDU_SE, (void *)&resp, sizeof(net_nfc_response_send_apdu_t), apdu->apdu_resp, apdu->apdu_resp_len, NULL);
	}
	else
	{
		DEBUG_MSG("send response send apdu msg");
		net_nfc_send_response_msg(param->client_fd, NET_NFC_MESSAGE_SEND_APDU_SE, (void *)&resp, sizeof(net_nfc_response_send_apdu_t), NULL);
	}

	_net_nfc_util_free_mem(param);
}

void _uicc_get_atr_cb(TapiHandle *handle, int result, void *data, void *user_data)
{
	net_nfc_request_msg_t *param = (net_nfc_request_msg_t *)user_data;

	/* TODO : response message */

	DEBUG_SERVER_MSG("_uicc_get_atr_cb");

	_net_nfc_util_free_mem(param);
}

void _uicc_status_noti_cb(TapiHandle *handle, const char *noti_id, void *data, void *user_data)
{
	TelSimCardStatus_t *status = (TelSimCardStatus_t *)data;

	/* TODO :  */
	DEBUG_SERVER_MSG("_uicc_status_noti_cb");

	switch (*status)
	{
	case TAPI_SIM_STATUS_SIM_INIT_COMPLETED :
		DEBUG_SERVER_MSG("TAPI_SIM_STATUS_SIM_INIT_COMPLETED");
		break;

	case TAPI_SIM_STATUS_CARD_REMOVED :
		DEBUG_SERVER_MSG("TAPI_SIM_STATUS_CARD_REMOVED");
		break;

	default :
		DEBUG_SERVER_MSG("unknown status [%d]", *status);
		break;
	}
}

bool net_nfc_service_se_transaction_receive(net_nfc_request_msg_t* msg)
{
	bool res = true;
	net_nfc_request_se_event_t *se_event = (net_nfc_request_se_event_t *)msg;

	if (se_event->request_type == NET_NFC_MESSAGE_SE_START_TRANSACTION)
	{
		DEBUG_SERVER_MSG("launch se app");

		net_nfc_app_util_launch_se_transaction_app(se_event->aid.buffer, se_event->aid.length, se_event->param.buffer, se_event->param.length);

		DEBUG_SERVER_MSG("launch se app end");
	}

	return res;
}
