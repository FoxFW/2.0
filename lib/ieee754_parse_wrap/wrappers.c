#include "wrappers.h"

float __wrap_strtof(const char* in, char** tail) {
    return strtof_l(in, tail, NULL);
}

double __wrap_strtod(const char* in, char** tail) {
    return strtod_l(in, tail, NULL);
}
