#pragma once

#include "app.h"

/* Scripts menu: a dynamic list of saved scripts (from SCRIPTLIST) plus
   a trailing "New Script" item. Selecting an existing script drills
   into a small ScriptActions menu (Run / Show / Delete) rather than
   running it immediately - Delete needs to not be one accidental OK
   press away. "New Script" is a two-step text entry (name, then
   source) - see app.h's TextInputPurpose - ending in a SCRIPTSAVE.

   Real on-device typing for a whole FoxScript one-liner is genuinely
   slow on Flipper's keyboard; text_input_buffer is sized well under
   SCRIPT_SOURCE_MAX's 2048 for that reason (see app.h) - scripts
   longer than that are still fully supported by the firmware, they
   just need to arrive via Terminal's SCRIPTSAVE: line instead of this
   screen. Documented in README.md rather than silently capped with no
   explanation. */

void scripts_render_menu(App* app);
void scripts_menu_select(App* app, uint32_t index);

void scripts_actions_render_menu(App* app);
void scripts_actions_select(App* app, uint32_t index);

/* Called from main.c's text_input_result_callback() for the two
   TextInputPurposeScript* stages. */
void scripts_name_submitted(App* app);
void scripts_source_submitted(App* app);
