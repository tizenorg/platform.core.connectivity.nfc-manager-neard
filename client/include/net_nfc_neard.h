#ifndef NET_NFC_NEARD_H
#define NET_NFC_NEARD_H

#include "net_nfc.h"
#include "net_nfc_typedef.h"

net_nfc_error_e net_nfc_neard_set_active(int state,
		net_nfc_client_manager_set_active_completed callback,
		void *user_data);
net_nfc_error_e net_nfc_neard_initialize(void);
void net_nfc_neard_deinitialize(void);

#endif
