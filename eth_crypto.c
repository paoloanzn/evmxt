#include <stddef.h>
#include <openssl/evp.h>
#include <stdint.h>

#include "eth_crypto.h"

int eth_keccak256(
    const void *data,
    size_t data_len,
    uint8_t out[32])
{
    size_t out_len = 0;

    if (!EVP_Q_digest(
            NULL,
            "KECCAK-256",
            NULL,
            data,
            data_len,
            out,
            &out_len))
    {
        return 0;
    }

    return out_len == 32;
}