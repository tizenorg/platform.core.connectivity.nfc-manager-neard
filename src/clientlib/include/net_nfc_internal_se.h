/*
  * Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
  *
  * Licensed under the Flora License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at

  *     http://floralicense.org/license/
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */


#ifndef __NET_NFC_INTERNAL_SE_H__
#define __NET_NFC_INTERNAL_SE_H__

#include "net_nfc_typedef.h"


#ifdef __cplusplus
 extern "C" {
#endif


/**

@addtogroup NET_NFC_MANAGER_SECURE_ELEMENT
@{
	This document is for the APIs reference document

        NFC Manager defines are defined in <net_nfc_typedef.h>

        @li @c #net_nfc_set_secure_element_type		set secure element type
        @li @c #net_nfc_get_secure_element_type		get current selected secure element
        @li @c #net_nfc_open_internal_secure_element	open selected secure element
        @li @c #net_nfc_close_internal_secure_element	close selected secure element
        @li @c #net_nfc_send_apdu						send apdu



*/

/**
	set secure element type. secure element will be a UICC or ESE. only one secure element is selected in a time. external reader can communicate with
	secure element by emitting RF

	\par Sync (or) Async: Sync
	This is a Asynchronous API

	@param[in] 	se_type			secure element type
	@param[in]	trans_param		user data that will be delivered to callback

	@return		return the result of the calling the function

	@exception NET_NFC_INVALID_PARAM 	not supported se_type

*/

net_nfc_error_e net_nfc_set_secure_element_type(net_nfc_se_type_e se_type, void* trans_param);

/**
	get current select se type.

	\par Sync (or) Async: Sync
	This is a Asynchronous API

	@param[in] 	se_type			secure element type
	@param[in]	trans_param		user data that will be delivered to callback

	@return		return the result of the calling the function

	@exception NET_NFC_INVALID_PARAM 	not supported se_type

*/

net_nfc_error_e net_nfc_get_secure_element_type(void* trans_param);

/**
	open and intialize the type of secure element. if the type of secure element is selected, then change mode as MODE OFF to prevent to be detected by external reader

	\par Sync (or) Async: Sync
	This is a Asynchronous API

	@param[in] 	se_type			secure element type
	@param[in]	trans_param		user data that will be delivered to callback

	@return		return the result of the calling the function

	@exception NET_NFC_INVALID_PARAM 	not supported se_type

*/

net_nfc_error_e net_nfc_open_internal_secure_element(net_nfc_se_type_e se_type, void* trans_param);

/**
	close opend secure element and change back to previous setting

	\par Sync (or) Async: Sync
	This is a Asynchronous API

	@param[in] 	handle			the handle of opend secure element
	@param[in]	trans_param		user data that will be delivered to callback

	@return		return the result of the calling the function

	@exception NET_NFC_INVALID_PARAM 	not supported se_type

*/

net_nfc_error_e net_nfc_close_internal_secure_element(net_nfc_target_handle_h handle, void* trans_param);

/**
	send apdu to opend secure element

	\par Sync (or) Async: Sync
	This is a Asynchronous API

	@param[in] 	handle			the handle of opend secure element
	@param[in] 	apdu			apdu command to send
	@param[in]	trans_param		user data that will be delivered to callback

	@return		return the result of the calling the function

	@exception NET_NFC_INVALID_PARAM 	invalid

*/

net_nfc_error_e net_nfc_send_apdu(net_nfc_target_handle_h handle, data_h apdu, void* trans_param);


#ifdef __cplusplus
}
#endif


#endif

