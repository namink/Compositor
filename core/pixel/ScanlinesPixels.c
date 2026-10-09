#include "ScanlinesPixels.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Ported from the macOS app's Rendering/DitherPixels.c (scanlines_apply). The algorithm is unchanged; the
// Grand Central Dispatch bands are replaced by plain loops so it builds on MSVC as well as clang.

static inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// Density darkens (positive) or lightens as a gamma, so black and white stay put; contrast pivots on mid gray.
static inline float adjust_tone(float v, float gamma, float contrast) {
    v = powf(clamp01(v), gamma);
    return clamp01((v - 0.5f) * contrast + 0.5f);
}

static inline void write_pixel(uint8_t *px, float r, float g, float b) {
    float a = (float)px[3] / 255.0f;
    px[0] = (uint8_t)lroundf(clamp01(r) * a * 255.0f);
    px[1] = (uint8_t)lroundf(clamp01(g) * a * 255.0f);
    px[2] = (uint8_t)lroundf(clamp01(b) * a * 255.0f);
}

// Draws the lines over the scanned tones, a column at a time. Displaced, they're a landscape seen from the front, after
// the Rutt-Etra video synthesizer: each line is lifted (or, displaced the other way, lowered) by the picture's
// brightness smoothed into hills, and hides whatever lies behind it, so nearer lines wrap over the shapes rather than
// crossing the ones beyond. Nearest line first: the bottom one when lines rise, the top one when they fall.
static int draw_lines(uint8_t *rgba, size_t width, size_t height, size_t stride, const ScanlinesParams *p,
                      const float *scan, const uint8_t *alpha, size_t lines, size_t spacing) {
    int original = p->originalColors;
    size_t plane = lines * width;
    float *tone = malloc(plane * sizeof(float)), *lift = malloc(plane * sizeof(float)), *spread = malloc(plane * sizeof(float));
    if (!tone || !lift || !spread) { free(tone); free(lift); free(spread); return 0; }
    for (size_t at = 0; at < plane; ++at)
        tone[at] = original ? 0.2126f * scan[at] + 0.7152f * scan[plane + at] + 0.0722f * scan[2 * plane + at] : scan[at];
    // The height: brightness blurred along each line (three box passes, close to a Gaussian) and then across its
    // neighbors, so a face rises as one rounded hill instead of a staircase of the pixels under it.
    float smoothness = clamp01(p->smoothness);
    long radius = lroundf(smoothness * (float)spacing * 2);
    memcpy(lift, tone, plane * sizeof(float));
    if (radius > 0 && p->displace != 0) {
        float *copy = malloc(width * sizeof(float));
        if (!copy) { free(tone); free(lift); free(spread); return 0; }
        for (size_t line = 0; line < lines; ++line) {
            float *row = lift + line * width;
            for (int pass = 0; pass < 3; ++pass) {
                memcpy(copy, row, width * sizeof(float));
                float sum = 0;
                for (long x = -radius; x <= radius; ++x) sum += copy[x < 0 ? 0 : x >= (long)width ? width - 1 : (size_t)x];
                for (size_t x = 0; x < width; ++x) {
                    row[x] = sum / (float)(2 * radius + 1);
                    long out = (long)x - radius, in = (long)x + radius + 1;
                    sum += copy[in >= (long)width ? width - 1 : (size_t)in] - copy[out < 0 ? 0 : (size_t)out];
                }
            }
        }
        free(copy);
        long across = lroundf(smoothness * 2);
        for (long step = 0; step < across; ++step) {
            for (size_t line = 0; line < lines; ++line) {
                const float *above = lift + (line ? line - 1 : 0) * width, *here = lift + line * width;
                const float *below = lift + (line + 1 < lines ? line + 1 : line) * width;
                for (size_t x = 0; x < width; ++x) spread[line * width + x] = (above[x] + 2 * here[x] + below[x]) / 4;
            }
            memcpy(lift, spread, plane * sizeof(float));
        }
    }
    free(spread);

    float middle = (float)spacing / 2, dots = clamp01(p->dots), threshold = clamp01(p->threshold), displace = p->displace;
    float thickness = clamp01(p->thickness), blackLevel = clamp01(p->blackLevel);
    int rising = displace >= 0;
    float dark[3] = { p->dark[0] / 255.0f, p->dark[1] / 255.0f, p->dark[2] / 255.0f };
    float light[3] = { p->light[0] / 255.0f, p->light[1] / 255.0f, p->light[2] / 255.0f };
    if (original) dark[0] = dark[1] = dark[2] = 0;
    const float *screen = dark, *phosphor = light;
    float *cover = malloc(height * sizeof(float) * 4);
    if (!cover) { free(tone); free(lift); return 0; }
    float *color = cover + height;
    for (size_t x = 0; x < width; ++x) {
        memset(cover, 0, height * sizeof(float));
        float along = fmodf((float)x + 0.5f, (float)spacing) - middle;
        // The edge of everything drawn so far in this column; lines further back show only beyond it.
        float horizon = rising ? INFINITY : -INFINITY;
        for (size_t step = 0; step < lines; ++step) {
            size_t line = rising ? lines - 1 - step : step;
            // Dots: darker than the Dots level, the line breaks into beads, one every line spacing, each lit in the
            // tone at its middle; just above it, into dashes that close up into the solid line.
            float beading = dots > 0 ? clamp01((dots - tone[line * width + x]) / 0.2f) : 0, across = along * beading;
            long centered = lroundf((float)x - across);
            size_t at = centered < 0 ? 0 : (size_t)centered >= width ? width - 1 : (size_t)centered;
            size_t i = line * width + at;
            float t = tone[i];
            float lit = threshold > 0 ? clamp01((t - threshold) / 0.04f) : 1;
            if (lit <= 0) continue;
            // Drawn between this column's height and the last's, so a steep climb never breaks the line.
            float base = (float)line * (float)spacing + middle;
            float here = base - displace * lift[i], before = at > 0 ? base - displace * lift[i - 1] : here;
            float lo = fminf(here, before), hi = fmaxf(here, before);
            // Black Level: the line's least brightness, so it still shows where the picture is black.
            float level = blackLevel + (1 - blackLevel) * t, c[3];
            for (int k = 0; k < 3; ++k)
                c[k] = original ? blackLevel + (1 - blackLevel) * scan[(size_t)k * plane + i] : screen[k] + (phosphor[k] - screen[k]) * level;
            // Half the line's height: thinner where the picture is dim.
            float beam = middle * thickness * (0.29f + 0.71f * sqrtf(clamp01(level)));
            long top = (long)floorf(rising ? lo - beam - 1 : fmaxf(lo - beam - 1, horizon - 1));
            long bottom = (long)ceilf(rising ? fminf(hi + beam + 1, horizon + 1) : hi + beam + 1);
            if (top < 0) top = 0;
            if (bottom > (long)height) bottom = (long)height;
            for (long y = top; y < bottom; ++y) {
                float yy = (float)y + 0.5f;
                float off = yy < lo ? lo - yy : yy > hi ? yy - hi : 0;
                float distance = sqrtf(off * off + across * across);
                float hidden = rising ? clamp01(horizon - yy + 0.5f) : clamp01(yy - horizon + 0.5f);
                float shown = clamp01(beam - distance + 0.5f) * lit * hidden;
                if (shown > cover[y]) {
                    cover[y] = shown;
                    color[y * 3] = c[0]; color[y * 3 + 1] = c[1]; color[y * 3 + 2] = c[2];
                }
            }
            if (lit > 0.5f) horizon = rising ? fminf(horizon, lo - beam) : fmaxf(horizon, hi + beam);
        }
        for (size_t y = 0; y < height; ++y) {
            if (!alpha[y * width + x]) continue;
            // The beam is driven brighter than the picture, making up for the dark screen between lines.
            float shown = cover[y];
            write_pixel(rgba + y * stride + x * 4, screen[0] + (color[y * 3] * 1.35f - screen[0]) * shown,
                        screen[1] + (color[y * 3 + 1] * 1.35f - screen[1]) * shown,
                        screen[2] + (color[y * 3 + 2] * 1.35f - screen[2]) * shown);
        }
    }
    free(cover);
    free(tone); free(lift);
    return 1;
}

int scanlines_apply(uint8_t *rgba, size_t width, size_t height, size_t stride, const ScanlinesParams *p) {
    size_t count = width * height;
    if (!count) return 1;
    size_t spacing = (size_t)(p->spacing < 2 ? 2 : p->spacing);
    size_t lines = (height + spacing - 1) / spacing;
    int planes = p->originalColors ? 3 : 1;
    float *tone = malloc(count * sizeof(float) * (size_t)planes);
    uint8_t *alpha = malloc(count);
    // Each line's tone along it: the average of the rows it covers, sampled where its wobble moves it from.
    float *scan = malloc(lines * width * sizeof(float) * (size_t)planes);
    if (!tone || !alpha || !scan) { free(tone); free(alpha); free(scan); return 0; }

    float gamma = exp2f(p->density * 1.5f);
    float contrast = p->contrast >= 0 ? 1.0f / (1.0f - 0.95f * p->contrast) : 1.0f + p->contrast;
    int original = p->originalColors;
    for (size_t y = 0; y < height; ++y) {
        const uint8_t *row = rgba + y * stride;
        for (size_t x = 0; x < width; ++x) {
            const uint8_t *px = row + x * 4;
            size_t at = y * width + x;
            alpha[at] = px[3];
            float r = 0, g = 0, b = 0;
            if (px[3]) { float scale = 1.0f / (float)px[3]; r = px[0] * scale; g = px[1] * scale; b = px[2] * scale; }
            if (original) {
                tone[at] = adjust_tone(r, gamma, contrast);
                tone[count + at] = adjust_tone(g, gamma, contrast);
                tone[2 * count + at] = adjust_tone(b, gamma, contrast);
            } else {
                tone[at] = adjust_tone(0.2126f * r + 0.7152f * g + 0.0722f * b, gamma, contrast);
            }
        }
    }
    float wobble = p->wobble;
    for (size_t line = 0; line < lines; ++line) {
        size_t top = line * spacing, bottom = top + spacing < height ? top + spacing : height;
        // A slow wave down the screen with a quicker one over it, as a CRT's picture wavers when its sync drifts.
        float wave = sinf((float)line * 0.45f) * 0.7f + sinf((float)line * 1.7f + 1.3f) * 0.3f;
        long shift = lroundf(wobble * wave);
        for (size_t x = 0; x < width; ++x) {
            float sum[3] = { 0, 0, 0 }; int n = 0;
            long sx = (long)x - shift;
            if (sx >= 0 && sx < (long)width)
                for (size_t y = top; y < bottom; ++y) {
                    size_t at = y * width + (size_t)sx;
                    if (!alpha[at]) continue;
                    for (int c = 0; c < planes; ++c) sum[c] += tone[(size_t)c * count + at];
                    ++n;
                }
            for (int c = 0; c < planes; ++c) scan[((size_t)c * lines + line) * width + x] = n ? sum[c] / (float)n : 0;
        }
    }

    int drawn = draw_lines(rgba, width, height, stride, p, scan, alpha, lines, spacing);
    free(tone); free(alpha); free(scan);
    if (!drawn) return 0;

    // Color split: red moved one way and blue the other, for colored fringes on the lines' edges.
    long split = lroundf(p->split);
    if (split > 0) {
        uint8_t *copy = malloc(width * 4);
        if (!copy) return 0;
        for (size_t y = 0; y < height; ++y) {
            uint8_t *row = rgba + y * stride;
            memcpy(copy, row, width * 4);
            for (size_t x = 0; x < width; ++x) {
                long from = (long)x - split, to = (long)x + split;
                uint8_t a = row[x * 4 + 3];
                uint8_t r = from >= 0 ? copy[from * 4] : 0, b = to < (long)width ? copy[to * 4 + 2] : 0;
                row[x * 4] = r > a ? a : r;
                row[x * 4 + 2] = b > a ? a : b;
            }
        }
        free(copy);
    }
    return 1;
}
