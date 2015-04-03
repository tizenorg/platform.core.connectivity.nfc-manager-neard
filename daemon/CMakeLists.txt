LINK_DIRECTORIES(${CMAKE_BINARY_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/common/)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/common/include)

SET(NFC_DAEMON "nfc-manager-daemon")

FILE(GLOB DAEMON_SRCS *.c)
#LIST(REMOVE_ITEM DAEMON_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/net_nfc_server_process_handover.c)
LIST(REMOVE_ITEM DAEMON_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/net_nfc_server_handover_bt.c)

IF(X11_SUPPORT)
       SET(WIN_PKG "ecore-x")
ENDIF(X11_SUPPORT)
IF(WAYLAND_SUPPORT)
       SET(WIN_PKG "${WIN_PKG} ecore-wayland")
ENDIF(WAYLAND_SUPPORT)
INCLUDE(FindPkgConfig)
IF(ARM_TARGET)
	pkg_check_modules(daemon_pkgs REQUIRED aul glib-2.0 gio-unix-2.0
		vconf dlog tapi appsvc libcurl libssl svi capi-media-wav-player
		pkgmgr pkgmgr-info capi-network-wifi pmapi wifi-direct
		libtzplatform-config neardal ${WIN_PKG})
ELSE(ARM_TARGET)
	pkg_check_modules(daemon_pkgs REQUIRED aul glib-2.0 gio-unix-2.0
		vconf dlog tapi appsvc libcurl libssl svi capi-media-wav-player
		pkgmgr pkgmgr-info capi-network-wifi pmapi libtzplatform-config neardal
		${WIN_PKG})
ENDIF(ARM_TARGET)

IF(WAYLAND_SUPPORT)
 ADD_DEFINITIONS("-DHAVE_WAYLAND")
ENDIF(WAYLAND_SUPPORT)

IF(X11_SUPPORT)
 ADD_DEFINITIONS("-DHAVE_X11")
ENDIF(X11_SUPPORT)

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
