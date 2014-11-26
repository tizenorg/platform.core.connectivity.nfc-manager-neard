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
#include <vconf.h>

#include "net_nfc_debug_internal.h"
#include "net_nfc_typedef.h"
#include "net_nfc_client.h"
#include "net_nfc_neard.h"


API net_nfc_error_e net_nfc_client_initialize()
{
	return net_nfc_neard_initialize();
}


API net_nfc_error_e net_nfc_client_deinitialize()
{
	net_nfc_error_e result = NET_NFC_OK;

	net_nfc_neard_deinitialize();

	return result;
}


API net_nfc_error_e net_nfc_client_is_nfc_supported(int *state)
{
	net_nfc_error_e ret;

	if (state != NULL)
	{
		if (vconf_get_bool(VCONFKEY_NFC_FEATURE, state) == 0)
		{
			ret = NET_NFC_OK;
		}
		else
		{
			ret = NET_NFC_INVALID_STATE;
		}
	}
	else
	{
		ret = NET_NFC_NULL_PARAMETER;
	}

	return ret;
}


API net_nfc_error_e net_nfc_client_get_nfc_state(int *state)
{
	net_nfc_error_e ret;

	if (state != NULL)
	{
		if (vconf_get_bool(VCONFKEY_NFC_STATE, state) == 0)
			ret = NET_NFC_OK;
		else
			ret = NET_NFC_INVALID_STATE;
	}
	else
	{
		ret = NET_NFC_NULL_PARAMETER;
	}

	return ret;
}
