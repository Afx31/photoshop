#ifndef PHOTO_ENGINE_H
#define PHOTO_ENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
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
    float saturation);

#ifdef __cplusplus
}
#endif

#endif
