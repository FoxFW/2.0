#pragma once

#include <gui/view.h>
#include <gui/icon.h>
#include <furi.h>

/* Fox splash - a themeable boot splash for Fox apps: shows a centered
   icon for hold_ms, then dissolves it to blank over fade_ms, then
   calls done_cb exactly once.

   "Dissolve", not a true fade - Flipper's Canvas is 1-bit monochrome
   with no alpha blending, so there's no real fade to do. This instead
   erases the icon in randomly-ordered small blocks over fade_ms, which
   reads as a fade/dissolve on a real screen and is the practical
   equivalent on this hardware. Pass fade_ms = 0 to skip it entirely and
   cut straight from hold to done - the explicit fallback mentioned when
   this was requested, fully supported, not just a "todo".

   Meant to be dropped into any Fox app with minimal wiring - see
   fox_esp32_commander's main.c (fox_splash_done_cb, app_alloc) for the
   reference integration. To reuse in another Fox app:
     1. Copy fox_splash.c/.h into the new app's source folder verbatim.
     2. Add your own 64x64 1-bit PNG under that app's images/ folder
        (e.g. images/fox_64x64.png).
     3. Add fap_icon_assets="images" to that app's application.fam.
     4. #include "<that app's appid>_icons.h" (fbt's generated header -
        named after the app's own appid, e.g. fox_chameleon_icons.h for
        fox_chameleon, NOT a generic "icons.h") to get the Icon symbol
        (e.g. I_fox_64x64) to pass into fox_splash_alloc().
   That's the whole integration - no changes to fox_splash.c/.h itself
   needed per app. */

typedef struct FoxSplash FoxSplash;

typedef void (*FoxSplashDoneCallback)(void* context);

FoxSplash* fox_splash_alloc(
    const Icon* icon,
    uint32_t hold_ms,
    uint32_t fade_ms,
    FoxSplashDoneCallback done_cb,
    void* done_context);
void fox_splash_free(FoxSplash* splash);

View* fox_splash_get_view(FoxSplash* splash);

/* (Re)starts the hold/dissolve sequence from the beginning. Call this
   right after switching the ViewDispatcher to this view - allocation
   alone doesn't start the timer, so the splash doesn't start ticking
   while some other view is still on screen. */
void fox_splash_start(FoxSplash* splash);
