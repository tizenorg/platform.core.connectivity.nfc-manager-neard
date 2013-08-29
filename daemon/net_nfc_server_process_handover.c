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

#include "bluetooth-api.h"
#include "net_nfc_server_handover.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_ndef_record.h"
#include "net_nfc_util_handover.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_server_llcp.h"
#include "net_nfc_server_process_handover.h"
#include "net_nfc_server_process_snep.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_server_handover_bss.h"
#include "net_nfc_server_handover_bt.h"

typedef void (*_net_nfc_server_handover_create_carrier_msg_cb)(
		net_nfc_error_e result,
		ndef_message_s *selector,
		void *user_param);

typedef struct _net_nfc_handover_context_t
{
	net_nfc_target_handle_s *handle;
	net_nfc_error_e result;
	net_nfc_llcp_socket_t socket;
	uint32_t state;
	net_nfc_conn_handover_carrier_type_e type;
	data_s data;
	ndef_record_s *record;
	void *user_param;
}
net_nfc_handover_context_t;

#define NET_NFC_CH_CONTEXT net_nfc_target_handle_s *handle;\
	net_nfc_llcp_socket_t socket;\
	net_nfc_error_e result;\
	int step;\
	net_nfc_conn_handover_carrier_type_e type;\
	void *user_param;

typedef struct _net_nfc_server_handover_create_config_context_t
{
	NET_NFC_CH_CONTEXT;

	_net_nfc_server_handover_create_carrier_msg_cb cb;
	net_nfc_conn_handover_carrier_type_e current_type;
	ndef_message_s *ndef_message;
	ndef_message_s *requester; /* for low power selector */

	ndef_record_s *record;
}
net_nfc_server_handover_create_config_context_t;

typedef struct _net_nfc_server_handover_process_config_context_t
{
	NET_NFC_CH_CONTEXT;

	net_nfc_server_handover_process_carrier_record_cb cb;
}net_nfc_server_handover_process_config_context_t;



static void _net_nfc_server_handover_send_response(net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e carrier,
		data_s *ac_data,
		void *user_param);

static void _net_nfc_server_handover_client_process(
		net_nfc_handover_context_t *context);

static void _net_nfc_server_handover_client_error_cb(
		net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param);

static void _net_nfc_server_handover_client_connected_cb(
		net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param);

static void _net_nfc_server_handover_get_response_process(
		net_nfc_handover_context_t *context);

static net_nfc_error_e
_net_nfc_server_handover_create_requester_from_rawdata(
		ndef_message_s **requestor,
		data_s *data);

static net_nfc_error_e
_net_nfc_server_handover_create_requester_carrier_configs(
		net_nfc_conn_handover_carrier_type_e type,
		void *cb,
		void *user_param);

static bool _net_nfc_server_handover_check_hr_record_validation(
		ndef_message_s *message);

static bool _net_nfc_server_handover_check_hs_record_validation(
		ndef_message_s *message);

static net_nfc_error_e
_net_nfc_server_handover_create_selector_carrier_configs(
		net_nfc_conn_handover_carrier_type_e type,
		void *cb,
		void *user_param);

static int _net_nfc_server_handover_iterate_create_carrier_configs(
		net_nfc_server_handover_create_config_context_t *context);

static int _net_nfc_server_handover_iterate_carrier_configs_to_next(
		net_nfc_server_handover_create_config_context_t *context);

static int _net_nfc_server_handover_iterate_carrier_configs_step(
		net_nfc_server_handover_create_config_context_t *context);

static void _net_nfc_server_handover_server_process(
		net_nfc_handover_context_t *context);

////////////////////////////////////////////////////////////////////////////

static void _net_nfc_server_handover_send_response(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e carrier,
		data_s *ac_data,
		void *user_param)
{
	HandoverRequestData *handover_data = (HandoverRequestData *)user_param;

	g_assert(handover_data != NULL);
	g_assert(handover_data->invocation != NULL);
	g_assert(handover_data->handoverobj != NULL);

	net_nfc_gdbus_handover_complete_request(
			handover_data->handoverobj,
			handover_data->invocation,
			result,
			carrier,
			net_nfc_util_gdbus_data_to_variant(ac_data));

	if (handover_data->data)
	{
		g_free(handover_data->data->buffer);
		g_free(handover_data->data);
	}

	g_object_unref(handover_data->invocation);
	g_object_unref(handover_data->handoverobj);

	g_free(handover_data);
}

static net_nfc_error_e _net_nfc_server_handover_convert_ndef_message_to_data(
		ndef_message_s *msg,
		data_s *data)
{
	net_nfc_error_e result;
	uint32_t length;

	if (msg == NULL || data == NULL)
	{
		return NET_NFC_INVALID_PARAM;
	}

	length = net_nfc_util_get_ndef_message_length(msg);
	if (length > 0)
	{
		net_nfc_util_alloc_data(data, length);
		result = net_nfc_util_convert_ndef_message_to_rawdata(msg, data);
	}
	else
	{
		result = NET_NFC_INVALID_PARAM;
	}

	return result;
}

static void _net_nfc_server_handover_bt_get_carrier_record_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_state_e cps,
		ndef_record_s *carrier,
		uint32_t aux_data_count,
		ndef_record_s *aux_data,
		void *user_param)
{
	net_nfc_server_handover_create_config_context_t *context =
		(net_nfc_server_handover_create_config_context_t *)user_param;

	/* append record to ndef message */
	if (result == NET_NFC_OK)
	{
		ndef_record_s *record;

		/* FIXME : copy record and append */
		net_nfc_util_create_record(carrier->TNF,
				&carrier->type_s,
				&carrier->id_s,
				&carrier->payload_s,
				&record);

		if ((result = net_nfc_util_append_carrier_config_record(
						context->ndef_message,
						record,
						cps)) == NET_NFC_OK)
		{
			DEBUG_SERVER_MSG("net_nfc_util_append_carrier"
					"_config_record success");
		}
		else
		{
			DEBUG_ERR_MSG("net_nfc_util_append_carrier"
					"_config_record failed [%d]",
					result);
			net_nfc_util_free_record(record);
		}

		g_idle_add(
				(GSourceFunc)_net_nfc_server_handover_iterate_carrier_configs_to_next,
				(gpointer)context);
	}

	/* don't free context */
}

static void _net_nfc_server_handover_bt_process_carrier_record_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_s *data,
		void *user_param)
{
	net_nfc_server_handover_process_config_context_t *context =
		(net_nfc_server_handover_process_config_context_t *)user_param;

	if (result == NET_NFC_OK)
	{
		if (context->cb != NULL)
		{
			context->cb(result, type, data, context->user_param);
		}
	}
	else
	{
		DEBUG_ERR_MSG("_handover_bt_process_carrier_record failed [%d]",
				result);
	}

	_net_nfc_util_free_mem(context);
}


net_nfc_error_e net_nfc_server_handover_get_carrier_record_by_priority_order(
		ndef_message_s *request, ndef_record_s **record)
{
	net_nfc_error_e result;
	unsigned int carrier_count = 0;

	LOGD("[%s] START", __func__);

	if (request == NULL || record == NULL)
	{
		return NET_NFC_NULL_PARAMETER;
	}

	*record = NULL;

	if ((result = net_nfc_util_get_alternative_carrier_record_count(
					request,
					&carrier_count)) == NET_NFC_OK)
	{
		int idx, priority;
		net_nfc_conn_handover_carrier_type_e carrier_type =
			NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;

		for (priority = NET_NFC_CONN_HANDOVER_CARRIER_BT;*record == NULL
				&& priority < NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;priority++)
		{
			/* check each carrier record and create matched record */
			for (idx = 0; idx < carrier_count; idx++)
			{
				if ((net_nfc_util_get_alternative_carrier_type(
								request,
								idx,
								&carrier_type)== NET_NFC_OK)
						&& (carrier_type == priority))
				{
					DEBUG_SERVER_MSG("selected carrier type"
							" = [%d]", carrier_type);
					net_nfc_util_get_carrier_config_record(
							request,
							idx,
							record);
					result = NET_NFC_OK;
					break;
				}
			}
		}
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_util_get_alternative_carrier"
				"_record_count failed");
	}

	LOGD("[%s] END", __func__);

	return result;
}

static net_nfc_error_e _net_nfc_server_handover_create_requester_from_rawdata(
		ndef_message_s **requestor,
		data_s *data)
{
	net_nfc_error_e result;

	if (requestor == NULL)
		return NET_NFC_NULL_PARAMETER;

	*requestor = NULL;

	result = net_nfc_util_create_ndef_message(requestor);
	if (result == NET_NFC_OK)
	{
		result = net_nfc_util_convert_rawdata_to_ndef_message(data,
				*requestor);

		if (result == NET_NFC_OK)
		{
			if (_net_nfc_server_handover_check_hr_record_validation(
						*requestor) == true)
			{
				result = NET_NFC_OK;
			}
			else
			{
				DEBUG_ERR_MSG("record is not valid or"
						" is not available");
				net_nfc_util_free_ndef_message(*requestor);
				result = NET_NFC_INVALID_PARAM;
			}
		}
		else
		{
			DEBUG_ERR_MSG("_net_nfc_ndef_rawdata_to_ndef"
					"failed [%d]",result);
		}
	} else {
		DEBUG_ERR_MSG("net_nfc_util_create_ndef_message failed [%d]",
				result);
	}

	return result;
}

net_nfc_error_e net_nfc_server_handover_create_selector_from_rawdata(
		ndef_message_s **selector, data_s *data)
{
	net_nfc_error_e result;

	if (selector == NULL)
		return NET_NFC_NULL_PARAMETER;

	*selector = NULL;

	if ((result = net_nfc_util_create_ndef_message(selector))
			== NET_NFC_OK)
	{
		if ((result = net_nfc_util_convert_rawdata_to_ndef_message(data,
						*selector)) == NET_NFC_OK)
		{

			/* if record is not Hs record, then */
			if (_net_nfc_server_handover_check_hs_record_validation(
						*selector) == true)
			{
				result = NET_NFC_OK;
			}
			else
			{
				DEBUG_ERR_MSG("record is not valid or is "
						"not available");
				net_nfc_util_free_ndef_message(*selector);
				result = NET_NFC_INVALID_PARAM;
			}
		}
		else
		{
			DEBUG_ERR_MSG("_net_nfc_ndef_rawdata_to_ndef"
					" failed [%d]",result);
		}
	}
	else
	{
		DEBUG_ERR_MSG("_net_nfc_util_create_ndef_message"
				" failed [%d]",result);
	}

	return result;
}

static bool _net_nfc_server_handover_check_hr_record_validation(
		ndef_message_s *message)
{
	unsigned int count;
	ndef_record_s *rec;

	LOGD("[%s] START", __func__);

	if (message == NULL)
		return false;

	rec = (ndef_record_s *)message->records;

	if (memcmp(rec->type_s.buffer, CH_REQ_RECORD_TYPE,
				rec->type_s.length) != 0)
	{
		DEBUG_ERR_MSG("This is not connection handover"
				" request message");
		goto ERROR;
	}

	if (rec->payload_s.buffer[0] != CH_VERSION)
	{
		DEBUG_ERR_MSG("connection handover version is not matched");
		goto ERROR;
	}

	if (net_nfc_util_get_alternative_carrier_record_count(message, &count)
			!= NET_NFC_OK || count == 0)
	{
		DEBUG_ERR_MSG("there is no carrier reference");
		goto ERROR;
	}

	LOGD("[%s] END", __func__);

	return true;

ERROR :
	LOGD("[%s] END", __func__);

	return false;
}

static bool _net_nfc_server_handover_check_hs_record_validation(
		ndef_message_s *message)
{
	unsigned int count;
	ndef_record_s *rec;

	LOGD("[%s] START", __func__);

	if (message == NULL)
		return false;

	rec = (ndef_record_s *)message->records;

	if (memcmp(rec->type_s.buffer, CH_SEL_RECORD_TYPE,
				rec->type_s.length) != 0)
	{
		DEBUG_ERR_MSG("This is not connection handover"
				" request message");
		goto ERROR;
	}

	if (net_nfc_util_get_alternative_carrier_record_count(
				message,&count)!= NET_NFC_OK || count == 0)
	{
		DEBUG_ERR_MSG("there is no carrrier reference");
		goto ERROR;
	}

	/*	check version */
	if (rec->payload_s.buffer[0] != CH_VERSION)
	{
		DEBUG_ERR_MSG("connection handover version"
				" is not matched");
		goto ERROR;
	}

	LOGD("[%s] END", __func__);

	return true;

ERROR :
	LOGD("[%s] END", __func__);

	return false;
}

static int _net_nfc_server_handover_iterate_carrier_configs_step(
		net_nfc_server_handover_create_config_context_t *context)
{
	LOGD("[%s:%d] START", __func__, __LINE__);

	if (context == NULL)
	{
		return 0;
	}

	if (context->cb != NULL)
	{
		context->cb(NET_NFC_OK, context->ndef_message,
				context->user_param);
	}

	if (context->ndef_message != NULL)
	{
		net_nfc_util_free_ndef_message(context->ndef_message);
	}

	_net_nfc_util_free_mem(context);

	LOGD("[%s:%d] END", __func__, __LINE__);

	return 0;
}

static int _net_nfc_server_handover_iterate_carrier_configs_to_next(
		net_nfc_server_handover_create_config_context_t *context)
{
	if (context->result == NET_NFC_OK || context->result == NET_NFC_BUSY)
	{
		if (context->type == NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN)
		{
			if (context->current_type <
					NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN)
			{
				context->current_type++;
			}
		}
		else
		{
			context->current_type =
				NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;
		}

		g_idle_add(
				(GSourceFunc)_net_nfc_server_handover_iterate_create_carrier_configs,
				(gpointer)context);
	}
	else
	{
		DEBUG_ERR_MSG("context->result is error [%d]", context->result);

		g_idle_add(
				(GSourceFunc)_net_nfc_server_handover_iterate_carrier_configs_step,
				(gpointer)context);
	}

	return 0;
}

static int _net_nfc_server_handover_iterate_create_carrier_configs(
		net_nfc_server_handover_create_config_context_t *context)
{
	LOGD("[%s:%d] START", __func__, __LINE__);

	switch (context->current_type)
	{
	case NET_NFC_CONN_HANDOVER_CARRIER_BT :
		DEBUG_MSG("[NET_NFC_CONN_HANDOVER_CARRIER_BT]");
		net_nfc_server_handover_bt_get_carrier_record(
				_net_nfc_server_handover_bt_get_carrier_record_cb,
				context);
		break;

		//	case NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS :
		//		DEBUG_MSG("[NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS]");
		//		g_idle_add(
		//		(GSourceFunc)net_nfc_service_llcp_handover_append_wifi_carrier_config,
		//				context);
		//		break;

		//	case NET_NFC_CONN_HANDOVER_CARRIER_WIFI_IBSS :
		//		DEBUG_MSG("[NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS]");
		//		g_idle_add(
		//		(GSourceFunc)_net_nfc_server_handover_append_wifi_carrier_config,
		//			context);
		//		break;

	case NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN :
		DEBUG_MSG("[NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN]");
		g_idle_add(
				(GSourceFunc)_net_nfc_server_handover_iterate_carrier_configs_step,
				(gpointer)context);
		break;

	default :
		DEBUG_MSG("[unknown : %d]", context->current_type);
		g_idle_add(
				(GSourceFunc)_net_nfc_server_handover_iterate_carrier_configs_step,
				(gpointer)context);
		break;
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return 0;
}

static net_nfc_error_e _net_nfc_server_handover_create_requester_carrier_configs(
		net_nfc_conn_handover_carrier_type_e type,
		void *cb,
		void *user_param)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_server_handover_create_config_context_t *context = NULL;

	LOGD("[%s:%d] START", __func__, __LINE__);

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	if (context != NULL)
	{
		context->type = type;
		if (type == NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN)
		{
			context->current_type =
				NET_NFC_CONN_HANDOVER_CARRIER_BT;
		}
		else
			context->current_type = context->type;

		context->cb = cb;
		context->user_param = user_param;
		net_nfc_util_create_handover_request_message(
				&context->ndef_message);

		/* append carrier record */
		g_idle_add(
				(GSourceFunc)_net_nfc_server_handover_iterate_create_carrier_configs,
				(gpointer)context);
	}
	else
	{
		DEBUG_ERR_MSG("alloc failed");
		result = NET_NFC_ALLOC_FAIL;
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return result;
}

static net_nfc_error_e _net_nfc_server_handover_create_selector_carrier_configs(
		net_nfc_conn_handover_carrier_type_e type,
		void *cb,
		void *user_param)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_server_handover_create_config_context_t *context = NULL;

	LOGD("[%s:%d] START", __func__, __LINE__);

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	if (context != NULL)
	{
		context->type = type;
		if (type == NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN)
			context->current_type = NET_NFC_CONN_HANDOVER_CARRIER_BT;
		else
			context->current_type = context->type;

		context->cb = cb;
		context->user_param = user_param;
		net_nfc_util_create_handover_select_message(
				&context->ndef_message);

		/* append carrier record */
		g_idle_add(
				(GSourceFunc)_net_nfc_server_handover_iterate_create_carrier_configs,
				(gpointer)context);
	}
	else
	{
		DEBUG_ERR_MSG("alloc failed");
		result = NET_NFC_ALLOC_FAIL;
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return result;
}

net_nfc_error_e net_nfc_server_handover_process_carrier_record(
		ndef_record_s *carrier, void *cb, void *user_param)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_server_handover_process_config_context_t *context = NULL;

	LOGD("[%s:%d] START", __func__, __LINE__);

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	if (context != NULL)
	{
		net_nfc_conn_handover_carrier_type_e type;

		net_nfc_util_get_alternative_carrier_type_from_record(
				carrier,
				&type);

		context->type = type;
		context->user_param = user_param;
		context->cb = cb;
		context->step = NET_NFC_LLCP_STEP_01;

		/* process carrier record */
		switch (type)
		{
		case NET_NFC_CONN_HANDOVER_CARRIER_BT :
			DEBUG_MSG("[NET_NFC_CONN_HANDOVER_CARRIER_BT]");
			net_nfc_server_handover_bt_process_carrier_record(
					carrier,
					_net_nfc_server_handover_bt_process_carrier_record_cb,
					context);
			break;

		case NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS :
			DEBUG_MSG("[NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS]");
			net_nfc_server_handover_bss_process_carrier_record(
					carrier,
					NULL,
					NULL);
			_net_nfc_util_free_mem(context);
			break;

		case NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN :
			DEBUG_MSG("[NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN]");
			_net_nfc_util_free_mem(context);
			break;

		default :
			DEBUG_MSG("[unknown]");
			_net_nfc_util_free_mem(context);
			break;
		}
	}
	else
	{
		DEBUG_ERR_MSG("alloc failed");
		result = NET_NFC_ALLOC_FAIL;
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return result;
}

#if 0
static net_nfc_error_e _net_nfc_server_handover_snep_client_cb(
		net_nfc_snep_handle_h handle,
		net_nfc_error_e result,
		uint32_t type,
		data_s *data,
		void *user_param)
{
	_net_nfc_server_handover_client_context_t *context =
		(_net_nfc_server_handover_client_context_t *)user_param;

	DEBUG_SERVER_MSG("type [%d], result [%d], data [%p], user_param [%p]",
			type, result, data, user_param);

	switch (type) {
	case SNEP_RESP_SUCCESS :
		{
			ndef_message_s *selector;

			result = net_nfc_server_handover_create_selector_from_rawdata(
					&selector,
					data);
			if (result == NET_NFC_OK) {
				if (false /* is low power ??? */) {
					result =
						_net_nfc_server_handover_process_selector_msg(
								selector,
								user_param);
				} else {
					result =
						_net_nfc_server_handover_process_selector_msg(
								selector,
								user_param);
				}

				if (result != NET_NFC_OK) {
					DEBUG_ERR_MSG("_net_nfc_server_handover_process"
							"_selector_msg failed [%d]",result);
					if (context->cb != NULL) {
						context->cb(result,
								context->type,
								NULL,
								context->user_param);
					}
					_net_nfc_util_free_mem(context);
				}

				net_nfc_util_free_ndef_message(selector);
			} else {
				DEBUG_ERR_MSG("_net_nfc_server_handover_create"
						"_selector_from_rawdata failed [%d]",result);
				if (context->cb != NULL) {
					context->cb(result,
							context->type,
							NULL,
							context->user_param);
				}
				_net_nfc_util_free_mem(context);
			}
		}
		break;

	case SNEP_RESP_BAD_REQ :
	case SNEP_RESP_EXCESS_DATA :
	case SNEP_RESP_NOT_FOUND :
	case SNEP_RESP_NOT_IMPLEMENT :
	case SNEP_RESP_REJECT :
	case SNEP_RESP_UNSUPPORTED_VER :
	default :
		{
			DEBUG_ERR_MSG("error response [0x%02x]", type);
			if (context->cb != NULL) {
				context->cb(result, context->type, NULL,
						context->user_param);
			}
			_net_nfc_util_free_mem(context);
		}
		break;
	}

	return result;
}


static void _net_nfc_server_handover_create_requester_carrier_configs_cb(
		net_nfc_error_e result,
		ndef_message_s *msg,
		void *user_param)
{
	_net_nfc_server_handover_client_context_t *context =
		(_net_nfc_server_handover_client_context_t *)user_param;
	data_s data;

	if (context == NULL)
	{
		return;
	}

	if (msg != NULL) {
		/* convert ndef message */
		if ((result = _net_nfc_server_handover_convert_ndef_message_to_data(msg,
						&data)) == NET_NFC_OK) {
			net_nfc_service_snep_client(context->handle,
					SNEP_SAN,
					0,
					SNEP_REQ_GET,
					&data,
					_net_nfc_server_handover_snep_client_cb,
					context);

			net_nfc_util_free_data(&data);
		}
		else
		{
			DEBUG_ERR_MSG("_net_nfc_server_handover_convert"
					"_ndef_message_to_datafailed [%d]",result);
			if (context->cb != NULL)
			{
				context->cb(result,
						context->type,
						NULL,
						context->user_param);
			}
			_net_nfc_util_free_mem(context);
		}
	}
	else
	{
		DEBUG_ERR_MSG("null param, [%d]", result);
		if (context->cb != NULL)
		{
			context->cb(result,
					context->type,
					NULL,
					context->user_param);
		}
		_net_nfc_util_free_mem(context);
	}
}
#endif


#if 0

static void _net_nfc_server_handover_server_create_carrier_configs_cb(
		net_nfc_error_e result,
		ndef_message_s *selector,
		void *user_param)
{
	_net_nfc_server_handover_create_config_context_t *context =
		(_net_nfc_server_handover_create_config_context_t *)user_param;
	data_s data;

	if (context == NULL)
	{
		return;
	}

	if (result == NET_NFC_OK)
	{
		result = _net_nfc_server_handover_convert_ndef_message_to_data(
				selector,
				&data);

		if (result == NET_NFC_OK)
		{
			/* process message */
			net_nfc_server_handover_process_carrier_record(
					context->record,
					NULL,
					NULL);

			result = net_nfc_service_snep_server_send_get_response(
					context->user_param,
					&data);
			if (result != NET_NFC_OK)
			{
				DEBUG_ERR_MSG("net_nfc_service_snep_server"
						"_send_get_response failed [%d]",result);
			}
			net_nfc_util_free_data(&data);
		}
		else
		{
			DEBUG_ERR_MSG("_net_nfc_server_handover_convert_ndef_message_to_data"
					"failed [%d]",result);
		}
	}
	else
	{
		DEBUG_ERR_MSG("_net_nfc_server_handover_create_selector_msg"
				"failed [%d]",result);
	}

	_net_nfc_util_free_mem(context);
}

static net_nfc_error_e _net_nfc_server_handover_create_selector_msg(
		net_nfc_snep_handle_h handle,
		ndef_message_s *request,
		void *user_param)
{
	net_nfc_error_e result;
	uint32_t count;

	net_nfc_manager_util_play_sound(NET_NFC_TASK_END);

	/* get requester message */
	if ((result = net_nfc_util_get_alternative_carrier_record_count(
					request,
					&count)) == NET_NFC_OK)
	{
		if (1/* power state */ || count == 1)
		{
			ndef_record_s *record = NULL;

			/* fill alternative carrier information */
			if ((result =
						net_nfc_server_handover_get_carrier_record_by_priority_order(
							request,
							&record)) == NET_NFC_OK)
			{
				net_nfc_conn_handover_carrier_type_e type =
					NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;

				if ((result = net_nfc_util_get_alternative_carrier_type_from_record(
								record,
								&type)) == NET_NFC_OK)
				{
					_net_nfc_server_handover_create_config_context_t *context = NULL;

					_net_nfc_util_alloc_mem(context, sizeof(*context));

					context->user_param = handle;

					net_nfc_util_create_record(record->TNF,
							&record->type_s, &record->id_s,
							&record->payload_s,
							&context->record);

					if ((result = _net_nfc_server_handover_create_selector_carrier_configs(
									type,
									_net_nfc_server_handover_server_create_carrier_configs_cb,
									context)) != NET_NFC_OK)
					{
						DEBUG_ERR_MSG("_create_selector_carrier_configs "
								"failed [%d]", result);
					}
				}
				else
				{
					DEBUG_ERR_MSG("get_alternative_carrier_type_from_record "
							"failed [%d]", result);
				}
			}
			else
			{
				DEBUG_ERR_MSG("r_get_carrier_record_by_priority_order"
						" failed [%d]", result);
			}
		}
		else /* low power && count > 1 */
		{
			ndef_message_s selector;

			if ((result = _net_nfc_server_handover_create_low_power_selector_message(
							request,
							&selector)) == NET_NFC_OK)
			{
				_net_nfc_server_handover_server_create_carrier_configs_cb(
						NET_NFC_OK,
						&selector,
						user_param);

				net_nfc_util_free_ndef_message(&selector);
			}
			else
			{
				DEBUG_ERR_MSG("_create_low_power_selector_message"
						"failed [%d]", result);
			}
		}
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_util_get_alternative_carrier_record_count"
				"failed [%d]", result);
	}

	return result;
}


static net_nfc_error_e _net_nfc_server_handover_create_server_cb(
		net_nfc_snep_handle_h handle,
		net_nfc_error_e result,
		uint32_t type,
		data_s *data,
		void *user_param)
{
	DEBUG_SERVER_MSG("type [0x%02x], result [%d], data [%p], user_param [%p]",
			type, result, data, user_param);

	if (result != NET_NFC_OK || data == NULL || data->buffer == NULL)
	{
		/* restart */
		return NET_NFC_NULL_PARAMETER;
	}

	switch (type)
	{
	case SNEP_REQ_GET :
		{
			ndef_message_s *request;

			/* TODO : send select response to requester */
			result =
				_net_nfc_server_handover_create_requester_from_rawdata(
						&request,
						data);
			if (result == NET_NFC_OK) {
				if (1/* TODO : check version */)
				{
					_net_nfc_server_handover_create_selector_msg(
							handle,
							request,
							user_param);
				}
				else
				{
					DEBUG_ERR_MSG("not supported version [0x%x]",
							result);

					result = NET_NFC_NOT_SUPPORTED;
				}

				net_nfc_util_free_ndef_message(request);
			}
			else
			{
				DEBUG_ERR_MSG("_net_nfc_server_handover_create"
						"_requester_from_rawdata failed [%d]",result);
			}
		}
		break;

	case SNEP_REQ_PUT :
		DEBUG_ERR_MSG("PUT request doesn't supported");
		result = NET_NFC_NOT_SUPPORTED;
		break;

	default :
		DEBUG_ERR_MSG("error [%d]", result);
		break;
	}

	return result;
}
#else



static net_nfc_error_e _net_nfc_server_handover_select_carrier_record(
		ndef_message_s *request,
		net_nfc_conn_handover_carrier_type_e *type,
		ndef_record_s **record)
{
	net_nfc_error_e result;
	uint32_t count;

	*record = NULL;
	*type = NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;

	/* get requester message */
	if ((result = net_nfc_util_get_alternative_carrier_record_count(request,
					&count)) == NET_NFC_OK)
	{
		if (1/* power state */ || count == 1)
		{
			ndef_record_s *temp;

			/* fill alternative carrier information */
			if ((result =
						net_nfc_server_handover_get_carrier_record_by_priority_order(
							request,
							&temp)) == NET_NFC_OK)
			{
				if ((result =
							net_nfc_util_get_alternative_carrier_type_from_record(
								temp,
								type)) == NET_NFC_OK)
				{
					net_nfc_util_create_record(temp->TNF,
							&temp->type_s, &temp->id_s,
							&temp->payload_s,
							record);
				}
				else
				{
					DEBUG_ERR_MSG("net_nfc_util_get_alternative"
							"_carrier_type_from_record failed [%d]", result);
				}
			}
			else
			{
				DEBUG_ERR_MSG("_handover_get_carrier_record"
						"_by_priority_order failed [%d]", result);
			}
		}
		else /* low power && count > 1 */
		{
			result = NET_NFC_INVALID_STATE;
		}
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_util_get_alternative_carrier_record_count "
				"failed [%d]", result);
	}

	return result;
}

#endif

static void _net_nfc_server_handover_create_carrier_configs_2_cb(
		net_nfc_error_e result,
		ndef_message_s *selector,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	DEBUG_SERVER_MSG("_net_nfc_server_handover_server_create_carrier_config_cb"
			"result [%d]",result);

	if (context == NULL)
	{
		return;
	}

	context->result = result;

	if (result == NET_NFC_OK) {
		result = _net_nfc_server_handover_convert_ndef_message_to_data(
				selector,
				&context->data);

		DEBUG_SERVER_MSG("selector message created, length [%d]",
				context->data.length);

		context->state = NET_NFC_LLCP_STEP_03;
	}
	else
	{
		DEBUG_ERR_MSG("_net_nfc_server_handover_create_selector_msg failed [%d]",
				result);
		context->state = NET_NFC_MESSAGE_LLCP_ERROR;
	}

	_net_nfc_server_handover_get_response_process(context);
}

static void _net_nfc_server_handover_process_carrier_record_2_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	DEBUG_SERVER_MSG("_net_nfc_server_handover_server_process_carrier_record_cb"
			"result [%d]",result);

	context->result = result;
	if (result == NET_NFC_OK)
	{
		context->state = NET_NFC_LLCP_STEP_04;
	}
	else
	{
		context->state = NET_NFC_MESSAGE_LLCP_ERROR;
	}

	context->data.length = data->length;
	_net_nfc_util_alloc_mem(context->data.buffer, context->data.length);
	memcpy(context->data.buffer, data->buffer, context->data.length);


	_net_nfc_server_handover_server_process(context);
}

static void _net_nfc_server_handover_get_response_process(
		net_nfc_handover_context_t *context)
{
	net_nfc_error_e result;

	if (context == NULL)
	{
		return;
	}

	switch (context->state)
	{
	case NET_NFC_LLCP_STEP_02 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_02");

		result = _net_nfc_server_handover_create_selector_carrier_configs(
				context->type,
				_net_nfc_server_handover_create_carrier_configs_2_cb,
				context);

		if (result != NET_NFC_OK)
		{
			DEBUG_ERR_MSG("_net_nfc_server_handover_create_"
					"selector_carrier_config failed [%d]",result);
		}
		break;

	case NET_NFC_LLCP_STEP_03 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_03");

		result = net_nfc_server_handover_process_carrier_record(
				context->record,
				_net_nfc_server_handover_process_carrier_record_2_cb,
				context);

		if (result != NET_NFC_OK)
		{
			DEBUG_ERR_MSG("net_nfc_server_handover_process_carrier_record"
					"failed [%d]",result);
		}
		break;

	case NET_NFC_LLCP_STEP_04 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_04");

		/* response select message */
		result = net_nfc_server_snep_server_send_get_response(
				(net_nfc_snep_handle_h)context->handle,
				&context->data);
		break;

	case NET_NFC_STATE_ERROR :
		DEBUG_SERVER_MSG("NET_NFC_STATE_ERROR");
		break;

	default :
		DEBUG_ERR_MSG("NET_NFC_LLCP_STEP_??");
		/* TODO */
		break;
	}
}

static bool _net_nfc_server_handover_get_response_cb(
		net_nfc_snep_handle_h handle,
		uint32_t type,
		uint32_t max_len,
		data_s *data,
		void *user_param)
{
	net_nfc_error_e result;
	ndef_message_s *request;

	DEBUG_SERVER_MSG("type [%d], data [%p], user_param [%p]",
			type, data, user_param);

	if (data == NULL || data->buffer == NULL)
	{
		/* restart */
		return false;
	}

	/* TODO : send select response to requester */
	result = _net_nfc_server_handover_create_requester_from_rawdata(
			&request,
			data);

	if (result == NET_NFC_OK)
	{
		net_nfc_conn_handover_carrier_type_e type;
		ndef_record_s *record;

		if (_net_nfc_server_handover_select_carrier_record(
					request,
					&type,
					&record) == NET_NFC_OK)
		{
			net_nfc_handover_context_t *context = NULL;

			_net_nfc_util_alloc_mem(context, sizeof(*context));

			if (context != NULL)
			{
				context->handle = (void *)handle;
				context->type = type;
				context->user_param = user_param;
				context->state = NET_NFC_LLCP_STEP_02;

				net_nfc_util_create_record(record->TNF,
						&record->type_s,
						&record->id_s,
						&record->payload_s,
						&context->record);

				_net_nfc_server_handover_get_response_process(context);
			}
			else
			{
				DEBUG_ERR_MSG("_net_nfc_util_alloc_mem failed");
				result = NET_NFC_ALLOC_FAIL;
			}
		}
		else
		{
			/* low power */
		}

		net_nfc_util_free_ndef_message(request);
	}
	else
	{
		DEBUG_SERVER_MSG("it is not handover requester message, [%d]",
				result);
	}

	return (result == NET_NFC_OK);
}

////////////////////////////////////////////////////////////////////////////////
static void _net_nfc_server_handover_server_process_carrier_record_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	DEBUG_SERVER_MSG("_net_nfc_server_handover_server_process"
			"_carrier_record_cb result [%d]",result);

	context->result = result;
	if (result == NET_NFC_OK)
	{
		context->state = NET_NFC_LLCP_STEP_04;
	}
	else
	{
		context->state = NET_NFC_MESSAGE_LLCP_ERROR;
	}

	_net_nfc_server_handover_server_process(context);
}

static void _net_nfc_server_handover_server_create_carrier_config_cb(
		net_nfc_error_e result,
		ndef_message_s *selector,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	DEBUG_SERVER_MSG("_net_nfc_server_handover_server_create"
			"_carrier_config_cb,result [%d]",result);

	if (context == NULL)
	{
		return;
	}

	context->result = result;

	if (result == NET_NFC_OK)
	{
		result = _net_nfc_server_handover_convert_ndef_message_to_data(
				selector,
				&context->data);
		DEBUG_SERVER_MSG("selector message created, length [%d]",
				context->data.length);
		context->state = NET_NFC_LLCP_STEP_03;
	}
	else
	{
		DEBUG_ERR_MSG("_net_nfc_server_handover_create_selector_msg"
				" failed [%d]",	result);
		context->state = NET_NFC_MESSAGE_LLCP_ERROR;
	}

	_net_nfc_server_handover_server_process(context);
}

static void _net_nfc_server_handover_server_recv_cb(
		net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;
	ndef_message_s *request;

	DEBUG_SERVER_MSG("_net_nfc_server_handover_server_recv_cb,"
			" socket [%x], result [%d]", socket, result);

	context->result = result;

	if (result == NET_NFC_OK)
	{
		result = _net_nfc_server_handover_create_requester_from_rawdata(
				&request,
				data);

		if (result == NET_NFC_OK)
		{
			ndef_record_s *record;

			if (_net_nfc_server_handover_select_carrier_record(
						request,
						&context->type,
						&record) == NET_NFC_OK)
			{
				net_nfc_util_create_record(record->TNF,
						&record->type_s, &record->id_s,
						&record->payload_s,
						&context->record);

				context->state = NET_NFC_LLCP_STEP_02;
			}
			else
			{
				/* low power */
				context->state = NET_NFC_LLCP_STEP_06;
			}

			net_nfc_util_free_ndef_message(request);
		}
		else
		{
			DEBUG_ERR_MSG("_net_nfc_server_handover_create"
					"_requester_from_rawdata failed [%d]",result);
			context->state = NET_NFC_MESSAGE_LLCP_ERROR;
		}
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_server_llcp_simple_receive failed [%d]",
				result);
		context->state = NET_NFC_MESSAGE_LLCP_ERROR;
	}

	_net_nfc_server_handover_server_process(context);
}

static void _net_nfc_server_handover_server_send_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	DEBUG_SERVER_MSG("_net_nfc_server_handover_server_send_cb"
			" socket [%x], result [%d]", socket, result);

	context->result = result;

	if (result == NET_NFC_OK)
	{
		context->state = NET_NFC_LLCP_STEP_01;
		net_nfc_util_free_data(&context->data);
		net_nfc_util_free_record(context->record);
		context->record = NULL;
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_server_llcp_simple_send failed [%d]",
				result);
		context->state = NET_NFC_MESSAGE_LLCP_ERROR;
	}

	_net_nfc_server_handover_server_process(context);
}

static void _net_nfc_server_handover_server_process(
		net_nfc_handover_context_t *context)
{
	if (context == NULL)
	{
		return;
	}

	switch (context->state)
	{
	case NET_NFC_LLCP_STEP_01 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_01");

		/* receive request message */
		net_nfc_server_llcp_simple_receive(context->handle,
				context->socket,
				_net_nfc_server_handover_server_recv_cb,
				context);
		break;

	case NET_NFC_LLCP_STEP_02 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_02");

		context->result = _net_nfc_server_handover_create_selector_carrier_configs(
				context->type,
				_net_nfc_server_handover_server_create_carrier_config_cb,
				context);

		if (context->result != NET_NFC_OK)
		{
			DEBUG_ERR_MSG("_net_nfc_server_handover_create_selector"
					"_carrier_configs failed [%d]",	context->result);
		}
		break;

	case NET_NFC_LLCP_STEP_03 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_03");

		context->result = net_nfc_server_handover_process_carrier_record(
				context->record,
				_net_nfc_server_handover_server_process_carrier_record_cb,
				context);
		if (context->result != NET_NFC_OK)
		{
			DEBUG_ERR_MSG("_net_nfc_server_handover_process_carrier_"
					"record failed [%d]",context->result);
		}
		break;

	case NET_NFC_LLCP_STEP_04 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_04");

		/* send select message */
		net_nfc_server_llcp_simple_send(
				context->handle,
				context->socket,
				&context->data,
				_net_nfc_server_handover_server_send_cb,
				context);
		break;

	case NET_NFC_STATE_ERROR :
		DEBUG_SERVER_MSG("NET_NFC_STATE_ERROR");

		/* error, invoke callback */
		DEBUG_ERR_MSG("handover_server failed, [%d]",
				context->result);

		/* restart?? */
		break;

	default :
		DEBUG_ERR_MSG("NET_NFC_LLCP_STEP_??");
		/* TODO */
		break;
	}
}

static void _net_nfc_server_handover_server_error_cb(
		net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	DEBUG_ERR_MSG("result [%d], socket [%x], user_param [%p]",
			result, socket, user_param);

	if (context == NULL)
	{
		return;
	}

	net_nfc_controller_llcp_socket_close(socket, &result);

	net_nfc_util_free_record(context->record);
	net_nfc_util_free_data(&context->data);
	_net_nfc_util_free_mem(user_param);
}

static void _net_nfc_server_handover_server_incomming_cb(
		net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param)
{
	DEBUG_SERVER_MSG("result [%d], socket [%x], user_param [%p]",
			result, socket, user_param);

	net_nfc_handover_context_t *accept_context = NULL;

	_net_nfc_util_alloc_mem(accept_context, sizeof(*accept_context));
	if (accept_context == NULL)
	{
		DEBUG_ERR_MSG("_net_nfc_util_alloc_mem failed");

		result = NET_NFC_ALLOC_FAIL;
		goto ERROR;
	}

	accept_context->handle = handle;
	accept_context->socket = socket;
	accept_context->state = NET_NFC_LLCP_STEP_01;

	result = net_nfc_server_llcp_simple_accept(
			handle,
			socket,
			_net_nfc_server_handover_server_error_cb,
			accept_context);

	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("net_nfc_server_llcp_simple_accept failed, [%d]",
				result);

		goto ERROR;
	}

	_net_nfc_server_handover_server_process(accept_context);

	return;

ERROR :
	if (accept_context != NULL)
	{
		_net_nfc_util_free_mem(accept_context);
	}

	net_nfc_controller_llcp_socket_close(socket, &result);

	/* TODO : restart ?? */
}

net_nfc_error_e net_nfc_server_handover_default_server_start(
		net_nfc_target_handle_s *handle)
{
	net_nfc_error_e result;

	/* start default handover server using snep */
	result =
		net_nfc_server_snep_default_server_register_get_response_cb(
				_net_nfc_server_handover_get_response_cb,
				NULL);

	/* start default handover server */
	result = net_nfc_server_llcp_simple_server(
			handle,
			CH_SAN,
			CH_SAP,
			_net_nfc_server_handover_server_incomming_cb,
			_net_nfc_server_handover_server_error_cb,
			NULL);

	if (result == NET_NFC_OK)
	{
		DEBUG_SERVER_MSG("start handover server, san [%s], sap [%d]",
				CH_SAN,CH_SAP);
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_server_llcp_simple_server failed, [%d]",
				result);
	}

	return result;
}

static void _handover_default_activate_cb(int event,
		net_nfc_target_handle_s *handle, uint32_t sap, const char *san, void *user_param)
{
	net_nfc_error_e result;

	DEBUG_SERVER_MSG("event [%d], handle [%p], sap [%d], san [%s]",
			event, handle, sap, san);

	if (event == NET_NFC_LLCP_START) {
		/* start default handover server using snep */
		result = net_nfc_server_snep_default_server_register_get_response_cb(
				_net_nfc_server_handover_get_response_cb, NULL);

		/* start default handover server */
		result = net_nfc_server_llcp_simple_server(handle,
				CH_SAN, CH_SAP,
				_net_nfc_server_handover_server_incomming_cb,
				_net_nfc_server_handover_server_error_cb, NULL);

		if (result == NET_NFC_OK) {
			DEBUG_SERVER_MSG("start handover server, san [%s], sap [%d]",
					CH_SAN, CH_SAP);
		} else {
			DEBUG_ERR_MSG("net_nfc_service_llcp_server failed, [%d]",
					result);
		}
	} else if (event == NET_NFC_LLCP_UNREGISTERED) {
		/* unregister server, do nothing */
	}
}

net_nfc_error_e net_nfc_server_handover_default_server_register()
{
	char id[20];

	/* TODO : make id, */
	snprintf(id, sizeof(id), "%d", getpid());

	/* start default snep server */
	return net_nfc_server_llcp_register_service(id,
			CH_SAP,
			CH_SAN,
			_handover_default_activate_cb,
			NULL);
}

net_nfc_error_e net_nfc_server_handover_default_server_unregister()
{
	char id[20];

	/* TODO : make id, */
	snprintf(id, sizeof(id), "%d", getpid());

	/* start default snep server */
	return net_nfc_server_llcp_unregister_service(id,
			CH_SAP,
			CH_SAN);
}

////////////////////////////////////////////////////////////////////////////////

static void _net_nfc_server_handover_client_post_process_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	if (context == NULL)
	{
		return;
	}

	context->state = NET_NFC_LLCP_STEP_RETURN;

	_net_nfc_server_handover_client_process(context);
}

static void _net_nfc_server_handover_client_process_carrier_record_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	DEBUG_SERVER_MSG("_net_nfc_server_handover_server_process"
			"_carrier_record_cb,result [%d]",result);

	context->result = result;

	net_nfc_util_free_data(&context->data);

	if (result == NET_NFC_OK)
	{
		net_nfc_util_alloc_data(&context->data, data->length);
		memcpy(context->data.buffer, data->buffer, data->length);

		context->state = NET_NFC_LLCP_STEP_05;
	}
	else
	{
		context->state = NET_NFC_MESSAGE_LLCP_ERROR;
	}

	_net_nfc_server_handover_client_process(context);
}

static void _net_nfc_server_handover_client_recv_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	if (context == NULL)
	{
		return;
	}

	context->result = result;

	if (result == NET_NFC_OK)
	{
		ndef_message_s *selector;
		ndef_record_s *record;

		result = net_nfc_server_handover_create_selector_from_rawdata(
				&selector,
				data);

		if (result == NET_NFC_OK)
		{
			result =
				net_nfc_server_handover_get_carrier_record_by_priority_order(
						selector,
						&record);

			if (result == NET_NFC_OK)
			{
				net_nfc_util_create_record(
						record->TNF,
						&record->type_s, &record->id_s,
						&record->payload_s,
						&context->record);

				context->state = NET_NFC_LLCP_STEP_04;
			}
			else
			{
				DEBUG_ERR_MSG("_get_carrier_record_by_priority_order"
						" failed, [%d]",result);
				context->state = NET_NFC_STATE_ERROR;
			}
		}
		else
		{
			DEBUG_ERR_MSG("_net_nfc_server_handover_create"
					"_selector_from_rawdata failed, [%d]",result);
			context->state = NET_NFC_STATE_ERROR;
		}
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_server_llcp_simple_receive failed, [%d]",
				result);
		context->state = NET_NFC_STATE_ERROR;
	}

	_net_nfc_server_handover_client_process(context);
}

static void _net_nfc_server_handover_client_send_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	if (context == NULL)
	{
		return;
	}

	context->result = result;

	net_nfc_util_free_data(&context->data);

	if (result == NET_NFC_OK)
	{
		context->state = NET_NFC_LLCP_STEP_03;
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_server_llcp_simple_client failed, [%d]",
				result);
		context->state = NET_NFC_STATE_ERROR;
	}

	_net_nfc_server_handover_client_process(context);
}

static void _net_nfc_server_handover_client_create_carrier_configs_cb(
		net_nfc_error_e result,
		ndef_message_s *msg,
		void *user_param)
{
	net_nfc_handover_context_t *context =
		(net_nfc_handover_context_t *)user_param;

	if (context == NULL) {
		return;
	}

	context->result = result;

	if (msg != NULL)
	{
		if ((result =
					_net_nfc_server_handover_convert_ndef_message_to_data(
						msg,
						&context->data)) == NET_NFC_OK)
		{
			context->state = NET_NFC_LLCP_STEP_02;
		}
		else
		{
			DEBUG_ERR_MSG("_net_nfc_server_handover_convert_ndef_"
					"message_to_data failed [%d]",result);
			context->state = NET_NFC_STATE_ERROR;
			context->result = result;
		}
	}
	else
	{
		DEBUG_ERR_MSG("null param, [%d]", result);
		context->state = NET_NFC_STATE_ERROR;
		context->result = result;
	}

	_net_nfc_server_handover_client_process(context);
}


////////////////////////////////////////////////////////////////////////////////
static void _net_nfc_server_handover_client_process(
		net_nfc_handover_context_t *context)
{
	net_nfc_error_e result;

	if (context == NULL)
	{
		return;
	}

	switch (context->state)
	{
	case NET_NFC_LLCP_STEP_01 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_01");

		if ((result = _net_nfc_server_handover_create_requester_carrier_configs(
						context->type,
						_net_nfc_server_handover_client_create_carrier_configs_cb,
						context)) != NET_NFC_OK)
		{
			DEBUG_ERR_MSG("_net_nfc_server_handover_create_requester"
					"_carrier_configs failed [%d]",result);
		}
		break;

	case NET_NFC_LLCP_STEP_02 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_02");

		/* send request */
		net_nfc_server_llcp_simple_send(
				context->handle,
				context->socket,
				&context->data,
				_net_nfc_server_handover_client_send_cb,
				context);
		break;

	case NET_NFC_LLCP_STEP_03 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_03");

		/* receive response */
		net_nfc_server_llcp_simple_receive(context->handle,
				context->socket,
				_net_nfc_server_handover_client_recv_cb,
				context);
		break;

	case NET_NFC_LLCP_STEP_04 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_04");

		result = net_nfc_server_handover_process_carrier_record(
				context->record,
				_net_nfc_server_handover_client_process_carrier_record_cb,
				context);

		if (result != NET_NFC_OK)
		{
			DEBUG_ERR_MSG("net_nfc_server_handover_process_carrier_record failed [%d]",result);
		}
		break;

	case NET_NFC_LLCP_STEP_05 :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_05");

		/* start post process */
		if (context->type == NET_NFC_CONN_HANDOVER_CARRIER_BT)
		{
			net_nfc_server_handover_bt_post_process(
					&context->data,
					_net_nfc_server_handover_client_post_process_cb,
					context);
		}
		else
		{
			DEBUG_ERR_MSG("not supported...");
		}
		break;

	case NET_NFC_LLCP_STEP_RETURN :
		DEBUG_SERVER_MSG("NET_NFC_LLCP_STEP_RETURN");

		/* complete and invoke callback */
		_net_nfc_server_handover_send_response(
				context->result,
				context->type,
				&context->data,
				context->user_param);

		net_nfc_util_free_data(&context->data);
		net_nfc_util_free_record(context->record);
		_net_nfc_util_free_mem(context);
		break;

	case NET_NFC_STATE_ERROR :
	default :
		DEBUG_ERR_MSG("NET_NFC_STATE_ERROR");

		_net_nfc_server_handover_send_response(
				context->result,
				context->type,
				NULL,
				context->user_param);
		break;
	}
}



static void _net_nfc_server_handover_client_connected_cb(
		net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param)
{
	DEBUG_SERVER_MSG("result [%d], socket [%x], user_param [%p]",
			result, socket, user_param);

	if (result == NET_NFC_OK)
	{
		net_nfc_handover_context_t *context = NULL;
		_net_nfc_util_alloc_mem(context, sizeof(*context));

		if (context != NULL)
		{
			context->handle = handle;
			context->socket = socket;
			context->state = NET_NFC_LLCP_STEP_01;
			context->user_param = user_param;
			_net_nfc_server_handover_client_process(context);
		}
		else
		{
			DEBUG_ERR_MSG("_net_nfc_util_alloc_mem failed");
			result = NET_NFC_ALLOC_FAIL;
		}
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_server_llcp_simple_client"
				" failed, [%d]", result);
	}
}

static void _net_nfc_server_handover_client_error_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param)
{

	DEBUG_ERR_MSG("result [%d], socket [%x], user_param [%p]",
			result, socket, user_param);

	if (false)
	{
		_net_nfc_server_handover_send_response(
				result,
				NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN,
				NULL,
				user_param);
	}

	net_nfc_controller_llcp_socket_close(socket, &result);
}


net_nfc_error_e net_nfc_server_handover_default_client_start(
		net_nfc_target_handle_s *handle, void *user_data)
{
	net_nfc_error_e result;

	result = net_nfc_server_llcp_simple_client(
			handle,
			CH_SAN,
			CH_SAP,
			_net_nfc_server_handover_client_connected_cb,
			_net_nfc_server_handover_client_error_cb,
			user_data);

	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("net_nfc_server_llcp_simple_client"
				" failed, [%d]",result);
	}

	return result;
}
