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

#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include "net_nfc.h"

#include "net_nfc_test_client.h"
#include "net_nfc_test_llcp.h"
#include "net_nfc_test_snep.h"
#include "net_nfc_test_manager.h"
#include "net_nfc_test_tag.h"
#include "net_nfc_test_ndef.h"
#include "net_nfc_test_p2p.h"
#include "net_nfc_test_handover.h"
#include "net_nfc_test_transceive.h"
#include "net_nfc_test_jewel.h"
#include "net_nfc_test_tag_mifare.h"
#include "net_nfc_test_tag_felica.h"
#include "net_nfc_test_se.h"
#include "net_nfc_test_sys_handler.h"


typedef struct _TestData TestData;

struct _TestData
{
	gchar *interface;
	gchar *name;
	GFunc func;
	GFunc func_sync;
	gchar *comment;
};

static TestData test_data[] = {
	{
		"Manager",
		"SetActive",
		net_nfc_test_manager_set_active,
		net_nfc_test_manager_set_active_sync,
		"Set activation",
	},

	{
		"Manager",
		"GetServerState",
		net_nfc_test_manager_get_server_state,
		net_nfc_test_manager_get_server_state_sync,
		"Get server state"
	},

	{
		"Client",
		"ClientInitialize",
		net_nfc_test_initialize,
		NULL,
		"Start Client Initialization"
	},

	{
		"Client",
		"ClientDeinitialize",
		net_nfc_test_deinitialize,
		NULL,
		"Start Client Deinitialization"
	},

	{
		"Client",
		"ClientIsNfcSupported",
		net_nfc_test_is_nfc_supported,
		NULL,
		"Get is nfc supported"
	},

	{
		"Client",
		"ClientGetNfcState",
		net_nfc_test_get_nfc_state,
		NULL,
		"Get nfc state"
	},
#if 0
	{
		"Tag",
		"IsTagConnected",
		net_nfc_test_tag_is_tag_connected,
		net_nfc_test_tag_is_tag_connected_sync,
		"Get is tag connected"
	},
	{
		"Tag",
		"GetCurrentTagInfo",
		net_nfc_test_tag_get_current_tag_info,
		net_nfc_test_tag_get_current_tag_info_sync,
		"Get current tag info"
	},

	{
		"Tag",
		"GetCurrentTargetHandle",
		net_nfc_test_tag_get_current_target_handle,
		net_nfc_test_tag_get_current_target_handle_sync,
		"Get current target handle id"
	},
#endif
	{
		"Tag",
		"TagDiscovered",		/* waiting for signal */
		net_nfc_test_tag_set_tag_discovered,
		NULL,
		"Waiting for TagDiscoved signal"
	},

	{
		"Tag",
		"SetFilter",
		net_nfc_test_tag_set_filter,
		NULL,
		"Set Tag filter"
	},

	{
		"Tag",
		"GetFilter",
		net_nfc_test_tag_get_filter,
		NULL,
		"Get Tag filter"
	},


	{
		"Tag",
		"TagDetached",		/* waiting for signal */
		net_nfc_test_tag_set_tag_detached,
		NULL,
		"Waiting for TagDetached signal"
	},

	{
		"Ndef",
		"Read",
		net_nfc_test_ndef_read,
		net_nfc_test_ndef_read_sync,
		"Read NDEF",
	},

	{
		"Ndef",
		"Write",
		net_nfc_test_ndef_write,
		net_nfc_test_ndef_write_sync,
		"Write NDEF"
	},

	{
		"Ndef",
		"Format",
		net_nfc_test_ndef_format,
		net_nfc_test_ndef_format_sync,
		"Format NDEF"
	},

	{
		"Ndef",
		"MakeReadOnly",
		net_nfc_test_ndef_make_read_only,
		net_nfc_test_ndef_make_read_only_sync,
		"Make Tag Read only"
	},

	{
		"p2p",
		"Send",
		net_nfc_test_p2p_send,
		net_nfc_test_p2p_send_sync,
		"P2P Send"

	},

	{
		"p2p",
		"Discovered", 		/* waiting for signal*/
		net_nfc_test_p2p_set_device_discovered,
		NULL,
		"Waiting for  Device Discovered Signal"
	},

	{
		"p2p",
		"Detached", 		/* waiting for signal*/
		net_nfc_test_p2p_set_device_detached,
		NULL,
		"Waiting for  Device Detached Signal"
	},

	{
		"p2p",
		"Received", 		/* waiting for signal*/
		net_nfc_test_p2p_set_data_received,
		NULL,
		"Waiting for Device Data Received Signal"
	},

	{
		"Transceive",
		"TransceiveData",
		net_nfc_test_transceive_data,
		net_nfc_test_transceive_data_sync,
		"Tansceive data method call "
	},

	{
		"Transceive",
		"Transceive",
		net_nfc_test_transceive,
		net_nfc_test_transceive_sync,
		"Tansceive method call"
	},

	{
		"Handover",
		"BTRequest",
		net_nfc_test_p2p_connection_handover_with_BT,
		net_nfc_test_p2p_connection_handover_with_BT_sync,
		"Handover Request Call"

	},

	{
		"Handover",
		"WIFIRequest",
		net_nfc_test_p2p_connection_handover_with_WIFI,
		net_nfc_test_p2p_connection_handover_with_WIFI_sync,
		"Handover Request Call"

	},

	{
		"Handover",
		"GetCarrierType",
		net_nfc_test_handover_get_alternative_carrier_type,
		NULL,
		"Get Handover Carrier Type"

	},

	{
		"Handover",
		"HandleCarrierData",
		net_nfc_test_handover_handle_alternative_carrier_data,
		NULL,
		"Create/Free Handover Carrier Data"

	},

	{
		"JewelTag",
		"ReadId",
		net_nfc_test_tag_jewel_read_id,
		NULL,
		"Jewel Read id"

	},

	{
		"JewelTag",
		"ReadByte",
		net_nfc_test_tag_jewel_read_byte,
		NULL,
		"Jewel Read Byte"

	},

	{
		"JewelTag",
		"ReadAll",
		net_nfc_test_tag_jewel_read_all,
		NULL,
		"Jewel Read All"

	},

	{
		"JewelTag",
		"WriteWithErase",
		net_nfc_test_tag_jewel_write_with_erase,
		NULL,
		"Jewel Write With Erase"

	},

	{
		"JewelTag",
		"WriteWithNoErase",
		net_nfc_test_tag_jewel_write_with_no_erase,
		NULL,
		"Jewel Write With No Erase"

	},

	{
		"MifareTag",
		"Read",
		net_nfc_test_tag_mifare_read,
		NULL,
		"Read Mifare Tag"
	},

	{
		"MifareTag",
		"WriteBlock",
		net_nfc_test_tag_mifare_write_block,
		NULL,
		"Write data block"
	},

	{
		"MifareTag",
		"WritePage",
		net_nfc_test_tag_mifare_write_page,
		NULL,
		"Write Page Data"
	},

	{
		"MifareTag",
		"Increment",
		net_nfc_test_tag_mifare_increment,
		NULL,
		"Increment block value"
	},

	{
		"MifareTag",
		"Decrement",
		net_nfc_test_tag_mifare_decrement,
		NULL,
		"Decrement block value"
	},

	{
		"MifareTag",
		"Transfer",
		net_nfc_test_tag_mifare_transfer,
		NULL,
		" Data Transfer"
	},

	{
		"MifareTag",
		"Restore",
		net_nfc_test_tag_mifare_restore,
		NULL,
		"Data Restore"
	},

	{
		"MifareTag",
		"AuthKeyA",
		net_nfc_test_tag_mifare_authenticate_with_keyA,
		NULL,
		"Authenticate with key A"
	},

	{
		"MifareTag",
		"AuthKeyB",
		net_nfc_test_tag_mifare_authenticate_with_keyB,
		NULL,
		"Authenticate with key B"
	},

	{
		"FelicaTag",
		"FelicaPoll",
		net_nfc_test_felica_poll,
		NULL,
		"Felica Poll"
	},

	{
		"FelicaTag",
		"FelicaRequestService",
		net_nfc_test_felica_request_service,
		NULL,
		"Felica Request Service"
	},

	{
		"FelicaTag",
		"FelicaRequestResponse",
		net_nfc_test_felica_request_response,
		NULL,
		"Felica Request Response"
	},

	{
		"FelicaTag",
		"FelicaReadWithoutEncryption",
		net_nfc_test_felica_read_without_encryption,
		NULL,
		"Felica Read Without Encryption"
	},

	{
		"FelicaTag",
		"FelicaWriteWithoutEncryption",
		net_nfc_test_felica_write_without_encryption,
		NULL,
		"Felica Write Without Encryption"
	},

	{
		"FelicaTag",
		"FelicaRequestSystemCode",
		net_nfc_test_felica_request_system_code,
		NULL,
		"Felica Request System Code"
	},

	{
		"llcp",
		"GetConfigWKS",
		net_nfc_test_llcp_get_config_wks,
		NULL,
		"Get the Config of WKS"
	},


	{
		"llcp",
		"GetConfigLTO",
		net_nfc_test_llcp_get_config_lto,
		NULL,
		"Get the Config of LTO"
	},


	{
		"llcp",
		"GetConfigMIU",
		net_nfc_test_llcp_get_config_miu,
		NULL,
		"Get the Config of MIU"
	},

	{
		"llcp",
		"GetConfigOption",
		net_nfc_test_llcp_get_config_option,
		NULL,
		"Get the Config Option Type"
	},

	{
		"llcp",
		"SetConfigWKS",
		net_nfc_test_llcp_set_config_wks,
		NULL,
		"Set the Config for WKS"
	},


	{
		"llcp",
		"SetConfigLTO",
		net_nfc_test_llcp_set_config_lto,
		NULL,
		"Set the Config for LTO"
	},


	{
		"llcp",
		"SetConfigMIU",
		net_nfc_test_llcp_set_config_miu,
		NULL,
		"Set the Config for MIU"
	},

	{
		"llcp",
		"SetConfigOption",
		net_nfc_test_llcp_set_config_option,
		NULL,
		"Set the Config Option Type"
	},

	{
		"llcp",
		"FreeConfig",
		net_nfc_test_llcp_free_config,
		NULL,
		"Clear the llcp configuration options"
	},


	{
		"llcp",
		"CreateCustomSocketOption",
		net_nfc_test_llcp_create_custom_socket_option,
		NULL,
		"Create Custom Socket Option"
	},

	{
		"llcp",
		"CreateDefaultSocketOption",
		net_nfc_test_llcp_create_default_socket_option,
		NULL,
		"Create Default Socket Option"
	},


	{
		"llcp",
		"GetLocalSocketOption",
		net_nfc_test_llcp_get_local_socket_option,
		NULL,
		"Get Local Socket Option"
	},


	{
		"llcp",
		"GetLocalSocketMIU",
		net_nfc_test_llcp_get_socket_option_miu,
		NULL,
		"Get Local Socket MIU"
	},


	{
		"llcp",
		"GetLocalSocketRW",
		net_nfc_test_llcp_get_socket_option_rw,
		NULL,
		"Get Local Socket RW"
	},


	{
		"llcp",
		"GetLocalSocketOptionType",
		net_nfc_test_llcp_get_socket_option_type,
		NULL,
		"Get Local Socket Option Type"
	},

	{
		"llcp",
		"SetLocalSocketMIU",
		net_nfc_test_llcp_set_socket_option_miu,
		NULL,
		"Set Local Socket MIU"
	},


	{
		"llcp",
		"SetLocalSocketRW",
		net_nfc_test_llcp_set_socket_option_rw,
		NULL,
		"Set Local Socket RW"
	},


	{
		"llcp",
		"SetLocalSocketOptionType",
		net_nfc_test_llcp_set_socket_option_type,
		NULL,
		"Set Local Socket Option Type"
	},

	{
		"llcp",
		"FreeSocketOption",
		net_nfc_test_llcp_free_socket_option,
		NULL,
		"Free Socket Option"
	},

	{
		"llcp",
		"ListenToSocket",
		net_nfc_test_llcp_listen,
		net_nfc_test_llcp_listen_sync,
		"Listen To Server Socket"
	},

	{
		"llcp",
		"ReceiveSocket",
		net_nfc_test_llcp_receive,
		net_nfc_test_llcp_receive_sync,
		"Receive data from socket "
	},

	{
		"llcp",
		"ReceiveFromSocket",
		net_nfc_test_llcp_receive_from,
		net_nfc_test_llcp_receive_from_sync,
		"Receive data from socket (sap data also included)"
	},

	{
		"llcp",
		"ConnectToSocket",
		net_nfc_test_llcp_connect,
		net_nfc_test_llcp_connect_sync,
		"Connect to a socket"
	},

	{
		"llcp",
		"ConnectToSapSocket",
		net_nfc_test_llcp_connect_sap,
		net_nfc_test_llcp_connect_sap_sync,
		"Connect to a specific SAP on the socket"
	},

	{
		"llcp",
		"SendToSocket",
		net_nfc_test_llcp_send,
		net_nfc_test_llcp_send_sync,
		"Send Data to a socket"
	},

	{
		"llcp",
		"SendToSapSocket",
		net_nfc_test_llcp_send_to,
		net_nfc_test_llcp_send_to_sync,
		"Send data to a specific SAP on the socket"
	},

	{
		"llcp",
		"DisconnectSocket",
		net_nfc_test_llcp_disconnect,
		net_nfc_test_llcp_disconnect_sync,
		"Disconnects the client socket"
	},

	{
		"llcp",
		"DisconnectOtherSockets",
		net_nfc_test_llcp_disconnect_server,
		NULL,
		"Disconnects the Server and Accepted sockets"
	},

	{
		"snep",
		"SNEPTagDiscovery",
		net_nfc_test_snep_set_tag_discovered,
		NULL,
		"Discovers the tag/target before starting SNEP operation"
	},

	{
		"snep",
		"SNEPP2PDeviceDiscovery",
		net_nfc_test_snep_set_p2p_device_discovered,
		NULL,
		"Discover the P2P device for snep operation"
	},

	{
		"snep",
		"StartSNEPServer",
		net_nfc_test_snep_start_server,
		net_nfc_test_snep_start_server_sync,
		"Starts the SNEP server"
	},

	{
		"snep",
		"StartSNEPClient",
		net_nfc_test_snep_start_client,
		NULL,
		"Starts the SNEP client"
	},

	{
		"snep",
		"SendClientRequest",
		net_nfc_test_snep_send_client_request,
		NULL,
		"Sends the SNEP client Request"
	},

	{
		"snep",
		"RegisterServer",
		net_nfc_test_snep_register_server,
		NULL,
		"Registers the SNEP server"
	},

	{
		"snep",
		"UnRegisterServer",
		net_nfc_test_snep_unregister_server,
		NULL,
		"UnRegisters the SNEP server"
	},

	{
		"snep",
		"ServerRegisterUnregister",
		net_nfc_test_snep_register_unregister_server,
		NULL,
		"Registers and UnRegisters the SNEP server"
	},

	{
		"snep",
		"StopService",
		net_nfc_test_snep_stop_service_sync,
		NULL,
		"Registers and UnRegisters the SNEP server"
	},

	{
		"SE",
		"SendApdu",
		net_nfc_test_se_send_apdu,
		net_nfc_test_se_send_apdu_sync,
		"Secure element send apdu data"
	},

	{
		"SE",
		"SetType",
		net_nfc_test_se_set_secure_element_type,
		net_nfc_test_se_set_secure_element_type_sync,
		"Set secure element type"
	},

	{
		"SE",
		"Open",
		net_nfc_test_se_open_internal_secure_element,
		net_nfc_test_se_open_internal_secure_element_sync,
		"Open internal secure element"
	},

	{
		"SE",
		"Close",
		net_nfc_test_se_close_internal_secure_element,
		net_nfc_test_se_close_internal_secure_element_sync,
		"Close internal secure element"
	},

	{
		"SE",
		"GetAtr",
		net_nfc_test_se_get_atr,
		net_nfc_test_se_get_atr_sync,
		"Get secure element attribute"
	},

	{
		"SE",
		"SetEventCallback",
		net_nfc_test_se_set_event_cb,
		NULL,
		"Set event callback"
	},

	{
		"SE",
		"UnsetEventCallback",
		net_nfc_test_se_unset_event_cb,
		NULL,
		"Unset event callback"
	},

	{
		"SE",
		"SetDetectionCallback",
		net_nfc_test_se_set_ese_detection_cb,
		NULL,
		"Set detection callback"
	},

	{
		"SE",
		"UnsetDetectionCallback",
		net_nfc_test_se_unset_ese_detection_cb,
		NULL,
		"Unset detection callback"
	},

	{
		"SE",
		"SetTransactionCallback",
		net_nfc_test_se_set_transaction_event_cb,
		NULL,
		"Set transaction  callback"
	},

	{
		"SE",
		"UnsetTransactionCallback",
		net_nfc_test_se_unset_transaction_event_cb,
		NULL,
		"Unset transaction callback"
	},

	{
		"SystemHandler",
		"SetLaunchPopState",
		net_nfc_test_sys_handler_set_launch_popup_state,
		net_nfc_test_sys_handler_set_launch_popup_state_sync,
		"Set launch popup state"
	},

	{
		"SystemHandler",
		"SetLaunchPopStateForce",
		net_nfc_test_sys_handler_set_launch_popup_state_force,
		net_nfc_test_sys_handler_set_launch_popup_state_force_sync,
		"Set launch popup state"
	},

	{
		"SystemHandler",
		"GetLaunchPopState",
		net_nfc_test_sys_handler_get_launch_popup_state,
		NULL,
		"Get launch popup state"
	},

	{ NULL }
};


static GMainLoop *loop = NULL;

static GList *pos = NULL;

static void test_string_free(gpointer data);

static void run_test(void);

static void test_string_free(gpointer data)
{
	gchar **strv;

	if (data)
	{
		strv = data;
		g_strfreev(strv);
	}
}

static void run_test(void)
{
	gchar **strv;
	gint  i = 0;

	if (pos == NULL)
		return;

	strv = pos->data;

	for (i = 0; test_data[i].interface != NULL; i++)
	{
		if (strv[0] == NULL)
			continue;

		if (strv[1] == NULL)
			continue;

		if ((strcmp(strv[0], test_data[i].interface) == 0) &&
				(strcmp(strv[1], test_data[i].name) == 0))
		{
			pos = pos->next;


			if (strv[2] && strcmp(strv[2], "Sync") == 0)
			{
				g_print("run %s.%s(Sync)\n", strv[0], strv[1]);
				test_data[i].func_sync(NULL,
						(gpointer)run_test);
			}
			else
			{
				g_print("run %s.%s\n", strv[0], strv[1]);
				test_data[i].func(NULL,
						(gpointer)run_test);
			}

			break;
		}
	}
}

int main(int argc, char *argv[])
{
	gint i;

	if (argc == 2 && strcmp(argv[1], "--help") == 0)
	{
		g_print("nfc-client-test: nfc-client-test [inteface.name]\n");
		g_print("\n");

		for (i = 0; i < G_N_ELEMENTS(test_data) - 1; i++)
		{
			g_print("\t%s - %s : %s\n", test_data[i].interface,
					test_data[i].name,
					test_data[i].comment);
		}
		return 0;
	}

	for (i = 1 ; i < argc ; i++)
	{
		gchar **strv;

		strv = g_strsplit(argv[i], ".", 3);
		pos = g_list_append(pos, strv);
	}

	if (pos == NULL)
	{
		gchar **strv;

		strv = g_strsplit("Manager.SetActive", ".", 3);
		pos = g_list_append(pos, strv);
	}
	else
	{
		gchar **strv;

		strv = pos->data;

		if (strcmp(strv[0], "Manager") != 0 ||
				strcmp(strv[1], "SetActive") != 0)
		{
			gchar **manager_strv;

			manager_strv = g_strsplit("Manager.SetActive", ".", 3);
			pos = g_list_prepend(pos, manager_strv);
		}
	}

	net_nfc_client_initialize();

	run_test();

	loop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(loop);

	net_nfc_client_deinitialize();

	if (pos)
		g_list_free_full(pos, test_string_free);

	return 0;
}
