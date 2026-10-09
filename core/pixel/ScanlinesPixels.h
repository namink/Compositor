#ifndef ScanlinesPixels_h
#define ScanlinesPixels_h
#include <stddef.h>
#include <stdint.h>

// Filter › Scanlines: the picture as a CRT draws it, in lines of light on a dark screen.
typedef struct {
    // Pixels from one line to the next.
    int spacing;
    // How much of the gap a full-bright line fills, 0–1; dimmer parts draw it thinner.
    float thickness;
    // 0–1: tones darker than this break the lines into round dots, blending through dashes into the solid line above it.
    float dots;
    // Pixels a line wavers sideways, in a wave down the screen.
    float wobble;
    // Pixels a line rises where the picture under it is bright (falls, if negative), so lines swell into its shapes.
    float displace;
    // 0–1: tones darker than this draw no line at all.
    float threshold;
    // Pixels the red and blue are moved apart, either way, for colored fringes.
    float split;
    // −1…1: darker or lighter, and flatter or punchier, before the lines are drawn.
    float density;
    float contrast;
    // 0–1: how bright the lines are where the picture is black.
    float blackLevel;
    // 0–1: how far the brightness is smoothed, over a few line spacings, before it displaces the lines.
    float smoothness;
    // 0: lines of `light` on `dark` (straight sRGB), brighter where the picture is. 1: lines in the picture's colors.
    int originalColors;
    uint8_t dark[3];
    uint8_t light[3];
} ScanlinesParams;

// Draws premultiplied RGBA pixels (4 bytes per pixel, `stride` bytes per row) as scanlines, in place. Alpha is kept and
// fully transparent pixels are left alone. Returns 0 if working memory couldn't be had.
int scanlines_apply(uint8_t *rgba, size_t width, size_t height, size_t stride, const ScanlinesParams *params);
#endif
