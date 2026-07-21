#ifndef FILTERS_H
#define FILTERS_H

#include <ostream>
#include <thread>
#include <vector>

#include "image.h"
#include "threadpool.h"

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

void grayscale(Image& dst, const Image& src, ThreadPool& pool,
               unsigned int num_threads);

void gaussian_blur(Image& output, const Image& input, ThreadPool& pool,
                   unsigned int num_threads, int kernel_size, float sigmaX,
                   float sigmaY = 0.0, BorderMode mode = BORDER_REFLECT,
                   const double* borderValue = nullptr);

template <typename TYPE>
void sobel(std::vector<TYPE>& output, const Image& input, ThreadPool& pool,
           unsigned int num_threads, int dx, int dy, int kernel_size,
           double scale = 1.0, double delta = 0.0,
           BorderMode mode = BORDER_REFLECT,
           const double* borderValue = nullptr);

#endif
