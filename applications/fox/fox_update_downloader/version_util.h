#pragma once

#include <stdbool.h>

int version_compare_dotted(const char* a, const char* b);

bool version_commit_equal(const char* a, const char* b);
