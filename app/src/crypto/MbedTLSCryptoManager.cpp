#include "MbedTLSCryptoManager.hpp"
#include "Settings.hpp"
#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>
#include <string.h>

static Data m_cert;
static Data m_key;

static bool _generate_new_cert_key_pair();

bool MbedTLSCryptoManager::load_cert_key_pair() {
    if (m_key.is_empty() || m_cert.is_empty()) {
        Data cert = Data::read_from_file(Settings::instance().key_dir() + "/" +
                                         CERTIFICATE_FILE_NAME);
        Data key = Data::read_from_file(Settings::instance().key_dir() + "/" +
                                        KEY_FILE_NAME);

        if (!cert.is_empty() && !key.is_empty()) {
            m_cert = cert;
            m_key = key;
            return true;
        }
        return false;
    }
    return true;
}

bool MbedTLSCryptoManager::generate_new_cert_key_pair() {
    if (_generate_new_cert_key_pair()) {
        if (!m_cert.is_empty() && !m_key.is_empty()) {
            m_cert.write_to_file(Settings::instance().key_dir() + "/" +
                                 CERTIFICATE_FILE_NAME);
            m_key.write_to_file(Settings::instance().key_dir() + "/" +
                                KEY_FILE_NAME);
            return true;
        }
    }
    return false;
}

void MbedTLSCryptoManager::remove_cert_key_pair() {
    remove(
        (Settings::instance().key_dir() + "/" + CERTIFICATE_FILE_NAME).c_str());
    remove((Settings::instance().key_dir() + "/" + KEY_FILE_NAME).c_str());
    m_cert = Data();
    m_key = Data();
}

Data MbedTLSCryptoManager::cert_data() { return m_cert; }

Data MbedTLSCryptoManager::key_data() { return m_key; }

Data MbedTLSCryptoManager::SHA1_hash_data(Data data) {
    mbedtls_sha1_context ctx;
    unsigned char sha1[20];
    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_ret(data.bytes(), data.size(), sha1);
    mbedtls_sha1_free(&ctx);
    return Data(sha1, sizeof(sha1));
}

Data MbedTLSCryptoManager::SHA256_hash_data(Data data) {
    mbedtls_sha256_context ctx;
    unsigned char sha256[32];
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_ret(data.bytes(), data.size(), sha256, 0);
    mbedtls_sha256_free(&ctx);
    return Data(sha256, sizeof(sha256));
}

Data MbedTLSCryptoManager::create_AES_key_from_salt_SHA1(Data salted_pin) {
    return SHA1_hash_data(salted_pin).subdata(0, 16);
}

Data MbedTLSCryptoManager::create_AES_key_from_salt_SHA256(Data salted_pin) {
    return SHA256_hash_data(salted_pin).subdata(0, 16);
}

static int get_encrypt_size(Data data) {
    // the size is the length of the data ceiling to the nearest 16 bytes
    return (((int)data.size() + 15) / 16) * 16;
}

Data MbedTLSCryptoManager::aes_encrypt(Data data, Data key) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key.bytes(), 128);

    int size = get_encrypt_size(data);
    unsigned char* buffer = (unsigned char*)malloc(size);
    unsigned char* block_rounded_buffer = (unsigned char*)calloc(1, size);
    memcpy(block_rounded_buffer, data.bytes(), data.size());

    int block_offset = 0;
    while (block_offset < size) {
        mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT,
                              block_rounded_buffer + block_offset,
                              buffer + block_offset);
        block_offset += 16;
    }

    Data encrypted_data = Data((char*)buffer, size);
    mbedtls_aes_free(&ctx);
    free(buffer);
    free(block_rounded_buffer);
    return encrypted_data;
}

Data MbedTLSCryptoManager::aes_decrypt(Data data, Data key) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key.bytes(), 128);
    unsigned char* buffer = (unsigned char*)malloc(data.size());

    int block_offset = 0;
    while (block_offset < data.size()) {
        mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT,
                              data.bytes() + block_offset,
                              buffer + block_offset);
        block_offset += 16;
    }

    Data decrypted_data = Data(buffer, data.size());
    mbedtls_aes_free(&ctx);
    free(buffer);
    return decrypted_data;
}

Data MbedTLSCryptoManager::signature(Data cert) {
    mbedtls_x509_crt x509;
    mbedtls_x509_crt_init(&x509);

    mbedtls_x509_crt_parse(&x509, cert.bytes(), cert.size() + 1);

    Data data(x509.sig.p, x509.sig.len);
    mbedtls_x509_crt_free(&x509);
    return data;
}

bool MbedTLSCryptoManager::verify_signature(Data data, Data signature,
                                            Data cert) {
    // TODO
    return true;
}

Data MbedTLSCryptoManager::sign_data(Data data, Data key) {
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    unsigned char hash[32];
    unsigned char buf[MBEDTLS_MPI_MAX_SIZE];
    size_t size = 0;

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_pk_init(&pk);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);

    mbedtls_pk_parse_key(&pk, key.bytes(), key.size() + 1, NULL, 0);
    mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), data.bytes(),
               data.size(), hash);
    mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, 0, buf, &size,
                    mbedtls_ctr_drbg_random, &ctr_drbg);

    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    if (size > 0) {
        return Data(buf, size);
    }
    return data;
}

// Cert and key generator

static void _generate_key(mbedtls_pk_context* key) {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);

    mbedtls_pk_init(key);

    mbedtls_pk_setup(key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    mbedtls_rsa_gen_key(mbedtls_pk_rsa(*key), mbedtls_ctr_drbg_random,
                        &ctr_drbg, 2048, 65537);

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

static void _generate_cert(mbedtls_x509write_cert* cert,
                           mbedtls_pk_context* key) {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_mpi serial;

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    mbedtls_mpi_init(&serial);
    mbedtls_mpi_lset(&serial, 1);

    mbedtls_x509write_crt_init(cert);

    mbedtls_x509write_crt_set_version(cert, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_subject_name(cert, "CN=NVIDIA GameStream Client");
    mbedtls_x509write_crt_set_issuer_name(cert, "CN=NVIDIA GameStream Client");
    mbedtls_x509write_crt_set_subject_key(cert, key);
    mbedtls_x509write_crt_set_issuer_key(cert, key);
    mbedtls_x509write_crt_set_md_alg(cert, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_validity(cert, "20200101000000",
                                       "20300101000000");
    mbedtls_x509write_crt_set_serial(cert, &serial);

    mbedtls_mpi_free(&serial);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

static bool _generate_new_cert_key_pair() {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);

    mbedtls_x509write_cert cert;
    mbedtls_pk_context key;

    _generate_key(&key);
    _generate_cert(&cert, &key);

    unsigned char tmp[4096];
    memset(tmp, 0, 4096);
    size_t len = 0;
    int i = mbedtls_pk_write_key_pem(&key, tmp, 4096);

    len = strlen((char*)tmp);
    m_key = Data(tmp, len);
    memset(tmp, 0, 4096);

    i = mbedtls_x509write_crt_pem(&cert, tmp, 4096, mbedtls_ctr_drbg_random,
                                  &ctr_drbg);
    len = strlen((char*)tmp);
    m_cert = Data(tmp, len);

    mbedtls_x509write_crt_free(&cert);
    mbedtls_pk_free(&key);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return true;
}
