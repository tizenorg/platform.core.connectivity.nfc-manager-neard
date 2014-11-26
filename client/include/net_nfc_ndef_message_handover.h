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
#ifndef __NET_NFC_NDEF_MESSAGE_HANDOVER_H__
#define __NET_NFC_NDEF_MESSAGE_HANDOVER_H__

#include "net_nfc_typedef.h"

/**
  This function create wifi configure handler instance.

  @param[out] 	config 		instance handler
  @param[in] 	type 				Carrier types it would be wifi add hoc or wifi AP

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_OUT_OF_BOUND			type value is not enum value
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  */

net_nfc_error_e net_nfc_create_carrier_config(net_nfc_carrier_config_s **config,
		net_nfc_conn_handover_carrier_type_e type);

/**
  Add property key and value for configuration.
  the data will be copied to config handle, you should free used data array.

  @param[in] 	config 		instance handler
  @param[in] 	attribute 				attribue key for value.
  @param[in] 	size	 				size of value
  @param[in] 	data	 				value array (binary type)

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  */

net_nfc_error_e net_nfc_add_carrier_config_property(
		net_nfc_carrier_config_s *config, uint16_t attribute, uint16_t size, uint8_t *data);

/**
  Remove the key and value from configuration, you can also remove the group  withe CREDENTIAL key.
  The the attribute is exist then it will be removed and also freed automatically.

  @param[in] 	config 		instance handler
  @param[in] 	attribute 				attribue key for value.

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_NO_DATA_FOUND		the given key is not found
  @exception NET_NFC_ALREADY_REGISTERED	the given attribute is already registered
  */

net_nfc_error_e net_nfc_remove_carrier_config_property(
		net_nfc_carrier_config_s *config, uint16_t attribute);
/**
  Get the property value by attribute.

  @param[in] 	config 		instance handler
  @param[in] 	attribute 				attribue key for value.
  @param[out] 	size	 				size of value
  @param[out] 	data	 				value array (binary type)

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_NO_DATA_FOUND		The given key is not found
  */

net_nfc_error_e net_nfc_get_carrier_config_property(
		net_nfc_carrier_config_s *config,
		uint16_t attribute,
		uint16_t *size,
		uint8_t **data);

/**
  The group will be joined into the configure

  @param[in] 	config 		instance handler
  @param[in]	group				group handle

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALREADY_REGISTERED	The given group is already registered
  */

net_nfc_error_e net_nfc_append_carrier_config_group(
		net_nfc_carrier_config_s *config, net_nfc_carrier_property_s *group);

/**
  Remove the group from configure handle

  @param[in] 	config 		instance handler
  @param[in]	group				group handle

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_NO_DATA_FOUND		given handle pointer is not exist in the configure handle
  */

net_nfc_error_e net_nfc_remove_carrier_config_group(
		net_nfc_carrier_config_s *config, net_nfc_carrier_property_s *group);

/**
  Get the group from configure handle by index

  @param[in] 	config 		instance handler
  @param[in]	index				index number
  @param[out]	group				group handle

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_OUT_OF_BOUND			the index number is not bound of the max count
  @exception NET_NFC_NO_DATA_FOUND		this is should be happened if the configure handle is dammaged
  */

net_nfc_error_e net_nfc_get_carrier_config_group(
		net_nfc_carrier_config_s *config, int index, net_nfc_carrier_property_s **group);

/**
  free the configure handle

  @param[in] 	config 		instance handler

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  */

net_nfc_error_e net_nfc_free_carrier_config(net_nfc_carrier_config_s *config);

/**
  create the group handle

  @param[out] 	group 		instance group handler

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  */

net_nfc_error_e net_nfc_create_carrier_config_group (
		net_nfc_carrier_property_s **group, uint16_t attribute);

/**
  add property into the group

  @param[in] 	group 		instance group handler
  @param[in]	attribute		attribute of the property
  @param[in]	size			size of data (value)
  @param[in]	data			data of the property

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  @exception NET_NFC_ALREADY_REGISTERED	the given key is already registered
  */

net_nfc_error_e net_nfc_add_carrier_config_group_property(
		net_nfc_carrier_property_s *group,
		uint16_t attribute,
		uint16_t size,
		uint8_t *data);

/**
  get property from group handle

  @param[in] 	group 		instance group handler
  @param[in]	attribute		attribute of the property
  @param[out]	size			size of data (value)
  @param[out]	data			data of the property

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_NO_DATA_FOUND		the attribute does not exist in the group
  */

net_nfc_error_e net_nfc_get_carrier_config_group_property(
		net_nfc_carrier_property_s *group,
		uint16_t attribute,
		uint16_t *size,
		uint8_t **data);

/**
  remove the property from the group

  @param[in] 	group 		instance group handler
  @param[in]	attribute		attribute of the property

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_NO_DATA_FOUND		the attribute does not exist in the group
  */

net_nfc_error_e net_nfc_remove_carrier_config_group_property(
		net_nfc_carrier_property_s *group, uint16_t attribute);


/**
  free the group

  @param[in] 	group 		instance group handler

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  */

net_nfc_error_e net_nfc_free_carrier_group(net_nfc_carrier_property_s *group);

/**
  Create record handler with config.

  @param[out] 	record 		record handler
  @param[in]	config	the wifi configure handle

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  */

net_nfc_error_e net_nfc_create_ndef_record_with_carrier_config(
		ndef_record_s **record, net_nfc_carrier_config_s *config);


/**
  create configure from the ndef record. the. the record must contained the configuration.
  config should be freed after using

  @param[in] 	record 		record handler
  @param[out]	config	the configure handle

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  @exception NET_NFC_INVALID_FORMAT		if the record does not contained the valid format.
  */

net_nfc_error_e net_nfc_create_carrier_config_from_config_record(
		net_nfc_carrier_config_s **config, ndef_record_s *record);

/**
  append record into the connection handover request for reponse message;

  @param[in] 	message 		ndef message handler
  @param[in] 	record 		record handler
  @param[in]	power_status	the power status of the current configuration

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  @exception NET_NFC_INVALID_FORMAT		if the record does not contained the valid format.
  @exception NET_NFC_NO_DATA_FOUND		the attribute does not exist in the group
  */
net_nfc_error_e net_nfc_append_carrier_config_record(ndef_message_s *message,
		ndef_record_s *record, net_nfc_conn_handover_carrier_state_e power_status);

/**
  append record into the connection handover request for reponse message;

  @param[in] 	message 		ndef message handler
  @param[in] 	record 		record handler

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  @exception NET_NFC_INVALID_FORMAT		if the record does not contained the valid format.
  @exception NET_NFC_NO_DATA_FOUND		Given record does not exist in the ndef message
  */

net_nfc_error_e net_nfc_remove_carrier_config_record(ndef_message_s *message,
		ndef_record_s *record);

/**
  get configure record from ndef message by index

  @param[in] 	message 		ndef message handler
  @param[in]	index
  @param[out] 	record 		record handler

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  @exception NET_NFC_INVALID_FORMAT		if the record does not contained the valid format.
  */

net_nfc_error_e net_nfc_get_carrier_config_record(ndef_message_s *message,
		int index, ndef_record_s **record);

/**
  get randome number from the connection request message

  @param[in] 	message 			ndef message handler
  @param[out] 	randome_number 	randome number

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_INVALID_FORMAT		if the record does not contained the valid format.
  */

net_nfc_error_e net_nfc_get_handover_random_number(ndef_message_s *message,
		unsigned short *random_number);

/**
  get the count of the alternative (configuration) in the message

  @param[in] 	message 		ndef message handler
  @param[out] 	count	 	number configuration in the message

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_INVALID_FORMAT		if the record does not contained the valid format.
  */
net_nfc_error_e net_nfc_get_alternative_carrier_record_count(
		ndef_message_s *message,  unsigned int *count);


/**
  get power status of the given configruation

  @param[in] 	message 		ndef message handler
  @param[in] 	index	 	index
  @param[out]	power_state	power state of the alternative

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_INVALID_FORMAT		if the record does not contained the valid format.
  @exception NET_NFC_NO_DATA_FOUND		there is no alternative record is found
  @exception NET_NFC_OUT_OF_BOUND			index number is out of message count
  */

net_nfc_error_e net_nfc_get_alternative_carrier_power_status(
		ndef_message_s *message,
		int index,
		net_nfc_conn_handover_carrier_state_e *power_state);

/**
  set power status of the given configruation

  @param[in] 	message 		ndef message handler
  @param[in] 	index	 	index
  @param[in]	power_state	power state of the alternative

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_INVALID_FORMAT		if the record does not contained the valid format.
  @exception NET_NFC_NO_DATA_FOUND		there is no alternative record is found
  @exception NET_NFC_OUT_OF_BOUND			index number is out of message count
  */
net_nfc_error_e net_nfc_set_alternative_carrier_power_status(
		ndef_message_s *message,
		int index,
		net_nfc_conn_handover_carrier_state_e power_status);

/**
  this function will get carrier type.

  @param[in] 	carrier_info 			connection handover carrier info handler
  @param[in] 	carrier_type 			record type. it can be a NET_NFC_CONN_HANDOVER_CARRIER_BT or NET_NFC_CONN_HANDOVER_CARRIER_WIFI or NET_NFC_CONN_HANDOVER_CARRIER_UNKNOWN.

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  */

net_nfc_error_e net_nfc_get_alternative_carrier_type(
		ndef_message_s *message,
		int index,
		net_nfc_conn_handover_carrier_type_e *power_state);

/**
  craete the connection handover request message

  @param[in] 	message 			connection handover carrier info handler

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  */

net_nfc_error_e net_nfc_create_handover_request_message(
		ndef_message_s **message);

/**
  craete the connection handover select message

  @param[in] 	message 			connection handover carrier info handler

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  */

net_nfc_error_e net_nfc_create_handover_select_message(
		ndef_message_s **message);

/**
  craete the connection handover error record message

  @param[out] 	record 			connection handover carrier info handler
  @param[in]	reason			error codes (reason)
  @param[in]	data				extra data for each error codes

  @return		return the result of the calling the function

  @exception NET_NFC_NULL_PARAMETER		parameter(s) has(have) illigal NULL pointer(s)
  @exception NET_NFC_ALLOC_FAIL			allocation is failed
  */
net_nfc_error_e net_nfc_create_handover_error_record(ndef_record_s **record,
		uint8_t reason, uint32_t data);

#endif //__NET_NFC_NDEF_MESSAGE_HANDOVER_H__
