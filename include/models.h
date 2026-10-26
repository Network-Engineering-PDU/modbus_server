#ifndef MODELS_H
#define MODELS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct models_in_data_t {
	float voltage;
	float current;
	float active_power;
	float reactive_power;
	float apparent_power;
	float power_factor;
	float phase;
	float frequency;
	float energy;
} models_in_data_t;

typedef struct models_out_data_t {
	float voltage;
	float current;
	float active_power;
	float reactive_power;
	float apparent_power;
	float power_factor;
	float phase;
	float frequency;
	float energy;
	const char* conn;
	int fuse;
} models_out_data_t;

typedef enum {
	A1,
	A2,
	B1,
	B2,
	UNK,
} models_license_t;

typedef struct models_out_sw_t {
	int line_id;
	bool status;
} models_out_sw_t;

typedef struct models_modbus_t {
	int addr;
	/* RTU (RS-485, the "485" ports). Data bits are always 8. */
	int baud_rate;
	char parity; /* 'N', 'E' or 'O' */
	int stop_bits;
	/* TCP: each NIC gated independently, since the socket binds 0.0.0.0. */
	bool tcp_eth1;
	bool tcp_eth2;
	/* Daisy chain: a master polls the other PDUs on the 485 bus (unit IDs
	 * scan_first..scan_last) instead of answering as a slave at addr. */
	bool master;
	int scan_first;
	int scan_last;
	int timeout_ms;
	int poll_interval_s;
	int offline_after;
} models_modbus_t;

const models_in_data_t* models_get_in_data();
void models_set_in_data(const models_in_data_t* in_data);

const models_out_data_t* models_get_out_data();
void models_set_out_data(const models_out_data_t* out_data);

models_license_t models_get_license();
void models_set_license(models_license_t license);

int models_get_switch();
void models_set_switch(int status);

const models_out_sw_t* models_get_out_sw(int* len);
void models_set_out_sw(const models_out_sw_t* out_sw, int len);

const models_out_sw_t* models_get_out_sw_id(int id);
void models_set_out_sw_idx(const models_out_sw_t* l_out_sw, int line_id);

const models_modbus_t* models_get_modbus();
void models_set_modbus(const models_modbus_t* modbus);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MODELS_H */
