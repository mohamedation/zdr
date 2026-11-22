#include "ui/ui_text.h"

#include "font.h"
#include "framebuffer.h"

#include <cstring>

namespace {

constexpr int kCaptionPx = 10;
constexpr int kBodyPx = 12;
constexpr int kTitlePx = 18;
constexpr int kMonoPx = 11;

}  // namespace

UiText& UiText::instance() {
    static UiText inst;
    return inst;
}

TtfText& UiText::face(TextStyle style) {
    switch (style) {
        case TextStyle::Caption: return caption_;
        case TextStyle::Title: return title_;
        case TextStyle::Mono: return mono_;
        case TextStyle::Body:
        default: return body_;
    }
}

const TtfText& UiText::face(TextStyle style) const {
    return const_cast<UiText*>(this)->face(style);
}

bool UiText::ensureLoaded() {
    if (attempted_) return ok_;
    attempted_ = true;

    // Prefer a clean sans for body/title/caption; mono face for tables.
    // Paths cover Arch (TTF/) and Debian/Ubuntu (truetype/).
    static const char* kSans[] = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    };
    static const char* kMono[] = {
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
    };

    auto load_list = [](TtfText& font, const char* const* paths, int n,
                        int px) -> bool {
        for (int i = 0; i < n; ++i) {
            if (font.load(paths[i], px)) return true;
        }
        return loadDefaultUiFont(font, px);
    };

    const bool sans_ok =
        load_list(caption_, kSans, 4, kCaptionPx) &&
        load_list(body_, kSans, 4, kBodyPx) &&
        load_list(title_, kSans, 4, kTitlePx);
    const bool mono_ok = load_list(mono_, kMono, 4, kMonoPx);

    // Partial success is still useful (e.g. mono for ADSB list).
    ok_ = sans_ok || mono_ok;
    return ok_;
}

void UiText::drawBitmapFallback(Framebuffer& fb, int x, int y, const char* text,
                                uint16_t color, bool large) const {
    if (!text) return;
    const int gw = large ? FONT_8X16_WIDTH : FONT_6X8_WIDTH;
    const int gh = large ? FONT_8X16_HEIGHT : FONT_6X8_HEIGHT;
    int cx = x;
    for (const char* p = text; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x20 || c > 0x7E) {
            cx += gw + 1;
            continue;
        }
        if (large) {
            const uint8_t* glyph = font8x16[c - 0x20];
            for (int row = 0; row < gh; ++row) {
                const uint8_t line = glyph[row];
                for (int col = 0; col < gw; ++col) {
                    if (line & (0x80 >> col)) {
                        fb.drawPixel(cx + col, y + row, color);
                    }
                }
            }
        } else {
            const uint8_t* glyph = font6x8[c - 0x20];
            for (int row = 0; row < gh; ++row) {
                const uint8_t line = glyph[row];
                for (int col = 0; col < gw; ++col) {
                    if (line & (0x80 >> col)) {
                        fb.drawPixel(cx + col, y + row, color);
                    }
                }
            }
        }
        cx += gw + 1;
    }
}

int UiText::bitmapWidth(const char* text, bool large) const {
    if (!text) return 0;
    const int gw = large ? FONT_8X16_WIDTH : FONT_6X8_WIDTH;
    return static_cast<int>(strlen(text)) * (gw + 1);
}

void UiText::draw(Framebuffer& fb, int x, int y, const char* text,
                  TextStyle style, uint16_t color) const {
    if (!text || !*text) return;

    const_cast<UiText*>(this)->ensureLoaded();
    const TtfText& f = face(style);
    if (f.valid()) {
        f.drawTopLeft(fb, x, y, text, color);
        return;
    }

    // Title only uses the large bitmap; Mono stays compact for tables.
    const bool large = (style == TextStyle::Title);
    drawBitmapFallback(fb, x, y, text, color, large);
}

int UiText::width(const char* text, TextStyle style) const {
    if (!text) return 0;
    const_cast<UiText*>(this)->ensureLoaded();
    const TtfText& f = face(style);
    if (f.valid()) return f.textWidth(text);
    const bool large = (style == TextStyle::Title);
    return bitmapWidth(text, large);
}

int UiText::lineHeight(TextStyle style) const {
    const_cast<UiText*>(this)->ensureLoaded();
    const TtfText& f = face(style);
    if (f.valid()) return f.lineHeight();
    return (style == TextStyle::Title) ? FONT_8X16_HEIGHT : FONT_6X8_HEIGHT;
}

int UiText::ascender(TextStyle style) const {
    const_cast<UiText*>(this)->ensureLoaded();
    const TtfText& f = face(style);
    if (f.valid()) return f.ascender();
    return lineHeight(style);
}
