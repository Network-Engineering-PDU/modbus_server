#include <stdlib.h>
#include <string.h>
#include "models.h"

/* Global variables ***********************************************************/

static models_in_data_t in_data;
static models_out_data_t out_data;
static models_license_t license;

static models_out_sw_t* out_sw_list;
static int out_sw_list_len;
/* Same defaults as ne-fw-api's DEFAULT_MODBUS_SETTINGS, used if the HTTP
 * fetch from ne-fw-api ever fails, so a transient fetch problem can't turn
 * into "nothing works" (invalid serial params, or no TCP bound at all). */
static models_modbus_t modbus = {
	.addr = 1,
	.baud_rate = 115200,
	.parity = 'N',
	.stop_bits = 1,
	.tcp_eth1 = true,
	.tcp_eth2 = true,
	.master = false,
	.scan_first = 2,
	.scan_last = 16,
	.timeout_ms = 200,
	.poll_interval_s = 5,
	.offline_after = 3,
};

/* Function prototypes ********************************************************/

static const char* stralloc(const char* str);

/* Callbacks ******************************************************************/
/* Function definitions *******************************************************/

static const char* stralloc(const char* str)
{
	char* dest;
	if (str == NULL) {
		str = "N/A";
	}
	dest = malloc(strlen(str) + 1);
	strcpy(dest, str);
	return dest;
}

/* Public functions ***********************************************************/

const models_in_data_t* models_get_in_data()
{
	return &in_data;
}

void models_set_in_data(const models_in_data_t* l_in_data)
{
	in_data.voltage = l_in_data->voltage;
	in_data.current = l_in_data->current;
	in_data.active_power = l_in_data->active_power;
	in_data.reactive_power = l_in_data->reactive_power;
	in_data.apparent_power = l_in_data->apparent_power;
	in_data.power_factor = l_in_data->power_factor;
	in_data.phase = l_in_data->phase;
	in_data.frequency = l_in_data->frequency;
	in_data.energy = l_in_data->energy;
}

const models_out_data_t* models_get_out_data()
{
	return &out_data;
}

void models_set_out_data(const models_out_data_t* l_out_data)
{
	free((void*)out_data.conn);
	out_data.voltage = l_out_data->voltage;
	out_data.current = l_out_data->current;
	out_data.active_power = l_out_data->active_power;
	out_data.reactive_power = l_out_data->reactive_power;
	out_data.apparent_power = l_out_data->apparent_power;
	out_data.power_factor = l_out_data->power_factor;
	out_data.phase = l_out_data->phase;
	out_data.frequency = l_out_data->frequency;
	out_data.energy = l_out_data->energy;
	out_data.conn = stralloc(l_out_data->conn);
	out_data.fuse = l_out_data->fuse;
}

models_license_t models_get_license()
{
	return license;
}

void models_set_license(models_license_t l_license)
{
	license = l_license;
}

const models_out_sw_t* models_get_out_sw(int* len)
{
	*len = out_sw_list_len;

	return out_sw_list;
}

const models_out_sw_t* models_get_out_sw_id(int id)
{
	return &out_sw_list[id-1];
}

//TODO: 1 o todos?
void models_set_out_sw(const models_out_sw_t* l_out_sw, int len)
{
	free((void*)out_sw_list);
	out_sw_list = malloc(len * sizeof(models_out_sw_t));
	for (int i = 0; i < len; i++) {
		out_sw_list[i].line_id = l_out_sw[i].line_id;
		out_sw_list[i].status = l_out_sw[i].status;
	}
	out_sw_list_len = len;
}

void models_set_out_sw_idx(const models_out_sw_t* l_out_sw, int line_id)
{
	if (line_id >= out_sw_list_len) {
		return;
	}
	out_sw_list[line_id].line_id = l_out_sw->line_id;
	out_sw_list[line_id].status = l_out_sw->status;
}

const models_modbus_t* models_get_modbus()
{
	return &modbus;
}

void models_set_modbus(const models_modbus_t* l_modbus)
{
	modbus = *l_modbus;
}
