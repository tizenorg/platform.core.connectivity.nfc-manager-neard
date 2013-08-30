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

#ifndef __NET_NFC_TEST_SYS_HANDLER_H__
#define __NET_NFC_TEST_SYS_HANDLER_H__

#include <glib.h>

void net_nfc_test_sys_handler_set_launch_popup_state(gpointer data,
		gpointer user_data);

void net_nfc_test_sys_handler_get_launch_popup_state(gpointer data,
		gpointer user_data);

void net_nfc_test_sys_handler_set_launch_popup_state_sync(gpointer data,
				gpointer user_data);

void net_nfc_test_sys_handler_set_launch_popup_state_force(gpointer data,
				gpointer user_data);

void net_nfc_test_sys_handler_set_launch_popup_state_force_sync(gpointer data,
				gpointer user_data);

#endif//__NET_NFC_TEST_SYS_HANDLER_H__
