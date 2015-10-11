#pragma once

#include "celib/string/stringid_types.h"
#include "celib/memory/memory_types.h"
#include "cetech/filesystem/file.h"

namespace cetech {
    namespace resource_lua {
        struct Resource {
            uint32_t type;
	    uint32_t size;
        };

        StringId64_t type_hash();

        void compiler(const char* filename, FSFile* in, FSFile* out);
        void* loader(FSFile* f, Allocator& a);
        void unloader(Allocator& a, void* data);

        const char* get_source(const Resource* rs);
    }
}