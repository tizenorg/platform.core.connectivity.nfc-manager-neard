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

#include "bluetooth-api.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_ndef_record.h"
#include "net_nfc_util_handover.h"
#include "net_nfc_server_handover_bt.h"
#include "net_nfc_server_llcp.h"


typedef struct _net_nfc_handover_bt_get_context_t
{
	int step;
	net_nfc_error_e result;
	net_nfc_conn_handover_carrier_state_e cps;
	net_nfc_server_handover_get_carrier_record_cb cb;
	ndef_record_s *carrier;
	uint32_t aux_data_count;
	ndef_record_s *aux_data;
	void *user_param;
}
net_nfc_handover_bt_get_context_t;

typedef struct _net_nfc_handover_bt_process_context_t
{
	int step;
	net_nfc_error_e result;
	net_nfc_server_handover_process_carrier_record_cb cb;
	ndef_record_s *carrier;
	data_s data;
	bluetooth_device_address_t addr;
	void *user_param;
}
net_nfc_handover_bt_process_context_t;


static int _net_nfc_handover_bt_get_carrier_record(
		net_nfc_handover_bt_get_context_t *context);
static int _net_nfc_handover_bt_process_carrier_record(
		net_nfc_handover_bt_process_context_t *context);

static net_nfc_error_e _net_nfc_handover_bt_get_oob_data(
		net_nfc_carrier_config_s *config,
		bt_oob_data_t *oob)
{
	data_s hash = { NULL, 0 };
	data_s randomizer = { NULL, 0 };
	net_nfc_error_e result = NET_NFC_UNKNOWN_ERROR;

	LOGD("[%s:%d] START", __func__, __LINE__);

	RETV_IF(NULL == oob, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == config, NET_NFC_NULL_PARAMETER);

	memset(oob, 0, sizeof(bt_oob_data_t));

	result = net_nfc_util_get_carrier_config_property(
						config,
						NET_NFC_BT_ATTRIBUTE_OOB_HASH_C,
						(uint16_t *)&hash.length,
						&hash.buffer);

	if (NET_NFC_OK == result)
	{
		if (hash.length == 16)
		{
			NFC_DBG("hash.length == 16");

			net_nfc_convert_byte_order(hash.buffer, 16);

			oob->hash_len = MIN(sizeof(oob->hash), hash.length);
			memcpy(oob->hash, hash.buffer, oob->hash_len);
		}
		else
		{
			NFC_ERR("hash.length error : [%d] bytes", hash.length);
		}
	}

	result = net_nfc_util_get_carrier_config_property(
					config,
					NET_NFC_BT_ATTRIBUTE_OOB_HASH_R,
					(uint16_t *)&randomizer.length,
					&randomizer.buffer);

	if (NET_NFC_OK == result)
	{
		if (randomizer.length == 16)
		{
			NFC_DBG("randomizer.length == 16");

			net_nfc_convert_byte_order(randomizer.buffer, 16);

			oob->randomizer_len = MIN(sizeof(oob->randomizer), randomizer.length);
			memcpy(oob->randomizer, randomizer.buffer, oob->randomizer_len);
		}
		else
		{
			NFC_ERR("randomizer.length error :"
					" [%d] bytes", randomizer.length);
		}
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return result;
}

static void _net_nfc_handover_bt_get_carrier_config_cb(int event,
		bluetooth_event_param_t *param, void *user_data)
{
	net_nfc_handover_bt_get_context_t *context =
		(net_nfc_handover_bt_get_context_t *)user_data;

	if (context == NULL)
	{
		NFC_ERR("user_data is null");
		return;
	}

	switch (event)
	{
	case BLUETOOTH_EVENT_ENABLED :
		NFC_DBG("BLUETOOTH_EVENT_ENABLED");
		if (context->step == NET_NFC_LLCP_STEP_02)
		{
			_net_nfc_handover_bt_get_carrier_record(context);
		}
		else
		{
			NFC_ERR("step is incorrect");
		}
		break;

	case BLUETOOTH_EVENT_DISABLED :
		NFC_DBG("BLUETOOTH_EVENT_DISABLED");
		break;

	default :
		NFC_ERR("unhandled bt event [%d],[0x%04x]", event, param->result);
		break;
	}

	LOGD("[%s] END", __func__);
}

static net_nfc_error_e _net_nfc_handover_bt_create_config_record(
		ndef_record_s **record)
{
	bluetooth_device_address_t bt_addr = { { 0, } };
	net_nfc_carrier_config_s *config = NULL;
	net_nfc_error_e result;

	/* append config to ndef message */
	if ((result = bluetooth_get_local_address(&bt_addr))
			== BLUETOOTH_ERROR_NONE)
	{
		if ((result = net_nfc_util_create_carrier_config(
						&config,
						NET_NFC_CONN_HANDOVER_CARRIER_BT)) == NET_NFC_OK)
		{
			bt_oob_data_t oob = { { 0 }, };

			net_nfc_convert_byte_order(bt_addr.addr, 6);

			if ((result = net_nfc_util_add_carrier_config_property(
							config,
							NET_NFC_BT_ATTRIBUTE_ADDRESS,
							sizeof(bt_addr.addr), bt_addr.addr)) != NET_NFC_OK)
			{
				NFC_ERR("net_nfc_util_add_carrier"
						"_config_property failed"
						"[%d]", result);
			}

			/* get oob data */
			if (bluetooth_oob_read_local_data(&oob) == BLUETOOTH_ERROR_NONE)
			{
				if (oob.hash_len == 16 && oob.randomizer_len == 16)
				{
					NFC_DBG("oob.hash_len [%d]", oob.hash_len);
					NFC_DBG("oob.randomizer_len [%d]", oob.randomizer_len);

					net_nfc_convert_byte_order(oob.hash, 16);

					if ((result =
								net_nfc_util_add_carrier_config_property(
									config,
									NET_NFC_BT_ATTRIBUTE_OOB_HASH_C,
									oob.hash_len, oob.hash)) != NET_NFC_OK)
					{
						NFC_ERR("net_nfc_util_add_carrier"
								"_config_property failed"
								" [%d]",result);
					}

					net_nfc_convert_byte_order(oob.randomizer, 16);

					if ((result = net_nfc_util_add_carrier_config_property(
									config,
									NET_NFC_BT_ATTRIBUTE_OOB_HASH_R,
									oob.randomizer_len,
									oob.randomizer)) != NET_NFC_OK)
					{
						NFC_ERR("net_nfc_util_add_carrier"
								"_config_property failed"
								" [%d]",result);
					}
				}
				else
				{
					NFC_ERR("abnormal oob data, skip... [%d]", result);
				}
			}

			if ((result = net_nfc_util_create_ndef_record_with_carrier_config(
							record,
							config)) != NET_NFC_OK)
			{
				NFC_ERR("net_nfc_util_create_ndef_record"
						"_with_carrier_config failed"
						"[%d]",result);
			}

			net_nfc_util_free_carrier_config(config);
		}
		else
		{
			NFC_ERR("net_nfc_util_create_carrier_config failed "
					"[%d]", result);
		}
	}
	else
	{
		NFC_ERR("bluetooth_get_local_address failed"
				" [%d]", result);
		result = NET_NFC_OPERATION_FAIL;
	}

	return result;
}

static int _net_nfc_handover_bt_get_carrier_record(
		net_nfc_handover_bt_get_context_t *context)
{
	LOGD("[%s:%d] START", __func__, __LINE__);

	if (context->result != NET_NFC_OK && context->result != NET_NFC_BUSY)
	{
		NFC_ERR("context->result is error"
				" [%d]", context->result);

		context->step = NET_NFC_LLCP_STEP_RETURN;
	}

	switch (context->step)
	{
	case NET_NFC_LLCP_STEP_01 :
		NFC_DBG("STEP [1]");

		if (bluetooth_register_callback(
					_net_nfc_handover_bt_get_carrier_config_cb,
					context) >= BLUETOOTH_ERROR_NONE)
		{
			context->step = NET_NFC_LLCP_STEP_02;
			context->result = NET_NFC_OK;

			if (bluetooth_check_adapter() != BLUETOOTH_ADAPTER_ENABLED)
			{
				bluetooth_enable_adapter();
			}
			else
			{
				NFC_DBG("bluetooth is enabled already");

				/* do next step */
				g_idle_add((GSourceFunc)
						_net_nfc_handover_bt_get_carrier_record,
						(gpointer)context);
			}
		}
		else
		{
			NFC_ERR("bluetooth_register_callback failed");

			context->step = NET_NFC_LLCP_STEP_RETURN;
			context->result = NET_NFC_OPERATION_FAIL;

			g_idle_add((GSourceFunc)
					_net_nfc_handover_bt_get_carrier_record,
					(gpointer)context);
		}
		break;

	case NET_NFC_LLCP_STEP_02 :
		NFC_DBG("STEP [2]");

		context->step = NET_NFC_LLCP_STEP_RETURN;

		/* append config to ndef message */
		if ((context->result =
					_net_nfc_handover_bt_create_config_record(
						&context->carrier)) != NET_NFC_OK)
		{
			NFC_ERR("_ch_create_bt_config_record failed"
					"[%d]", context->result);
		}

		/* complete and return to upper step */
		g_idle_add((GSourceFunc)
				_net_nfc_handover_bt_get_carrier_record,
				(gpointer)context);
		break;

	case NET_NFC_LLCP_STEP_RETURN :
		NFC_DBG("STEP return");

		/* unregister current callback */
		bluetooth_unregister_callback();

		/* complete and return to upper step */
		context->cb(context->result,
				context->cps,
				context->carrier,
				context->aux_data_count,
				context->aux_data,
				context->user_param);
		break;

	default :
		break;
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return 0;
}

net_nfc_error_e net_nfc_server_handover_bt_get_carrier_record(
		net_nfc_server_handover_get_carrier_record_cb cb,
		void *user_param)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_handover_bt_get_context_t *context = NULL;

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	if (context != NULL)
	{
		context->cb = cb;
		context->user_param = user_param;
		context->step = NET_NFC_LLCP_STEP_01;
		/* TODO : check cps of bt */
		context->cps = NET_NFC_CONN_HANDOVER_CARRIER_ACTIVATE;

		g_idle_add((GSourceFunc)_net_nfc_handover_bt_get_carrier_record,
				(gpointer)context);
	}
	else
	{
		result = NET_NFC_ALLOC_FAIL;
	}

	return result;
}

static bool _net_nfc_handover_bt_check_bond_device(
		bluetooth_device_address_t *address)
{
	bool result = false;
	int ret;
	GPtrArray *devinfo = NULL;

	LOGD("[%s] START", __func__);

	/* allocate the g_pointer_array */
	devinfo = g_ptr_array_new();

	ret = bluetooth_get_bonded_device_list(&devinfo);
	if (ret == BLUETOOTH_ERROR_NONE)
	{
		int i;
		bluetooth_device_info_t *ptr;

		NFC_DBG("g pointer array count : [%d]", devinfo->len);

		for (i = 0; i < devinfo->len; i++)
		{
			ptr = g_ptr_array_index(devinfo, i);
			if (ptr != NULL)
			{
				NFC_SECURE_DBG("Name [%s]", ptr->device_name.name);
				NFC_DBG("Major Class [%d]", ptr->device_class.major_class);
				NFC_DBG("Minor Class [%d]", ptr->device_class.minor_class);
				NFC_DBG("Service Class [%d]", ptr->device_class.service_class);
				NFC_DBG("%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
						ptr->device_address.addr[0],
						ptr->device_address.addr[1],
						ptr->device_address.addr[2],
						ptr->device_address.addr[3],
						ptr->device_address.addr[4],
						ptr->device_address.addr[5]);

				/* compare selector address */
				if (memcmp(&(ptr->device_address),
							address,
							sizeof(ptr->device_address)) == 0)
				{
					NFC_DBG("Found!!!");
					result = true;
					break;
				}
			}
		}
	}
	else
	{
		NFC_ERR("bluetooth_get_bonded_device_list failed with"
				" [%d]", ret);
	}

	/* free g_pointer_array */
	g_ptr_array_free(devinfo, TRUE);

	LOGD("[%s] END", __func__);

	return result;
}

static void _net_nfc_handover_bt_process_carrier_record_cb(
		int event,
		bluetooth_event_param_t *param,
		void *user_data)
{
	net_nfc_handover_bt_process_context_t *context =
		(net_nfc_handover_bt_process_context_t *)user_data;

	if (context == NULL)
	{
		NFC_ERR("user_data is null");
		return;
	}

	switch (event)
	{
	case BLUETOOTH_EVENT_ENABLED :
		NFC_DBG("BLUETOOTH_EVENT_ENABLED");
		if (context->step == NET_NFC_LLCP_STEP_02)
		{
			_net_nfc_handover_bt_process_carrier_record(context);
		}
		else
		{
			NFC_ERR("step is incorrect");
		}
		break;

	case BLUETOOTH_EVENT_DISABLED :
		NFC_DBG("BLUETOOTH_EVENT_DISABLED");
		break;

	case BLUETOOTH_EVENT_BONDING_FINISHED :
		NFC_DBG("BLUETOOTH_EVENT_BONDING_FINISHED, result [0x%04x]", param->result);
		if (context->step == NET_NFC_LLCP_STEP_03)
		{
			if (param->result < BLUETOOTH_ERROR_NONE)
			{
				NFC_ERR("bond failed");
				context->result = NET_NFC_OPERATION_FAIL;
			}

			_net_nfc_handover_bt_process_carrier_record(context);
		}
		else
		{
			NFC_ERR("step is incorrect");
		}
		break;

	default :
		NFC_ERR("unhandled bt event [%d],[0x%04x]", event, param->result);
		break;
	}

	LOGD("[%s] END", __func__);
}

static int _net_nfc_handover_bt_process_carrier_record(
		net_nfc_handover_bt_process_context_t *context)
{
	LOGD("[%s:%d] START", __func__, __LINE__);

	if (context->result != NET_NFC_OK && context->result != NET_NFC_BUSY)
	{
		NFC_ERR("context->result is error"
				" [%d]", context->result);
		context->step = NET_NFC_LLCP_STEP_RETURN;
	}

	switch (context->step)
	{
	case NET_NFC_LLCP_STEP_01 :
		NFC_DBG("STEP [1]");

		if (bluetooth_register_callback(
					_net_nfc_handover_bt_process_carrier_record_cb,
					context) >= BLUETOOTH_ERROR_NONE)
		{
			/* next step */
			context->step = NET_NFC_LLCP_STEP_02;

			if (bluetooth_check_adapter() != BLUETOOTH_ADAPTER_ENABLED)
			{
				context->result = NET_NFC_OK;
				bluetooth_enable_adapter();
			}
			else
			{
				/* do next step */
				NFC_DBG("BT is enabled already, go next step");
				context->result = NET_NFC_OK;

				g_idle_add((GSourceFunc)
						_net_nfc_handover_bt_process_carrier_record,
						(gpointer)context);
			}
		}
		else
		{
			NFC_ERR("bluetooth_register_callback failed");
			context->result = NET_NFC_OPERATION_FAIL;

			g_idle_add(
					(GSourceFunc)_net_nfc_handover_bt_process_carrier_record,
					(gpointer)context);
		}
		break;

	case NET_NFC_LLCP_STEP_02 :
		{
			net_nfc_carrier_config_s *config;
			data_s temp = { NULL, 0 };

			NFC_DBG("STEP [2]");

			net_nfc_util_create_carrier_config_from_config_record(
					&config,
					context->carrier);

			net_nfc_util_get_carrier_config_property(config,
					NET_NFC_BT_ATTRIBUTE_ADDRESS,
					(uint16_t *)&temp.length, &temp.buffer);

			if (temp.length == 6)
			{
				net_nfc_convert_byte_order(temp.buffer, 6);

				memcpy(context->addr.addr,
						temp.buffer,
						MIN(sizeof(context->addr.addr),
							temp.length));

				if (_net_nfc_handover_bt_check_bond_device
						(&context->addr) == true)
				{
					NFC_DBG("already paired with [%02x:%02x:%02x:%02x:%02x:%02x]",
							context->addr.addr[0],
							context->addr.addr[1],
							context->addr.addr[2],
							context->addr.addr[3],
							context->addr.addr[4],
							context->addr.addr[5]);

					/* return */
					context->step = NET_NFC_LLCP_STEP_RETURN;
					context->result = NET_NFC_OK;
				}
				else
				{
					bt_oob_data_t oob = { { 0 } , };

					if (_net_nfc_handover_bt_get_oob_data(
								config,
								&oob) == NET_NFC_OK)
					{
						/* set oob data */
						bluetooth_oob_add_remote_data(
								&context->addr,
								&oob);
					}

					/* pair and send reponse */
					context->result = NET_NFC_OK;
					context->step = NET_NFC_LLCP_STEP_RETURN;
				}
			}
			else
			{
				NFC_ERR("bluetooth address is invalid."
						" [%d] bytes", temp.length);

				context->step = NET_NFC_LLCP_STEP_RETURN;
				context->result = NET_NFC_OPERATION_FAIL;
			}

			net_nfc_util_free_carrier_config(config);

			g_idle_add(
					(GSourceFunc)_net_nfc_handover_bt_process_carrier_record,
					(gpointer)context);
		}
		break;

	case NET_NFC_LLCP_STEP_RETURN :
		{
			NFC_DBG("STEP return");
			data_s data = { context->addr.addr,
				sizeof(context->addr.addr) };

			/* unregister bluetooth callback */
			bluetooth_unregister_callback();

			context->cb(context->result,
					NET_NFC_CONN_HANDOVER_CARRIER_BT,
					&data,
					context->user_param);
		}
		break;

	default :
		break;
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return 0;
}

net_nfc_error_e net_nfc_server_handover_bt_process_carrier_record(
		ndef_record_s *record,
		net_nfc_server_handover_process_carrier_record_cb cb,
		void *user_param)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_handover_bt_process_context_t *context = NULL;

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	if (context != NULL)
	{
		context->cb = cb;
		context->user_param = user_param;
		context->step = NET_NFC_LLCP_STEP_01;
		net_nfc_util_create_record(record->TNF, &record->type_s,
				&record->id_s, &record->payload_s, &context->carrier);

		g_idle_add(
				(GSourceFunc)_net_nfc_handover_bt_process_carrier_record,
				(gpointer)context);
	}
	else
	{
		result = NET_NFC_ALLOC_FAIL;
	}

	return result;
}

static void _net_nfc_handover_bt_post_process_cb(int event,
		bluetooth_event_param_t *param, void *user_data)
{
	net_nfc_handover_bt_process_context_t *context =
		(net_nfc_handover_bt_process_context_t *)user_data;

	if (context == NULL)
	{
		NFC_ERR("user_data is null");
		return;
	}

	switch (event)
	{
	case BLUETOOTH_EVENT_ENABLED :
		NFC_DBG("BLUETOOTH_EVENT_ENABLED");
		break;

	case BLUETOOTH_EVENT_DISABLED :
		NFC_DBG("BLUETOOTH_EVENT_DISABLED");
		break;

	case BLUETOOTH_EVENT_BONDING_FINISHED :
		NFC_DBG("BLUETOOTH_EVENT_BONDING_FINISHED, result [0x%04x]",param->result);

		if (param->result < BLUETOOTH_ERROR_NONE)
		{
			NFC_ERR("bond failed");
			context->result = NET_NFC_OPERATION_FAIL;
		}
		else
		{
			context->result = NET_NFC_OK;
		}

		context->cb(context->result,
				NET_NFC_CONN_HANDOVER_CARRIER_BT,
				NULL,
				context->user_param);
		break;

	default :
		NFC_ERR("unhandled bt event [%d], [0x%04x]", event, param->result);
		break;
	}

	LOGD("[%s] END", __func__);
}

net_nfc_error_e net_nfc_server_handover_bt_post_process(
		data_s *data,
		net_nfc_server_handover_process_carrier_record_cb cb,
		void *user_param)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_handover_bt_process_context_t *context = NULL;

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	if (context != NULL)
	{
		bluetooth_device_address_t bt_addr;

		context->cb = cb;
		context->user_param = user_param;

		memcpy(&bt_addr.addr, data->buffer, sizeof(bt_addr.addr));

		if (bluetooth_register_callback(
					_net_nfc_handover_bt_post_process_cb,
					context) >= BLUETOOTH_ERROR_NONE)
		{
			bluetooth_bond_device(&bt_addr);
		}
		else
		{
			_net_nfc_util_free_mem(context);
			result = NET_NFC_OPERATION_FAIL;
		}
	}
	else
	{
		result = NET_NFC_ALLOC_FAIL;
	}

	return result;
}

