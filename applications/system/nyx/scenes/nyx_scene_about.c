#include "../nyx_i.h"

void nyx_scene_about_on_enter(void* context) {
    NyxApp* app = context;

    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "\e#Nyx " NYX_VERSION "\n"
        "by at0m-b0mb\n"
        "\n"
        "A covert night-vision camera has to\n"
        "light the room to see in it. It does\n"
        "that with 850/940 nm infrared your\n"
        "eyes cannot register. Nyx turns that\n"
        "giveaway into a meter.\n"
        "\n"
        "\e#Modes\n"
        "\e*Onboard\e* uses the built-in\n"
        "TSOP-75338. That part band-passes\n"
        "38 kHz, so it only sees PULSED IR:\n"
        "remotes, beacons, PIR floodlights,\n"
        "and some PWM-driven illuminators.\n"
        "It is deaf to a steady DC\n"
        "illuminator no matter how bright it\n"
        "is. A clean 0 in this mode does NOT\n"
        "mean the room is clean.\n"
        "\n"
        "\e*Probe\e* uses an IR phototransistor\n"
        "on a GPIO ADC pin. No filter and no\n"
        "AGC, so it reads steady light too.\n"
        "This is the mode that actually finds\n"
        "night-vision cameras. See Probe\n"
        "Setup for the wiring.\n"
        "\n"
        "\e#Reading it\n"
        "\e*STEADY\e* - flat DC. The\n"
        "night-vision illuminator signature.\n"
        "\e*FLICKER\e* - about 100/120 Hz.\n"
        "Riding the mains, so a lamp or a\n"
        "screen rather than a camera.\n"
        "\e*PULSED\e* - faster modulation. A\n"
        "remote, a beacon, or a PWM'd\n"
        "illuminator.\n"
        "\n"
        "\e#Hunting with it\n"
        "The eye is the instrument. Its ring\n"
        "fills clockwise with the live level\n"
        "and the pupil widens as you close in;\n"
        "the single tick on the ring is your\n"
        "best reading so far, so the game is\n"
        "to push the ring past the tick.\n"
        "\n"
        "The arrow on the right is the part\n"
        "you actually hunt with: it says\n"
        "whether you are getting warmer. Pan\n"
        "slowly and close in on the peak.\n"
        "\n"
        "\e#Keys\n"
        "\e*Sweep\e*\n"
        "OK - zero the peak and hit count\n"
        "Hold OK - re-null ambient (probe)\n"
        "Left / Right - sensitivity, live.\n"
        "Changing it either way zeroes the\n"
        "peak, because the old readings were\n"
        "taken against a different floor.\n"
        "\n"
        "\e*Probe Setup\e*\n"
        "Left / Right - change page\n"
        "OK - clear the peak reading\n"
        "\n"
        "\e#Limits\n"
        "Nyx finds IR \e*emitters\e*, not\n"
        "cameras. A camera that is not\n"
        "illuminating - one sitting in a lit\n"
        "room, or one that is switched off -\n"
        "emits nothing and Nyx will not see\n"
        "it. IR reflects, so a peak can be a\n"
        "bounce off a wall rather than the\n"
        "source. Sunlight and incandescent\n"
        "bulbs are full of IR. Treat a hit as\n"
        "a reason to look, not as proof, and\n"
        "confirm it with your eyes.\n"
        "\n"
        "Listen-only. Nyx never transmits IR.\n"
        "\n"
        "MIT licensed.\n"
        "github.com/at0m-b0mb/Nyx-FlipperZero\n");

    view_dispatcher_switch_to_view(app->view_dispatcher, NyxViewAbout);
}

bool nyx_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nyx_scene_about_on_exit(void* context) {
    NyxApp* app = context;
    widget_reset(app->widget);
}
