#include <cjson/cJSON.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_helper.h"
#include "models.h"

#define MAX_OUTLETS 48
#define MAX_SENSORS 8

/* Global variables ***********************************************************/
/* Function prototypes ********************************************************/

static const char* json_get_string(cJSON* json, const char* id);
static int json_get_int(int* num, cJSON* json, char* id);
static int json_get_float(float* num, cJSON* json, char* id);
static int json_get_bool(bool* bl, cJSON* json, char* id);

/* Callbacks ******************************************************************/
/* Function definitions *******************************************************/

static const char* json_get_string(cJSON* json, const char* id)
{
	cJSON* res = cJSON_GetObjectItemCaseSensitive(json, id);
	if (cJSON_IsString(res)) {
		return res->valuestring;
	}
	return NULL;
}

static int json_get_int(int* num, cJSON* json, char* id)
{
	cJSON* res = cJSON_GetObjectItem(json, id);
	if (cJSON_IsNumber(res)) {
		*num = res->valueint;
		return 0;
	}
	return 1;
}

static int json_get_float(float* num, cJSON* json, char* id)
{
	cJSON* res = cJSON_GetObjectItem(json, id);
	if (cJSON_IsNumber(res)) {
		*num = (float)res->valuedouble;
		return 0;
	}
	return 1;
}

static int json_get_bool(bool* bl, cJSON* json, char* id)
{
	cJSON* res = cJSON_GetObjectItem(json, id);
	if (cJSON_IsBool(res)) {
		*bl = cJSON_IsTrue(res);
		return 0;
	}
	return 1;
}

/* Public functions ***********************************************************/

int json_helper_update_in_data(const char* json_str)
{
	cJSON* json = cJSON_Parse(json_str);
	if (json == NULL) {
		return 1;
	}
	models_in_data_t in_data;

	int err;
	err = json_get_float(&in_data.voltage, json, "voltage");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&in_data.current, json, "current");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&in_data.active_power, json, "active_power");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&in_data.reactive_power, json, "reactive_power");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&in_data.apparent_power, json, "apparent_power");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&in_data.power_factor, json, "power_factor");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&in_data.phase, json, "phase");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&in_data.frequency , json, "frequency");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&in_data.energy, json, "energy");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}

	models_set_in_data(&in_data);

	cJSON_Delete(json);

	return 0;
}

int json_helper_update_out_data(const char* json_str)
{
	cJSON* json = cJSON_Parse(json_str);
	if (json == NULL) {
		return 1;
	}

	models_out_data_t out_data;
	int err;
	const char* str;
	err = json_get_float(&out_data.voltage, json, "voltage");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&out_data.current, json, "current");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&out_data.active_power, json, "active_power");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&out_data.reactive_power, json, "reactive_power");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&out_data.apparent_power , json, "apparent_power");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&out_data.power_factor, json, "power_factor");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&out_data.phase, json, "phase");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&out_data.frequency, json, "frequency");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	err = json_get_float(&out_data.energy, json, "energy");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	str = json_get_string(json, "conn");
	if (str == NULL) {
		cJSON_Delete(json);
		return 1;
	}
	out_data.conn = str;
	err = json_get_int(&out_data.fuse, json, "fuse");
	if (err != 0) {
		cJSON_Delete(json);
		return 1;
	}
	models_set_out_data(&out_data);

	cJSON_Delete(json);

	return 0;
}

int json_helper_update_license(const char* json_str)
{
	cJSON* json = cJSON_Parse(json_str);
	if (json == NULL) {
		return 1;
	}

	models_license_t license;
	const char* str;

	str = json_get_string(json, "type_id");
	if (str == NULL) {
		cJSON_Delete(json);
		return 1;
	}

	if (strcmp(str, "A1") == 0) {
		license = A1;
	} else if (strcmp(str, "A2") == 0) {
		license = A2;
	} else if (strcmp(str, "B1") == 0) {
		license = B1;
	} else if (strcmp(str, "B2") == 0) {
		license = B2;
	} else {
		license = UNK;
	}
	
	models_set_license(license);

	cJSON_Delete(json);

	return 0;
}

int json_helper_update_out_sw(const char* json_str)
{
	cJSON* json = cJSON_Parse(json_str);
	if (json == NULL) {
		return 1;
	}
	models_out_sw_t out_sw[MAX_OUTLETS];

	int i;
	for (i = 0; i < MAX_OUTLETS; i++) {
		int err;
		char s[5];
		sprintf(s, "%d", i);
		err = json_get_bool(&out_sw[i].status, json, s);
		if (err != 0) {
			break;
		}
		out_sw[i].line_id = i + 1;
	}

	models_set_out_sw(out_sw, i);

	cJSON_Delete(json);

	return 0;
}

int json_helper_update_modbus(const char* json_str)
{
	cJSON* json = cJSON_Parse(json_str);
	if (json == NULL) {
		return 1;
	}

	/* Start from the values already in use, so one missing or malformed
	 * field in the response does not discard the rest. */
	models_modbus_t modbus = *models_get_modbus();
	const char* parity_str;
	bool bl;

	json_get_int(&modbus.addr, json, "addr");
	json_get_int(&modbus.baud_rate, json, "baud_rate");
	parity_str = json_get_string(json, "parity");
	if (parity_str != NULL && parity_str[0] != '\0') {
		modbus.parity = parity_str[0];
	}
	json_get_int(&modbus.stop_bits, json, "stop_bits");
	if (json_get_bool(&bl, json, "tcp_eth1_enabled") == 0) {
		modbus.tcp_eth1 = bl;
	}
	if (json_get_bool(&bl, json, "tcp_eth2_enabled") == 0) {
		modbus.tcp_eth2 = bl;
	}
	const char* role_str = json_get_string(json, "role");
	if (role_str != NULL) {
		modbus.master = strcmp(role_str, "master") == 0;
	}
	json_get_int(&modbus.scan_first, json, "scan_first");
	json_get_int(&modbus.scan_last, json, "scan_last");
	json_get_int(&modbus.timeout_ms, json, "timeout_ms");
	json_get_int(&modbus.poll_interval_s, json, "poll_interval_s");
	json_get_int(&modbus.offline_after, json, "offline_after");

	models_set_modbus(&modbus);

	cJSON_Delete(json);

	return 0;
}
