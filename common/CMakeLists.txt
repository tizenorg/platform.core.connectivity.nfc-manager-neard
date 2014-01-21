INCLUDE_DIRECTORIES(include)

SET(NFC_COMMON "nfc-common")

FIND_PROGRAM(GDBUS_CODEGEN NAMES gdbus-codegen)
EXEC_PROGRAM(${GDBUS_CODEGEN} ARGS
	" --generate-c-code ${CMAKE_CURRENT_SOURCE_DIR}/net_nfc_gdbus"
	" --c-namespace NetNfcGDbus"
	" --interface-prefix org.tizen.NetNfcService."
	" ${CMAKE_CURRENT_SOURCE_DIR}/net_nfc.xml"
)

AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR} COMMON_SRCS)

pkg_check_modules(common_pkgs REQUIRED glib-2.0 gio-2.0 gio-unix-2.0 dlog bluetooth-api
	libssl libtzplatform-config)

FOREACH(flag ${common_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")

ADD_LIBRARY(${NFC_COMMON} STATIC ${COMMON_SRCS})
TARGET_LINK_LIBRARIES(${NFC_COMMON} ${common_pkgs_LDFLAGS})

FILE(GLOB COMMON_HEADER include/*.h)

CONFIGURE_FILE(${NFC_COMMON}.pc.in ${NFC_COMMON}.pc @ONLY)
INSTALL(FILES ${NFC_COMMON}.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)

FOREACH(hfile ${COMMON_HEADER})
        INSTALL(FILES ${hfile} DESTINATION include/${NFC_COMMON})
ENDFOREACH(hfile)

