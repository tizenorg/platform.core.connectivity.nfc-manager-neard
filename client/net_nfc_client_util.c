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
#define _GNU_SOURCE
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <curl/curl.h>
#include <glib.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <vconf.h>
#include <svi.h>
#include <wav_player.h>
#include <appsvc.h>
#include <aul.h>
#include <Ecore_X.h>

#include "net_nfc_typedef.h"
#include "net_nfc_typedef_internal.h"
#include "net_nfc_debug_internal.h"
#include "net_nfc_util_defines.h"
#include "net_nfc_util_internal.h"
#include "net_nfc_util_ndef_message.h"
#include "net_nfc_util_ndef_record.h"
#include "net_nfc_client_util.h"
#include "net_nfc_client_system_handler.h"

#define NET_NFC_MANAGER_SOUND_PATH_TASK_START \
		"/usr/share/nfc-manager-daemon/sounds/Operation_sdk.wav"

#define NET_NFC_MANAGER_SOUND_PATH_TASK_END \
		"/usr/share/nfc-manager-daemon/sounds/Operation_sdk.wav"

#define NET_NFC_MANAGER_SOUND_PATH_TASK_ERROR \
		"/usr/share/nfc-manager-daemon/sounds/Operation_sdk.wav"

#define OSP_K_COND		"__OSP_COND_NAME__"
#define OSP_K_COND_TYPE		"nfc"
#define OSP_K_LAUNCH_TYPE	"__OSP_LAUNCH_TYPE__"

static const char osp_launch_type_condition[] = "condition";

static inline int _mkdir_recursive(char *path, mode_t mode)
{
	char *found = path;

	while (found)
	{
		char tmp_ch;

		if ('\0' == found[1])
			break;

		found = strchr(found+1, '/');

		if (found)
		{
			int ret;
			DIR *exist;

			tmp_ch = *found;
			*found = '\0';

			exist = opendir(path);
			if (NULL == exist)
			{
				if (ENOENT == errno)
				{
					ret = mkdir(path, mode);
					if (-1 == ret)
					{
						char buf[1024];
						NFC_ERR("mkdir() Failed(%s)", strerror_r(errno, buf, sizeof(buf)));
						return -1;
					}
				}
				else
				{
					char buf[1024];
					NFC_ERR("opendir() Failed(%s)", strerror_r(errno, buf, sizeof(buf)));
					return -1;
				}
			}
			else
			{
				closedir(exist);
			}
			*found = tmp_ch;
		}
		else
		{
			mkdir(path, mode);
		}
	}
	return 0;
}

static bool _net_nfc_app_util_change_file_owner_permission(FILE *file)
{
	char *buffer = NULL;
	size_t buffer_len = 0;
	struct group grp = { 0, };
	struct passwd pwd = { 0, };
	struct group *gr_inhouse = NULL;
	struct passwd *pw_inhouse = NULL;

	RETV_IF(NULL == file, false);

	/* change permission */
	fchmod(fileno(file), 0777);

	/* change owner */
	/* get passwd id */
	buffer_len = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (buffer_len == -1)
		buffer_len = 16384;

	_net_nfc_util_alloc_mem(buffer, buffer_len);
	if (NULL == buffer)
		return false;

	getpwnam_r("inhouse", &pwd, buffer, buffer_len, &pw_inhouse);
	_net_nfc_util_free_mem(buffer);

	/* get group id */
	buffer_len = sysconf(_SC_GETGR_R_SIZE_MAX);
	if (buffer_len == -1)
		buffer_len = 16384;

	_net_nfc_util_alloc_mem(buffer, buffer_len);
	if (NULL == buffer)
		return false;

	getgrnam_r("inhouse", &grp, buffer, buffer_len, &gr_inhouse);
	_net_nfc_util_free_mem(buffer);

	if ((pw_inhouse != NULL) && (gr_inhouse != NULL))
	{
		if (fchown(fileno(file), pw_inhouse->pw_uid, gr_inhouse->gr_gid) < 0)
			NFC_ERR("failed to change owner");
	}

	return true;
}

static net_nfc_error_e net_nfc_app_util_store_ndef_message(data_s *data)
{
	int ret;
	FILE *fp = NULL;
	char file_name[1024] = { 0, };
	net_nfc_error_e result = NET_NFC_UNKNOWN_ERROR;

	RETV_IF(NULL == data, NET_NFC_NULL_PARAMETER);

	/* check and make directory */
	snprintf(file_name, sizeof(file_name), "%s/%s", NET_NFC_MANAGER_DATA_PATH,
			NET_NFC_MANAGER_DATA_PATH_MESSAGE);

	ret = _mkdir_recursive(file_name, 0755);
	if (-1 == ret)
	{
		NFC_ERR("_mkdir_recursive() Failed");
		return NET_NFC_UNKNOWN_ERROR;
	}

	/* create file */
	snprintf(file_name, sizeof(file_name), "%s/%s/%s", NET_NFC_MANAGER_DATA_PATH,
			NET_NFC_MANAGER_DATA_PATH_MESSAGE, NET_NFC_MANAGER_NDEF_FILE_NAME);
	SECURE_LOGD("file path : %s", file_name);

	unlink(file_name);

	if ((fp = fopen(file_name, "w")) != NULL)
	{
		int length = 0;

		if ((length = fwrite(data->buffer, 1, data->length, fp)) > 0)
		{
			NFC_DBG("[%d] bytes is written", length);

			_net_nfc_app_util_change_file_owner_permission(fp);

			fflush(fp);
			fsync(fileno(fp));

			result = NET_NFC_OK;
		}
		else
		{
			NFC_ERR("write is failed = [%d]", data->length);
			result = NET_NFC_UNKNOWN_ERROR;
		}

		fclose(fp);
	}

	return result;
}

static bool _net_nfc_app_util_get_operation_from_record(
		ndef_record_s *record, char *operation, size_t length)
{
	bool result = false;
	char *op_text = NULL;

	RETV_IF(NULL == record, result);
	RETV_IF(NULL == operation, result);
	RETV_IF(0 == length, result);

	switch (record->TNF)
	{
	case NET_NFC_RECORD_WELL_KNOWN_TYPE :
		op_text = "http://tizen.org/appcontrol/operation/nfc/wellknown";
		break;

	case NET_NFC_RECORD_MIME_TYPE :
		op_text = "http://tizen.org/appcontrol/operation/nfc/mime";
		break;

	case NET_NFC_RECORD_URI : /* Absolute URI */
		op_text = "http://tizen.org/appcontrol/operation/nfc/uri";
		break;

	case NET_NFC_RECORD_EXTERNAL_RTD : /* external type */
	if(strncmp((char*)record->type_s.buffer, NET_NFC_APPLICATION_RECORD,
			record->type_s.length)==0)
	{
		//use APPSVC_OPERATION_VIEW in case of Selective App launch
		op_text = APPSVC_OPERATION_VIEW;
	}
	else
	{
		op_text = "http://tizen.org/appcontrol/operation/nfc/external";
	}
	break;

	case NET_NFC_RECORD_EMPTY : /* empty_tag */
		op_text = "http://tizen.org/appcontrol/operation/nfc/empty";
		break;

	case NET_NFC_RECORD_UNKNOWN : /* unknown msg. discard it */
	case NET_NFC_RECORD_UNCHAGNED : /* RFU msg. discard it */
	default :
		break;
	}

	if (op_text != NULL)
	{
		snprintf(operation, length, "%s", op_text);
		result = TRUE;
	}

	return result;
}

static void _to_lower_utf_8(char *str)
{
	while (*str != 0)
	{
		if (*str >= 'A' && *str <= 'Z')
			*str += ('a' - 'A');

		str++;
	}
}

static void _to_lower(int type, char *str)
{
	_to_lower_utf_8(str);
}

static bool _net_nfc_app_util_get_mime_from_record(
		ndef_record_s *record, char *mime, size_t length)
{
	bool result = false;

	RETV_IF(NULL == record, result);
	RETV_IF(NULL == mime, result);
	RETV_IF(0 == length, result);

	switch (record->TNF)
	{
	case NET_NFC_RECORD_WELL_KNOWN_TYPE :
		{
			if (record->type_s.buffer == NULL || record->type_s.length == 0 ||
					record->payload_s.buffer == NULL || record->payload_s.length == 0)
			{
				NFC_ERR("Broken NDEF Message [NET_NFC_RECORD_WELL_KNOWN_TYPE]");
				break;
			}

			if (record->type_s.length == 1 && record->type_s.buffer[0] == 'U')
			{
				snprintf(mime, length, "U/0x%02x", record->payload_s.buffer[0]);
			}
			else
			{
				memcpy(mime, record->type_s.buffer, record->type_s.length);
				mime[record->type_s.length] = '\0';

				strncat(mime, "/*", 2);
				mime[record->type_s.length + 2] = '\0';
			}

			result = true;
		}
		break;

	case NET_NFC_RECORD_MIME_TYPE :
		{
			int len = 0;
			char *token = NULL;
			char *buffer = NULL;

			if (record->type_s.buffer == NULL || record->type_s.length == 0)
			{
				NFC_ERR("Broken NDEF Message [NET_NFC_RECORD_MIME_TYPE]");
				break;
			}

			/* get mime type */
			_net_nfc_util_alloc_mem(buffer, record->type_s.length + 1);
			if (NULL == buffer)
			{
				NFC_ERR("_net_nfc_manager_util_alloc_mem return NULL");
				break;
			}
			memcpy(buffer, record->type_s.buffer, record->type_s.length);

			token = strchr(buffer, ';');
			if (token != NULL)
				len = MIN(token - buffer, length - 1);
			else
				len = MIN(strlen(buffer), length - 1);

			strncpy(mime, buffer, len);
			mime[len] = '\0';

			_to_lower(0, mime);

			_net_nfc_util_free_mem(buffer);

			result = true;
		}
		break;

	case NET_NFC_RECORD_URI : /* Absolute URI */
	case NET_NFC_RECORD_EXTERNAL_RTD : /* external type */
	case NET_NFC_RECORD_EMPTY :  /* empty_tag */
		result = true;
		break;

	case NET_NFC_RECORD_UNKNOWN : /* unknown msg. discard it */
	case NET_NFC_RECORD_UNCHAGNED : /* RFU msg. discard it */
	default :
		break;
	}

	return result;
}

#ifdef USE_FULL_URI
static bool _net_nfc_app_util_get_uri_from_record(ndef_record_s *record, char *data, size_t length)
{
	bool result = false;

	RETV_IF(NULL == record, result);
	RETV_IF(NULL == data, result);
	RETV_IF(0 == length, result);

	switch (record->TNF)
	{
	case NET_NFC_RECORD_WELL_KNOWN_TYPE :
	case NET_NFC_RECORD_URI : /* Absolute URI */
		{
			char *uri = NULL;

			if (net_nfc_util_create_uri_string_from_uri_record(record, &uri) == NET_NFC_OK &&
					uri != NULL)
			{
				snprintf(data, length, "%s", uri);

				_net_nfc_util_free_mem(uri);
			}
			result = true;
		}
		break;

	case NET_NFC_RECORD_EXTERNAL_RTD : /* external type */
		{
			data_s *type = &record->type_s;

			if (type->length > 0)
			{
#if 0
				char *buffer = NULL;
				int len = strlen(NET_NFC_UTIL_EXTERNAL_TYPE_SCHEME);

				_net_nfc_util_alloc_mem(buffer, type->length + len + 1);
				if (buffer != NULL)
				{
					memcpy(buffer, NET_NFC_UTIL_EXTERNAL_TYPE_SCHEME, len);
					memcpy(buffer + len, type->buffer, type->length);

					/* to lower case!! */
					strlwr(buffer);

					NFC_DBG("uri record : %s", buffer);
					snprintf(data, length, "%s", buffer);

					_net_nfc_util_free_mem(buffer);
				}
#else
				int len = MIN(type->length, length - 1);
				memcpy(data, type->buffer, len);
				data[len] = 0;

				/* to lower case!! */
				_to_lower(0, data);

				result = true;
#endif
			}
		}
		break;

	case NET_NFC_RECORD_MIME_TYPE :
	case NET_NFC_RECORD_EMPTY : /* empy msg. discard it */
		result = true;
		break;

	case NET_NFC_RECORD_UNKNOWN : /* unknown msg. discard it */
	case NET_NFC_RECORD_UNCHAGNED : /* RFU msg. discard it */
	default :
		break;
	}

	return result;
}
#endif

static bool _net_nfc_app_util_get_data_from_record(ndef_record_s *record, char *data, size_t length)
{
	bool result = false;

	RETV_IF(NULL == record, result);
	RETV_IF(NULL == data, result);
	RETV_IF(0 == length, result);

	switch (record->TNF)
	{
	case NET_NFC_RECORD_WELL_KNOWN_TYPE :
		{
			if (record->type_s.buffer == NULL || record->type_s.length == 0
					|| record->payload_s.buffer == NULL || record->payload_s.length == 0)
			{
				NFC_ERR("Broken NDEF Message [NET_NFC_RECORD_WELL_KNOWN_TYPE]");
				break;
			}

			if (record->type_s.length == 1 && record->type_s.buffer[0] == 'T')
			{
				uint8_t *buffer_temp = record->payload_s.buffer;
				uint32_t buffer_length = record->payload_s.length;

				int index = (buffer_temp[0] & 0x3F) + 1;
				int text_length = buffer_length - index;

				memcpy(data, &(buffer_temp[index]), MIN(text_length, length));
			}

			result = true;
		}
		break;

	case NET_NFC_RECORD_MIME_TYPE :
	case NET_NFC_RECORD_URI : /* Absolute URI */
		break;
	case NET_NFC_RECORD_EXTERNAL_RTD : /* external type */
		{
			NFC_DBG("NDEF Message with  external type");
			if(strncmp((char*)record->type_s.buffer, NET_NFC_APPLICATION_RECORD,
			record->type_s.length)==0)
			{
				uint8_t *buffer_temp = record->payload_s.buffer;
				uint32_t buffer_length = record->payload_s.length;
				if(buffer_length > length)
				{
					result= false;
				}
				else
				{
					//Copy application id into data
					memcpy(data,buffer_temp,MIN(buffer_length,length));
					result = true;
				}
			}
		}
		break;
	case NET_NFC_RECORD_EMPTY : /* empy msg. discard it */
		result = true;
		break;

	case NET_NFC_RECORD_UNKNOWN : /* unknown msg. discard it */
	case NET_NFC_RECORD_UNCHAGNED : /* RFU msg. discard it */
	default :
		break;
	}

	return result;
}

net_nfc_error_e net_nfc_app_util_process_ndef(data_s *data)
{
	int ret = 0;
	char mime[2048] = { 0, };
	char text[2048] = { 0, };
	ndef_message_s *msg = NULL;
	char operation[2048] = { 0, };
	net_nfc_error_e result = NET_NFC_UNKNOWN_ERROR;
#ifdef USE_FULL_URI
	char uri[2048] = { 0, };
#endif

	RETV_IF(NULL == data, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == data->buffer, NET_NFC_NULL_PARAMETER);
	RETV_IF(NULL == data->length, NET_NFC_NULL_PARAMETER);

	/* create file */
	if ((result = net_nfc_app_util_store_ndef_message(data)) != NET_NFC_OK)
	{
		NFC_ERR("net_nfc_app_util_store_ndef_message failed [%d]", result);
		return result;
	}

	/* check state of launch popup */
	if(net_nfc_app_util_check_launch_state() == NET_NFC_NO_LAUNCH_APP_SELECT)
	{
		NFC_DBG("skip launch popup!!!");
		result = NET_NFC_OK;
		return result;
	}

	if (net_nfc_util_create_ndef_message(&msg) != NET_NFC_OK)
	{
		NFC_ERR("memory alloc fail..");
		return NET_NFC_ALLOC_FAIL;
	}

	/* parse ndef message and fill appsvc data */
	if ((result = net_nfc_util_convert_rawdata_to_ndef_message(data, msg)) != NET_NFC_OK)
	{
		NFC_ERR("net_nfc_app_util_store_ndef_message failed [%d]", result);
		goto ERROR;
	}

	if (_net_nfc_app_util_get_operation_from_record(msg->records, operation,
				sizeof(operation)) == FALSE)
	{
		NFC_ERR("_net_nfc_app_util_get_operation_from_record failed [%d]", result);
		result = NET_NFC_UNKNOWN_ERROR;
		goto ERROR;
	}

	if (_net_nfc_app_util_get_mime_from_record(msg->records, mime, sizeof(mime)) == FALSE)
	{
		NFC_ERR("_net_nfc_app_util_get_mime_from_record failed [%d]", result);
		result = NET_NFC_UNKNOWN_ERROR;
		goto ERROR;
	}
#ifdef USE_FULL_URI
	if (_net_nfc_app_util_get_uri_from_record(msg->records, uri, sizeof(uri)) == FALSE)
	{
		NFC_ERR("_net_nfc_app_util_get_uri_from_record failed [%d]", result);
		result = NET_NFC_UNKNOWN_ERROR;
		goto ERROR;
	}
#endif
	/* launch appsvc */
	if (_net_nfc_app_util_get_data_from_record(msg->records, text, sizeof(text)) == FALSE)
	{
		NFC_ERR("_net_nfc_app_util_get_data_from_record failed [%d]", result);
		result = NET_NFC_UNKNOWN_ERROR;
		goto ERROR;
	}

	ret = net_nfc_app_util_appsvc_launch(operation, uri, mime, text);
#if 0
	if (ret == APPSVC_RET_ENOMATCH)
	{
		/* TODO : check again */
		ret = net_nfc_app_util_appsvc_launch(operation, uri, mime, text);
	}
#endif

	NFC_DBG("net_nfc_app_util_appsvc_launch return %d", ret);

	result = NET_NFC_OK;

ERROR :
	net_nfc_util_free_ndef_message(msg);

	return result;
}


static bool net_nfc_app_util_is_dir(const char* path_name)
{
	struct stat statbuf = { 0 };

	if (stat(path_name, &statbuf) == -1)
		return false;

	if (S_ISDIR(statbuf.st_mode) != 0)
		return true;
	else
		return false;
}

void net_nfc_app_util_clean_storage(char* src_path)
{
	DIR* dir = NULL;
	char path[1024] = { 0 };
	struct dirent* ent = NULL;

	RET_IF((dir = opendir(src_path)) == NULL);

	while ((ent = readdir(dir)) != NULL)
	{
		if (strncmp(ent->d_name, ".", 1) == 0 || strncmp(ent->d_name, "..", 2) == 0)
		{
			continue;
		}
		else
		{
			snprintf(path, 1024, "%s/%s", src_path, ent->d_name);

			if (net_nfc_app_util_is_dir(path) != false)
			{
				net_nfc_app_util_clean_storage(path);
				rmdir(path);
			}
			else
			{
				unlink(path);
			}
		}
	}

	closedir(dir);

	rmdir(src_path);
}

void net_nfc_app_util_aul_launch_app(char* package_name, bundle* kb)
{
	int result = 0;
	if((result = aul_launch_app(package_name, kb)) < 0)
	{
		switch(result)
		{
		case AUL_R_EINVAL:
			NFC_ERR("aul launch error : AUL_R_EINVAL");
			break;
		case AUL_R_ECOMM:
			NFC_ERR("aul launch error : AUL_R_ECOM");
			break;
		case AUL_R_ERROR:
			NFC_ERR("aul launch error : AUL_R_ERROR");
			break;
		default:
			NFC_ERR("aul launch error : unknown ERROR");
			break;
		}
	}
	else
	{
		NFC_DBG("success to launch [%s]", package_name);
	}
}

int net_nfc_app_util_appsvc_launch(const char *operation, const char *uri, const char *mime, const char *data)
{
	int result = -1;

	bundle *bd = NULL;
	bool specific_app_launch = false;
	bd = bundle_create();
	if (NULL == bd)
		return result;

	if (operation != NULL && strlen(operation) > 0)
	{
		NFC_DBG("operation : %s", operation);
		appsvc_set_operation(bd, operation);
		if(strncmp(operation, APPSVC_OPERATION_VIEW,strlen(APPSVC_OPERATION_VIEW))==0)
		{
			appsvc_set_appid(bd, data);
			specific_app_launch = true;
			goto LAUNCH;
		}
	}

	if (uri != NULL && strlen(uri) > 0)
	{
		NFC_DBG("uri : %s", uri);
		appsvc_set_uri(bd, uri);
	}

	if (mime != NULL && strlen(mime) > 0)
	{
		NFC_DBG("mime : %s", mime);
		appsvc_set_mime(bd, mime);
	}

	if (data != NULL && strlen(data) > 0)
	{
		NFC_DBG("data : %s", data);
		appsvc_add_data(bd, "data", data);
	}

	bundle_add(bd, OSP_K_COND, OSP_K_COND_TYPE);
	bundle_add(bd, OSP_K_LAUNCH_TYPE, osp_launch_type_condition);

LAUNCH:
	result = appsvc_run_service(bd, 0, NULL, NULL);

	/*if the app could not be found*/
	if(specific_app_launch && result == APPSVC_RET_ENOMATCH)
	{
		/*TODO: tizen store launch*/
	}

	bundle_free(bd);

	return result;
}

void _binary_to_string(uint8_t *buffer, uint32_t len, char *out_buf, uint32_t max_len)
{
	int current = 0;

	RET_IF(0 == len);
	RET_IF(0 == max_len);
	RET_IF(NULL == buffer);
	RET_IF(NULL == out_buf);

	while (len > 0 && current < max_len)
	{
		current += snprintf(out_buf + current, max_len - current, "%02X", *(buffer++));
		len--;
	}
}

void _string_to_binary(const char *input, uint8_t *output, uint32_t *length)
{
	int temp;
	int current = 0;

	RET_IF(NULL == input);
	RET_IF(NULL == length);
	RET_IF(0 == *length);
	RET_IF(NULL == output);
	NFC_DBG("_string_to_binary ");

	/* strlen("nfc://secure/aid/") = 17 */

	input += 17;

	while (*input && (current < *length))
	{
		temp = (*input++) - '0';

		if(temp > 9)
			temp -= 7;

		if(current % 2)
			output[current / 2] += temp;
		else
			output[current / 2] = temp << 4;

		current++;
	}

	*length = current / 2;
}

int net_nfc_app_util_launch_se_transaction_app(
		net_nfc_secure_element_type_e se_type,
		uint8_t *aid,
		uint32_t aid_len,
		uint8_t *param,
		uint32_t param_len)
{
	bundle *bd = NULL;

	/* launch */
	bd = bundle_create();

	appsvc_set_operation(bd, "http://tizen.org/appcontrol/operation/nfc/transaction");

	/* convert aid to aid string */
	if (aid != NULL && aid_len > 0)
	{
		char temp_string[1024] = { 0, };
		char aid_string[1024] = { 0, };

		_binary_to_string(aid, aid_len, temp_string, sizeof(temp_string));

		switch(se_type)
		{
		case SECURE_ELEMENT_TYPE_UICC:
			snprintf(aid_string, sizeof(aid_string), "nfc://secure/SIM1/aid/%s", temp_string);
			break;

		case SECURE_ELEMENT_TYPE_ESE:
			snprintf(aid_string, sizeof(aid_string), "nfc://secure/eSE/aid/%s", temp_string);
			break;
		default:
			snprintf(aid_string, sizeof(aid_string), "nfc://secure/aid/%s", temp_string);
			break;
		}

		NFC_DBG("aid_string : %s", aid_string);
		appsvc_set_uri(bd, aid_string);
	}

	if (param != NULL && param_len > 0)
	{
		char param_string[1024] = { 0, };

		_binary_to_string(param, param_len, param_string, sizeof(param_string));
		NFC_DBG("param_string : %s", param_string);
		appsvc_add_data(bd, "data", param_string);
	}

	appsvc_run_service(bd, 0, NULL, NULL);
	bundle_free(bd);

	return 0;
}

int net_nfc_app_util_encode_base64(uint8_t *buffer, uint32_t buf_len, char *result, uint32_t max_result)
{
	int ret = -1;
	BUF_MEM *bptr;
	BIO *b64, *bmem;

	RETV_IF(NULL == buffer, ret);
	RETV_IF(0 == buf_len, ret);
	RETV_IF(NULL == result, ret);
	RETV_IF(0 == max_result, ret);

	/* base 64 */
	b64 = BIO_new(BIO_f_base64());
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);

	BIO_write(b64, buffer, buf_len);
	BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	memset(result, 0, max_result);
	memcpy(result, bptr->data, MIN(bptr->length, max_result - 1));

	BIO_free_all(b64);

	ret = 0;

	return ret;
}

int net_nfc_app_util_decode_base64(const char *buffer, uint32_t buf_len, uint8_t *result, uint32_t *res_len)
{
	int ret = -1;
	char *temp = NULL;

	RETV_IF(NULL == buffer, ret);
	RETV_IF(0 == buf_len, ret);
	RETV_IF(NULL == result, ret);
	RETV_IF(NULL == res_len, ret);
	RETV_IF(0 == *res_len, ret);

	_net_nfc_util_alloc_mem(temp, buf_len);
	if (temp != NULL)
	{
		BIO *b64, *bmem;
		uint32_t temp_len;

		b64 = BIO_new(BIO_f_base64());
		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
		bmem = BIO_new_mem_buf((void *)buffer, buf_len);
		bmem = BIO_push(b64, bmem);

		temp_len = BIO_read(bmem, temp, buf_len);

		BIO_free_all(bmem);

		memset(result, 0, *res_len);
		memcpy(result, temp, MIN(temp_len, *res_len));

		*res_len = MIN(temp_len, *res_len);

		_net_nfc_util_free_mem(temp);

		ret = 0;
	}
	else
	{
		NFC_ERR("alloc failed");
	}

	return ret;
}

pid_t net_nfc_app_util_get_focus_app_pid()
{
	pid_t pid;
	Ecore_X_Window focus;

	ecore_x_init(":0");

	focus = ecore_x_window_focus_get();
	if (ecore_x_netwm_pid_get(focus, &pid))
		return pid;

	return -1;
}

bool net_nfc_app_util_check_launch_state()
{
	pid_t focus_app_pid;
	int state;

	focus_app_pid = net_nfc_app_util_get_focus_app_pid();
	net_nfc_client_sys_handler_get_launch_popup_state(&state);

	return state;
}

static void _play_sound_callback(int id, void *data)
{
	NFC_DBG("_play_sound_callback");

	if (WAV_PLAYER_ERROR_NONE != wav_player_stop(id))
		NFC_ERR("wav_player_stop failed");
}

void net_nfc_manager_util_play_sound(net_nfc_sound_type_e sound_type)
{
	int bSoundOn = 0;
	int bVibrationOn = 0;

	if (vconf_get_bool(VCONFKEY_SETAPPL_SOUND_STATUS_BOOL, &bSoundOn) != 0)
	{
		NFC_ERR("vconf_get_bool failed for Sound");
		return;
	}

	if (vconf_get_bool(VCONFKEY_SETAPPL_VIBRATION_STATUS_BOOL, &bVibrationOn) != 0)
	{
		NFC_ERR("vconf_get_bool failed for Vibration");
		return;
	}

	if ((sound_type > NET_NFC_TASK_ERROR) || (sound_type < NET_NFC_TASK_START))
	{
		NFC_ERR("Invalid Sound Type");
		return;
	}

	if (bVibrationOn)
	{
		int svi_handle = -1;

		NFC_DBG("Play Vibration");

		if (SVI_SUCCESS == svi_init(&svi_handle))
		{
			if (SVI_SUCCESS == svi_play_vib(svi_handle, SVI_VIB_TOUCH_SIP))
				NFC_DBG("svi_play_vib success");

			svi_fini(svi_handle);
		}
	}

	if (bSoundOn)
	{
		char *sound_path = NULL;

		NFC_DBG("Play Sound");

		switch (sound_type)
		{
		case NET_NFC_TASK_START :
			sound_path = strdup(NET_NFC_MANAGER_SOUND_PATH_TASK_START);
			break;
		case NET_NFC_TASK_END :
			sound_path = strdup(NET_NFC_MANAGER_SOUND_PATH_TASK_END);
			break;
		case NET_NFC_TASK_ERROR :
			sound_path = strdup(NET_NFC_MANAGER_SOUND_PATH_TASK_ERROR);
			break;
		}

		if (sound_path != NULL)
		{
			if (WAV_PLAYER_ERROR_NONE ==
				wav_player_start(sound_path, SOUND_TYPE_MEDIA, _play_sound_callback,
					NULL, NULL))
			{
				NFC_DBG("wav_player_start success");
			}

			_net_nfc_util_free_mem(sound_path);
		}
		else
		{
			NFC_ERR("Invalid Sound Path");
		}
	}
}
