AC_INIT([modbus_server], [0.1], [alejandro.vicario@tychetools.com])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Check dependencies
AC_CHECK_LIB([modbus], [modbus_new_tcp], [], [AC_MSG_ERROR([libmodbus no encontrado])])
AC_CHECK_LIB([cjson], [cJSON_Parse], [], [AC_MSG_ERROR([cJSON no encontrado])])
AC_CHECK_LIB([curl], [curl_easy_init], [], [AC_MSG_ERROR([curl no encontrado])])

# Configuration options
AC_PROG_CC
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h stdio.h])

# Output files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AC_OUTPUT



