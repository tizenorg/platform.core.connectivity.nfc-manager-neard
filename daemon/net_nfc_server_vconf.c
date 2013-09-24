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
#include <vconf.h>

#include "net_nfc_typedef.h"

#include "net_nfc_server_vconf.h"
#include "net_nfc_server_common.h"
#include "net_nfc_server_manager.h"

#include "net_nfc_debug_internal.h"

static gboolean powered_off_by_flightmode = FALSE;

static void vconf_set_flight_mode(int boolval);

static void net_nfc_server_vconf_pm_state_changed(keynode_t *key,
		void *user_data);

static void net_nfc_server_vconf_flight_mode_changed(keynode_t *key,
		void *user_data);


static void vconf_set_flight_mode(int boolval)
{
	gint result;

	/* set predefined item */
	result = vconf_set_bool(VCONFKEY_NFC_PREDEFINED_ITEM_STATE, boolval);
	if (result != 0)
	{
		NFC_ERR("can not set to %d: %s",
				boolval,
				"VCONKEY_NFC_PREDEFINED_ITEM_STATE");
	}
}

static void net_nfc_server_vconf_pm_state_changed(keynode_t *key,
		void *user_data)
{
	gint state = 0;
	gint pm_state = 0;
	gint result;

	result = vconf_get_bool(VCONFKEY_NFC_STATE, &state);
	if (result != 0)
		NFC_ERR("can not get %s", "VCONFKEY_NFC_STATE");

	if (state == false)
	{
		NFC_DBG("NFC off");
		return;
	}

	result = vconf_get_int(VCONFKEY_PM_STATE, &pm_state);
	if (result != 0)
		NFC_ERR("can not get %s", "VCONFKEY_PM_STATE");

	NFC_DBG("pm_state : %d", pm_state);

	if (pm_state == VCONFKEY_PM_STATE_NORMAL ||
			pm_state == VCONFKEY_PM_STATE_LCDOFF)
	{
		net_nfc_server_restart_polling_loop();
	}
}

static void net_nfc_server_vconf_flight_mode_changed(keynode_t *key,
		void *user_data)
{
	gint flight_mode = 0;
	gint nfc_state = 0;

	gint result = 0;

	result = vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE, &flight_mode);
	if (result != 0)
	{
		NFC_ERR("Can not get %s",
				"VCONFKEY_TELEPHONY_FLIGHT_MODE");
	}

	NFC_DBG("flight mode %d", flight_mode);

	result = vconf_get_bool(VCONFKEY_NFC_STATE, &nfc_state);
	if (result != 0)
	{
		NFC_ERR("Can not get %s",
				"VCONFKEY_NET_STATE");
	}

	NFC_DBG("nfc_state %d", nfc_state);
	NFC_DBG("powerd_off_by_flightmode %d", powered_off_by_flightmode);

	if (flight_mode) /* turn on flight mode */
	{
		/* nfc is already disabled ignore it */
		if (nfc_state == VCONFKEY_NFC_STATE_OFF)
			return;

		NFC_INFO("Turning NFC off");
		net_nfc_server_manager_set_active(FALSE);

		powered_off_by_flightmode = TRUE;

		vconf_set_flight_mode(0);
	}
	else /* turn off flight mode */
	{
		/* nfc is already enabled, ignre it */
		if (nfc_state == VCONFKEY_NFC_STATE_ON)
			return;

		if (powered_off_by_flightmode == FALSE)
			return;

		NFC_INFO("Turning NFC on");
		net_nfc_server_manager_set_active(TRUE);

		powered_off_by_flightmode = FALSE;

		vconf_set_flight_mode(1);
	}
}

void net_nfc_server_vconf_init(void)
{
	vconf_notify_key_changed(VCONFKEY_PM_STATE,
			net_nfc_server_vconf_pm_state_changed,
			NULL);

	vconf_notify_key_changed(VCONFKEY_TELEPHONY_FLIGHT_MODE,
			net_nfc_server_vconf_flight_mode_changed,
			NULL);
}

void net_nfc_server_vconf_deinit(void)
{
	vconf_ignore_key_changed(VCONFKEY_PM_STATE,
			net_nfc_server_vconf_pm_state_changed);

	vconf_ignore_key_changed(VCONFKEY_TELEPHONY_FLIGHT_MODE,
			net_nfc_server_vconf_flight_mode_changed);
}
