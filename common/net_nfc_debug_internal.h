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
#ifndef __NET_NFC_DEBUG_INTERNAL_H__
#define __NET_NFC_DEBUG_INTERNAL_H__

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <libgen.h>
#include <dlog.h>

#define NFC_DEBUGGING

#define LOG_SERVER_TAG "NET_NFC_MANAGER"
#define LOG_CLIENT_TAG "NET_NFC_CLIENT"

#define LOG_COLOR_RED		"\033[0;31m"
#define LOG_COLOR_GREEN		"\033[0;32m"
#define LOG_COLOR_BROWN		"\033[0;33m"
#define LOG_COLOR_BLUE		"\033[0;34m"
#define LOG_COLOR_PURPLE	"\033[0;35m"
#define LOG_COLOR_CYAN		"\033[0;36m"
#define LOG_COLOR_LIGHTBLUE	"\033[0;37m"
#define LOG_COLOR_END		"\033[0;m"

#ifdef API
#undef API
#endif
#define API __attribute__((visibility("default")))

const char* net_nfc_get_log_tag();

#define DBG(fmt, arg...) SLOG(LOG_DEBUG, net_nfc_get_log_tag(), fmt, ##arg)
#define INFO(fmt, arg...) SLOG(LOG_INFO, net_nfc_get_log_tag(), fmt, ##arg)
#define WARN(fmt, arg...) SLOG(LOG_WARN, net_nfc_get_log_tag(), fmt, ##arg)
#define ERR(fmt, arg...) SLOG(LOG_ERROR, net_nfc_get_log_tag(), fmt, ##arg)


#ifdef NFC_DEBUGGING

#define NFC_FN_CALL DBG(">>>>>>>> called")
#define NFC_FN_END DBG("<<<<<<<< ended")
#define NFC_DBG(fmt, arg...) DBG(fmt, ##arg)
#define NFC_WARN(fmt, arg...) WARN(LOG_COLOR_BROWN fmt LOG_COLOR_END, ##arg)
#define NFC_ERR(fmt, arg...) ERR(LOG_COLOR_RED fmt LOG_COLOR_END, ##arg)
#define NFC_INFO(fmt, arg...) INFO(LOG_COLOR_BLUE fmt LOG_COLOR_END, ##arg)
#define NFC_SECURE_DBG(fmt, arg...) \
	SECURE_SLOG(LOG_DEBUG, net_nfc_get_log_tag(), fmt, ##arg)
#define NFC_SECURE_ERR(fmt, arg...) \
	SECURE_SLOG(LOG_ERROR, net_nfc_get_log_tag(), fmt, ##arg)

#else /* NFC_DEBUGGING */

#define NFC_FN_CALL
#define NFC_FN_END
#define NFC_DBG(fmt, arg...)
#define NFC_WARN(fmt, arg...)
#define NFC_ERR(fmt, arg...) ERR(fmt, ##arg)
#define NFC_INFO(fmt, arg...)
#define NFC_VERBOSE(fmt, arg...)
#define NFC_SECURE_DBG(fmt, arg...)
#define NFC_SECURE_ERR(fmt, arg...) \
	SECURE_SLOG(LOG_ERROR, net_nfc_get_log_tag(), fmt, ##arg)

#endif /* NFC_DEBUGGING */


#define DEBUG_MSG_PRINT_BUFFER(buffer, length) \
	do { \
		int i = 0, offset = 0; \
		char temp_buffer[4096] = { 0, }; \
		NFC_INFO("BUFFER [%d] = {", length); \
		for(; i < length && offset < sizeof(temp_buffer); i++) \
		{ \
			offset += snprintf(temp_buffer + offset, sizeof(temp_buffer) - offset, " %02x", buffer[i]); \
			if (i % 16 == 15) \
			{ \
				NFC_INFO("\t%s", temp_buffer); \
				offset = 0; \
			} \
		} \
		NFC_INFO("\t%s", temp_buffer); \
		NFC_INFO("}"); \
	} while(0)

#define PROFILING(str) \
	do { \
		struct timeval mytime;\
		char buf[128]; = {0};\
		memset(buf, 0x00, 128);\
		gettimeofday(&mytime, NULL);\
		char time_string[128] = {0,};\
		sprintf(time_string, "%d.%4d", mytime.tv_sec, mytime.tv_usec);\
		NFC_LOGD(str); \
		NFC_LOGD("\t time = [%s]", time_string);\
	} while(0)

#define RET_IF(expr) \
	do { \
		if (expr) { \
			NFC_ERR("(%s)", #expr); \
			return; \
		}\
	} while(0)
#define RETV_IF(expr, val) \
	do {\
		if (expr) { \
			NFC_ERR("(%s)", #expr); \
			return (val); \
		} \
	} while(0)
#define RETM_IF(expr, fmt, arg...) \
	do {\
		if (expr) { \
			NFC_ERR(fmt, ##arg); \
			return; \
		}\
	} while(0)
#define RETVM_IF(expr, val, fmt, arg...) \
	do {\
		if (expr) { \
			NFC_ERR(fmt, ##arg); \
			return (val); \
		} \
	} while(0)


#endif //__NET_NFC_DEBUG_INTERNAL_H__
