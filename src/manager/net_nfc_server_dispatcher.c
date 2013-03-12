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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>
#include <glib.h>

#include "vconf.h"

#include "net_nfc_server_dispatcher_private.h"
#include "net_nfc_typedef_private.h"
#include "net_nfc_controller_private.h"
#include "net_nfc_server_ipc_private.h"
#include "net_nfc_debug_private.h"
#include "net_nfc_util_private.h"
#include "net_nfc_util_defines.h"
#include "net_nfc_controller_private.h"
#include "net_nfc_service_private.h"
#include "net_nfc_service_llcp_private.h"
#include "net_nfc_service_llcp_handover_private.h"
#include "net_nfc_service_tag_private.h"
#include "net_nfc_manager_util_private.h"
#include "net_nfc_service_se_private.h"
#include "net_nfc_util_access_control_private.h"
#include "net_nfc_server_context_private.h"

static GQueue *g_dispatcher_queue;
static pthread_cond_t g_dispatcher_queue_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_dispatcher_queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_dispatcher_thread;

static uint8_t g_se_prev_type = SECURE_ELEMENT_TYPE_INVALID;
static uint8_t g_se_prev_mode = SECURE_ELEMENT_OFF_MODE;

static void *_net_nfc_dispatcher_thread_func(void *data);
static net_nfc_request_msg_t *_net_nfc_dispatcher_queue_pop();

static net_nfc_request_msg_t *_net_nfc_dispatcher_queue_pop()
{
	net_nfc_request_msg_t *msg = NULL;
	msg = g_queue_pop_head(g_dispatcher_queue);
	return msg;
}

void net_nfc_dispatcher_queue_push(net_nfc_request_msg_t *req_msg)
{
	pthread_mutex_lock(&g_dispatcher_queue_lock);
	g_queue_push_tail(g_dispatcher_queue, req_msg);
	pthread_cond_signal(&g_dispatcher_queue_cond);
	pthread_mutex_unlock(&g_dispatcher_queue_lock);
}

void net_nfc_dispatcher_cleanup_queue(void)
{
	net_nfc_request_msg_t *req_msg = NULL;

	pthread_mutex_lock(&g_dispatcher_queue_lock);

	DEBUG_SERVER_MSG("cleanup dispatcher Q start");

	while ((req_msg = _net_nfc_dispatcher_queue_pop()) != NULL)
	{
		DEBUG_MSG("abandon request : %d", req_msg->request_type);
		_net_nfc_util_free_mem(req_msg);
	}

	DEBUG_SERVER_MSG("cleanup dispatcher Q end");

	pthread_mutex_unlock(&g_dispatcher_queue_lock);
}

void net_nfc_dispatcher_put_cleaner(void)
{
	net_nfc_request_msg_t *req_msg = NULL;

	_net_nfc_util_alloc_mem(req_msg, sizeof(net_nfc_request_msg_t));
	if (req_msg != NULL)
	{
		DEBUG_SERVER_MSG("put cleaner request");

		req_msg->length = sizeof(net_nfc_request_msg_t);
		req_msg->request_type = NET_NFC_MESSAGE_SERVICE_CLEANER;
		net_nfc_dispatcher_queue_push(req_msg);
	}
}

static void *_net_nfc_dispatcher_copy_message(void *msg)
{
	net_nfc_request_msg_t *origin = (net_nfc_request_msg_t *)msg;
	net_nfc_request_msg_t *result = NULL;

	if (origin == NULL || origin->length == 0)
	{
		return result;
	}

	_net_nfc_util_alloc_mem(result, origin->length);
	if (result != NULL)
	{
		memcpy(result, origin, origin->length);
	}

	return result;
}

bool net_nfc_dispatcher_start_thread()
{
	net_nfc_request_msg_t *req_msg = NULL;
	pthread_attr_t attr;
	int result, state;

	DEBUG_SERVER_MSG("init queue");

	g_dispatcher_queue = g_queue_new();

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	result = vconf_get_bool(VCONFKEY_NFC_STATE, &state);
	if (result != 0)
	{
		DEBUG_SERVER_MSG("VCONFKEY_NFC_STATE is not exist: %d ", result);
		return false;
	}

	DEBUG_SERVER_MSG("net_nfc_dispatcher_start_thread vconf state value [%d]", state);

	if (state == TRUE)
	{
		_net_nfc_util_alloc_mem(req_msg, sizeof(net_nfc_request_msg_t));
		if (req_msg == NULL)
		{
			DEBUG_ERR_MSG("alloc failed");
			return false;
		}

		req_msg->length = sizeof(net_nfc_request_msg_t);
		req_msg->request_type = NET_NFC_MESSAGE_SERVICE_INIT;

		DEBUG_SERVER_MSG("put controller init request");
		net_nfc_dispatcher_queue_push(req_msg);
	}
	else
	{
		/*Don't need to initialize the stack!!*/
	}

	if (pthread_create(&g_dispatcher_thread, &attr, _net_nfc_dispatcher_thread_func, NULL) != 0)
	{
		net_nfc_dispatcher_cleanup_queue();
		DEBUG_ERR_MSG("pthread_create failed");
		return false;
	}

	usleep(0); /* switch to new thread */
	return true;
}

static void *_net_nfc_dispatcher_thread_func(void *data)
{
	net_nfc_request_msg_t *req_msg = NULL;

	DEBUG_SERVER_MSG("net_nfc_controller_thread is created ");

	while (1)
	{
		pthread_mutex_lock(&g_dispatcher_queue_lock);
		if ((req_msg = _net_nfc_dispatcher_queue_pop()) == NULL)
		{
			pthread_cond_wait(&g_dispatcher_queue_cond, &g_dispatcher_queue_lock);
			pthread_mutex_unlock(&g_dispatcher_queue_lock);
			continue;
		}
		pthread_mutex_unlock(&g_dispatcher_queue_lock);

//		DEBUG_SERVER_MSG("net_nfc_controller get command = [%d]", req_msg->request_type);

		switch (req_msg->request_type)
		{
		case NET_NFC_MESSAGE_SERVICE_CLEANER :
			{
				DEBUG_SERVER_MSG("client is terminated abnormally");

				if (g_se_prev_type == SECURE_ELEMENT_TYPE_ESE)
				{
					net_nfc_error_e result = NET_NFC_OK;
					net_nfc_target_handle_s *ese_handle = net_nfc_service_se_get_current_ese_handle();

					if (ese_handle != NULL)
					{
						DEBUG_SERVER_MSG("ese_handle was not freed and disconnected");

						net_nfc_service_se_close_ese();
#ifdef BROADCAST_MESSAGE
						net_nfc_server_set_server_state(NET_NFC_SERVER_IDLE);
#endif
					}

					if ((g_se_prev_type != net_nfc_service_se_get_se_type()) || (g_se_prev_mode != net_nfc_service_se_get_se_mode()))
					{
						net_nfc_controller_set_secure_element_mode(g_se_prev_type, g_se_prev_mode, &result);

						net_nfc_service_se_set_se_type(g_se_prev_type);
						net_nfc_service_se_set_se_mode(g_se_prev_mode);
					}
				}
				else if (g_se_prev_type == SECURE_ELEMENT_TYPE_UICC)
				{
					net_nfc_service_tapi_deinit();
				}
				else
				{
					DEBUG_SERVER_MSG("SE type is not valid");
				}
			}
			break;

		case NET_NFC_MESSAGE_SEND_APDU_SE :
			{
				net_nfc_request_send_apdu_t *detail = (net_nfc_request_send_apdu_t *)req_msg;

				if (detail->handle == (net_nfc_target_handle_s *)UICC_TARGET_HANDLE)
				{
					data_s apdu_data = { NULL, 0 };

					if (net_nfc_util_duplicate_data(&apdu_data, &detail->data) == false)
						break;

					net_nfc_service_transfer_apdu(req_msg->client_fd, &apdu_data, detail->trans_param);

					net_nfc_util_free_data(&apdu_data);
				}
				else if (detail->handle == net_nfc_service_se_get_current_ese_handle())
				{
					data_s *data = NULL;
					net_nfc_error_e result = NET_NFC_OK;
					net_nfc_transceive_info_s info;
					bool success = true;

					info.dev_type = NET_NFC_ISO14443_A_PICC;
					if (net_nfc_util_duplicate_data(&info.trans_data, &detail->data) == false)
					{
						DEBUG_ERR_MSG("alloc failed");
						break;
					}

					if ((success = net_nfc_controller_transceive(detail->handle, &info, &data, &result)) == true)
					{
						if (data != NULL)
						{
							DEBUG_SERVER_MSG("trasceive data recieved [%d], Success = %d", data->length, success);
						}
					}
					else
					{
						DEBUG_SERVER_MSG("trasceive is failed = [%d]", result);
					}
					net_nfc_util_free_data(&info.trans_data);

					if (net_nfc_server_check_client_is_running(req_msg->client_fd))
					{
						net_nfc_response_send_apdu_t resp = { 0 };

						resp.length = sizeof(net_nfc_response_send_apdu_t);
						resp.flags = detail->flags;
						resp.trans_param = detail->trans_param;
						resp.result = result;

						if (success && data != NULL)
						{
							DEBUG_MSG("send response send apdu msg");
							resp.data.length = data->length;
							net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_send_apdu_t),
									data->buffer, data->length, NULL);
						}
						else
						{
							DEBUG_MSG("send response send apdu msg");
							net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_send_apdu_t), NULL);
						}
					}
				}
				else
				{
					DEBUG_SERVER_MSG("invalid se handle");

					if (net_nfc_server_check_client_is_running(req_msg->client_fd))
					{
						net_nfc_response_send_apdu_t resp = { 0 };

						resp.length = sizeof(net_nfc_response_send_apdu_t);
						resp.flags = detail->flags;
						resp.trans_param = detail->trans_param;
						resp.result = NET_NFC_INVALID_PARAM;

						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_send_apdu_t), NULL);
					}
				}
			}
			break;

		case NET_NFC_MESSAGE_CLOSE_INTERNAL_SE :
			{
				net_nfc_request_close_internal_se_t *detail = (net_nfc_request_close_internal_se_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;

				if (detail->handle == (net_nfc_target_handle_s *)UICC_TARGET_HANDLE)
				{
					/*deinit TAPI*/
					DEBUG_SERVER_MSG("UICC is current secure element");
					net_nfc_service_tapi_deinit();

				}
				else if (detail->handle == net_nfc_service_se_get_current_ese_handle())
				{
					result = net_nfc_service_se_close_ese();
#ifdef BROADCAST_MESSAGE
					net_nfc_server_unset_server_state(NET_NFC_SE_CONNECTED);
#endif
				}
				else
				{
					DEBUG_ERR_MSG("invalid se handle received handle = [0x%p] and current handle = [0x%p]", detail->handle, net_nfc_service_se_get_current_ese_handle());
				}

				if ((g_se_prev_type != net_nfc_service_se_get_se_type()) || (g_se_prev_mode != net_nfc_service_se_get_se_mode()))
				{
					/*return back se mode*/
					net_nfc_controller_set_secure_element_mode(g_se_prev_type, g_se_prev_mode, &result);

					net_nfc_service_se_set_se_type(g_se_prev_type);
					net_nfc_service_se_set_se_mode(g_se_prev_mode);
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_close_internal_se_t resp = { 0 };

					resp.length = sizeof(net_nfc_response_close_internal_se_t);
					resp.flags = detail->flags;
					resp.trans_param = detail->trans_param;
					resp.result = result;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_close_internal_se_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_OPEN_INTERNAL_SE :
			{
				net_nfc_request_open_internal_se_t *detail = (net_nfc_request_open_internal_se_t *)req_msg;
				net_nfc_target_handle_s *handle = NULL;
				net_nfc_error_e result = NET_NFC_OK;

				g_se_prev_type = net_nfc_service_se_get_se_type();
				g_se_prev_mode = net_nfc_service_se_get_se_mode();

				if (detail->se_type == SECURE_ELEMENT_TYPE_UICC)
				{
					/*off ESE*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_ESE, SECURE_ELEMENT_OFF_MODE, &result);

					/*Off UICC. UICC SHOULD not be detected by external reader when being communicated in internal process*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_UICC, SECURE_ELEMENT_OFF_MODE, &result);

					net_nfc_service_se_set_se_type(SECURE_ELEMENT_TYPE_UICC);
					net_nfc_service_se_set_se_mode(SECURE_ELEMENT_OFF_MODE);

					/*Init tapi api and return back response*/
					if (net_nfc_service_tapi_init() != true)
					{
						net_nfc_service_tapi_deinit();
						result = NET_NFC_INVALID_STATE;
						handle = NULL;
					}
					else
					{
						result = NET_NFC_OK;
						handle = (net_nfc_target_handle_s *)UICC_TARGET_HANDLE;
					}
				}
				else if (detail->se_type == SECURE_ELEMENT_TYPE_ESE)
				{
					/*Connect NFC-WI to ESE*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_ESE, SECURE_ELEMENT_WIRED_MODE, &result);

					/*off UICC*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_UICC, SECURE_ELEMENT_OFF_MODE, &result);

					net_nfc_service_se_set_se_type(SECURE_ELEMENT_TYPE_ESE);
					net_nfc_service_se_set_se_mode(SECURE_ELEMENT_WIRED_MODE);
					net_nfc_service_se_get_se_setting()->open_request_trans_param = detail->trans_param;

					result = NET_NFC_OK;
					handle = (net_nfc_target_handle_s *)1;
				}
				else
				{
					result = NET_NFC_INVALID_STATE;
					handle = NULL;
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_open_internal_se_t resp = { 0 };

					resp.length = sizeof(net_nfc_response_open_internal_se_t);
					resp.flags = detail->flags;
					resp.trans_param = detail->trans_param;
					resp.result = result;
					resp.se_type = detail->se_type;
					resp.handle = handle;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_set_se_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_SET_SE :
			{
				net_nfc_request_set_se_t *detail = (net_nfc_request_set_se_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;
				bool isTypeChange = false;

				if (detail->se_type != net_nfc_service_se_get_se_type())
				{
					result = net_nfc_service_se_change_se(detail->se_type);
					isTypeChange = true;
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_set_se_t resp = { 0 };

					resp.length = sizeof(net_nfc_response_set_se_t);
					resp.flags = detail->flags;
					resp.trans_param = detail->trans_param;
					resp.se_type = detail->se_type;
					resp.result = result;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_set_se_t), NULL);
				}

				if (isTypeChange)
				{
					net_nfc_response_notify_t noti_se = { 0, };

					net_nfc_broadcast_response_msg(NET_NFC_MESSAGE_SE_TYPE_CHANGED, (void *)&noti_se, sizeof(net_nfc_response_notify_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_GET_SE :
			{
				net_nfc_request_get_se_t *detail = (net_nfc_request_get_se_t *)req_msg;

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_get_se_t resp = { 0 };

					resp.length = sizeof(net_nfc_request_get_se_t);
					resp.flags = detail->flags;
					resp.trans_param = detail->trans_param;
					resp.result = NET_NFC_OK;
					resp.se_type = net_nfc_service_se_get_se_type();

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_get_se_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_P2P_SEND :
			{
				net_nfc_request_p2p_send_t *exchanger = (net_nfc_request_p2p_send_t *)req_msg;

				if (net_nfc_server_is_target_connected(exchanger->handle))
				{
					if (net_nfc_service_send_exchanger_msg(exchanger) != NET_NFC_OK)
					{
						DEBUG_SERVER_MSG("net_nfc_service_send_exchanger_msg is failed");

						/*send result to client*/
						net_nfc_response_p2p_send_t resp_msg = { 0, };

						resp_msg.handle = exchanger->handle;
						resp_msg.result = NET_NFC_P2P_SEND_FAIL;
						resp_msg.trans_param = (void *)exchanger->user_param;

						if (net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, &resp_msg, sizeof(net_nfc_response_p2p_send_t), NULL) == true)
						{
							DEBUG_SERVER_MSG("send exchange failed message to client");
						}
					}
				}
				else
				{
					net_nfc_response_p2p_send_t resp_msg = { 0, };

					resp_msg.handle = exchanger->handle;
					resp_msg.result = NET_NFC_TARGET_IS_MOVED_AWAY;
					resp_msg.trans_param = (void *)exchanger->user_param;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, &resp_msg, sizeof(net_nfc_response_p2p_send_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_TRANSCEIVE :
			{
				net_nfc_request_transceive_t *detail = (net_nfc_request_transceive_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;
				data_s *data = NULL;

				if (net_nfc_server_is_target_connected(detail->handle))
				{
					net_nfc_transceive_info_s info;

					if (net_nfc_util_duplicate_data(&info.trans_data, &detail->info.trans_data) == true)
					{
						bool success;

						DEBUG_MSG("call transceive");
						if ((success = net_nfc_controller_transceive(detail->handle, &info, &data, &result)) == true)
						{
							if (data != NULL)
								DEBUG_SERVER_MSG("trasceive data recieved [%d], Success = %d", data->length, success);
						}
						else
						{
							DEBUG_SERVER_MSG("trasceive is failed = [%d]", result);
						}
						net_nfc_util_free_data(&info.trans_data);
					}
				}
				else
				{
					result = NET_NFC_TARGET_IS_MOVED_AWAY;
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_transceive_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_transceive_t);
					resp.flags = detail->flags;
					resp.trans_param = detail->trans_param;
					resp.result = result;

					if (result == NET_NFC_OK && data != NULL)
					{
						resp.data.length = data->length;

						DEBUG_SERVER_MSG("send response trans msg");
						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_transceive_t),
								data->buffer, data->length, NULL);
					}
					else
					{
						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_transceive_t), NULL);
					}
				}

				if (data != NULL)
				{
					net_nfc_util_free_data(data);
					_net_nfc_util_free_mem(data);
				}
			}
			break;

		case NET_NFC_MESSAGE_MAKE_READ_ONLY_NDEF :
			{
				net_nfc_request_make_read_only_ndef_t *detail = (net_nfc_request_make_read_only_ndef_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;

				if (net_nfc_server_is_target_connected(detail->handle))
				{
					net_nfc_controller_make_read_only_ndef(detail->handle, &result);
				}
				else
				{
					result = NET_NFC_TARGET_IS_MOVED_AWAY;
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_make_read_only_ndef_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_make_read_only_ndef_t);
					resp.flags = detail->flags;
					resp.result = result;
					resp.trans_param = detail->trans_param;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_write_ndef_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_IS_TAG_CONNECTED :
			{
				net_nfc_request_is_tag_connected_t *detail = (net_nfc_request_is_tag_connected_t *)req_msg;
				net_nfc_current_target_info_s* target_info = NULL;

				target_info = net_nfc_server_get_tag_info();
				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_is_tag_connected_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_is_tag_connected_t);
					resp.flags = detail->flags;
					resp.trans_param = detail->trans_param;

					if (target_info != NULL)
					{
						resp.result = NET_NFC_OK;
						resp.devType = target_info->devType;
					}
					else
					{
						resp.result = NET_NFC_NOT_CONNECTED;
						resp.devType = NET_NFC_UNKNOWN_TARGET;
					}

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_is_tag_connected_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_GET_CURRENT_TAG_INFO :
			{
				net_nfc_response_get_current_tag_info_t resp = { 0, };
				net_nfc_request_get_current_tag_info_t *detail = (net_nfc_request_get_current_tag_info_t *)req_msg;
				net_nfc_current_target_info_s *target_info = NULL;
				net_nfc_error_e result = NET_NFC_OK;

				resp.length = sizeof(net_nfc_response_get_current_tag_info_t);
				resp.flags = detail->flags;
				resp.trans_param = detail->trans_param;

				target_info = net_nfc_server_get_tag_info();

				if (target_info != NULL)
				{
					bool success = true;
					data_s* recv_data = NULL;

					if (target_info->devType != NET_NFC_NFCIP1_TARGET && target_info->devType != NET_NFC_NFCIP1_INITIATOR)
					{
#ifdef BROADCAST_MESSAGE
						net_nfc_server_set_server_state(NET_NFC_TAG_CONNECTED);
#endif
						DEBUG_SERVER_MSG("tag is connected");

						uint8_t ndef_card_state = 0;
						int max_data_size = 0;
						int real_data_size = 0;

						if (net_nfc_controller_check_ndef(target_info->handle, &ndef_card_state, &max_data_size, &real_data_size, &result) == true)
						{
							resp.ndefCardState = ndef_card_state;
							resp.maxDataSize = max_data_size;
							resp.actualDataSize = real_data_size;
							resp.is_ndef_supported = 1;
						}

						resp.devType = target_info->devType;
						resp.handle = target_info->handle;
						resp.number_of_keys = target_info->number_of_keys;

						net_nfc_util_duplicate_data(&resp.target_info_values, &target_info->target_info_values);

						if (resp.is_ndef_supported)
						{
							if (net_nfc_controller_read_ndef(target_info->handle, &recv_data, &(resp.result)) == true)
							{
								DEBUG_SERVER_MSG("net_nfc_controller_read_ndef is success");

								resp.raw_data.length = recv_data->length;

								success = net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_get_current_tag_info_t),
								                (void *)(resp.target_info_values.buffer), resp.target_info_values.length,
								                (void *)(recv_data->buffer), recv_data->length, NULL);
							}
							else
							{
								DEBUG_SERVER_MSG("net_nfc_controller_read_ndef is fail");

								resp.raw_data.length = 0;

								success = net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_get_current_tag_info_t),
								                (void *)(resp.target_info_values.buffer), resp.target_info_values.length, NULL);
							}
						}
						else
						{
							resp.raw_data.length = 0;

							success = net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_get_current_tag_info_t),
							                (void *)(resp.target_info_values.buffer), resp.target_info_values.length, NULL);
						}

						net_nfc_util_free_data(&resp.target_info_values);
					}
				}
				else
				{
					resp.result = NET_NFC_NOT_CONNECTED;
					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_get_current_tag_info_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_GET_CURRENT_TARGET_HANDLE :
			{
				net_nfc_request_get_current_target_handle_t *detail = (net_nfc_request_get_current_target_handle_t *)req_msg;
				net_nfc_current_target_info_s *target_info = NULL;

				target_info = net_nfc_server_get_tag_info();
				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_get_current_target_handle_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_get_current_target_handle_t);
					resp.flags = detail->flags;
					resp.trans_param = detail->trans_param;

					if (target_info != NULL)
					{
						resp.handle = target_info->handle;
						resp.devType = target_info->devType;
						resp.result = NET_NFC_OK;
					}
					else
					{
						resp.result = NET_NFC_NOT_CONNECTED;
					}

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_get_current_target_handle_t), NULL);
				}
			}
			break;

		case NET_NFC_GET_SERVER_STATE :
			{
				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_get_server_state_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_get_server_state_t);
					resp.flags = req_msg->flags;
					resp.state = net_nfc_server_get_server_state();
					resp.result = NET_NFC_OK;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_get_server_state_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_READ_NDEF :
			{
				net_nfc_request_read_ndef_t *detail = (net_nfc_request_read_ndef_t*)req_msg;
				net_nfc_error_e result = NET_NFC_OK;
				data_s *data = NULL;
				bool success = false;

				if (net_nfc_server_is_target_connected(detail->handle))
				{
					success = net_nfc_controller_read_ndef(detail->handle, &data, &result);
				}
				else
				{
					result = NET_NFC_TARGET_IS_MOVED_AWAY;
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_read_ndef_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_read_ndef_t);
					resp.flags = detail->flags;
					resp.result = result;
					resp.trans_param = detail->trans_param;

					if (success)
					{
						resp.data.length = data->length;
						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_read_ndef_t),
						                data->buffer, data->length, NULL);
					}
					else
					{
						resp.data.length = 0;
						resp.data.buffer = NULL;
						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_read_ndef_t), NULL);
					}
				}

				if (data != NULL)
				{
					net_nfc_util_free_data(data);
					_net_nfc_util_free_mem(data);
				}
			}
			break;

		case NET_NFC_MESSAGE_WRITE_NDEF :
			{
				net_nfc_request_write_ndef_t *detail = (net_nfc_request_write_ndef_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;

				if (net_nfc_server_is_target_connected(detail->handle))
				{
					data_s data = { NULL, 0 };

					if (net_nfc_util_duplicate_data(&data, &detail->data) == true)
					{
						net_nfc_controller_write_ndef(detail->handle, &data, &result);

						net_nfc_util_free_data(&data);
					}
					else
					{
						result = NET_NFC_ALLOC_FAIL;
					}
				}
				else
				{
					result = NET_NFC_TARGET_IS_MOVED_AWAY;
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_write_ndef_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_write_ndef_t);
					resp.flags = detail->flags;
					resp.result = result;
					resp.trans_param = detail->trans_param;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_write_ndef_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_FORMAT_NDEF :
			{
				net_nfc_request_format_ndef_t *detail = (net_nfc_request_format_ndef_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;

				if (net_nfc_server_is_target_connected(detail->handle))
				{
					data_s data = { NULL, 0 };

					if (net_nfc_util_duplicate_data(&data, &detail->key) == true)
					{
						net_nfc_controller_format_ndef(detail->handle, &data, &result);
						net_nfc_util_free_data(&data);
					}
					else
					{
						result = NET_NFC_ALLOC_FAIL;
					}
				}
				else
				{
					result = NET_NFC_TARGET_IS_MOVED_AWAY;
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_format_ndef_t resp = { 0 };

					resp.length = sizeof(net_nfc_response_format_ndef_t);
					resp.flags = detail->flags;
					resp.result = result;
					resp.trans_param = detail->trans_param;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_format_ndef_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_SIM_TEST :
			{
				net_nfc_request_test_t *detail = (net_nfc_request_test_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;

				if (net_nfc_controller_sim_test(&result) == true)
				{
					DEBUG_SERVER_MSG("net_nfc_controller_sim_test Result [SUCCESS]");
				}
				else
				{
					DEBUG_SERVER_MSG("net_nfc_controller_sim_test Result [ERROR1]");
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_test_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_test_t);
					resp.flags = detail->flags;
					resp.result = result;
					resp.trans_param = detail->trans_param;

					DEBUG_SERVER_MSG("SEND RESPONSE!!");
					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_test_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_GET_FIRMWARE_VERSION :
			{
				net_nfc_error_e result = NET_NFC_OK;
				data_s *data = NULL;

				if (net_nfc_controller_get_firmware_version(&data, &result) == true)
				{
					DEBUG_SERVER_MSG("net_nfc_controller_update_firmware Result [SUCCESS]");

				}
				else
				{
					DEBUG_ERR_MSG("net_nfc_controller_update_firmware Result [ERROR3]");
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_firmware_version_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_firmware_version_t);
					resp.flags = req_msg->flags;
					resp.result = result;
					resp.data.length = data->length;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_firmware_version_t), (void *)data->buffer, resp.data.length, NULL);
				}

				net_nfc_util_free_data(data);
			}
			break;

		case NET_NFC_MESSAGE_PRBS_TEST :
			{
				net_nfc_request_test_t *detail = (net_nfc_request_test_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;
				uint32_t local_tech = 0;
				uint32_t local_rate = 0;

				local_tech = detail->tech;
				local_rate = detail->rate;

				DEBUG_SERVER_MSG("NET_NFC_MESSAGE_PRBS_TEST local_tech [%d]\n", local_tech);
				DEBUG_SERVER_MSG("NET_NFC_MESSAGE_PRBS_TEST local_rate [%d]\n", local_rate);

				if (net_nfc_controller_prbs_test(&result, local_tech, local_rate) == true)
				{
					DEBUG_SERVER_MSG("net_nfc_controller_prbs_test Result [SUCCESS]");
				}
				else
				{
					DEBUG_ERR_MSG("net_nfc_controller_prbs_test Result [ERROR3]");
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_test_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_test_t);
					resp.flags = detail->flags;
					resp.result = result;
					resp.trans_param = detail->trans_param;

					DEBUG_SERVER_MSG("SEND RESPONSE!!");
					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_test_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_SET_EEDATA :
			{
				net_nfc_request_eedata_register_t *detail = (net_nfc_request_eedata_register_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;
				uint32_t local_mode = 0;
				uint32_t local_reg_id = 0;
				data_s data = { NULL, 0 };

				local_mode = detail->mode;
				local_reg_id = detail->reg_id;

				DEBUG_SERVER_MSG("NET_NFC_MESSAGE_SET_EEDATA local_mode [%d]\n", local_mode);
				DEBUG_SERVER_MSG("NET_NFC_MESSAGE_SET_EEDATA local_reg_id [%d]\n", local_reg_id);

				if (net_nfc_util_duplicate_data(&data, &detail->data) == true)
				{
					if (net_nfc_controller_eedata_register_set(&result, local_mode, local_reg_id, &data) == true)
					{
						DEBUG_SERVER_MSG("net_nfc_controller_eedata_register_set Result [SUCCESS]");
					}
					else
					{
						DEBUG_ERR_MSG("net_nfc_controller_eedata_register_set Result [ERROR3]");
					}
					net_nfc_util_free_data(&data);

					if (net_nfc_server_check_client_is_running(req_msg->client_fd))
					{
						net_nfc_response_test_t resp = { 0, };

						resp.length = sizeof(net_nfc_response_test_t);
						resp.flags = detail->flags;
						resp.result = result;
						resp.trans_param = detail->trans_param;

						DEBUG_SERVER_MSG("SEND RESPONSE!!");
						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_test_t), NULL);
					}
				}
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_DEINIT :
			{
				net_nfc_error_e result;

				result = net_nfc_service_se_change_se(SECURE_ELEMENT_TYPE_INVALID);

				/* release access control instance */
				net_nfc_util_access_control_release();

				net_nfc_server_free_current_tag_info();

				if (net_nfc_controller_deinit() == TRUE)
				{
					DEBUG_SERVER_MSG("net_nfc_controller_deinit success [%d]", result);

					/*vconf off*/
					if (vconf_set_bool(VCONFKEY_NFC_STATE, FALSE) != 0)
					{
						DEBUG_ERR_MSG("vconf_set_bool failed");
					}

					net_nfc_response_test_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_test_t);
					resp.flags = req_msg->flags;
					resp.result = NET_NFC_OK;
					resp.trans_param = (void *)req_msg->user_param;

					net_nfc_broadcast_response_msg(req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_test_t), NULL);
				}
				else
				{
					DEBUG_SERVER_MSG("net_nfc_controller_deinit failed");

					if (net_nfc_server_check_client_is_running(req_msg->client_fd))
					{
						net_nfc_response_test_t resp = { 0, };

						resp.length = sizeof(net_nfc_response_test_t);
						resp.flags = req_msg->flags;
						resp.result = NET_NFC_UNKNOWN_ERROR;
						resp.trans_param = (void *)req_msg->user_param;

						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_test_t), NULL);
					}
				}
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_INIT :
			{
				net_nfc_error_e result;

				if (net_nfc_controller_init(&result) == true)
				{
					net_nfc_llcp_config_info_s config = { 128, 1, 100, 0 };

					if (net_nfc_controller_register_listener(net_nfc_service_target_detected_cb, net_nfc_service_se_transaction_cb, net_nfc_service_llcp_event_cb, &result) == true)
					{
						DEBUG_SERVER_MSG("net_nfc_controller_register_listener Success!!");
					}
					else
					{
						DEBUG_ERR_MSG("net_nfc_controller_register_listener failed [%d]", result);
					}

					if (net_nfc_controller_llcp_config(&config, &result) == true)
					{
						/*We need to check the stack that supports the llcp or not.*/
						DEBUG_SERVER_MSG("llcp is enabled");
					}
					else
					{
						DEBUG_ERR_MSG("net_nfc_controller_llcp_config failed [%d]", result);
					}

					result = net_nfc_service_se_change_se(SECURE_ELEMENT_TYPE_UICC);

					if (net_nfc_controller_confiure_discovery(NET_NFC_DISCOVERY_MODE_CONFIG, NET_NFC_ALL_ENABLE, &result) == true)
					{
						DEBUG_SERVER_MSG("now, nfc is ready");
					}
					else
					{
						DEBUG_ERR_MSG("net_nfc_controller_confiure_discovery failed [%d]", result);
					}

					/* initialize access control instance */
					net_nfc_util_access_control_initialize();

					/*Send the Init Success Response Msg*/
					{
						net_nfc_response_test_t resp = { 0, };

						DEBUG_SERVER_MSG("net_nfc_controller_init success [%d]", result);

						resp.length = sizeof(net_nfc_response_test_t);
						resp.flags = req_msg->flags;
						resp.result = NET_NFC_OK;
						resp.trans_param = (void *)req_msg->user_param;

						/*vconf on*/
						if (vconf_set_bool(VCONFKEY_NFC_STATE, TRUE) != 0)
						{
							DEBUG_ERR_MSG("vconf_set_bool failed");
						}

						net_nfc_broadcast_response_msg(req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_test_t), NULL);
					}
				}
				else
				{
					DEBUG_ERR_MSG("net_nfc_controller_init failed [%d]", result);

					if (net_nfc_server_check_client_is_running(req_msg->client_fd))
					{
						net_nfc_response_test_t resp = { 0, };

						resp.length = sizeof(net_nfc_response_test_t);
						resp.flags = req_msg->flags;
						resp.result = result;
						resp.trans_param = (void *)req_msg->user_param;

						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_test_t), NULL);
					}
				}
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_STANDALONE_TARGET_DETECTED :
			{
#ifndef BROADCAST_MESSAGE
				net_nfc_service_standalone_mode_target_detected(req_msg);
#endif
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_RESTART_POLLING_LOOP :
			{
				net_nfc_error_e result = NET_NFC_OK;
				int pm_state = 0;
				int set_config = 0;
				net_nfc_request_msg_t *discovery_req = (net_nfc_request_msg_t *)req_msg;

				pm_state = discovery_req->user_param;

				DEBUG_SERVER_MSG("NET_NFC_MESSAGE_SERVICE_RESTART_POLLING_LOOP PM State = [%d]", pm_state);

				if (pm_state == 1)
				{
					set_config = NET_NFC_ALL_ENABLE;
				}
				else if (pm_state == 3)
				{
					set_config = NET_NFC_ALL_DISABLE;
				}
				else
				{
					DEBUG_SERVER_MSG("Do not anything!!");
				}

				//if (net_nfc_controller_confiure_discovery(NET_NFC_DISCOVERY_MODE_RESUME, NET_NFC_ALL_ENABLE, &result) == true)
				if (net_nfc_controller_confiure_discovery(NET_NFC_DISCOVERY_MODE_CONFIG, set_config, &result) == true)
				{
					DEBUG_SERVER_MSG("now, nfc polling loop is running again");
				}
			}
			break;

		case NET_NFC_MESSAGE_SE_START_TRANSACTION :
			{
				net_nfc_service_se_transaction_receive(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_ACCEPT :
			{
				net_nfc_service_llcp_process_accept(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_DEACTIVATED :
			{
				net_nfc_service_llcp_disconnect_target(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_SOCKET_ERROR :
			{
				net_nfc_service_llcp_process_socket_error(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_SOCKET_ACCEPTED_ERROR :
			{
				net_nfc_service_llcp_process_accepted_socket_error(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_SEND :
			{
				net_nfc_service_llcp_process_send_socket(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_SEND_TO :
			{
				net_nfc_service_llcp_process_send_to_socket(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_RECEIVE :
			{
				net_nfc_service_llcp_process_receive_socket(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_RECEIVE_FROM :
			{
				net_nfc_service_llcp_process_receive_from_socket(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_CONNECT :
			{
				net_nfc_service_llcp_process_connect_socket(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_CONNECT_SAP :
			{
				net_nfc_service_llcp_process_connect_sap_socket(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_DISCONNECT :
			{
				net_nfc_service_llcp_process_disconnect_socket(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_SE :
			{
				net_nfc_request_set_se_t *detail = (net_nfc_request_set_se_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;
				int mode;

				mode = (int)detail->se_type;

				if (mode == NET_NFC_SE_CMD_UICC_ON)
				{
					/*turn on UICC*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_UICC, SECURE_ELEMENT_VIRTUAL_MODE, &result);

					/*turn off ESE*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_ESE, SECURE_ELEMENT_OFF_MODE, &result);
				}
				else if (mode == NET_NFC_SE_CMD_ESE_ON)
				{
					/*turn off UICC*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_UICC, SECURE_ELEMENT_OFF_MODE, &result);

					/*turn on ESE*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_ESE, SECURE_ELEMENT_VIRTUAL_MODE, &result);
				}
				else if (mode == NET_NFC_SE_CMD_ALL_OFF)
				{
					/*turn off both*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_UICC, SECURE_ELEMENT_OFF_MODE, &result);

					/*turn on ESE*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_ESE, SECURE_ELEMENT_OFF_MODE, &result);
				}
				else
				{
					/*turn off both*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_UICC, SECURE_ELEMENT_VIRTUAL_MODE, &result);

					/*turn on ESE*/
					net_nfc_controller_set_secure_element_mode(SECURE_ELEMENT_TYPE_ESE, SECURE_ELEMENT_VIRTUAL_MODE, &result);
				}
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_TERMINATION :
			{
				net_nfc_service_termination(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_SLAVE_TARGET_DETECTED :
			{
				net_nfc_service_slave_mode_target_detected(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_SLAVE_ESE_DETECTED :
			{
				net_nfc_service_se_detected(req_msg);
			}
			break;

		case NET_NFC_MESSAGE_LLCP_LISTEN :
			{
				net_nfc_request_listen_socket_t *detail = (net_nfc_request_listen_socket_t *)req_msg;
				net_nfc_response_llcp_socket_error_t *error = NULL;
				net_nfc_error_e result = NET_NFC_OK;
				bool success = false;

				_net_nfc_util_alloc_mem(error, sizeof (net_nfc_response_llcp_socket_error_t));
				if (error == NULL)
				{
					DEBUG_SERVER_MSG("ERROR: allocation is failed");
					break;
				}

				error->length = sizeof(net_nfc_response_llcp_socket_error_t);
				error->client_socket = detail->client_socket;
				error->handle = detail->handle;

				success = net_nfc_controller_llcp_create_socket(&(detail->oal_socket), detail->type, detail->miu, detail->rw, &result, error);
				if (success == true)
				{
					error->oal_socket = detail->oal_socket;
					success = net_nfc_controller_llcp_bind(detail->oal_socket, detail->sap, &result);
				}
				else
				{
					_net_nfc_util_free_mem(error);
				}

				if (success == true)
				{
					DEBUG_SERVER_MSG("OAL socket in Listen :%d", detail->oal_socket);
					success = net_nfc_controller_llcp_listen(detail->handle, detail->service_name.buffer, detail->oal_socket, &result, error);
				}

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_listen_socket_t resp = { 0 };

					resp.length = sizeof(net_nfc_response_listen_socket_t);
					resp.flags = detail->flags;
					resp.result = result;
					resp.oal_socket = detail->oal_socket;
					resp.client_socket = detail->client_socket;
					resp.trans_param = detail->trans_param;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_listen_socket_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_LLCP_CONNECT :
			{
				net_nfc_request_connect_socket_t *detail = (net_nfc_request_connect_socket_t *)req_msg;
				net_nfc_response_connect_socket_t *resp = NULL;
				net_nfc_response_llcp_socket_error_t *error = NULL;
				bool success = false;

				_net_nfc_util_alloc_mem(error, sizeof (net_nfc_response_llcp_socket_error_t));
				if (error == NULL)
				{
					DEBUG_SERVER_MSG("ERROR: invalid detail info or allocation is failed");
					break;
				}

				_net_nfc_util_alloc_mem(resp, sizeof (net_nfc_response_connect_socket_t));
				if (resp == NULL)
				{
					DEBUG_SERVER_MSG("ERROR: invalid detail info or allocation is failed");
					_net_nfc_util_free_mem(error);
					break;
				}

				error->length = sizeof(net_nfc_response_llcp_socket_error_t);
				error->client_socket = detail->client_socket;
				error->handle = detail->handle;

				resp->length = sizeof(net_nfc_response_connect_socket_t);
				resp->flags = detail->flags;
				resp->result = NET_NFC_IPC_FAIL;

				success = net_nfc_controller_llcp_create_socket(&(detail->oal_socket), detail->type, detail->miu, detail->rw, &(resp->result), error);
				if (success == true)
				{
					error->oal_socket = resp->oal_socket = detail->oal_socket;
					DEBUG_SERVER_MSG("connect client socket [%d]", detail->client_socket);
					resp->client_socket = detail->client_socket;
					resp->trans_param = detail->trans_param;

					success = net_nfc_controller_llcp_connect_by_url(detail->handle, detail->oal_socket, detail->service_name.buffer, &(resp->result), resp);
					if (success == false)
					{
						DEBUG_ERR_MSG("connect client socket is failed");

						net_nfc_controller_llcp_socket_close(resp->oal_socket, &(resp->result));

						if (net_nfc_server_check_client_is_running(req_msg->client_fd))
						{
							net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)resp, sizeof(net_nfc_response_connect_socket_t), NULL);
						}
						_net_nfc_util_free_mem(resp);
					}
				}
				else
				{
					if (net_nfc_server_check_client_is_running(req_msg->client_fd))
					{
						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)resp, sizeof(net_nfc_response_connect_socket_t), NULL);
					}

					_net_nfc_util_free_mem(error);
					_net_nfc_util_free_mem(resp);
				}
			}
			break;

		case NET_NFC_MESSAGE_LLCP_CONNECT_SAP :
			{
				net_nfc_request_connect_sap_socket_t *detail = (net_nfc_request_connect_sap_socket_t *)req_msg;
				net_nfc_response_connect_sap_socket_t *resp = NULL;
				bool success = false;

				_net_nfc_util_alloc_mem(resp, sizeof(net_nfc_response_connect_sap_socket_t));
				if (resp == NULL)
				{
					DEBUG_SERVER_MSG("ERROR: allocation is failed");
					break;
				}

				resp->length = sizeof(net_nfc_response_connect_sap_socket_t);
				resp->flags = detail->flags;
				resp->result = NET_NFC_IPC_FAIL;

				success = net_nfc_controller_llcp_create_socket(&(detail->oal_socket), detail->type, detail->miu, detail->rw, &(resp->result), NULL);
				if (success == true)
				{
					resp->oal_socket = detail->oal_socket;
					resp->client_socket = detail->client_socket;
					resp->trans_param = detail->trans_param;

					success = net_nfc_controller_llcp_connect(detail->handle, detail->oal_socket, detail->sap, &(resp->result), resp);
				}

				if (success == false)
				{
					if (net_nfc_server_check_client_is_running(req_msg->client_fd))
					{
						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)resp, sizeof(net_nfc_response_connect_sap_socket_t), NULL);
					}
					_net_nfc_util_free_mem(resp);
				}
			}
			break;

		case NET_NFC_MESSAGE_LLCP_SEND :
			{
				net_nfc_request_send_socket_t *detail = (net_nfc_request_send_socket_t *)req_msg;
				data_s data = { NULL, 0 };

				if (net_nfc_util_duplicate_data(&data, &detail->data) == true)
				{
					net_nfc_response_send_socket_t *resp = NULL;

					_net_nfc_util_alloc_mem(resp, sizeof (net_nfc_response_send_socket_t));
					if (resp != NULL)
					{
						resp->length = sizeof(net_nfc_response_send_socket_t);
						resp->flags = detail->flags;
						resp->result = NET_NFC_IPC_FAIL;
						resp->client_socket = detail->client_socket;
						resp->trans_param = detail->trans_param;

						if (net_nfc_controller_llcp_send(detail->handle, detail->oal_socket, &data, &(resp->result), resp) == false)
						{
							if (net_nfc_server_check_client_is_running(req_msg->client_fd))
							{
								net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)resp, sizeof(net_nfc_response_send_socket_t), NULL);
							}
							_net_nfc_util_free_mem(resp);
						}
					}

					net_nfc_util_free_data(&data);
				}
			}
			break;

		case NET_NFC_MESSAGE_LLCP_RECEIVE :
			{
				net_nfc_request_receive_socket_t *detail = (net_nfc_request_receive_socket_t *)req_msg;
				net_nfc_response_receive_socket_t *resp = NULL;

				_net_nfc_util_alloc_mem(resp, sizeof (net_nfc_response_receive_socket_t));
				if (resp == NULL)
				{
					DEBUG_SERVER_MSG("ERROR: allocation is failed");
					break;
				}

				resp->length = sizeof(net_nfc_response_receive_socket_t);
				resp->flags = detail->flags;
				resp->result = NET_NFC_IPC_FAIL;
				resp->client_socket = detail->client_socket;
				resp->trans_param = detail->trans_param;
				resp->data.length = detail->req_length;
				_net_nfc_util_alloc_mem(resp->data.buffer, detail->req_length);
				if (resp->data.buffer == NULL)
				{
					DEBUG_SERVER_MSG("ERROR: allocation is failed");
					_net_nfc_util_free_mem(resp);
					break;
				}

				if (net_nfc_controller_llcp_recv(detail->handle, detail->oal_socket, &(resp->data), &(resp->result), resp) == false)
				{
					if (net_nfc_server_check_client_is_running(req_msg->client_fd))
					{
						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)resp, sizeof(net_nfc_response_receive_socket_t), NULL);
					}
					_net_nfc_util_free_mem(resp->data.buffer);
					_net_nfc_util_free_mem(resp);
				}
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_LLCP_CLOSE :
			{
				net_nfc_request_close_socket_t *detail = (net_nfc_request_close_socket_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;

				DEBUG_SERVER_MSG("socket close :: NET_NFC_MESSAGE_SERVICE_LLCP_CLOSE");
				net_nfc_controller_llcp_socket_close(detail->oal_socket, &result);

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_close_socket_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_close_socket_t);
					resp.flags = detail->flags;
					resp.result = result;
					resp.client_socket = detail->client_socket;
					resp.trans_param = detail->trans_param;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_close_socket_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_LLCP_DISCONNECT : /* change resp to local variable. if there is some problem, check this first. */
			{
				net_nfc_request_disconnect_socket_t *detail = (net_nfc_request_disconnect_socket_t *)req_msg;
				net_nfc_request_disconnect_socket_t *context = NULL;
				net_nfc_error_e result = NET_NFC_OK;

				context = _net_nfc_dispatcher_copy_message(detail);
				if (context == NULL)
				{
					DEBUG_ERR_MSG("alloc failed");
					break;
				}

				if (net_nfc_controller_llcp_disconnect(detail->handle, detail->oal_socket, &result, &context) == false)
				{
					if (net_nfc_server_check_client_is_running(req_msg->client_fd))
					{
						net_nfc_response_disconnect_socket_t resp = { 0, };

						resp.length = sizeof(net_nfc_response_disconnect_socket_t);
						resp.flags = detail->flags;
						resp.result = result;
						resp.client_socket = detail->client_socket;
						resp.trans_param = detail->trans_param;

						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_disconnect_socket_t), NULL);
					}

					_net_nfc_util_free_mem(context);
				}

			}
			break;

		case NET_NFC_MESSAGE_LLCP_ACCEPTED :
			{
				net_nfc_request_accept_socket_t *detail = (net_nfc_request_accept_socket_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;

				net_nfc_controller_llcp_accept(detail->incomming_socket, &result);

				if (net_nfc_server_check_client_is_running(req_msg->client_fd))
				{
					net_nfc_response_accept_socket_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_accept_socket_t);
					resp.flags = detail->flags;
					resp.trans_param = detail->trans_param;
					resp.result = result;
					resp.client_socket = detail->client_socket;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_accept_socket_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_LLCP_CONFIG :
			{
				net_nfc_request_config_llcp_t *detail = (net_nfc_request_config_llcp_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;

				net_nfc_controller_llcp_config(&(detail->config), &result);

				if (net_nfc_server_check_client_is_running(detail->client_fd))
				{
					net_nfc_response_config_llcp_t resp = { 0, };

					resp.length = sizeof(net_nfc_response_config_llcp_t);
					resp.flags = detail->flags;
					resp.trans_param = detail->trans_param;
					resp.result = result;

					net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_config_llcp_t), NULL);
				}
			}
			break;

		case NET_NFC_MESSAGE_CONNECTION_HANDOVER :
			{
				net_nfc_request_connection_handover_t *detail = (net_nfc_request_connection_handover_t *)req_msg;
				net_nfc_error_e result = NET_NFC_OK;

				net_nfc_request_msg_t *param = NULL;

				if ((param = _net_nfc_dispatcher_copy_message(detail)) == NULL)
				{
					DEBUG_ERR_MSG("alloc failed");
					break;
				}

				if ((result = net_nfc_service_llcp_handover_send_request_msg((net_nfc_request_connection_handover_t *)param)) != NET_NFC_OK)
				{
					if (net_nfc_server_check_client_is_running(req_msg->client_fd))
					{
						net_nfc_response_connection_handover_t resp = { 0, };

						resp.length = sizeof(net_nfc_response_connection_handover_t);
						resp.flags = detail->flags;
						resp.user_param = detail->user_param;
						resp.result = result;
						resp.event = NET_NFC_OPERATION_FAIL;
						resp.type = NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN;

						net_nfc_send_response_msg(req_msg->client_fd, req_msg->request_type, (void *)&resp, sizeof(net_nfc_response_connection_handover_t), NULL);
					}
				}

				_net_nfc_util_free_mem(param);
			}
			break;

		case NET_NFC_MESSAGE_SERVICE_WATCH_DOG :
			{
				net_nfc_service_watch_dog(req_msg);
				continue;
			}
			break;

		default :
			break;
		}

		/*need to free req_msg*/
		_net_nfc_util_free_mem(req_msg);
	}

	return (void *)NULL;
}
