#include "display_fonts.h"
#include "ui_font_assets.h"

namespace display_fonts {
namespace {
lgfx::VLWfont small, body, title, temperature, headerClockFont, labelFont, homeLabelFont, captionFont, homeTitleFont, recordedHeaderFont;
lgfx::PointerWrapper smallData(ui_font_small, sizeof(ui_font_small));
lgfx::PointerWrapper bodyData(ui_font_body, sizeof(ui_font_body));
lgfx::PointerWrapper titleData(ui_font_title, sizeof(ui_font_title));
lgfx::PointerWrapper temperatureData(ui_font_temperature, sizeof(ui_font_temperature));
lgfx::PointerWrapper headerClockData(ui_font_header_clock, sizeof(ui_font_header_clock));
lgfx::PointerWrapper labelData(ui_font_label, sizeof(ui_font_label));
lgfx::PointerWrapper homeLabelData(ui_font_home_label, sizeof(ui_font_home_label));
lgfx::PointerWrapper captionData(ui_font_caption, sizeof(ui_font_caption));
lgfx::PointerWrapper homeTitleData(ui_font_home_title, sizeof(ui_font_home_title));
lgfx::PointerWrapper recordedHeaderData(ui_font_recorded_header, sizeof(ui_font_recorded_header));
bool ready = false;
}

bool init() {
    static bool attempted = false;
    if (attempted) return ready;
    attempted = true;
    ready = small.loadFont(&smallData) && body.loadFont(&bodyData)
        && title.loadFont(&titleData) && temperature.loadFont(&temperatureData)
        && headerClockFont.loadFont(&headerClockData)
        && labelFont.loadFont(&labelData) && homeLabelFont.loadFont(&homeLabelData)
        && captionFont.loadFont(&captionData) && homeTitleFont.loadFont(&homeTitleData)
        && recordedHeaderFont.loadFont(&recordedHeaderData);
    if (!ready) {
        small.unloadFont();
        body.unloadFont();
        title.unloadFont();
        temperature.unloadFont();
        headerClockFont.unloadFont();
        labelFont.unloadFont();
        homeLabelFont.unloadFont();
        captionFont.unloadFont();
        homeTitleFont.unloadFont();
        recordedHeaderFont.unloadFont();
    } else {
        // VLW otherwise guesses word spacing from line height, which is wider
        // than Roboto's actual space. Keep measured and drawn widths consistent.
        small.spaceWidth = ui_font_small_space_width;
        body.spaceWidth = ui_font_body_space_width;
        title.spaceWidth = ui_font_title_space_width;
        temperature.spaceWidth = ui_font_temperature_space_width;
        headerClockFont.spaceWidth = ui_font_header_clock_space_width;
        labelFont.spaceWidth = ui_font_label_space_width;
        homeLabelFont.spaceWidth = ui_font_home_label_space_width;
        captionFont.spaceWidth = ui_font_caption_space_width;
        homeTitleFont.spaceWidth = ui_font_home_title_space_width;
        recordedHeaderFont.spaceWidth = ui_font_recorded_header_space_width;
    }
    return ready;
}

const lgfx::IFont* label() { return ready ? &labelFont : static_cast<const lgfx::IFont*>(&fonts::Font0); }
const lgfx::IFont* homeLabel() { return ready ? &homeLabelFont : static_cast<const lgfx::IFont*>(&fonts::Font0); }
const lgfx::IFont* caption() { return ready ? &captionFont : static_cast<const lgfx::IFont*>(&fonts::Font0); }
const lgfx::IFont* homeTitle() { return ready ? &homeTitleFont : static_cast<const lgfx::IFont*>(&fonts::FreeSans9pt7b); }
const lgfx::IFont* recordedHeader() { return ready ? &recordedHeaderFont : static_cast<const lgfx::IFont*>(&fonts::FreeSans9pt7b); }
const lgfx::IFont* headerClock() { return ready ? &headerClockFont : static_cast<const lgfx::IFont*>(&fonts::FreeSans9pt7b); }

const lgfx::IFont* smooth(const lgfx::IFont* bitmap) {
    if (!ready) return bitmap;
    if (bitmap == &fonts::Font0) return &small;
    if (bitmap == &fonts::FreeSans9pt7b) return &body;
    if (bitmap == &fonts::FreeSansBold12pt7b) return &title;
    if (bitmap == &fonts::FreeSansBold18pt7b) return &temperature;
    return bitmap;
}
}
