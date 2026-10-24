#ifndef DEFOLD_ARCORE_IMAGE_H
#define DEFOLD_ARCORE_IMAGE_H
#include <stdint.h>
#include <stddef.h>
#include <vector>
namespace arcore {
struct ImagePlane { const uint8_t* data; int64_t length, row_stride, pixel_stride; };
inline uint8_t ClampColor(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
// Validate before allocating or touching bytes. Limit camera allocation to 64
// MiB in addition to Defold's uint32 byte limit, including hostile SDK fixtures.
inline bool ConvertYuv(int width, int height, const ImagePlane p[3], std::vector<uint8_t>& rgb) {
    if (width <= 0 || height <= 0 || (uint64_t)width * height > (64u * 1024u * 1024u) / 3) return false;
    for (int i = 0; i < 3; ++i) {
        int64_t x = i ? (width - 1) / 2 : width - 1, y = i ? (height - 1) / 2 : height - 1;
        if (!p[i].data || p[i].length <= 0 || p[i].row_stride <= 0 || p[i].pixel_stride <= 0 ||
            p[i].row_stride > INT32_MAX || p[i].pixel_stride > INT32_MAX ||
            x * p[i].pixel_stride >= p[i].row_stride ||
            y * p[i].row_stride + x * p[i].pixel_stride >= p[i].length) return false;
    }
    rgb.resize((size_t)width * height * 3);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        int Y = (int)p[0].data[y*p[0].row_stride + x*p[0].pixel_stride] - 16;
        int U = (int)p[1].data[(y/2)*p[1].row_stride + (x/2)*p[1].pixel_stride] - 128;
        int V = (int)p[2].data[(y/2)*p[2].row_stride + (x/2)*p[2].pixel_stride] - 128;
        if (Y < 0) Y = 0;
        size_t pos = ((size_t)y * width + x) * 3;
        rgb[pos] = ClampColor((298*Y + 409*V + 128) >> 8);
        rgb[pos+1] = ClampColor((298*Y - 100*U - 208*V + 128) >> 8);
        rgb[pos+2] = ClampColor((298*Y + 516*U + 128) >> 8);
    }
    return true;
}
}
#endif
