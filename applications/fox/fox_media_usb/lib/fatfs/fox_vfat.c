#include "fox_vfat.h"
#include "fox_vfat_diskio.h"
#include "ff.h"
#include <stdlib.h>
#include <string.h>

#define SS FOX_VFAT_SECTOR

typedef struct {
    uint32_t sector;
    uint32_t block;
} MetaIdx;

typedef struct {
    uint32_t first_lba;
    uint32_t sectors;
    uint32_t size;
    uint32_t src_off;
} FoxVfatMap;

struct FoxVfat {
    void* storage;
    char meta_path[256];

    FoxVfatFile* meta;
    MetaIdx* idx;
    uint32_t idx_n, idx_cap;
    uint32_t meta_blocks;

    FoxVfatMap* map;
    uint32_t map_n, map_cap;

    char* pool;
    uint32_t pool_len, pool_cap;

    uint32_t vol_sectors;
    uint32_t csize;
    uint32_t database;
    bool geom_ok;

    FATFS* fs;
    bool mounted;

    FoxVfatFile* src_cur;
    uint32_t src_cur_off;
};

static int idx_find(FoxVfat* v, uint32_t sector, uint32_t* pos) {
    uint32_t lo = 0, hi = v->idx_n;
    while(lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if(v->idx[mid].sector < sector)
            lo = mid + 1;
        else
            hi = mid;
    }
    *pos = lo;
    return (lo < v->idx_n && v->idx[lo].sector == sector);
}

static bool store_put(FoxVfat* v, uint32_t sector, const uint8_t* buf) {
    uint32_t pos;
    if(idx_find(v, sector, &pos)) {
        if(!fvp_seek(v->meta, (uint64_t)v->idx[pos].block * SS)) return false;
        return fvp_write(v->meta, buf, SS) == SS;
    }

    if(!fvp_seek(v->meta, (uint64_t)v->meta_blocks * SS)) return false;
    if(fvp_write(v->meta, buf, SS) != SS) return false;
    if(v->idx_n == v->idx_cap) {
        uint32_t nc = v->idx_cap ? v->idx_cap * 2 : 256;
        MetaIdx* ni = realloc(v->idx, nc * sizeof(MetaIdx));
        if(!ni) return false;
        v->idx = ni;
        v->idx_cap = nc;
    }
    memmove(&v->idx[pos + 1], &v->idx[pos], (v->idx_n - pos) * sizeof(MetaIdx));
    v->idx[pos].sector = sector;
    v->idx[pos].block = v->meta_blocks;
    v->idx_n++;
    v->meta_blocks++;
    return true;
}

static bool store_get(FoxVfat* v, uint32_t sector, uint8_t* buf) {
    uint32_t pos;
    if(!idx_find(v, sector, &pos)) return false;
    if(!fvp_seek(v->meta, (uint64_t)v->idx[pos].block * SS)) return false;
    return fvp_read(v->meta, buf, SS) == SS;
}

static FoxVfat* g_active = NULL;
void fox_vfat_diskio_bind(FoxVfat* v) {
    g_active = v;
}
void fox_vfat_diskio_unbind(void) {
    g_active = NULL;
}
int fox_vfat_diskio_ready(void) {
    return g_active != NULL;
}
uint32_t fox_vfat_diskio_sector_count(void) {
    return g_active ? g_active->vol_sectors : 0;
}
int fox_vfat_diskio_read(uint32_t sector, uint8_t* buf, uint32_t count) {
    if(!g_active) return 0;
    for(uint32_t i = 0; i < count; i++) {
        if(!store_get(g_active, sector + i, buf + i * SS)) memset(buf + i * SS, 0, SS);
    }
    return 1;
}
int fox_vfat_diskio_write(uint32_t sector, const uint8_t* buf, uint32_t count) {
    if(!g_active) return 0;
    for(uint32_t i = 0; i < count; i++) {
        if(!store_put(g_active, sector + i, buf + i * SS)) return 0;
    }
    return 1;
}

FoxVfat* fox_vfat_alloc(void* storage, const char* meta_path) {
    FoxVfat* v = calloc(1, sizeof(FoxVfat));
    if(!v) return NULL;
    v->storage = storage;
    strncpy(v->meta_path, meta_path, sizeof(v->meta_path) - 1);
    v->src_cur_off = UINT32_MAX;
    return v;
}

void fox_vfat_free(FoxVfat* v) {
    if(!v) return;
    if(v->mounted) f_mount(NULL, "", 0);
    fox_vfat_diskio_unbind();
    if(v->meta) fvp_close(v->meta);
    if(v->src_cur) fvp_close(v->src_cur);
    free(v->idx);
    free(v->map);
    free(v->pool);
    free(v->fs);
    free(v);
}

static uint32_t pool_add(FoxVfat* v, const char* s) {
    uint32_t need = (uint32_t)strlen(s) + 1;
    if(v->pool_len + need > v->pool_cap) {
        uint32_t nc = v->pool_cap ? v->pool_cap * 2 : 1024;
        while(nc < v->pool_len + need) nc *= 2;
        char* np = realloc(v->pool, nc);
        if(!np) return UINT32_MAX;
        v->pool = np;
        v->pool_cap = nc;
    }
    uint32_t off = v->pool_len;
    memcpy(v->pool + off, s, need);
    v->pool_len += need;
    return off;
}

bool fox_vfat_build_begin(FoxVfat* v, uint64_t volume_bytes, const char* label) {
    v->vol_sectors = (uint32_t)(volume_bytes / SS);
    v->meta = fvp_open_rw(v->storage, v->meta_path);
    if(!v->meta) return false;
    v->fs = calloc(1, sizeof(FATFS));
    if(!v->fs) return false;

    fox_vfat_diskio_bind(v);

    void* work = malloc(4096);
    if(!work) return false;
    bool ok = false;
    do {
        if(f_mount(v->fs, "", 0) != FR_OK) break;
        v->mounted = true;
        BYTE opt = (volume_bytes >= 512ull * 1024 * 1024) ? FM_FAT32 : FM_FAT;
        if(f_mkfs("", opt, 0, work, 4096) != FR_OK) break;
        if(label && label[0]) f_setlabel(label);
        ok = true;
    } while(0);
    free(work);
    return ok;
}

static void capture_geom(FoxVfat* v) {
    if(!v->geom_ok && v->fs->database != 0) {
        v->csize = v->fs->csize;
        v->database = v->fs->database;
        v->geom_ok = true;
    }
}

bool fox_vfat_build_mkdir(FoxVfat* v, const char* dir) {
    FRESULT fr = f_mkdir(dir);
    capture_geom(v);
    return (fr == FR_OK || fr == FR_EXIST);
}

bool fox_vfat_build_add(FoxVfat* v, const char* rel_path, const char* src_path, uint32_t size) {
    FIL fil;
    if(f_open(&fil, rel_path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    capture_geom(v);

    bool ok = true;
    if(size > 0) {
        if(f_expand(&fil, size, 1) != FR_OK) {
            ok = false;
        } else if(v->geom_ok) {
            uint32_t sclust = (uint32_t)fil.obj.sclust;
            uint32_t first_lba = v->database + (sclust - 2) * v->csize;
            uint32_t sectors = (size + SS - 1) / SS;
            uint32_t src_off = pool_add(v, src_path);
            if(src_off == UINT32_MAX) {
                ok = false;
            } else {
                if(v->map_n == v->map_cap) {
                    uint32_t nc = v->map_cap ? v->map_cap * 2 : 64;
                    FoxVfatMap* nm = realloc(v->map, nc * sizeof(FoxVfatMap));
                    if(!nm)
                        ok = false;
                    else {
                        v->map = nm;
                        v->map_cap = nc;
                    }
                }
                if(ok) {
                    v->map[v->map_n].first_lba = first_lba;
                    v->map[v->map_n].sectors = sectors;
                    v->map[v->map_n].size = size;
                    v->map[v->map_n].src_off = src_off;
                    v->map_n++;
                }
            }
        } else {
            ok = false;
        }
    }
    f_close(&fil);
    return ok;
}

bool fox_vfat_build_end(FoxVfat* v) {
    bool ok = true;
    if(v->mounted) {
        if(f_mount(NULL, "", 0) != FR_OK) ok = false;
        v->mounted = false;
    }
    fox_vfat_diskio_unbind();

    for(uint32_t a = 1; a < v->map_n; a++) {
        FoxVfatMap key = v->map[a];
        uint32_t b = a;
        while(b > 0 && v->map[b - 1].first_lba > key.first_lba) {
            v->map[b] = v->map[b - 1];
            b--;
        }
        v->map[b] = key;
    }
    return ok;
}

uint32_t fox_vfat_total_sectors(FoxVfat* v) {
    return v->vol_sectors;
}

static FoxVfatMap* map_find(FoxVfat* v, uint32_t sector) {
    uint32_t lo = 0, hi = v->map_n;
    while(lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        FoxVfatMap* m = &v->map[mid];
        if(sector < m->first_lba)
            hi = mid;
        else if(sector >= m->first_lba + m->sectors)
            lo = mid + 1;
        else
            return m;
    }
    return NULL;
}

static bool src_open_for(FoxVfat* v, uint32_t src_off) {
    if(v->src_cur && v->src_cur_off == src_off) return true;
    if(v->src_cur) {
        fvp_close(v->src_cur);
        v->src_cur = NULL;
        v->src_cur_off = UINT32_MAX;
    }
    v->src_cur = fvp_open_read(v->storage, v->pool + src_off);
    if(!v->src_cur) return false;
    v->src_cur_off = src_off;
    return true;
}

bool fox_vfat_read(FoxVfat* v, uint32_t lba, uint32_t count, uint8_t* out, uint32_t out_cap, uint32_t* out_len) {
    uint32_t n = count;
    if(n * SS > out_cap) n = out_cap / SS;
    uint32_t produced = 0;
    uint32_t i = 0;
    while(i < n) {
        uint32_t s = lba + i;
        FoxVfatMap* m = map_find(v, s);
        if(m) {

            uint32_t run = 1;
            while(i + run < n && (s + run) < (m->first_lba + m->sectors))
                run++;
            uint32_t rel_sector = s - m->first_lba;
            uint64_t file_off = (uint64_t)rel_sector * SS;
            uint32_t want = run * SS;

            uint32_t avail = 0;
            if(file_off < m->size) avail = (uint32_t)(m->size - file_off);
            if(avail > want) avail = want;
            uint8_t* dst = out + i * SS;
            memset(dst, 0, want);
            if(avail > 0) {
                if(!src_open_for(v, m->src_off)) return false;
                if(!fvp_seek(v->src_cur, file_off)) return false;
                uint32_t got = 0;
                while(got < avail) {
                    uint32_t chunk = fvp_read(v->src_cur, dst + got, avail - got);
                    if(chunk == 0) break;
                    got += chunk;
                }

            }
            i += run;
            produced += want;
        } else {
            uint8_t* dst = out + i * SS;
            if(!store_get(v, s, dst)) memset(dst, 0, SS);
            i++;
            produced += SS;
        }
    }
    if(out_len) *out_len = produced;
    return true;
}
