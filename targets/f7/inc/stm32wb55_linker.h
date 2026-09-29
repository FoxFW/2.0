#pragma once

#ifdef __cplusplus
extern "C" {
typedef const char linker_symbol_t;
#else
typedef const void linker_symbol_t;
#endif

extern linker_symbol_t _stack_end;
extern linker_symbol_t _stack_size;

extern linker_symbol_t _sidata;
extern linker_symbol_t _sdata;
extern linker_symbol_t _edata;

extern linker_symbol_t _sbss;
extern linker_symbol_t _ebss;

extern linker_symbol_t _sMB_MEM2;
extern linker_symbol_t _eMB_MEM2;

extern linker_symbol_t __heap_start__;
extern linker_symbol_t __heap_end__;

extern linker_symbol_t __free_flash_start__;

#ifdef __cplusplus
}
#endif
