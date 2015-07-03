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
/* Need to handle in later */
#if 0
	gint result;

	/* set predefined item */
	result = vconf_set_bool(VCONFKEY_NFC_PREDEFINED_ITEM_STATE, boolval);
	if (result != 0)
		NFC_ERR("can not set to %d: %s", boolval, "VCONKEY_NFC_PREDEFINED_ITEM_STATE");
#endif
}

static void net_nfc_server_vconf_pm_state_changed(keynode_t *key,
		void *user_data)
{
	gint result;
	gint state = 0;
	gint pm_state = 0;

	result = vconf_get_bool(VCONFKEY_NFC_STATE, &state);
	if (result != 0)
		NFC_ERR("can not get %s", "VCONFKEY_NFC_STATE");

	if (false == state)
	{
		NFC_DBG("NFC off");
		return;
	}

	result = vconf_get_int(VCONFKEY_PM_STATE, &pm_state);
	if (result != 0)
		NFC_ERR("can not get %s", "VCONFKEY_PM_STATE");

	NFC_DBG("pm_state : %d", pm_state);

	if (VCONFKEY_PM_STATE_NORMAL == pm_state || VCONFKEY_PM_STATE_LCDOFF == pm_state)
		net_nfc_server_restart_polling_loop();
}

static void net_nfc_server_vconf_flight_mode_changed(keynode_t *key,
		void *user_data)
{
	gint result = 0;
	gint nfc_state = 0;
	gint flight_mode = 0;


	result = vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE, &flight_mode);
	if (result != 0)
		NFC_ERR("Can not get VCONFKEY_TELEPHONY_FLIGHT_MODE");

	NFC_DBG("flight mode %d", flight_mode);

	result = vconf_get_bool(VCONFKEY_NFC_STATE, &nfc_state);
	if (result != 0)
		NFC_ERR("Can not get VCONFKEY_NET_STATE");

	NFC_DBG("nfc_state %d", nfc_state);
	NFC_DBG("powerd_off_by_flightmode %d", powered_off_by_flightmode);

	if (flight_mode) /* turn on flight mode */
	{
		/* nfc is already disabled ignore it */
		if (VCONFKEY_NFC_STATE_OFF == nfc_state)
			return;

		NFC_INFO("Turning NFC off");
		net_nfc_server_manager_set_active(FALSE);

		powered_off_by_flightmode = TRUE;

		vconf_set_flight_mode(0);
	}
	else /* turn off flight mode */
	{
		/* nfc is already enabled, ignre it */
		if (VCONFKEY_NFC_STATE_ON == nfc_state)
			return;

		if (FALSE == powered_off_by_flightmode)
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
			net_nfc_server_vconf_pm_state_changed, NULL);

	vconf_notify_key_changed(VCONFKEY_TELEPHONY_FLIGHT_MODE,
			net_nfc_server_vconf_flight_mode_changed, NULL);
}

void net_nfc_server_vconf_deinit(void)
{
	vconf_ignore_key_changed(VCONFKEY_PM_STATE, net_nfc_server_vconf_pm_state_changed);

	vconf_ignore_key_changed(VCONFKEY_TELEPHONY_FLIGHT_MODE,
			net_nfc_server_vconf_flight_mode_changed);
}
