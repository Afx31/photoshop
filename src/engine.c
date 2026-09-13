#include <stdint.h>
#include <stddef.h>
#include <math.h>

#ifdef _OPENMP
#include <omp.h>
#endif

void apply_adjustments(
    const uint8_t *src,
    uint8_t *dst,
    int width,
    int height,
    int src_stride,
    int src_channels,
    int dst_stride,
    float exposure,
    float contrast,
    float highlights,
    float shadows,
    float whites,
    float blacks,
    float temperature,
    float tint,
    float vibrance,
    float saturation
) {
    const float exp_gain = exp2f(exposure);
    const float contrast_f = 1.0f + contrast;
    const float temp_r = 1.0f + temperature * 0.18f;
    const float temp_b = 1.0f - temperature * 0.18f;
    const float tint_g = 1.0f - tint * 0.14f;
    const float tint_rb = 1.0f + tint * 0.07f;
    const int nch = src_channels < 3 ? 3 : src_channels;

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int y = 0; y < height; y++) {
        const uint8_t *srow = src + (size_t)y * (size_t)src_stride;
        uint8_t *drow = dst + (size_t)y * (size_t)dst_stride;
        for (int x = 0; x < width; x++) {
            const uint8_t *p = srow + x * nch;
            float r = p[0] * (1.0f / 255.0f);
            float g = p[1] * (1.0f / 255.0f);
            float b = p[2] * (1.0f / 255.0f);

            r *= temp_r * tint_rb;
            g *= tint_g;
            b *= temp_b * tint_rb;

            r *= exp_gain;
            g *= exp_gain;
            b *= exp_gain;

            r = (r - 0.5f) * contrast_f + 0.5f;
            g = (g - 0.5f) * contrast_f + 0.5f;
            b = (b - 0.5f) * contrast_f + 0.5f;

            float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            if (lum < 0.0f) lum = 0.0f;
            float inv = 1.0f - lum;
            float h2 = lum * lum;
            float s2 = inv * inv;
            float lift = shadows * inv * 0.55f + blacks * s2 * inv * 0.70f;
            float gain = highlights * h2 * 0.55f + whites * h2 * lum * 0.70f;
            r += lift + gain;
            g += lift + gain;
            b += lift + gain;

            float mx = r > g ? r : g;
            if (b > mx) mx = b;
            float mn = r < g ? r : g;
            if (b < mn) mn = b;
            float satp = mx > 1e-5f ? (mx - mn) / mx : 0.0f;
            float gray = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            float vmix = 1.0f + vibrance * (1.0f - satp) + saturation;
            r = gray + (r - gray) * vmix;
            g = gray + (g - gray) * vmix;
            b = gray + (b - gray) * vmix;

            if (r < 0.0f) r = 0.0f; else if (r > 1.0f) r = 1.0f;
            if (g < 0.0f) g = 0.0f; else if (g > 1.0f) g = 1.0f;
            if (b < 0.0f) b = 0.0f; else if (b > 1.0f) b = 1.0f;

            uint8_t *d = drow + x * 3;
            d[0] = (uint8_t)(r * 255.0f + 0.5f);
            d[1] = (uint8_t)(g * 255.0f + 0.5f);
            d[2] = (uint8_t)(b * 255.0f + 0.5f);
        }
    }
}
