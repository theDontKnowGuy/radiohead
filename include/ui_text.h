#pragma once

#include <Arduino.h>

// LovyanGFX draws glyphs in byte order and does not apply Unicode bidi. This
// adapter prepares the bounded Hebrew/Latin subset used by the radio UI.
struct UiTextLayout {
    String visual;
    bool rightToLeft = false;
};

UiTextLayout uiTextLayout(const String& logicalText);
