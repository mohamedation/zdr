#include "ui/aircraft_glyph.h"

#include "framebuffer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace AircraftGlyph {
namespace {

struct Vec2 {
    float x;
    float y;
};

// ATC-style dart: tip forward, notched tail. Readable at ~9 px.
constexpr Vec2 kDart[] = {
    {0.0f, 4.6f},    // nose
    {-3.4f, -3.2f},  // left trailing corner
    {0.0f, -1.0f},   // tail notch
    {3.4f, -3.2f},   // right trailing corner
};

// Top-down airliner outline (nose = +y). Used for the detail rose.
constexpr Vec2 kJet[] = {
    {0.0f, 7.0f},     // nose tip
    {0.85f, 4.2f},
    {0.95f, 1.6f},
    {6.8f, 0.35f},    // right wing tip (leading)
    {7.0f, -0.55f},   // right wing tip (trailing)
    {1.15f, -0.35f},
    {1.05f, -3.4f},
    {3.4f, -4.1f},    // right stabilizer
    {3.4f, -5.0f},
    {0.55f, -4.55f},
    {0.45f, -6.6f},   // fin / tail tip
    {-0.45f, -6.6f},
    {-0.55f, -4.55f},
    {-3.4f, -5.0f},
    {-3.4f, -4.1f},
    {-1.05f, -3.4f},
    {-1.15f, -0.35f},
    {-7.0f, -0.55f},
    {-6.8f, 0.35f},
    {-0.95f, 1.6f},
    {-0.85f, 4.2f},
};

void transform(const Vec2& local, float scale, float cos_h, float sin_h,
               float& ox, float& oy) {
    const float dx = local.x * scale;
    const float dy = local.y * scale;
    // Same basis as before: heading 0° → nose up on screen.
    ox = dx * (-sin_h) + dy * cos_h;
    oy = dx * cos_h + dy * sin_h;
}

void fillPolygon(Framebuffer& fb, const std::vector<int>& xs,
                 const std::vector<int>& ys, uint16_t color) {
    const int n = static_cast<int>(xs.size());
    if (n < 3) return;

    int ymin = ys[0];
    int ymax = ys[0];
    for (int i = 1; i < n; ++i) {
        ymin = std::min(ymin, ys[i]);
        ymax = std::max(ymax, ys[i]);
    }

    std::vector<int> nodes;
    nodes.reserve(static_cast<size_t>(n));
    for (int y = ymin; y <= ymax; ++y) {
        nodes.clear();
        for (int i = 0, j = n - 1; i < n; j = i++) {
            const int yi = ys[static_cast<size_t>(i)];
            const int yj = ys[static_cast<size_t>(j)];
            if ((yi < y && yj >= y) || (yj < y && yi >= y)) {
                const int xi = xs[static_cast<size_t>(i)];
                const int xj = xs[static_cast<size_t>(j)];
                const int x = xi + (y - yi) * (xj - xi) / (yj - yi);
                nodes.push_back(x);
            }
        }
        std::sort(nodes.begin(), nodes.end());
        for (size_t k = 0; k + 1 < nodes.size(); k += 2) {
            int x0 = nodes[k];
            int x1 = nodes[k + 1];
            if (x1 < x0) std::swap(x0, x1);
            for (int x = x0; x <= x1; ++x) {
                fb.drawPixel(x, y, color);
            }
        }
    }
}

void strokePolygon(Framebuffer& fb, const std::vector<int>& xs,
                   const std::vector<int>& ys, uint16_t color) {
    const int n = static_cast<int>(xs.size());
    if (n < 2) return;
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        fb.drawLine(xs[static_cast<size_t>(i)], ys[static_cast<size_t>(i)],
                    xs[static_cast<size_t>(j)], ys[static_cast<size_t>(j)],
                    color);
    }
}

template <size_t N>
void drawShape(Framebuffer& fb, int cx, int cy, uint16_t color,
               float heading_deg, float scale, const Vec2 (&outline)[N],
               bool stroke) {
    const float rad =
        (heading_deg - 90.0f) * static_cast<float>(M_PI) / 180.0f;
    const float sin_h = std::sin(rad);
    const float cos_h = std::cos(rad);

    std::vector<int> xs;
    std::vector<int> ys;
    xs.reserve(N);
    ys.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        float ox = 0.0f;
        float oy = 0.0f;
        transform(outline[i], scale, cos_h, sin_h, ox, oy);
        xs.push_back(cx + static_cast<int>(std::lround(ox)));
        ys.push_back(cy + static_cast<int>(std::lround(oy)));
    }

    fillPolygon(fb, xs, ys, color);
    if (stroke) strokePolygon(fb, xs, ys, color);
}

}  // namespace

int patternCount() { return static_cast<int>(sizeof(kJet) / sizeof(kJet[0])); }

void draw(Framebuffer& fb, int cx, int cy, uint16_t color, float heading_deg,
          float scale) {
    if (scale <= 0.0f) return;

    // Radar / profile: compact directional dart.
    // Detail rose (scale ≳ 1.8): filled swept-wing jet.
    if (scale < 1.8f) {
        const float dart_scale = std::max(0.55f, scale);
        drawShape(fb, cx, cy, color, heading_deg, dart_scale, kDart,
                  /*stroke=*/false);
    } else {
        drawShape(fb, cx, cy, color, heading_deg, scale, kJet,
                  /*stroke=*/true);
    }
}

}  // namespace AircraftGlyph
