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
#ifndef __NET_NFC_UTIL_INTERNAL_H__
#define __NET_NFC_UTIL_INTERNAL_H__

#include <stdio.h>
#include <libgen.h>
#include <netinet/in.h>

#include "net_nfc_typedef_internal.h"

#define NET_NFC_MANAGER_DATA_PATH		"/opt/data/nfc-manager-daemon"
#define NET_NFC_MANAGER_DATA_PATH_MESSAGE	"message"
#define NET_NFC_MANAGER_NDEF_FILE_NAME		"ndef-message.txt"


typedef enum
{
	CRC_A = 0x00,
	CRC_B,
} CRC_type_e;

void net_nfc_change_log_tag();

/* Memory utils */
/* allocation memory */
void __net_nfc_util_alloc_mem(void **mem, int size, char *filename, unsigned int line);
#define	 _net_nfc_util_alloc_mem(mem,size) __net_nfc_util_alloc_mem((void **)&mem,size, basename(__FILE__), __LINE__)

/* allocation memory */
void __net_nfc_util_strdup(char **output, const char *origin, char *filename, unsigned int line);
#define	 _net_nfc_util_strdup(output, origin) __net_nfc_util_strdup(&output, origin, basename(__FILE__), __LINE__)

/* free memory, after free given memory it set NULL. Before proceed free, this function also check NULL */
void __net_nfc_util_free_mem(void **mem, char *filename, unsigned int line);
#define	 _net_nfc_util_free_mem(mem) __net_nfc_util_free_mem((void **)&mem, basename(__FILE__), __LINE__)

bool net_nfc_util_alloc_data(data_s *data, uint32_t length);
bool net_nfc_util_duplicate_data(data_s *dest, net_nfc_data_s *src);
void net_nfc_util_free_data(data_s *data);

net_nfc_conn_handover_carrier_state_e net_nfc_util_get_cps(net_nfc_conn_handover_carrier_type_e carrier_type);

uint8_t *net_nfc_util_get_local_bt_address();
void net_nfc_util_enable_bluetooth(void);

bool net_nfc_util_strip_string(char *buffer, int buffer_length);

void net_nfc_util_compute_CRC(CRC_type_e CRC_type, uint8_t *buffer, uint32_t length);

const char *net_nfc_util_get_schema_string(int index);

#endif //__NET_NFC_UTIL_INTERNAL_H__
