// libounce's cryptographically secure random service supplies TLS entropy.
#include <mbedtls/entropy.h>
#include <mbedtls/entropy_poll.h>
#include <switch.h>

int mbedtls_hardware_poll(void* data, unsigned char* output, size_t len, size_t* olen)
{
    (void)data;
    *olen     = 0;
    Result rc = csrngInitialize();
    if (R_FAILED(rc))
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    rc = csrngGetRandomBytes(output, len);
    csrngExit();
    if (R_FAILED(rc))
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    *olen = len;
    return 0;
}
