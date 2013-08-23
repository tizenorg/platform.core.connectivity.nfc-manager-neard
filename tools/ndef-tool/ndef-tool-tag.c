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


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include "net_nfc.h"
#include "net_nfc_exchanger.h"
#include "ndef-tool.h"

typedef struct _response_context_t
{
	int type;
	void *user_param;
} response_context_t;

static GMainLoop *main_loop = NULL;
static response_context_t response_context = { 0, };

static void _tag_read_completed_cb(ndef_message_h msg, void *user_data)
{
	response_context_t *context = (response_context_t *)user_data;

	fprintf(stdout, "read complete!!!\n");
	if (msg != NULL)
	{
		ndef_tool_write_ndef_message_to_file((char *)context->user_param, msg);

		ndef_tool_display_ndef_message_from_file((char *)context->user_param);
	}

	g_main_loop_quit(main_loop);
}

static void _tag_read_cb(net_nfc_target_handle_h handle, void *user_data)
{
	fprintf(stdout, "\nreading...\n\n");

	net_nfc_read_tag(handle, user_data);
}

static void _tag_write_completed_cb(net_nfc_error_e result, void *user_data)
{
	if (result == NET_NFC_OK)
		fprintf(stdout, "write success!!!\n\n");
	else
		fprintf(stdout, "write failed.\n\n");

	g_main_loop_quit(main_loop);
}

static void _tag_write_cb(net_nfc_target_handle_h handle, void *user_data)
{
	response_context_t *context = (response_context_t *)user_data;

	fprintf(stdout, "\nwriting...\n\n");

	net_nfc_write_ndef(handle, (ndef_message_h)context->user_param, user_data);
}

static void _p2p_receive_completed_cb(data_h data, void *user_data)
{
	response_context_t *context = (response_context_t *)user_data;

	fprintf(stdout, "\np2p receive complete!!!\n\n");
	if (data != NULL)
	{
		ndef_message_h msg;

		net_nfc_create_ndef_message_from_rawdata(&msg, data);

		ndef_tool_write_ndef_message_to_file((char *)context->user_param, msg);

		net_nfc_free_ndef_message(msg);

		ndef_tool_display_ndef_message_from_file((char *)context->user_param);
	}

	g_main_loop_quit(main_loop);
}

static void _p2p_send_completed_cb(net_nfc_error_e result, void *user_data)
{
	if (result == NET_NFC_OK)
		fprintf(stdout, "send success!!!\n\n");
	else
		fprintf(stdout, "send failed.\n\n");

	g_main_loop_quit(main_loop);
}

static void _p2p_send_cb(net_nfc_target_handle_h handle, void *user_data)
{
	response_context_t *context = (response_context_t *)user_data;

	fprintf(stdout, "\nsending...\n\n");

	net_nfc_exchanger_data_h data_handle;
	data_h rawdata;

	net_nfc_create_rawdata_from_ndef_message((ndef_message_h)context->user_param, &rawdata);
	net_nfc_create_exchanger_data(&data_handle,  rawdata);
	net_nfc_free_data(rawdata);

	net_nfc_send_exchanger_data(data_handle, handle, user_data);
	net_nfc_free_exchanger_data(data_handle);
}

static void _handover_completed_cb(net_nfc_error_e result,
		data_h data, void *user_data)
{
	//	response_context_t *context = (response_context_t *)user_data;
	//	data_h rawdata;

	if (result == NET_NFC_OK)
		fprintf(stdout, "handover success!!!\n\n");
	else
		fprintf(stdout, "handover failed.\n\n");

	//	net_nfc_create_rawdata_from_ndef_message((ndef_message_h)context->user_param, &rawdata);
	//
	//	net_nfc_ex
	g_main_loop_quit(main_loop);
}

static void _open_se_cb(net_nfc_error_e result, net_nfc_target_handle_h handle,
		void *user_data)
{
	response_context_t *context = (response_context_t *)user_data;

	if (result == NET_NFC_OK)
	{
		data_h data = NULL;

		fprintf(stdout, "net_nfc_open_internal_secure_element success!!!\n\n");

		net_nfc_create_data(&data, (uint8_t *)context->user_param,
				context->type);
		if (data != NULL) {
			net_nfc_send_apdu(handle, data, handle);
			net_nfc_free_data(data);
		}
	}
	else
	{
		fprintf(stdout, "net_nfc_open_internal_secure_element failed.\n\n");
		g_main_loop_quit(main_loop);
	}
}

static void _send_apdu_se_cb(net_nfc_error_e result, net_nfc_target_handle_h handle, void *user_data)
{
	//	response_context_t *context = (response_context_t *)user_data;
	//	data_h rawdata;

	if (result == NET_NFC_OK)
	{
		fprintf(stdout, "net_nfc_send_apdu success!!!\n\n");
		net_nfc_close_internal_secure_element(handle, user_data);
	}
	else
	{
		fprintf(stdout, "net_nfc_send_apdu failed.\n\n");
		g_main_loop_quit(main_loop);
	}
}

static void _close_se_cb(net_nfc_error_e result, void *user_data)
{
	if (result == NET_NFC_OK)
		fprintf(stdout, "net_nfc_close_internal_secure_element success!!!\n\n");
	else
		fprintf(stdout, "net_nfc_close_internal_secure_element failed.\n\n");

	g_main_loop_quit(main_loop);
}

static void _handover_cb(net_nfc_target_handle_h handle, void *user_data)
{
	fprintf(stdout, "\ntry to handover...\n\n");

	net_nfc_exchanger_request_connection_handover(handle,
			NET_NFC_CONN_HANDOVER_CARRIER_BT);
}

void _nfc_response_cb(net_nfc_message_e message, net_nfc_error_e result,
		void *data, void *user_param, void *trans_data)
{
	response_context_t *context = (response_context_t *)user_param;

	switch (message)
	{
	case NET_NFC_MESSAGE_TAG_DISCOVERED :
		{
			net_nfc_target_handle_h handle = NULL;
			bool is_ndef = false;

			net_nfc_get_tag_handle((net_nfc_target_info_h)data, &handle);
			net_nfc_get_tag_ndef_support((net_nfc_target_info_h)data, &is_ndef);

			ndef_tool_display_discovered_tag(data);

			if (is_ndef == true)
			{
				if (context->type == 0) /* read */
				{
					_tag_read_cb(handle, user_param);
				}
				else
				{
					_tag_write_cb(handle, user_param);
				}
			}
			else
			{
				fprintf(stdout, "No NDEF tag.. read failed.\n\n");
				g_main_loop_quit(main_loop);
			}
		}
		break;

	case NET_NFC_MESSAGE_READ_NDEF :
		_tag_read_completed_cb((ndef_message_h)data, user_param);
		break;

	case NET_NFC_MESSAGE_WRITE_NDEF :
		_tag_write_completed_cb(result, user_param);
		break;

	case NET_NFC_MESSAGE_P2P_DISCOVERED :

		ndef_tool_display_discovered_target(data);

		if (context->type == 1) /* receive */
		{
			_p2p_send_cb((net_nfc_target_handle_h)data, user_param);
		}
		else if (context->type == 2) /* handover */
		{
			_handover_cb((net_nfc_target_handle_h)data, user_param);
		}
		break;

	case NET_NFC_MESSAGE_P2P_SEND :
		_p2p_send_completed_cb(result, user_param);
		break;

	case NET_NFC_MESSAGE_P2P_RECEIVE :
		_p2p_receive_completed_cb(data, user_param);
		break;

	case NET_NFC_MESSAGE_CONNECTION_HANDOVER :
		_handover_completed_cb(result, data, user_param);
		break;

	case NET_NFC_MESSAGE_OPEN_INTERNAL_SE :
		_open_se_cb(result, data, user_param);
		break;

	case NET_NFC_MESSAGE_SEND_APDU_SE :
		_send_apdu_se_cb(result, trans_data, user_param);
		break;

	case NET_NFC_MESSAGE_CLOSE_INTERNAL_SE :
		_close_se_cb(result, user_param);
		break;

	default :
		break;
	}
}

static void _initialize_tag_context(response_context_t *context)
{
	int ret = 0;

	ret = net_nfc_initialize();
	if (ret == NET_NFC_OK)
	{
		net_nfc_set_response_callback(_nfc_response_cb, (void *)context);
	}
}

static void _run_tag_action()
{
	main_loop = g_main_new(TRUE);
	g_main_loop_run(main_loop);
}

static void _release_tag_context(void)
{
	net_nfc_unset_response_callback();

	net_nfc_deinitialize();
}

int ndef_tool_read_ndef_from_tag(const char *file)
{
	int result = 0;

	response_context.type = 0;
	response_context.user_param = (void *)file;

	_initialize_tag_context(&response_context);

	fprintf(stdout, "Contact a tag to device.....\n");

	_run_tag_action();

	_release_tag_context();

	return result;
}

int ndef_tool_receive_ndef_via_p2p(const char *file)
{
	int result = 0;

	response_context.type = 0;
	response_context.user_param = (void *)file;

	_initialize_tag_context(&response_context);

	fprintf(stdout, "Contact a target to device.....\n");

	_run_tag_action();

	_release_tag_context();

	return result;
}

int ndef_tool_write_ndef_to_tag(const char *file)
{
	int result = 0;
	ndef_message_h msg = NULL;

	if (ndef_tool_read_ndef_message_from_file(file, &msg) > 0)
	{
		response_context.type = 1;
		response_context.user_param = (void *)msg;

		_initialize_tag_context(&response_context);

		fprintf(stdout, "Contact a tag to device.....\n");

		_run_tag_action();

		net_nfc_free_ndef_message(msg);

		_release_tag_context();
	}

	return result;
}

int ndef_tool_send_ndef_via_p2p(const char *file)
{
	int result = 0;
	ndef_message_h msg = NULL;

	if (ndef_tool_read_ndef_message_from_file(file, &msg) > 0)
	{
		response_context.type = 1;
		response_context.user_param = (void *)msg;

		_initialize_tag_context(&response_context);

		fprintf(stdout, "Contact a target to device.....\n");

		_run_tag_action();

		net_nfc_free_ndef_message(msg);

		_release_tag_context();
	}

	return result;
}

static int _make_file_to_ndef_message(ndef_message_h *msg, const char *file_name)
{
	int result = 0;
	FILE *file = NULL;

	file = fopen(file_name, "rb");
	if (file != NULL)
	{
		long int file_size = 0;
		size_t read = 0;

		fseek(file, 0, SEEK_END);
		file_size = ftell(file);
		fseek(file, 0, SEEK_SET);

		if (file_size > 0)
		{
			data_h data;

			net_nfc_create_data(&data, NULL, file_size);
			if (data != NULL)
			{
				ndef_record_h record;
				data_h type;

				read = fread((void *)net_nfc_get_data_buffer(data), 1, file_size, file);

				net_nfc_create_ndef_message(msg);

				net_nfc_create_data(&type, (uint8_t *)"image/jpeg", 10);

				net_nfc_create_record(&record, NET_NFC_RECORD_MIME_TYPE, type, NULL, data);

				net_nfc_append_record_to_ndef_message(*msg, record);

				net_nfc_free_data(type);
				net_nfc_free_data(data);

				result = file_size;
			}
		}

		fclose(file);
	}

	return result;
}

int ndef_tool_connection_handover(const char *file)
{
	int result = 0;
	ndef_message_h msg = NULL;

	if (_make_file_to_ndef_message(&msg, file) > 0)
	{
		response_context.type = 2;
		response_context.user_param = (void *)msg;

		_initialize_tag_context(&response_context);

		fprintf(stdout, "Contact a target to device.....\n");

		_run_tag_action();

		net_nfc_free_ndef_message(msg);

		_release_tag_context();
	}

	return result;
}

static unsigned char char_to_num[] =
{
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

int _convert_string_to_hex(const char *str, unsigned char *buffer, size_t length)
{
	size_t i, j, len = strlen(str);

	for (i = 0, j = 0; i < len; j++)
	{
		buffer[j] = (char_to_num[(unsigned char)str[i++]] << 4);
		if (i < len)
		{
			buffer[j] |= char_to_num[(unsigned char)str[i++]];
		}
	}

	return (int)j;
}

int ndef_tool_send_apdu(const char *apdu)
{
	int result = 0;
	unsigned char *buffer;
	unsigned int length = (strlen(apdu) >> 1) + 1;

	buffer = calloc(1, length);
	if (buffer != NULL)
	{
		length = _convert_string_to_hex(apdu, buffer, length);
		if (length > 0)
		{
			response_context.type = length;
			response_context.user_param = (void *)buffer;

			_initialize_tag_context(&response_context);

			fprintf(stdout, "try to open eSE.....\n");

			net_nfc_open_internal_secure_element(NET_NFC_SE_TYPE_ESE, buffer);

			_run_tag_action();


			_release_tag_context();
		}

		free(buffer);
	}

	return result;
}
