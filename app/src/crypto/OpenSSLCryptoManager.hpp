#include <stdio.h>
#include "Data.hpp"

#pragma once

#define CERTIFICATE_FILE_NAME "client.pem"
#define KEY_FILE_NAME "key.pem"

class OpenSSLCryptoManager {
public:
    static bool load_cert_key_pair();
    static bool generate_new_cert_key_pair();
    static void remove_cert_key_pair();
    
    static Data cert_data();
    static Data key_data();
    
    static Data SHA1_hash_data(Data data);
    static Data SHA256_hash_data(Data data);
    static Data create_AES_key_from_salt_SHA1(Data salted_pin);
    static Data create_AES_key_from_salt_SHA256(Data salted_pin);
    static Data aes_encrypt(Data data, Data key);
    static Data aes_decrypt(Data data, Data key);
    
    static Data signature(Data cert);
    static bool verify_signature(Data data, Data signature, Data cert);
    static Data sign_data(Data data, Data key);
};
