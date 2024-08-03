/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "Settings.hpp"
#include "client.h"
#include "CryptoManager.hpp"
#include "errors.h"
#include "http.h"
#include <Limelight.h>
#include <borealis/core/logger.hpp>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CHANNEL_COUNT_STEREO 2
#define CHANNEL_COUNT_51_SURROUND 6

#define CHANNEL_MASK_STEREO 0x3
#define CHANNEL_MASK_51_SURROUND 0xFC

static std::string unique_id = "0123456789ABCDEF";

static int load_server_status(PSERVER_DATA server, bool skip_https) {
    int ret;
    char url[4096];
    int i = skip_https ? 1 : 0;

    do {
        std::string pairedText;
        std::string currentGameText;
        std::string stateText;

        ret = GS_INVALID;

        // Modern GFE versions don't allow serverinfo to be fetched over HTTPS
        // if the client is not already paired. Since we can't pair without
        // knowing the server version, we make another request over HTTP if the
        // HTTPS request fails. We can't just use HTTP for everything because it
        // doesn't accurately tell us if we're paired.

        snprintf(url, sizeof(url), "%s://%s:%d/serverinfo?uniqueid=%s",
                 i == 0 ? "https" : "http", server->serverInfo.address,
                 i == 0 ? 47984 : 47989, unique_id.c_str());

        Data data;

        if (http_request(url, &data, HTTPRequestTimeoutLow) != GS_OK) {
            ret = GS_IO_ERROR;
            goto cleanup;
        }

        if (xml_status(data) == GS_ERROR) {
            ret = GS_ERROR;
            goto cleanup;
        }

        if (xml_search(data, "currentgame", &currentGameText) != GS_OK) {
            goto cleanup;
        }

        if (xml_search(data, "PairStatus", &pairedText) != GS_OK)
            goto cleanup;

        if (xml_search(data, "appversion", &server->serverInfoAppVersion) !=
            GS_OK) {
            goto cleanup;
        }

        if (xml_search(data, "state", &stateText) != GS_OK)
            goto cleanup;

        if (xml_search(data, "ServerCodecModeSupport",
                       &server->serverInfo.serverCodecModeSupport) != GS_OK)
            goto cleanup;

        if (xml_search(data, "gputype", &server->gpuType) != GS_OK)
            goto cleanup;

        if (xml_search(data, "GsVersion", &server->gsVersion) != GS_OK)
            goto cleanup;

        if (xml_search(data, "hostname", &server->hostname) != GS_OK)
            goto cleanup;

        if (xml_search(data, "GfeVersion", &server->serverInfoGfeVersion) !=
            GS_OK)
            goto cleanup;

        if (xml_search(data, "mac", &server->mac) != GS_OK)
            goto cleanup;

        // These fields are present on all version of GFE that this client
        // supports
        if (currentGameText.empty() || pairedText.empty() ||
            server->serverInfoAppVersion.empty() || stateText.empty()) {
            goto cleanup;
        }

        server->paired = pairedText == "1";
        server->currentGame =
            currentGameText.empty() ? 0 : atoi(currentGameText.c_str());
        server->supports4K = server->serverInfo.serverCodecModeSupport != 0;
        server->serverMajorVersion = atoi(server->serverInfoAppVersion.c_str());

        if (stateText == "_SERVER_BUSY") {
            // After GFE 2.8, current game remains set even after streaming
            // has ended. We emulate the old behavior by forcing it to zero
            // if streaming is not active.
            server->currentGame = 0;
        }
        ret = GS_OK;

    cleanup:
        i++;
    } while (ret != GS_OK && i < 2);

    if (ret == GS_OK && !server->unsupported) {
        if (server->serverMajorVersion > MAX_SUPPORTED_GFE_VERSION) {
            gs_set_error(
                "Ensure you're running the latest version of Moonlight "
                "Embedded or downgrade GeForce Experience and try again");
            ret = GS_UNSUPPORTED_VERSION;
        } else if (server->serverMajorVersion < MIN_SUPPORTED_GFE_VERSION) {
            gs_set_error(
                "Moonlight Embedded requires a newer version of GeForce "
                "Experience. Please upgrade GFE on your PC and try again.");
            ret = GS_UNSUPPORTED_VERSION;
        }
    }

    return ret;
}

static std::string _gs_error = "";

void gs_set_error(std::string error) { _gs_error = error; }

std::string gs_error() {
    if (_gs_error.empty()) {
        return "Unknown error...";
    }
    return _gs_error;
}

int gs_unpair(PSERVER_DATA server) {
    int ret = GS_OK;
    char url[4096];

    Data data;

    snprintf(url, sizeof(url), "http://%s:47989/unpair?uniqueid=%s",
             server->serverInfo.address, unique_id.c_str());
    ret = http_request(url, &data, HTTPRequestTimeoutLow);
    return ret;
}

static int gs_pair_validate(Data& data, std::string* result) {
    *result = "";

    int ret = GS_OK;
    if ((ret = xml_status(data) != GS_OK)) {
        return ret;
    } else if ((ret = xml_search(data, "paired", result)) != GS_OK) {
        return ret;
    }

    //    if (strcmp(*result, "1") != 0) {
    //        gs_error = "Pairing failed";
    //        ret = GS_FAILED;
    //    }

    return ret;
}

static int gs_pair_cleanup(int ret, PSERVER_DATA server, std::string* result) {
    if (ret != GS_OK) {
        gs_unpair(server);
    }
    return ret;
}

int gs_pair(PSERVER_DATA server, char* pin) {
    int ret = GS_OK;
    Data data;
    std::string result;
    char url[4096];

    if (server->paired) {
        gs_set_error("Already paired");
        return GS_WRONG_STATE;
    }

    if (server->currentGame != 0) {
        gs_set_error(
            "The computer is currently in a game. You must close the game "
            "before pairing");
        return GS_WRONG_STATE;
    }

    brls::Logger::info("Client: Pairing with generation {} server",
                       server->serverMajorVersion);
    brls::Logger::info("Client: Start pairing stage #1");

    Data salt = Data::random_bytes(16);
    Data salted_pin = salt.append(Data(pin, strlen(pin)));
//    brls::Logger::info("Client: PIN: {}, salt {}", pin, salt.hex().bytes());

    snprintf(url, sizeof(url),
             "http://%s:47989/"
             "pair?uniqueid=%s&devicename=roth&updateState=1&phrase="
             "getservercert&salt=%s&clientcert=%s",
             server->serverInfo.address, unique_id.c_str(), salt.hex().bytes(),
             CryptoManager::cert_data().hex().bytes());

    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = gs_pair_validate(data, &result) != GS_OK)) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = xml_search(data, "plaincert", &result)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    brls::Logger::info("Client: Start pairing stage #2");

    Data plainCert = Data((char*)result.c_str(), result.size());
    Data aesKey;

    // Gen 7 servers use SHA256 to get the key
    int hashLength;
    if (server->serverMajorVersion >= 7) {
        aesKey = CryptoManager::create_AES_key_from_salt_SHA256(salted_pin);
        hashLength = 32;
    } else {
        aesKey = CryptoManager::create_AES_key_from_salt_SHA1(salted_pin);
        hashLength = 20;
    }

    Data randomChallenge = Data::random_bytes(16);
    Data encryptedChallenge =
        CryptoManager::aes_encrypt(randomChallenge, aesKey);

    snprintf(
        url, sizeof(url),
        "http://%s:47989/"
        "pair?uniqueid=%s&devicename=roth&updateState=1&clientchallenge=%s",
        server->serverInfo.address, unique_id.c_str(),
        encryptedChallenge.hex().bytes());

    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = gs_pair_validate(data, &result) != GS_OK)) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if (xml_search(data, "challengeresponse", &result) != GS_OK) {
        ret = GS_INVALID;
        return gs_pair_cleanup(ret, server, &result);
    }

    brls::Logger::info("Client: Start pairing stage #3");

    Data encServerChallengeResp =
        Data((char*)result.c_str(), result.size()).hex_to_bytes();
    Data decServerChallengeResp =
        CryptoManager::aes_decrypt(encServerChallengeResp, aesKey);
    Data serverResponse = decServerChallengeResp.subdata(0, hashLength);
    Data serverChallenge = decServerChallengeResp.subdata(hashLength, 16);

    Data clientSecret = Data::random_bytes(16);
    Data challengeRespHashInput =
        serverChallenge
            .append(CryptoManager::signature(CryptoManager::cert_data()))
            .append(clientSecret);

    Data challengeRespHash;

    if (server->serverMajorVersion >= 7) {
        challengeRespHash =
            CryptoManager::SHA256_hash_data(challengeRespHashInput);
    } else {
        challengeRespHash =
            CryptoManager::SHA1_hash_data(challengeRespHashInput);
    }
    Data challengeRespEncrypted =
        CryptoManager::aes_encrypt(challengeRespHash, aesKey);

    snprintf(
        url, sizeof(url),
        "http://%s:47989/"
        "pair?uniqueid=%s&devicename=roth&updateState=1&serverchallengeresp=%s",
        server->serverInfo.address, unique_id.c_str(),
        challengeRespEncrypted.hex().bytes());

    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = gs_pair_validate(data, &result) != GS_OK)) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if (xml_search(data, "pairingsecret", &result) != GS_OK) {
        ret = GS_INVALID;
        return gs_pair_cleanup(ret, server, &result);
    }

    brls::Logger::info("Client: Start pairing stage #4");

    Data serverSecretResp =
        Data((char*)result.c_str(), result.size()).hex_to_bytes();
    Data serverSecret = serverSecretResp.subdata(0, 16);
    Data serverSignature = serverSecretResp.subdata(16, 256);

    if (!CryptoManager::verify_signature(serverSecret, serverSignature,
                                         plainCert.hex_to_bytes())) {
        gs_set_error("MITM attack detected");
        ret = GS_FAILED;
        return gs_pair_cleanup(ret, server, &result);
    }

    Data serverChallengeRespHashInput =
        randomChallenge
            .append(CryptoManager::signature(plainCert.hex_to_bytes()))
            .append(serverSecret);
    Data serverChallengeRespHash;

    if (server->serverMajorVersion >= 7) {
        serverChallengeRespHash =
            CryptoManager::SHA256_hash_data(serverChallengeRespHashInput);
    } else {
        serverChallengeRespHash =
            CryptoManager::SHA1_hash_data(serverChallengeRespHashInput);
    }

    Data clientPairingSecret = clientSecret.append(
        CryptoManager::sign_data(clientSecret, CryptoManager::key_data()));

    snprintf(
        url, sizeof(url),
        "http://%s:47989/"
        "pair?uniqueid=%s&devicename=roth&updateState=1&clientpairingsecret=%s",
        server->serverInfo.address, unique_id.c_str(),
        clientPairingSecret.hex().bytes());
    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = gs_pair_validate(data, &result) != GS_OK)) {
        return gs_pair_cleanup(ret, server, &result);
    }

    brls::Logger::info("Client: Start pairing stage #5");

    snprintf(
        url, sizeof(url),
        "https://%s:47984/"
        "pair?uniqueid=%s&devicename=roth&updateState=1&phrase=pairchallenge",
        server->serverInfo.address, unique_id.c_str());
    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = gs_pair_validate(data, &result) != GS_OK)) {
        return gs_pair_cleanup(ret, server, &result);
    }

    server->paired = true;

    return gs_pair_cleanup(ret, server, &result);
}

int gs_applist(PSERVER_DATA server, PAPP_LIST* list) {
    int ret = GS_OK;
    char url[4096];
    Data data;

    snprintf(url, sizeof(url), "https://%s:47984/applist?uniqueid=%s",
             server->serverInfo.address, unique_id.c_str());

    if (http_request(url, &data, HTTPRequestTimeoutMedium) != GS_OK)
        ret = GS_IO_ERROR;
    else if (xml_status(data) == GS_ERROR)
        ret = GS_ERROR;
    else if (xml_applist(data, list) != GS_OK)
        ret = GS_INVALID;
    return ret;
}

int gs_app_boxart(PSERVER_DATA server, int app_id, Data* out) {
    int ret = GS_OK;
    char url[4096];
    Data data;

    snprintf(
        url, sizeof(url),
        "https://%s:47984/appasset?uniqueid=%s&appid=%d&AssetType=2&AssetIdx=0",
        server->serverInfo.address, unique_id.c_str(), app_id);

    if (http_request(url, &data, HTTPRequestTimeoutMedium) != GS_OK) {
        ret = GS_IO_ERROR;
    } else {
        *out = data;
    }
    return ret;
}

int gs_start_app(PSERVER_DATA server, STREAM_CONFIGURATION* config, int appId,
                 bool sops, bool localaudio, int gamepad_mask) {
    int ret = GS_OK;
    std::string result;

    if (server->unsupported) {
        gs_set_error(std::string("Mode ") + std::to_string(config->width) +
                     "x" + std::to_string(config->height) + "x" +
                     std::to_string(config->fps) + " not supported");
        return GS_NOT_SUPPORTED_MODE;
    }

    if (config->height >= 2160 && !server->supports4K) {
        gs_set_error("4K not supported");
        return GS_NOT_SUPPORTED_4K;
    }

    Data rand = Data::random_bytes(16);
    memcpy(config->remoteInputAesKey, rand.bytes(), 16);

    char url[4096];
    int rikeyid = 0;

    Data data;

    if (server->currentGame == 0) {
        int channelCounnt =
            config->audioConfiguration == AUDIO_CONFIGURATION_STEREO
                ? CHANNEL_COUNT_STEREO
                : CHANNEL_COUNT_51_SURROUND;
        int mask = config->audioConfiguration == AUDIO_CONFIGURATION_STEREO
                       ? CHANNEL_MASK_STEREO
                       : CHANNEL_MASK_51_SURROUND;
        int fps = sops && config->fps > 60 ? 60 : config->fps;
        snprintf(url, sizeof(url),
                 "https://%s:47984/"
                 "launch?uniqueid=%s&appid=%d&mode=%dx%dx%d&additionalStates=1&"
                 "sops=%d&rikey=%s&rikeyid=%d&localAudioPlayMode=%d&"
                 "surroundAudioInfo=%d&remoteControllersBitmap=%d&gcmap=%d",
                 server->serverInfo.address, unique_id.c_str(), appId,
                 config->width, config->height, fps, sops, rand.hex().bytes(),
                 rikeyid, localaudio, (mask << 16) + channelCounnt,
                 gamepad_mask, gamepad_mask);
    } else {
        snprintf(url, sizeof(url),
                 "https://%s:47984/resume?uniqueid=%s&rikey=%s&rikeyid=%d",
                 server->serverInfo.address, unique_id.c_str(),
                 rand.hex().bytes(), rikeyid);
    }

    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) == GS_OK) {
        server->currentGame = appId;
    } else {
        goto exit;
    }

    if ((ret = xml_status(data) != GS_OK)) {
        goto exit;
    } else if ((ret = xml_search(data, "gamesession", &result)) != GS_OK) {
        goto exit;
    }

    if (result == "0") {
        ret = GS_FAILED;
        goto exit;
    }

exit:
    return ret;
}

int gs_quit_app(PSERVER_DATA server) {
    int ret = GS_OK;
    char url[4096];
    std::string result;
    Data data;

    snprintf(url, sizeof(url), "https://%s:47984/cancel?uniqueid=%s",
             server->serverInfo.address, unique_id.c_str());
    if ((ret = http_request(url, &data, HTTPRequestTimeoutMedium)) != GS_OK)
        goto exit;

    if ((ret = xml_status(data) != GS_OK)) {
        goto exit;
    } else if ((ret = xml_search(data, "cancel", &result)) != GS_OK) {
        goto exit;
    }

    if (result == "0") {
        ret = GS_FAILED;
        goto exit;
    }

exit:
    return ret;
}

int gs_init(PSERVER_DATA server, const std::string address, bool skip_https) {
    if (!CryptoManager::load_cert_key_pair()) {
        brls::Logger::info("Client: No certs, generate new...");

        if (!CryptoManager::generate_new_cert_key_pair()) {
            brls::Logger::info("Client: Failed to generate certs...");
            return GS_FAILED;
        }
    }

    http_init(Settings::instance().key_dir());

    LiInitializeServerInformation(&server->serverInfo);
    server->address = address;
    server->serverInfo.address = server->address.c_str();
    server->unsupported = Settings::instance().ignore_unsupported_resolutions();

    int result = load_server_status(server, skip_https);
    server->serverInfo.serverInfoAppVersion =
        server->serverInfoAppVersion.c_str();
    server->serverInfo.serverInfoGfeVersion =
        server->serverInfoGfeVersion.c_str();
    return result;
}
