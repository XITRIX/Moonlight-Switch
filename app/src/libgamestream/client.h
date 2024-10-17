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

#pragma once

#include "Data.hpp"
#include "xml.h"
#include <Limelight.h>
#include <stdbool.h>

#define MIN_SUPPORTED_GFE_VERSION 3
#define MAX_SUPPORTED_GFE_VERSION 7

typedef struct _SERVER_DATA {
    std::string address;
    std::string serverInfoAppVersion;
    std::string serverInfoGfeVersion;
    std::string mac;
    std::string gpuType;
    bool paired;
    bool supports4K;
    int currentGame;
    int serverMajorVersion;
    std::string gsVersion;
    std::string hostname;
    SERVER_INFORMATION serverInfo;
    unsigned short httpPort;
    unsigned short httpsPort;
} SERVER_DATA, *PSERVER_DATA;

void gs_set_error(std::string error);
std::string gs_error();

int gs_init(PSERVER_DATA server, const std::string address);
int gs_app_boxart(PSERVER_DATA server, int app_id, Data* out);
int gs_start_app(PSERVER_DATA server, PSTREAM_CONFIGURATION config, int appId, bool sops, bool localaudio, int gamepad_mask);
int gs_applist(PSERVER_DATA server, PAPP_LIST* app_list);
int gs_unpair(PSERVER_DATA server);
int gs_pair(PSERVER_DATA server, char* pin);
int gs_quit_app(PSERVER_DATA server);
