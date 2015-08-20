#include "common/memory/memory.h"
#include "common/math/math_types.h"
#include "common/math/math.h"
#include "common/math/quat.h"
#include "common/math/vector2.h"
#include "common/math/matrix33.h"
#include "common/math/simd/simd.h"
#include "common/types.h"
#include "common/container/array.h"
#include "common/container/hash.h"
#include "common/ecs/entitymanager.h"
#include "runtime/runtime.h"

#include <iostream>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace cetech1;
using namespace rapidjson;

void frame_start() {
    runtime::frame_start();
}

void frame_end() {
    runtime::frame_end();
}

void run() {
    while (1) {
        frame_start();
        frame_end();
    }
}

void init() {
    memory_globals::init();
    runtime::init();

    Window w = runtime::window::make_window(
        "aaa",
        runtime::window::WINDOWPOS_CENTERED, runtime::window::WINDOWPOS_CENTERED,
        800, 600,
        runtime::window::WINDOW_NOFLAG
        );
}

void shutdown() {
    runtime::shutdown();
    memory_globals::shutdown();
}

int main(int argc, char** argv) {
    init();
    run();
    shutdown();

    return 0; // TODO: error check
}
