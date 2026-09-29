#pragma once

#include <core/thread.h>

void run_parallel(FuriThreadCallback callback, void* context, uint32_t stack_size);
