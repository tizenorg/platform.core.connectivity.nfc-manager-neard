#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "vconf.h"

#include "net_nfc.h"
#include "net_nfc_typedef.h"
#include "net_nfc_debug_private.h"
#include "net_nfc_typedef_private.h"
#include "net_nfc_client_nfc_private.h"

#include "neardal.h"

static net_nfc_response_cb client_cb;
static client_context_t *client_context;

static char *nfc_adapter_path;
static bool nfc_adapter_powered;
static bool nfc_adapter_polling;
static char *nfc_adapter_mode;
static neardal_tag *tag;
static neardal_dev *dev;

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
}
