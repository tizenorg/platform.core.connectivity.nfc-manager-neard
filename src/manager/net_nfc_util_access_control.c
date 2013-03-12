/*
  * Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
  *
  * Licensed under the Flora License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at

  *     http://floralicense.org/license/
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include "package-manager.h"
#include "SEService.h"
#include "Reader.h"
#include "Session.h"
#include "ClientChannel.h"
#include "GPSEACL.h"

#include "net_nfc_debug_private.h"
#include "net_nfc_util_private.h"
#include "net_nfc_manager_util_private.h"
#include "net_nfc_util_openssl_private.h"

static bool initialized = false;
static se_service_h se_service = NULL;
static pthread_mutex_t g_access_control_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_access_control_cond = PTHREAD_COND_INITIALIZER;

#if 0
static reader_h readers[10] = { NULL, };
static session_h sessions[10] = { NULL, };
static channel_h channels[10] = { NULL, };
static gp_se_acl_h acls[10] = { NULL, };
#endif

static void _se_service_connected_cb(se_service_h handle, void *data)
{
	if (handle != NULL)
	{
		se_service = handle;
		initialized = true;
	}
	else
	{
		DEBUG_ERR_MSG("invalid handle");
	}
	pthread_cond_signal(&g_access_control_cond);
}

bool net_nfc_util_access_control_is_initialized(void)
{
	return initialized;
}

void net_nfc_util_access_control_initialize(void)
{
	struct timeval now;
	struct timespec ts;

	if (net_nfc_util_access_control_is_initialized() == false)
	{
		pthread_mutex_lock(&g_access_control_lock);
		if (se_service_create_instance((void *)1, _se_service_connected_cb) == NULL)
		{
			DEBUG_ERR_MSG("se_service_create_instance failed");
			pthread_mutex_unlock(&g_access_control_lock);
			return;
		}
		gettimeofday(&now, NULL);
		ts.tv_sec = now.tv_sec + 1;
		ts.tv_nsec = now.tv_usec * 1000;

		pthread_cond_timedwait(&g_access_control_cond, &g_access_control_lock, &ts);
		pthread_mutex_unlock(&g_access_control_lock);
	}
}

void net_nfc_util_access_control_update_list(void)
{
#if 0
	int i;

	if (net_nfc_util_access_control_is_initialized() == true)
	{
		for (i = 0; i < (sizeof(acls) / sizeof(gp_se_acl_h)); i++)
		{
			if (acls[i] != NULL)
			{
				gp_se_acl_update_acl(acls[i]);
			}
		}
	}
#endif
}

static gp_se_acl_h _get_acl(reader_h reader)
{
	gp_se_acl_h result = NULL;
	session_h session = NULL;

	session = reader_open_session_sync(reader);
	if (session != NULL)
	{
		unsigned char aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x63, 0x50, 0x4B, 0x43, 0x53, 0x2D, 0x31, 0x35 };
		channel_h channel = NULL;

		channel = session_open_basic_channel_sync(session, aid, sizeof(aid));
		if (channel != NULL)
		{
			result = gp_se_acl_create_instance(channel);
			if (result != NULL)
			{
				gp_se_acl_update_acl(result, channel);
			}
			channel_close_sync(channel);
		}
		session_close_sync(session);
	}

	return result;
}

static bool _is_authorized_package(gp_se_acl_h acl, const char *value, uint8_t *aid, uint32_t aid_len)
{
	bool result = false;
	uint32_t decoded_len;
	uint8_t *decoded = NULL;

	if (value == NULL)
	{
		return result;
	}

	decoded_len = strlen(value);

	if (decoded_len == 0)
	{
		return result;
	}

	_net_nfc_util_alloc_mem(decoded, decoded_len);
	if (decoded != NULL)
	{
		if (net_nfc_util_openssl_decode_base64(value, decoded, &decoded_len, false) == true)
		{
			uint8_t hash[128];
			uint32_t hash_len = sizeof(hash);

			if (net_nfc_util_openssl_digest("sha1", decoded, decoded_len, hash, &hash_len) == true)
			{
				DEBUG_MSG_PRINT_BUFFER(hash, hash_len);
				result = gp_se_acl_is_authorized_access(acl, aid, aid_len, hash, hash_len);
			}
		}

		_net_nfc_util_free_mem(decoded);
	}
	else
	{
		DEBUG_ERR_MSG("alloc failed");
	}

	return result;
}

static pkgmgr_certinfo_h _get_cert_info(const char *pkg_name)
{
	int ret = 0;
	pkgmgr_certinfo_h handle = NULL;

	DEBUG_MSG("package name : %s", pkg_name);

	if ((ret = pkgmgr_pkginfo_create_certinfo(&handle)) == 0)
	{
		if ((ret = pkgmgr_pkginfo_load_certinfo(pkg_name, handle)) == 0)
		{
		}
		else
		{
			DEBUG_ERR_MSG("pkgmgr_pkginfo_load_certinfo failed [%d]", ret);
			pkgmgr_pkginfo_destroy_certinfo(handle);
			handle = NULL;
		}
	}
	else
	{
		DEBUG_ERR_MSG("pkgmgr_pkginfo_create_certinfo failed [%d]", ret);
	}

	return handle;
}

bool net_nfc_util_access_control_is_authorized_package(const char *pkg_name, uint8_t *aid, uint32_t length)
{
	bool result = false;

	DEBUG_SERVER_MSG("aid : { %02X %02X %02X %02X ... }", aid[0], aid[1], aid[2], aid[3]);

	net_nfc_util_access_control_initialize();
	{
		pkgmgr_certinfo_h cert_info = NULL;

		cert_info = _get_cert_info(pkg_name);
		if (cert_info != NULL)
		{
			int i;
			reader_h readers[10] = { NULL, };
			int count = (sizeof(readers) / sizeof(reader_h));

			se_service_get_readers(se_service, readers, &count);

			for (i = 0; i < count && result == false; i++)
			{
				gp_se_acl_h acl = NULL;

				acl = _get_acl(readers[i]);
				if (acl != NULL)
				{
					int j;
					const char *value = NULL;

					for (j = (int)PM_AUTHOR_ROOT_CERT;
						j <= (int)PM_DISTRIBUTOR2_SIGNER_CERT && result == false;
						j++)
					{
						pkgmgr_pkginfo_get_cert_value(cert_info, (pkgmgr_cert_type)j, &value);
						result = _is_authorized_package(acl, value, aid, length);
					}
					gp_se_acl_destroy_instance(acl);
				}
			}

			pkgmgr_pkginfo_destroy_certinfo(cert_info);
		}
		else
		{
			/* hash not found */
			DEBUG_ERR_MSG("hash doesn't exist : %s", pkg_name);
		}
	}

	DEBUG_ERR_MSG("net_nfc_util_access_control_is_authorized_package end [%d]", result);

	return result;
}

void net_nfc_util_access_control_release(void)
{
#if 0
	int i;

	for (i = 0; i < (sizeof(acls) / sizeof(gp_se_acl_h)); i++)
	{
		if (acls[i] != NULL)
		{
			gp_se_acl_destroy_instance(acls[i]);
			acls[i] = NULL;
		}
	}
#endif
	if (se_service != NULL)
	{
		se_service_destroy_instance(se_service);
		se_service = NULL;
#if 0
		memset(readers, 0, sizeof(readers));
		memset(sessions, 0, sizeof(sessions));
		memset(channels, 0, sizeof(channels));
#endif
	}

	initialized = false;
}
