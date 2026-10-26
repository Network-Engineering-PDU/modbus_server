#ifndef REGISTER_MAP_H
#define REGISTER_MAP_H

/* Modbus data every PDU serves, and a daisy chain master reads:
 *   holding registers 0-5    input current (A x 100, signed)
 *   holding registers 6-11   input power (W, signed)
 *   holding registers 12-59  outlet current (A x 100, signed)
 *   holding registers 60-107 outlet power (W, signed)
 *   coils 0-47               outlet relay on/off */
#define MAX_INPUTS 6
#define MAX_OUTLETS 48

#define REG_INPUT_CURRENT  0
#define REG_INPUT_POWER    (REG_INPUT_CURRENT + MAX_INPUTS)
#define REG_OUTLET_CURRENT (REG_INPUT_POWER + MAX_INPUTS)
#define REG_OUTLET_POWER   (REG_OUTLET_CURRENT + MAX_OUTLETS)

#define N_READ_BITS       MAX_OUTLETS
#define N_INPUT_BITS      0
#define N_READ_REGISTERS  (2*MAX_INPUTS + 2*MAX_OUTLETS)
#define N_INPUT_REGISTERS 0

#endif /* REGISTER_MAP_H */
