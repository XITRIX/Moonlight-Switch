/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
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

#include "Data.hpp"
#pragma once

typedef struct _APP_LIST {
    char* name;
    int id;
    struct _APP_LIST* next;
} APP_LIST, *PAPP_LIST;

int xml_search(const Data& data, const std::string node, int* result);
int xml_search(const Data& data, const std::string node, std::string* result);
int xml_applist(const Data& data, PAPP_LIST* app_list);
int xml_status(const Data& data);
