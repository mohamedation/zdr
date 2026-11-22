#ifndef UI_TEXT_H
#define UI_TEXT_H

#include "ui/ttf_text.h"

#include <cstdint>

class Framebuffer;

/** Named text styles for the UI (FreeType-backed, bitmap fallback). */
enum class TextStyle {
    Caption,  // ~10px — compact labels / help keys
    Body,     // ~12px — general UI copy
    Title,    // ~18px — callsigns / emphasis
    Mono,     // ~11px — tables, ICAO, numeric columns
};

/**
 * Shared typography layer. One process-wide instance loads a few TTF sizes
 * once; callers pick a TextStyle instead of hard-coding bitmap fonts.
 */
class UiText {
public:
    static UiText& instance();

    /** Load fonts if needed. Safe to call repeatedly. */
    bool ensureLoaded();

    bool ready() const { return ok_; }

    /** Draw with top-left at (x, y). Falls back to 6x8 bitmap if TTF missing. */
    void draw(Framebuffer& fb, int x, int y, const char* text, TextStyle style,
              uint16_t color) const;

    int width(const char* text, TextStyle style) const;
    int lineHeight(TextStyle style) const;
    int ascender(TextStyle style) const;

private:
    UiText() = default;

    TtfText& face(TextStyle style);
    const TtfText& face(TextStyle style) const;

    void drawBitmapFallback(Framebuffer& fb, int x, int y, const char* text,
                            uint16_t color, bool large) const;
    int bitmapWidth(const char* text, bool large) const;

    TtfText caption_;
    TtfText body_;
    TtfText title_;
    TtfText mono_;
    bool attempted_ = false;
    bool ok_ = false;
};

inline UiText& uiText() { return UiText::instance(); }

#endif  // UI_TEXT_H
