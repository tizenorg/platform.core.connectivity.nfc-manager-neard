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
#ifndef __NET_NFC_DATA_H__
#define __NET_NFC_DATA_H__

#include "net_nfc_typedef.h"

/**

  @addtogroup NET_NFC_MANAGER_INFO
  @{
  This document is for the APIs reference document

  NFC Manager defines are defined in <nfc-manager-def.h>

  @li @c #net_nfc_client_initialize                  Initialize the nfc device.

*/

/**
  create data handler only.

  @param[out] 	data		data handler

  @return 	return the result of this operation

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illegal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			memory allocation is failed
  */

net_nfc_error_e net_nfc_create_data_only(data_s **data);

/**
  create data handler with initial values, bytes will be copied into the data handler.

  @param[out] 	data		data handler
  @param[in] 	bytes	binary data
  @param[in]	length	size of binary data;

  @return 	return the result of this operation

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illegal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			memory allocation is failed
  */
net_nfc_error_e net_nfc_create_data(data_s **data, const uint8_t *bytes, size_t length);

/**
  get the byes and length from data handler. data handler assume bytes may have '0x0' value.
  that's why this function also provides the length.

  @param[in] 	data	data handler
  @param[out]	bytes	binary pointer (it returns the direct pointer of handler's data) do not free this
  @param[out]	length	length of the binary data;

  @return 	return the result of this operation

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illegal NULL pointer(s)
  */
net_nfc_error_e net_nfc_get_data(const data_s *data, uint8_t **bytes, size_t *length);

/**
  replace the data handler with given bytes. binary data (bytes) will be copied to data hander.
  application should free or use local variable for given byte pointer.

  @param[in] 	data	data handler
  @param[in] 	bytes	binary data
  @param[in]	length	size of binary data

  @return 	return the result of this operation

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  */

net_nfc_error_e net_nfc_set_data(data_s *data, const uint8_t *bytes, size_t length);

/**
  get length of data handler's bytes.

  @param[in] 	data		data handler

  @return 	return the length of bytes

  @exception 	0 is returned if data is NULL
  */

size_t net_nfc_get_data_length(const data_s *data);

/**
  get pointer of the handler's bytes (do not free this. it should be freed when the application call "net_nfc_free_data" function

  @param[in] 	data	data handler

  @return 	return the pointer of bytes.

  @exception 	NULL is returned if data is NULL
  */

uint8_t* net_nfc_get_data_buffer(const data_s *data);

/**
  free data handler. (it also free the copied bytes)

  @param[in] 	data		data handler

  @return 	return the result of this operation

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  */

net_nfc_error_e net_nfc_free_data (data_s *data);


/**
  @}
  */

#endif //__NET_NFC_DATA_H__
