#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <m-core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FURI_STRING_FAILURE ((size_t) - 1)

typedef struct FuriString FuriString;

FuriString* furi_string_alloc(void);

FuriString* furi_string_alloc_set(const FuriString* source);

FuriString* furi_string_alloc_set_str(const char cstr_source[]);

FuriString* furi_string_alloc_printf(const char format[], ...)
    _ATTRIBUTE((__format__(__printf__, 1, 2)));

FuriString* furi_string_alloc_vprintf(const char format[], va_list args);

FuriString* furi_string_alloc_move(FuriString* source);

void furi_string_free(FuriString* string);

void furi_string_reserve(FuriString* string, size_t size);

void furi_string_reset(FuriString* string);

void furi_string_swap(FuriString* string_1, FuriString* string_2);

void furi_string_move(FuriString* destination, FuriString* source);

size_t furi_string_hash(const FuriString* string);

size_t furi_string_size(const FuriString* string);

bool furi_string_empty(const FuriString* string);

char furi_string_get_char(const FuriString* string, size_t index);

const char* furi_string_get_cstr(const FuriString* string);

void furi_string_set(FuriString* string, FuriString* source);

void furi_string_set_str(FuriString* string, const char source[]);

void furi_string_set_strn(FuriString* string, const char source[], size_t length);

void furi_string_set_char(FuriString* string, size_t index, const char c);

void furi_string_set_n(FuriString* string, const FuriString* source, size_t offset, size_t length);

int furi_string_printf(FuriString* string, const char format[], ...)
    _ATTRIBUTE((__format__(__printf__, 2, 3)));

int furi_string_vprintf(FuriString* string, const char format[], va_list args);

void furi_string_push_back(FuriString* string, char c);

void furi_string_cat(FuriString* string_1, const FuriString* string_2);

void furi_string_cat_str(FuriString* string_1, const char cstring_2[]);

int furi_string_cat_printf(FuriString* string, const char format[], ...)
    _ATTRIBUTE((__format__(__printf__, 2, 3)));

int furi_string_cat_vprintf(FuriString* string, const char format[], va_list args);

int furi_string_cmp(const FuriString* string_1, const FuriString* string_2);

int furi_string_cmp_str(const FuriString* string_1, const char cstring_2[]);

int furi_string_cmpi(const FuriString* string_1, const FuriString* string_2);

int furi_string_cmpi_str(const FuriString* string_1, const char cstring_2[]);

size_t furi_string_search(const FuriString* string, const FuriString* needle, size_t start);

size_t furi_string_search_str(const FuriString* string, const char needle[], size_t start);

size_t furi_string_search_char(const FuriString* string, char c, size_t start);

size_t furi_string_search_rchar(const FuriString* string, char c, size_t start);

bool furi_string_equal(const FuriString* string_1, const FuriString* string_2);

bool furi_string_equal_str(const FuriString* string_1, const char cstring_2[]);

void furi_string_replace_at(FuriString* string, size_t pos, size_t len, const char replace[]);

size_t
    furi_string_replace(FuriString* string, FuriString* needle, FuriString* replace, size_t start);

size_t furi_string_replace_str(
    FuriString* string,
    const char needle[],
    const char replace[],
    size_t start);

void furi_string_replace_all(
    FuriString* string,
    const FuriString* needle,
    const FuriString* replace);

void furi_string_replace_all_str(FuriString* string, const char needle[], const char replace[]);

bool furi_string_start_with(const FuriString* string, const FuriString* start);

bool furi_string_start_with_str(const FuriString* string, const char start[]);

bool furi_string_end_with(const FuriString* string, const FuriString* end);

bool furi_string_end_withi(const FuriString* string, const FuriString* end);

bool furi_string_end_with_str(const FuriString* string, const char end[]);

bool furi_string_end_withi_str(const FuriString* string, const char end[]);

void furi_string_left(FuriString* string, size_t index);

void furi_string_right(FuriString* string, size_t index);

void furi_string_mid(FuriString* string, size_t index, size_t size);

void furi_string_trim(FuriString* string, const char chars[]);

typedef unsigned int FuriStringUnicodeValue;

size_t furi_string_utf8_length(FuriString* string);

void furi_string_utf8_push(FuriString* string, FuriStringUnicodeValue unicode);

typedef enum {
    FuriStringUTF8StateStarting,
    FuriStringUTF8StateDecoding1,
    FuriStringUTF8StateDecoding2,
    FuriStringUTF8StateDecoding3,
    FuriStringUTF8StateError
} FuriStringUTF8State;

void furi_string_utf8_decode(char c, FuriStringUTF8State* state, FuriStringUnicodeValue* unicode);

#define FURI_STRING_SELECT1(func1, func2, a)                                                       \
    _Generic((a), char*: func2, const char*: func2, FuriString*: func1, const FuriString*: func1)( \
        a)

#define FURI_STRING_SELECT2(func1, func2, a, b)                                                    \
    _Generic((b), char*: func2, const char*: func2, FuriString*: func1, const FuriString*: func1)( \
        a, b)

#define FURI_STRING_SELECT3(func1, func2, a, b, c)                                                 \
    _Generic((b), char*: func2, const char*: func2, FuriString*: func1, const FuriString*: func1)( \
        a, b, c)

#define FURI_STRING_SELECT4(func1, func2, a, b, c, d)                                              \
    _Generic((b), char*: func2, const char*: func2, FuriString*: func1, const FuriString*: func1)( \
        a, b, c, d)

#define furi_string_alloc_set(a) \
    FURI_STRING_SELECT1(furi_string_alloc_set, furi_string_alloc_set_str, a)

#define furi_string_set(a, b) FURI_STRING_SELECT2(furi_string_set, furi_string_set_str, a, b)

#define furi_string_cmp(a, b) FURI_STRING_SELECT2(furi_string_cmp, furi_string_cmp_str, a, b)

#define furi_string_cmpi(a, b) FURI_STRING_SELECT2(furi_string_cmpi, furi_string_cmpi_str, a, b)

#define furi_string_equal(a, b) FURI_STRING_SELECT2(furi_string_equal, furi_string_equal_str, a, b)

#define furi_string_replace_all(a, b, c) \
    FURI_STRING_SELECT3(furi_string_replace_all, furi_string_replace_all_str, a, b, c)

#define furi_string_search(...) \
    M_APPLY(                    \
        FURI_STRING_SELECT3,    \
        furi_string_search,     \
        furi_string_search_str, \
        M_DEFAULT_ARGS(3, (0), __VA_ARGS__))

#define furi_string_search_str(...) furi_string_search_str(M_DEFAULT_ARGS(3, (0), __VA_ARGS__))

#define furi_string_start_with(a, b) \
    FURI_STRING_SELECT2(furi_string_start_with, furi_string_start_with_str, a, b)

#define furi_string_end_with(a, b) \
    FURI_STRING_SELECT2(furi_string_end_with, furi_string_end_with_str, a, b)

#define furi_string_end_withi(a, b) \
    FURI_STRING_SELECT2(furi_string_end_withi, furi_string_end_withi_str, a, b)

#define furi_string_cat(a, b) FURI_STRING_SELECT2(furi_string_cat, furi_string_cat_str, a, b)

#define furi_string_trim(...) furi_string_trim(M_DEFAULT_ARGS(2, ("  \n\r\t"), __VA_ARGS__))

#define furi_string_search_char(...) furi_string_search_char(M_DEFAULT_ARGS(3, (0), __VA_ARGS__))

#define furi_string_search_rchar(...) furi_string_search_rchar(M_DEFAULT_ARGS(3, (0), __VA_ARGS__))

#define furi_string_replace(...) \
    M_APPLY(                     \
        FURI_STRING_SELECT4,     \
        furi_string_replace,     \
        furi_string_replace_str, \
        M_DEFAULT_ARGS(4, (0), __VA_ARGS__))

#define furi_string_replace_str(...) furi_string_replace_str(M_DEFAULT_ARGS(4, (0), __VA_ARGS__))

#define F_STR_INIT(a) ((a) = furi_string_alloc())

#define F_STR_INIT_SET(a, b) ((a) = furi_string_alloc_set(b))

#define F_STR_INIT_MOVE(a, b) ((a) = furi_string_alloc_move(b))

#define FURI_STRING_OPLIST       \
    (INIT(F_STR_INIT),           \
     INIT_SET(F_STR_INIT_SET),   \
     SET(furi_string_set),       \
     INIT_MOVE(F_STR_INIT_MOVE), \
     MOVE(furi_string_move),     \
     SWAP(furi_string_swap),     \
     RESET(furi_string_reset),   \
     EMPTY_P(furi_string_empty), \
     CLEAR(furi_string_free),    \
     HASH(furi_string_hash),     \
     EQUAL(furi_string_equal),   \
     CMP(furi_string_cmp),       \
     TYPE(FuriString*))

#ifdef __cplusplus
}
#endif
