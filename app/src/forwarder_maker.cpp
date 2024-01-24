//
//  forwarder_maker.cpp
//  Moonlight
//
//  Created by XITRIX on 23/01/2024.
//

#include <forwarder_maker.hpp>
#include <Settings.hpp>

extern "C" {
#include "settings.h"
#include "cnmt.h"
#include "pki.h"
#include "extkeys.h"
#include "npdm.h"
#include "nacp.h"
#include "nca.h"
}

int makeForwarder(const Host& host, const App& app) {
    hbp_settings_t settings;
    cnmt_ctx_t cnmt_ctx;
    memset(&settings, 0, sizeof(settings));
    memset(&cnmt_ctx, 0, sizeof(cnmt_ctx));

//    printf("hacBrewPack %s by The-4n\n\n", HACBREWPACK_VERSION);

    // Hardcode default temp directory
    filepath_init(&settings.temp_dir);
    filepath_set(&settings.temp_dir, (Settings::instance().working_dir() + "/forwarder/hacbrewpack_temp").c_str());

    // Hardcode default output nca directory
    filepath_init(&settings.nca_dir);
    filepath_set(&settings.nca_dir, (Settings::instance().working_dir() + "/forwarder/hacbrewpack_nca").c_str());

    // Hardcode default output nsp directory
    filepath_init(&settings.nsp_dir);
    filepath_set(&settings.nsp_dir, (Settings::instance().working_dir() + "/forwarder/hacbrewpack_nsp").c_str());

    // Hardcode default backup directory
    filepath_init(&settings.backup_dir);
    filepath_set(&settings.backup_dir, (Settings::instance().working_dir() + "/forwarder/hacbrewpack_backup").c_str());

    // Hardcode Program exeFS directory
    filepath_init(&settings.exefs_dir);
    filepath_set(&settings.exefs_dir, (Settings::instance().working_dir() + "/forwarder/exefs").c_str());

    // Hardcode Program RomFS directory
    filepath_init(&settings.romfs_dir);
    filepath_set(&settings.romfs_dir, (Settings::instance().working_dir() + "/forwarder/romfs").c_str());

    // Hardcode Program Logo directory
    filepath_init(&settings.logo_dir);
    filepath_set(&settings.logo_dir, (Settings::instance().working_dir() + "/forwarder/logo").c_str());

    // Hardcode Control RomFS directory
    filepath_init(&settings.control_romfs_dir);
    filepath_set(&settings.control_romfs_dir, (Settings::instance().working_dir() + "/forwarder/control").c_str());

    filepath_t keypath;
    filepath_init(&keypath);

    pki_initialize_keyset(&settings.keyset);

    // Default Settings
    settings.keygeneration = 1;
    settings.sdk_version = 0x000C1100;
    settings.keyareakey = (unsigned char *)calloc(1, 0x10);
    memset(settings.keyareakey, 4, 0x10);

    // Prepare Settings
    filepath_set(&keypath, (Settings::instance().working_dir() + "/forwarder/prod.keys").c_str());
    settings.title_id = strtoull("01723bae95e06000", NULL, 16);
    strcpy(settings.titlename, "Moonlight");
    settings.noromfs = 1;
    settings.nologo = 1;

    // Remove existing temp and nca directories and Create new ones + nsp directory
    printf("Removing existing temp and nca directories\n");
    filepath_remove_directory(&settings.temp_dir);
    filepath_remove_directory(&settings.nca_dir);
    printf("Creating temp, nca, nsp and backup directories\n");
    os_makedir(settings.temp_dir.os_path);
    os_makedir(settings.nca_dir.os_path);
    os_makedir(settings.nsp_dir.os_path);
    os_makedir(settings.backup_dir.os_path);


    // Try to populate default keyfile.
    FILE *keyfile = NULL;
    if (keypath.valid == VALIDITY_INVALID)
    {
        // Locating default key file
        filepath_set(&keypath, "keys.dat");
        keyfile = os_fopen(keypath.os_path, OS_MODE_READ);
        if (keyfile == NULL)
        {
            filepath_set(&keypath, "keys.txt");
            keyfile = os_fopen(keypath.os_path, OS_MODE_READ);
        }
        if (keyfile == NULL)
        {
            filepath_set(&keypath, "keys.ini");
            keyfile = os_fopen(keypath.os_path, OS_MODE_READ);
        }
        if (keyfile == NULL)
        {
            filepath_set(&keypath, "prod.keys");
            keyfile = os_fopen(keypath.os_path, OS_MODE_READ);
        }
        if (keyfile == NULL)
        {
            /* Use $HOME/.switch/prod.keys if it exists */
            char *home = getenv("HOME");
            if (home == NULL)
                home = getenv("USERPROFILE");
            if (home != NULL)
            {
                filepath_set(&keypath, home);
                filepath_append(&keypath, ".switch");
                filepath_append(&keypath, "prod.keys");
                keyfile = os_fopen(keypath.os_path, OS_MODE_READ);
            }
        }
    }
    else if (keypath.valid == VALIDITY_VALID)
        keyfile = os_fopen(keypath.os_path, OS_MODE_READ);

    // Try to populate keyfile.
    if (keyfile != NULL)
    {
        printf("Loading '%s' keyset file\n", keypath.char_path);
        extkeys_initialize_keyset(&settings.keyset, keyfile);
        pki_derive_keys(&settings.keyset);
        fclose(keyfile);
    }
    else
    {
        printf("\n");
        fprintf(stderr, "Error: Unable to open keyset file\n"
                        "Use -k or --keyset to specify your keyset file path or place your keyset in ." OS_PATH_SEPARATOR "keys.dat\n");
        return EXIT_FAILURE;
    }

    // Make sure that key_area_key_application_keygen exists
    uint8_t has_keygen_key = 0;
    for (unsigned int i = 0; i < 0x10; i++)
    {
        if (settings.keyset.key_area_keys[settings.keygeneration - 1][0][i] != 0)
        {
            has_keygen_key = 1;
            break;
        }
    }
    if (has_keygen_key == 0)
    {
        fprintf(stderr, "Error: key_area_key_application for keygeneration %i is not present in keyset file\n", settings.keygeneration);
        return EXIT_FAILURE;
    }

    // Make sure that header_key exists
    uint8_t has_header_Key = 0;
    for (unsigned int i = 0; i < 0x10; i++)
    {
        if (settings.keyset.header_key[i] != 0)
        {
            has_header_Key = 1;
            break;
        }
    }
    if (has_header_Key == 0)
    {
        fprintf(stderr, "Error: header_key is not present in keyset file\n");
        return EXIT_FAILURE;
    }

    // Process NPDM
    printf("\n");
    printf("----> Processing NPDM\n");
    npdm_process(&settings, &cnmt_ctx);
    printf("\n");

    // Process NACP
    printf("----> Processing NACP\n");
    nacp_process(&settings);
    printf("\n");

    // Create NCAs
    nca_create_program(&settings, &cnmt_ctx);
    printf("\n");
    nca_create_control(&settings, &cnmt_ctx);
    printf("\n");
    if (settings.htmldoc_romfs_dir.valid == VALIDITY_VALID)
    {
        nca_create_manual_htmldoc(&settings, &cnmt_ctx);
        printf("\n");
    }
    if (settings.legalinfo_romfs_dir.valid == VALIDITY_VALID)
    {
        nca_create_manual_legalinfo(&settings, &cnmt_ctx);
        printf("\n");
    }
    nca_create_meta(&settings, &cnmt_ctx);
    printf("\n");

    // Create NSP
    printf("----> Creating NSP:\n");
    filepath_t nsp_file_path;
    filepath_init(&nsp_file_path);
    filepath_copy(&nsp_file_path, &settings.nsp_dir);
    filepath_append(&nsp_file_path, "%016" PRIx64 ".nsp", cnmt_ctx.cnmt_header.title_id);
    uint64_t pfs0_size;
    pfs0_build(&settings.nca_dir, &nsp_file_path, &pfs0_size);
    printf("\n----> Created NSP: %s\n", nsp_file_path.char_path);

    // Remove temp and nca directories
    printf("\n");
    if (settings.keepncadir == 1)
    {
        printf("Removing created temp directory\n");
        filepath_remove_directory(&settings.temp_dir);
    }
    else
    {
        printf("Removing created temp and nca directories\n");
        filepath_remove_directory(&settings.temp_dir);
        filepath_remove_directory(&settings.nca_dir);
    }

    // Summary
    printf("\n\n");
    printf("Summary:\n\n");
    printf("Title ID: %016" PRIx64 "\n", cnmt_ctx.cnmt_header.title_id);
    printf("SDK Version: %" PRId8 ".%" PRId8 ".%" PRId8 ".%" PRId8 "\n", settings.sdk_major, settings.sdk_minor, settings.sdk_micro, settings.sdk_revision);
    if (settings.plaintext == 0)
        printf("Section Crypto Type: Regular Crypto\n");
    else
        printf("Sections Crypto Type: Plaintext\n");
    printf("Keygeneration: %i\n", settings.keygeneration);
    char keyareakey_hex[33];
    hexBinaryString(settings.keyareakey, 16, keyareakey_hex, 33);
    keyareakey_hex[32] = '\0';
    printf("Key area key 2: %s\n", keyareakey_hex);
    if (settings.noromfs == 0)
        printf("Program NCA RomFS Section: Yes\n");
    else
        printf("Program NCA RomFS Section: No\n");
    if (settings.nologo == 0)
        printf("Program NCA Logo Section: Yes\n");
    else
        printf("Program NCA Logo Section: No\n");
    if (settings.htmldoc_romfs_dir.valid == VALIDITY_VALID)
        printf("HtmlDoc NCA: Yes\n");
    else
        printf("HtmlDoc NCA: No\n");
    if (settings.legalinfo_romfs_dir.valid == VALIDITY_VALID)
        printf("LegalInfo NCA: Yes\n");
    else
        printf("LegalInfo NCA: No\n");
    printf("Created NSP: %s\n", nsp_file_path.char_path);

    free(settings.keyareakey);
    return EXIT_SUCCESS;
}