#include "github_repo.h"
#include "download.h"
#include "json_mini.h"

#include <storage/storage.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define FOX_GITHUB_REPO_INFO_PATH FOX_DOWNLOAD_DATA_DIR "/github_repo_info.json"
#define FOX_GITHUB_TREE_PATH      FOX_DOWNLOAD_DATA_DIR "/github_tree.json"

void github_repo_open(App* app) {
    app_show_text_input(app, "owner/repo", TextInputPurposeGithubRepo);
}

static const char* skip_ws(const char* p) {
    while(*p == ' ' || *p == '\t') p++;
    return p;
}

void github_repo_submitted(App* app) {
    const char* input = skip_ws(app->text_input_buffer);
    const char* slash = strchr(input, '/');
    if(!slash || slash == input || slash[1] == '\0') {
        app_log(app, "Enter as owner/repo, e.g. jblanked/FlipDownloader");
        app_render_log(app);
        return;
    }

    size_t owner_len = (size_t)(slash - input);
    if(owner_len >= sizeof(app->github_owner)) owner_len = sizeof(app->github_owner) - 1;
    memcpy(app->github_owner, input, owner_len);
    app->github_owner[owner_len] = '\0';

    snprintf(app->github_repo, sizeof(app->github_repo), "%s", slash + 1);
    for(size_t i = 0; app->github_repo[i]; i++) {
        if(app->github_repo[i] == '/' || app->github_repo[i] == ' ' ||
           app->github_repo[i] == '\n' || app->github_repo[i] == '\r') {
            app->github_repo[i] = '\0';
            break;
        }
    }
    if(app->github_owner[0] == '\0' || app->github_repo[0] == '\0') {
        app_log(app, "Enter as owner/repo, e.g. jblanked/FlipDownloader");
        app_render_log(app);
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, FOX_DOWNLOAD_DATA_DIR);
    furi_record_close(RECORD_STORAGE);

    snprintf(
        app->download_url,
        sizeof(app->download_url),
        "https://api.github.com/repos/%s/%s",
        app->github_owner,
        app->github_repo);
    snprintf(app->download_path, sizeof(app->download_path), "%s", FOX_GITHUB_REPO_INFO_PATH);
    snprintf(app->download_found_name, sizeof(app->download_found_name), "%s", app->github_repo);
    app->download_purpose = DownloadPurposeGithubRepoInfo;

    app_log(app, "Fetching %s/%s...", app->github_owner, app->github_repo);
    app_start_download(app);
}

void github_repo_info_loaded(App* app) {
    char buf[512];
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    size_t len = 0;
    if(storage_file_open(file, FOX_GITHUB_REPO_INFO_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        len = storage_file_read(file, buf, sizeof(buf) - 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    char branch[64];
    bool found = false;
    if(len > 0) {
        buf[len] = '\0';
        found = json_mini_get_string(buf, "default_branch", branch, sizeof(branch));
    }
    if(!found) {
        app_log(app, "Repo not found: %s/%s", app->github_owner, app->github_repo);
        app_render_log(app);
        return;
    }

    snprintf(
        app->download_url,
        sizeof(app->download_url),
        "https://api.github.com/repos/%.30s/%.50s/git/trees/%.40s?recursive=1",
        app->github_owner,
        app->github_repo,
        branch);
    snprintf(app->download_path, sizeof(app->download_path), "%s", FOX_GITHUB_TREE_PATH);
    app->download_purpose = DownloadPurposeGithubTree;
    app_start_download(app);
}

static bool path_ends_with_fap(const char* path) {
    size_t len = strlen(path);
    if(len < 4) return false;
    const char* tail = path + (len - 4);
    return (tail[0] == '.' && tolower((unsigned char)tail[1]) == 'f' &&
            tolower((unsigned char)tail[2]) == 'a' && tolower((unsigned char)tail[3]) == 'p');
}

void github_free_buffers(App* app) {
    if(app->github_files) {
        free(app->github_files);
        app->github_files = NULL;
    }
}

void github_show_file_list(App* app) {
    if(!app->github_files) app->github_files = malloc(FOX_GITHUB_FILE_MAX * sizeof(GithubFileEntry));
    if(!app->github_files) {
        app_log(app, "Out of memory.");
        app_render_log(app);
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool opened = storage_file_open(file, FOX_GITHUB_TREE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(!opened) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        app_log(app, "No response - check the owner/repo and your connection.");
        app_render_log(app);
        return;
    }

    JsonFileReader reader;
    json_file_reader_init(&reader, file);

    if(!json_file_skip_to(&reader, "\"tree\":[")) {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        app_log(app, "Unexpected response from GitHub.");
        app_render_log(app);
        return;
    }

    app->github_file_count = 0;
    char obj_buf[400];

    while(app->github_file_count < FOX_GITHUB_FILE_MAX &&
          json_file_reader_next_object(&reader, obj_buf, sizeof(obj_buf))) {
        char path[FOX_GITHUB_PATH_MAX];
        if(!json_mini_get_string(obj_buf, "path", path, sizeof(path))) continue;
        if(!path_ends_with_fap(path)) continue;

        snprintf(
            app->github_files[app->github_file_count].path,
            sizeof(app->github_files[app->github_file_count].path),
            "%s",
            path);
        app->github_file_count++;
    }

    app->github_tree_truncated = json_file_skip_to(&reader, "\"truncated\":true");

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(app->github_file_count == 0) {
        app_log(app, "No .fap files found in %s/%s.", app->github_owner, app->github_repo);
        app_render_log(app);
        return;
    }

    app->github_file_selected = 0;
    app->github_file_scroll = 0;
    app->current_view = FoxDownloaderViewGithubFileList;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewGithubFileList);
    with_view_model(app->github_file_list_view, uint8_t * m, { UNUSED(m); }, true);
}

static const char* path_basename(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void github_install_selected(App* app) {
    if(app->github_file_count == 0) return;
    const char* path = app->github_files[app->github_file_selected].path;
    char base[64];
    snprintf(base, sizeof(base), "%.63s", path_basename(path));

    snprintf(
        app->download_url,
        sizeof(app->download_url),
        "https://raw.githubusercontent.com/%.39s/%.59s/HEAD/%.159s",
        app->github_owner,
        app->github_repo,
        path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    char dir[64];
    snprintf(dir, sizeof(dir), "%s/GitHub", FOX_APPS_DIR);
    storage_simply_mkdir(storage, FOX_APPS_DIR);
    storage_simply_mkdir(storage, dir);
    furi_record_close(RECORD_STORAGE);

    snprintf(app->download_path, sizeof(app->download_path), "%s/%s", dir, base);
    snprintf(app->download_found_name, sizeof(app->download_found_name), "%s", base);
    snprintf(
        app->download_found_type,
        sizeof(app->download_found_type),
        "%.20s/%.20s",
        app->github_owner,
        app->github_repo);
    app->download_found_size = 0;
    app->download_found_type_suspicious = false;
    app->download_purpose = DownloadPurposeGithubFile;
    app->download_return_view = FoxDownloaderViewGithubFileList;
    app->download_found_focus_left = false;
    app->current_view = FoxDownloaderViewDownloadFound;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewDownloadFound);
}

static App* s_github_list_app = NULL;

#define GITHUB_ROW_H   11
#define GITHUB_VISIBLE 4

static void github_list_draw(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_github_list_app;
    if(!app) return;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    char header[56];
    snprintf(
        header,
        sizeof(header),
        "%.20s/%.16s%s",
        app->github_owner,
        app->github_repo,
        app->github_tree_truncated ? " (partial)" : "");
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignCenter, header);
    canvas_set_color(canvas, ColorBlack);

    if(app->github_file_count == 0) return;

    if(app->github_file_selected < app->github_file_scroll) {
        app->github_file_scroll = app->github_file_selected;
    } else if(app->github_file_selected >= app->github_file_scroll + GITHUB_VISIBLE) {
        app->github_file_scroll = app->github_file_selected - GITHUB_VISIBLE + 1;
    }

    for(size_t row = 0; row < GITHUB_VISIBLE; row++) {
        size_t idx = app->github_file_scroll + row;
        if(idx >= app->github_file_count) break;
        int32_t y = 11 + (int32_t)row * GITHUB_ROW_H;
        bool selected = (idx == app->github_file_selected);
        if(selected) {
            canvas_draw_box(canvas, 0, y, 128, GITHUB_ROW_H);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 3, y + GITHUB_ROW_H - 2, app->github_files[idx].path);
        if(selected) canvas_set_color(canvas, ColorBlack);
    }
}

static bool github_list_input(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(app->github_file_count == 0) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->github_file_selected > 0) app->github_file_selected--;
        with_view_model(app->github_file_list_view, uint8_t * m, { UNUSED(m); }, true);
        return true;
    case InputKeyDown:
        if(app->github_file_selected + 1 < app->github_file_count) app->github_file_selected++;
        with_view_model(app->github_file_list_view, uint8_t * m, { UNUSED(m); }, true);
        return true;
    case InputKeyOk:
        if(event->type == InputTypeShort) github_install_selected(app);
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* github_file_list_view_alloc(App* app) {
    s_github_list_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, github_list_draw);
    view_set_input_callback(v, github_list_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    return v;
}

void github_file_list_view_free(View* v) {
    s_github_list_app = NULL;
    view_free(v);
}
