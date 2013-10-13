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

// libc header
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>

// platform header
#include <bluetooth-api.h>
#include <vconf.h>

// nfc-manager header
#include "net_nfc_util_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_oem_controller.h"
#include "net_nfc_util_defines.h"

static const char *schema[] =
{
	"",
	"http://www.",
	"https://www.",
	"http://",
	"https://",
	"tel:",
	"mailto:",
	"ftp://anonymous:anonymous@",
	"ftp://ftp.",
	"ftps://",
	"sftp://",
	"smb://",
	"nfs://",
	"ftp://",
	"dav://",
	"news:",
	"telnet://",
	"imap:",
	"rtsp://",
	"urn:",
	"pop:",
	"sip:",
	"sips:",
	"tftp:",
	"btspp://",
	"btl2cap://",
	"btgoep://",
	"tcpobex://",
	"irdaobex://",
	"file://",
	"urn:epc:id:",
	"urn:epc:tag:",
	"urn:epc:pat:",
	"urn:epc:raw:",
	"urn:epc:",
	"urn:epc:nfc:",
};

static uint8_t *bt_addr = NULL;

/* for log tag */
static const char *log_tag = LOG_CLIENT_TAG;

const char *net_nfc_get_log_tag()
{
	return log_tag;
}

void net_nfc_change_log_tag()
{
	log_tag = LOG_SERVER_TAG;
}


API void __net_nfc_util_free_mem(void **mem, char *filename, unsigned int line)
{
	if (mem == NULL)
	{
		SECURE_LOGD("FILE: %s, LINE:%d, Invalid parameter in mem free util, mem is NULL", filename, line);
		return;
	}

	if (*mem == NULL)
	{
		SECURE_LOGD("FILE: %s, LINE:%d, Invalid Parameter in mem free util, *mem is NULL", filename, line);
		return;
	}

	g_free(*mem);
	*mem = NULL;
}

API void __net_nfc_util_alloc_mem(void **mem, int size, char *filename, unsigned int line)
{
	if (mem == NULL || size <= 0)
	{
		SECURE_LOGD("FILE: %s, LINE:%d, Invalid parameter in mem alloc util, mem [%p], size [%d]", filename, line, mem, size);
		return;
	}

	if (*mem != NULL)
	{
		SECURE_LOGD("FILE: %s, LINE:%d, WARNING: Pointer is not NULL, mem [%p]", filename, line, *mem);
	}

	*mem = g_malloc0(size);

	if (*mem == NULL)
	{
		SECURE_LOGD("FILE: %s, LINE:%d, Allocation is failed, size [%d]", filename, line, size);
	}
}

API void __net_nfc_util_strdup(char **output, const char *origin, char *filename, unsigned int line)
{
	if (output == NULL || origin == NULL)
	{
		SECURE_LOGD("FILE: %s, LINE:%d, Invalid parameter in strdup, output [%p], origin [%p]", filename, line, output, origin);
		return;
	}

	if (*output != NULL)
	{
		SECURE_LOGD("FILE: %s, LINE:%d, WARNING: Pointer is not NULL, mem [%p]", filename, line, *output);
	}

	*output = g_strdup(origin);

	if (*output == NULL)
	{
		SECURE_LOGD("FILE: %s, LINE:%d, strdup failed", filename, line);
	}
}

API bool net_nfc_util_alloc_data(data_s *data, uint32_t length)
{
	if (data == NULL || length == 0)
		return false;

	_net_nfc_util_alloc_mem(data->buffer, length);
	if (data->buffer == NULL)
		return false;

	data->length = length;

	return true;
}

API void net_nfc_util_free_data(data_s *data)
{
	if (data == NULL || data->buffer == NULL)
		return;

	_net_nfc_util_free_mem(data->buffer);
	data->length = 0;
}

net_nfc_conn_handover_carrier_state_e net_nfc_util_get_cps(net_nfc_conn_handover_carrier_type_e carrier_type)
{
	net_nfc_conn_handover_carrier_state_e cps = NET_NFC_CONN_HANDOVER_CARRIER_INACTIVATE;

	if (carrier_type == NET_NFC_CONN_HANDOVER_CARRIER_BT)
	{
		int ret = bluetooth_check_adapter();

		switch (ret)
		{
		case BLUETOOTH_ADAPTER_ENABLED :
			cps = NET_NFC_CONN_HANDOVER_CARRIER_ACTIVATE;
			break;

		case BLUETOOTH_ADAPTER_CHANGING_ENABLE :
			cps = NET_NFC_CONN_HANDOVER_CARRIER_ACTIVATING;
			break;

		case BLUETOOTH_ADAPTER_DISABLED :
		case BLUETOOTH_ADAPTER_CHANGING_DISABLE :
		case BLUETOOTH_ERROR_NO_RESOURCES :
		default :
			cps = NET_NFC_CONN_HANDOVER_CARRIER_INACTIVATE;
			break;
		}
	}
	else if (carrier_type == NET_NFC_CONN_HANDOVER_CARRIER_WIFI_BSS)
	{
		int wifi_state = 0;

		vconf_get_int(VCONFKEY_WIFI_STATE, &wifi_state);

		switch (wifi_state)
		{
		case VCONFKEY_WIFI_UNCONNECTED :
		case VCONFKEY_WIFI_CONNECTED :
		case VCONFKEY_WIFI_TRANSFER :
			cps = NET_NFC_CONN_HANDOVER_CARRIER_ACTIVATE;
			break;

		case VCONFKEY_WIFI_OFF :
		default :
			cps = NET_NFC_CONN_HANDOVER_CARRIER_INACTIVATE;
			break;
		}
	}

	return cps;
}

uint8_t *net_nfc_util_get_local_bt_address()
{
	if (bt_addr != NULL)
	{
		return bt_addr;
	}

	_net_nfc_util_alloc_mem(bt_addr, BLUETOOTH_ADDRESS_LENGTH);
	if (bt_addr != NULL)
	{
		if (net_nfc_util_get_cps(NET_NFC_CONN_HANDOVER_CARRIER_BT) != NET_NFC_CONN_HANDOVER_CARRIER_ACTIVATE)
		{
			// bt power is off. so get bt address from configuration file.
			FILE *fp = NULL;

			if ((fp = fopen(HIDDEN_BT_ADDR_FILE, "r")) != NULL)
			{
				unsigned char temp[BLUETOOTH_ADDRESS_LENGTH * 2] = { 0, };

				int ch;
				int count = 0;
				int i = 0;

				while ((ch = fgetc(fp)) != EOF && count < BLUETOOTH_ADDRESS_LENGTH * 2)
				{
					if (((ch >= '0') && (ch <= '9')))
					{
						temp[count++] = ch - '0';
					}
					else if (((ch >= 'a') && (ch <= 'z')))
					{
						temp[count++] = ch - 'a' + 10;
					}
					else if (((ch >= 'A') && (ch <= 'Z')))
					{
						temp[count++] = ch - 'A' + 10;
					}
				}

				for (; i < BLUETOOTH_ADDRESS_LENGTH; i++)
				{
					bt_addr[i] = temp[i * 2] << 4 | temp[i * 2 + 1];
				}

				fclose(fp);
			}
		}
		else
		{
			bluetooth_device_address_t local_address;

			memset(&local_address, 0x00, sizeof(bluetooth_device_address_t));

			bluetooth_get_local_address(&local_address);

			memcpy(bt_addr, &local_address.addr, BLUETOOTH_ADDRESS_LENGTH);
		}
	}

	return bt_addr;
}

void net_nfc_util_enable_bluetooth(void)
{
	bluetooth_enable_adapter();
}

bool net_nfc_util_strip_string(char *buffer, int buffer_length)
{
	bool result = false;
	char *temp = NULL;
	int i = 0;

	_net_nfc_util_alloc_mem(temp, buffer_length);
	if (temp == NULL)
	{
		return result;
	}

	for (; i < buffer_length; i++)
	{
		if (buffer[i] != ' ' && buffer[i] != '\t')
			break;
	}

	if (i < buffer_length)
	{
		memcpy(temp, &buffer[i], buffer_length - i);
		memset(buffer, 0x00, buffer_length);

		memcpy(buffer, temp, buffer_length - i);

		result = true;
	}
	else
	{
		result = false;
	}

	_net_nfc_util_free_mem(temp);

	return true;
}

static uint16_t _net_nfc_util_update_CRC(uint8_t ch, uint16_t *lpwCrc)
{
	ch = (ch ^ (uint8_t)((*lpwCrc) & 0x00FF));
	ch = (ch ^ (ch << 4));
	*lpwCrc = (*lpwCrc >> 8) ^ ((uint16_t)ch << 8) ^ ((uint16_t)ch << 3) ^ ((uint16_t)ch >> 4);
	return (*lpwCrc);
}

void net_nfc_util_compute_CRC(CRC_type_e CRC_type, uint8_t *buffer, uint32_t length)
{
	uint8_t chBlock = 0;
	int msg_length = length - 2;
	uint8_t *temp = buffer;

	// default is CRC_B
	uint16_t wCrc = 0xFFFF; /* ISO/IEC 13239 (formerly ISO/IEC 3309) */

	switch (CRC_type)
	{
	case CRC_A :
		wCrc = 0x6363;
		break;

	case CRC_B :
		wCrc = 0xFFFF;
		break;
	}

	do
	{
		chBlock = *buffer++;
		_net_nfc_util_update_CRC(chBlock, &wCrc);
	}
	while (--msg_length > 0);

	if (CRC_type == CRC_B)
	{
		wCrc = ~wCrc; /* ISO/IEC 13239 (formerly ISO/IEC 3309) */
	}

	temp[length - 2] = (uint8_t)(wCrc & 0xFF);
	temp[length - 1] = (uint8_t)((wCrc >> 8) & 0xFF);
}

const char *net_nfc_util_get_schema_string(int index)
{
	if (index == 0 || index >= NET_NFC_SCHEMA_MAX)
		return NULL;
	else
		return schema[index];
}

