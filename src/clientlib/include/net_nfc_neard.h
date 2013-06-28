#ifndef NET_NFC_NEARD_H
#define NET_NFC_NEARD_H

#include "net_nfc_typedef.h"
#include "net_nfc_typedef_private.h"

net_nfc_error_e net_nfc_neard_register_cb(net_nfc_response_cb cb);
net_nfc_error_e net_nfc_neard_unregister_cb(void);
net_nfc_error_e net_nfc_neard_is_tag_connected(int *dev_type);
net_nfc_error_e net_nfc_neard_enable(void);
net_nfc_error_e net_nfc_neard_disable(void);
net_nfc_error_e net_nfc_neard_initialize(void);
void net_nfc_neard_deinitialize(void);

#endif
