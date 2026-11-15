#ifndef MCUX_MOCK_FSL_LPSPI_H
#define MCUX_MOCK_FSL_LPSPI_H

#include "fsl_flash.h"

#include <stddef.h>

typedef struct {
    uint32_t unused;
} LPSPI_Type;

typedef struct {
    uint32_t baudRate;
    uint32_t pcs;
} lpspi_master_config_t;

typedef struct {
    uint8_t *txData;
    uint8_t *rxData;
    size_t dataSize;
} lpspi_transfer_t;

#define LPSPI1 ((LPSPI_Type *)0x40003000u)
#define LPSPI2 ((LPSPI_Type *)0x40004000u)
#define kLPSPI_Pcs1 1u

void LPSPI_MasterGetDefaultConfig(lpspi_master_config_t *config);
void LPSPI_MasterInit(LPSPI_Type *base, const lpspi_master_config_t *config,
                      uint32_t srcClock_Hz);
status_t LPSPI_MasterTransferBlocking(LPSPI_Type *base, lpspi_transfer_t *transfer);

#endif /* MCUX_MOCK_FSL_LPSPI_H */
