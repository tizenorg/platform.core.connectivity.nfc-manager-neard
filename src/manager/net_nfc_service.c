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
#include <malloc.h>

#include "net_nfc_controller_private.h"
#include "net_nfc_util_private.h"
#include "net_nfc_typedef.h"
#include "net_nfc_debug_private.h"
#include "net_nfc_service_private.h"
#include "net_nfc_service_se_private.h"
#include "net_nfc_app_util_private.h"
#include "net_nfc_server_ipc_private.h"
#include "net_nfc_server_dispatcher_private.h"
#include "net_nfc_manager_util_private.h"
#include "net_nfc_service_tag_private.h"
#include "net_nfc_service_llcp_private.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_ndef_record.h"
#include "net_nfc_util_access_control_private.h"

/* static variable */

/* static callback function */

/* static function */

#ifndef BROADCAST_MESSAGE
extern uint8_t g_se_cur_type;
extern uint8_t g_se_cur_mode;

static bool _net_nfc_service_check_internal_ese_detected()
{
	if (g_se_cur_type == SECURE_ELEMENT_TYPE_ESE && g_se_cur_mode == SECURE_ELEMENT_WIRED_MODE)
	{
		return true;
	}

	return false;
}
#endif

static void _net_nfc_service_show_exception_msg(char* msg);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void net_nfc_service_target_detected_cb(void *info, void *user_context)
{
	net_nfc_request_msg_t *req_msg = (net_nfc_request_msg_t *)info;

	if (info == NULL)
		return;

	if (req_msg->request_type == NET_NFC_MESSAGE_SERVICE_RESTART_POLLING_LOOP)
	{
		net_nfc_dispatcher_queue_push(req_msg);
	}
#ifdef BROADCAST_MESSAGE
	else
	{
		net_nfc_server_set_tag_info(info);

		req_msg->request_type = NET_NFC_MESSAGE_SERVICE_SLAVE_TARGET_DETECTED;
		net_nfc_dispatcher_queue_push(req_msg);
	}
#else
	else if (net_nfc_server_get_current_client_context(&client_context) == true &&
		net_nfc_server_check_client_is_running(&client_context) == true)
	{
		net_nfc_request_target_detected_t *detail = (net_nfc_request_target_detected_t *)req_msg;

		/* If target detected, sound should be played. */
		net_nfc_manager_util_play_sound(NET_NFC_TASK_START);

		net_nfc_server_set_tag_info(info);

		if (!_net_nfc_service_check_internal_ese_detected())
		{
			req_msg->request_type = NET_NFC_MESSAGE_SERVICE_SLAVE_TARGET_DETECTED;
		}
		else
		{
			req_msg->request_type = NET_NFC_MESSAGE_SERVICE_SLAVE_ESE_DETECTED;

			(detail->handle)->connection_type = NET_NFC_SE_CONNECTION;
		}

		net_nfc_server_set_current_client_target_handle(client_context, detail->handle);

		net_nfc_dispatcher_queue_push(req_msg);

		DEBUG_SERVER_MSG("current client is listener. stand alone mode will be activated");
	}
	else
	{
		/* If target detected, sound should be played. */
		net_nfc_manager_util_play_sound(NET_NFC_TASK_START);

		req_msg->request_type = NET_NFC_MESSAGE_SERVICE_STANDALONE_TARGET_DETECTED;
		net_nfc_dispatcher_queue_push(req_msg);
	}
#endif
}

void net_nfc_service_se_transaction_cb(void *info, void *user_context)
{
	net_nfc_request_se_event_t *req_msg = (net_nfc_request_se_event_t *)info;

	DEBUG_SERVER_MSG("se event [%d]", req_msg->request_type);

	net_nfc_dispatcher_queue_push((net_nfc_request_msg_t *)req_msg);
}

void net_nfc_service_llcp_event_cb(void* info, void* user_context)
{
	net_nfc_request_llcp_msg_t *req_msg = (net_nfc_request_llcp_msg_t *)info;

	if (req_msg == NULL)
	{
		DEBUG_SERVER_MSG("req msg is null");
		return;
	}

	if (req_msg->request_type == NET_NFC_MESSAGE_SERVICE_LLCP_ACCEPT)
	{
		net_nfc_request_accept_socket_t *detail = (net_nfc_request_accept_socket_t *)req_msg;
		detail->trans_param = user_context;
	}

	req_msg->user_param = (uint32_t)user_context;

	net_nfc_dispatcher_queue_push((net_nfc_request_msg_t *)req_msg);
}

static bool _is_isp_dep_ndef_formatable(net_nfc_target_handle_s *handle, int dev_type)
{
	bool result = false;
	uint8_t cmd[] = { 0x90, 0x60, 0x00, 0x00, 0x00 };
	net_nfc_transceive_info_s info;
	data_s *response = NULL;
	net_nfc_error_e error = NET_NFC_OK;

	info.dev_type = dev_type;
	info.trans_data.buffer = cmd;
	info.trans_data.length = sizeof(cmd);

	if (net_nfc_controller_transceive(handle, &info, &response, &error) == true)
	{
		if (response != NULL)
		{
			if (response->length == 9 && response->buffer[7] == (uint8_t)0x91 &&
				response->buffer[8] == (uint8_t)0xAF)
			{
				result = true;
			}
			else
			{
				DEBUG_ERR_MSG("Unknown response....");
				DEBUG_MSG_PRINT_BUFFER(response->buffer, response->length);
			}
		}
		else
		{
			DEBUG_ERR_MSG("net_nfc_controller_transceive response is null");
		}
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_controller_transceive is failed, [%d]", result);
	}

	return result;
}

static net_nfc_error_e _read_ndef_message(net_nfc_target_handle_s *handle, int dev_type, data_s **read_ndef)
{
	net_nfc_error_e result = NET_NFC_OK;
	data_s *temp = NULL;

	if (handle == NULL || read_ndef == NULL)
		return NET_NFC_INVALID_PARAM;

	*read_ndef = NULL;

	/* Desfire */
	if (dev_type == NET_NFC_MIFARE_DESFIRE_PICC)
	{
		DEBUG_SERVER_MSG("DESFIRE : check ISO-DEP ndef formatable");

		if (_is_isp_dep_ndef_formatable(handle, dev_type) == true)
		{
			DEBUG_SERVER_MSG("DESFIRE : ISO-DEP ndef formatable");

			if (!net_nfc_controller_connect(handle, &result))
			{
				DEBUG_ERR_MSG("net_nfc_controller_connect failed & Retry Polling!!");

				if (net_nfc_controller_confiure_discovery(NET_NFC_DISCOVERY_MODE_RESUME,
					NET_NFC_ALL_ENABLE, &result) == false)
				{
					net_nfc_controller_exception_handler();
				}

				return NET_NFC_TAG_READ_FAILED;
			}
		}
		else
		{
			DEBUG_ERR_MSG("DESFIRE : ISO-DEP ndef not formatable");

			return NET_NFC_TAG_READ_FAILED;
		}
	}

	if (net_nfc_controller_read_ndef(handle, &temp, &result) == true)
	{
		DEBUG_SERVER_MSG("net_nfc_controller_read_ndef success");

		if (dev_type == NET_NFC_MIFARE_DESFIRE_PICC)
		{
			if (!net_nfc_controller_connect(handle, &result))
			{
				DEBUG_ERR_MSG("net_nfc_controller_connect failed & Retry Polling!!");

				if (net_nfc_controller_confiure_discovery(NET_NFC_DISCOVERY_MODE_RESUME, NET_NFC_ALL_ENABLE, &result) == false)
				{
					net_nfc_controller_exception_handler();
				}

				net_nfc_util_free_data(temp);
				_net_nfc_util_free_mem(temp);

				return NET_NFC_TAG_READ_FAILED;
			}
		}

		*read_ndef = temp;
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_controller_read_ndef failed [%d]", result);
	}

	return result;
}

#ifndef BROADCAST_MESSAGE
bool net_nfc_service_standalone_mode_target_detected(net_nfc_request_msg_t* msg)
{
	net_nfc_request_target_detected_t* stand_alone = (net_nfc_request_target_detected_t *)msg;
	net_nfc_error_e error_status = NET_NFC_OK;

	data_s *recv_data = NULL;

	DEBUG_SERVER_MSG("*** Detected! type [0x%X)] ***", stand_alone->devType);

	switch (stand_alone->devType)
	{
	case NET_NFC_NFCIP1_TARGET :
	case NET_NFC_NFCIP1_INITIATOR :
		{
			DEBUG_SERVER_MSG(" LLCP is detected");

			net_nfc_service_llcp_process(stand_alone->handle, stand_alone->devType, &error_status);

			malloc_trim(0);
			return true;
		}
		break;

	default :
		{
			DEBUG_SERVER_MSG(" PICC or PCD is detectd.");
			recv_data = net_nfc_service_tag_process(stand_alone->handle, stand_alone->devType, &error_status);
		}
		break;
	}

	if (recv_data != NULL)
	{
		net_nfc_service_msg_processing(recv_data);
	}
	else
	{
		if (((stand_alone->devType == NET_NFC_NFCIP1_INITIATOR) || (stand_alone->devType == NET_NFC_NFCIP1_TARGET)))
		{
			DEBUG_SERVER_MSG("p2p operation. recv data is NULL");
		}
		else
		{
			if (error_status == NET_NFC_NO_NDEF_SUPPORT)
			{
				DEBUG_SERVER_MSG("device type = [%d], it has null data", stand_alone->devType);

				/* launch empty tag */
				uint8_t empty[] = { 0xd0, 0x00, 0x00 }; /* raw-data of empty ndef-message */
				data_s rawdata = { &empty, sizeof(empty) };

				net_nfc_service_msg_processing(&rawdata);
			}
			else
			{
				_net_nfc_service_show_exception_msg("Try again");
			}
		}
	}

	if (stand_alone->devType != NET_NFC_NFCIP1_INITIATOR && stand_alone->devType != NET_NFC_NFCIP1_TARGET)
	{
		net_nfc_request_watch_dog_t* watch_dog_msg = NULL;

		_net_nfc_util_alloc_mem(watch_dog_msg, sizeof(net_nfc_request_watch_dog_t));

		if (watch_dog_msg != NULL)
		{
			watch_dog_msg->length = sizeof(net_nfc_request_watch_dog_t);
			watch_dog_msg->request_type = NET_NFC_MESSAGE_SERVICE_WATCH_DOG;
			watch_dog_msg->devType = stand_alone->devType;
			watch_dog_msg->handle = stand_alone->handle;

			net_nfc_dispatcher_queue_push((net_nfc_request_msg_t *)watch_dog_msg);
		}
	}

	DEBUG_SERVER_MSG("stand alone mode is end");
	malloc_trim(0);

	return true;
}
#endif

bool net_nfc_service_slave_mode_target_detected(net_nfc_request_msg_t *msg)
{
	net_nfc_request_target_detected_t *detail_msg = (net_nfc_request_target_detected_t *)msg;
	net_nfc_error_e result = NET_NFC_OK;
	bool success = true;

	DEBUG_SERVER_MSG("target detected callback for client, device type = [%d]", detail_msg->devType);

	if (detail_msg == NULL)
	{
		return false;
	}

	if (detail_msg->devType != NET_NFC_NFCIP1_TARGET && detail_msg->devType != NET_NFC_NFCIP1_INITIATOR)
	{
		net_nfc_response_tag_discovered_t resp_msg = { 0, };
		int request_type = NET_NFC_MESSAGE_TAG_DISCOVERED;
		data_s *recv_data = NULL;

		if (!net_nfc_controller_connect(detail_msg->handle, &result))
		{
			DEBUG_ERR_MSG("connect failed & Retry Polling!!");

			if (net_nfc_controller_confiure_discovery(NET_NFC_DISCOVERY_MODE_RESUME, NET_NFC_ALL_ENABLE, &result) == false)
			{
				net_nfc_controller_exception_handler();
			}
			return false;
		}

#ifdef BROADCAST_MESSAGE
		net_nfc_server_set_server_state(NET_NFC_TAG_CONNECTED);
#endif
		DEBUG_SERVER_MSG("tag is connected");

		uint8_t ndef_card_state = 0;
		int max_data_size = 0;
		int real_data_size = 0;

		if (net_nfc_controller_check_ndef(detail_msg->handle, &ndef_card_state, &max_data_size, &real_data_size, &result) == true)
		{
			resp_msg.ndefCardState = ndef_card_state;
			resp_msg.maxDataSize = max_data_size;
			resp_msg.actualDataSize = real_data_size;
			resp_msg.is_ndef_supported = 1;
		}

		resp_msg.devType = detail_msg->devType;
		resp_msg.handle = detail_msg->handle;
		resp_msg.number_of_keys = detail_msg->number_of_keys;
		resp_msg.raw_data.length = 0;

		net_nfc_util_duplicate_data(&resp_msg.target_info_values, &detail_msg->target_info_values);

		if (resp_msg.is_ndef_supported)
		{
			DEBUG_SERVER_MSG("support NDEF");

			if ((result = _read_ndef_message(detail_msg->handle, detail_msg->devType, &recv_data)) == NET_NFC_OK &&
				recv_data != NULL)
			{
				DEBUG_SERVER_MSG("net_nfc_controller_read_ndef success");
#ifdef BROADCAST_MESSAGE
				net_nfc_service_msg_processing(recv_data);
#endif
				resp_msg.raw_data.length = recv_data->length;
				success = net_nfc_broadcast_response_msg(request_type, (void *)&resp_msg, sizeof(net_nfc_response_tag_discovered_t),
					(void *)(resp_msg.target_info_values.buffer), resp_msg.target_info_values.length,
					(void *)(recv_data->buffer), recv_data->length, NULL);

				net_nfc_util_free_data(recv_data);
				_net_nfc_util_free_mem(recv_data);
			}
			else
			{
				DEBUG_ERR_MSG("net_nfc_controller_read_ndef failed");

				success = net_nfc_broadcast_response_msg(request_type, (void *)&resp_msg, sizeof(net_nfc_response_tag_discovered_t),
					(void *)(resp_msg.target_info_values.buffer), resp_msg.target_info_values.length, NULL);
			}
		}
		else
		{
			DEBUG_SERVER_MSG("not support NDEF");
#ifdef BROADCAST_MESSAGE
			/* launch empty tag */
			uint8_t empty[] = { 0xd0, 0x00, 0x00 }; /* raw-data of empty ndef-message */
			data_s rawdata = { empty, sizeof(empty) };

			net_nfc_service_msg_processing(&rawdata);
#endif
			success = net_nfc_broadcast_response_msg(request_type, (void *)&resp_msg, sizeof(net_nfc_response_tag_discovered_t),
				(void *)(resp_msg.target_info_values.buffer), resp_msg.target_info_values.length, NULL);
		}

		net_nfc_util_free_data(&resp_msg.target_info_values);

		DEBUG_SERVER_MSG("turn on watch dog");

		net_nfc_request_watch_dog_t* watch_dog_msg = NULL;

		_net_nfc_util_alloc_mem(watch_dog_msg, sizeof(net_nfc_request_watch_dog_t));
		if (watch_dog_msg != NULL)
		{
			watch_dog_msg->length = sizeof(net_nfc_request_watch_dog_t);
			watch_dog_msg->request_type = NET_NFC_MESSAGE_SERVICE_WATCH_DOG;
			watch_dog_msg->devType = detail_msg->devType;
			watch_dog_msg->handle = detail_msg->handle;

			net_nfc_dispatcher_queue_push((net_nfc_request_msg_t *)watch_dog_msg);
		}
	}
	else /* LLCP */
	{
		net_nfc_error_e error_status = NET_NFC_OK;

		net_nfc_service_llcp_process(detail_msg->handle, detail_msg->devType, &error_status);
	}

	/* If target detected, sound should be played. */
	net_nfc_manager_util_play_sound(NET_NFC_TASK_START);

	return success;
}

bool net_nfc_service_termination(net_nfc_request_msg_t* msg)
{
	net_nfc_error_e result;

	if (net_nfc_controller_is_ready(&result) == true)
	{
	}
	else
	{
		DEBUG_SERVER_MSG("Not initialized");
		net_nfc_controller_init(&result);
	}

	if (net_nfc_controller_confiure_discovery(NET_NFC_DISCOVERY_MODE_CONFIG, NET_NFC_ALL_DISABLE, &result) != true)
	{
		DEBUG_SERVER_MSG("failed to discover off %d", result);
	}

	if (net_nfc_controller_set_secure_element_mode(NET_NFC_SE_CMD_UICC_ON, SECURE_ELEMENT_VIRTUAL_MODE, &result) != true)
	{
		DEBUG_SERVER_MSG("failed to set se mode to default mode: %d", result);
	}

	return true;
}

void net_nfc_service_msg_processing(data_s* data)
{
	if (data != NULL)
	{
		net_nfc_app_util_process_ndef(data);
	}
	else
	{
		_net_nfc_service_show_exception_msg("unknown type tag");
	}
}

static void _net_nfc_service_show_exception_msg(char* msg)
{
	bundle *kb = NULL;

	kb = bundle_create();
	bundle_add(kb, "type", "default");
	bundle_add(kb, "text", msg);

	net_nfc_app_util_aul_launch_app("com.samsung.nfc-app", kb); /* empty_tag */

	bundle_free(kb);
}
