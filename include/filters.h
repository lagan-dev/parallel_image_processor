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

enum class Axis { Horizontal, Vertical };

struct GaussianKernels {
  std::vector<double> kernelX;
  std::vector<double> kernelY;
};

struct SobelKernels {
  std::vector<long long> kernelX;
  std::vector<long long> kernelY;
};

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec);

void grayscale(Image& dst, const Image& src);

void gaussian_blur(Image& output, const Image& input, int kernel_size,
                   float sigmaX, float sigmaY = 0.0,
                   BorderMode mode = BORDER_REFLECT,
                   const double* borderValue = nullptr);

template <typename TYPE>
void sobel(std::vector<TYPE>& output, const Image& input, int dx, int dy,
           int kernel_size, double scale = 1.0, double delta = 0.0,
           BorderMode mode = BORDER_REFLECT,
           const double* borderValue = nullptr);

#endif