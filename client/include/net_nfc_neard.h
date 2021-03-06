#ifndef NET_NFC_NEARD_H
#define NET_NFC_NEARD_H

#include "net_nfc.h"
#include "net_nfc_typedef.h"

net_nfc_error_e net_nfc_neard_p2p_connection_handover(
		net_nfc_target_handle_s *handle,
		net_nfc_conn_handover_carrier_type_e arg_type,
		net_nfc_p2p_connection_handover_completed_cb callback,
		void *cb_data);
net_nfc_error_e net_nfc_neard_send_p2p(net_nfc_target_handle_s *handle, data_s *data,
			net_nfc_client_p2p_send_completed callback, void *user_data);
net_nfc_error_e net_nfc_neard_get_current_tag_info(net_nfc_target_info_s **info);
net_nfc_error_e net_nfc_neard_get_current_target_handle(
			net_nfc_target_handle_s **handle);
net_nfc_error_e net_nfc_neard_read_tag(net_nfc_target_handle_s *handle,
		net_nfc_client_ndef_read_completed callback, void *user_data);
net_nfc_error_e net_nfc_neard_write_ndef(net_nfc_target_handle_s *handle,
					data_s *data,
					net_nfc_client_ndef_write_completed callback,
					void *user_data);
net_nfc_error_e net_nfc_neard_set_active(int state,
		net_nfc_client_manager_set_active_completed callback,
		void *user_data);
void net_nfc_neard_set_activated(net_nfc_client_manager_activated callback,
		void *user_data);
void net_nfc_neard_unset_activated(void);
void net_nfc_neard_set_p2p_discovered(
		net_nfc_client_p2p_device_discovered callback, void *user_data);
void net_nfc_neard_unset_p2p_discovered(void);
void net_nfc_neard_set_p2p_detached(
		net_nfc_client_p2p_device_detached callback, void *user_data);
void net_nfc_neard_unset_p2p_detached(void);
void net_nfc_neard_set_p2p_data_received(
		net_nfc_client_p2p_data_received callback, void *user_data);
void net_nfc_neard_unset_p2p_data_received(void);
void net_nfc_neard_set_tag_discovered(
		net_nfc_client_tag_tag_discovered callback, void *user_data);
void net_nfc_neard_unset_tag_discovered(void);
void net_nfc_neard_set_tag_detached(
		net_nfc_client_tag_tag_detached callback, void *user_data);
void net_nfc_neard_unset_tag_detached(void);
bool net_nfc_neard_is_tag_connected(void);
net_nfc_error_e net_nfc_neard_initialize(void);
void net_nfc_neard_deinitialize(void);

#endif
