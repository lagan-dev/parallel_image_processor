#include "../include/filters.h"
#include <sys/types.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
  os << "[";

  for (std::size_t i = 0; i < vec.size(); i++) {
    os << vec[i];
    if (i + 1 < vec.size()) {
      os << ", ";
    }
  }

  os << "]\n";
  return os;
}

void grayscale(Image& dst, const Image& src) {
  // Grayscale = (0.299 × R) + (0.587 × G) + (0.114 × B)

  auto height = src.getHeight();
  auto width = src.getWidth();
  auto channels = src.getChannels();
  auto input_data = src.getData();

  auto out_data = dst.getDataMutable();

  for (int idx = 0; idx < width * height; idx++) {
    auto px = input_data + (idx * channels);

    // Get RGB values
    unsigned char r = px[0];
    unsigned char g = px[1];
    unsigned char b = px[2];

    // Calc grayscale values
    unsigned char gray = (0.299 * r) + (0.587 * g) + (0.114 * b);

    // Store output values
    out_data[idx] = gray;
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

int get_bounded_index(int h, int w, int height, int width, int channels,
                      BorderMode mode) {
  auto get_reflect_idx = [](int k, int n, BorderMode mode) {
    int delta = (mode == BORDER_REFLECT);
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

  switch (mode) {
  case BORDER_CLAMP: {
    h = std::clamp(h, 0, height - 1);
    w = std::clamp(w, 0, width - 1);

    break;
  }

  case BORDER_WRAP: {
    h = h % height;
    if (h < 0) h += height;

    w = w % width;
    if (w < 0) w += width;

    break;
  }

  case BORDER_REFLECT:
  case BORDER_MIRROR: {
    h = get_reflect_idx(h, height, mode);
    w = get_reflect_idx(w, width, mode);

    break;
  }

  default:
    std::cout << "BorderMode not supported!!" << std::endl;
  }

  return (h * width + w) * channels;
}

template <typename T>
void sample_pixel(std::vector<double>& px, const T* data, int h, int w,
                  int height, int width, int channels, BorderMode mode,
                  const double* borderValue = nullptr) {
  if (mode == BORDER_CONSTANT) {
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
    auto idx = get_bounded_index(h, w, height, width, channels, mode);

    for (int c = 0; c < channels; c++) {
      px[c] = data[idx + c];
    }
  }
}

template <Axis A, typename IN_T, typename KRNL_T>
void convolution1D(double* out_data, IN_T* in_data,
                   const std::vector<KRNL_T>& kernel, int width, int height,
                   int channels, BorderMode mode, const double* borderValue) {
  std::vector<double> sum(channels, 0.0);
  std::vector<double> px(channels, 0.0);
  int n = kernel.size() / 2;

  for (int row = 0; row < height; row++) {
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
        sample_pixel(px, in_data, row_, col_, height, width, channels, mode,
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
}

void gaussian_blur(Image& output, const Image& input, int kernel_size,
                   float sigmaX, float sigmaY, BorderMode mode,
                   const double* borderValue) {
  if (sigmaY == 0.0) {
    sigmaY = sigmaX;
  }

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
  convolution1D<Axis::Horizontal>(temp_bufferX.data(), input_data_ptr,
                                  kernels.kernelX, width, height, channels,
                                  mode, borderValue);

  // vertical pass
  convolution1D<Axis::Vertical>(temp_bufferY.data(), temp_bufferX.data(),
                                kernels.kernelY, width, height, channels, mode,
                                borderValue);

  // Convert double to uint8_t data
  for (int idx = 0; idx < temp_bufferY.size(); idx++) {
    output_data_ptr[idx] = static_cast<uint8_t>(
        std::clamp(std::lround(temp_bufferY[idx]), 0L, 255L));
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
void sobel(std::vector<TYPE>& output, const Image& input, int dx, int dy,
           int kernel_size, double scale, double delta, BorderMode mode,
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
  convolution1D<Axis::Horizontal>(temp_buffer.data(), input_data_ptr,
                                  kernels.kernelX, width, height, channels,
                                  mode, borderValue);

  // vertical pass
  convolution1D<Axis::Vertical>(output.data(), temp_buffer.data(),
                                kernels.kernelY, width, height, channels, mode,
                                borderValue);

  // Apply scale and delta
  for (int idx = 0; idx < output.size(); idx++) {
    output[idx] = output[idx] * scale + delta;
  }
}

template void sobel<double>(std::vector<double>& output, const Image& input,
                            int dx, int dy, int kernel_size, double scale,
                            double delta, BorderMode mode,
                            const double* borderValue);
