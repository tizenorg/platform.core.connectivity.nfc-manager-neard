#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "vconf.h"

#include "net_nfc.h"
#include "net_nfc_typedef.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_client_util.h"

#include "neardal.h"

/* TODO get real data */
#define NET_NFC_JEWEL_PICC_MAX_SIZE		116
#define NET_NFC_MIFARE_ULTRA_PICC_MAX_SIZE	48
#define NET_NFC_FELICA_PICC_MAX_SIZE		256
#define NET_NFC_MIFARE_DESFIRE_PICC_MAX_SIZE	2048

typedef struct _net_nfc_client_cb
{
	net_nfc_client_manager_set_active_completed active_cb;
	void *active_ud;

	/* callback for power status changed */
	net_nfc_client_manager_activated activated_cb;
	void *activated_ud;

	net_nfc_client_tag_tag_discovered tag_discovered_cb;
	void *tag_discovered_ud;
	net_nfc_client_tag_tag_detached tag_detached_cb;
	void *tag_detached_ud;

	net_nfc_client_ndef_read_completed ndef_read_cb;
	void *ndef_read_ud;
	net_nfc_client_ndef_write_completed ndef_write_cb;
	void *ndef_write_ud;

	net_nfc_client_p2p_device_discovered p2p_discovered_cb;
	void *p2p_discovered_ud;
	net_nfc_client_p2p_device_detached p2p_detached_cb;
	void *p2p_detached_ud;
	net_nfc_client_p2p_send_completed p2p_send_completed_cb;
	void *p2p_send_completed_ud;
	net_nfc_client_p2p_data_received p2p_data_received_cb;
	void *p2p_data_received_ud;
} net_nfc_client_cb;

static net_nfc_client_cb client_cb;

static char *nfc_adapter_path;
static bool nfc_adapter_powered;
static bool nfc_adapter_polling;
static char *nfc_adapter_mode;
static neardal_tag *tag;
static neardal_dev *dev;
static data_s *rawNDEF;
static net_nfc_target_info_s *target_info;
static net_nfc_target_handle_s *target_handle;

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
	NFC_DBG("adapter added %s", name);

	if (nfc_adapter_path) {
		g_free(nfc_adapter_path);
		nfc_adapter_path = NULL;
	}

	if (name != NULL)
		nfc_adapter_path = g_strdup(name);
}

static void _adapter_removed_cb(const char *name, void *user_data)
{
	NFC_DBG("adapter removed %s", name);

	if (nfc_adapter_path) {
		g_free(nfc_adapter_path);
		nfc_adapter_path = NULL;
	}
}

static void _adapter_property_changed_cb(char *name, char *property,
					void *value, void *user_data)
{
	NFC_DBG("adapter name %s, property %s", name, property);

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

		if (client_cb.activated_cb != NULL)
			client_cb.activated_cb(nfc_adapter_powered, client_cb.activated_ud);
	}
}

static void _power_completed_cb(errorCode_t error_code, void *user_data)
{
	errorCode_t err;
	neardal_adapter *neard_adapter = NULL;
	bool powered;
	net_nfc_error_e result;

	NFC_DBG("power completed adapter path %s", nfc_adapter_path);
	if (nfc_adapter_path == NULL)
		goto exit;

	err = neardal_get_adapter_properties(nfc_adapter_path, &neard_adapter);
	if (err != NEARDAL_SUCCESS)
		goto exit;

	NFC_DBG("power completed %d", neard_adapter->powered);
	powered = (neard_adapter->powered) ? true : false;
	if (vconf_set_bool(VCONFKEY_NFC_STATE, powered) != 0)
		NFC_DBG("vconf_set_bool failed ");

	if (powered == true) {
		if (nfc_adapter_polling == false) {
			err = neardal_start_poll_loop(nfc_adapter_path,
						NEARD_ADP_MODE_DUAL);

			if (err == NEARDAL_SUCCESS)
				nfc_adapter_polling = true;
		}
	}

exit:
	if (client_cb.active_cb != NULL && client_cb.active_ud != NULL) {
		result = _convert_error_code(error_code);

		client_cb.active_cb(result, client_cb.active_ud);

		client_cb.active_cb = NULL;
		client_cb.active_ud = NULL;
	}
}

static void _tag_found_cb(const char *tagName, void *user_data)
{
	NFC_DBG("NFC tag found tagName: %s", tagName);

	if (neardal_get_tag_properties(tagName, &tag) != NEARDAL_SUCCESS)
		return;
	if (tag == NULL || tag->records == NULL)
		return;

	net_nfc_manager_util_play_sound(NET_NFC_TASK_START);
	if (neardal_tag_get_rawNDEF(tag->name)
				!= NEARDAL_SUCCESS) {
		NFC_DBG("Failed to get rawNDEF");
		return;
	}
}

static void _tag_lost_cb(const char *tagName, void *user_data)
{
	NFC_DBG("NFC tag lost");

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

	if (client_cb.tag_detached_cb != NULL)
		client_cb.tag_detached_cb(client_cb.tag_detached_ud);
}

static void _create_target_info(data_s *data)
{
	uint8_t ndefCardstate;
	int actualDataSize, maxDataSize;
	bool ndef_support;

	if (target_info == NULL)
		target_info = g_try_malloc0(sizeof(net_nfc_target_info_s));

	if (target_info == NULL) {
		NFC_DBG("target_info mem alloc is failed");
		return;
	}
	if (target_handle == NULL)
		target_handle = g_try_malloc0(sizeof(net_nfc_target_handle_s));

	if (target_handle == NULL) {
		NFC_DBG("handle mem alloc is failed");
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
}

static void _read_completed_cb(GVariant *ret, void *user_data)
{
	gconstpointer value;
	gsize length;
	ndef_message_s *ndef = NULL;
	net_nfc_error_e result = NET_NFC_TAG_READ_FAILED;

	NFC_DBG("read completed cb adapter path %s", nfc_adapter_path);

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
		NFC_DBG("ndef memory alloc fail..");
		goto exit;
	}
	result = net_nfc_util_convert_rawdata_to_ndef_message(
						rawNDEF, ndef);
exit:
	if (client_cb.tag_discovered_cb != NULL)
		client_cb.tag_discovered_cb(target_info, client_cb.tag_discovered_ud);

	if (client_cb.ndef_read_cb != NULL) {
		client_cb.ndef_read_cb(result, ndef, client_cb.ndef_read_ud);

		net_nfc_util_free_ndef_message(ndef);
		client_cb.ndef_read_cb = NULL;
		client_cb.ndef_read_ud = NULL;
	}
}

static void _write_completed_cb(errorCode_t error_code, void *user_data)
{
	net_nfc_error_e result;

	NFC_DBG("write completed cb adapter path %s", nfc_adapter_path);

	result = _convert_error_code(error_code);

	if (client_cb.ndef_write_cb != NULL) {
		client_cb.ndef_write_cb(result, client_cb.ndef_write_ud);

		client_cb.ndef_write_cb = NULL;
		client_cb.ndef_write_ud = NULL;
	}
}

static void _device_found_cb(const char *devName, void *user_data)
{
	int devType = 0;

	NFC_DBG("p2p device found %s", devName);
	if (neardal_get_dev_properties(devName, &dev) != NEARDAL_SUCCESS)
		return;

	if (target_handle == NULL)
		target_handle = g_try_malloc0(sizeof(net_nfc_target_handle_s));

	if (target_handle == NULL) {
		NFC_DBG("handle mem alloc is failed");
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

	if (client_cb.p2p_discovered_cb != NULL)
		client_cb.p2p_discovered_cb(target_handle, client_cb.p2p_discovered_ud);
}

static void _device_lost_cb(const char *devName, void *user_data)
{
	errorCode_t err;

	NFC_DBG("p2p device lost %s", devName);
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
	if (client_cb.p2p_detached_cb != NULL)
		client_cb.p2p_detached_cb(client_cb.p2p_detached_ud);
}

static void _p2p_received_cb(GVariant *ret, void *user_data)
{
	gconstpointer value;
	gsize length;
	ndef_message_s *ndef = NULL;

	NFC_DBG("p2p received cb adapter path %s", nfc_adapter_path);
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

	if (client_cb.p2p_data_received_cb != NULL)
		client_cb.p2p_data_received_cb(rawNDEF, client_cb.p2p_data_received_ud);
}

static void _p2p_send_completed_cb(errorCode_t error_code, void *user_data)
{
	net_nfc_error_e result;

	NFC_DBG("p2p send completed cb error code %d", error_code);

	result = _convert_error_code(error_code);

	if (client_cb.p2p_send_completed_cb != NULL)
		client_cb.p2p_send_completed_cb(result,
				client_cb.p2p_send_completed_ud);
}

net_nfc_error_e net_nfc_neard_send_p2p(net_nfc_target_handle_s *handle, data_s *data,
			net_nfc_client_p2p_send_completed callback, void *user_data)
{
	neardal_record *record;

	NFC_DBG("neard send p2p");

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

	NFC_DBG("neard send p2p successfully");
	client_cb.p2p_send_completed_cb = callback;
	client_cb.p2p_send_completed_ud = user_data;
	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_get_current_tag_info(net_nfc_target_info_s **info)
{

	if (target_info == NULL)
		return NET_NFC_NOT_CONNECTED;
	else
		*info = target_info;

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_get_current_target_handle(
			net_nfc_target_handle_s **handle)
{
	net_nfc_error_e result = NET_NFC_OK;

	if (target_handle == NULL || target_info == NULL) {
		result = NET_NFC_NOT_CONNECTED;
	} else if (NET_NFC_NFCIP1_INITIATOR == target_info->devType ||
			NET_NFC_NFCIP1_TARGET == target_info->devType) {
		if (handle)
			*handle = target_info->handle;

		result = NET_NFC_OK;
	} else {
		result = NET_NFC_NOT_CONNECTED;
	}

	return result;
}

net_nfc_error_e net_nfc_neard_read_tag(net_nfc_target_handle_s *handle,
		net_nfc_client_ndef_read_completed callback, void *user_data)
{
	if (handle != target_handle || rawNDEF == NULL)
		return NET_NFC_TARGET_IS_MOVED_AWAY;

	if (tag == NULL && tag->records == NULL)
		return NET_NFC_TARGET_IS_MOVED_AWAY;

	if (neardal_tag_get_rawNDEF(tag->name)
				!= NEARDAL_SUCCESS) {
		NFC_DBG("Failed to get rawNDEF");
		return NET_NFC_TAG_READ_FAILED;
	}

	client_cb.ndef_read_cb = callback;
	client_cb.ndef_read_ud = user_data;

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_write_ndef(net_nfc_target_handle_s *handle,
					data_s *data,
					net_nfc_client_ndef_write_completed callback,
					void *user_data)
{
	neardal_record *record;

	NFC_DBG("write ndef message");
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

	client_cb.ndef_write_cb = callback;
	client_cb.ndef_write_ud = user_data;

	memcpy(record->rawNDEF, data->buffer, data->length);
	record->rawNDEFSize = data->length;

	if (neardal_tag_write(record) != NEARDAL_SUCCESS) {
		neardal_free_record(record);
		return NET_NFC_TAG_WRITE_FAILED;
	}

	neardal_free_record(record);

	return NET_NFC_OK;
}

bool net_nfc_neard_is_tag_connected(void)
{
	if (tag == NULL)
		return false;

	return true;
}

net_nfc_error_e net_nfc_neard_set_active(int state,
		net_nfc_client_manager_set_active_completed callback,
		void *user_data)
{
	errorCode_t err;

	NFC_DBG("set active: %d", state);

	if (state == nfc_adapter_powered)
		return NET_NFC_OK;

	RETV_IF(NULL == nfc_adapter_path, NET_NFC_NOT_INITIALIZED);

	client_cb.active_cb = callback;
	client_cb.active_ud = user_data;

	if (nfc_adapter_polling == true)
		neardal_stop_poll(nfc_adapter_path);

	err = neardal_set_adapter_property(nfc_adapter_path,
			NEARD_ADP_PROP_POWERED, (void *) state);
	if (err != NEARDAL_SUCCESS)
		return NET_NFC_OPERATION_FAIL;

	return NET_NFC_OK;
}

void net_nfc_neard_set_p2p_discovered(
		net_nfc_client_p2p_device_discovered callback, void *user_data)
{
	client_cb.p2p_discovered_cb = callback;
	client_cb.p2p_discovered_ud = user_data;
}

void net_nfc_neard_unset_p2p_discovered(void)
{
	client_cb.p2p_discovered_cb = NULL;
	client_cb.p2p_discovered_ud = NULL;
}

void net_nfc_neard_set_p2p_detached(
		net_nfc_client_p2p_device_detached callback, void *user_data)
{
	client_cb.p2p_detached_cb = callback;
	client_cb.p2p_detached_ud = user_data;
}

void net_nfc_neard_unset_p2p_detached(void)
{
	client_cb.p2p_detached_cb = NULL;
	client_cb.p2p_detached_ud = NULL;
}

void net_nfc_neard_set_p2p_data_received(
		net_nfc_client_p2p_data_received callback, void *user_data)
{
	client_cb.p2p_data_received_cb = callback;
	client_cb.p2p_data_received_ud = user_data;
}

void net_nfc_neard_unset_p2p_data_received(void)
{
	client_cb.p2p_data_received_cb = NULL;
	client_cb.p2p_data_received_ud = NULL;
}

void net_nfc_neard_set_tag_discovered(
		net_nfc_client_tag_tag_discovered callback, void *user_data)
{
	client_cb.tag_discovered_cb = callback;
	client_cb.tag_discovered_ud = user_data;
}

void net_nfc_neard_unset_tag_discovered(void)
{
	client_cb.tag_discovered_cb = NULL;
	client_cb.tag_discovered_ud = NULL;
}

void net_nfc_neard_set_tag_detached(
		net_nfc_client_tag_tag_detached callback, void *user_data)
{
	client_cb.tag_detached_cb = callback;
	client_cb.tag_detached_ud = user_data;
}

void net_nfc_neard_unset_tag_detached(void)
{
	client_cb.tag_detached_cb = NULL;
	client_cb.tag_detached_ud = NULL;
}

void net_nfc_neard_set_activated(net_nfc_client_manager_activated callback,
		void *user_data)
{
	client_cb.activated_cb = callback;
	client_cb.activated_ud = user_data;
}

void net_nfc_neard_unset_activated(void)
{
	client_cb.activated_cb = NULL;
	client_cb.activated_ud = NULL;
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

		NFC_DBG("failed to register the callback");

		return NET_NFC_NOT_REGISTERED;
	}

	return NET_NFC_OK;
}

net_nfc_error_e net_nfc_neard_initialize(void)
{
	errorCode_t err;
	int len;
	neardal_adapter *neard_adapter = NULL;
	char **adapters = NULL;

	NFC_DBG("neard init");

	if (nfc_adapter_path != NULL)
		return NET_NFC_OK;

	net_nfc_neard_cb_init();

	err = neardal_get_adapters(&adapters, &len);
	NFC_DBG("failed to get adapter error: %d", err);
	if (err != NEARDAL_SUCCESS)
		goto out_err;

	if (!(len > 0 && adapters != NULL))
		goto out_err;

	nfc_adapter_path = g_strdup(adapters[0]);
	NFC_DBG("adapter_path: %s", nfc_adapter_path);

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

	NFC_DBG("adapter powered %d, polling %d", nfc_adapter_powered,
							nfc_adapter_polling);

	if (vconf_set_bool(VCONFKEY_NFC_STATE, nfc_adapter_powered) != 0)
		NFC_DBG("vconf NFC state set failed ");
	if (nfc_adapter_powered == TRUE && nfc_adapter_polling == false)
		neardal_start_poll_loop(nfc_adapter_path, NEARD_ADP_MODE_DUAL);

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
	NFC_DBG("neard deinitialize");

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
		g_free(target_info);
		target_info = NULL;
	}
}
