#include <filters.h>
#include <gtest/gtest.h>
#include <image.h>
#include <opencv2/core/hal/interface.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

cv::BorderTypes ToCvBorderType(BorderMode mode) {
  switch (mode) {
  case BORDER_CLAMP:
    return cv::BORDER_REPLICATE;
  case BORDER_REFLECT:
    return cv::BORDER_REFLECT_101;
  case BORDER_MIRROR:
    return cv::BORDER_REFLECT;
  case BORDER_WRAP:
    return cv::BORDER_WRAP;
  case BORDER_CONSTANT:
    return cv::BORDER_CONSTANT;
  }
  throw std::invalid_argument("Unknown BorderMode");
}

void CopyToImage(Image& image, const std::vector<uint8_t>& input) {
  ASSERT_EQ(static_cast<int>(input.size()),
            image.getWidth() * image.getHeight() * image.getChannels());
  std::memcpy(image.getDataMutable(), input.data(), input.size());
}

std::vector<double> RunFilter(const std::vector<uint8_t>& input, int width,
                              int height, int channels, int dx, int dy,
                              int kernel_size, double scale, double delta,
                              BorderMode mode) {
  Image src(width, height, channels);
  CopyToImage(src, input);
  std::vector<double> output(width * height * channels);
  ThreadPool pool(4);
  sobel<double>(output, src, pool, 4, dx, dy, kernel_size, scale, delta, mode);
  return output;
}

cv::Mat RunOpenCvSobel(const cv::Mat& src32, int dx, int dy, int kernel_size,
                       double scale, double delta, BorderMode mode) {
  const int ksize = kernel_size;
  cv::Mat dst;

  if (mode != BORDER_WRAP) {
    cv::Sobel(src32, dst, CV_32F, dx, dy, ksize, scale, delta,
              ToCvBorderType(mode));
  } else {
    const int radius = kernel_size / 2;

    cv::Mat padded;
    cv::copyMakeBorder(src32, padded, radius, radius, radius, radius,
                       cv::BORDER_WRAP);

    cv::Mat grad;
    cv::Sobel(padded, grad, CV_32F, dx, dy, ksize, scale, delta,
              cv::BORDER_CONSTANT);

    dst = grad(cv::Rect(radius, radius, src32.cols, src32.rows)).clone();
  }

  return dst;
}

std::vector<float> RunOpenCvSobel(const std::vector<uint8_t>& input, int width,
                                  int height, int channels, int dx, int dy,
                                  int kernel_size, double scale, double delta,
                                  BorderMode mode) {
  cv::Mat src(height, width, CV_8UC3, const_cast<uint8_t*>(input.data()));

  cv::Mat src32;
  src.convertTo(src32, CV_32FC3);

  const cv::Mat dst =
      RunOpenCvSobel(src32, dx, dy, kernel_size, scale, delta, mode);

  std::vector<float> output(dst.total() * dst.channels());
  std::memcpy(output.data(), dst.data, output.size() * sizeof(float));
  return output;
}

void FillRandomImage(std::vector<uint8_t>& image, int width, int height,
                     int channels, std::mt19937& rng) {
  std::uniform_int_distribution<int> dist(0, 255);

  for (uint8_t& value : image) {
    value = static_cast<uint8_t>(dist(rng));
  }
}

std::vector<uint8_t> GenerateRandomImage(int width, int height, int channels,
                                         std::mt19937& rng) {
  std::vector<uint8_t> image(width * height * channels);
  FillRandomImage(image, width, height, channels, rng);
  return image;
}

void CompareToOpenCv(const std::vector<double>& output,
                     const std::vector<float>& reference) {
  ASSERT_EQ(output.size(), reference.size());

  constexpr double kAbsTol = 1e-3;
  constexpr double kRelTol = 1e-5;
  constexpr size_t kMaxErrors = 10;

  double max_abs = 0.0;
  for (const float value : reference)
    max_abs = std::max(max_abs, std::abs(static_cast<double>(value)));

  const double tol = kAbsTol + kRelTol * max_abs;

  size_t errors = 0;
  for (size_t i = 0; i < output.size(); ++i) {
    const double diff = std::abs(output[i] - static_cast<double>(reference[i]));
    if (diff <= tol) continue;

    std::printf("  [idx=%zu] ref: %f  actual: %f  diff: %f\n", i,
                static_cast<double>(reference[i]), output[i], diff);

    if (++errors > kMaxErrors) break;
  }

  EXPECT_EQ(errors, 0);
}

}  // namespace

TEST(SobelTest, MatchesOpenCvSobelGradientX) {
  constexpr int width = 5;
  constexpr int height = 5;
  constexpr int channels = 3;
  constexpr int kernel_size = 5;
  constexpr double scale = 1.0;
  constexpr double delta = 0.0;

  std::vector<uint8_t> image(width * height * channels, 0);
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      const int index = (row * width + col) * channels;
      if (row == 2 && col == 2) {
        image[index + 0] = 255;
        image[index + 1] = 255;
        image[index + 2] = 255;
      } else if (row >= 1 && row <= 3 && col >= 1 && col <= 3) {
        image[index + 0] = 128;
        image[index + 1] = 64;
        image[index + 2] = 32;
      }
    }
  }

  const std::vector<double> output =
      RunFilter(image, width, height, channels, 1, 0, kernel_size, scale, delta,
                BORDER_REFLECT);
  const std::vector<float> reference =
      RunOpenCvSobel(image, width, height, channels, 1, 0, kernel_size, scale,
                     delta, BORDER_REFLECT);

  CompareToOpenCv(output, reference);
}

TEST(SobelTest, MatchesOpenCvSobelGradientY) {
  constexpr int width = 5;
  constexpr int height = 5;
  constexpr int channels = 3;
  constexpr int kernel_size = 5;
  constexpr double scale = 1.0;
  constexpr double delta = 0.0;

  std::vector<uint8_t> image(width * height * channels, 0);
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      const int index = (row * width + col) * channels;
      if (row == 2 && col == 2) {
        image[index + 0] = 255;
        image[index + 1] = 255;
        image[index + 2] = 255;
      } else if (row >= 1 && row <= 3 && col >= 1 && col <= 3) {
        image[index + 0] = 128;
        image[index + 1] = 64;
        image[index + 2] = 32;
      }
    }
  }

  const std::vector<double> output =
      RunFilter(image, width, height, channels, 0, 1, kernel_size, scale, delta,
                BORDER_REFLECT);
  const std::vector<float> reference =
      RunOpenCvSobel(image, width, height, channels, 0, 1, kernel_size, scale,
                     delta, BORDER_REFLECT);

  CompareToOpenCv(output, reference);
}

TEST(SobelTest, MatchesOpenCvSobelWithRandomShapes) {
  constexpr int channels = 3;
  constexpr int kernel_size = 5;
  std::mt19937 rng(13579u);

  const std::array<std::pair<int, int>, 6> shapes = {
      std::make_pair(1, 1),   std::make_pair(5, 7),   std::make_pair(16, 9),
      std::make_pair(32, 32), std::make_pair(63, 45), std::make_pair(128, 77),
  };

  for (const auto& shape : shapes) {
    const int width = shape.first;
    const int height = shape.second;
    std::vector<uint8_t> image =
        GenerateRandomImage(width, height, channels, rng);

    for (const auto& order : {std::make_pair(1, 0), std::make_pair(0, 1)}) {
      const int dx = order.first;
      const int dy = order.second;

      const std::vector<double> output =
          RunFilter(image, width, height, channels, dx, dy, kernel_size, 1.0,
                    0.0, BORDER_REFLECT);
      const std::vector<float> reference =
          RunOpenCvSobel(image, width, height, channels, dx, dy, kernel_size,
                         1.0, 0.0, BORDER_REFLECT);

      CompareToOpenCv(output, reference);
    }
  }
}

namespace {

void ExpectMatchesOpenCvForBorderMode(BorderMode mode) {
  constexpr int width = 7;
  constexpr int height = 5;
  constexpr int channels = 3;
  constexpr int kernel_size = 5;
  constexpr double scale = 1.0;
  constexpr double delta = 0.0;

  std::vector<uint8_t> image(width * height * channels);
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      const int index = (row * width + col) * channels;
      image[index + 0] = static_cast<uint8_t>((row * 23 + col * 17) % 256);
      image[index + 1] = static_cast<uint8_t>((row * 41 + col * 13) % 256);
      image[index + 2] = static_cast<uint8_t>((row * 67 + col * 29) % 256);
    }
  }

  for (const auto& order : {std::make_pair(1, 0), std::make_pair(0, 1)}) {
    const int dx = order.first;
    const int dy = order.second;

    const std::vector<double> output =
        RunFilter(image, width, height, channels, dx, dy, kernel_size, scale,
                  delta, mode);
    const std::vector<float> reference =
        RunOpenCvSobel(image, width, height, channels, dx, dy, kernel_size,
                       scale, delta, mode);

    CompareToOpenCv(output, reference);
  }
}

}  // namespace

TEST(SobelTest, Clamp) {
  ExpectMatchesOpenCvForBorderMode(BORDER_CLAMP);
}

TEST(SobelTest, Reflect) {
  ExpectMatchesOpenCvForBorderMode(BORDER_REFLECT);
}

TEST(SobelTest, Mirror) {
  ExpectMatchesOpenCvForBorderMode(BORDER_MIRROR);
}

TEST(SobelTest, Wrap) {
  ExpectMatchesOpenCvForBorderMode(BORDER_WRAP);
}

TEST(SobelTest, Constant) {
  ExpectMatchesOpenCvForBorderMode(BORDER_CONSTANT);
}

TEST(SobelTest, MatchesOpenCvSobelWithRandomParams) {
  constexpr int channels = 3;
  constexpr int kNumTrials = 100;

  std::mt19937 rng(42u);

  std::uniform_int_distribution<int> dim_dist(1, 200);
  std::uniform_int_distribution<int> kernel_idx_dist(0, 4);
  std::uniform_real_distribution<double> scale_dist(0.5, 2.0);
  std::uniform_real_distribution<double> delta_dist(0.0, 20.0);

  const std::vector<std::pair<int, int>> orders = {{1, 0}, {0, 1}};
  const std::vector<BorderMode> modes = {BORDER_CLAMP, BORDER_REFLECT,
                                         BORDER_MIRROR, BORDER_WRAP,
                                         BORDER_CONSTANT};

  for (const auto mode : modes) {
    for (int trial = 0; trial < kNumTrials; ++trial) {
      const int width = dim_dist(rng);
      const int height = dim_dist(rng);
      const int kernel_size = 5 + kernel_idx_dist(rng) * 2;
      const double scale = scale_dist(rng);
      const double delta = delta_dist(rng);
      const auto order = orders[rng() % orders.size()];
      const int dx = order.first;
      const int dy = order.second;

      std::cout << "[Trial " << std::setw(2) << trial << "] "
                << "width=" << std::setw(4) << width << " "
                << "height=" << std::setw(4) << height << " "
                << "kernel=" << std::setw(2) << kernel_size << " "
                << "scale=" << std::fixed << std::setprecision(4) << scale
                << " delta=" << std::fixed << std::setprecision(4) << delta
                << " dx=" << dx << " dy=" << dy << " mode=" << mode << "\n";

      std::vector<uint8_t> image =
          GenerateRandomImage(width, height, channels, rng);

      const std::vector<double> output =
          RunFilter(image, width, height, channels, dx, dy, kernel_size, scale,
                    delta, mode);
      const std::vector<float> reference =
          RunOpenCvSobel(image, width, height, channels, dx, dy, kernel_size,
                         scale, delta, mode);

      CompareToOpenCv(output, reference);
    }
  }
}

TEST(SobelTest, debug) {
  constexpr int width = 49;
  constexpr int height = 4;
  constexpr int channels = 3;
  constexpr int kernel_size = 9;
  constexpr double scale = 1.0;
  constexpr double delta = 0.0;
  BorderMode mode = BORDER_REFLECT;

  std::vector<uint8_t> image(width * height * channels);
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      const int index = (row * width + col) * channels;
      image[index + 0] = static_cast<uint8_t>((row * 23 + col * 17) % 256);
      image[index + 1] = static_cast<uint8_t>((row * 41 + col * 13) % 256);
      image[index + 2] = static_cast<uint8_t>((row * 67 + col * 29) % 256);
    }
  }

  const std::vector<double> output = RunFilter(
      image, width, height, channels, 1, 0, kernel_size, scale, delta, mode);
  const std::vector<float> reference = RunOpenCvSobel(
      image, width, height, channels, 1, 0, kernel_size, scale, delta, mode);

  CompareToOpenCv(output, reference);
}
