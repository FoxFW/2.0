#pragma once

#include <flipper_application/elf/elf_api_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CompositeApiResolver CompositeApiResolver;

CompositeApiResolver* composite_api_resolver_alloc(void);

void composite_api_resolver_free(CompositeApiResolver* resolver);

void composite_api_resolver_add(CompositeApiResolver* resolver, const ElfApiInterface* interface);

const ElfApiInterface* composite_api_resolver_get(CompositeApiResolver* resolver);

#ifdef __cplusplus
}
#endif
