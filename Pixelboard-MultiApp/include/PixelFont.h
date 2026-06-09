#pragma once
#include "Shared.h"

// ─── 3×5 Pixel-Font (gemeinsam für Snake, Pacman, App-Menü) ──────────────────
static const uint8_t PF_DIGITS[10][5] = {
    {7,5,5,5,7}, // 0
    {2,6,2,2,7}, // 1
    {7,1,7,4,7}, // 2
    {7,1,7,1,7}, // 3
    {5,5,7,1,1}, // 4
    {7,4,7,1,7}, // 5
    {7,4,7,5,7}, // 6
    {7,1,1,1,1}, // 7
    {7,5,7,5,7}, // 8
    {7,5,7,1,7}, // 9
};
static const uint8_t PF_S[5] = {7,4,7,1,7};
static const uint8_t PF_H[5] = {5,5,7,5,5};
static const uint8_t PF_P[5] = {6,5,6,4,4};
static const uint8_t PF_C[5] = {7,4,4,4,7};
static const uint8_t PF_A[5] = {7,5,7,5,5};
static const uint8_t PF_N[5] = {5,7,7,5,5};
static const uint8_t PF_W[5] = {5,5,7,7,5};
static const uint8_t PF_T[5] = {7,2,2,2,2};
static const uint8_t PF_U[5] = {5,5,5,5,7};
static const uint8_t PF_R[5] = {6,5,6,5,5};

inline void pfDrawChar(int x, int y, const uint8_t p[5], CRGB c) {
    for (int r = 0; r < 5; r++) {
        if (p[r] & 4) setPixel(x,     y + r, c);
        if (p[r] & 2) setPixel(x + 1, y + r, c);
        if (p[r] & 1) setPixel(x + 2, y + r, c);
    }
}
inline void pfDrawDigit(int x, int y, int d, CRGB c) {
    pfDrawChar(x, y, PF_DIGITS[d], c);
}
inline void pfDrawColon(int x, int y, CRGB c) {
    setPixel(x, y + 1, c);
    setPixel(x, y + 3, c);
}
// Zahl rechtsbündig in x=9..30 auf Zeile y
inline void pfDrawNumberRight(int y, int num, CRGB c) {
    num = num < 0 ? 0 : (num > 999 ? 999 : num);
    int d[3]; int n = 0;
    if (num >= 100) d[n++] = num / 100;
    if (n > 0 || num >= 10) d[n++] = (num / 10) % 10;
    d[n++] = num % 10;
    int sx = 9 + (21 - (n * 4 - 1)) / 2;
    for (int i = 0; i < n; i++) pfDrawDigit(sx + i * 4, y, d[i], c);
}
