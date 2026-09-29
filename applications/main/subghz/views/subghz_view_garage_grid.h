#pragma once
#include <gui/view.h>

#define GGRID_IDX_READ      0
#define GGRID_IDX_SAVED     1
#define GGRID_IDX_READRAW   2
#define GGRID_IDX_FREQANA   3
#define GGRID_IDX_MODANA    4
#define GGRID_IDX_PROTOCOLS 5
#define GGRID_BTN_COUNT     6

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SubGhzGarageGrid SubGhzGarageGrid;

typedef void (*SubGhzGarageGridCallback)(void* context, uint32_t event);

SubGhzGarageGrid* subghz_garage_grid_alloc(void);
void subghz_garage_grid_free(SubGhzGarageGrid* instance);
View* subghz_garage_grid_get_view(SubGhzGarageGrid* instance);
void subghz_garage_grid_set_callback(
    SubGhzGarageGrid* instance,
    SubGhzGarageGridCallback callback,
    void* context);
void subghz_garage_grid_set_visible(SubGhzGarageGrid* instance, uint8_t btn_idx, bool visible);
void subghz_garage_grid_set_selected(SubGhzGarageGrid* instance, uint8_t btn_idx);

#ifdef __cplusplus
}
#endif
