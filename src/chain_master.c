#include <cjson/cJSON.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "chain_master.h"
#include "register_map.h"

/* Modbus RTU unit IDs: 0 is broadcast, 248-255 are reserved. */
#define UNIT_ID_MIN 1
#define UNIT_ID_MAX 247

/* Global variables ***********************************************************/

typedef struct chain_unit_t {
	bool seen;     /* answered at least once since the master started */
	bool online;
	int misses;    /* polls without an answer, in a row */
	time_t last_seen;
	uint16_t regs[N_READ_REGISTERS];
	uint8_t coils[N_READ_BITS];
	bool have_coils;
} chain_unit_t;

static chain_unit_t units[UNIT_ID_MAX + 1];

/* Function definitions *******************************************************/

static cJSON* scaled_array(const uint16_t* regs, int count, double scale)
{
	cJSON* array = cJSON_CreateArray();
	for (int i = 0; i < count; i++) {
		/* Registers carry signed 16-bit values. */
		double value = (int16_t)regs[i] / scale;
		cJSON_AddItemToArray(array, cJSON_CreateNumber(value));
	}
	return array;
}

/* Writes the chain status atomically, so readers never see half a file. */
static void write_status(const models_modbus_t* cfg)
{
	cJSON* root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "updated", (double)time(NULL));
	cJSON_AddNumberToObject(root, "master", cfg->addr);
	cJSON_AddNumberToObject(root, "scan_first", cfg->scan_first);
	cJSON_AddNumberToObject(root, "scan_last", cfg->scan_last);
	cJSON* list = cJSON_AddArrayToObject(root, "units");

	for (int id = UNIT_ID_MIN; id <= UNIT_ID_MAX; id++) {
		const chain_unit_t* unit = &units[id];
		if (!unit->seen) {
			continue;
		}
		cJSON* item = cJSON_CreateObject();
		cJSON_AddNumberToObject(item, "id", id);
		cJSON_AddBoolToObject(item, "online", unit->online);
		cJSON_AddNumberToObject(item, "misses", unit->misses);
		cJSON_AddNumberToObject(item, "last_seen", (double)unit->last_seen);
		cJSON_AddItemToObject(item, "input_current",
				scaled_array(&unit->regs[REG_INPUT_CURRENT], MAX_INPUTS, 100.0));
		cJSON_AddItemToObject(item, "input_power",
				scaled_array(&unit->regs[REG_INPUT_POWER], MAX_INPUTS, 1.0));
		cJSON_AddItemToObject(item, "outlet_current",
				scaled_array(&unit->regs[REG_OUTLET_CURRENT], MAX_OUTLETS, 100.0));
		cJSON_AddItemToObject(item, "outlet_power",
				scaled_array(&unit->regs[REG_OUTLET_POWER], MAX_OUTLETS, 1.0));
		if (unit->have_coils) {
			cJSON* coils = cJSON_CreateArray();
			for (int i = 0; i < N_READ_BITS; i++) {
				cJSON_AddItemToArray(coils, cJSON_CreateBool(unit->coils[i]));
			}
			cJSON_AddItemToObject(item, "outlet_on", coils);
		}
		cJSON_AddItemToArray(list, item);
	}

	char* text = cJSON_Print(root);
	cJSON_Delete(root);
	if (text == NULL) {
		return;
	}
	FILE* f = fopen(CHAIN_STATUS_FILE ".tmp", "w");
	if (f != NULL) {
		fputs(text, f);
		fclose(f);
		rename(CHAIN_STATUS_FILE ".tmp", CHAIN_STATUS_FILE);
	} else {
		syslog(LOG_ERR, "Can not write %s: %s", CHAIN_STATUS_FILE, strerror(errno));
	}
	free(text);
}

static void poll_unit(modbus_t* ctx, int id, const models_modbus_t* cfg)
{
	chain_unit_t* unit = &units[id];
	uint16_t regs[N_READ_REGISTERS];

	modbus_set_slave(ctx, id);
	int rc = modbus_read_registers(ctx, 0, N_READ_REGISTERS, regs);
	if (rc != N_READ_REGISTERS) {
		/* Drop any half-received reply so it can't be read as the next
		 * unit's answer. */
		modbus_flush(ctx);
		unit->misses++;
		if (unit->online && unit->misses >= cfg->offline_after) {
			unit->online = false;
			syslog(LOG_WARNING, "Daisy chain: PDU %d is offline (%s)", id,
					modbus_strerror(errno));
		}
		return;
	}

	if (!unit->online) {
		syslog(LOG_INFO, "Daisy chain: PDU %d is online", id);
	}
	unit->seen = true;
	unit->online = true;
	unit->misses = 0;
	unit->last_seen = time(NULL);
	memcpy(unit->regs, regs, sizeof(regs));

	/* Relay states exist only on switched PDUs; a failure here does not
	 * make the unit offline. */
	unit->have_coils = modbus_read_bits(ctx, 0, N_READ_BITS, unit->coils)
			== N_READ_BITS;
	if (!unit->have_coils) {
		modbus_flush(ctx);
	}
}

/* Public functions ***********************************************************/

void chain_master_run(modbus_t* ctx, const models_modbus_t* cfg)
{
	int first = cfg->scan_first < UNIT_ID_MIN ? UNIT_ID_MIN : cfg->scan_first;
	int last = cfg->scan_last > UNIT_ID_MAX ? UNIT_ID_MAX : cfg->scan_last;
	int offline_after = cfg->offline_after > 0 ? cfg->offline_after : 1;
	int poll_interval = cfg->poll_interval_s > 0 ? cfg->poll_interval_s : 1;
	int timeout_ms = cfg->timeout_ms > 0 ? cfg->timeout_ms : 200;
	models_modbus_t settings = *cfg;
	settings.offline_after = offline_after;

	memset(units, 0, sizeof(units));
	modbus_set_response_timeout(ctx, timeout_ms / 1000,
			(timeout_ms % 1000) * 1000);
	syslog(LOG_INFO, "Daisy chain master %d: polling IDs %d-%d every %d s, "
			"timeout %d ms, offline after %d missed", cfg->addr, first, last,
			poll_interval, timeout_ms, offline_after);
	write_status(&settings);

	for (;;) {
		for (int id = first; id <= last; id++) {
			if (id != cfg->addr) {
				poll_unit(ctx, id, &settings);
			}
		}
		write_status(&settings);
		sleep(poll_interval);
	}
}
