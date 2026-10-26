#ifndef CHAIN_MASTER_H
#define CHAIN_MASTER_H

#include <modbus/modbus.h>

#include "models.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Where the master publishes what it read from the chain, as JSON. */
#define CHAIN_STATUS_FILE "/tmp/modbus_chain.json"

/* Polls the other PDUs on the RS-485 bus forever; never returns. ctx must
 * be a connected RTU context. */
void chain_master_run(modbus_t* ctx, const models_modbus_t* cfg);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CHAIN_MASTER_H */
