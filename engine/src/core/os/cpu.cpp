#include <cetech/core/api_system.h>
#include <cetech/core/os/cpu.h>
#include <cetech/core/log.h>

#include <include/SDL2/SDL_cpuinfo.h>
#include <celib/macros.h>
#include <cetech/core/module.h>

int cpu_count() {
    return SDL_GetCPUCount();
}

static ct_cpu_a0 cpu_api = {
        .count = cpu_count
};

CETECH_MODULE_DEF(
        cpu,
        {
            api->register_api("ct_cpu_a0", &cpu_api);
        },
        {
            CEL_UNUSED(api);
        }
)
