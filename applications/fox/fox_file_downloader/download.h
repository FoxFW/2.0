#pragma once

#include "app.h"

void http_download_url_submitted(App* app);
void url_derive_filename(const char* url, char* out, size_t out_size);
void download_derive_found_info(App* app, uint32_t size, const char* type);
void download_unconfirmed_finished(App* app, bool ok, const char* error);
void download_pending_cancel(App* app);
void download_install_confirm(App* app);

void file_downloader_open(App* app);

void file_downloader_check_queue_or_keyboard(App* app);

void download_found_confirm(App* app);
void download_found_cancel(App* app);

View* download_found_view_alloc(App* app);
void download_found_view_free(View* v);

void download_resume_show(App* app, const char* url, const char* path);
void download_resume_start(App* app);
void download_resume_delete(App* app);

View* download_resume_view_alloc(App* app);
void download_resume_view_free(View* v);
