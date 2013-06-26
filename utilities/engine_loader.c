/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "utilities/engine_loader.h"
#include <memcached/types.h>
#include <platform/platform.h>

static const char * const feature_descriptions[] = {
    "compare and swap",
    "persistent storage",
    "secondary engine",
    "access control",
    "multi tenancy",
    "LRU"
};

cb_dlhandle_t *handle = NULL;

void unload_engine(void)
{
    if (handle != NULL) {
        cb_dlclose(handle);
    }
}

bool load_engine(const char *soname,
                 SERVER_HANDLE_V1 *(*get_server_api)(void),
                 EXTENSION_LOGGER_DESCRIPTOR *logger,
                 ENGINE_HANDLE **engine_handle)
{
    ENGINE_HANDLE *engine = NULL;
    /* Hack to remove the warning from C99 */
    union my_hack {
        CREATE_INSTANCE create;
        void* voidptr;
    } my_create;
    void *symbol;
    ENGINE_ERROR_CODE error;
    char *errmsg;

    handle = cb_dlopen(soname, &errmsg);
    if (handle == NULL) {
        logger->log(EXTENSION_LOG_WARNING, NULL,
                    "Failed to open library \"%s\": %s\n",
                    soname ? soname : "self",
                    errmsg);
        free(errmsg);
        return false;
    }

    symbol = cb_dlsym(handle, "create_instance", &errmsg);
    if (symbol == NULL) {
        logger->log(EXTENSION_LOG_WARNING, NULL,
                    "Could not find symbol \"create_instance\" in %s: %s\n",
                    soname ? soname : "self", errmsg);
        free(errmsg);
        return false;
    }
    my_create.voidptr = symbol;

    /* request a instance with protocol version 1 */
    error = (*my_create.create)(1, get_server_api, &engine);

    if (error != ENGINE_SUCCESS || engine == NULL) {
        logger->log(EXTENSION_LOG_WARNING, NULL,
                "Failed to create instance. Error code: %d\n", error);
        cb_dlclose(handle);
        return false;
    }
    *engine_handle = engine;
    return true;
}

bool init_engine(ENGINE_HANDLE * engine,
                 const char *config_str,
                 EXTENSION_LOGGER_DESCRIPTOR *logger)
{
    ENGINE_HANDLE_V1 *engine_v1 = NULL;
    ENGINE_ERROR_CODE error;

    if (handle == NULL) {
        logger->log(EXTENSION_LOG_WARNING, NULL,
                "Failed to initialize engine, engine must fist be loaded.");
        return false;
    }

    if (engine->interface == 1) {
        engine_v1 = (ENGINE_HANDLE_V1*)engine;

        /* validate that the required engine interface is implemented: */
        if (engine_v1->get_info == NULL || engine_v1->initialize == NULL ||
            engine_v1->destroy == NULL || engine_v1->allocate == NULL ||
            engine_v1->remove == NULL || engine_v1->release == NULL ||
            engine_v1->get == NULL || engine_v1->store == NULL ||
            engine_v1->flush == NULL ||
            engine_v1->get_stats == NULL || engine_v1->reset_stats == NULL ||
            engine_v1->item_set_cas == NULL ||
            engine_v1->get_item_info == NULL)
        {
            logger->log(EXTENSION_LOG_WARNING, NULL,
                        "Failed to initialize engine; it does not implement the engine interface.");
            return false;
        }

        error = engine_v1->initialize(engine,config_str);
        if (error != ENGINE_SUCCESS) {
            engine_v1->destroy(engine, false);
            logger->log(EXTENSION_LOG_WARNING, NULL,
                    "Failed to initialize instance. Error code: %d\n",
                    error);
            cb_dlclose(handle);
            return false;
        }
    } else {
        logger->log(EXTENSION_LOG_WARNING, NULL,
                 "Unsupported interface level\n");
        cb_dlclose(handle);
        return false;
    }
    return true;
}

void log_engine_details(ENGINE_HANDLE * engine,
                        EXTENSION_LOGGER_DESCRIPTOR *logger)
{
    ENGINE_HANDLE_V1 *engine_v1 = (ENGINE_HANDLE_V1*)engine;
    const engine_info *info;
    info = engine_v1->get_info(engine);
    if (info) {
        ssize_t offset;
        bool comma;
        char message[4096];
        ssize_t nw = snprintf(message, sizeof(message), "Loaded engine: %s\n",
                                        info->description ?
                                        info->description : "Unknown");
        if (nw == -1) {
            return;
        }
        offset = nw;
        comma = false;

        if (info->num_features > 0) {
            int ii;
            nw = snprintf(message + offset, sizeof(message) - offset,
                          "Supplying the following features: ");
            if (nw == -1) {
                return;
            }
            offset += nw;
            for (ii = 0; ii < info->num_features; ++ii) {
                if (info->features[ii].description != NULL) {
                    nw = snprintf(message + offset, sizeof(message) - offset,
                                  "%s%s", comma ? ", " : "",
                                  info->features[ii].description);
                } else {
                    if (info->features[ii].feature <= LAST_REGISTERED_ENGINE_FEATURE) {
                        nw = snprintf(message + offset, sizeof(message) - offset,
                                      "%s%s", comma ? ", " : "",
                                      feature_descriptions[info->features[ii].feature]);
                    } else {
                        nw = snprintf(message + offset, sizeof(message) - offset,
                                      "%sUnknown feature: %d", comma ? ", " : "",
                                      info->features[ii].feature);
                    }
                }
                comma = true;
                if (nw == -1) {
                    return;
                }
                offset += nw;
            }
        }
        logger->log(EXTENSION_LOG_INFO, NULL, "%s\n", message);
    } else {
        logger->log(EXTENSION_LOG_INFO, NULL,
                                        "Loaded engine: Unknown\n");
    }
}
