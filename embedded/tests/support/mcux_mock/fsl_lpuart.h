#ifndef MCUX_MOCK_FSL_LPUART_H
#define MCUX_MOCK_FSL_LPUART_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t unused;
} LPUART_Type;

#define LPUART0 ((LPUART_Type *)0x40002000u)

void LPUART_WriteBlocking(LPUART_Type *base, const uint8_t *data, size_t length);

#endif /* MCUX_MOCK_FSL_LPUART_H */
