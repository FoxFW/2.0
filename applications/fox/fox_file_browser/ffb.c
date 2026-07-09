/**
 * FFV — Fox File Viewer
 *
 * Home screen: Browse SD Card / Search SD Card / Favorites (Favorites only
 * shown when non-empty). Up/Down + OK to navigate; Back exits.
 *
 * "Run in app" stops FFV immediately after launching so the target app
 * can own the screen (same behaviour as the Archive app).
 *
 * favorites.txt is shared with the Archive app's Favourites tab.
 */

#include <furi.h>
#include <furi_hal.h>
#include <ctype.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/text_input.h>
#include <gui/modules/dialog_ex.h>
#include <gui/icon_i.h>
#include <storage/storage.h>
#include <loader/loader.h>
#include <dialogs/dialogs.h>
#include "assets_icons.h"
#include "fox_scroll_text.h"

/* ── View IDs ────────────────────────────────────────────────────────────── */
typedef enum {
    ViewStart      = 0,   /* home screen — opened first                     */
    ViewBrowser    = 1,   /* SD card file browser                           */
    ViewFavourites = 2,   /* pinned-files list — also doubles as search results */
    ViewMenu       = 3,   /* file options context menu                      */
    ViewTextInput  = 4,   /* text input — used for both rename and search   */
} FFVViewId;

/* ── Context-menu actions ────────────────────────────────────────────────── */
typedef enum { MenuRun=0, MenuPin=1, MenuRename=2, MenuDelete=3 } FFVMenuAction;

/* ── Home screen menu items (dynamic — Favorites only shown if non-empty) ── */
typedef enum { StartItemBrowse=0, StartItemSearch=1, StartItemFavs=2 } FFVStartItem;

/* ── What's currently populating fav_list: saved favorites or search hits ── */
typedef enum { ListSrcFavorites=0, ListSrcSearch=1 } FFVListSource;

/* ── Which buffer/callback the shared text input view is bound to ────────── */
typedef enum { TextInputRename=0, TextInputSearch=1 } FFVTextInputMode;

/* ── File types ──────────────────────────────────────────────────────────── */
typedef enum {
    TypeFolder, TypeSubGhz, TypeSubGhzRemote, TypeNfc, TypeInfrared,
    TypeLFRFID, TypeBadUsb, TypeIButton, TypeU2F, TypeApp, TypeUpdate,
    TypeScript, TypeSettings, TypeMusic, TypeFile, TypeUnknown,
} FFVFileType;

/* ── Layout constants ────────────────────────────────────────────────────── */
#define ROW_HEADER_H  14
#define ROW_ITEM_H    12
#define ROW_VISIBLE    4
#define MENU_MAX_ITEMS 4
/* Favorites view uses taller 2-line rows to show filename + path hint */
#define FAV_ROW_H        22   /* row height (box_h = 20) */
#define FAV_ROW_VIS       2   /* rows visible at once    */
#define FAV_SCROLL_MS  50   /* timer period for scroll animation (50 ms) */

/* Search */
#define SEARCH_MAX_RESULTS 200
#define SEARCH_NAME_MAX    128

/* ── App context ─────────────────────────────────────────────────────────── */
typedef struct {
    Gui*            gui;
    ViewDispatcher* view_dispatcher;
    View*           start_view;
    FileBrowser*    browser;
    View*           fav_view;
    View*           menu_view;
    TextInput*      text_input;
    FFVTextInputMode text_input_mode;

    FuriString*     selected_path;
    FuriString*     current_dir;
    char            rename_buf[128];
    char            search_query[SEARCH_NAME_MAX];

    Storage*        storage;
    Loader*         loader;
    DialogsApp*     dialogs;
    FFVViewId       current_view;

    /* home screen */
    FFVStartItem    start_items[3];
    uint8_t         start_count;
    uint8_t         start_selected;   /* index into start_items            */
    bool            browser_started;  /* file_browser_start has been called */

    /* context menu */
    char            menu_labels[MENU_MAX_ITEMS][24];
    FFVMenuAction   menu_actions[MENU_MAX_ITEMS];
    uint8_t         menu_count;
    uint8_t         menu_selected;
    bool            menu_from_favs;

    /* favorites / search results (shared list view) */
    FFVListSource   list_source;
    bool            searching;        /* search scan currently running     */
    FuriTimer*      fav_scroll_timer; /* drives path-hint scroll animation */
    FoxScrollText   fav_text_anim;    /* bounce-scroll state for selected row text */
    FuriString**    fav_list;
    size_t          fav_count;
    size_t          fav_selected;
    size_t          fav_scroll;
} FFVApp;

/* Draw callbacks receive the VIEW MODEL (second arg), not the context.
 * Use this static so all custom draw callbacks can access the app.    */
static FFVApp* s_ffv_ctx = NULL;

/* forward declarations */
static void ffv_go_browse(FFVApp* app);
static void ffv_go_favs(FFVApp* app);
static void ffv_go_start(FFVApp* app);
static void ffv_refresh_browser(FFVApp* app);
static void ffv_return_from_menu(FFVApp* app);
static void ffv_build_menu(FFVApp* app);
static void ffv_do_action(FFVApp* app, FFVMenuAction action);
static void ffv_favs_load(FFVApp* app);
static void ffv_favs_free(FFVApp* app);
static void ffv_go_search_input(FFVApp* app);
static void ffv_start_build_items(FFVApp* app);
static bool ffv_favorites_non_empty(FFVApp* app);
static void ffv_search_done(void* ctx);

/* ═══════════════════════════════════════════════════════════════════════════
 *  FILE-TYPE + ICON HELPERS
 * ═════════════════════════════════════════════════════════════════════════ */

static FFVFileType ffv_type_from_path(const char* path) {
    const char* s = strrchr(path, '/');
    const char* n = s ? s+1 : path;
    const char* d = strrchr(n, '.');
    if(!d) return TypeFolder;
    if(!strcmp(d,".sub"))                              return TypeSubGhz;
    if(!strcmp(d,".rem"))                              return TypeSubGhzRemote;
    if(!strcmp(d,".nfc"))                              return TypeNfc;
    if(!strcmp(d,".ir"))                               return TypeInfrared;
    if(!strcmp(d,".rfid")||!strcmp(d,".lfrfid"))       return TypeLFRFID;
    if(!strcmp(d,".bad"))                              return TypeBadUsb;
    if(!strcmp(d,".ibtn")||!strcmp(d,".ibutton"))      return TypeIButton;
    if(!strcmp(d,".u2f"))                              return TypeU2F;
    if(!strcmp(d,".fap"))                              return TypeApp;
    if(!strcmp(d,".fuf")||!strcmp(d,".tgz")||!strcmp(d,".tar")) return TypeUpdate;
    if(!strcmp(d,".jss")||!strcmp(d,".js"))            return TypeScript;
    if(!strcmp(d,".mp3")||!strcmp(d,".wav"))           return TypeMusic;
    if(!strcmp(d,".txt")||!strcmp(d,".csv"))           return TypeFile;
    return TypeUnknown;
}

static const uint8_t* ffv_icon(FFVFileType t) {
    switch(t){
    case TypeFolder:       return I_dir_10px.frames[0];
    case TypeSubGhz:       return I_sub1_10px.frames[0];
    case TypeSubGhzRemote: return I_subrem_10px.frames[0];
    case TypeNfc:          return I_Nfc_10px.frames[0];
    case TypeInfrared:     return I_ir_10px.frames[0];
    case TypeLFRFID:       return I_125_10px.frames[0];
    case TypeBadUsb:       return I_badusb_10px.frames[0];
    case TypeIButton:      return I_ibutt_10px.frames[0];
    case TypeU2F:          return I_u2f_10px.frames[0];
    case TypeApp:          return I_Apps_10px.frames[0];
    case TypeUpdate:       return I_update_10px.frames[0];
    case TypeScript:       return I_js_script_10px.frames[0];
    case TypeSettings:     return I_settings_10px.frames[0];
    case TypeMusic:        return I_music_10px.frames[0];
    case TypeFile:         return I_file_10px.frames[0];
    default:               return I_unknown_10px.frames[0];
    }
}

static const char* ffv_app_for(FFVFileType t) {
    switch(t){
    case TypeSubGhz:       return "Sub-GHz";
    case TypeSubGhzRemote: return "Sub-GHz Remote";
    case TypeNfc:          return "NFC";
    case TypeInfrared:     return "Infrared";
    case TypeLFRFID:       return "125 kHz RFID";
    case TypeBadUsb:       return "Bad USB";
    case TypeIButton:      return "iButton";
    case TypeU2F:          return "U2F";
    case TypeUpdate:       return "UpdaterApp";
    case TypeScript:       return "JS Runner";
    case TypeMusic:        return "Music Player";
    default:               return NULL;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  FAVORITES.TXT
 * ═════════════════════════════════════════════════════════════════════════ */

#define FAV_PATH EXT_PATH("favorites.txt")
#define FAV_TMP  EXT_PATH("favorites.txt.tmp")

static bool ffv_is_pinned(Storage* st, const char* path) {
    File* f = storage_file_alloc(st);
    bool found = false;
    if(storage_file_open(f, FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[256]; size_t i=0; char c;
        while(storage_file_read(f,&c,1)==1){
            if(c=='\r') continue;
            if(c=='\n'){line[i]='\0';if(!strcmp(line,path)){found=true;break;}i=0;}
            else if(i<sizeof(line)-1) line[i++]=c;
        }
        /* Also check the last line if it doesn't end with '\n' — prevents
         * ffv_pin adding a duplicate when the file lacks a trailing newline. */
        if(!found && i > 0) { line[i]='\0'; if(!strcmp(line,path)) found=true; }
        storage_file_close(f);
    }
    storage_file_free(f);
    return found;
}
static void ffv_pin(Storage* st, const char* path) {
    if(ffv_is_pinned(st,path)) return;
    File* f=storage_file_alloc(st);
    if(storage_file_open(f,FAV_PATH,FSAM_WRITE,FSOM_OPEN_APPEND)){
        storage_file_write(f,path,strlen(path));
        storage_file_write(f,"\n",1);
        storage_file_close(f);
    }
    storage_file_free(f);
}
static void ffv_unpin(Storage* st, const char* path) {
    File* fi=storage_file_alloc(st); File* fo=storage_file_alloc(st);
    if(storage_file_open(fi,FAV_PATH,FSAM_READ,FSOM_OPEN_EXISTING)){
        if(storage_file_open(fo,FAV_TMP,FSAM_WRITE,FSOM_CREATE_ALWAYS)){
            char line[256]; size_t i=0; char c;
            while(storage_file_read(fi,&c,1)==1){
                if(c=='\r') continue;
                if(c=='\n'){line[i]='\0';if(strcmp(line,path)!=0){storage_file_write(fo,line,i);storage_file_write(fo,"\n",1);}i=0;}
                else if(i<sizeof(line)-1) line[i++]=c;
            }
            storage_file_close(fo);
        }
        storage_file_close(fi);
        storage_common_remove(st,FAV_PATH);
        storage_common_rename(st,FAV_TMP,FAV_PATH);
    }
    storage_file_free(fi); storage_file_free(fo);
}

static void ffv_favs_free(FFVApp* app){
    for(size_t i=0;i<app->fav_count;i++) furi_string_free(app->fav_list[i]);
    free(app->fav_list); app->fav_list=NULL;
    app->fav_count=app->fav_selected=app->fav_scroll=0;
}
static void ffv_favs_load(FFVApp* app){
    ffv_favs_free(app);
    File* f=storage_file_alloc(app->storage);
    if(!storage_file_open(f,FAV_PATH,FSAM_READ,FSOM_OPEN_EXISTING)){storage_file_free(f);return;}
    char line[256]; size_t li=0; char c; size_t cnt=0;
    while(storage_file_read(f,&c,1)==1){if(c=='\r')continue;if(c=='\n'){if(li>0)cnt++;li=0;}else if(li<255)line[li++]=c;}
    if(li>0) cnt++;
    if(!cnt){storage_file_close(f);storage_file_free(f);return;}
    app->fav_list=malloc(cnt*sizeof(FuriString*)); app->fav_count=0;
    if(storage_file_seek(f,0,true)){
        li=0;
        while(storage_file_read(f,&c,1)==1){
            if(c=='\r')continue;
            if(c=='\n'){if(li>0){line[li]='\0';app->fav_list[app->fav_count++]=furi_string_alloc_set_str(line);li=0;}}
            else if(li<255) line[li++]=c;
        }
        if(li>0){line[li]='\0';app->fav_list[app->fav_count++]=furi_string_alloc_set_str(line);}
    }
    storage_file_close(f); storage_file_free(f);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SHARED ROW DRAWING
 * ═════════════════════════════════════════════════════════════════════════ */

static void ffv_draw_row(Canvas* canvas, int row, const char* label, bool sel){
    int ry=ROW_HEADER_H+row*ROW_ITEM_H, by=ry+1, bh=ROW_ITEM_H-2;
    if(sel){canvas_draw_rbox(canvas,2,by,124,bh,3);canvas_set_color(canvas,ColorWhite);}
    else    canvas_draw_rframe(canvas,2,by,124,bh,3);
    canvas_draw_str_aligned(canvas,64,by+bh/2,AlignCenter,AlignCenter,label);
    canvas_set_color(canvas,ColorBlack);
}
static void ffv_draw_scroll(Canvas* canvas, size_t total, size_t vis, size_t scroll){
    if(total<=vis) return;
    int ah=64-ROW_HEADER_H;
    int bh=(int)(ah*vis/total); if(bh<3)bh=3;
    int by=ROW_HEADER_H+(int)(ah*scroll/total);
    canvas_draw_box(canvas,125,by,3,bh);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HOME SCREEN VIEW
 *  Up/Down select, OK opens the selected item, Back exits.
 * ═════════════════════════════════════════════════════════════════════════ */

static const char* ffv_start_label(FFVStartItem item){
    switch(item){
    case StartItemBrowse: return "Browse SD Card";
    case StartItemSearch: return "Search SD Card";
    case StartItemFavs:   return "Favorites";
    }
    return "";
}

/* Recompute which items are on the home menu (Favorites only if non-empty)
 * and clamp the current selection to the new count.                       */
static void ffv_start_build_items(FFVApp* app){
    app->start_count = 0;
    app->start_items[app->start_count++] = StartItemBrowse;
    app->start_items[app->start_count++] = StartItemSearch;
    if(ffv_favorites_non_empty(app)) app->start_items[app->start_count++] = StartItemFavs;
    if(app->start_selected >= app->start_count) app->start_selected = 0;
}

/* Layout for N home-menu items within Y=13..63 (51 px), small gap between boxes */
static void ffv_start_item_rect(uint8_t idx, uint8_t count, int* y, int* h){
    if(count <= 2){
        static const int ys[2] = {18, 41};
        *h = 18; *y = ys[idx];
    } else {
        static const int ys[3] = {14, 31, 48};
        *h = 14; *y = ys[idx];
    }
}

static void ffv_start_draw_cb(Canvas* canvas, void* model){
    UNUSED(model);
    FFVApp* app=s_ffv_ctx; if(!app) return;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "File Browser");

    const int BX = 3, BW = 122, BR = 4;
    for(uint8_t i = 0; i < app->start_count; i++){
        int y, h;
        ffv_start_item_rect(i, app->start_count, &y, &h);
        bool sel = (i == app->start_selected);
        if(sel){
            canvas_draw_rbox(canvas, BX, y, BW, h, BR);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BX, y, BW, h, BR);
        }
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, y + h/2, AlignCenter, AlignCenter,
                                ffv_start_label(app->start_items[i]));
        canvas_set_color(canvas, ColorBlack);
    }
}
static bool ffv_start_input_cb(InputEvent* ev, void* ctx){
    FFVApp* app=(FFVApp*)ctx;
    if(ev->type!=InputTypeShort && ev->type!=InputTypeRepeat) return false;
    switch(ev->key){
    case InputKeyUp:
        app->start_selected = (app->start_selected==0) ? app->start_count-1 : app->start_selected-1;
        with_view_model(app->start_view,uint8_t* _m,{UNUSED(_m);},true);
        return true;
    case InputKeyDown:
        app->start_selected = (app->start_selected+1)%app->start_count;
        with_view_model(app->start_view,uint8_t* _m,{UNUSED(_m);},true);
        return true;
    case InputKeyOk: case InputKeyRight:
        switch(app->start_items[app->start_selected]){
        case StartItemBrowse: ffv_go_browse(app); break;
        case StartItemSearch: ffv_go_search_input(app); break;
        case StartItemFavs:   ffv_go_favs(app); break;
        }
        return true;
    case InputKeyBack:
        return false;   /* ViewDispatcher stops → app exits */
    default: return false;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  FAVOURITES VIEW
 * ═════════════════════════════════════════════════════════════════════════ */

/* Timer callback: advances the path-hint scroll counter and forces a repaint */
static void ffb_fav_scroll_timer_cb(void* ctx) {
    FFVApp* app = ctx;
    fox_scroll_text_tick(&app->fav_text_anim);
    with_view_model(app->fav_view, uint8_t* _m, { UNUSED(_m); }, true);
}

/* Create blank favorites.txt if it does not exist yet */
static void ffv_ensure_favorites_file(FFVApp* app){
    if(!storage_file_exists(app->storage, FAV_PATH)){
        File* f=storage_file_alloc(app->storage);
        if(storage_file_open(f,FAV_PATH,FSAM_WRITE,FSOM_CREATE_NEW))
            storage_file_close(f);
        storage_file_free(f);
    }
}

/* Start / stop the path-hint scroll animation timer */
static void ffv_fav_scroll_start(FFVApp* app) {
    fox_scroll_text_reset(&app->fav_text_anim);
    furi_timer_start(app->fav_scroll_timer, FAV_SCROLL_MS);
}
static void ffv_fav_scroll_stop(FFVApp* app) {
    furi_timer_stop(app->fav_scroll_timer);
    fox_scroll_text_reset(&app->fav_text_anim);
}

/* Check quickly (no full list load) whether favorites.txt has any entries */
static bool ffv_favorites_non_empty(FFVApp* app){
    File* f=storage_file_alloc(app->storage);
    bool has=false;
    if(storage_file_open(f,FAV_PATH,FSAM_READ,FSOM_OPEN_EXISTING)){
        char c;
        while(storage_file_read(f,&c,1)==1){
            if(c!='\r'&&c!='\n'){has=true;break;}
        }
        storage_file_close(f);
    }
    storage_file_free(f);
    return has;
}

/* Build "/ seg / seg /" directory hint from a full path.
 * Input:  /ext/update/f7-update-local/update.fuf
 * Output: / update / f7-update-local /
 * The /ext/ prefix (or a leading /) is stripped first.              */
static void ffv_build_path_hint(const char* full_path, char* buf, size_t buf_size){
    if(!buf_size) return;
    const char* p=full_path;
    if(strncmp(p,"/ext/",5)==0) p+=5; else if(*p=='/') p++;
    /* End of directory part = last slash position */
    const char* fn=strrchr(full_path,'/');
    size_t dir_len = (fn && fn>=p) ? (size_t)(fn-p) : 0;
    size_t out=0;
    /* Always open with "/ " */
    buf[out++]='/';
    if(out<buf_size-1) buf[out++]=' ';
    for(size_t i=0; i<dir_len && out<buf_size-4; i++){
        if(p[i]=='/'){
            buf[out++]=' '; buf[out++]='/'; buf[out++]=' ';
        } else {
            buf[out++]=p[i];
        }
    }
    /* Close with " /" */
    if(out+2<=buf_size){ buf[out++]=' '; buf[out++]='/'; }
    buf[out<buf_size?out:buf_size-1]='\0';
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SD CARD SEARCH
 * ═════════════════════════════════════════════════════════════════════════ */

static bool ffv_stristr_pos(const char* hay, const char* needle, size_t* pos){
    size_t nlen = strlen(needle);
    if(!nlen) return false;
    for(const char* p = hay; *p; p++){
        size_t i = 0;
        while(i < nlen && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
        if(i == nlen){ *pos = (size_t)(p - hay); return true; }
    }
    return false;
}
static bool ffv_stristr(const char* hay, const char* needle){
    size_t pos;
    return ffv_stristr_pos(hay, needle, &pos);
}

static char* ffv_strdup(const char* s){
    size_t n = strlen(s) + 1;
    char* d = malloc(n);
    if(d) memcpy(d, s, n);
    return d;
}
/* Join dir + name into out, tolerating a trailing slash on dir */
static void ffv_join_path(char* out, size_t out_size, const char* dir, const char* name){
    size_t dlen = strlen(dir);
    if(dlen && dir[dlen-1] == '/') dlen--;
    snprintf(out, out_size, "%.*s/%s", (int)dlen, dir, name);
}

/* Iterative (non-recursive) directory walk — avoids deep call-stack use on
 * the app's small thread stack. Pending directories live on a heap stack. */
static void ffv_do_search(FFVApp* app, const char* needle){
    ffv_favs_free(app);
    if(!needle || !needle[0]) return;

    char** dstack = NULL; size_t dcount = 0, dcap = 0;
    dcap = 8; dstack = malloc(dcap * sizeof(char*));
    dstack[dcount++] = ffv_strdup(EXT_PATH(""));

    size_t list_cap = 0;
    File* dir = storage_file_alloc(app->storage);
    FileInfo info;
    char name[SEARCH_NAME_MAX];

    while(dcount > 0 && app->fav_count < SEARCH_MAX_RESULTS){
        char* cur = dstack[--dcount];
        if(storage_dir_open(dir, cur)){
            while(app->fav_count < SEARCH_MAX_RESULTS &&
                  storage_dir_read(dir, &info, name, sizeof(name))){
                char full[300];
                ffv_join_path(full, sizeof(full), cur, name);
                if(info.flags & FSF_DIRECTORY){
                    if(dcount == dcap){ dcap *= 2; dstack = realloc(dstack, dcap * sizeof(char*)); }
                    dstack[dcount++] = ffv_strdup(full);
                } else if(ffv_stristr(name, needle)){
                    if(app->fav_count == list_cap){
                        list_cap = list_cap ? list_cap * 2 : 16;
                        app->fav_list = realloc(app->fav_list, list_cap * sizeof(FuriString*));
                    }
                    app->fav_list[app->fav_count++] = furi_string_alloc_set_str(full);
                }
            }
        }
        storage_dir_close(dir);
        free(cur);
    }
    storage_file_free(dir);

    while(dcount > 0) free(dstack[--dcount]);
    free(dstack);
}

/* Draw a filename, underlining the portion matching `query`
 * when do_underline is set. Falls back to a plain centred draw otherwise. */
static void ffv_draw_name_row(Canvas* canvas, int cy, const char* name, const char* query, bool do_underline){
    canvas_set_font(canvas, FontPrimary);
    size_t pos;
    if(!do_underline || !ffv_stristr_pos(name, query, &pos)){
        canvas_draw_str_aligned(canvas, 64, cy, AlignCenter, AlignCenter, name);
        return;
    }
    size_t nlen = strlen(name), qlen = strlen(query);
    size_t mid_len = (pos + qlen > nlen) ? nlen - pos : qlen;

    char pre[SEARCH_NAME_MAX], mid[SEARCH_NAME_MAX], post[SEARCH_NAME_MAX];
    size_t pre_len = pos < sizeof(pre)-1 ? pos : sizeof(pre)-1;
    memcpy(pre, name, pre_len); pre[pre_len] = '\0';
    size_t m_len = mid_len < sizeof(mid)-1 ? mid_len : sizeof(mid)-1;
    memcpy(mid, name + pos, m_len); mid[m_len] = '\0';
    strlcpy(post, name + pos + mid_len, sizeof(post));

    int w_total = (int)canvas_string_width(canvas, name);
    int x = 64 - w_total/2;
    canvas_draw_str_aligned(canvas, x, cy, AlignLeft, AlignCenter, pre);
    x += (int)canvas_string_width(canvas, pre);
    int mid_x = x;
    int mid_w = (int)canvas_string_width(canvas, mid);
    canvas_draw_str_aligned(canvas, mid_x, cy, AlignLeft, AlignCenter, mid);
    canvas_draw_line(canvas, mid_x, cy+6, mid_x+mid_w-1, cy+6); /* underline the match */
    x += mid_w;
    canvas_draw_str_aligned(canvas, x, cy, AlignLeft, AlignCenter, post);
}

static void ffv_fav_draw_cb(Canvas* canvas, void* model){
    UNUSED(model);
    FFVApp* app=s_ffv_ctx; if(!app) return;
    canvas_clear(canvas);
    canvas_set_font(canvas,FontPrimary);
    bool is_search = (app->list_source == ListSrcSearch);
    canvas_draw_str_aligned(canvas,64,2,AlignCenter,AlignTop,is_search?"Search Results":"Favorites");
    if(app->fav_count==0){
        canvas_set_font(canvas,FontSecondary);
        const char* msg = app->searching ? "Searching..." : (is_search ? "No matches found" : "");
        canvas_draw_str_aligned(canvas,64,34,AlignCenter,AlignCenter,msg);
        return;
    }
    /* 2-line rows: filename (FontPrimary) + path hint (FontSecondary).
     * FAV_ROW_H = 22, box_h = 20.  Each row from ROW_HEADER_H + row*FAV_ROW_H. */
    for(size_t i=app->fav_scroll;
        i<app->fav_count && (int)(i-app->fav_scroll)<FAV_ROW_VIS; i++){
        const char* full=furi_string_get_cstr(app->fav_list[i]);
        const char* sl=strrchr(full,'/');
        const char* name=sl?sl+1:full;
        int row=(int)(i-app->fav_scroll);
        int ry=ROW_HEADER_H+row*FAV_ROW_H;
        int by=ry+1, bh=FAV_ROW_H-2;  /* bh = 20 */
        bool sel=(i==app->fav_selected);
        if(sel){ canvas_draw_rbox(canvas,2,by,124,bh,3); canvas_set_color(canvas,ColorWhite); }
        else     canvas_draw_rframe(canvas,2,by,124,bh,3);
        /* Line 1: filename, bolded match portion when this is a search list */
        ffv_draw_name_row(canvas, by+5, name, app->search_query, is_search);
        /* Line 2: path hint in FontSecondary, centred in lower half.
         * When the row is selected and the hint is too wide to fit, the text
         * scrolls: PAUSE → slide left → PAUSE → repeat.                    */
        char hint[100];
        ffv_build_path_hint(full,hint,sizeof(hint));
        canvas_set_font(canvas, FontSecondary);
        if(!sel) {
            /* Unselected: show "..." + the end of the path that fits.
             * Cutting from the start keeps the most specific part visible.
             * inner_w = bw(124) - 2×margin(4) = 116 px.
             * Leave 2 px breathing room (1 px each side within text area)
             * so text never touches the border.                          */
            int inner_w = 116;
            int hw = (int)canvas_string_width(canvas, hint);
            if(hw <= inner_w) {
                canvas_draw_str_aligned(canvas, 64, by+15, AlignCenter, AlignCenter, hint);
            } else {
                int dots_w = (int)canvas_string_width(canvas, "...");
                int avail  = inner_w - dots_w - 2; /* 1 px gap each side */
                /* Walk forward from the start until the suffix fits */
                int hlen = (int)strlen(hint);
                int sfx  = 0;
                while(sfx < hlen && (int)canvas_string_width(canvas, hint + sfx) > avail)
                    sfx++;
                char buf[128];
                snprintf(buf, sizeof(buf), "...%s", hint + sfx);
                canvas_draw_str_aligned(canvas, 64, by+15, AlignCenter, AlignCenter, buf);
            }
        } else {
            /* Selected: bounce-scroll via the shared fox_scroll_text helper.
             * Box: x=2, w=124 (right edge x=126).
             * margin=4 gives a 4 px gap between border and text on each side,
             * making the text area x=6..118 (112 px wide).
             * text_y=by+15 centres the hint in the lower half of the 2-line row.
             * The function redraws the border internally — no extra call needed. */
            fox_scroll_text_draw(canvas, 2, by, 124, bh, by+15, 4, true,
                                 hint, &app->fav_text_anim);
        }
        canvas_set_color(canvas,ColorBlack);
    }
    ffv_draw_scroll(canvas,app->fav_count,FAV_ROW_VIS,app->fav_scroll);
}
static bool ffv_fav_input_cb(InputEvent* ev, void* ctx){
    FFVApp* app=(FFVApp*)ctx;
    if(ev->type!=InputTypeShort && ev->type!=InputTypeRepeat) return false;
    if(app->fav_count==0){ return false; }
    switch(ev->key){
    case InputKeyUp:
        fox_scroll_text_reset(&app->fav_text_anim);
        if(app->fav_selected>0){app->fav_selected--;if(app->fav_selected<app->fav_scroll)app->fav_scroll=app->fav_selected;}
        else{app->fav_selected=app->fav_count-1;app->fav_scroll=(app->fav_count>(size_t)FAV_ROW_VIS)?app->fav_count-FAV_ROW_VIS:0;}
        with_view_model(app->fav_view,uint8_t* _m,{UNUSED(_m);},true);
        return true;
    case InputKeyDown:
        fox_scroll_text_reset(&app->fav_text_anim);
        if(app->fav_selected+1<app->fav_count){app->fav_selected++;if(app->fav_selected>=app->fav_scroll+FAV_ROW_VIS)app->fav_scroll=app->fav_selected-FAV_ROW_VIS+1;}
        else{app->fav_selected=0;app->fav_scroll=0;}
        with_view_model(app->fav_view,uint8_t* _m,{UNUSED(_m);},true);
        return true;
    case InputKeyOk: case InputKeyRight:
        ffv_fav_scroll_stop(app);  /* pause animation while in menu */
        furi_string_set(app->selected_path,app->fav_list[app->fav_selected]);
        app->menu_from_favs=true;
        ffv_build_menu(app);
        app->current_view=ViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher,ViewMenu);
        return true;
    case InputKeyBack: case InputKeyLeft:
        return false;   /* nav callback → back to start screen */
    default: return false;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  CONTEXT MENU VIEW
 * ═════════════════════════════════════════════════════════════════════════ */

static void ffv_menu_draw_cb(Canvas* canvas, void* model){
    UNUSED(model);
    FFVApp* app=s_ffv_ctx; if(!app) return;
    canvas_clear(canvas);
    canvas_set_font(canvas,FontPrimary);
    canvas_draw_str_aligned(canvas,64,2,AlignCenter,AlignTop,"File Options");
    canvas_set_font(canvas,FontSecondary);
    for(uint8_t i=0;i<app->menu_count;i++)
        ffv_draw_row(canvas,i,app->menu_labels[i],i==app->menu_selected);
}
static bool ffv_menu_input_cb(InputEvent* ev, void* ctx){
    FFVApp* app=(FFVApp*)ctx;
    if(ev->type!=InputTypeShort && ev->type!=InputTypeRepeat) return false;
    switch(ev->key){
    case InputKeyUp:
        app->menu_selected=(app->menu_selected==0)?app->menu_count-1:app->menu_selected-1;
        with_view_model(app->menu_view,uint8_t* _m,{UNUSED(_m);},true);
        return true;
    case InputKeyDown:
        app->menu_selected=(app->menu_selected+1)%app->menu_count;
        with_view_model(app->menu_view,uint8_t* _m,{UNUSED(_m);},true);
        return true;
    case InputKeyOk: case InputKeyRight:
        ffv_do_action(app,app->menu_actions[app->menu_selected]);
        return true;
    case InputKeyBack: case InputKeyLeft:
        return false;   /* nav → return to browse or favs */
    default: return false;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  NAV CALLBACK  (Back key not consumed by current view)
 * ═════════════════════════════════════════════════════════════════════════ */

static bool ffv_nav_callback(void* ctx){
    FFVApp* app=(FFVApp*)ctx;
    switch(app->current_view){
    case ViewMenu:
        ffv_return_from_menu(app);
        return true;
    case ViewTextInput:
        if(app->text_input_mode == TextInputSearch) ffv_go_start(app);
        else                                          ffv_return_from_menu(app);
        return true;
    case ViewBrowser:
        /* Home screen is always reachable now */
        ffv_go_start(app);
        return true;
    case ViewFavourites:
        /* Back in Favourites → home; back from Search results → search input */
        ffv_fav_scroll_stop(app);
        if(app->list_source == ListSrcSearch) ffv_go_search_input(app);
        else                                   ffv_go_start(app);
        return true;
    case ViewStart:
        /* Back at home → exit */
        return false;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  NAVIGATION HELPERS
 * ═════════════════════════════════════════════════════════════════════════ */

static void ffv_go_start(FFVApp* app){
    ffv_start_build_items(app);
    app->current_view=ViewStart;
    view_dispatcher_switch_to_view(app->view_dispatcher,ViewStart);
}
static void ffv_go_browse(FFVApp* app){
    file_browser_start(app->browser, app->current_dir);
    app->browser_started = true;
    app->current_view=ViewBrowser;
    view_dispatcher_switch_to_view(app->view_dispatcher,ViewBrowser);
}
static void ffv_go_favs(FFVApp* app){
    ffv_favs_load(app);
    app->list_source = ListSrcFavorites;
    if(app->fav_count==0){
        /* Nothing to show — go to Browse instead */
        ffv_favs_free(app);
        ffv_go_browse(app);
        return;
    }
    app->fav_selected=0; app->fav_scroll=0;
    ffv_fav_scroll_start(app);
    app->current_view=ViewFavourites;
    view_dispatcher_switch_to_view(app->view_dispatcher,ViewFavourites);
}
static void ffv_go_search_input(FFVApp* app){
    text_input_reset(app->text_input);
    text_input_set_result_callback(app->text_input, ffv_search_done, app,
                                   app->search_query, sizeof(app->search_query), true);
    text_input_set_header_text(app->text_input, "Search SD Card:");
    app->text_input_mode = TextInputSearch;
    app->current_view = ViewTextInput;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewTextInput);
}

static void ffv_refresh_browser(FFVApp* app){
    /* Reopen browser in the parent directory of the last selected file */
    const char* sel=furi_string_get_cstr(app->selected_path);
    const char* sl=strrchr(sel,'/');
    if(sl && sl>sel){ furi_string_set_str(app->current_dir,sel); furi_string_left(app->current_dir,(size_t)(sl-sel)); }
    else furi_string_set_str(app->current_dir,EXT_PATH(""));
    file_browser_stop(app->browser);
    file_browser_start(app->browser,app->current_dir);
    app->current_view=ViewBrowser;
    view_dispatcher_switch_to_view(app->view_dispatcher,ViewBrowser);
}
static void ffv_return_from_menu(FFVApp* app){
    if(app->menu_from_favs && app->list_source == ListSrcSearch){
        /* Re-run the last search so the list reflects rename/delete/etc. */
        ffv_do_search(app, app->search_query);
        if(app->fav_selected>=app->fav_count) app->fav_selected = app->fav_count ? app->fav_count-1 : 0;
        if(app->fav_scroll>app->fav_selected) app->fav_scroll = app->fav_selected;
        ffv_fav_scroll_start(app);
        app->current_view=ViewFavourites;
        view_dispatcher_switch_to_view(app->view_dispatcher,ViewFavourites);
    } else if(app->menu_from_favs){
        ffv_favs_load(app);
        if(app->fav_count==0){
            /* Last favourite removed: skip favs+home, go straight to Browse */
            ffv_favs_free(app);
            ffv_go_browse(app);
        } else {
            if(app->fav_selected>=app->fav_count) app->fav_selected=app->fav_count-1;
            ffv_fav_scroll_start(app);
            app->current_view=ViewFavourites;
            view_dispatcher_switch_to_view(app->view_dispatcher,ViewFavourites);
        }
    } else {
        ffv_refresh_browser(app);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BROWSER callbacks
 * ═════════════════════════════════════════════════════════════════════════ */

static bool ffv_item_cb(FuriString* path, void* ctx, uint8_t** icon, FuriString* name){
    UNUSED(ctx); UNUSED(name);
    *icon=(uint8_t*)ffv_icon(ffv_type_from_path(furi_string_get_cstr(path)));
    return true;
}

static void ffv_build_menu(FFVApp* app){
    const char* path=furi_string_get_cstr(app->selected_path);
    FFVFileType t=ffv_type_from_path(path);
    app->menu_count=app->menu_selected=0;
    bool run=(t!=TypeUnknown&&t!=TypeFile&&t!=TypeFolder);
    if(run){
        /* Firmware update files get "Install" instead of "Run in app" */
        strlcpy(app->menu_labels[app->menu_count],
                (t==TypeUpdate) ? "Install" : "Run in app", 24);
        app->menu_actions[app->menu_count++]=MenuRun;
    }
    bool pin=ffv_is_pinned(app->storage,path);
    strlcpy(app->menu_labels[app->menu_count],pin?"Remove Favorite":"Add to Favorites",24);app->menu_actions[app->menu_count++]=MenuPin;
    strlcpy(app->menu_labels[app->menu_count],"Rename",24);app->menu_actions[app->menu_count++]=MenuRename;
    strlcpy(app->menu_labels[app->menu_count],"Delete",24);app->menu_actions[app->menu_count++]=MenuDelete;
}

static void ffv_browser_callback(void* ctx){
    FFVApp* app=ctx;
    app->menu_from_favs=false;
    ffv_build_menu(app);
    app->current_view=ViewMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher,ViewMenu);
}

/* ── Rename ─────────────────────────────────────────────────────────────── */
static void ffv_rename_done(void* ctx){
    FFVApp* app=ctx;
    const char* old=furi_string_get_cstr(app->selected_path);
    if(!strlen(app->rename_buf)){ffv_return_from_menu(app);return;}
    FuriString* np=furi_string_alloc_set(app->selected_path);
    size_t sl=furi_string_search_rchar(np,'/');
    if(sl!=FURI_STRING_FAILURE){furi_string_left(np,sl+1);furi_string_cat_str(np,app->rename_buf);}
    if(ffv_is_pinned(app->storage,old)){ffv_unpin(app->storage,old);ffv_pin(app->storage,furi_string_get_cstr(np));}
    storage_common_rename(app->storage,old,furi_string_get_cstr(np));
    furi_string_free(np);
    ffv_return_from_menu(app);
}

/* ── Search ─────────────────────────────────────────────────────────────── */
static void ffv_search_done(void* ctx){
    FFVApp* app=ctx;
    if(!strlen(app->search_query)){ ffv_go_start(app); return; }

    ffv_favs_free(app);
    app->list_source = ListSrcSearch;
    app->fav_selected = app->fav_scroll = 0;
    app->searching = true;
    app->current_view = ViewFavourites;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewFavourites);

    ffv_do_search(app, app->search_query);

    app->searching = false;
    ffv_fav_scroll_start(app);
    with_view_model(app->fav_view, uint8_t* _m, { UNUSED(_m); }, true);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ACTION HANDLER
 * ═════════════════════════════════════════════════════════════════════════ */

static void ffv_do_action(FFVApp* app, FFVMenuAction action){
    const char* path=furi_string_get_cstr(app->selected_path);
    switch(action){

    case MenuRun: {
        /* loader_start_with_gui_error and loader_start_detached_with_gui_error
         * both call loader_do_start_by_name which checks loader_do_is_locked().
         * While ANY app thread is alive, the Loader is locked and both functions
         * silently fail with LoaderStatusErrorAppStarted (no dialog shown).
         *
         * The correct API for a FAP to launch another app is loader_enqueue_launch:
         * it pushes to loader->launch_queue WITHOUT a lock check.  When FFV's
         * thread exits, loader_do_app_closed() → loader_do_next_deferred_launch_if_available()
         * pops the queue and starts the app.  loader_enqueue_launch uses strdup()
         * internally so strings are safely copied before FFV exits.            */
        FFVFileType t = ffv_type_from_path(path);
        const char* app_name = NULL;
        const char* app_args = NULL;
        char fap_path_buf[512];

        if(t == TypeUpdate) {
            app_name = "updater_app";  /* appid from application.fam */
            app_args = path;           /* full .fuf file path — updater reads it directly */
        } else if(t == TypeApp) {
            app_name = path;           /* FAP: path is the executable */
            app_args = NULL;
        } else {
            app_name = ffv_app_for(t);
            app_args = path;
        }

        if(app_name) {
            if(t == TypeApp) {
                strlcpy(fap_path_buf, path, sizeof(fap_path_buf));
                app_name = fap_path_buf;
            }
            loader_enqueue_launch(
                app->loader,
                app_name,
                app_args,
                LoaderDeferredLaunchFlagGui);  /* show error dialog if launch fails */
        }
        view_dispatcher_stop(app->view_dispatcher);
        break;
    }

    case MenuPin: {
        bool was = ffv_is_pinned(app->storage, path);
        if(was) ffv_unpin(app->storage, path);
        else    ffv_pin(app->storage, path);

        if(app->menu_from_favs && app->list_source == ListSrcFavorites) {
            /* Opened from Favourites → "Remove Favorite" was clicked.
             * Close the menu and return to the list; the next item
             * is selected automatically (ffv_return_from_menu clamps). */
            ffv_return_from_menu(app);
        } else {
            /* Opened from Browse → flip the label in-place and stay in
             * the File Options menu so the user sees the change.       */
            for(uint8_t i = 0; i < app->menu_count; i++) {
                if(app->menu_actions[i] == MenuPin) {
                    strlcpy(app->menu_labels[i],
                            was ? "Add to Favorites" : "Remove Favorite", 24);
                    break;
                }
            }
            with_view_model(app->menu_view, uint8_t* _m, { UNUSED(_m); }, true);
        }
        break;
    }

    case MenuRename: {
        const char* sl=strrchr(path,'/');
        strlcpy(app->rename_buf,sl?sl+1:path,sizeof(app->rename_buf));
        text_input_reset(app->text_input);
        text_input_set_result_callback(app->text_input, ffv_rename_done, app,
                                       app->rename_buf, sizeof(app->rename_buf), true);
        text_input_set_header_text(app->text_input,"Rename:");
        app->text_input_mode=TextInputRename;
        app->current_view=ViewTextInput;
        view_dispatcher_switch_to_view(app->view_dispatcher,ViewTextInput);
        break;
    }

    case MenuDelete: {
        DialogMessage* m=dialog_message_alloc();
        dialog_message_set_header(m,"Delete file?",64,4,AlignCenter,AlignTop);
        const char* sl=strrchr(path,'/');
        dialog_message_set_text(m,sl?sl+1:path,64,32,AlignCenter,AlignCenter);
        dialog_message_set_buttons(m,"Cancel",NULL,"Delete");
        DialogMessageButton btn=dialog_message_show(app->dialogs,m);
        dialog_message_free(m);
        if(btn==DialogMessageButtonRight){
            if(ffv_is_pinned(app->storage,path)) ffv_unpin(app->storage,path);
            storage_common_remove(app->storage,path);
        }
        ffv_return_from_menu(app);
        break;
    }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ALLOC / FREE / ENTRY POINT
 * ═════════════════════════════════════════════════════════════════════════ */

static FFVApp* ffv_alloc(void){
    FFVApp* app=malloc(sizeof(FFVApp)); furi_assert(app);
    memset(app,0,sizeof(FFVApp));
    app->storage =furi_record_open(RECORD_STORAGE);
    app->loader  =furi_record_open(RECORD_LOADER);
    app->dialogs =furi_record_open(RECORD_DIALOGS);
    app->gui     =furi_record_open(RECORD_GUI);
    app->current_view=ViewStart;
    s_ffv_ctx=app;
    app->selected_path=furi_string_alloc();
    app->current_dir  =furi_string_alloc();
    furi_string_set_str(app->current_dir,EXT_PATH(""));

    app->view_dispatcher=view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher,app->gui,ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher,app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher,ffv_nav_callback);

    /* Home screen */
    app->start_view=view_alloc();
    view_set_draw_callback(app->start_view,ffv_start_draw_cb);
    view_set_input_callback(app->start_view,ffv_start_input_cb);
    view_set_context(app->start_view,app);
    view_allocate_model(app->start_view,ViewModelTypeLocking,sizeof(uint8_t));
    view_dispatcher_add_view(app->view_dispatcher,ViewStart,app->start_view);

    /* File browser */
    app->browser=file_browser_alloc(app->selected_path);
    file_browser_configure(app->browser,"*",EXT_PATH(""),false,true,&I_unknown_10px,false);
    file_browser_set_item_callback(app->browser,ffv_item_cb,app);
    file_browser_set_callback(app->browser,ffv_browser_callback,app);
    view_dispatcher_add_view(app->view_dispatcher,ViewBrowser,file_browser_get_view(app->browser));

    /* Favorites */
    app->fav_view=view_alloc();
    view_set_draw_callback(app->fav_view,ffv_fav_draw_cb);
    view_set_input_callback(app->fav_view,ffv_fav_input_cb);
    view_set_context(app->fav_view,app);
    view_allocate_model(app->fav_view,ViewModelTypeLocking,sizeof(uint8_t));
    app->fav_scroll_timer=furi_timer_alloc(ffb_fav_scroll_timer_cb,FuriTimerTypePeriodic,app);
    view_dispatcher_add_view(app->view_dispatcher,ViewFavourites,app->fav_view);

    /* Context menu */
    app->menu_view=view_alloc();
    view_set_draw_callback(app->menu_view,ffv_menu_draw_cb);
    view_set_input_callback(app->menu_view,ffv_menu_input_cb);
    view_set_context(app->menu_view,app);
    view_allocate_model(app->menu_view,ViewModelTypeLocking,sizeof(uint8_t));
    view_dispatcher_add_view(app->view_dispatcher,ViewMenu,app->menu_view);

    /* Rename */
    app->text_input=text_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher,ViewTextInput,text_input_get_view(app->text_input));

    return app;
}

static void ffv_free(FFVApp* app){
    s_ffv_ctx=NULL;
    ffv_favs_free(app);
    /* Only stop the browser if it was actually started — calling
     * file_browser_stop on an unstarted browser trips a furi_check. */
    if(app->browser_started) file_browser_stop(app->browser);
    view_dispatcher_remove_view(app->view_dispatcher,ViewStart);
    view_dispatcher_remove_view(app->view_dispatcher,ViewBrowser);
    view_dispatcher_remove_view(app->view_dispatcher,ViewFavourites);
    view_dispatcher_remove_view(app->view_dispatcher,ViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher,ViewTextInput);
    view_free(app->start_view);
    file_browser_free(app->browser);
    furi_timer_stop(app->fav_scroll_timer);
    furi_timer_free(app->fav_scroll_timer);
    view_free(app->fav_view);
    view_free(app->menu_view);
    text_input_free(app->text_input);
    view_dispatcher_free(app->view_dispatcher);
    furi_string_free(app->selected_path);
    furi_string_free(app->current_dir);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_LOADER);
    furi_record_close(RECORD_DIALOGS);
    free(app);
}

int32_t ffb_app(void* p){
    UNUSED(p);
    FFVApp* app=ffv_alloc();

    /* Create a blank favorites.txt if it does not yet exist */
    ffv_ensure_favorites_file(app);

    /* Home screen is always shown now (Favorites entry hidden when empty) */
    ffv_go_start(app);
    view_dispatcher_run(app->view_dispatcher);
    /* loader_enqueue_launch() was called inside the event loop before
     * view_dispatcher_stop().  When this thread exits (return 0 below),
     * the Loader's loader_do_app_closed() fires and pops the launch queue,
     * starting the queued app cleanly with no lock conflict.              */
    ffv_free(app);
    return 0;
}
