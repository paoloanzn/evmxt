#ifndef ETH_CRYPTO_H
#define ETH_CRYPTO_H

#include <stdint.h>
#include <stdio.h>

int eth_keccak256(const void *data, size_t data_len, uint8_t out[32]);

#endif /* ETH_CRYPTO_H */