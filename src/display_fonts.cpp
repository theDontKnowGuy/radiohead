#include "display_fonts.h"
#include "ui_font_assets.h"

namespace display_fonts {
namespace {
lgfx::VLWfont small, body, title, temperature, clock, labelFont, captionFont;
lgfx::PointerWrapper smallData(ui_font_small, sizeof(ui_font_small));
lgfx::PointerWrapper bodyData(ui_font_body, sizeof(ui_font_body));
lgfx::PointerWrapper titleData(ui_font_title, sizeof(ui_font_title));
lgfx::PointerWrapper temperatureData(ui_font_temperature, sizeof(ui_font_temperature));
lgfx::PointerWrapper clockData(ui_font_clock, sizeof(ui_font_clock));
lgfx::PointerWrapper labelData(ui_font_label, sizeof(ui_font_label));
lgfx::PointerWrapper captionData(ui_font_caption, sizeof(ui_font_caption));
bool ready = false;
}

bool init() {
    static bool attempted = false;
    if (attempted) return ready;
    attempted = true;
    ready = small.loadFont(&smallData) && body.loadFont(&bodyData)
        && title.loadFont(&titleData) && temperature.loadFont(&temperatureData)
        && clock.loadFont(&clockData) && labelFont.loadFont(&labelData)
        && captionFont.loadFont(&captionData);
    if (!ready) {
        small.unloadFont();
        body.unloadFont();
        title.unloadFont();
        temperature.unloadFont();
        clock.unloadFont();
        labelFont.unloadFont();
        captionFont.unloadFont();
    } else {
        // VLW otherwise guesses word spacing from line height, which is wider
        // than Roboto's actual space. Keep measured and drawn widths consistent.
        small.spaceWidth = ui_font_small_space_width;
        body.spaceWidth = ui_font_body_space_width;
        title.spaceWidth = ui_font_title_space_width;
        temperature.spaceWidth = ui_font_temperature_space_width;
        clock.spaceWidth = ui_font_clock_space_width;
        labelFont.spaceWidth = ui_font_label_space_width;
        captionFont.spaceWidth = ui_font_caption_space_width;
    }
    return ready;
}

const lgfx::IFont* label() { return ready ? &labelFont : static_cast<const lgfx::IFont*>(&fonts::Font0); }
const lgfx::IFont* caption() { return ready ? &captionFont : static_cast<const lgfx::IFont*>(&fonts::Font0); }

const lgfx::IFont* smooth(const lgfx::IFont* bitmap) {
    if (!ready) return bitmap;
    if (bitmap == &fonts::Font0) return &small;
    if (bitmap == &fonts::FreeSans9pt7b) return &body;
    if (bitmap == &fonts::FreeSansBold12pt7b) return &title;
    if (bitmap == &fonts::FreeSansBold18pt7b) return &temperature;
    if (bitmap == &fonts::FreeSansBold24pt7b) return &clock;
    return bitmap;
}
}
