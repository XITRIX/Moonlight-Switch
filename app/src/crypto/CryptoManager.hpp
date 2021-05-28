#include <stdio.h>
#pragma once

#if defined(USE_OPENSSL_CRYPTO)

#include "OpenSSLCryptoManager.hpp"
#define CryptoManager OpenSSLCryptoManager

#elif defined(USE_MBEDTLS_CRYPTO)

#include "MbedTLSCryptoManager.hpp"
#define CryptoManager MbedTLSCryptoManager

#else
#error Select crypto!
#endif
