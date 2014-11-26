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

#ifndef __NET_NFC_TEST_JEWEL_TAG_H__
#define __NET_NFC_TEST_JEWEL_TAG_H__

#include <glib.h>


void net_nfc_test_tag_jewel_read_id(gpointer data,
		gpointer user_data);

void net_nfc_test_tag_jewel_read_byte(gpointer data,
		gpointer user_data);

void net_nfc_test_tag_jewel_read_all(gpointer data,
		gpointer user_data);

void net_nfc_test_tag_jewel_write_with_erase(gpointer data,
		gpointer user_data);

void net_nfc_test_tag_jewel_write_with_no_erase(gpointer data,
		gpointer user_data);

#endif //__NET_NFC_TEST_JEWEL_TAG_H__