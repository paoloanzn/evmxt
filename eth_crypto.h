#ifndef ETH_CRYPTO_H
#define ETH_CRYPTO_H

#include <stdint.h>
#include <stdio.h>

int eth_keccak256(const void *data, size_t data_len, uint8_t out[32]);
// Ethereum private key is a 32 bytes scalar, the public key is
// derived from the private key and it's 64 bytes uncompressed.
// The corresponding address is the last 20 bytes of 
// keccak256(public_key_64_bytes).
int derive_address(const uint8_t private_key[32], uint8_t address_out[20]);
// Sign a 32 bytes tx hash; produces r(32), s(32), v,
// (recovery id, 0 or 1). Returns 0 on success.
int sign_hash(const uint8_t private_key[32], const uint8_t hash[32], 
            uint8_t result[32], uint8_t s[32], int *v);

#endif /* ETH_CRYPTO_H */