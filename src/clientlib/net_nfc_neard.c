#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "vconf.h"

#include "net_nfc.h"
#include "net_nfc_typedef.h"
#include "net_nfc_debug_private.h"
#include "net_nfc_typedef_private.h"
#include "net_nfc_client_nfc_private.h"
#include "net_nfc_util_private.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_manager_util_private.h"
#include "net_nfc_app_util_private.h"

#include "neardal.h"

/* TODO get real data */
#define NET_NFC_JEWEL_PICC_MAX_SIZE		116
#define NET_NFC_MIFARE_ULTRA_PICC_MAX_SIZE	48
#define NET_NFC_FELICA_PICC_MAX_SIZE		256
#define NET_NFC_MIFARE_DESFIRE_PICC_MAX_SIZE	2048

static net_nfc_response_cb client_cb;
static client_context_t *client_context;

static char *nfc_adapter_path;
static bool nfc_adapter_powered;
static bool nfc_adapter_polling;
static char *nfc_adapter_mode;
static neardal_tag *tag;
static neardal_dev *dev;
static data_s *rawNDEF;
static net_nfc_target_info_s *target_info;
static net_nfc_target_handle_s *target_handle;
static void *callback_data;
static bool read_flag;

static net_nfc_error_e _convert_error_code(errorCode_t error_code)
{
	switch (error_code) {
	case NEARDAL_SUCCESS:
		return NET_NFC_OK;
	case NEARDAL_ERROR_GENERAL_ERROR:
		return NET_NFC_UNKNOWN_ERROR;
	case NEARDAL_ERROR_INVALID_PARAMETER:
		return NET_NFC_INVALID_PARAM;
	case NEARDAL_ERROR_NO_MEMORY:
		return NET_NFC_ALLOC_FAIL;
	case NEARDAL_ERROR_DBUS:
	case NEARDAL_ERROR_DBUS_CANNOT_CREATE_PROXY:
	case NEARDAL_ERROR_DBUS_CANNOT_INVOKE_METHOD:
	case NEARDAL_ERROR_DBUS_INVOKE_METHOD_ERROR:
		return NET_NFC_OPERATION_FAIL;
	case NEARDAL_ERROR_NO_ADAPTER:
		return NET_NFC_NOT_SUPPORTED;
	case NEARDAL_ERROR_POLLING_ALREADY_ACTIVE:
		return NET_NFC_ALREADY_REGISTERED;
	case NEARDAL_ERROR_NO_TAG:
	case NEARDAL_ERROR_INVALID_RECORD:
		return NET_NFC_TAG_READ_FAILED;
	case NEARDAL_ERROR_NO_RECORD:
		return NET_NFC_NO_DATA_FOUND;
	case NEARDAL_ERROR_NO_DEV:
		return NET_NFC_TARGET_IS_MOVED_AWAY;
	default:
		return NET_NFC_UNKNOWN_ERROR;
	}
}

static int _convert_target_type(const char *type)
{
	int t_type;

	/* TODO tag sub types */

	if (type == NULL)
		return NET_NFC_UNKNOWN_TARGET;

	if (!g_strcmp0(type, "Type 1"))
		t_type = NET_NFC_JEWEL_PICC;
	else if (!g_strcmp0(type, "Type 2"))
		t_type = NET_NFC_MIFARE_ULTRA_PICC;
	/*
		NET_NFC_MIFARE_MINI_PICC
		NET_NFC_MIFARE_1K_PICC
		NET_NFC_MIFARE_4K_PICC
		NET_NFC_MIFARE_ULTRA_PICC
	*/
	else if (!g_strcmp0(type, "Type 3"))
		t_type = NET_NFC_FELICA_PICC;
	else if (!g_strcmp0(type, "Type 4"))
		t_type = NET_NFC_MIFARE_DESFIRE_PICC;
	else if (!g_strcmp0(type, "Target"))
		t_type = NET_NFC_NFCIP1_TARGET;
	else if (!g_strcmp0(type, "Initiator"))
		t_type = NET_NFC_NFCIP1_INITIATOR;
	else
		t_type = NET_NFC_UNKNOWN_TARGET;

	return t_type;
}

static uint32_t _get_tag_id(const char *name)
{
	uint32_t id;
	char **s;

	s = g_strsplit(name, "tag", 2);
	id = atoi(s[1]);
	g_strfreev(s);

	return id;
}

static uint32_t _get_dev_id(const char *name)
{
	uint32_t id;
	char **s;

	s = g_strsplit(name, "device", 2);
	id = atoi(s[1]);
	g_strfreev(s);

	return id;
}

static bool _check_ndef(uint8_t *ndef_card_state,
					int *max_data_size,
					int *real_data_size)
{
	if (ndef_card_state == NULL ||
		max_data_size == NULL || real_data_size == NULL) {

		return false;
	}

	if (tag == NULL || rawNDEF == NULL)
		return false;

	if (tag->readOnly)
		*ndef_card_state = NET_NFC_NDEF_CARD_READ_ONLY;
	else
		*ndef_card_state = NET_NFC_NDEF_CARD_READ_WRITE;

	/* TODO get real data */
	switch (_convert_target_type(tag->type)) {
	case NET_NFC_JEWEL_PICC:
		*max_data_size = NET_NFC_JEWEL_PICC_MAX_SIZE;
		break;

	case NET_NFC_MIFARE_ULTRA_PICC:
		*max_data_size = NET_NFC_MIFARE_ULTRA_PICC_MAX_SIZE;
		break;

	case NET_NFC_FELICA_PICC:
		*max_data_size = NET_NFC_FELICA_PICC_MAX_SIZE;
		break;

	case NET_NFC_MIFARE_DESFIRE_PICC:
		*max_data_size = NET_NFC_MIFARE_DESFIRE_PICC_MAX_SIZE;
		break;
	}

	*real_data_size = rawNDEF->length;

	return true;
}

static void _adapter_added_cb(const char *name, void *user_data)
{
	DEBUG_CLIENT_MSG("adapter added %s", name);

	if (nfc_adapter_path) {
		g_free(nfc_adapter_path);
		nfc_adapter_path = NULL;
	}

	if (name != NULL)
		nfc_adapter_path = g_strdup(name);
}

static void _adapter_removed_cb(const char *name, void *user_data)
{
	DEBUG_CLIENT_MSG("adapter removed %s", name);

	if (nfc_adapter_path) {
		g_free(nfc_adapter_path);
		nfc_adapter_path = NULL;
	}
}

static void _adapter_property_changed_cb(char *name, char *property,
					void *value, void *user_data)
{
	DEBUG_CLIENT_MSG("adapter name %s, property %s", name, property);

	if (!g_strcmp0(property, "Polling")) {
		if ((int *) value == 0)
			nfc_adapter_polling = false;
		else
			nfc_adapter_polling = true;
	} else if (!g_strcmp0(property, "Mode")) {
		if (nfc_adapter_mode) {
			g_free(nfc_adapter_mode);
			nfc_adapter_mode = NULL;
		}

		nfc_adapter_mode = g_strdup((char *)value);
	} else if (!g_strcmp0(property, "Powered")) {
		if ((int *) value == 0)
			nfc_adapter_powered = false;
		else
			nfc_adapter_powered = true;
	}
}

static void _power_completed_cb(errorCode_t error_code, void *user_data)
{
	errorCode_t err;
	neardal_adapter *neard_adapter = NULL;
	bool powered;
	net_nfc_error_e result;

	DEBUG_CLIENT_MSG("power completed adapter path %s", nfc_adapter_path);
	if (nfc_adapter_path == NULL)
		return;

	err = neardal_get_adapter_properties(nfc_adapter_path, &neard_adapter);
	if (err != NEARDAL_SUCCESS)
		return;

	DEBUG_CLIENT_MSG("power completed %d", neard_adapter->powered);
	powered = (neard_adapter->powered) ? true : false;
	if (vconf_set_bool(VCONFKEY_NFC_STATE, powered) != 0)
		DEBUG_CLIENT_MSG("vconf_set_boot failed ");

	if (powered == true) {
		if (nfc_adapter_polling == false) {
			err = neardal_start_poll_loop(nfc_adapter_path,
						NEARD_ADP_MODE_DUAL);

			if (err == NEARDAL_SUCCESS)
				nfc_adapter_polling = true;
		}
	}

	if (client_cb != NULL && client_context != NULL) {
		result = _convert_error_code(error_code);
		if (neard_adapter->powered == 0)
			client_cb(NET_NFC_MESSAGE_DEINIT, result, NULL,
					client_context->register_user_param,
					NULL);
		else
			client_cb(NET_NFC_MESSAGE_INIT, result, NULL,
					client_context->register_user_param,
					NULL);
	}
}

static void _tag_found_cb(const char *tagName, void *user_data)
{
	DEBUG_CLIENT_MSG("NFC tag found %s", tagName);

	net_nfc_manager_util_play_sound(NET_NFC_TASK_START);

	if (neardal_get_tag_properties(tagName, &tag) != NEARDAL_SUCCESS)
		return;
	if (tag == NULL || tag->records == NULL)
		return;

	if (neardal_tag_get_rawNDEF(tag->name)
				!= NEARDAL_SUCCESS) {
		DEBUG_CLIENT_MSG("Failed to get rawNDEF");
		return;
	}
}

static void _tag_lost_cb(const char *tagName, void *user_data)
{
	DEBUG_CLIENT_MSG("NFC tag lost");

	if (tag != NULL) {
		neardal_free_tag(tag);
		tag = NULL;
	}

	if (rawNDEF != NULL) {
		net_nfc_util_free_data(rawNDEF);
		rawNDEF = NULL;
	}

	if (target_handle != NULL) {
		g_free(target_handle);
		target_handle = NULL;
	}

	if (target_info != NULL) {
		g_free(target_info);
		target_info = NULL;
	}

	if (nfc_adapter_polling == true)
		return;

	if (neardal_start_poll_loop(nfc_adapter_path, NEARD_ADP_MODE_DUAL)
							== NEARDAL_SUCCESS)
		nfc_adapter_polling = true;

	if (client_cb != NULL && client_context != NULL)
		client_cb(NET_NFC_MESSAGE_TAG_DETACHED, NET_NFC_OK, NULL,
				client_context->register_user_param, NULL);
}

static void _create_target_info(data_s *data)
{
	uint8_t ndefCardstate;
	int actualDataSize, maxDataSize;
	bool ndef_support;

	if (target_info == NULL)
		target_info = g_try_malloc0(sizeof(net_nfc_target_info_s));

	if (target_info == NULL) {
		DEBUG_CLIENT_MSG("target_info mem alloc is failed");
		return;
	}
	if (target_handle == NULL)
		target_handle = g_try_malloc0(sizeof(net_nfc_target_handle_s));

	if (target_handle == NULL) {
		DEBUG_CLIENT_MSG("handle mem alloc is failed");
		return;
	}

	target_handle->connection_id = _get_tag_id(tag->name);
	target_handle->connection_type = NET_NFC_TAG_CONNECTION;
	target_handle->target_type = _convert_target_type(tag->type);

	ndef_support =  _check_ndef(&ndefCardstate, &maxDataSize,
						&actualDataSize);

	if (ndef_support == false) {
		ndefCardstate = 0;
		maxDataSize   = -1;
		actualDataSize  = -1;
	}

	target_info->ndefCardState = ndefCardstate;
	target_info->actualDataSize = actualDataSize;
	target_info->maxDataSize = maxDataSize;
	target_info->devType = _convert_target_type(tag->type);
	target_info->handle = target_handle;
	target_info->is_ndef_supported = ndef_support;
	target_info->number_of_keys = 0;
	target_info->tag_info_list = NULL;
	target_info->raw_data = *data;

	client_context->target_info = target_info;
}

static void _read_completed_cb(GVariant *ret, void *user_data)
{
	gconstpointer value;
	gsize length;
	ndef_message_s *ndef = NULL;
	net_nfc_error_e result = NET_NFC_TAG_READ_FAILED;

	DEBUG_CLIENT_MSG("read completed cb adapter path %s", nfc_adapter_path);

	if (ret == NULL)
		goto exit;

	value = g_variant_get_data(ret);
	length = g_variant_get_size(ret);

	if (length < 0)
		goto exit;

	if (rawNDEF != NULL) {
		net_nfc_util_free_data(rawNDEF);
		rawNDEF = NULL;
	}

	rawNDEF = g_try_malloc0(sizeof(data_s));
	if (rawNDEF == NULL)
		goto exit;

	rawNDEF->length = (int)length;
	rawNDEF->buffer = g_try_malloc0(rawNDEF->length);
	if (rawNDEF->buffer == NULL) {
		g_free(rawNDEF);
		goto exit;
	}

	memcpy(rawNDEF->buffer, value, rawNDEF->length);

	net_nfc_app_util_process_ndef(rawNDEF);

	_create_target_info(rawNDEF);

	if (net_nfc_util_create_ndef_message(&ndef) != NET_NFC_OK) {
		DEBUG_CLIENT_MSG("ndef memory alloc fail..");
		goto exit;
	}
	result = net_nfc_util_convert_rawdata_to_ndef_message(
						rawNDEF, ndef);
exit:
	if (client_cb != NULL) {
		if (read_flag == false) {
			client_cb(NET_NFC_MESSAGE_TAG_DISCOVERED,
					result, target_info,
					client_context->register_user_param,
					NULL);
			return;
		}

		client_cb(NET_NFC_MESSAGE_READ_NDEF, result, ndef,
				client_context->register_user_param,
				callback_data);

		net_nfc_util_free_ndef_message(ndef);

		read_flag = false;
	}
}

static void _write_completed_cb(errorCode_t error_code, void *user_data)
{
	net_nfc_error_e result;
	DEBUG_CLIENT_MSG("write completed cb adapter path %s",
						nfc_adapter_path);
	result = _convert_error_code(error_code);

	if (client_cb != NULL)
		client_cb(NET_NFC_MESSAGE_WRITE_NDEF, result, NULL,
			client_context->register_user_param, callback_data);
}

static void _device_found_cb(const char *devName, void *user_data)
{
	int devType = 0;

	DEBUG_CLIENT_MSG("p2p device found %s", devName);
	if (neardal_get_dev_properties(devName, &dev) != NEARDAL_SUCCESS)
		return;

	if (target_handle == NULL)
		target_handle = g_try_malloc0(sizeof(net_nfc_target_handle_s));

	if (target_handle == NULL) {
		DEBUG_CLIENT_MSG("handle mem alloc is failed");
		return;
	}


	devType = _convert_target_type(nfc_adapter_mode);
	if (devType == NET_NFC_NFCIP1_INITIATOR)
		target_handle->connection_type = NET_NFC_P2P_CONNECTION_TARGET;
	else if (devType == NET_NFC_NFCIP1_TARGET)
		target_handle->connection_type =
					NET_NFC_P2P_CONNECTION_INITIATOR;
	target_handle->connection_id = _get_dev_id(devName);
	target_handle->target_type = devType;

	if (client_cb != NULL)
		client_cb(NET_NFC_MESSAGE_P2P_DISCOVERED,
					NET_NFC_OK, target_handle,
					client_context->register_user_param,
					NULL);
}

static void _device_lost_cb(const char *devName, void *user_data)
{
	errorCode_t err;

	DEBUG_CLIENT_MSG("p2p device lost %s", devName);
	if (target_handle != NULL) {
		g_free(target_handle);
		target_handle = NULL;
	}

	if (nfc_adapter_polling == true)
		goto exit;

	if (nfc_adapter_path == NULL)
		goto exit;

	err = neardal_start_poll_loop(nfc_adapter_path,
						NEARD_ADP_MODE_DUAL);
	if (err != NEARDAL_SUCCESS)
		goto exit;

	nfc_adapter_polling = true;

exit:
	if (client_cb != NULL)
		client_cb(NET_NFC_MESSAGE_P2P_DETACHED, NET_NFC_OK, NULL,
				client_context->register_user_param, NULL);
}

static void _p2p_send_completed_cb(errorCode_t error_code, void *user_data)
{
	net_nfc_error_e result;

	DEBUG_CLIENT_MSG("p2p send completed cb error code %d", error_code);

	result = _convert_error_code(error_code);

	if (client_cb != NULL)
		client_cb(NET_NFC_MESSAGE_P2P_SEND, result, NULL,
			client_context->register_user_param, NULL);
}

static void _p2p_received_cb(GVariant *ret, void *user_data)
{
	gconstpointer value;
	gsize length;
	ndef_message_s *ndef = NULL;

	DEBUG_CLIENT_MSG("p2p received cb adapter path %s", nfc_adapter_path);
	if (nfc_adapter_path == NULL)
		return;

	if (ret == NULL)
		return;

	value = g_variant_get_data(ret);
	length = g_variant_get_size(ret);

	if (length < 0)
		return;

	if (rawNDEF != NULL) {
		net_nfc_util_free_data(rawNDEF);
		rawNDEF = NULL;
	}

	rawNDEF = g_try_malloc0(sizeof(data_s));
	if (rawNDEF == NULL)
		return;

	rawNDEF->length = (int)length;
	rawNDEF->buffer = g_try_malloc0(rawNDEF->length);
	if (rawNDEF->buffer == NULL) {
		g_free(rawNDEF);
		return;
	}

	memcpy(rawNDEF->buffer, value, rawNDEF->length);

	net_nfc_app_util_process_ndef(rawNDEF);

	if (client_cb != NULL)
		client_cb(NET_NFC_MESSAGE_P2P_RECEIVE, NET_NFC_OK, rawNDEF,
			client_context->register_user_param, NULL);
}

net_nfc_error_e net_nfc_neard_read_tag(net_nfc_target_handle_s *handle,
							void *trans_param)
{
	if (handle != target_handle || rawNDEF == NULL)
		return NET_NFC_TARGET_IS_MOVED_AWAY;

	if (tag == NULL && tag->records == NULL)
		return NET_NFC_TARGET_IS_MOVED_AWAY;

	if (neardal_tag_get_rawNDEF(tag->name)
				!= NEARDAL_SUCCESS) {
		DEBUG_CLIENT_MSG("Failed to get rawNDEF");
		return NET_NFC_TAG_READ_FAILED;
	}

	callback_data = trans_param;
	read_flag = true;

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_write_ndef(net_nfc_target_handle_s *handle,
					data_s *data, void *trans_param)
{
	neardal_record *record;

	DEBUG_CLIENT_MSG("write ndef message");
	if (target_handle == NULL || handle != target_handle)
		return NET_NFC_TARGET_IS_MOVED_AWAY;

	if ((data->buffer == NULL && data->length != 0) ||
		(data->buffer != NULL && data->length == 0))
		return NET_NFC_NULL_PARAMETER;

	record = g_try_malloc0(sizeof(neardal_record));
	if (record == NULL)
		return NET_NFC_ALLOC_FAIL;

	record->name = g_strdup(tag->records[0]);
	record->type = g_strdup("Raw");
	record->rawNDEF = g_try_malloc0(data->length);
	if (record->rawNDEF == NULL) {
		neardal_free_record(record);
		return NET_NFC_ALLOC_FAIL;
	}

	memcpy(record->rawNDEF, data->buffer, data->length);
	record->rawNDEFSize = data->length;

	if (neardal_tag_write(record) != NEARDAL_SUCCESS) {
		neardal_free_record(record);
		return NET_NFC_TAG_WRITE_FAILED;
	}

	neardal_free_record(record);

	callback_data = trans_param;
	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_send_p2p(net_nfc_target_handle_s *handle, data_s *data, void *trans_param)
{
	neardal_record *record;

	DEBUG_CLIENT_MSG("neard send p2p");

	if (target_handle == NULL || handle != target_handle)
		return NET_NFC_TARGET_IS_MOVED_AWAY;

	if ((data->buffer == NULL && data->length != 0) ||
		(data->buffer != NULL && data->length == 0))
		return NET_NFC_NULL_PARAMETER;

	record = g_try_malloc0(sizeof(neardal_record));
	if (record == NULL)
		return NET_NFC_ALLOC_FAIL;

	record->name = g_strdup(dev->name);
	record->type = g_strdup("Raw");
	record->rawNDEF = g_try_malloc0(data->length);
	if (record->rawNDEF == NULL) {
		neardal_free_record(record);
		return NET_NFC_ALLOC_FAIL;
	}

	memcpy(record->rawNDEF, data->buffer, data->length);
	record->rawNDEFSize = data->length;

	if (neardal_dev_push(record) != NEARDAL_SUCCESS) {
		neardal_free_record(record);
		return NET_NFC_TAG_WRITE_FAILED;
	}

	neardal_free_record(record);

	DEBUG_CLIENT_MSG("neard send p2p successfully");
	callback_data = trans_param;
	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_is_tag_connected(int *dev_type)
{
	if (tag == NULL)
		return NET_NFC_NOT_CONNECTED;

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_register_cb(net_nfc_response_cb cb)
{
	if (cb == NULL)
		return NET_NFC_NULL_PARAMETER;

	client_cb = cb;

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_unregister_cb(void)
{
	if (client_cb == NULL)
		return NET_NFC_NOT_REGISTERED;

	client_cb = NULL;

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_cb_init(void)
{
	if (neardal_set_cb_adapter_added(
		_adapter_added_cb, NULL) != NEARDAL_SUCCESS ||
		neardal_set_cb_adapter_removed(
			_adapter_removed_cb, NULL) != NEARDAL_SUCCESS ||
		neardal_set_cb_adapter_property_changed(
			_adapter_property_changed_cb, NULL) != NEARDAL_SUCCESS ||
		neardal_set_cb_power_completed(_power_completed_cb, NULL)
							!= NEARDAL_SUCCESS ||
		neardal_set_cb_tag_found(
			_tag_found_cb, NULL) != NEARDAL_SUCCESS ||
		neardal_set_cb_tag_lost(
			_tag_lost_cb, NULL) != NEARDAL_SUCCESS ||
		neardal_set_cb_read_completed(_read_completed_cb, NULL)
							!= NEARDAL_SUCCESS ||
		neardal_set_cb_write_completed(_write_completed_cb, NULL)
							!= NEARDAL_SUCCESS ||
		neardal_set_cb_dev_found(_device_found_cb, NULL)
							!= NEARDAL_SUCCESS ||
		neardal_set_cb_dev_lost(_device_lost_cb, NULL)
							!= NEARDAL_SUCCESS ||
		neardal_set_cb_push_completed(_p2p_send_completed_cb, NULL)
							!= NEARDAL_SUCCESS ||
		neardal_set_cb_p2p_received(_p2p_received_cb, NULL)
							!= NEARDAL_SUCCESS) {
		DEBUG_CLIENT_MSG("failed to register the callback");

		return NET_NFC_NOT_REGISTERED;
	}

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_enable(void)
{
	errorCode_t err;
	int powered;

	DEBUG_CLIENT_MSG("enable neard adapter");

	if (nfc_adapter_path == NULL)
		return NET_NFC_NOT_INITIALIZED;

	DEBUG_CLIENT_MSG("adapter powered %d, polling %d", nfc_adapter_powered,
							nfc_adapter_polling);
	if (nfc_adapter_powered == false) {
		powered = 1;
		err = neardal_set_adapter_property(nfc_adapter_path,
			NEARD_ADP_PROP_POWERED, (void *) powered);
		if (err != NEARDAL_SUCCESS)
			return NET_NFC_OPERATION_FAIL;
	}

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_disable(void)
{
	int powered = 0;

	DEBUG_CLIENT_MSG("disable neard adapter");

	if (nfc_adapter_polling == true)
		neardal_stop_poll(nfc_adapter_path);

	if (nfc_adapter_powered == true)
		neardal_set_adapter_property(nfc_adapter_path,
				NEARD_ADP_PROP_POWERED, (void *) powered);

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_initialize(void)
{
	errorCode_t err;
	int len;
	neardal_adapter *neard_adapter = NULL;
	char **adapters = NULL;

	DEBUG_CLIENT_MSG("neard init");

	client_context = net_nfc_get_client_context();

	if (nfc_adapter_path != NULL)
		return NET_NFC_OK;

	net_nfc_neard_cb_init();

	err = neardal_get_adapters(&adapters, &len);
	DEBUG_CLIENT_MSG("error %d", err);
	if (err != NEARDAL_SUCCESS)
		goto out_err;

	if (!(len > 0 && adapters != NULL))
		goto out_err;

	nfc_adapter_path = g_strdup(adapters[0]);
	DEBUG_CLIENT_MSG("adapter_path %s", nfc_adapter_path);

	neardal_free_array(&adapters);
	adapters = NULL;

	err = neardal_get_adapter_properties(nfc_adapter_path, &neard_adapter);
	if (err != NEARDAL_SUCCESS)
		goto out_err;

	if (neard_adapter == NULL)
		goto out_err;

	nfc_adapter_powered = (neard_adapter->powered) ? true : false;
	nfc_adapter_polling = (neard_adapter->polling) ? true : false;
	neardal_free_adapter(neard_adapter);
	neard_adapter = NULL;

	DEBUG_CLIENT_MSG("adapter powered %d, polling %d", nfc_adapter_powered,
							nfc_adapter_polling);
	return NET_NFC_OK;

out_err:
	if (adapters)
		neardal_free_array(&adapters);

	if (neard_adapter)
		neardal_free_adapter(neard_adapter);

	return NET_NFC_OPERATION_FAIL;
}

void net_nfc_neard_deinitialize(void)
{
	DEBUG_CLIENT_MSG("neard deinitialize");

	if (nfc_adapter_mode) {
		g_free(nfc_adapter_mode);
		nfc_adapter_mode = NULL;
	}

	if (tag != NULL) {
		neardal_free_tag(tag);
		tag = NULL;
	}

	if (dev != NULL) {
		neardal_free_device(dev);
		dev = NULL;
	}

	if (rawNDEF != NULL) {
		net_nfc_util_free_data(rawNDEF);
		rawNDEF = NULL;
	}

	if (target_handle != NULL) {
		g_free(target_handle);
		target_handle = NULL;
	}

	if (target_info != NULL) {
		net_nfc_util_release_tag_info(target_info);
		target_info = NULL;
	}
}
