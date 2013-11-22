/*
 * Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <glib.h>
#include "net_nfc_debug_internal.h"
#include "net_nfc_server_tag.h"
#include "net_nfc_util_defines.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_gdbus_internal.h"
#include "net_nfc_server_llcp.h"
#include "net_nfc_server_util.h"
#include "net_nfc_server_process_phdc.h"
#include "net_nfc_server_controller.h"
#include "net_nfc_server_phdc.h"



typedef struct _net_nfc_server_phdc_pdu_t
{
	uint8_t data[128];
}
__attribute__ ((packed)) net_nfc_server_phdc_pdu_t;

//callback for internal send/receive
typedef void (*_net_nfc_server_phdc_operation_cb)(
		net_nfc_error_e result,
		data_s *data,
		void *user_param);

static void _net_nfc_server_phdc_agent_connected_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param);

typedef struct _net_nfc_server_phdc_op_context_t
{
	net_nfc_target_handle_s *handle;
	net_nfc_error_e result;
	int socket;
	uint32_t state;
	uint32_t current;
	uint16_t miu;
	data_s data;
	_net_nfc_server_phdc_operation_cb cb;
	void *user_param;
}net_nfc_server_phdc_op_context_t;


typedef struct _net_nfc_server_phdc_job_t
{
	net_nfc_server_phdc_context_t *context;
	net_nfc_target_handle_s *handle;
	net_nfc_error_e result;
	net_nfc_llcp_socket_t socket;
	uint32_t state;
	data_s data;
	net_nfc_server_phdc_send_cb cb;
	void *user_param;
}net_nfc_server_phdc_job_t;

static void _net_nfc_server_phdc_agent_process(
		net_nfc_server_phdc_job_t *job);

static void _net_nfc_server_phdc_destory_context(
		net_nfc_server_phdc_op_context_t *context);

static void _net_nfc_server_phdc_clear_queue(
		gpointer data,
		gpointer user_data);

static void _net_nfc_server_phdc_socket_error_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param);

static void _net_nfc_server_phdc_recv_cb(
		net_nfc_llcp_socket_t socket,
		net_nfc_error_e result,
		data_s *data,
		void *extra,
		void *user_param);

static void _net_nfc_server_phdc_recv(
		net_nfc_server_phdc_op_context_t *context);

static net_nfc_server_phdc_op_context_t* _net_nfc_server_phdc_create_recv_context(
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		void *cb,
		void *user_param);

static void _net_nfc_server_phdc_manager_op_cb(
		net_nfc_error_e result,
		data_s *data,
		void *user_param);


static net_nfc_error_e net_nfc_server_phdc_recv(
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		_net_nfc_server_phdc_operation_cb cb,
		void *user_param);

static void _net_nfc_server_phdc_manager_process(
		net_nfc_server_phdc_context_t *context);

static void _net_nfc_server_phdc_incoming_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket,
		data_s *data,
		void *user_param);


static void _net_nfc_server_phdc_destory_context(net_nfc_server_phdc_op_context_t *context)
{
	if (context != NULL)
	{
		if (context->data.buffer != NULL)
			net_nfc_util_free_data(&context->data);

		_net_nfc_util_free_mem(context);
	}
}

static void _net_nfc_server_phdc_clear_queue(gpointer data,gpointer user_data)
{
	net_nfc_server_phdc_job_t *job = (net_nfc_server_phdc_job_t *)data;

	if (job != NULL)
	{
		if (job->cb != NULL)
		{
			job->cb((net_nfc_phdc_handle_h)job->context,NET_NFC_OPERATION_FAIL,
				NET_NFC_PHDC_OPERATION_FAILED, job->user_param);
		}

		if (job->data.buffer != NULL)
			net_nfc_util_free_data(&job->data);

		_net_nfc_util_free_mem(job);
	}
}

static void _net_nfc_server_phdc_socket_error_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle, net_nfc_llcp_socket_t socket, data_s *data,
		void *user_param)
{
	net_nfc_server_phdc_context_t *context = (net_nfc_server_phdc_context_t *)user_param;

	NFC_DBG("_net_nfc_server_phdc_socket_error_cb socket [%x], result [%d]",
			socket, result);

	RET_IF(NULL == context);

	if (context->data.buffer != NULL)
		net_nfc_util_free_data(&context->data);

	g_queue_foreach(&context->queue, _net_nfc_server_phdc_clear_queue, NULL);

	_net_nfc_util_free_mem(context);
}


static void _net_nfc_server_phdc_recv_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	net_nfc_server_phdc_op_context_t *context =
		(net_nfc_server_phdc_op_context_t *)user_param;

	uint8_t *buffer = NULL;
	uint16_t pdu_length = 0;

	NFC_DBG("_net_nfc_server_phdc_recv_cb, socket[%x], result[%d]", socket, result);

	RET_IF(NULL == context);

	context->result = result;

	if (result != NET_NFC_OK)
	{
		NFC_ERR("error [%d]", result);
		context->state = NET_NFC_STATE_ERROR;
		goto END;
	}

	if (NULL == data || NULL == data->buffer || 0 == data->length)
	{
		NFC_ERR("invalid response");
		context->state = NET_NFC_STATE_ERROR;
		goto END;
	}

	//first 2 bytes is phdc header length
	memcpy(&pdu_length,data->buffer,PHDC_HEADER_LEN);

	pdu_length = ntohs(pdu_length);
	NFC_INFO("pdu_legth [%d]", pdu_length);

	if(pdu_length > 0)
	{
		/* buffer create */
		net_nfc_util_alloc_data(&context->data,pdu_length);
		if (NULL == context->data.buffer)
		{
			NFC_ERR("net_nfc_util_alloc_data failed");
			context->result = NET_NFC_ALLOC_FAIL;
			goto END;
		}
		buffer =data->buffer;
		context->data.length = data->length - PHDC_HEADER_LEN;

		/* copy data */
		if(context->data.length > 0)
		{
			memcpy(context->data.buffer ,buffer, context->data.length);
			NFC_DBG("receive progress... [%d", context->data.length);
		}
	}
END:
	context->cb(context->result, &context->data, context->user_param);
}


static void _net_nfc_server_phdc_recv(net_nfc_server_phdc_op_context_t *context)
{
	RET_IF(NULL == context);

	bool ret;
	net_nfc_error_e result;

	NFC_DBG("socket [%x] waiting data.....", context->socket);

	ret = net_nfc_controller_llcp_recv(context->handle, context->socket, context->miu,
			&result, _net_nfc_server_phdc_recv_cb, context);

	if(false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_recv failed, [%d]", result);

		context->result = result;
		context->cb(context->result, &context->data, context->user_param);

		_net_nfc_server_phdc_destory_context(context);
	}

}


static net_nfc_server_phdc_op_context_t* _net_nfc_server_phdc_create_recv_context(
		net_nfc_target_handle_s *handle, net_nfc_llcp_socket_t socket, void *cb,
		void *user_param)
{

	bool ret;
	net_nfc_error_e result;
	net_nfc_server_phdc_op_context_t *context = NULL;
	net_nfc_llcp_config_info_s config;

	ret = net_nfc_controller_llcp_get_remote_config(handle, &config, &result);

	if(false == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_get_remote_config failed, [%d]", 	result);
		return NULL;
	}

	_net_nfc_util_alloc_mem(context, sizeof(*context));

	RETV_IF(NULL == context, NULL);

	context->handle = handle;
	context->state = NET_NFC_LLCP_STEP_01;
	context->socket = socket;
	context->cb = cb;
	context->user_param = user_param;
	context->miu = MIN(config.miu, net_nfc_server_llcp_get_miu());

	return context;
}


static net_nfc_error_e net_nfc_server_phdc_recv(net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket, _net_nfc_server_phdc_operation_cb cb,
		void *user_param)
{
	net_nfc_error_e result;
	net_nfc_server_phdc_op_context_t *context = NULL;

	/* create context */
	context = _net_nfc_server_phdc_create_recv_context(handle, socket, cb, user_param);

	if (context != NULL)
	{
		_net_nfc_server_phdc_recv(context);
		result = NET_NFC_OK;
	}
	else
	{
		result = NET_NFC_ALLOC_FAIL;

		if (cb != NULL)
			cb(result, NULL, user_param);
	}

	return result;
}


static void _net_nfc_server_phdc_agent_connected_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle, net_nfc_llcp_socket_t socket, data_s *data,
		void *user_param)
{
	net_nfc_server_phdc_context_t *context =
		(net_nfc_server_phdc_context_t *)user_param;

	RET_IF(NULL == context);

	context->socket = socket;

	if (NET_NFC_OK == result)
		NFC_DBG("socket [%x] connected. send message", socket);
	else
		NFC_ERR("connect socket failed, [%d]", result);

	net_nfc_server_phdc_recv(handle, socket,
		_net_nfc_server_phdc_manager_op_cb,
		context);

	if (context->cb != NULL)
		context->cb((net_nfc_phdc_handle_h)context, result,
			NET_NFC_PHDC_TARGET_CONNECTED, NULL);

}

net_nfc_error_e net_nfc_server_phdc_agent_connect(net_nfc_target_handle_s *handle,
		const char *san, sap_t sap, 	net_nfc_server_phdc_cb cb, void *user_param)
{
	bool ret;
	net_nfc_error_e result;
	net_nfc_server_phdc_context_t *context = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);

	ret = net_nfc_server_target_connected(handle);

	if(FALSE == ret)
		return NET_NFC_NOT_CONNECTED;

	_net_nfc_util_alloc_mem(context, sizeof(*context));

	if (NULL == context)
	{
		NFC_ERR("_net_nfc_util_alloc_mem failed");
		result = NET_NFC_ALLOC_FAIL;
		goto ERROR;
	}

	context->handle = handle;
	context->cb = cb;
	context->user_param = user_param;

	result = net_nfc_server_llcp_simple_client(handle, san, 	sap,
			_net_nfc_server_phdc_agent_connected_cb,
			_net_nfc_server_phdc_socket_error_cb,
			context);

	if (result != NET_NFC_OK)
	{
		NFC_ERR("net_nfc_server_llcp_simple_client failed, [%d]", result);
		goto ERROR;
	}

	if (san != NULL)
		NFC_DBG("start phdc agent san [%s]", san);
	else
		NFC_DBG("start phdc agent, sap [%d]", sap);

	return result;

ERROR :
	if (context != NULL)
	{
		if (context->data.buffer != NULL)
		{
			net_nfc_util_free_data(&context->data);
		}
		_net_nfc_util_free_mem(context);
	}

	return result;
}


static void _net_nfc_server_phdc_agent_do_job(
		net_nfc_server_phdc_context_t *context)
{
	if (context->state == NET_NFC_LLCP_IDLE && g_queue_is_empty(&context->queue) == false)
	{
		net_nfc_server_phdc_job_t *job;

		job = g_queue_pop_head(&context->queue);
		if (job != NULL)
		{
			context->state = NET_NFC_LLCP_STEP_01;
			job->state = NET_NFC_LLCP_STEP_01;
			_net_nfc_server_phdc_agent_process(job);
		}
	}
}

static net_nfc_server_phdc_op_context_t* _net_nfc_server_phdc_create_send_context(
		net_nfc_target_handle_s *handle, net_nfc_llcp_socket_t socket, data_s *data,
		void *cb, void *user_param)
{
	bool ret;
	uint32_t data_len = 0;
	uint16_t length;
	uint8_t *buffer = NULL;
	net_nfc_server_phdc_op_context_t *context = NULL;
	net_nfc_llcp_config_info_s config;
	net_nfc_error_e result;

	ret = net_nfc_controller_llcp_get_remote_config(handle,
			&config, &result);

	if(FALSE == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_get_remote_config failed, [%d]",	result);
		return NULL;
	}

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	RETV_IF(NULL == context, NULL);

	if (data != NULL)
		data_len =data->length;

	if (data_len > 0)
	{
		net_nfc_util_alloc_data(&context->data,data_len+PHDC_HEADER_LEN);
		if (NULL == context->data.buffer)
		{
			_net_nfc_util_free_mem(context);
			return NULL;
		}

		length = (uint16_t)data->length;
		length +=PHDC_HEADER_LEN;
		buffer = context->data.buffer;

		if(data->buffer != NULL)
		{
			uint16_t network_length = htons(length);
			memcpy(buffer,&network_length,PHDC_HEADER_LEN);
			buffer+=PHDC_HEADER_LEN;
			memcpy(buffer,data->buffer,data->length);
			context->data.length = length;
		}
	}

	context->handle = handle;
	context->socket = socket;
	context->cb = cb;
	context->user_param = user_param;
	context->miu = MIN(config.miu, net_nfc_server_llcp_get_miu());

	return context;
}

static void _net_nfc_server_phdc_send_cb(net_nfc_llcp_socket_t socket,
		net_nfc_error_e result, data_s *data, void *extra, void *user_param)
{
	net_nfc_server_phdc_op_context_t *context =
		(net_nfc_server_phdc_op_context_t *)user_param;

	NFC_DBG("_net_nfc_server_phdc_send_cb, socket[%x], result[%d]", socket, result);

	RET_IF(NULL == context);

	context->result = result;

	/* complete and invoke callback */
	context->cb(context->result, NULL, context->user_param);

	_net_nfc_server_phdc_destory_context(context);
}


static void _net_nfc_server_phdc_send(net_nfc_server_phdc_op_context_t *context)
{
	bool ret;
	data_s req_msg;
	net_nfc_error_e result;

	RET_IF(NULL == context);

	req_msg.length = context->data.length;
	req_msg.buffer = context->data.buffer;

	NFC_DBG("try to send data, socket [%x]",context->socket);

	ret= net_nfc_controller_llcp_send(context->handle, context->socket,
			&req_msg, &result, _net_nfc_server_phdc_send_cb, context);

	if(FALSE == ret)
	{
		NFC_ERR("net_nfc_controller_llcp_send failed, [%d]",result);
		context->result = result;

		context->cb(context->result, NULL, context->user_param);
		_net_nfc_server_phdc_destory_context(context);
	}
}

static void _net_nfc_server_phdc_agent_send_cb(net_nfc_error_e result,data_s *data,
		void *user_param)
{
	net_nfc_server_phdc_job_t*job = (net_nfc_server_phdc_job_t *)user_param;

	RET_IF(NULL == job);

	job->result = result;

	if (NET_NFC_OK ==result)
	{
		job->state = NET_NFC_LLCP_STEP_RETURN;
		net_nfc_util_free_data(&job->data);
	}
	else
	{
		NFC_ERR("net_nfc_server_phdc_send failed, [%d]", result);
		job->state = NET_NFC_STATE_ERROR;
	}

	_net_nfc_server_phdc_agent_process(job);
}


net_nfc_error_e net_nfc_server_phdc_send(net_nfc_target_handle_s *handle,
		net_nfc_llcp_socket_t socket, data_s *data, _net_nfc_server_phdc_operation_cb cb,
		void *user_param)
{
	net_nfc_error_e result;
	net_nfc_server_phdc_op_context_t *context = NULL;

	/* create context */
	context = _net_nfc_server_phdc_create_send_context(handle, socket,data,
			cb, user_param);

	if (context != NULL)
	{
		/* send data */
		_net_nfc_server_phdc_send(context);
		result = NET_NFC_OK;
	}
	else
	{
		NFC_ERR("_net_nfc_server_phdc_create_send_context failed");
		result = NET_NFC_ALLOC_FAIL;

		if (cb != NULL)
			cb(result, NULL, user_param);
	}

	return result;
}

static void _net_nfc_server_phdc_agent_process(net_nfc_server_phdc_job_t *job)
{
	bool finish = false;

	RET_IF(NULL == job);

	switch (job->state)
	{
	case NET_NFC_LLCP_STEP_01 :
		NFC_DBG("NET_NFC_LLCP_STEP_01");

		/* send request */
		net_nfc_server_phdc_send(job->handle, job->socket, 	&job->data,
				_net_nfc_server_phdc_agent_send_cb, job);
		break;

	case NET_NFC_LLCP_STEP_RETURN :
		NFC_DBG("NET_NFC_LLCP_STEP_RETURN");

		/* complete and invoke callback */
		if (job->cb != NULL)
		{
			job->cb((net_nfc_phdc_handle_h)job->context, NET_NFC_OK,
				NET_NFC_PHDC_DATA_RECEIVED, job->user_param);
		}
		/* finish job */
		finish = true;
		break;

	case NET_NFC_STATE_ERROR :
	default :
		NFC_ERR("NET_NFC_STATE_ERROR");

		/* error, invoke callback */
		NFC_ERR("phdc_agent_send failed, [%d]", job->result);
		if (job->cb != NULL)
		{
			job->cb((net_nfc_phdc_handle_h)job->context, job->result,
				NET_NFC_PHDC_OPERATION_FAILED, job->user_param);
		}
		/* finish job */
		finish = TRUE;
		break;
	}

	if (TRUE == finish)
	{
		net_nfc_server_phdc_context_t *context = job->context;

		if (job->data.buffer != NULL)
			net_nfc_util_free_data(&job->data);

		_net_nfc_util_free_mem(job);
		context->state = NET_NFC_LLCP_IDLE;
		_net_nfc_server_phdc_agent_do_job(context);
	}
}


net_nfc_error_e net_nfc_server_phdc_agent_request(net_nfc_phdc_handle_h handle,
		data_s *data, net_nfc_server_phdc_send_cb cb, void *user_param)
{
	bool ret;
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_server_phdc_job_t *job = NULL;
	net_nfc_server_phdc_context_t *context = (net_nfc_server_phdc_context_t *)handle;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == data, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == data->buffer, NET_NFC_NULL_PARAMETER);

	ret =net_nfc_server_target_connected(context->handle);

	if(FALSE == ret)
		return NET_NFC_NOT_CONNECTED;

	/* check type */
	_net_nfc_util_alloc_mem(job, sizeof(*job));
	if (job != NULL)
	{
		net_nfc_util_alloc_data(&job->data, data->length);
		if (job->data.buffer != NULL)
			memcpy(job->data.buffer, data->buffer, data->length);

		job->cb = cb;
		job->user_param = user_param;
		job->context = context;
		job->handle = context->handle;
		job->socket = context->socket;
		g_queue_push_tail(&context->queue, job);
	}
	else
		return NET_NFC_ALLOC_FAIL;

	NFC_INFO("enqueued jobs [%d]", g_queue_get_length(&context->queue));

	/* if agent is idle, starts sending request */
	if (context->state == NET_NFC_LLCP_IDLE)
		_net_nfc_server_phdc_agent_do_job(context);
	else
		NFC_INFO("agent is working. this job will be enqueued");

	return result;
}


net_nfc_error_e net_nfc_server_phdc_agent_start(net_nfc_target_handle_s *handle,
		const char *san, sap_t sap, net_nfc_server_phdc_cb cb, void *user_param)
{
	net_nfc_error_e result;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == san, NET_NFC_NULL_PARAMETER);

		/* start PHDC Agent, register your callback */
		result =  net_nfc_server_phdc_agent_connect(handle, san, sap,
				cb, user_param);

		if (result != NET_NFC_OK)
		{
			NFC_ERR("net_nfc_server_phdc_agent_connect failed, [%d]",result);
		}
		return result;
}

static void _net_nfc_server_phdc_manager_op_cb( net_nfc_error_e result, data_s *data,
		void *user_param)
{
	net_nfc_server_phdc_context_t *context =
		(net_nfc_server_phdc_context_t *)user_param;

	NFC_DBG("_net_nfc_server_phdc_manager_op_cb result [%d]", result);

	RET_IF(NULL == context);

	context->result = result;
	if (NET_NFC_OK == result && data != NULL && data->buffer != NULL)
	{
		NFC_DBG("received message, length [%d]", data->length);

		net_nfc_util_alloc_data(&context->data, data->length);
		if (context->data.buffer != NULL)
		{
			memcpy(context->data.buffer,data->buffer, data->length);
			context->state = NET_NFC_LLCP_STEP_02;
		}
		else
		{
			NFC_ERR("net_nfc_util_alloc_data failed");
			context->state = NET_NFC_STATE_ERROR;
			context->result = NET_NFC_ALLOC_FAIL;
		}
	}
	else
	{
		NFC_ERR("net_nfc_server_phdc_recv failed, [%d]", result);
		context->state = NET_NFC_STATE_ERROR;
	}

	_net_nfc_server_phdc_manager_process(context);

}


static void _net_nfc_server_phdc_manager_process(
		net_nfc_server_phdc_context_t *context)
{
	bool finish = false;
	RET_IF(NULL == context);

	switch (context->state)
	{
	case NET_NFC_LLCP_STEP_01 :
		NFC_INFO("NET_NFC_LLCP_STEP_01 ");
		/*receive the Agent data*/
		net_nfc_server_phdc_recv(	context->handle,context->socket,
				_net_nfc_server_phdc_manager_op_cb, context);
		break;
	case NET_NFC_LLCP_STEP_02 :
		NFC_INFO("NET_NFC_LLCP_STEP_02");
		if (context->cb != NULL)
		{
			/* complete operation and invoke the callback*/
			context->cb((net_nfc_phdc_handle_h)context,NET_NFC_OK,
					NET_NFC_PHDC_DATA_RECEIVED, &context->data);
		}
		finish = true;
		break;
	case NET_NFC_STATE_ERROR :
		NFC_INFO("NET_NFC_STATE_ERROR");
	default :
		if (context->cb != NULL)
		{
			context->cb((net_nfc_phdc_handle_h)context,context->result,
					NET_NFC_PHDC_OPERATION_FAILED,&context->data);
		}
		return;
		break;
	}

	if(true == finish)
	{
		context->state = NET_NFC_LLCP_IDLE;

		net_nfc_server_phdc_recv(context->handle,
			context->socket,
			_net_nfc_server_phdc_manager_op_cb,
			context);

	}
}


static void _net_nfc_server_phdc_incoming_cb(net_nfc_error_e result,
		net_nfc_target_handle_s *handle, net_nfc_llcp_socket_t socket, data_s *data,
		void *user_param)
{
	net_nfc_server_phdc_context_t *context =
		(net_nfc_server_phdc_context_t *)user_param;

	net_nfc_server_phdc_context_t *accept_context = NULL;

	RET_IF(NULL == context);

	NFC_DBG("phdc incoming socket [%x], result [%d]", socket, result);

	if (result != NET_NFC_OK)
	{
		NFC_ERR("listen socket failed, [%d]", result);
		goto ERROR;
	}

	_net_nfc_util_alloc_mem(accept_context, sizeof(*accept_context));

	if (NULL == accept_context)
	{
		NFC_ERR("_net_nfc_util_alloc_mem failed");
		result = NET_NFC_ALLOC_FAIL;
		goto ERROR;
	}

	accept_context->handle = context->handle;
	accept_context->socket = socket;
	accept_context->cb = context->cb;
	accept_context->user_param = context->user_param;
	accept_context->state = NET_NFC_LLCP_STEP_01;

	result = net_nfc_server_llcp_simple_accept(handle, socket,
			_net_nfc_server_phdc_socket_error_cb, accept_context);

	if (result != NET_NFC_OK)
	{
		NFC_ERR("net_nfc_server_llcp_simple_accept failed, [%d]",result);
		goto ERROR;
	}

	NFC_DBG("socket [%x] accepted.. waiting for request message", socket);

	accept_context->cb((net_nfc_phdc_handle_h)accept_context, result,
			NET_NFC_PHDC_TARGET_CONNECTED, data);

	_net_nfc_server_phdc_manager_process(accept_context);

	return;
ERROR :
	if (accept_context != NULL)
	{
		_net_nfc_util_free_mem(accept_context);
	}
}

net_nfc_error_e net_nfc_server_phdc_manager_start(net_nfc_target_handle_s *handle,
		const char *san, sap_t sap, 	net_nfc_server_phdc_cb cb, 	void *user_param)
{
	bool ret;
	net_nfc_error_e result;
	net_nfc_server_phdc_context_t *context = NULL;

	RETV_IF(NULL == handle, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == san, NET_NFC_NULL_PARAMETER);

	ret = net_nfc_server_target_connected(handle);

	if(FALSE == ret)
		return NET_NFC_NOT_CONNECTED;

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	if(NULL == context)
	{
		NFC_ERR("_create_phdc_context failed");
		result = NET_NFC_ALLOC_FAIL;
		goto ERROR;
	}

	context->handle = handle;
	context->cb = cb;
	context->user_param = user_param;
	context->state = NET_NFC_LLCP_STEP_01;

	if(!strcmp(san,PHDC_SAN))
	{
		/*Handle the default PHDC SAN*/
		result = net_nfc_server_llcp_simple_server(handle,	san,sap,
				_net_nfc_server_phdc_incoming_cb,_net_nfc_server_phdc_socket_error_cb,
				context);
	}
	else
	{
		/*Handle other SAN, Implement as and when needed.*/
		result = NET_NFC_NOT_SUPPORTED;
	}

	if (result != NET_NFC_OK)
	{
		NFC_ERR("net_nfc_server_llcp_simple_server failed, [%d]",result);
		goto ERROR;
	}

	NFC_DBG("start phdc manager, san [%s], sap [%d]", san, sap);
	return result;

ERROR :
	if (context != NULL)
		_net_nfc_util_free_mem(context);

	return result;
}

///////////////////////////////////////////////////////////////////

