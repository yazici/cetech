
#include <celib/stringid/stringid.h>
#include <celib/memory/memory.h>
#include <celib/os/vio.h>
#include <engine/core/resource.h>
#include <celib/yaml/yaml.h>
#include <engine/develop/resource_compiler.h>
#include <celib/string/string.h>
#include <celib/os/path.h>
#include <engine/core/memory.h>
#include <celib/containers/map.h>
#include <engine/core/entcom.h>


#define _G UnitGlobal
static struct G {
    stringid64_t type;
} _G = {0};

//==============================================================================
// Compiler private
//==============================================================================

static void preprocess(const char *filename,
                       yaml_node_t root,
                       struct compilator_api *capi) {
    yaml_node_t prefab_node = yaml_get_node(root, "prefab");

    if (yaml_is_valid(prefab_node)) {
        char prefab_file[256] = {0};
        char prefab_str[256] = {0};
        yaml_as_string(prefab_node, prefab_str, CE_ARRAY_LEN(prefab_str));
        snprintf(prefab_file, CE_ARRAY_LEN(prefab_file), "%s.unit", prefab_str);

        capi->add_dependency(filename, prefab_file);

        char full_path[256] = {0};
        const char *source_dir = resource_compiler_get_source_dir();
        os_path_join(full_path, CE_ARRAY_LEN(full_path), source_dir, prefab_file);

        log_debug("unit_resource", "Loading prefab from: %s", full_path);

        struct vio *prefab_vio = vio_from_file(full_path, VIO_OPEN_READ, memsys_main_allocator());

        char prefab_data[vio_size(prefab_vio) + 1];
        memory_set(prefab_data, 0, vio_size(prefab_vio) + 1);
        vio_read(prefab_vio, prefab_data, sizeof(char), vio_size(prefab_vio));

        yaml_document_t h;
        yaml_node_t prefab_root = yaml_load_str(prefab_data, &h);

        preprocess(filename, prefab_root, capi);
        yaml_merge(root, prefab_root);

        vio_close(prefab_vio);
    }
}

ARRAY_PROTOTYPE(yaml_node_t)

ARRAY_PROTOTYPE_N(struct array_yaml_node_t, array_yaml_node_t)

ARRAY_PROTOTYPE_N(struct array_u32, array_u32)

MAP_PROTOTYPE_N(struct array_yaml_node_t, array_yaml_node_t)

MAP_PROTOTYPE_N(struct array_u32, array_u32)

struct entity_compile_output {
    MAP_T(array_u32) component_ent;
    MAP_T(u32) entity_parent;
    MAP_T(array_yaml_node_t) component_body;
    ARRAY_T(u64) component_type;
    u32 ent_counter;
};

struct foreach_children_data {
    int parent_ent;
    struct entity_compile_output *output;
};


static void compile_entitity(yaml_node_t rootNode,
                             int parent,
                             struct entity_compile_output *output);

void forach_children_clb(yaml_node_t key,
                         yaml_node_t value,
                         void *_data) {
    struct foreach_children_data *data = _data;

    compile_entitity(value, data->parent_ent, data->output);
}


struct foreach_componets_data {
    struct entity_compile_output *output;
    int ent_id;
};

void foreach_components_clb(yaml_node_t key,
                            yaml_node_t value,
                            void *_data) {
    char uid_str[256] = {0};
    char component_type_str[256] = {0};
    stringid64_t cid;
    int contain_cid = 0;

    struct foreach_componets_data *data = _data;
    struct entity_compile_output *output = data->output;

    yaml_as_string(key, uid_str, CE_ARRAY_LEN(uid_str));

    yaml_node_t component_type_node = yaml_get_node(value, "component_type");
    yaml_as_string(component_type_node, component_type_str, CE_ARRAY_LEN(component_type_str));

    log_debug("resouce_compier", "compile component type: %s", component_type_str);

    cid = stringid64_from_string(component_type_str);

    for (int i = 0; i < ARRAY_SIZE(&output->component_type); ++i) {
        if (ARRAY_AT(&output->component_type, i) != cid.id) {
            continue;
        }

        contain_cid = 1;
    }

    if (!contain_cid) {
        ARRAY_PUSH_BACK(u64, &output->component_type, cid.id);

        ARRAY_T(u32) tmp_a = {0};
        ARRAY_INIT(u32, &tmp_a, memsys_main_allocator());

        MAP_SET(array_u32, &output->component_ent, cid.id, tmp_a);
    }

    if (!MAP_HAS(array_yaml_node_t, &output->component_body, cid.id)) {
        ARRAY_T(yaml_node_t) tmp_a = {0};
        ARRAY_INIT(yaml_node_t, &tmp_a, memsys_main_allocator());

        MAP_SET(array_yaml_node_t, &output->component_body, cid.id, tmp_a);

    }
    ARRAY_T(u32) *tmp_a = MAP_GET_PTR(array_u32, &output->component_ent, cid.id);
    ARRAY_PUSH_BACK(u32, tmp_a, data->ent_id);

    ARRAY_T(yaml_node_t) *tmp_b = MAP_GET_PTR(array_yaml_node_t, &output->component_body, cid.id);
    ARRAY_PUSH_BACK(yaml_node_t, tmp_b, value);
}

static void compile_entitity(yaml_node_t rootNode,
                             int parent,
                             struct entity_compile_output *output) {

    u32 ent_id = output->ent_counter++;

    MAP_SET(u32, &output->entity_parent, ent_id, parent);

    yaml_node_t components_node = yaml_get_node(rootNode, "components");
    CE_ASSERT("unit_system", yaml_is_valid(components_node));

    struct foreach_componets_data data = {
            .ent_id = ent_id,
            .output = output
    };

    yaml_node_foreach_dict(components_node, foreach_components_clb, &data);


    yaml_node_t children_node = yaml_get_node(rootNode, "children");
    if (yaml_is_valid(children_node)) {
        int parent_ent = ent_id;
        //output->ent_counter += 1;

        struct foreach_children_data data = {
                .parent_ent = parent_ent,
                .output = output
        };

        yaml_node_foreach_dict(children_node, forach_children_clb, &data);
    }
}

struct unit_resource {
    u32 ent_count;
    u32 comp_type_count;
    //u64 parents [ent_count]
    //u64 comp_types [comp_type_count]
    //component_data cdata[comp_type_count]
};

struct component_data {
    u32 ent_count;
    u32 size;
    // u32 ent[ent_count];
    // char data[ent_count];
};

#define unit_resource_parents(r) ((u32*)((r) + 1))
#define unit_resource_comp_types(r) ((u64*)(unit_resource_parents(r) + ((r)->ent_count)))
#define unit_resource_comp_data(r) ((struct component_data*)(unit_resource_comp_types(r) + ((r)->comp_type_count)))

#define component_data_ent(cd) ((u32*)((cd) + 1))
#define component_data_data(cd) ((char*)((component_data_ent(cd) + ((cd)->ent_count))))

struct entity_compile_output *unit_compiler_create_output() {
    struct allocator *a = memsys_main_allocator();

    struct entity_compile_output *output = CE_ALLOCATE(a, struct entity_compile_output, 1);
    output->ent_counter = 0;
    ARRAY_INIT(u64, &output->component_type, a);
    MAP_INIT(array_u32, &output->component_ent, a);
    MAP_INIT(u32, &output->entity_parent, a);
    MAP_INIT(array_yaml_node_t, &output->component_body, a);

    return output;
}

void unit_compiler_destroy_output(struct entity_compile_output *output) {
    ARRAY_DESTROY(u64, &output->component_type);
    MAP_DESTROY(u32, &output->entity_parent);

    // clean inner array
    const MAP_ENTRY_T(array_u32) *ce_it = MAP_BEGIN(array_u32, &output->component_ent);
    const MAP_ENTRY_T(array_u32) *ce_end = MAP_END(array_u32, &output->component_ent);
    while (ce_it != ce_end) {
        ARRAY_DESTROY(u32, (struct array_u32 *) &ce_it->value);
        ++ce_it;
    }
    MAP_DESTROY(array_u32, &output->component_ent);

    // clean inner array
    const MAP_ENTRY_T(array_yaml_node_t) *cb_it = MAP_BEGIN(array_yaml_node_t, &output->component_body);
    const MAP_ENTRY_T(array_yaml_node_t) *cb_end = MAP_END(array_yaml_node_t, &output->component_body);
    while (cb_it != cb_end) {
        ARRAY_DESTROY(yaml_node_t, (struct array_yaml_node_t *) &cb_it->value);
        ++cb_it;
    }
    MAP_DESTROY(array_yaml_node_t, &output->component_body);

    struct allocator *a = memsys_main_allocator();
    CE_DEALLOCATE(a, output);
}

void unit_compiler_compile_unit(struct entity_compile_output *output,
                                yaml_node_t root,
                                const char *filename,
                                struct compilator_api *compilator_api) {

    preprocess(filename, root, compilator_api);
    compile_entitity(root, UINT32_MAX, output);
}

u32 unit_compiler_ent_counter(struct entity_compile_output *output) {
    return output->ent_counter;
}

void unit_compiler_write_to_build(struct entity_compile_output *output,
                                  ARRAY_T(u8) *build) {
    struct unit_resource res = {0};
    res.ent_count = (u32) (output->ent_counter + 1);
    res.comp_type_count = (u32) ARRAY_SIZE(&output->component_type);

    ARRAY_PUSH(u8, build, (u8 *) &res, sizeof(struct unit_resource));

    //write parents
    for (int i = 0; i < res.ent_count; ++i) {
        u32 id = MAP_GET(u32, &output->entity_parent, i, UINT32_MAX);

        ARRAY_PUSH(u8, build, (u8 *) &id, sizeof(id));
    }

    //write comp types
    ARRAY_PUSH(u8, build, (u8 *) ARRAY_BEGIN(&output->component_type),
               sizeof(u64) * ARRAY_SIZE(&output->component_type));

    //write comp data
    for (int j = 0; j < res.comp_type_count; ++j) {
        u64 cid = ARRAY_AT(&output->component_type, j);
        stringid64_t id = {.id = cid};

        ARRAY_T(u32) *ent_arr = MAP_GET_PTR(array_u32, &output->component_ent, cid);

        struct component_data cdata = {
                .ent_count = ARRAY_SIZE(ent_arr)
        };

        ARRAY_T(yaml_node_t) *body = MAP_GET_PTR(array_yaml_node_t, &output->component_body, cid);
        ARRAY_T(u8) comp_data = {0};
        ARRAY_INIT(u8, &comp_data, memsys_main_allocator());

        for (int i = 0; i < cdata.ent_count; ++i) {
            component_compile(id, ARRAY_AT(body, i), &comp_data);
        }

        cdata.size = ARRAY_SIZE(&comp_data);

        ARRAY_PUSH(u8, build, (u8 *) &cdata, sizeof(cdata));
        ARRAY_PUSH(u8, build, (u8 *) ARRAY_BEGIN(ent_arr), sizeof(u32) * cdata.ent_count);
        ARRAY_PUSH(u8, build, ARRAY_BEGIN(&comp_data), sizeof(u8) * ARRAY_SIZE(&comp_data));

        ARRAY_DESTROY(u8, &comp_data);
    }
}

void unit_resource_compiler(yaml_node_t root,
                            const char *filename,
                            ARRAY_T(u8) *build,
                            struct compilator_api *compilator_api) {
    struct entity_compile_output *output = unit_compiler_create_output();
    unit_compiler_compile_unit(output, root, filename, compilator_api);
    unit_compiler_write_to_build(output, build);

    unit_compiler_destroy_output(output);
}

int _unit_resource_compiler(const char *filename,
                            struct vio *source_vio,
                            struct vio *build_vio,
                            struct compilator_api *compilator_api) {
    char source_data[vio_size(source_vio) + 1];
    memory_set(source_data, 0, vio_size(source_vio) + 1);
    vio_read(source_vio, source_data, sizeof(char), vio_size(source_vio));

    yaml_document_t h;
    yaml_node_t root = yaml_load_str(source_data, &h);

    ARRAY_T(u8) unit_data;
    ARRAY_INIT(u8, &unit_data, memsys_main_allocator());

    unit_resource_compiler(root, filename, &unit_data, compilator_api);

    vio_write(build_vio, &ARRAY_AT(&unit_data, 0), sizeof(u8), ARRAY_SIZE(&unit_data));


    ARRAY_DESTROY(u8, &unit_data);
    return 1;
}


//==============================================================================
// Resource
//==============================================================================

void *unit_resource_loader(struct vio *input,
                           struct allocator *allocator) {
    const i64 size = vio_size(input);
    char *data = CE_ALLOCATE(allocator, char, size);
    vio_read(input, data, 1, size);

    return data;
}

void unit_resource_unloader(void *new_data,
                            struct allocator *allocator) {
    CE_DEALLOCATE(allocator, new_data);
}

void unit_resource_online(stringid64_t name,
                          void *data) {
}

void unit_resource_offline(stringid64_t name,
                           void *data) {
}

void *unit_resource_reloader(stringid64_t name,
                             void *old_data,
                             void *new_data,
                             struct allocator *allocator) {
    unit_resource_offline(name, old_data);
    unit_resource_online(name, new_data);

    CE_DEALLOCATE(allocator, old_data);

    return new_data;
}

static const resource_callbacks_t unit_resource_callback = {
        .loader = unit_resource_loader,
        .unloader =unit_resource_unloader,
        .online =unit_resource_online,
        .offline =unit_resource_offline,
        .reloader = unit_resource_reloader
};


int unit_init(int stage) {
    if (stage == 0) {
        return 1;
    }


    _G = (struct G) {0};

    _G.type = stringid64_from_string("unit");

    resource_register_type(_G.type, unit_resource_callback);
    resource_compiler_register(_G.type, _unit_resource_compiler);

    return 1;
}

void unit_shutdown() {

    _G = (struct G) {0};
}


void unit_spawn_from_resource(world_t world,
                              void *resource,
                              ARRAY_T(entity_t) *spawned) {

    struct unit_resource *res = resource;

    for (int j = 0; j < res->ent_count; ++j) {
        ARRAY_PUSH_BACK(entity_t, spawned, entity_manager_create());
    }

    u32 *parents = unit_resource_parents(res);
    u64 *comp_types = unit_resource_comp_types(res);

    struct component_data *comp_data = unit_resource_comp_data(res);
    for (int i = 0; i < res->comp_type_count; ++i) {
        stringid64_t type = {.id = comp_types[i]};

        u32 *c_ent = component_data_ent(comp_data);
        char *c_data = component_data_data(comp_data);
        component_spawn(world, type, &ARRAY_AT(spawned, 0), c_ent, parents, comp_data->ent_count, c_data);

        comp_data = (struct component_data *) (c_data + comp_data->size);
    }

}

entity_t unit_spawn(world_t world,
                    stringid64_t name) {
    void *res = resource_get(_G.type, name);

    if (res == NULL) {
        log_error("unit", "Could not spawn unit.");
        return (entity_t) {.idx = 0};
    }

    ARRAY_T(entity_t) spawned;
    ARRAY_INIT(entity_t, &spawned, memsys_main_allocator());

    unit_spawn_from_resource(world, res, &spawned);

    entity_t root = ARRAY_AT(&spawned, 0);

    ARRAY_DESTROY(entity_t, &spawned);

    return root;
}