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

#include "xml.h"
#include "client.h"
#include "errors.h"

#include <expat.h>
#include <string.h>

#define STATUS_OK 200

struct xml_query {
    char* memory;
    size_t size;
    int start;
    void* data;
};

static void XMLCALL _xml_start_element(void* userData, const char* name,
                                       const char** atts) {
    struct xml_query* search = (struct xml_query*)userData;
    if (strcmp((const char*)search->data, name) == 0) {
        search->start++;
    }
}

static void XMLCALL _xml_end_element(void* userData, const char* name) {
    struct xml_query* search = (struct xml_query*)userData;
    if (strcmp((const char*)search->data, name) == 0) {
        search->start--;
    }
}

static void XMLCALL _xml_start_applist_element(void* userData, const char* name,
                                               const char** atts) {
    struct xml_query* search = (struct xml_query*)userData;
    if (strcmp("App", name) == 0) {
        PAPP_LIST app = (PAPP_LIST)malloc(sizeof(APP_LIST));
        if (app == NULL) {
            return;
        }

        app->id = 0;
        app->name = NULL;
        app->next = (PAPP_LIST)search->data;
        search->data = app;
    } else if (strcmp("ID", name) == 0 || strcmp("AppTitle", name) == 0) {
        search->memory = (char*)malloc(1);
        search->size = 0;
        search->start = 1;
    }
}

static void XMLCALL _xml_end_applist_element(void* userData, const char* name) {
    struct xml_query* search = (struct xml_query*)userData;
    if (search->start) {
        PAPP_LIST list = (PAPP_LIST)search->data;
        if (list == NULL)
            return;

        if (strcmp("ID", name) == 0) {
            list->id = atoi(search->memory);
            free(search->memory);
        } else if (strcmp("AppTitle", name) == 0) {
            list->name = search->memory;
        }
        search->start = 0;
    }
}

static void XMLCALL _xml_start_mode_element(void* userData, const char* name,
                                            const char** atts) {
    struct xml_query* search = (struct xml_query*)userData;
    if (strcmp("DisplayMode", name) == 0) {
        PDISPLAY_MODE mode = (PDISPLAY_MODE)calloc(1, sizeof(DISPLAY_MODE));
        if (mode != NULL) {
            mode->next = (PDISPLAY_MODE)search->data;
            search->data = mode;
        }
    } else if (search->data != NULL &&
               (strcmp("Height", name) == 0 || strcmp("Width", name) == 0 ||
                strcmp("RefreshRate", name) == 0)) {
        search->memory = (char*)malloc(1);
        search->size = 0;
        search->start = 1;
    }
}

static void XMLCALL _xml_end_mode_element(void* userData, const char* name) {
    struct xml_query* search = (struct xml_query*)userData;
    if (search->data != NULL && search->start) {
        PDISPLAY_MODE mode = (PDISPLAY_MODE)search->data;
        if (strcmp("Width", name) == 0) {
            mode->width = atoi(search->memory);
        } else if (strcmp("Height", name) == 0) {
            mode->height = atoi(search->memory);
        } else if (strcmp("RefreshRate", name) == 0) {
            mode->refresh = atoi(search->memory);
        }

        free(search->memory);
        search->start = 0;
    }
}

static void XMLCALL _xml_start_status_element(void* userData, const char* name,
                                              const char** atts) {
    if (strcmp("root", name) == 0) {
        int* status = (int*)userData;
        for (int i = 0; atts[i]; i += 2) {
            if (strcmp("status_code", atts[i]) == 0) {
                *status = atoi(atts[i + 1]);
            } else if (*status != STATUS_OK &&
                       strcmp("status_message", atts[i]) == 0) {
                gs_set_error(strdup(atts[i + 1]));
            }
        }
    }
}

static void XMLCALL _xml_end_status_element(void* userData, const char* name) {}

static void XMLCALL _xml_write_data(void* userData, const XML_Char* s,
                                    int len) {
    struct xml_query* search = (struct xml_query*)userData;
    if (search->start > 0) {
        search->memory = (char*)realloc(search->memory, search->size + len + 1);
        if (search->memory == NULL) {
            return;
        }

        memcpy(&(search->memory[search->size]), s, len);
        search->size += len;
        search->memory[search->size] = 0;
    }
}

int xml_search(const Data& data, const std::string node, std::string* result) {
    struct xml_query search;
    search.data = (void*)node.c_str();
    search.start = 0;
    search.memory = (char*)calloc(1, 1);
    search.size = 0;

    XML_Parser parser = XML_ParserCreate("UTF-8");
    XML_SetUserData(parser, &search);
    XML_SetElementHandler(parser, _xml_start_element, _xml_end_element);
    XML_SetCharacterDataHandler(parser, _xml_write_data);

    if (!XML_Parse(parser, (const char*)data.bytes(), (int)data.size(), 1)) {
        XML_Error code = XML_GetErrorCode(parser);
        gs_set_error(XML_ErrorString(code));
        XML_ParserFree(parser);
        free(search.memory);
        return GS_INVALID;
    } else if (search.memory == NULL) {
        XML_ParserFree(parser);
        return GS_OUT_OF_MEMORY;
    }

    XML_ParserFree(parser);
    *result = std::string(search.memory);
    return GS_OK;
}

int xml_applist(const Data& data, PAPP_LIST* app_list) {
    struct xml_query query;
    query.memory = (char*)calloc(1, 1);
    query.size = 0;
    query.start = 0;
    query.data = NULL;

    XML_Parser parser = XML_ParserCreate("UTF-8");
    XML_SetUserData(parser, &query);
    XML_SetElementHandler(parser, _xml_start_applist_element,
                          _xml_end_applist_element);
    XML_SetCharacterDataHandler(parser, _xml_write_data);

    if (!XML_Parse(parser, (const char*)data.bytes(), (int)data.size(), 1)) {
        XML_Error code = XML_GetErrorCode(parser);
        gs_set_error(XML_ErrorString(code));
        XML_ParserFree(parser);
        return GS_INVALID;
    }

    XML_ParserFree(parser);
    *app_list = (PAPP_LIST)query.data;
    return GS_OK;
}

int xml_modelist(const Data& data, PDISPLAY_MODE* mode_list) {
    struct xml_query query = {0};
    query.memory = (char*)calloc(1, 1);
    XML_Parser parser = XML_ParserCreate("UTF-8");
    XML_SetUserData(parser, &query);
    XML_SetElementHandler(parser, _xml_start_mode_element,
                          _xml_end_mode_element);
    XML_SetCharacterDataHandler(parser, _xml_write_data);

    if (!XML_Parse(parser, (const char*)data.bytes(), (int)data.size(), 1)) {
        XML_Error code = XML_GetErrorCode(parser);
        gs_set_error(XML_ErrorString(code));
        XML_ParserFree(parser);
        return GS_INVALID;
    }

    XML_ParserFree(parser);
    *mode_list = (PDISPLAY_MODE)query.data;
    return GS_OK;
}

int xml_status(const Data& data) {
    int status = 0;
    XML_Parser parser = XML_ParserCreate("UTF-8");
    XML_SetUserData(parser, &status);
    XML_SetElementHandler(parser, _xml_start_status_element,
                          _xml_end_status_element);

    if (!XML_Parse(parser, (const char*)data.bytes(), (int)data.size(), 1)) {
        XML_Error code = XML_GetErrorCode(parser);
        gs_set_error(XML_ErrorString(code));
        XML_ParserFree(parser);
        return GS_INVALID;
    }

    XML_ParserFree(parser);
    return status == STATUS_OK ? GS_OK : GS_ERROR;
}
