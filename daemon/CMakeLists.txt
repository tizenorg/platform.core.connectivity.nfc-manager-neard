LINK_DIRECTORIES(${CMAKE_BINARY_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/common/)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/common/include)

SET(NFC_DAEMON "nfc-manager-daemon")

FILE(GLOB DAEMON_SRCS *.c)

IF(ARM_TARGET)
	pkg_check_modules(daemon_pkgs REQUIRED aul glib-2.0 gio-unix-2.0 security-server
		vconf dlog tapi appsvc libcurl bluetooth-api libssl svi capi-media-wav-player
		pkgmgr pkgmgr-info capi-network-wifi ecore-x pmapi wifi-direct
		libtzplatform-config)
ELSE(ARM_TARGET)
	pkg_check_modules(daemon_pkgs REQUIRED aul glib-2.0 gio-unix-2.0 security-server
		vconf dlog tapi appsvc libcurl bluetooth-api libssl svi capi-media-wav-player
		pkgmgr pkgmgr-info capi-network-wifi ecore-x pmapi libtzplatform-config)
ENDIF(ARM_TARGET)

FOREACH(flag ${daemon_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")

ADD_EXECUTABLE(${NFC_DAEMON} ${DAEMON_SRCS})
TARGET_LINK_LIBRARIES(${NFC_DAEMON} ${daemon_pkgs_LDFLAGS} pthread dl rt nfc-common)

INSTALL(TARGETS ${NFC_DAEMON} DESTINATION bin)

INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.tizen.NetNfcService.service DESTINATION share/dbus-1/system-services)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/nfc-manager.xml DESTINATION share/packages)
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sounds DESTINATION share/nfc-manager-daemon)
