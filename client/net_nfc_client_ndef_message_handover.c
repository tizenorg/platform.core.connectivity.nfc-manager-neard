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
#include <glib.h>

#include "net_nfc_ndef_message_handover.h"
#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_handover.h"

API net_nfc_error_e net_nfc_create_carrier_config(
		net_nfc_carrier_config_s **config, net_nfc_conn_handover_carrier_type_e type)
{
	return  net_nfc_util_create_carrier_config(config, type);
}

API net_nfc_error_e net_nfc_add_carrier_config_property(
		net_nfc_carrier_config_s *config, uint16_t attribute, uint16_t size, uint8_t *data)
{
	return net_nfc_util_add_carrier_config_property(config, attribute, size, data);
}

API net_nfc_error_e net_nfc_remove_carrier_config_property(
		net_nfc_carrier_config_s *config, uint16_t attribute)
{
	return net_nfc_util_remove_carrier_config_property(config, attribute);
}

API net_nfc_error_e net_nfc_get_carrier_config_property(
		net_nfc_carrier_config_s *config,
		uint16_t attribute,
		uint16_t *size,
		uint8_t **data)
{
	return net_nfc_util_get_carrier_config_property(config, attribute, size, data);
}

API net_nfc_error_e net_nfc_append_carrier_config_group(
		net_nfc_carrier_config_s *config, net_nfc_carrier_property_s *group)
{
	return net_nfc_util_append_carrier_config_group(config, group);
}

API net_nfc_error_e net_nfc_remove_carrier_config_group(
		net_nfc_carrier_config_s *config, net_nfc_carrier_property_s *group)
{
	return net_nfc_util_remove_carrier_config_group(config, group);
}

API net_nfc_error_e net_nfc_get_carrier_config_group(
		net_nfc_carrier_config_s *config, int index, net_nfc_carrier_property_s **group)
{
	return net_nfc_util_get_carrier_config_group(config, index, group);
}

API net_nfc_error_e net_nfc_free_carrier_config(net_nfc_carrier_config_s *config)
{
	return net_nfc_util_free_carrier_config(config);
}

API net_nfc_error_e net_nfc_create_carrier_config_group(
		net_nfc_carrier_property_s **group, uint16_t attribute)
{
	return net_nfc_util_create_carrier_config_group(group, attribute);
}

API net_nfc_error_e net_nfc_add_carrier_config_group_property(
		net_nfc_carrier_property_s *group,
		uint16_t attribute,
		uint16_t size,
		uint8_t *data)
{
	return net_nfc_util_add_carrier_config_group_property(group, attribute, size, data);
}

API net_nfc_error_e net_nfc_get_carrier_config_group_property(
		net_nfc_carrier_property_s *group,
		uint16_t attribute,
		uint16_t *size,
		uint8_t **data)
{
	return net_nfc_util_get_carrier_config_group_property(group, attribute, size, data);
}

API net_nfc_error_e net_nfc_remove_carrier_config_group_property(
		net_nfc_carrier_property_s *group, uint16_t attribute)
{
	return net_nfc_util_remove_carrier_config_group_property(group, attribute);
}

API net_nfc_error_e net_nfc_free_carrier_group(net_nfc_carrier_property_s *group)
{
	return net_nfc_util_free_carrier_group(group);
}

API net_nfc_error_e net_nfc_create_ndef_record_with_carrier_config(
		ndef_record_s **record, net_nfc_carrier_config_s *config)
{
	return net_nfc_util_create_ndef_record_with_carrier_config(record, config);
}

API net_nfc_error_e net_nfc_create_carrier_config_from_config_record(
		net_nfc_carrier_config_s **config, ndef_record_s *record)
{
	return  net_nfc_util_create_carrier_config_from_config_record(config, record);
}

API net_nfc_error_e net_nfc_append_carrier_config_record(
		ndef_message_s *message,
		ndef_record_s *record,
		net_nfc_conn_handover_carrier_state_e power_status)
{
	return net_nfc_util_append_carrier_config_record(message, record, power_status);
}

API net_nfc_error_e net_nfc_remove_carrier_config_record(
		ndef_message_s *message, ndef_record_s *record)
{
	return net_nfc_util_remove_carrier_config_record(message, record);
}

API net_nfc_error_e net_nfc_get_carrier_config_record(ndef_message_s *message,
		int index, ndef_record_s **record)
{
	return net_nfc_util_get_carrier_config_record(message, index, record);
}

API net_nfc_error_e net_nfc_get_handover_random_number(
		ndef_message_s *message, unsigned short* random_number)
{
	return net_nfc_util_get_handover_random_number(message,  random_number);
}

API net_nfc_error_e net_nfc_get_alternative_carrier_record_count(
		ndef_message_s *message,  unsigned int *count)
{
	return net_nfc_util_get_alternative_carrier_record_count(message,  count);
}

API net_nfc_error_e net_nfc_get_alternative_carrier_power_status(
		ndef_message_s *message,
		int index,
		net_nfc_conn_handover_carrier_state_e *power_state)
{
	return net_nfc_util_get_alternative_carrier_power_status(message, index, power_state);
}

API net_nfc_error_e net_nfc_set_alternative_carrier_power_status(
		ndef_message_s *message,
		int index,
		net_nfc_conn_handover_carrier_state_e power_status)
{
	return net_nfc_util_set_alternative_carrier_power_status(message, index,
			power_status);
}

API net_nfc_error_e net_nfc_get_alternative_carrier_type(ndef_message_s *message,
		int index, net_nfc_conn_handover_carrier_type_e * type)
{
	return net_nfc_util_get_alternative_carrier_type(message,  index, type);
}

API net_nfc_error_e net_nfc_create_handover_request_message(
		ndef_message_s **message)
{
	return net_nfc_util_create_handover_request_message(message);
}

API net_nfc_error_e net_nfc_create_handover_select_message(
		ndef_message_s **message)
{
	return net_nfc_util_create_handover_select_message(message);
}

API net_nfc_error_e net_nfc_create_handover_error_record(
		ndef_record_s **record, uint8_t reason, uint32_t data)
{
	return net_nfc_util_create_handover_error_record(record, reason, data);
}
