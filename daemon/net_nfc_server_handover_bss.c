/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <glib.h>

#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_handover.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_ndef_record.h"
#include "net_nfc_server_llcp.h"
#include "net_nfc_server_handover_bss.h"

static int _net_nfc_handover_bss_process_carrier_record(
		net_nfc_handover_bss_process_context_t *context);

int _net_nfc_handover_bss_convert_security_type(int value)
{
	int retval = 0;
	switch (value)
	{
		case 0x0001: 		/* Open */
			retval = WIFI_SECURITY_TYPE_NONE;
			break;
		case 0x0002: 		/* WPA - Personal */
			retval = WIFI_SECURITY_TYPE_WPA_PSK;
			break;
		case 0x0004:		/* WPA - Shared */
			retval = WIFI_SECURITY_TYPE_WEP;
			break;
		case 0x0008:		/* WPA - Enterprise */
		case 0x0010:		/* WPA2 - Enterprise */
			retval = WIFI_SECURITY_TYPE_EAP;
			break;
		case 0x0020:		/* WPA2 - Personal */
			retval = WIFI_SECURITY_TYPE_WPA_PSK;
			break;
		default:
			DEBUG_ERR_MSG("Invalid security Type");
			retval = 0;
	}
	return retval;
}

int _net_nfc_handover_bss_convert_encryption_type (int enc_type)
{
	int retval = 0;
	switch (enc_type)
	{
		case 0x0001:		/* None */
			retval = WIFI_ENCRYPTION_TYPE_NONE;
			break;
		case 0x0002:		/* WEP */
			retval = WIFI_ENCRYPTION_TYPE_WEP;
			break;
		case 0x0004:		/* TKIP */
			retval = WIFI_ENCRYPTION_TYPE_TKIP;
			break;
		case 0x0008:		/* AES */
			retval = WIFI_ENCRYPTION_TYPE_AES;
			break;
		case 0x000C:		/* TKIP + AES */
			retval = WIFI_ENCRYPTION_TYPE_TKIP_AES_MIXED;
			break;
		default:
			DEBUG_ERR_MSG("Invalid Encryption type");
			retval = 0;
	}
	return retval;
}

wifi_ap_h
_net_nfc_handover_bss_create_ap(net_nfc_carrier_config_s *config)
{
	wifi_ap_h ap_handle = NULL;
	data_s temp = { NULL, 0 };
	int err = WIFI_ERROR_NONE;

	// Sets SSID
	err = net_nfc_util_get_carrier_config_property(config,
		NET_NFC_WIFI_ATTRIBUTE_SSID,(uint16_t *)&temp.length, &temp.buffer);
	DEBUG_SERVER_MSG("SSID = [%s] err %d",temp.buffer, err);
	err = wifi_ap_create((char*)temp.buffer, &ap_handle);
	if(err != WIFI_ERROR_NONE)
	{
		DEBUG_ERR_MSG("Failed to create AP information %d",err);
		goto error;
	}

	// Sets Authentication Type
	net_nfc_util_get_carrier_config_property(config,
	NET_NFC_WIFI_ATTRIBUTE_AUTH_TYPE,(uint16_t *)&temp.length, &temp.buffer);
	if(temp.length == 2)
	{
		uint16_t securitytype = temp.buffer[0] <<8 | temp.buffer[1];
		DEBUG_MSG("wifi_ap_set_security_type %x", securitytype);
		err = wifi_ap_set_security_type(ap_handle,
					_net_nfc_handover_bss_convert_security_type(securitytype));
		if(err != WIFI_ERROR_NONE)
		{
			DEBUG_ERR_MSG("set security type failed");
			goto error;
		}
	}
	else
	{
		DEBUG_ERR_MSG("Invalid authentication length");
		goto error;
	}
	net_nfc_util_get_carrier_config_property(config,
		NET_NFC_WIFI_ATTRIBUTE_NET_KEY,(uint16_t *)&temp.length, &temp.buffer);

	// Sets Network Key
	err = wifi_ap_set_passphrase(ap_handle,(char*)temp.buffer);
	if(err != WIFI_ERROR_NONE)
	{
		DEBUG_ERR_MSG("Failed to set passphrase");
		goto error;
	}
	// Sets encryption Type
	net_nfc_util_get_carrier_config_property(config,
		NET_NFC_WIFI_ATTRIBUTE_ENC_TYPE,(uint16_t *)&temp.length, &temp.buffer);
	if(temp.length == 2)
	{
		uint16_t enc_type = temp.buffer[0] <<8 | temp.buffer[1];
		DEBUG_MSG("wifi_ap_set_encryption_type %x", enc_type);
		err = wifi_ap_set_encryption_type(ap_handle,
					_net_nfc_handover_bss_convert_encryption_type(enc_type));
	}
	else
	{
		DEBUG_ERR_MSG("Invalid Encryption length");
		goto error;
	}
	return ap_handle;
error:
	if(ap_handle != NULL)
	{
		wifi_ap_destroy(ap_handle);
	}
	return NULL;
}

void
_net_nfc_handover_bss_on_wifi_activated(wifi_error_e errorCode, void* user_data)
{
	net_nfc_handover_bss_process_context_t *context = user_data;
	if(context == NULL)
	{
		DEBUG_ERR_MSG("Invalid context");
		context->result = NET_NFC_OPERATION_FAIL;
	}
	if (errorCode == WIFI_ERROR_NONE)
	{
		DEBUG_ERR_MSG("WIFI activated succesfully");
		context->result = NET_NFC_OK;
	}
	else
	{
		DEBUG_ERR_MSG("Failed to activate WIFI");
		context->result = NET_NFC_OPERATION_FAIL;
	}
}

bool
_net_nfc_handover_bss_wifi_for_each_access_point_found(
		wifi_ap_h ap_handle, void *user_data)
{
	data_s temp_ssid = { NULL, 0 };
	int err = WIFI_ERROR_NONE;
	char* essid = NULL;
	net_nfc_handover_bss_process_context_t *context = user_data;

	if(context == NULL)
	{
		DEBUG_ERR_MSG("Invalid context");
		context->result = NET_NFC_OPERATION_FAIL;
		context->step = NET_NFC_LLCP_STEP_RETURN;
		g_idle_add(
			(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
			(gpointer)context);
		return false;
	}

	wifi_ap_clone(&context->ap_handle, ap_handle);
	err = net_nfc_util_get_carrier_config_property(context->config,
			NET_NFC_WIFI_ATTRIBUTE_SSID,(uint16_t *)&temp_ssid.length,
			&temp_ssid.buffer);

	if(err != WIFI_ERROR_NONE)
	{
		DEBUG_ERR_MSG("Wifi get carrier config failed");
		context->result = NET_NFC_OPERATION_FAIL;
		context->step = NET_NFC_LLCP_STEP_RETURN;
		g_idle_add(
			(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
			(gpointer)context);
		return false;
	}

	wifi_ap_get_essid(ap_handle, &essid);
	DEBUG_MSG("Scan Result Ap essid [%s]",essid);
	if(memcmp(temp_ssid.buffer, essid,temp_ssid.length) == 0)
	{
		return false;
	}
	else
	{
		wifi_ap_destroy(context->ap_handle);
		context->ap_handle = NULL;
	}
	return true;
}

void
_net_nfc_handover_bss_on_wifi_bgscan_completed(wifi_error_e errorCode,
		void* user_data)
{
	int err = WIFI_ERROR_NONE;
	net_nfc_handover_bss_process_context_t *context = user_data;

	if(context == NULL)
	{
		DEBUG_ERR_MSG("Invalid context");
		context->result = NET_NFC_OPERATION_FAIL;
		context->step = NET_NFC_LLCP_STEP_RETURN;
		g_idle_add(
			(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
			(gpointer)context);
		return false;
	}

	if(errorCode != WIFI_ERROR_NONE)
	{
		DEBUG_ERR_MSG("Wifi scan failed");
		context->result = NET_NFC_OPERATION_FAIL;
		context->step = NET_NFC_LLCP_STEP_RETURN;
		g_idle_add(
			(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
			(gpointer)context);
	}
	else
	{
		net_nfc_util_create_carrier_config_from_config_record(
			&context->config, context->carrier);
		context->ap_handle = NULL;
		err = wifi_foreach_found_aps(
				_net_nfc_handover_bss_wifi_for_each_access_point_found,
				context);
		if(err != WIFI_ERROR_NONE)
		{
			DEBUG_ERR_MSG("wifi_foreach_found_aps failed Err[%x]",err);
			context->result = NET_NFC_OPERATION_FAIL;
			context->step = NET_NFC_LLCP_STEP_RETURN;
			g_idle_add(
				(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
				(gpointer)context);
		}
		if(context->ap_handle == NULL)
		{
			wifi_encryption_type_e enc_type;
			wifi_security_type_e sec_type;
			DEBUG_MSG("It's Hidden AP");
			context->ap_handle = _net_nfc_handover_bss_create_ap(
					context->config);
			wifi_ap_get_encryption_type(context->ap_handle, &enc_type);
			DEBUG_MSG("Encryption type %x",enc_type);
			wifi_ap_get_security_type(context->ap_handle, &sec_type);
			DEBUG_MSG("Authentication type %x", sec_type);
		}
		else
		{
			data_s temp = { NULL, 0 };
			wifi_encryption_type_e enc_type = WIFI_ENCRYPTION_TYPE_NONE;
			wifi_security_type_e sec_type = WIFI_SECURITY_TYPE_NONE;
			//set passkey
			net_nfc_util_get_carrier_config_property(context->config,
				NET_NFC_WIFI_ATTRIBUTE_NET_KEY,(uint16_t *)&temp.length,
				&temp.buffer);

			DEBUG_ERR_MSG("Network Key %s",temp.buffer);
			// Sets Network Key
			err = wifi_ap_set_passphrase(context->ap_handle,
				(char*)temp.buffer);

			wifi_ap_get_encryption_type(context->ap_handle, &enc_type);
			DEBUG_MSG("Encryption type %x",enc_type);
			wifi_ap_get_security_type(context->ap_handle, &sec_type);
			DEBUG_MSG("Authentication type %x", sec_type);
		}
		context->step = NET_NFC_LLCP_STEP_03;
		g_idle_add(
			(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
			(gpointer)context);

	}
}

void
_net_nfc_handover_bss_on_wifi_scan_completed(wifi_error_e errorCode,
		void* user_data)
{
	int err = WIFI_ERROR_NONE;
	net_nfc_handover_bss_process_context_t *context = user_data;

	if(context == NULL)
	{
		DEBUG_ERR_MSG("Invalid context");
		context->result = NET_NFC_OPERATION_FAIL;
		context->step = NET_NFC_LLCP_STEP_RETURN;
		g_idle_add(
			(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
			(gpointer)context);
		return false;
	}

	if(errorCode != WIFI_ERROR_NONE)
	{
		DEBUG_ERR_MSG("Wifi scan failed");
		context->result = NET_NFC_OPERATION_FAIL;
		context->step = NET_NFC_LLCP_STEP_RETURN;
		g_idle_add(
			(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
			(gpointer)context);
	}
	else
	{
		net_nfc_util_create_carrier_config_from_config_record(
				&context->config, context->carrier);
		context->ap_handle = NULL;
		err = wifi_foreach_found_aps(
			_net_nfc_handover_bss_wifi_for_each_access_point_found,
			context);
		if(err != WIFI_ERROR_NONE)
		{
			DEBUG_ERR_MSG("wifi_foreach_found_aps failed Err[%x]",err);
			context->result = NET_NFC_OPERATION_FAIL;
			context->step = NET_NFC_LLCP_STEP_RETURN;
			g_idle_add(
				(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
				(gpointer)context);

		}
		if(context->ap_handle == NULL)
		{
			wifi_encryption_type_e enc_type;
			wifi_security_type_e sec_type;
			DEBUG_MSG("It's Hidden AP");
			context->ap_handle = _net_nfc_handover_bss_create_ap(
					context->config);
			wifi_ap_get_encryption_type(context->ap_handle, &enc_type);
			DEBUG_MSG("Encryption type %x",enc_type);
			wifi_ap_get_security_type(context->ap_handle, &sec_type);
			DEBUG_MSG("Authentication type %x", sec_type);
		}
		else
		{
			data_s temp = { NULL, 0 };
			wifi_encryption_type_e enc_type = WIFI_ENCRYPTION_TYPE_NONE;
			wifi_security_type_e sec_type = WIFI_SECURITY_TYPE_NONE;
			//set passkey
			net_nfc_util_get_carrier_config_property(context->config,
				NET_NFC_WIFI_ATTRIBUTE_NET_KEY,(uint16_t *)&temp.length,
				&temp.buffer);

			DEBUG_ERR_MSG("Network Key %s",temp.buffer);
			// Sets Network Key
			err = wifi_ap_set_passphrase(context->ap_handle,(char*)temp.buffer);

			wifi_ap_get_encryption_type(context->ap_handle, &enc_type);
			DEBUG_MSG("Encryption type %x",enc_type);
			wifi_ap_get_security_type(context->ap_handle, &sec_type);
			DEBUG_MSG("Authentication type %x", sec_type);
		}
		context->step = NET_NFC_LLCP_STEP_03;
		g_idle_add(
			(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
			(gpointer)context);

	}

}

void
_net_nfc_handover_bss_on_wifi_connected(wifi_error_e errorCode, void* user_data)
{
	net_nfc_handover_bss_process_context_t *context = user_data;

	if(context == NULL)
	{
		DEBUG_ERR_MSG("Invalid context");
		context->result = NET_NFC_OPERATION_FAIL;
		context->step = NET_NFC_LLCP_STEP_RETURN;
		g_idle_add(
			(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
			(gpointer)context);
		return false;
	}

	if(errorCode == WIFI_ERROR_NONE)
	{
		DEBUG_ERR_MSG("WIFI Connected succesfully");
		context->result = NET_NFC_OK;
	}
	else
	{
		DEBUG_ERR_MSG("Failed to connect WIFI");
		context->result = NET_NFC_OPERATION_FAIL;
	}
	context->step = NET_NFC_LLCP_STEP_RETURN;
	g_idle_add(
	(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
	(gpointer)context);
}

static int _net_nfc_handover_bss_process_carrier_record(
		net_nfc_handover_bss_process_context_t *context)
{
	LOGD("[%s:%d] START", __func__, __LINE__);

	if(context == NULL)
	{
		DEBUG_ERR_MSG("Invalid context");
		DEBUG_ERR_MSG("Handover Failed");
		return -1;
	}

	if (context->result != NET_NFC_OK && context->result != NET_NFC_BUSY)
	{
		DEBUG_ERR_MSG("context->result is error"
			" [%d]", context->result);
		context->step = NET_NFC_LLCP_STEP_RETURN;
	}

	switch (context->step)
	{
	case NET_NFC_LLCP_STEP_01 :
	{
		int err = WIFI_ERROR_NONE;
		DEBUG_MSG("STEP [1]");
		err = wifi_initialize();
		/* WIFI returns WIFI_ERROR_INVALID_OPERATION in case already
		 *  initialized*/
		if (err == WIFI_ERROR_NONE || err == WIFI_ERROR_INVALID_OPERATION)
		{
			bool val = false;

			err = wifi_is_activated(&val);
			/* next step */

			if (val == false)
			{
				context->result = NET_NFC_OK;
				wifi_set_background_scan_cb(
					_net_nfc_handover_bss_on_wifi_bgscan_completed, context);
				wifi_activate(
					_net_nfc_handover_bss_on_wifi_activated, context);
			}
			else
			{
				/* do next step */
				DEBUG_MSG("Wifi is enabled already, go next step");
				context->result = NET_NFC_OK;
				context->step = NET_NFC_LLCP_STEP_02;
				g_idle_add((GSourceFunc)
					_net_nfc_handover_bss_process_carrier_record,
					(gpointer)context);
			}
		}
		else
		{
			DEBUG_ERR_MSG("Wifi init failed");
			context->result = NET_NFC_OPERATION_FAIL;
			context->step = NET_NFC_LLCP_STEP_RETURN;
			g_idle_add(
				(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
				(gpointer)context);
		}
	}
	break;

	case NET_NFC_LLCP_STEP_02 :
		{

			int err = WIFI_ERROR_NONE;
			DEBUG_MSG("STEP [2]");
			err = wifi_scan(_net_nfc_handover_bss_on_wifi_scan_completed,
					context);
			if(err != WIFI_ERROR_NONE)
			{
				DEBUG_ERR_MSG("Wifi scan failed");
				context->result = NET_NFC_OPERATION_FAIL;
				context->step = NET_NFC_LLCP_STEP_RETURN;
				g_idle_add(
					(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
					(gpointer)context);
			}
		}

		break;
	case NET_NFC_LLCP_STEP_03 :
		{
			DEBUG_MSG("Connect with WIFI");
			int err = wifi_connect(context->ap_handle,
				_net_nfc_handover_bss_on_wifi_connected, context);
			DEBUG_MSG("Connect with WIFI err [%x]",err);
			if(err != WIFI_ERROR_NONE)
			{
				DEBUG_ERR_MSG("Wifi Connect failed");
				context->result = NET_NFC_OPERATION_FAIL;
				context->step = NET_NFC_LLCP_STEP_RETURN;
				g_idle_add(
					(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
					(gpointer)context);
			}
		}
		break;
	case NET_NFC_LLCP_STEP_RETURN :
		{
			DEBUG_MSG("STEP return");
			if(context->result == NET_NFC_OK)
			{
				DEBUG_MSG("Handover completed succesfully");
			}
			else
			{
				DEBUG_ERR_MSG("Handover Failed");
			}
			wifi_deinitialize();
		}
		break;

	default :
		break;
	}

	LOGD("[%s:%d] END", __func__, __LINE__);

	return 0;
}

net_nfc_error_e net_nfc_server_handover_bss_process_carrier_record(
	ndef_record_s *record,
	net_nfc_server_handover_process_carrier_record_cb cb,
	void *user_param)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_handover_bss_process_context_t *context = NULL;

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	if (context != NULL)
	{
		context->cb = cb;
		context->user_param = user_param;
		context->step = NET_NFC_LLCP_STEP_01;
		net_nfc_util_create_record(record->TNF, &record->type_s,
			&record->id_s, &record->payload_s, &context->carrier);

		g_idle_add(
			(GSourceFunc)_net_nfc_handover_bss_process_carrier_record,
			(gpointer)context);
	}
	else
	{
		result = NET_NFC_ALLOC_FAIL;
	}

	return result;
}
