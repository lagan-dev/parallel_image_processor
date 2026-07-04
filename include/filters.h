#ifndef FILTERS_H
#define FILTERS_H

#include <ostream>
#include <vector>

enum BorderMode {
  BORDER_CLAMP,
  BORDER_REFLECT,
  BORDER_MIRROR,
  BORDER_WRAP,
  BORDER_CONSTANT
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec);

void grayscale(unsigned char *data, int width, int height, int channels);
void gaussian_blur(unsigned char *data, const int width, const int height,
                   const int channels, int kernel_size, float sigmaX,
                   float sigmaY = 0.0, BorderMode mode = BORDER_REFLECT,
                   const double *borderValue = nullptr);

#endif