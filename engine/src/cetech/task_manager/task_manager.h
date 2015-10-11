#pragma once

#include <cstdint>
#include <string.h>

#include "celib/memory/memory_types.h"

#define NULL_TASK {0}

namespace cetech {
    class TaskManager {
        public:
            typedef void (* TaskWorkFce_t)(void*);

            struct TaskID {
                uint32_t i;
            };

            virtual ~TaskManager() {};

            virtual TaskID add_begin(const TaskWorkFce_t fce,
                                     void* data,
                                     const uint32_t priority,
                                     const TaskID depend = NULL_TASK,
                                     const TaskID parent = NULL_TASK) = 0;

            virtual TaskID add_empty_begin(const uint32_t priority,
                                           const TaskID depend = NULL_TASK,
                                           const TaskID parent = NULL_TASK) = 0;

            virtual void add_end(const TaskID* tasks, const uint32_t count) = 0;

            virtual void wait(const TaskID id) = 0;

            static TaskManager* make(Allocator& allocator);
            static void destroy(Allocator& allocator, TaskManager* tm);
    };
}