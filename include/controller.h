#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "models.h"

#ifdef __cplusplus
extern "C" {
#endif

void controller_init();
void controller_get_in_data(int line_id);
void controller_get_out_data(int line_id);
void controller_get_license();
void controller_get_out_sw();
void controller_put_out_sw(const models_out_sw_t* out_sw, int line_id);
void controller_get_modbus();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CONTROLLER_H */
