#ifndef MCUX_MOCK_FSL_LPI2C_H
#define MCUX_MOCK_FSL_LPI2C_H

#include "fsl_flash.h"

typedef struct {
    uint32_t unused;
} LPI2C_Type;

typedef struct {
    uint32_t baudRate_Hz;
} lpi2c_master_config_t;

#define LPI2C0 ((LPI2C_Type *)0x40005000u)

void LPI2C_MasterGetDefaultConfig(lpi2c_master_config_t *config);
void LPI2C_MasterInit(LPI2C_Type *base, const lpi2c_master_config_t *config,
                      uint32_t srcClock_Hz);

#endif /* MCUX_MOCK_FSL_LPI2C_H */
