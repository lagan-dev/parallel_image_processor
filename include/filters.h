#ifndef FILTERS_H
#define FILTERS_H

#include <ostream>
#include <vector>

#include "image.h"

enum BorderMode {
  BORDER_CLAMP,
  BORDER_REFLECT,
  BORDER_MIRROR,
  BORDER_WRAP,
  BORDER_CONSTANT
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec);

void grayscale(Image &dst, const Image &src);
void gaussian_blur(Image &output, const Image &input, int kernel_size,
                   float sigmaX, float sigmaY = 0.0,
                   BorderMode mode = BORDER_REFLECT,
                   const double *borderValue = nullptr);

#endif