AUTOMAKE_OPTIONS = foreign subdir-objects

# Project sources
bin_PROGRAMS = modbus_server
modbus_server_SOURCES = src/main.c
modbus_server_SOURCES += src/controller.c
modbus_server_SOURCES += src/models.c
modbus_server_SOURCES += src/http_helper.c
modbus_server_SOURCES += src/json_helper.c
modbus_server_SOURCES += src/daemonize.c
modbus_server_LDFLAGS = -pthread

# Include dir
AM_CPPFLAGS = -I$(top_srcdir)/include

# Flags
AM_CPPFLAGS += -Wall -Werror -pedantic
