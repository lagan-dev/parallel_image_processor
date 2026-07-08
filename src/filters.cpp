#include "../include/filters.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <vector>

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec) {
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

void grayscale(Image &dst, const Image &src) {
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
    for (auto &val : kernel) {
      val = (val / sum);
    }
  }

  return kernel;
}

int get_bounded_index(int h, int w, int height, int width, int channels,
                      BorderMode mode) {
  auto get_reflect_idx = [](int k, int n, BorderMode mode) {
    int delta = (mode == BORDER_REFLECT);
    if (n == 1)
      return 0;

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
    if (h < 0)
      h += height;

    w = w % width;
    if (w < 0)
      w += width;

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
void sample_pixel(std::vector<T> &px, const T *data, int h, int w, int height,
                  int width, int channels, BorderMode mode,
                  const double *borderValue = nullptr) {
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

void gaussian_blur(Image &output, const Image &input, int kernel_size,
                   float sigmaX, float sigmaY, BorderMode mode,
                   const double *borderValue) {
  if (sigmaY == 0.0) {
    sigmaY = sigmaX;
  }

  auto kernelX = get_1d_gaussian_kernel(kernel_size, sigmaX);
  int n = kernelX.size() / 2;

  auto height = input.getHeight();
  auto width = input.getWidth();
  auto channels = input.getChannels();
  auto input_data_ptr = input.getData();
  auto output_data_ptr = output.getDataMutable();

  // Allocate a temporary buffer to store intermediate results
  std::vector<double> temp_buffer(height * width * channels);

  // horizontal pass
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      int index = (row * width + col) * channels;
      std::vector<double> sum(channels, 0.0);

      for (int k = -n; k <= n; k++) {
        std::vector<uint8_t> px(channels, 0);
        sample_pixel(px, input_data_ptr, row, col + k, height, width, channels,
                     mode, borderValue);

        auto kernel_val = kernelX[k + n];
        for (int c = 0; c < channels; c++) {
          sum[c] += (px[c] * kernel_val);
        }
      }

      // store in temp buffer
      for (int c = 0; c < channels; c++) {
        temp_buffer[index + c] = sum[c];
      }
    }
  }

  auto kernelY = get_1d_gaussian_kernel(kernel_size, sigmaY);
  n = kernelY.size() / 2;

  // vertical pass
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      int index = (row * width + col) * channels;
      std::vector<double> sum(channels, 0.0);

      for (int k = -n; k <= n; k++) {
        std::vector<double> px(channels, 0);
        sample_pixel(px, temp_buffer.data(), row + k, col, height, width,
                     channels, mode, borderValue);

        auto kernel_val = kernelY[k + n];
        for (int c = 0; c < channels; c++) {
          sum[c] += (px[c] * kernel_val);
        }
      }

      // store in output buffer
      for (int c = 0; c < channels; c++) {
        output_data_ptr[index + c] =
            static_cast<uint8_t>(std::clamp(std::lround(sum[c]), 0L, 255L));
      }
    }
  }
}
