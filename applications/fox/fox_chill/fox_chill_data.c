#include "app.h"

#include <furi_hal.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define FOX_CHILL_DATA_DIR    "/ext/apps_data/fox_chill"
#define FOX_CHILL_JOKES_PATH  FOX_CHILL_DATA_DIR "/jokes.txt"
#define FOX_CHILL_RIDDLES_PATH FOX_CHILL_DATA_DIR "/riddles.txt"
#define FOX_CHILL_FACTS_PATH  FOX_CHILL_DATA_DIR "/fun_facts.txt"
#define FOX_CHILL_STATISTICS_PATH FOX_CHILL_DATA_DIR "/statistics.txt"
#define FOX_CHILL_YO_MAMA_PATH FOX_CHILL_DATA_DIR "/yo_mama.txt"

#define FOX_CHILL_READ_CHUNK 64

static uint32_t s_rng_state;

static void rng_seed_if_needed(void) {
    if(s_rng_state != 0) return;
    uint32_t seed = 0;
    furi_hal_random_fill_buf((uint8_t*)&seed, sizeof(seed));
    s_rng_state = seed | 1;
}

static uint32_t rng_next(void) {
    rng_seed_if_needed();
    uint32_t x = s_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng_state = x;
    return x;
}

static bool pick_random_line(const char* path, char* out, size_t out_cap) {
    if(out_cap == 0) return false;
    out[0] = '\0';

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool found = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line_buf[FOX_CHILL_LINE_MAX];
        size_t line_len = 0;
        uint32_t line_index = 0;
        bool line_overflowed = false;

        uint8_t chunk[FOX_CHILL_READ_CHUNK];
        size_t got;
        do {
            got = storage_file_read(file, chunk, sizeof(chunk));
            for(size_t i = 0; i < got; i++) {
                char c = (char)chunk[i];
                if(c == '\r') continue;
                if(c == '\n') {
                    if(line_len > 0) {
                        line_buf[line_len] = '\0';
                        line_index++;
                        if(rng_next() % line_index == 0) {
                            size_t n = line_len < out_cap - 1 ? line_len : out_cap - 1;
                            memcpy(out, line_buf, n);
                            out[n] = '\0';
                            found = true;
                        }
                    }
                    line_len = 0;
                    line_overflowed = false;
                    continue;
                }
                if(line_overflowed) continue;
                if(line_len >= sizeof(line_buf) - 1) {
                    line_overflowed = true;
                    continue;
                }
                line_buf[line_len++] = c;
            }
        } while(got == sizeof(chunk));

        if(line_len > 0) {
            line_buf[line_len] = '\0';
            line_index++;
            if(rng_next() % line_index == 0) {
                size_t n = line_len < out_cap - 1 ? line_len : out_cap - 1;
                memcpy(out, line_buf, n);
                out[n] = '\0';
                found = true;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return found;
}

static void split_riddle(const char* raw, char* question, size_t qcap, char* answer, size_t acap) {
    question[0] = '\0';
    answer[0] = '\0';

    const char* open = strchr(raw, '{');
    if(open == NULL) {
        strncpy(question, raw, qcap - 1);
        question[qcap - 1] = '\0';
        return;
    }

    size_t qlen = (size_t)(open - raw);
    while(qlen > 0 && raw[qlen - 1] == ' ') qlen--;
    size_t qn = qlen < qcap - 1 ? qlen : qcap - 1;
    memcpy(question, raw, qn);
    question[qn] = '\0';

    const char* close = strrchr(open, '}');
    const char* astart = open + 1;
    size_t alen = (close != NULL && close > astart) ? (size_t)(close - astart) :
                                                       strlen(astart);
    size_t an = alen < acap - 1 ? alen : acap - 1;
    memcpy(answer, astart, an);
    answer[an] = '\0';
}

bool fox_chill_pick_random(App* app, ContentKind kind) {
    const char* path;
    switch(kind) {
    case ContentKindJoke:
        path = FOX_CHILL_JOKES_PATH;
        break;
    case ContentKindRiddle:
        path = FOX_CHILL_RIDDLES_PATH;
        break;
    case ContentKindFact:
        path = FOX_CHILL_FACTS_PATH;
        break;
    case ContentKindStatistic:
        path = FOX_CHILL_STATISTICS_PATH;
        break;
    case ContentKindYoMama:
    default:
        path = FOX_CHILL_YO_MAMA_PATH;
        break;
    }

    char raw[FOX_CHILL_LINE_MAX];
    if(!pick_random_line(path, raw, sizeof(raw))) {
        app->content_kind = kind;
        snprintf(
            app->content_question,
            sizeof(app->content_question),
            "Couldn't find the content file on the SD card.");
        app->content_answer[0] = '\0';
        app->content_has_answer = false;
        app->content_answer_shown = false;
        app->content_scroll = 0;
        return false;
    }

    app->content_kind = kind;
    app->content_answer_shown = false;
    app->content_scroll = 0;

    if(kind == ContentKindRiddle) {
        split_riddle(
            raw,
            app->content_question,
            sizeof(app->content_question),
            app->content_answer,
            sizeof(app->content_answer));
        app->content_has_answer = app->content_answer[0] != '\0';
    } else {
        strncpy(app->content_question, raw, sizeof(app->content_question) - 1);
        app->content_question[sizeof(app->content_question) - 1] = '\0';
        app->content_answer[0] = '\0';
        app->content_has_answer = false;
    }

    return true;
}
