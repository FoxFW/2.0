#include "version_util.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static void parse_components(const char* s, long* out, int max) {
    for(int i = 0; i < max; i++) out[i] = 0;
    if(s == NULL) return;

    const char* p = s;
    while(*p && !isdigit((unsigned char)*p)) p++;

    int idx = 0;
    while(*p && idx < max) {
        char* end = NULL;
        long v = strtol(p, &end, 10);
        if(end == p) break;
        out[idx++] = v;
        p = end;
        if(*p == '.') {
            p++;
        } else {
            break;
        }
    }
}

int version_compare_dotted(const char* a, const char* b) {
    long ca[4];
    long cb[4];
    parse_components(a, ca, 4);
    parse_components(b, cb, 4);

    for(int i = 0; i < 4; i++) {
        if(ca[i] != cb[i]) return (ca[i] > cb[i]) ? 1 : -1;
    }
    return 0;
}

bool version_commit_equal(const char* a, const char* b) {
    if(a == NULL || b == NULL) return false;
    size_t la = strlen(a);
    size_t lb = strlen(b);
    size_t n = la < lb ? la : lb;
    if(n == 0) return false;

    for(size_t i = 0; i < n; i++) {
        if(tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    }
    return true;
}
