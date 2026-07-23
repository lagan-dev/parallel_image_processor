#include "../include/filters.h"

#include <sys/types.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "threadpool.h"

template <typename T>
using SamplePixelFn = void (*)(std::vector<double>&, const T*, int, int, int,
                               int, int, const double*);

void grayscale(Image& dst, const Image& src, ThreadPool& pool,
               unsigned int num_threads) {
  // Standard formula:
  // ----> Grayscale = (0.299 × R) + (0.587 × G) + (0.114 × B)

  auto channels = src.getChannels();

  if (channels < 1 || channels > 4) {
    throw std::invalid_argument("Unsupported number of channels. Must be 1, 2, 3, or 4.");
  }

  auto height = src.getHeight();
  auto width = src.getWidth();
  auto input_data = src.getData();
  auto out_data = dst.getDataMutable();

  if (channels == 1) {
    std::copy(input_data, input_data + width * height, out_data);
    return;
  }

  // OpenCv uses integer coefficients for the weighted sum:
  // OpenCV uses 15-bit shift precision in C++ CPU fallback mode:
  static const int R2Y = 9798;  // round(0.299 * 32768)
  static const int G2Y = 19235; // round(0.587 * 32768)
  static const int B2Y = 3735;  // round(0.114 * 32768)
  static const int shift = 15;
  constexpr int half = 1 << (shift - 1);

  if (num_threads == 0) num_threads = 1;

  if (num_threads == 1) {
    for (int idx = 0; idx < width * height; idx++) {
      auto px = input_data + (idx * channels);

      // Get RGB values
      unsigned char r = px[0];
      unsigned char g = px[1];
      unsigned char b = px[2];

      // Calc grayscale values
      // Comply  with OpenCV's implementation
      unsigned char gray = static_cast<unsigned char>(
          (R2Y * r + G2Y * g + B2Y * b + half) >> shift);

      // Store output values
      out_data[idx] = gray;
    }
  } else {
    int64_t total = 1L * width * height;
    int64_t chunk = (total + num_threads - 1) / num_threads;

    for (int t = 0; t < num_threads; t++) {
      int64_t start = t * chunk;
      int64_t end = std::min((start + chunk), total);

      if (start >= end) break;

      pool.enqueue([=]() {
        for (int idx = start; idx < end; idx++) {
          auto px = input_data + (idx * channels);

          // Get RGB values
          unsigned char r = px[0];
          unsigned char g = px[1];
          unsigned char b = px[2];

          // Calc grayscale values
          // Comply  with OpenCV's implementation
          unsigned char gray = static_cast<unsigned char>(
              (R2Y * r + G2Y * g + B2Y * b + half) >> shift);

          // Store output values
          out_data[idx] = gray;
        }
      });
    }

    // wait for all threads to complete before returning
    pool.waitAll();
  }
}

std::vector<double> get_1d_gaussian_kernel(int kernel_size, double sigma) {
  // Validate sigma
  if (sigma <= 0.0f) {
    sigma = 1.0f;
  }

  // Determine radius/kernel size when not provided
  int radius;
  if (kernel_size <= 0) {
    radius = static_cast<int>(std::round(4.0f * sigma));
    kernel_size = 2 * radius + 1;
  } else {
    radius = kernel_size / 2;
  }

  // e ^(- (x / 2σ) ^ 2)
  std::vector<double> kernel(kernel_size);
  const double denom =
      2.0 * static_cast<double>(sigma) * static_cast<double>(sigma);
  double sum = 0.0;

  for (int i = 0; i < kernel_size; ++i) {
    int x = i - radius;
    double v = std::exp(-(static_cast<double>(x) * x) / denom);
    kernel[i] = v;
    sum += v;
  }

  // Normalize
  if (sum > 0.0) {
    for (auto& val : kernel) {
      val = (val / sum);
    }
  }

  return kernel;
}

GaussianKernels getGaussianKernels(int kernel_size, double sigmaX,
                                   double sigmaY) {
  GaussianKernels gk;

  gk.kernelX = get_1d_gaussian_kernel(kernel_size, sigmaX);
  gk.kernelY = get_1d_gaussian_kernel(kernel_size, sigmaY);

  return gk;
}

template <BorderMode Mode>
int get_bounded_index(int h, int w, int height, int width, int channels) {
  auto get_reflect_idx = [](int k, int n) {
    constexpr int delta = (Mode == BORDER_REFLECT);
    if (n == 1) return 0;

    while (k < 0 || k >= n) {
      if (k < 0) {
        k = -k - 1 + delta;
      } else {
        k = 2 * n - k - 1 - delta;
      }
    }

    return k;
  };

  if constexpr (Mode == BORDER_CLAMP) {
    h = std::clamp(h, 0, height - 1);
    w = std::clamp(w, 0, width - 1);
  } else if constexpr (Mode == BORDER_WRAP) {
    h = h % height;
    if (h < 0) h += height;

    w = w % width;
    if (w < 0) w += width;
  } else if constexpr (Mode == BORDER_REFLECT || Mode == BORDER_MIRROR) {
    h = get_reflect_idx(h, height);
    w = get_reflect_idx(w, width);
  } else {
    std::cout << "BorderMode not supported!!" << std::endl;
  }

  return (h * width + w) * channels;
}

template <typename T, BorderMode Mode>
void sample_pixel(std::vector<double>& px, const T* data, int h, int w,
                  int height, int width, int channels,
                  const double* borderValue = nullptr) {
  if constexpr (Mode == BORDER_CONSTANT) {
    if ((h >= 0 && h < height) && (w >= 0 && w < width)) {
      int idx = (h * width + w) * channels;
      for (int c = 0; c < channels; c++) {
        px[c] = data[idx + c];
      }
    } else {
      for (int c = 0; c < channels; c++) {
        auto border_val = borderValue ? borderValue[c] : 0;
        px[c] = border_val;
      }
    }
  } else {
    auto idx = get_bounded_index<Mode>(h, w, height, width, channels);

    for (int c = 0; c < channels; c++) {
      px[c] = data[idx + c];
    }
  }
}

template <typename T>
SamplePixelFn<T> GetSamplePixelFn(BorderMode mode) {
  switch (mode) {
    case BORDER_CLAMP:
      return &sample_pixel<T, BORDER_CLAMP>;

    case BORDER_REFLECT:
      return &sample_pixel<T, BORDER_REFLECT>;

    case BORDER_MIRROR:
      return &sample_pixel<T, BORDER_MIRROR>;

    case BORDER_WRAP:
      return &sample_pixel<T, BORDER_WRAP>;

    case BORDER_CONSTANT:
      return &sample_pixel<T, BORDER_CONSTANT>;

    default:
      throw std::invalid_argument("Unknown BorderMode");
  }
}

// Serial variant — same math, zero std::thread involvement
template <Axis A, typename IN_T, typename KRNL_T>
void convolution1D_serial(double* out_data, IN_T* in_data,
                          const std::vector<KRNL_T>& kernel, int width,
                          int height, int channels, BorderMode mode,
                          const double* borderValue) {
  int n = kernel.size() / 2;
  std::vector<double> sum(channels, 0.0);
  std::vector<double> px(channels, 0.0);

  auto sample_pixel_fn = GetSamplePixelFn<IN_T>(mode);

  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      int index = (row * width + col) * channels;
      std::fill(sum.begin(), sum.end(), 0.0);
      for (int k = -n; k <= n; k++) {
        int row_ = row, col_ = col;
        if constexpr (A == Axis::Horizontal)
          col_ = col + k;
        else
          row_ = row + k;

        sample_pixel_fn(px, in_data, row_, col_, height, width, channels,
                        borderValue);

        auto kernel_val = kernel[k + n];
        for (int c = 0; c < channels; c++) sum[c] += px[c] * kernel_val;
      }
      for (int c = 0; c < channels; c++) out_data[index + c] = sum[c];
    }
  }
}

template <Axis A, typename IN_T, typename KRNL_T>
void convolution1D(double* out_data, IN_T* in_data,
                   const std::vector<KRNL_T>& kernel, int width, int height,
                   int channels, BorderMode mode, const double* borderValue,
                   unsigned int num_threads, ThreadPool& pool) {
  if (num_threads == 0) num_threads = 1;

  int n = kernel.size() / 2;

  int64_t chunk = (height + num_threads - 1) / num_threads;
  auto sample_pixel_fn = GetSamplePixelFn<IN_T>(mode);

  for (int t = 0; t < num_threads; t++) {
    auto start_h = t * chunk;
    auto end_h = std::min((start_h + chunk), 1L * height);

    if (start_h >= end_h) break;

    pool.enqueue([=]() {
      std::vector<double> sum(channels, 0.0);
      std::vector<double> px(channels, 0.0);

      for (int row = start_h; row < end_h; row++) {
        for (int col = 0; col < width; col++) {
          int index = (row * width + col) * channels;

          std::fill(sum.begin(), sum.end(), 0.0);
          for (int k = -n; k <= n; k++) {
            int row_ = row;
            int col_ = col;
            if constexpr (A == Axis::Horizontal) {
              col_ = col + k;
            } else {
              row_ = row + k;
            }
            sample_pixel_fn(px, in_data, row_, col_, height, width, channels,
                            borderValue);

            auto kernel_val = kernel[k + n];
            for (int c = 0; c < channels; c++) {
              sum[c] += (px[c] * kernel_val);
            }
          }

          // store in temp buffer
          for (int c = 0; c < channels; c++) {
            out_data[index + c] = sum[c];
          }
        }
      }
    });
  }
}

void gaussian_blur(Image& output, const Image& input, ThreadPool& pool,
                   unsigned int num_threads, int kernel_size, float sigmaX,
                   float sigmaY, BorderMode mode, const double* borderValue) {
  if (sigmaY == 0.0) {
    sigmaY = sigmaX;
  }

  if (num_threads == 0) num_threads = 1;

  auto kernels = getGaussianKernels(kernel_size, sigmaX, sigmaY);

  auto height = input.getHeight();
  auto width = input.getWidth();
  auto channels = input.getChannels();
  auto input_data_ptr = input.getData();
  auto output_data_ptr = output.getDataMutable();

  // Allocate a temporary buffer to store intermediate results
  std::vector<double> temp_bufferX(height * width * channels);
  std::vector<double> temp_bufferY(height * width * channels);

  // horizontal pass
  if (num_threads == 1) {
    convolution1D_serial<Axis::Horizontal>(temp_bufferX.data(), input_data_ptr,
                                           kernels.kernelX, width, height,
                                           channels, mode, borderValue);
  } else {
    convolution1D<Axis::Horizontal>(temp_bufferX.data(), input_data_ptr,
                                    kernels.kernelX, width, height, channels,
                                    mode, borderValue, num_threads, pool);
    pool.waitAll();
  }

  // vertical pass
  if (num_threads == 1) {
    convolution1D_serial<Axis::Vertical>(
        temp_bufferY.data(), temp_bufferX.data(), kernels.kernelY, width,
        height, channels, mode, borderValue);
  } else {
    convolution1D<Axis::Vertical>(temp_bufferY.data(), temp_bufferX.data(),
                                  kernels.kernelY, width, height, channels,
                                  mode, borderValue, num_threads, pool);
    pool.waitAll();
  }

  // Convert double to uint8_t data
  if (num_threads == 1) {
    for (int idx = 0; idx < temp_bufferY.size(); idx++) {
      output_data_ptr[idx] = static_cast<uint8_t>(
          std::clamp(std::lround(temp_bufferY[idx]), 0L, 255L));
    }
  } else {
    int64_t total = temp_bufferY.size();
    int64_t chunk = (total + num_threads - 1) / num_threads;

    std::vector<std::thread> workers;
    for (int t = 0; t < num_threads; t++) {
      auto start = t * chunk;
      auto end = std::min((start + chunk), total);

      if (start >= end) break;

      pool.enqueue([=]() {
        for (int idx = start; idx < end; idx++) {
          output_data_ptr[idx] = static_cast<uint8_t>(
              std::clamp(std::lround(temp_bufferY[idx]), 0L, 255L));
        }
      });
    }

    // join all sub-threads with the main
    pool.waitAll();
  }
}

// ========= HELPER FUNCTIONS FOR SOBEL OPERATOR [START] ==========

// Helper function to calculate Binomial Coefficients: nCr
// Returns long long to avoid overflow during intermediate computation.
long long binomialCoefficient(int n, int r) {
  if (r < 0 || r > n) return 0;
  if (r == 0 || r == n) return 1;
  if (r > n / 2) r = n - r;  // Symmetry property

  long long res = 1;
  for (int i = 1; i <= r; ++i) {
    res = res * (n - i + 1) / i;
  }
  return res;
}

// Generates 1D Binomial Smoothing Vector of size N
// Uses long long to match binomialCoefficient's return type and avoid
// silent truncation/overflow for larger N.
std::vector<long long> generateSmoothingVector(int N) {
  std::vector<long long> S(N);
  for (int k = 0; k < N; ++k) {
    S[k] = binomialCoefficient(N - 1, k);
  }
  return S;
}

// Generates 1D Antisymmetric Differentiation Vector of size N
// Sign convention fixed to match standard Sobel: D[0] is positive,
// D[N-1] is negative (e.g. N=3 -> {-1, 0, 1} is what you get when this
// is used as [-1,0,1]; here we produce {1,0,-1}... see note below).
std::vector<long long> generateDifferentiationVector(int N) {
  if (N < 3) {
    throw std::invalid_argument("Differentiation vector requires N >= 3");
  }

  // Binomial row of size N-1 (i.e., S_{N-1}), sized exactly to what's filled.
  std::vector<long long> prev_binomial(N - 1);
  for (int k = 0; k < N - 1; ++k) {
    prev_binomial[k] = binomialCoefficient(N - 2, k);
  }

  std::vector<long long> D(N, 0);
  for (int k = 0; k < N; ++k) {
    long long left = (k > 0) ? prev_binomial[k - 1] : 0;
    long long right = (k < N - 1) ? prev_binomial[k] : 0;
    // Sign convention: left - right, so that D[0] > 0 and D[N-1] < 0,
    // matching the standard Sobel {-1, 0, 1} orientation for N=3.
    D[k] = left - right;
  }
  return D;
}

SobelKernels generateSobelKernels(int dx, int dy, int kernel_size) {
  if (dx == 0 && dy == 0) {
    throw std::invalid_argument("Both dx and dy can not be equal to 0");
  }

  auto S = generateSmoothingVector(kernel_size);
  auto D = generateDifferentiationVector(kernel_size);

  SobelKernels kernels;

  kernels.kernelX = (dx == 1) ? D : S;
  kernels.kernelY = (dy == 1) ? D : S;

  return kernels;
}

// ========= HELPER FUNCTIONS FOR SOBEL OPERATOR [END] ==========

template <typename TYPE>
void sobel(std::vector<TYPE>& output, const Image& input, ThreadPool& pool,
           unsigned int num_threads, int dx, int dy, int kernel_size,
           double scale, double delta, BorderMode mode,
           const double* borderValue) {
  auto height = input.getHeight();
  auto width = input.getWidth();
  auto channels = input.getChannels();
  auto input_data_ptr = input.getData();

  // Allocate a temporary buffer to store intermediate results
  std::vector<double> temp_buffer(height * width * channels);

  auto kernels = generateSobelKernels(dx, dy, kernel_size);

  std::vector<double> sum(channels, 0.0);
  std::vector<double> px(channels, 0.0);

  // horizontal pass
  if (num_threads == 1) {
    convolution1D_serial<Axis::Horizontal>(temp_buffer.data(), input_data_ptr,
                                           kernels.kernelX, width, height,
                                           channels, mode, borderValue);
  } else {
    convolution1D<Axis::Horizontal>(temp_buffer.data(), input_data_ptr,
                                    kernels.kernelX, width, height, channels,
                                    mode, borderValue, num_threads, pool);
    pool.waitAll();
  }

  // vertical pass
  if (num_threads == 1) {
    convolution1D_serial<Axis::Vertical>(output.data(), temp_buffer.data(),
                                         kernels.kernelY, width, height,
                                         channels, mode, borderValue);
  } else {
    convolution1D<Axis::Vertical>(output.data(), temp_buffer.data(),
                                  kernels.kernelY, width, height, channels,
                                  mode, borderValue, num_threads, pool);
    pool.waitAll();
  }

  // Apply scale and delta
  if (num_threads == 1) {
    for (int idx = 0; idx < output.size(); idx++) {
      output[idx] = std::fma(output[idx], scale, delta);
    }
  } else {
    int64_t total = output.size();
    int64_t chunk = (total + num_threads - 1) / num_threads;

    for (int t = 0; t < num_threads; t++) {
      auto start = t * chunk;
      auto end = std::min((start + chunk), total);

      if (start >= end) break;

      pool.enqueue([=, &output]() {
        for (int idx = start; idx < end; idx++) {
          output[idx] = std::fma(output[idx], scale, delta);
        }
      });
    }

    // join all sub-threads with the main
    pool.waitAll();
  }
}

template void sobel<double>(std::vector<double>& output, const Image& input,
                            ThreadPool& pool, unsigned int num_threads, int dx,
                            int dy, int kernel_size, double scale, double delta,
                            BorderMode mode, const double* borderValue);
