#ifdef USE_OPENSSL_CRYPTO

#include "OpenSSLCryptoManager.hpp"
#include "Settings.hpp"
//#include "Logger.hpp"
#include <string.h>
#include <cstdlib>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>

static Data m_cert;
static Data m_key;

static const int NUM_BITS = 2048;
static const int SERIAL = 0;
static const int NUM_YEARS = 10;

static bool _generate_new_cert_key_pair();

bool OpenSSLCryptoManager::load_cert_key_pair() {
    if (m_key.is_empty() || m_cert.is_empty()) {
        Data cert = Data::read_from_file(Settings::instance().key_dir() + "/" + CERTIFICATE_FILE_NAME);
        Data key = Data::read_from_file(Settings::instance().key_dir() + "/" + KEY_FILE_NAME);
        
        if (!cert.is_empty() && !key.is_empty()) {
            m_cert = cert;
            m_key = key;
            return true;
        }
        return false;
    }
    return true;
}

bool OpenSSLCryptoManager::generate_new_cert_key_pair() {
    if (_generate_new_cert_key_pair()) {
        if (!m_cert.is_empty() && !m_key.is_empty()) {
            m_cert.write_to_file(Settings::instance().key_dir() + "/" + CERTIFICATE_FILE_NAME);
            m_key.write_to_file(Settings::instance().key_dir() + "/" + KEY_FILE_NAME);
            return true;
        }
    }
    return false;
}

void OpenSSLCryptoManager::remove_cert_key_pair() {
    remove((Settings::instance().key_dir() + "/" + CERTIFICATE_FILE_NAME).c_str());
    remove((Settings::instance().key_dir() + "/" + KEY_FILE_NAME).c_str());
    m_cert = Data();
    m_key = Data();
}

Data OpenSSLCryptoManager::cert_data() {
    return m_cert;
}

Data OpenSSLCryptoManager::key_data() {
    return m_key;
}

Data OpenSSLCryptoManager::SHA1_hash_data(Data data) {
    unsigned char sha1[20];
    SHA1(data.bytes(), data.size(), sha1);
    return Data(sha1, sizeof(sha1));
}

Data OpenSSLCryptoManager::SHA256_hash_data(Data data) {
    unsigned char sha256[32];
    SHA256(data.bytes(), data.size(), sha256);
    return Data(sha256, sizeof(sha256));
}

Data OpenSSLCryptoManager::create_AES_key_from_salt_SHA1(Data salted_pin) {
    return SHA1_hash_data(salted_pin).subdata(0, 16);
}

Data OpenSSLCryptoManager::create_AES_key_from_salt_SHA256(Data salted_pin) {
    return SHA256_hash_data(salted_pin).subdata(0, 16);
}

static int get_encrypt_size(Data data) {
    // the size is the length of the data ceiling to the nearest 16 bytes
    return (((int)data.size() + 15) / 16) * 16;
}

Data OpenSSLCryptoManager::aes_encrypt(Data data, Data key) {
    AES_KEY aes_key;
    AES_set_encrypt_key((unsigned char*)key.bytes(), 128, &aes_key);
    int size = get_encrypt_size(data);
    unsigned char* buffer = (unsigned char*)malloc(size);
    unsigned char* block_rounded_buffer = (unsigned char*)calloc(1, size);
    memcpy(block_rounded_buffer, data.bytes(), data.size());
    
    // AES_encrypt only encrypts the first 16 bytes so iterate the entire buffer
    int block_offset = 0;
    while (block_offset < size) {
        AES_encrypt(block_rounded_buffer + block_offset, buffer + block_offset, &aes_key);
        block_offset += 16;
    }
    
    Data encrypted_data = Data((char*)buffer, size);
    free(buffer);
    free(block_rounded_buffer);
    return encrypted_data;
}

Data OpenSSLCryptoManager::aes_decrypt(Data data, Data key) {
    AES_KEY aes_key;
    AES_set_decrypt_key(key.bytes(), 128, &aes_key);
    unsigned char* buffer = (unsigned char*)malloc(data.size());
    
    // AES_decrypt only decrypts the first 16 bytes so iterate the entire buffer
    int block_offset = 0;
    while (block_offset < data.size()) {
        AES_decrypt(data.bytes() + block_offset, buffer + block_offset, &aes_key);
        block_offset += 16;
    }
    
    Data decrypted_data = Data(buffer, data.size());
    free(buffer);
    return decrypted_data;
}

Data OpenSSLCryptoManager::signature(Data cert) {
    BIO* bio = BIO_new_mem_buf(cert.bytes(), cert.size());
    X509* x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    
    if (!x509) {
//        Logger::error("Crypto", "Unable to parse certificate in memory!");
        return Data();
    }
    
#if (OPENSSL_VERSION_NUMBER < 0x10002000L)
    ASN1_BIT_STRING *asn_signature = x509->signature;
#elif (OPENSSL_VERSION_NUMBER < 0x10100000L)
    ASN1_BIT_STRING *asn_signature;
    X509_get0_signature(&asn_signature, NULL, x509);
#else
    const ASN1_BIT_STRING *asn_signature;
    X509_get0_signature(&asn_signature, NULL, x509);
#endif
    
    Data sig = Data(asn_signature->data, asn_signature->length);
    X509_free(x509);
    return sig;
}

bool OpenSSLCryptoManager::verify_signature(Data data, Data signature, Data cert) {
    BIO* bio = BIO_new_mem_buf(cert.bytes(), cert.size());
    X509* x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    
    BIO_free(bio);
    
    if (!x509) {
//        Logger::error("Crypto", "Unable to parse certificate in memory...");
        return false;
    }
    
    EVP_PKEY* pub_key = X509_get_pubkey(x509);
    EVP_MD_CTX *mdctx = NULL;
    mdctx = EVP_MD_CTX_create();
    EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pub_key);
    EVP_DigestVerifyUpdate(mdctx, data.bytes(), data.size());
    int result = EVP_DigestVerifyFinal(mdctx, signature.bytes(), signature.size());
    
    X509_free(x509);
    EVP_PKEY_free(pub_key);
    EVP_MD_CTX_destroy(mdctx);
    return result > 0;
}

Data OpenSSLCryptoManager::sign_data(Data data, Data key) {
    BIO* bio = BIO_new_mem_buf(key.bytes(), key.size());
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    
    BIO_free(bio);
    
    if (!pkey) {
//        Logger::error("Crypto", "Unable to parse private key in memory...");
        return Data();
    }
    
    EVP_MD_CTX *mdctx = EVP_MD_CTX_create();
    EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, pkey);
    EVP_DigestSignUpdate(mdctx, data.bytes(), data.size());
    size_t slen;
    EVP_DigestSignFinal(mdctx, NULL, &slen);
    unsigned char* signature = (unsigned char*)malloc(slen);
    int result = EVP_DigestSignFinal(mdctx, signature, &slen);
    
    EVP_PKEY_free(pkey);
    EVP_MD_CTX_destroy(mdctx);
    
    if (result <= 0) {
        free(signature);
//        Logger::error("Crypto", "Unable to sign data...");
        Data();
    }
    
    Data signed_data = Data(signature, slen);
    free(signature);
    return signed_data;
}

// Cert and key generator

static Data _cert_data(X509* cert) {
    BIO* bio = BIO_new(BIO_s_mem());
    
    PEM_write_bio_X509(bio, cert);
    
    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    Data data = Data(mem->data, mem->length);
    BIO_free(bio);
    return data;
}

static Data _key_data(EVP_PKEY* pk) {
    BIO* bio = BIO_new(BIO_s_mem());
    
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    PEM_write_bio_PrivateKey(bio, pk, NULL, NULL, 0, NULL, NULL);
#else
    PEM_write_bio_PrivateKey_traditional(bio, pk, NULL, NULL, 0, NULL, NULL);
#endif
    
    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    Data data = Data(mem->data, mem->length);
    BIO_free(bio);
    return data;
}

static bool _generate_new_cert_key_pair() {
//    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
    BIO *bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
    
    X509* cert = X509_new();
    EVP_PKEY* pk = EVP_PKEY_new();
    BIGNUM* bne = BN_new();
    RSA* rsa = RSA_new();
    
    BN_set_word(bne, RSA_F4);
    RSA_generate_key_ex(rsa, NUM_BITS, bne, NULL);
    
    EVP_PKEY_assign_RSA(pk, rsa);
    
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), SERIAL);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 60 * 60 * 24 * 365 * NUM_YEARS);
#else
    ASN1_TIME* before = ASN1_STRING_dup(X509_get0_notBefore(cert));
    ASN1_TIME* after = ASN1_STRING_dup(X509_get0_notAfter(cert));
    
    X509_gmtime_adj(before, 0);
    X509_gmtime_adj(after, 60 * 60 * 24 * 365 * NUM_YEARS);
    
    X509_set1_notBefore(cert, before);
    X509_set1_notAfter(cert, after);
    
    ASN1_STRING_free(before);
    ASN1_STRING_free(after);
#endif
    
    X509_set_pubkey(cert, pk);
    
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"NVIDIA GameStream Client", -1, -1, 0);
    X509_set_issuer_name(cert, name);
    
    X509_sign(cert, pk, EVP_sha256());
    
    BN_free(bne);
    
    BIO_free(bio_err);
    
    m_cert = _cert_data(cert);
    m_key = _key_data(pk);
    
    X509_free(cert);
    EVP_PKEY_free(pk);
    return true;
}

#endif