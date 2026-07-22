#pragma once

#include "app.h"

/* General HTTP client - GET (one text entry: the URL) and POST (two:
   URL then body, chained the same way the Scripts New Script flow
   chains name then source). Kept deliberately scoped to GET/POST only,
   no PUT/DELETE/custom headers - Terminal already reaches every
   [BRACKET] command by hand for anything more exotic than this. */

void http_render_menu(App* app);
void http_menu_select(App* app, uint32_t index);

/* Called from main.c's text_input_result_callback() for the three
   TextInputPurposeHttp* purposes. */
void http_get_url_submitted(App* app);
void http_post_url_submitted(App* app);
void http_post_body_submitted(App* app);
