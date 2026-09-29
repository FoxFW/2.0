#ifndef _PRINTF_H_
#define _PRINTF_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void _putchar(char character);

int printf_(const char* format, ...) _ATTRIBUTE((__format__(__printf__, 1, 2)));

int sprintf_(char* buffer, const char* format, ...) _ATTRIBUTE((__format__(__printf__, 2, 3)));

int snprintf_(char* buffer, size_t count, const char* format, ...)
    _ATTRIBUTE((__format__(__printf__, 3, 4)));
int vsnprintf_(char* buffer, size_t count, const char* format, va_list va);

int vprintf_(const char* format, va_list va);

int fctprintf(void (*out)(char character, void* arg), void* arg, const char* format, ...)
    _ATTRIBUTE((__format__(__printf__, 3, 4)));

#ifdef __cplusplus
}
#endif

#endif
