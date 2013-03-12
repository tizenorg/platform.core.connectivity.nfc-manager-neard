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


#ifndef __NET_NFC_APDU__
#define __NET_NFC_APDU__

#include "net_nfc_typedef.h"


#ifdef __cplusplus
 extern "C" {
#endif


typedef unsigned int apdu_slot_t;
typedef unsigned int apdu_channel_t;
typedef void (* net_nfc_apdu_response_cb ) (net_nfc_error_e result, apdu_channel_t channel, data_h data, void* user_data);

/**
@addtogroup NET_NFC_MANAGER_APDU
@{

*/

/**
	initialize the apdu api. the callback function will be called when the data is arrived from simcard or SE

	@param[in] 		callback 		callback function
	@param[in]		user_data	context value that will be delivered to the callback function

	@return 	return the result of the calling the function
*/
net_nfc_error_e net_nfc_register_apdu_cb (net_nfc_apdu_response_cb callback, void * user_data);

/**

	allows to select the slot. UICC or SE

	<ul>
	<li> slot# 0 : UICC	<\li>
	<li> slot# 1 : Secure Element<\li>
	</ul>

	@param[in] 		slot 		slot for mms

	@return 	return the result of the calling the function
*/
net_nfc_error_e net_nfc_select_apdu_slot (apdu_slot_t slot);

/**
	Answer to reset  is the sequence of bytes output by a chip card after the card reader has electrically reset the card.
	The ATR tells the reader which communication capabilities the card has. The format of the ATR is defined in ISO/IEC 7816-3.[1]
	this function return ATR data if the simcard is available, or NULL will be returned

	@param[out]		data		ATR data will be returned if the simcard is available.

	@return 	return the result of the calling the function

*/
net_nfc_error_e net_nfc_get_apdu_answer_to_reset (data_h  data);

/**
	this opens a channel to communicate with application on the simcard.
	you may open the channel with

	@param[out]		channel		return the assined chennel
	@param[in]		aid			application id that mached in the simcard application

	@return 	return the result of the calling the function

*/
net_nfc_error_e net_nfc_open_apdu_channel (apdu_channel_t *channel , net_nfc_string_t aid);

/**
	send the apud data to opened channel.

	@param[in]		channel 		channel number that already opened
	@param[in]		data			data value that will be send

	@return 	return the result of the calling the function
*/
net_nfc_error_e net_nfc_send_apdu_command (apdu_channel_t channel, data_t data);

/**
	closes opened channel

	@param[in]		channel 		channel number

 	@return 	return the result of the calling the function
*/
net_nfc_error_e net_nfc_close_apdu_channel (apdu_channel_t channel);

/**
	disconnect all the channel and return to original state. it stops calling callback function

	@return 	return the result of the calling the function

	@exception NET_NFC_NOT_REGISTERED	unset is requested but the callback was not registered before
*/
net_nfc_error_e net_nfc_unset_apdu_cb (void);


/**
@}
*/



#ifdef __cplusplus
}
#endif


#endif


