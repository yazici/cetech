//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <memory.h>

#include <celib/memory/allocator.h>
#include <celib/macros.h>
#include <celib/containers/buffer.h>
#include <celib/api.h>

#include <celib/module.h>
#include <celib/memory/memory.h>
#include <celib/config.h>
#include <celib/id.h>
#include <celib/fs.h>
#include <celib/cdb.h>
#include <celib/os/path.h>
#include <celib/os/object.h>

#include "celib/log.h"

//==============================================================================
// Defines
//==============================================================================

#define MAX_MODULES 128
#define MAX_PATH_LEN 256
#define MAX_MODULE_NAME 128

#define _G ModuleGlobals
#define LOG_WHERE "module"

//==============================================================================
// Globals
//==============================================================================
typedef struct module_t {
    char name[MAX_MODULE_NAME];
    void *handler;
    ce_load_module_t0 *load;
    ce_unload_module_t0 *unload;
} module_t;

static struct _G {
    module_t modules[MAX_MODULES];
    char path[MAX_MODULES][MAX_PATH_LEN];
    char used[MAX_MODULES];
    uint64_t config;
    ce_alloc_t0 *allocator;
} _G = {};


//==============================================================================
// Private
//==============================================================================

static void _add_module(const char *path,
                        struct module_t *module) {

    for (size_t i = 0; i < MAX_MODULES; ++i) {
        if (_G.used[i]) {
            continue;
        }

        memcpy(_G.path[i], path, strlen(path) + 1);

        _G.used[i] = 1;
        _G.modules[i] = *module;

        break;
    }
}

static const char *_get_module_name(const char *path,
                                    uint32_t *len) {
    const char *filename = ce_os_path_a0->filename(path);
    const char *name = strchr(filename, '_');
    if (NULL == name) {
        return NULL;
    }

    ++name;
    *len = strlen(name);

    return name;
}

static void _get_module_fce_name(char **buffer,
                                 const char *name,
                                 uint32_t name_len,
                                 const char *fce_name) {
    ce_buffer_clear(*buffer);
    ce_buffer_printf(buffer, _G.allocator, "%s%s", name, fce_name);
}


static bool _load_from_path(module_t *module,
                            const char *path) {
    uint32_t name_len;
    const char *name = _get_module_name(path, &name_len);

    char *buffer = NULL;

    _get_module_fce_name(&buffer, name, name_len, "_load_module");

    void *obj = ce_os_object_a0->load(path);
    if (obj == NULL) {
        return false;
    }

    ce_load_module_t0 *load_fce = ce_os_object_a0->load_function(obj, (buffer));
    if (load_fce == NULL) {
        return false;
    }

    _get_module_fce_name(&buffer, name, name_len, "_unload_module");

    ce_unload_module_t0 *unload_fce = ce_os_object_a0->load_function(obj, buffer);
    if (unload_fce == NULL) {
        return false;
    }

    *module = (module_t) {
            .handler = obj,
            .load = load_fce,
            .unload = unload_fce,
    };

    strncpy(module->name, name, MAX_MODULE_NAME);

    return true;
}

//==============================================================================
// Interface
//==============================================================================


static void add_static(const char *name,
                       ce_load_module_t0 load,
                       ce_unload_module_t0 unload) {
    module_t module = {
            .load = load,
            .unload = unload,
            .handler = NULL
    };

    strncpy(module.name, name, MAX_MODULE_NAME);

    ce_log_a0->info(LOG_WHERE, "Load module \"%s\"", name);

    _add_module("__STATIC__", &module);
    load(ce_api_a0, 0);
}


static void load(const char *path) {
    ce_log_a0->info(LOG_WHERE, "Loading module %s", path);

    module_t module;

    if (!_load_from_path(&module, path)) {
        return;
    }

    module.load(ce_api_a0, 0);

    _add_module(path, &module);
}

static void reload(const char *path) {
    for (size_t i = 0; i < MAX_MODULES; ++i) {
        struct module_t old_module = _G.modules[i];

        if ((old_module.handler == NULL) ||
            (strcmp(_G.path[i], path)) != 0) {
            continue;
        }

        struct module_t new_module;
        if (!_load_from_path(&new_module, path)) {
            continue;
        }

        new_module.load(ce_api_a0, 1);

        old_module.unload(ce_api_a0, 1);
        _G.modules[i] = new_module;

        ce_os_object_a0->unload(old_module.handler);

        break;
    }
}

static void reload_all() {
    for (size_t i = 0; i < MAX_MODULES; ++i) {
        if (_G.modules[i].handler == NULL) {
            continue;
        }

        reload(_G.path[i]);
    }
}


static void load_dirs(const char *path) {
    char key[64];
    size_t len = strlen("load_module.");
    strcpy(key, "load_module.");

    char *buffer = NULL;
    for (int i = 0; true; ++i) {
        ce_buffer_clear(buffer);

        sprintf(key + len, "%d", i);

        const uint64_t key_id = ce_id_a0->id64(key);

        const ce_cdb_obj_o0 *reader = ce_cdb_a0->read(ce_cdb_a0->db(),
                                                      _G.config);

        if (!ce_cdb_a0->prop_exist(reader, key_id)) {
            break;
        }


        const char *module_file = ce_cdb_a0->read_str(reader, key_id, "");
        ce_os_path_a0->join(&buffer,
                            _G.allocator,
                            2, path, module_file);
        load(buffer);

        ce_buffer_free(buffer, _G.allocator);
    }
}

static void unload_all() {
    for (int i = MAX_MODULES - 1; i >= 0; --i) {
        if (!_G.used[i]) {
            continue;
        }

        ce_log_a0->info(LOG_WHERE, "Unload module \"%s\"", _G.modules[i].name);

        _G.modules[i].unload(ce_api_a0, 0);
    }
}

//static void check_modules() {
//    ce_alloc_t0 *alloc = ce_memory_a0->system;
//
//    static uint64_t root = CE_ID64_0("modules", 0x6fd8ce9161fffc7ULL);
//
//    auto *wd_it = ce_fs_a0->event_begin(root);
//    const auto *wd_end = ce_fs_a0->event_end(root);
//
//    while (wd_it != wd_end) {
//        if (wd_it->type == CE_WATCHDOG_EVENT_FILE_MODIFIED) {
//            ce_wd_ev_file_write_end *ev = (ce_wd_ev_file_write_end *) wd_it;
//
//            const char *ext = ce_os_path_a0->extension(ev->filename);
//
//            if ((NULL != ext) && (strcmp(ext, "so") == 0)) {
//
//
//                char *path = NULL;
//                ce_os_path_a0->join(&path, alloc, 2, ev->dir, ev->filename);
//
//                char full_path[4096];
//                ce_fs_a0->get_full_path(root, path, full_path,
//                                               CE_ARRAY_LEN(full_path));
//
//                int pat_size = strlen(full_path);
//                int ext_size = strlen(ext);
//                full_path[pat_size - ext_size - 1] = '\0';
//
//                ce_log_a0->info(LOG_WHERE,
//                               "Reload module from path \"%s\"",
//                               full_path);
//
//                reload(full_path);
//
//                ce_buffer_free(path, alloc);
//            }
//        }
//
//        wd_it = ce_fs_a0->event_next(wd_it);
//    }
//}

static struct ce_module_a0 module_api = {
        .reload = reload,
        .reload_all = reload_all,
        .add_static = add_static,
        .load = load,
        .unload_all = unload_all,
        .load_dirs = load_dirs,
};

struct ce_module_a0 *ce_module_a0 = &module_api;

void CE_MODULE_LOAD(module)(struct ce_api_a0 *api,
                            int reload) {
    CE_UNUSED(reload);
    _G = (struct _G) {
            .allocator = ce_memory_a0->system,
            .config = ce_config_a0->obj()
    };

    ce_api_a0 = api;
    api->register_api(CE_MODULE0_API, &module_api, sizeof(module_api));


}

void CE_MODULE_UNLOAD(module)(struct ce_api_a0 *api,
                              int reload) {

    CE_UNUSED(reload);
    CE_UNUSED(api);
    ce_log_a0->debug(LOG_WHERE, "Shutdown");

    _G = (struct _G) {};
}
