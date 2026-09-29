#include "fox_vfat_plat.h"
#include <storage/storage.h>
#include <stdlib.h>

#define FVP_CHUNK 32768u

struct FoxVfatFile {
    Storage* storage;
    File* f;
};

static FoxVfatFile* fvp_wrap(Storage* storage, File* f) {
    FoxVfatFile* h = malloc(sizeof(FoxVfatFile));
    h->storage = storage;
    h->f = f;
    return h;
}

FoxVfatFile* fvp_open_read(void* storage, const char* path) {
    Storage* s = storage;
    File* f = storage_file_alloc(s);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return NULL;
    }
    return fvp_wrap(s, f);
}

FoxVfatFile* fvp_open_rw(void* storage, const char* path) {
    Storage* s = storage;
    File* f = storage_file_alloc(s);
    if(!storage_file_open(f, path, FSAM_READ | FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return NULL;
    }
    return fvp_wrap(s, f);
}

uint32_t fvp_read(FoxVfatFile* h, void* buf, uint32_t len) {
    uint8_t* p = buf;
    uint32_t done = 0;
    while(done < len) {
        uint32_t want = len - done;
        if(want > FVP_CHUNK) want = FVP_CHUNK;
        uint16_t got = storage_file_read(h->f, p + done, (uint16_t)want);
        done += got;
        if(got < want) break;
    }
    return done;
}

uint32_t fvp_write(FoxVfatFile* h, const void* buf, uint32_t len) {
    const uint8_t* p = buf;
    uint32_t done = 0;
    while(done < len) {
        uint32_t want = len - done;
        if(want > FVP_CHUNK) want = FVP_CHUNK;
        uint16_t put = storage_file_write(h->f, p + done, (uint16_t)want);
        done += put;
        if(put < want) break;
    }
    return done;
}

bool fvp_seek(FoxVfatFile* h, uint64_t off) {
    if(off > 0xFFFFFFFFull) return false;
    return storage_file_seek(h->f, (uint32_t)off, true);
}

uint64_t fvp_size(FoxVfatFile* h) {
    return storage_file_size(h->f);
}

void fvp_close(FoxVfatFile* h) {
    if(!h) return;
    storage_file_close(h->f);
    storage_file_free(h->f);
    free(h);
}
