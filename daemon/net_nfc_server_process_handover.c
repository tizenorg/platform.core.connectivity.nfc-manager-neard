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
//#include "net_nfc_server_handover_bt.h"

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

#define NET_NFC_CH_CONTEXT \
	net_nfc_target_handle_s *handle;\
	net_nfc_llcp_socket_t socket;\
	net_nfc_error_e result;\
	int step;\
	net_nfc_conn_handover_carrier_type_e type;\
	void *user_param;

typedef struct _net_nfc_server_handover_create_config_context_t
{
	net_nfc_target_handle_s *handle;
	net_nfc_llcp_socket_t socket;
	net_nfc_error_e result;
	int step;
	net_nfc_conn_handover_carrier_type_e type;
	void *user_param;
	// TODO: above value MUST be same with NET_NFC_CH_CONTEXT

	_net_nfc_server_handover_create_carrier_msg_cb cb;
	net_nfc_conn_handover_carrier_type_e current_type;
	ndef_message_s *ndef_message;
	ndef_message_s *requester; /* for low power selector */

	ndef_record_s *record;
}
net_nfc_server_handover_create_config_context_t;

typedef struct _net_nfc_server_handover_process_config_context_t
{
	net_nfc_target_handle_s *handle;
	net_nfc_llcp_socket_t socket;
	net_nfc_error_e result;
	int step;
	net_nfc_conn_handover_carrier_type_e type;
	void *user_param;
	// TODO: above value MUST be same with NET_NFC_CH_CONTEXT */

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

static void _net_nfc_server_handover_bss_get_carrier_record_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_state_e cps,
		ndef_record_s *carrier,
		uint32_t aux_data_count,
		ndef_record_s *aux_data,
		void *user_param);

static void _net_nfc_server_handover_bss_process_carrier_record_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
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
	HandoverRequestData *handover_data = user_param;

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
		ndef_message_s *msg, data_s *data)
{
	uint32_t length;
	net_nfc_error_e result;

	RETV_IF(NULL == msg, NET_NFC_INVALID_PARAM);
	RETV_IF(NULL == data, NET_NFC_INVALID_PARAM);

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

static void _net_nfc_server_handover_bss_get_carrier_record_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_state_e cps,
		ndef_record_s *carrier,
		uint32_t aux_data_count,
		ndef_record_s *aux_data,
		void *user_param)
{
	net_nfc_server_handover_create_config_context_t *context = user_param;

	/* append record to ndef message */
	if (NET_NFC_OK == result)
	{
		ndef_record_s *record;

		net_nfc_util_create_record(carrier->TNF, &carrier->type_s, &carrier->id_s,
				&carrier->payload_s, &record);

		result = net_nfc_util_append_carrier_config_record(context->ndef_message,
						record, cps);
		if (NET_NFC_OK == result)
		{
			NFC_DBG("net_nfc_util_append_carrier_config_record success");
		}
		else
		{
			NFC_ERR("net_nfc_util_append_carrier_config_record failed [%d]", result);
			net_nfc_util_free_record(record);
		}

		g_idle_add((GSourceFunc)_net_nfc_server_handover_iterate_carrier_configs_to_next,
				(gpointer)context);
	}

	/* don't free context */
}

static void _net_nfc_server_handover_bss_process_carrier_record_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_s *data,
		void *user_param)
{
	net_nfc_server_handover_process_config_context_t *context = user_param;

	if(context)
	{
		if (context->cb != NULL)
			context->cb(result, type, data, context->user_param);
		else
			NFC_ERR("Invalid Callback");

		_net_nfc_util_free_mem(context);
	}
}

net_nfc_error_e
net_nfc_server_handover_get_carrier_record_by_priority_order(
		ndef_message_s *request,
		ndef_record_s **record)
{
	net_nfc_error_e result;
	unsigned int carrier_count = 0;

	LOGD("[%s] START", __func__);

	RETV_IF(NULL == request, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == record, NET_NFC_NULL_PARAMETER);

	*record = NULL;

	result = net_nfc_util_get_alternative_carrier_record_count(request, &carrier_count);
	if (NET_NFC_OK == result)
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
				result = net_nfc_util_get_alternative_carrier_type(request, idx,
								&carrier_type);

				if ((NET_NFC_OK == result) && (carrier_type == priority))
				{
					NFC_DBG("selected carrier type = [%d]", carrier_type);
					net_nfc_util_get_carrier_config_record(request, idx, record);
					result = NET_NFC_OK;
					break;
				}
			}
		}
	}
	else
	{
		NFC_ERR("net_nfc_util_get_alternative_carrier_record_count failed");
	}

	LOGD("[%s] END", __func__);

	return result;
}

static net_nfc_error_e _net_nfc_server_handover_create_requester_from_rawdata(
		ndef_message_s **requestor, data_s *data)
{
	net_nfc_error_e result;

	if (NULL == requestor)
		return NET_NFC_NULL_PARAMETER;

	*requestor = NULL;

	result = net_nfc_util_create_ndef_message(requestor);
	if (result == NET_NFC_OK)
	{
		result = net_nfc_util_convert_rawdata_to_ndef_message(data, *requestor);

		if (result == NET_NFC_OK)
		{
			if (_net_nfc_server_handover_check_hr_record_validation(*requestor) == true)
			{
				result = NET_NFC_OK;
			}
			else
			{
				NFC_ERR("record is not valid or is not available");
				net_nfc_util_free_ndef_message(*requestor);
				result = NET_NFC_INVALID_PARAM;
			}
		}
		else
		{
			NFC_ERR("_net_nfc_ndef_rawdata_to_ndef failed [%d]",result);
		}
	} else {
		NFC_ERR("net_nfc_util_create_ndef_message failed [%d]", result);
	}

	return result;
}

net_nfc_error_e net_nfc_server_handover_create_selector_from_rawdata(
		ndef_message_s **selector, data_s *data)
{
	net_nfc_error_e result;

	if (NULL == selector)
		return NET_NFC_NULL_PARAMETER;

	*selector = NULL;

	if ((result = net_nfc_util_create_ndef_message(selector)) == NET_NFC_OK)
	{
		result = net_nfc_util_convert_rawdata_to_ndef_message(data, *selector);
		if (NET_NFC_OK == result)
		{
			/* if record is not Hs record, then */
			if (_net_nfc_server_handover_check_hs_record_validation(*selector) == true)
			{
				result = NET_NFC_OK;
			}
			else
			{
				NFC_ERR("record is not valid or is not available");
				net_nfc_util_free_ndef_message(*selector);
				result = NET_NFC_INVALID_PARAM;
			}
		}
		else
		{
			NFC_ERR("_net_nfc_ndef_rawdata_to_ndef failed [%d]",result);
		}
	}
	else
	{
		NFC_ERR("_net_nfc_util_create_ndef_message failed [%d]",result);
	}

	return result;
}

static bool _net_nfc_server_handover_check_hr_record_validation(
		ndef_message_s *message)
{
	ndef_record_s *rec;
	unsigned int count;
	net_nfc_error_e ret;

	LOGD("[%s] START", __func__);

	if (NULL == message)
		return false;

	rec = (ndef_record_s *)message->records;

	if (memcmp(rec->type_s.buffer, CH_REQ_RECORD_TYPE, rec->type_s.length) != 0)
	{
		NFC_ERR("This is not connection handover request message");
		goto ERROR;
	}

	if (rec->payload_s.buffer[0] != CH_VERSION)
	{
		NFC_ERR("connection handover version is not matched");
		goto ERROR;
	}

	ret = net_nfc_util_get_alternative_carrier_record_count(message, &count);
	if (ret != NET_NFC_OK || 0 == count)
	{
		NFC_ERR("there is no carrier reference");
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
	ndef_record_s *rec;
	unsigned int count;
	net_nfc_error_e ret;

	LOGD("[%s] START", __func__);

	if (NULL == message)
		return false;

	rec = (ndef_record_s *)message->records;

	if (memcmp(rec->type_s.buffer, CH_SEL_RECORD_TYPE, rec->type_s.length) != 0)
	{
		NFC_ERR("This is not connection handover request message");
		goto ERROR;
	}

	ret = net_nfc_util_get_alternative_carrier_record_count(message,&count);
	if (ret != NET_NFC_OK || 0 == count)
	{
		NFC_ERR("there is no carrrier reference");
		goto ERROR;
	}

	/*	check version */
	if (rec->payload_s.buffer[0] != CH_VERSION)
	{
		NFC_ERR("connection handover version is not matched");
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

	RETV_IF(NULL == context, 0);

	if (context->cb != NULL)
		context->cb(NET_NFC_OK, context->ndef_message, context->user_param);

	if (context->ndef_message != NULL)
		net_nfc_util_free_ndef_message(context->ndef_message);

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
			if (context->current_type < NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN)
				context->current_type++;
		}
		else
		{
			context->current_type = NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;
		}

		g_idle_add((GSourceFunc)_net_nfc_server_handover_iterate_create_carrier_configs,
				(gpointer)context);
	}
	else
	{
		NFC_ERR("context->result is error [%d]", context->result);

		g_idle_add((GSourceFunc)_net_nfc_server_handover_iterate_carrier_configs_step,
				(gpointer)context);
	}

	return 0;
}

static int _net_nfc_server_handover_iterate_create_carrier_configs(
		net_nfc_server_handover_create_config_context_t *context)
{
	LOGD("[%s:%d] START", __func__, __LINE__);
	net_nfc_error_e result = NET_NFC_OK;

	switch (context->current_type)
	{
/*
	case NET_NFC_CONN_HANDOVER_CARRIER_BT :
		NFC_DBG("[NET_NFC_CONN_HANDOVER_CARRIER_BT]");
		net_nfc_server_handover_bt_get_carrier_record(
				_net_nfc_server_handover_bt_get_carrier_record_cb, context);
		break;
*/
	case NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS :
		NFC_DBG("[NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS]");

#ifdef TARGET
		if(memcmp(context->ndef_message->records->type_s.buffer ,CH_SEL_RECORD_TYPE,
					context->ndef_message->records->type_s.length)==0)
		{
			result = net_nfc_server_handover_bss_wfd_get_carrier_record(
					_net_nfc_server_handover_bss_get_carrier_record_cb, context);
		}
		else
#endif
		{
			result = net_nfc_server_handover_bss_get_carrier_record(
					_net_nfc_server_handover_bss_get_carrier_record_cb, context);
		}
		NFC_DBG("[%d]",result);
		break;

		//	case NET_NFC_CONN_HANDOVER_CARRIER_WIFI_IBSS :
		//		NFC_DBG("[NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS]");
		//		g_idle_add(
		//		(GSourceFunc)_net_nfc_server_handover_append_wifi_carrier_config,
		//			context);
		//		break;

	case NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN :
		NFC_DBG("[NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN]");
		g_idle_add((GSourceFunc)_net_nfc_server_handover_iterate_carrier_configs_step,
				(gpointer)context);
		break;

	default :
		NFC_DBG("[unknown : %d]", context->current_type);
		g_idle_add((GSourceFunc)_net_nfc_server_handover_iterate_carrier_configs_step,
				(gpointer)context);
		break;
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return 0;
}

static net_nfc_error_e
_net_nfc_server_handover_create_requester_carrier_configs(
		net_nfc_conn_handover_carrier_type_e type, void *cb, void *user_param)
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
		net_nfc_util_create_handover_request_message(&context->ndef_message);

		/* append carrier record */
		g_idle_add((GSourceFunc)_net_nfc_server_handover_iterate_create_carrier_configs,
				(gpointer)context);
	}
	else
	{
		NFC_ERR("alloc failed");
		result = NET_NFC_ALLOC_FAIL;
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return result;
}

static net_nfc_error_e
_net_nfc_server_handover_create_selector_carrier_configs(
		net_nfc_conn_handover_carrier_type_e type, void *cb, void *user_param)
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
		net_nfc_util_create_handover_select_message(&context->ndef_message);

		/* append carrier record */
		g_idle_add((GSourceFunc)_net_nfc_server_handover_iterate_create_carrier_configs,
				(gpointer)context);
	}
	else
	{
		NFC_ERR("alloc failed");
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

		net_nfc_util_get_alternative_carrier_type_from_record(carrier, &type);

		context->type = type;
		context->user_param = user_param;
		context->cb = cb;
		context->step = NET_NFC_LLCP_STEP_01;

		/* process carrier record */
		switch (type)
		{
		/*
		case NET_NFC_CONN_HANDOVER_CARRIER_BT :
			NFC_DBG("[NET_NFC_CONN_HANDOVER_CARRIER_BT]");
			net_nfc_server_handover_bt_process_carrier_record(carrier,
					_net_nfc_server_handover_bt_process_carrier_record_cb, context);
			break;
		*/

		case NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS :
			NFC_DBG("[NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS]");
			net_nfc_server_handover_bss_process_carrier_record(carrier,
					_net_nfc_server_handover_bss_process_carrier_record_cb, context);
			break;

		case NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN :
			NFC_DBG("[NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN]");
			_net_nfc_util_free_mem(context);
			break;

		default :
			NFC_DBG("[unknown]");
			_net_nfc_util_free_mem(context);
			break;
		}
	}
	else
	{
		NFC_ERR("alloc failed");
		result = NET_NFC_ALLOC_FAIL;
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return result;
}

static net_nfc_error_e _net_nfc_server_handover_select_carrier_record(
		ndef_message_s *request,
		net_nfc_conn_handover_carrier_type_e *type,
		ndef_record_s **record)
{
	uint32_t count;
	net_nfc_error_e result;

	*record = NULL;
	*type = NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;

	/* get requester message */
	result = net_nfc_util_get_alternative_carrier_record_count(request, &count);
	if (NET_NFC_OK == result)
	{
		if (1/* power state */ || 1 == count)
		{
			ndef_record_s *temp;

			/* fill alternative carrier information */
			result = net_nfc_server_handover_get_carrier_record_by_priority_order(
							request, &temp);
			if (NET_NFC_OK == result)
			{
				result = net_nfc_util_get_alternative_carrier_type_from_record(temp, type);
				if (NET_NFC_OK == result)
				{
					net_nfc_util_create_record(temp->TNF, &temp->type_s, &temp->id_s,
							&temp->payload_s, record);
				}
				else
				{
					NFC_ERR("net_nfc_util_get_alternative"
							"_carrier_type_from_record failed [%d]", result);
				}
			}
			else
			{
				NFC_ERR("_handover_get_carrier_record_by_priority_order failed [%d]", result);
			}
		}
		else /* low power && count > 1 */
		{
			result = NET_NFC_INVALID_STATE;
		}
	}
	else
	{
		NFC_ERR("net_nfc_util_get_alternative_carrier_record_count failed [%d]", result);
	}

	return result;
}

static void _net_nfc_server_handover_create_carrier_configs_2_cb(
		net_nfc_error_e result, ndef_message_s *selector, void *user_param)
{
	net_nfc_handover_context_t *context = user_param;

	RET_IF(NULL == context);

	NFC_DBG("_net_nfc_server_handover_server_create_carrier_config_cb result [%d]",
			result);

	context->result = result;

	if (NET_NFC_OK == result)
	{
		result = _net_nfc_server_handover_convert_ndef_message_to_data(
				selector, &context->data);

		NFC_DBG("selector message created, length [%d]", context->data.length);

		context->state = NET_NFC_LLCP_STEP_03;
	}
	else
	{
		NFC_ERR("_net_nfc_server_handover_create_selector_msg failed [%d]", result);
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
	net_nfc_handover_context_t *context = (net_nfc_handover_context_t *)user_param;

	NFC_DBG("_net_nfc_server_handover_server_process_carrier_record_cb result [%d]",
			result);

	context->result = result;
	if (NET_NFC_OK == result)
		context->state = NET_NFC_LLCP_STEP_04;
	else
		context->state = NET_NFC_MESSAGE_LLCP_ERROR;

	context->data.length = data->length;
	_net_nfc_util_alloc_mem(context->data.buffer, context->data.length);
	memcpy(context->data.buffer, data->buffer, context->data.length);


	_net_nfc_server_handover_server_process(context);
}

static void _net_nfc_server_handover_get_response_process(
		net_nfc_handover_context_t *context)
{
	net_nfc_error_e result;

	RET_IF(NULL == context);

	switch (context->state)
	{
	case NET_NFC_LLCP_STEP_02 :
		NFC_DBG("NET_NFC_LLCP_STEP_02");

		result = _net_nfc_server_handover_create_selector_carrier_configs(
				context->type, _net_nfc_server_handover_create_carrier_configs_2_cb, context);

		if (result != NET_NFC_OK)
		{
			NFC_ERR("_net_nfc_server_handover_create_"
					"selector_carrier_config failed [%d]",result);
		}
		break;

	case NET_NFC_LLCP_STEP_03 :
		NFC_DBG("NET_NFC_LLCP_STEP_03");

		result = net_nfc_server_handover_process_carrier_record(
				context->record,
				_net_nfc_server_handover_process_carrier_record_2_cb,
				context);

		if (result != NET_NFC_OK)
		{
			NFC_ERR("net_nfc_server_handover_process_carrier_record failed [%d]",result);
		}
		break;

	case NET_NFC_LLCP_STEP_04 :
		NFC_DBG("NET_NFC_LLCP_STEP_04");

		/* response select message */
		// TODO: context->handle may is not valid type. we should find a right function or parameter
		result = net_nfc_server_snep_server_send_get_response(
				(net_nfc_snep_handle_h)context->handle, &context->data);
		break;

	case NET_NFC_STATE_ERROR :
		NFC_DBG("NET_NFC_STATE_ERROR");
		break;

	default :
		NFC_ERR("NET_NFC_LLCP_STEP_??");
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

	RETV_IF(NULL == data, false);
	RETV_IF(NULL == data->buffer, false);

	NFC_DBG("type [%d], data [%p], user_param [%p]", type, data, user_param);

	/* TODO : send select response to requester */
	result = _net_nfc_server_handover_create_requester_from_rawdata(&request, data);

	if (NET_NFC_OK == result)
	{
		ndef_record_s *record;
		net_nfc_conn_handover_carrier_type_e type;

		result = _net_nfc_server_handover_select_carrier_record(request, &type, &record);
		if (NET_NFC_OK == result)
		{
			net_nfc_handover_context_t *context = NULL;

			_net_nfc_util_alloc_mem(context, sizeof(*context));

			if (context != NULL)
			{
				context->handle = (void *)handle;
				context->type = type;
				context->user_param = user_param;
				context->state = NET_NFC_LLCP_STEP_02;

				net_nfc_util_create_record(record->TNF, &record->type_s, &record->id_s,
						&record->payload_s, &context->record);

				_net_nfc_server_handover_get_response_process(context);
			}
			else
			{
				NFC_ERR("_net_nfc_util_alloc_mem failed");
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
		NFC_ERR("it is not handover requester message, [%d]", result);
	}

	return (NET_NFC_OK == result);
}

////////////////////////////////////////////////////////////////////////////////
static void _net_nfc_server_handover_server_process_carrier_record_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context = user_param;

	NFC_DBG("_net_nfc_server_handover_server_process_carrier_record_cb result [%d]",
			result);

	context->result = result;

	if (NET_NFC_OK == result)
		context->state = NET_NFC_LLCP_STEP_04;
	else
		context->state = NET_NFC_MESSAGE_LLCP_ERROR;

	_net_nfc_server_handover_server_process(context);
}

static void _net_nfc_server_handover_server_create_carrier_config_cb(
		net_nfc_error_e result, ndef_message_s *selector, void *user_param)
{
	net_nfc_conn_handover_carrier_type_e type;
	net_nfc_handover_context_t *context = user_param;

	RET_IF(NULL == context);

	NFC_DBG("_net_nfc_server_handover_server_create_carrier_config_cb,result [%d]",
			result);

	context->result = result;

	net_nfc_util_get_alternative_carrier_type_from_record(context->record, &type);
	if (NET_NFC_OK == result)
	{
		result = _net_nfc_server_handover_convert_ndef_message_to_data(
				selector, &context->data);
		NFC_DBG("selector message created, length [%d]", context->data.length);
		if(type == NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS)
			context->state = NET_NFC_LLCP_STEP_04;
		else
			context->state = NET_NFC_LLCP_STEP_03;
	}
	else
	{
		NFC_ERR("_net_nfc_server_handover_create_selector_msg failed [%d]", result);
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
	net_nfc_error_e ret;
	ndef_message_s *request;
	net_nfc_handover_context_t *context = user_param;

	NFC_DBG("_net_nfc_server_handover_server_recv_cb, socket [%x], result [%d]",
			socket, result);

	context->result = result;

	if (NET_NFC_OK == result)
	{
		result = _net_nfc_server_handover_create_requester_from_rawdata(&request, data);

		if (NET_NFC_OK == result)
		{
			ndef_record_s *record;

			ret = _net_nfc_server_handover_select_carrier_record(request,
						&context->type, &record);

			if (NET_NFC_OK == ret)
			{
				net_nfc_util_create_record(record->TNF, &record->type_s, &record->id_s,
						&record->payload_s, &context->record);

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
			NFC_ERR("_net_nfc_server_handover_create"
					"_requester_from_rawdata failed [%d]",result);
			context->state = NET_NFC_MESSAGE_LLCP_ERROR;
		}
	}
	else
	{
		NFC_ERR("net_nfc_server_llcp_simple_receive failed [%d]", result);
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
	net_nfc_handover_context_t *context = user_param;

	NFC_DBG("_net_nfc_server_handover_server_send_cb socket[%x], result[%d]",
			socket, result);

	context->result = result;

	if (NET_NFC_OK == result)
	{
		context->state = NET_NFC_LLCP_STEP_01;
		net_nfc_util_free_data(&context->data);
		net_nfc_util_free_record(context->record);
		context->record = NULL;
	}
	else
	{
		NFC_ERR("net_nfc_server_llcp_simple_send failed [%d]", result);
		context->state = NET_NFC_MESSAGE_LLCP_ERROR;
	}

	_net_nfc_server_handover_server_process(context);
}

static void _net_nfc_server_handover_server_process(
		net_nfc_handover_context_t *context)
{
	RET_IF(NULL == context);

	switch (context->state)
	{
	case NET_NFC_LLCP_STEP_01 :
		NFC_DBG("NET_NFC_LLCP_STEP_01");

		/* receive request message */
		net_nfc_server_llcp_simple_receive(context->handle, context->socket,
				_net_nfc_server_handover_server_recv_cb, context);
		break;

	case NET_NFC_LLCP_STEP_02 :
		NFC_DBG("NET_NFC_LLCP_STEP_02");

		context->result = _net_nfc_server_handover_create_selector_carrier_configs(
				context->type,
				_net_nfc_server_handover_server_create_carrier_config_cb,
				context);

		if (context->result != NET_NFC_OK)
		{
			NFC_ERR("_net_nfc_server_handover_create_selector"
					"_carrier_configs failed [%d]", context->result);
		}
		break;

	case NET_NFC_LLCP_STEP_03 :
		NFC_DBG("NET_NFC_LLCP_STEP_03");

		context->result = net_nfc_server_handover_process_carrier_record(
				context->record,
				_net_nfc_server_handover_server_process_carrier_record_cb,
				context);

		if (context->result != NET_NFC_OK)
		{
			NFC_ERR("_net_nfc_server_handover_process_carrier_"
					"record failed [%d]",context->result);
		}
		break;

	case NET_NFC_LLCP_STEP_04 :
		NFC_DBG("NET_NFC_LLCP_STEP_04");

		/* send select message */
		net_nfc_server_llcp_simple_send(context->handle, context->socket, &context->data,
				_net_nfc_server_handover_server_send_cb, context);
		break;

	case NET_NFC_STATE_ERROR :
		NFC_DBG("NET_NFC_STATE_ERROR");

		/* error, invoke callback */
		NFC_ERR("handover_server failed, [%d]", context->result);

		/* restart?? */
		break;

	default :
		NFC_ERR("NET_NFC_LLCP_STEP_??");
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
	net_nfc_handover_context_t *context = user_param;

	NFC_ERR("result [%d], socket [%x], user_param [%p]", result, socket, user_param);

	RET_IF(NULL == context);

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
	net_nfc_handover_context_t *accept_context = NULL;

	NFC_DBG("result[%d], socket[%x], user_param[%p]", result, socket, user_param);

	_net_nfc_util_alloc_mem(accept_context, sizeof(*accept_context));
	if (NULL == accept_context)
	{
		NFC_ERR("_net_nfc_util_alloc_mem failed");

		result = NET_NFC_ALLOC_FAIL;
		goto ERROR;
	}

	accept_context->handle = handle;
	accept_context->socket = socket;
	accept_context->state = NET_NFC_LLCP_STEP_01;

	result = net_nfc_server_llcp_simple_accept(handle, socket,
			_net_nfc_server_handover_server_error_cb, accept_context);

	if (result != NET_NFC_OK)
	{
		NFC_ERR("net_nfc_server_llcp_simple_accept failed, [%d]", result);

		goto ERROR;
	}

	_net_nfc_server_handover_server_process(accept_context);

	return;

ERROR :
	if (accept_context != NULL)
		_net_nfc_util_free_mem(accept_context);

	net_nfc_controller_llcp_socket_close(socket, &result);

	/* TODO : restart ?? */
}

net_nfc_error_e net_nfc_server_handover_default_server_start(
		net_nfc_target_handle_s *handle)
{
	net_nfc_error_e result;

	/* start default handover server using snep */
	result = net_nfc_server_snep_default_server_register_get_response_cb(
					_net_nfc_server_handover_get_response_cb, NULL);

	/* start default handover server */
	result = net_nfc_server_llcp_simple_server(
			handle,
			CH_SAN,
			CH_SAP,
			_net_nfc_server_handover_server_incomming_cb,
			_net_nfc_server_handover_server_error_cb,
			NULL);

	if (NET_NFC_OK == result)
		NFC_DBG("start handover server, san[%s], sap[%d]", CH_SAN, CH_SAP);
	else
		NFC_ERR("net_nfc_server_llcp_simple_server failed, [%d]", result);

	return result;
}

static void _handover_default_activate_cb(int event,
		net_nfc_target_handle_s *handle,uint32_t sap, const char *san, void *user_param)
{
	net_nfc_error_e result;

	NFC_DBG("event [%d], handle [%p], sap [%d], san [%s]", event, handle, sap, san);

	if (NET_NFC_LLCP_START == event)
	{
		/* start default handover server using snep */
		result = net_nfc_server_snep_default_server_register_get_response_cb(
				_net_nfc_server_handover_get_response_cb, NULL);

		/* start default handover server */
		result = net_nfc_server_llcp_simple_server(handle,
				CH_SAN, CH_SAP,
				_net_nfc_server_handover_server_incomming_cb,
				_net_nfc_server_handover_server_error_cb, NULL);

		if (NET_NFC_OK == result)
			NFC_DBG("start handover server, san [%s], sap [%d]", CH_SAN, CH_SAP);
		else
			NFC_ERR("net_nfc_service_llcp_server failed, [%d]", result);

	}
	else if (NET_NFC_LLCP_UNREGISTERED == event)
	{
		/* unregister server, do nothing */
	}
}

net_nfc_error_e net_nfc_server_handover_default_server_register()
{
	char id[20];

	/* TODO : make id, */
	snprintf(id, sizeof(id), "%d", getpid());

	/* start default snep server */
	return net_nfc_server_llcp_register_service(id, CH_SAP, CH_SAN,
			_handover_default_activate_cb, NULL);
}

net_nfc_error_e net_nfc_server_handover_default_server_unregister()
{
	char id[20];

	/* TODO : make id, */
	snprintf(id, sizeof(id), "%d", getpid());

	/* start default snep server */
	return net_nfc_server_llcp_unregister_service(id, CH_SAP, CH_SAN);
}

////////////////////////////////////////////////////////////////////////////////

static void _net_nfc_server_handover_client_process_carrier_record_cb(
		net_nfc_error_e result,
		net_nfc_conn_handover_carrier_type_e type,
		data_s *data,
		void *user_param)
{
	net_nfc_handover_context_t *context = user_param;

	NFC_DBG("_net_nfc_server_handover_server_process_carrier_record_cb, result[%d]",
			result);

	context->result = result;

	net_nfc_util_free_data(&context->data);

	if (NET_NFC_OK == result)
	{
		if(context->type == NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS)
		{
			context->state = NET_NFC_LLCP_STEP_RETURN;
		}
		else
		{
			net_nfc_util_alloc_data(&context->data, data->length);
			memcpy(context->data.buffer, data->buffer, data->length);

			context->state = NET_NFC_LLCP_STEP_05;
		}
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
	net_nfc_handover_context_t *context = user_param;

	RET_IF(NULL == context);

	context->result = result;

	if (NET_NFC_OK == result)
	{
		ndef_record_s *record;
		ndef_message_s *selector;

		result = net_nfc_server_handover_create_selector_from_rawdata(&selector, data);

		if (NET_NFC_OK == result)
		{
			result = net_nfc_server_handover_get_carrier_record_by_priority_order(
						selector, &record);

			if (NET_NFC_OK == result)
			{
				net_nfc_util_create_record(record->TNF, &record->type_s, &record->id_s,
						&record->payload_s, &context->record);

				context->state = NET_NFC_LLCP_STEP_04;
			}
			else
			{
				NFC_ERR("_get_carrier_record_by_priority_order failed, [%d]",result);
				context->state = NET_NFC_STATE_ERROR;
			}
		}
		else
		{
			NFC_ERR("_net_nfc_server_handover_create"
					"_selector_from_rawdata failed, [%d]",result);
			context->state = NET_NFC_STATE_ERROR;
		}
	}
	else
	{
		NFC_ERR("net_nfc_server_llcp_simple_receive failed, [%d]", result);
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
	net_nfc_handover_context_t *context = user_param;

	RET_IF(NULL == context);

	context->result = result;

	net_nfc_util_free_data(&context->data);

	if (NET_NFC_OK == result)
	{
		context->state = NET_NFC_LLCP_STEP_03;
	}
	else
	{
		NFC_ERR("net_nfc_server_llcp_simple_client failed, [%d]", result);
		context->state = NET_NFC_STATE_ERROR;
	}

	_net_nfc_server_handover_client_process(context);
}

static void _net_nfc_server_handover_client_create_carrier_configs_cb(
		net_nfc_error_e result, ndef_message_s *msg, void *user_param)
{
	net_nfc_handover_context_t *context = user_param;

	RET_IF(NULL == context);

	context->result = result;

	if (msg != NULL)
	{
		result = _net_nfc_server_handover_convert_ndef_message_to_data(msg, &context->data);
		if (NET_NFC_OK == result)
		{
			context->state = NET_NFC_LLCP_STEP_02;
		}
		else
		{
			NFC_ERR("_net_nfc_server_handover_convert_ndef_"
					"message_to_data failed [%d]",result);
			context->state = NET_NFC_STATE_ERROR;
			context->result = result;
		}
	}
	else
	{
		NFC_ERR("null param, [%d]", result);
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

	RET_IF(NULL == context);

	switch (context->state)
	{
	case NET_NFC_LLCP_STEP_01 :
		NFC_DBG("NET_NFC_LLCP_STEP_01");

		result = _net_nfc_server_handover_create_requester_carrier_configs(
						context->type,
						_net_nfc_server_handover_client_create_carrier_configs_cb,
						context);
		if (result != NET_NFC_OK)
		{
			NFC_ERR("_net_nfc_server_handover_create_requester"
					"_carrier_configs failed [%d]",result);
		}
		break;

	case NET_NFC_LLCP_STEP_02 :
		NFC_DBG("NET_NFC_LLCP_STEP_02");

		/* send request */
		net_nfc_server_llcp_simple_send(context->handle, context->socket, &context->data,
				_net_nfc_server_handover_client_send_cb, context);
		break;

	case NET_NFC_LLCP_STEP_03 :
		NFC_DBG("NET_NFC_LLCP_STEP_03");

		/* receive response */
		net_nfc_server_llcp_simple_receive(context->handle, context->socket,
				_net_nfc_server_handover_client_recv_cb, context);
		break;

	case NET_NFC_LLCP_STEP_04 :
		NFC_DBG("NET_NFC_LLCP_STEP_04");

		result = net_nfc_server_handover_process_carrier_record(context->record,
				_net_nfc_server_handover_client_process_carrier_record_cb, context);

		if (result != NET_NFC_OK)
			NFC_ERR("net_nfc_server_handover_process_carrier_record failed [%d]",result);
		break;

	case NET_NFC_LLCP_STEP_05 :
		NFC_DBG("NET_NFC_LLCP_STEP_05");

		/* start post process */
		if (NET_NFC_CONN_HANDOVER_CARRIER_BT == context->type)
		{
			/*
			net_nfc_server_handover_bt_post_process(&context->data,
					_net_nfc_server_handover_client_post_process_cb, context);
			*/
		}
		else
		{
			NFC_ERR("not supported...");
		}
		break;

	case NET_NFC_LLCP_STEP_RETURN :
		NFC_DBG("NET_NFC_LLCP_STEP_RETURN");

		/* complete and invoke callback */
		_net_nfc_server_handover_send_response(context->result, context->type,
				&context->data, context->user_param);

		net_nfc_util_free_data(&context->data);
		net_nfc_util_free_record(context->record);
		_net_nfc_util_free_mem(context);
		break;

	case NET_NFC_STATE_ERROR :
	default :
		NFC_ERR("NET_NFC_STATE_ERROR");

		_net_nfc_server_handover_send_response(context->result, context->type,
				NULL, context->user_param);
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
	NFC_DBG("result [%d], socket [%x], user_param [%p]", result, socket, user_param);

	HandoverRequestData *handover_data = NULL;
	handover_data = (HandoverRequestData *)user_param;
	if (NET_NFC_OK == result)
	{
		net_nfc_handover_context_t *context = NULL;
		_net_nfc_util_alloc_mem(context, sizeof(*context));

		if (context != NULL)
		{
			context->handle = handle;
			context->socket = socket;
			context->state = NET_NFC_LLCP_STEP_01;
			context->type = handover_data->type;
			context->user_param = user_param;
			_net_nfc_server_handover_client_process(context);
		}
		else
		{
			NFC_ERR("_net_nfc_util_alloc_mem failed");
			result = NET_NFC_ALLOC_FAIL;
		}
	}
	else
	{
		NFC_ERR("net_nfc_server_llcp_simple_client"
				" failed, [%d]", result);
	}
}

static void _net_nfc_server_handover_client_error_cb(
		net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param)
{
	NFC_ERR("result [%d], socket [%x], user_param [%p]", result, socket, user_param);

	if (false)
	{
		_net_nfc_server_handover_send_response(result,
				NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN, NULL, user_param);
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
		NFC_ERR("net_nfc_server_llcp_simple_client failed, [%d]",result);

	return result;
}
