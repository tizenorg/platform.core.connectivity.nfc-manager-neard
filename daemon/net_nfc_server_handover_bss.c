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
#include <execinfo.h>

static gboolean _net_nfc_handover_bss_process_carrier_record(
		gpointer user_data);

static void _net_nfc_handover_bss_get_carrier_record(
		net_nfc_handover_bss_create_context_t *context);

static net_nfc_error_e _net_nfc_handover_bss_create_carrier_record(
		ndef_record_s **record);

#ifdef TARGET
static net_nfc_error_e _net_nfc_handover_bss_create_config_record(
		ndef_record_s **record);

static int _net_nfc_handover_process_wifi_direct_setup(
		net_nfc_handover_bss_get_context_t *context);

static gboolean _net_nfc_handover_bss_wfd_get_carrier_record(
		net_nfc_handover_bss_get_context_t *context);

static void _net_nfc_wifi_process_error(int error);

static int _net_nfc_handover_process_wifi_group_setup(
		net_nfc_handover_bss_get_context_t *context);

static int _net_nfc_handover_getpassword(uint8_t** password );
#endif


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
		return;
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
		g_idle_add(_net_nfc_handover_bss_process_carrier_record,
				context);
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
_net_nfc_handover_bss_on_wifi_bgscan_completed(wifi_error_e error_code,
		void* user_data)
{

	net_nfc_handover_bss_process_context_t *context = user_data;

	if(context == NULL)
	{
		DEBUG_ERR_MSG("Invalid context");
		return;
	}
	if(error_code != WIFI_ERROR_NONE)
	{
		DEBUG_ERR_MSG("Wifi scan failed");
		context->result = NET_NFC_OPERATION_FAIL;
		context->step = NET_NFC_LLCP_STEP_RETURN;
		g_idle_add(
				_net_nfc_handover_bss_process_carrier_record,
				context);
	}
	else
	{
		context->result = NET_NFC_OK;
		context->step = NET_NFC_LLCP_STEP_02;

	}
}

void
_net_nfc_handover_bss_on_wifi_scan_completed(wifi_error_e error_code,
		void* user_data)
{
	int err = WIFI_ERROR_NONE;
	net_nfc_handover_bss_process_context_t *context = user_data;

	if(error_code != WIFI_ERROR_NONE)
	{
		DEBUG_ERR_MSG("Wifi scan failed");
		context->result = NET_NFC_OPERATION_FAIL;
		context->step = NET_NFC_LLCP_STEP_RETURN;
		g_idle_add(
				_net_nfc_handover_bss_process_carrier_record,
				context);
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
					_net_nfc_handover_bss_process_carrier_record,
					context);

		}
		if(context->ap_handle == NULL)
		{
			wifi_encryption_type_e enc_type;
			wifi_security_type_e sec_type;

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
				_net_nfc_handover_bss_process_carrier_record,
				context);

	}

}

void
_net_nfc_handover_bss_on_wifi_connected(wifi_error_e error_code, void* user_data)
{
	net_nfc_handover_bss_process_context_t *context = user_data;

	if(context == NULL)
	{
		DEBUG_ERR_MSG("Invalid context");
		return;
	}

	if(error_code == WIFI_ERROR_NONE)
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
			_net_nfc_handover_bss_process_carrier_record,
			context);
}

static gboolean _net_nfc_handover_bss_process_carrier_record(
		gpointer user_data)
{
	LOGD("[%s:%d] START", __func__, __LINE__);

	net_nfc_handover_bss_process_context_t *context = user_data;

	if(context == NULL)
	{
		DEBUG_ERR_MSG("Invalid context");
		DEBUG_ERR_MSG("Handover Failed");
		return 0;
	}

	if (context->result != NET_NFC_OK && context->result != NET_NFC_BUSY)
	{
		DEBUG_ERR_MSG("context->result is error [%d]", context->result);
		context->step = NET_NFC_LLCP_STEP_RETURN;
	}

	switch (context->step)
	{
	case NET_NFC_LLCP_STEP_01:
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
					g_idle_add(_net_nfc_handover_bss_process_carrier_record,
							context);
				}
			}
			else
			{
				DEBUG_ERR_MSG("Wifi init failed");
				context->result = NET_NFC_OPERATION_FAIL;
				context->step = NET_NFC_LLCP_STEP_RETURN;
				g_idle_add(
						_net_nfc_handover_bss_process_carrier_record,
						context);
			}
		}
		break;

	case NET_NFC_LLCP_STEP_02:
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
						_net_nfc_handover_bss_process_carrier_record,
						context);
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
						_net_nfc_handover_bss_process_carrier_record,
						context);
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
			context->cb(context->result,
					NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS,
					NULL,
					context->user_param);
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
				_net_nfc_handover_bss_process_carrier_record,
				context);
	}
	else
	{
		result = NET_NFC_ALLOC_FAIL;
	}
	return result;
}

#ifdef TARGET
void
_net_nfc_wifi_direct_power_changed(int err, wifi_direct_device_state_e device_state, void* user_data)
{

	if(device_state == WIFI_DIRECT_DEVICE_STATE_ACTIVATED && err == WIFI_DIRECT_ERROR_NONE)
	{
		int err = wifi_direct_start_discovery(true, 0);
		DEBUG_MSG("wifi direct is enabled %d",err);
	}
	else
	{
		DEBUG_ERR_MSG("wifi direct power state error");
		net_nfc_handover_bss_create_context_t* context =
			(net_nfc_handover_bss_create_context_t*)user_data;

		context->step = NET_NFC_LLCP_STEP_RETURN;
		context->result = NET_NFC_OPERATION_FAIL;
		g_idle_add((GSourceFunc)
				_net_nfc_handover_bss_wfd_get_carrier_record,
				(gpointer)context);
	}
}

void
_net_nfc_wifi_scan_completed_cb(int err, wifi_direct_discovery_state_e discovery_state, void* user_data)
{

	net_nfc_handover_bss_create_context_t* context =
		(net_nfc_handover_bss_create_context_t*) user_data;

	if(discovery_state == WIFI_DIRECT_ONLY_LISTEN_STARTED && err == WIFI_DIRECT_ERROR_NONE)
	{
		g_idle_add((GSourceFunc)
				_net_nfc_handover_bss_wfd_get_carrier_record,
				(gpointer)context);
	}
	else
	{
		DEBUG_ERR_MSG("wifi scan error");
		context->step = NET_NFC_LLCP_STEP_RETURN;
		context->result = NET_NFC_OPERATION_FAIL;
		g_idle_add((GSourceFunc)
				_net_nfc_handover_bss_wfd_get_carrier_record,
				(gpointer)context);
	}

}

void
_net_nfc_wifi_direct_connection_changed(wifi_direct_error_e error_code,
		wifi_direct_connection_state_e connection_state,
		const char* mac_address,
		void *user_data)
{

	net_nfc_handover_bss_create_context_t* context =
		(net_nfc_handover_bss_create_context_t*) user_data;

	if(connection_state == WIFI_DIRECT_GROUP_CREATED
			&& error_code == WIFI_DIRECT_ERROR_NONE)
	{
		g_idle_add((GSourceFunc)
				_net_nfc_handover_bss_wfd_get_carrier_record,
				(gpointer)context);
	}
	else
	{
		DEBUG_ERR_MSG("_net_nfc_wifi_direct_connection_changed failed"
				"[%d] [%d]",error_code,connection_state);

		context->step = NET_NFC_LLCP_STEP_RETURN;
		context->result = NET_NFC_OPERATION_FAIL;
		g_idle_add((GSourceFunc)
				_net_nfc_handover_bss_wfd_get_carrier_record,
				(gpointer)context);
	}
}

#endif

static net_nfc_error_e _net_nfc_handover_bss_create_carrier_record(
		ndef_record_s **record)
{
	net_nfc_error_e result;

	if ((result = net_nfc_util_create_handover_carrier_record(
					record)) == NET_NFC_OK)
	{
		DEBUG_ERR_MSG("net_nfc_util_create_ndef_record"
				"_with_carrier_config [%d]",result);
	}
	else
	{
		DEBUG_ERR_MSG("net_nfc_util_create_carrier_config failed "
				"[%d]", result);
	}

	return result;
}


#ifdef TARGET
static int _net_nfc_handover_getpassword(uint8_t** password )
{

	char data[256];
	int ret_val;

	ret_val = system("wpa_cli -g /var/run/wpa_supplicant/p2p-wlan0-0 p2p_get_passphrase "
			"> /tmp/nfc_p2p_passphrase.txt");

	DEBUG_MSG("system command returned with [%d]",ret_val);

	FILE *f = fopen("/tmp/nfc_p2p_passphrase.txt","r");
	if(f != NULL)
	{
		int readlength;
		int cnt;
		readlength = fread(data, 1 , 255, f);
		for(cnt = 0; cnt < readlength; cnt++)
		{
			if(data[cnt] == '\n')
			{
				break;
			}
		}
		_net_nfc_util_alloc_mem(*password,(readlength - cnt)+1);
		memcpy(*password, &data[cnt+1], (readlength - cnt));
		fclose(f);
		return (readlength-cnt);
	}
	else
		return 0;
}


static net_nfc_error_e _net_nfc_handover_bss_create_config_record(
		ndef_record_s **record)
{
	char* mac_address = NULL;
	char* ssid = NULL;
	net_nfc_carrier_config_s *config = NULL;
	net_nfc_carrier_property_s *cred_config = NULL;
	net_nfc_error_e result;
	uint8_t *password = NULL;
	int pw_length = 0;

	data_s version_data = {NULL,0};
	data_s net_index_data = {NULL,0};
	data_s ssid_data = {NULL,0};
	data_s auth_data = {NULL,0};
	data_s enc_type = {NULL,0};
	data_s mac_data = {NULL,0};

	_net_nfc_util_alloc_mem(version_data.buffer,1);
	if(version_data.buffer)
	{
		version_data.length = 1;
		*version_data.buffer = 0x01;

		if ((result = net_nfc_util_create_carrier_config(
						&config,
						NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS)) == NET_NFC_OK)
		{
			if ((result = net_nfc_util_add_carrier_config_property(
							config,
							NET_NFC_WIFI_ATTRIBUTE_VERSION,
							version_data.length, version_data.buffer)) != NET_NFC_OK)
			{
				DEBUG_ERR_MSG("net_nfc_util_add_carrier_config_property failed"
						"[%d]", result);
			}
		}

		_net_nfc_util_free_mem(version_data.buffer);
	}

	if ((result = net_nfc_util_create_carrier_config_group(
					&cred_config,
					NET_NFC_WIFI_ATTRIBUTE_CREDENTIAL)) == NET_NFC_OK)
	{

		_net_nfc_util_alloc_mem(net_index_data.buffer,1);
		if(net_index_data.buffer)
		{
			net_index_data.length = 1;
			*net_index_data.buffer = 1;


			net_nfc_util_add_carrier_config_group_property(
					cred_config,
					NET_NFC_WIFI_ATTRIBUTE_NET_INDEX,
					net_index_data.length, net_index_data.buffer);

			_net_nfc_util_free_mem(net_index_data.buffer);
		}

		wifi_direct_get_ssid(&ssid);

		_net_nfc_util_alloc_mem(ssid_data.buffer,strlen(ssid)+2);
		if(ssid_data.buffer)
		{
			ssid_data.length = strlen(ssid);
			memcpy(ssid_data.buffer,ssid, strlen(ssid));

			net_nfc_util_add_carrier_config_group_property(
					cred_config,
					NET_NFC_WIFI_ATTRIBUTE_SSID,
					ssid_data.length, ssid_data.buffer);

			_net_nfc_util_free_mem(version_data.buffer);
		}

		_net_nfc_util_alloc_mem(auth_data.buffer,2);
		if(auth_data.buffer)
		{
			auth_data.length = 1;
			*auth_data.buffer = 20;

			net_nfc_util_add_carrier_config_group_property(
					cred_config,
					NET_NFC_WIFI_ATTRIBUTE_AUTH_TYPE,
					auth_data.length, auth_data.buffer);

			_net_nfc_util_free_mem(auth_data.buffer);
		}

		_net_nfc_util_alloc_mem(enc_type.buffer,2);
		if(enc_type.buffer)
		{
			enc_type.length = 1;
			*enc_type.buffer = 8;

			net_nfc_util_add_carrier_config_group_property(
					cred_config,
					NET_NFC_WIFI_ATTRIBUTE_ENC_TYPE,
					enc_type.length, enc_type.buffer);

			_net_nfc_util_free_mem(enc_type.buffer);
		}

		pw_length = _net_nfc_handover_getpassword(&password);

		DEBUG_ERR_MSG("_net_nfc_handover_getpassword[%s]", password);

		net_nfc_util_add_carrier_config_group_property(
				cred_config,
				NET_NFC_WIFI_ATTRIBUTE_NET_KEY,
				pw_length, password);

		_net_nfc_util_free_mem(password);

		wifi_direct_get_mac_address(&mac_address);

		_net_nfc_util_alloc_mem(mac_data.buffer,strlen(mac_address)+2);
		if(mac_data.buffer)
		{
			mac_data.length = strlen(mac_address);
			memcpy(mac_data.buffer,mac_address,strlen(mac_address));

			if ((result = net_nfc_util_add_carrier_config_group_property(
							cred_config,
							NET_NFC_WIFI_ATTRIBUTE_MAC_ADDR,
							mac_data.length, mac_data.buffer)) != NET_NFC_OK)
			{
				DEBUG_ERR_MSG("net_nfc_util_add_carrier"
						"_config_property failed"
						"[%d]", result);
			}
		}

		_net_nfc_util_free_mem(mac_data.buffer);

	}
	net_nfc_util_append_carrier_config_group(config, cred_config);

	result = net_nfc_util_create_ndef_record_with_carrier_config(
			record,
			config);

	if (result != NET_NFC_OK)
	{
		DEBUG_ERR_MSG("net_nfc_util_create_ndef_record"
				"_with_carrier_config failed"
				"[%d]",result);
	}

	return result;
}
#endif


static void _net_nfc_handover_bss_get_carrier_record(
		net_nfc_handover_bss_create_context_t *context)
{
	LOGD("[%s:%d] START", __func__, __LINE__);

	if(context != NULL)
	{

		if (context->result != NET_NFC_OK && context->result != NET_NFC_BUSY)
		{
			DEBUG_ERR_MSG("context->result is error [%d]", context->result);
		}

		context->result = NET_NFC_OK;

		context->cps = NET_NFC_CONN_HANDOVER_CARRIER_ACTIVATE;

		/* Create carrier record */
		context->result =_net_nfc_handover_bss_create_carrier_record(
				&context->carrier);
		if (context->result!= NET_NFC_OK)
		{
			DEBUG_ERR_MSG("create_bss_config_record failed"
					" [%d]", context->result);
		}

		/* complete and return to upper step */
		context->cb(context->result,
				context->cps,
				context->carrier,
				0,
				NULL,
				context->user_param);
	}
}

#ifdef TARGET
static void _net_nfc_wifi_process_error(int error)
{
	DEBUG_ERR_MSG("_net_nfc_wifi_process_error - [%d]",error);

	context->step = NET_NFC_LLCP_STEP_RETURN;
	context->result = NET_NFC_OPERATION_FAIL;

	g_idle_add((GSourceFunc)
			_net_nfc_handover_bss_wfd_get_carrier_record,
			(gpointer)context);

	return;
}

static int _net_nfc_handover_process_wifi_direct_setup(
		net_nfc_handover_bss_get_context_t *context)
{
	int err = WIFI_DIRECT_ERROR_NONE;
	err = wifi_direct_initialize();
	if(err != WIFI_DIRECT_ERROR_NONE)
	{
		DEBUG_ERR_MSG("wifi_direct_initialize err value %d",err);
		return err;
	}

	err = wifi_direct_set_device_state_changed_cb(
			_net_nfc_wifi_direct_power_changed,
			context);

	if(err != WIFI_DIRECT_ERROR_NONE)
	{
		DEBUG_ERR_MSG("wifi_direct_set_device_state_changed_cb err value %d",err);
		return err;
	}

	err = wifi_direct_set_discovery_state_changed_cb(
			_net_nfc_wifi_scan_completed_cb,
			context);

	if(err != WIFI_DIRECT_ERROR_NONE)
	{
		DEBUG_ERR_MSG("wifi_direct_set_discovery_state_changed_cb err value %d",err);
		return err;
	}

	err = wifi_direct_set_connection_state_changed_cb(
			_net_nfc_wifi_direct_connection_changed,
			context);

	if (err != WIFI_DIRECT_ERROR_NONE)
	{
		DEBUG_ERR_MSG("wifi_direct_set_connection_state_changed_cb err value %d",err);
		return err;
	}
	else
	{
		context->step = NET_NFC_LLCP_STEP_02;
		context->result = NET_NFC_OK;

		wifi_direct_state_e status = WIFI_DIRECT_STATE_DEACTIVATED;

		err = wifi_direct_get_state(&status);
		DEBUG_ERR_MSG("status value %d",status);

		if (status != WIFI_DIRECT_STATE_ACTIVATED)
		{
			wifi_direct_activate();
		}
		else
		{
			DEBUG_MSG("wifi direct is enabled already");

			/* do next step */
			g_idle_add((GSourceFunc)
					_net_nfc_handover_bss_wfd_get_carrier_record,
					(gpointer)context);
		}
		return WIFI_DIRECT_ERROR_NONE;

	}
}

static int _net_nfc_handover_process_wifi_group_setup(
		net_nfc_handover_bss_get_context_t *context)
{
	int err = WIFI_DIRECT_ERROR_NONE;
	bool group_owner = false;

	err = wifi_direct_is_group_owner(&group_owner);
	DEBUG_ERR_MSG("error value %d",err);

	context->step = NET_NFC_LLCP_STEP_03;
	context->result = NET_NFC_OK;

	if(err == WIFI_DIRECT_ERROR_NONE)
	{
		if(group_owner == true)
		{
			DEBUG_MSG("Already group owner, continue next step");
			g_idle_add((GSourceFunc)
					_net_nfc_handover_bss_wfd_get_carrier_record,
					(gpointer)context);
		}
		else
		{
			err = wifi_direct_create_group();
			if (err != WIFI_DIRECT_ERROR_NONE)
			{
				DEBUG_ERR_MSG("wifi_direct_create_group failed [%d]",err);
				return err;
			}
		}
	}
	else
	{
		DEBUG_ERR_MSG("wifi_direct_is_group_owner failed [%d]",err);
		return err;
	}

}

static gboolean _net_nfc_handover_bss_wfd_get_carrier_record(
		net_nfc_handover_bss_get_context_t *context)
{
	LOGD("[%s:%d] START", __func__, __LINE__);
	if(context == NULL)
	{
		DEBUG_ERR_MSG("Invalid context");
		return FALSE;
	}

	if(context != NULL)
	{
		int err = WIFI_DIRECT_ERROR_NONE;

		if (context->result != NET_NFC_OK && context->result != NET_NFC_BUSY)
		{
			DEBUG_ERR_MSG("context->result is error"
					" [%d]", context->result);

			context->step = NET_NFC_LLCP_STEP_RETURN;
		}

		switch (context->step)
		{
		case NET_NFC_LLCP_STEP_01 :
			DEBUG_ERR_MSG("NET_NFC_LLCP_STEP_01");
			err = _net_nfc_handover_process_wifi_direct_setup(context);
			if(err != WIFI_DIRECT_ERROR_NONE)
			{
				DEBUG_ERR_MSG("_net_nfc_handover_process_wifi_direct_setup failed");
				_net_nfc_wifi_process_error(err);
			}
			break;
		case NET_NFC_LLCP_STEP_02:
			DEBUG_ERR_MSG("NET_NFC_LLCP_STEP_02");
			err = _net_nfc_handover_process_wifi_group_setup(context);
			if(err != WIFI_DIRECT_ERROR_NONE)
			{
				DEBUG_ERR_MSG("_net_nfc_handover_process_wifi_group_setup failed");
				_net_nfc_wifi_process_error(err);
			}
			break;
		case NET_NFC_LLCP_STEP_03 :
			DEBUG_ERR_MSG("NET_NFC_LLCP_STEP_03");
			context->step = NET_NFC_LLCP_STEP_RETURN;

			/* append config to ndef message */
			context->result =_net_nfc_handover_bss_create_config_record(
					&context->carrier);
			if (context->result != NET_NFC_OK)
			{
				DEBUG_ERR_MSG("_net_nfc_handover_bss_create_config_record failed"
						"[%d]", context->result);
			}

			g_idle_add((GSourceFunc)
					_net_nfc_handover_bss_wfd_get_carrier_record,
					(gpointer)context);

			break;
		case NET_NFC_LLCP_STEP_RETURN :
			DEBUG_ERR_MSG("NET_NFC_LLCP_STEP_RETURN");
			/* unregister current callback */
			wifi_direct_unset_connection_state_changed_cb();
			wifi_direct_unset_device_state_changed_cb();
			wifi_direct_unset_discovery_state_changed_cb();
			wifi_direct_deinitialize();

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
	}
	LOGD("[%s:%d] END", __func__, __LINE__);

	return TRUE;
}
#endif

net_nfc_error_e net_nfc_server_handover_bss_get_carrier_record(
		net_nfc_server_handover_get_carrier_record_cb cb,
		void *user_param)
{
	net_nfc_error_e result = NET_NFC_OK;
	net_nfc_handover_bss_create_context_t *context = NULL;

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	if (context != NULL)
	{
		context->cb = cb;
		context->user_param = user_param;
		context->step = NET_NFC_LLCP_STEP_01;

		_net_nfc_handover_bss_get_carrier_record(context);

	}
	else
	{
		result = NET_NFC_ALLOC_FAIL;
	}

	return result;
}

#ifdef TARGET
net_nfc_error_e net_nfc_server_handover_bss_wfd_get_carrier_record(
		net_nfc_server_handover_get_carrier_record_cb cb,
		void *user_param)
{
	net_nfc_handover_bss_get_context_t *context = NULL;

	_net_nfc_util_alloc_mem(context, sizeof(*context));
	if (context != NULL)
	{
		context->cb = cb;
		context->user_param = user_param;
		context->step = NET_NFC_LLCP_STEP_01;
		context->cps = NET_NFC_CONN_HANDOVER_CARRIER_ACTIVATE;

		g_idle_add((GSourceFunc)_net_nfc_handover_bss_wfd_get_carrier_record,
				(gpointer)context);

		return NET_NFC_OK:
	}
	else
	{
		return NET_NFC_ALLOC_FAIL;
	}

}
#endif
