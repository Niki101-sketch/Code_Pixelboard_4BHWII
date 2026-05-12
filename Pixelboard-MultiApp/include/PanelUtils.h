#pragma once
#include <FastLED.h>
#include <LEDMatrix.h>

// Gemeinsame Konstanten und Hilfsfunktionen für das 32×8 Panel-Layout.
// Werden von AppWetter und AppUhrzeit genutzt.

constexpr uint8_t PANEL_W = 32;
constexpr uint8_t PANEL_H = 8;

using PanelMat  = cLEDMatrix<PANEL_W, PANEL_H, VERTICAL_ZIGZAG_MATRIX>;
using Canvas16  = cLEDMatrix<64, 16, HORIZONTAL_MATRIX>;

inline void verschiebeEineZeileNachUnten(Canvas16& c16) {
    for (int y = 15; y > 0; y--)
        for (uint8_t x = 0; x < 64; x++)
            c16(x, y) = c16(x, y - 1);
    for (uint8_t x = 0; x < 64; x++)
        c16(x, 0) = CRGB::Black;
}

inline void mirrorH(PanelMat& p) {
    for (uint8_t y = 0; y < PANEL_H; y++)
        for (uint8_t x = 0; x < PANEL_W / 2; x++) {
            uint8_t xo = PANEL_W - 1 - x;
            CRGB tmp = p(x, y); p(x, y) = p(xo, y); p(xo, y) = tmp;
        }
}

inline void rotate180(PanelMat& p) {
    for (uint8_t y = 0; y < PANEL_H; y++)
        for (uint8_t x = 0; x < PANEL_W / 2; x++) {
            uint8_t xo = PANEL_W - 1 - x, yo = PANEL_H - 1 - y;
            CRGB tmp = p(x, y); p(x, y) = p(xo, yo); p(xo, yo) = tmp;
        }
}

inline void blitPanels(Canvas16& c16, PanelMat& top, PanelMat& bot) {
    for (uint8_t y = 0; y < PANEL_H; y++)
        for (uint8_t x = 0; x < PANEL_W; x++)
            top(x, y) = c16(x, y + PANEL_H);
    for (uint8_t y = 0; y < PANEL_H; y++)
        for (uint8_t x = 0; x < PANEL_W; x++)
            bot(x, y) = c16(x, y);
    mirrorH(top);
    mirrorH(bot);
    rotate180(top);
}
