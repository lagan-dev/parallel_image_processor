#include <filters.h>
#include <gtest/gtest.h>
#include <image.h>

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

std::vector<uint8_t> ReadImageBytes(const Image& image) {
  return std::vector<uint8_t>(
      image.getData(), image.getData() + image.getWidth() * image.getHeight() *
                                             image.getChannels());
}

std::vector<uint8_t> RunFilter(const std::vector<uint8_t>& input, int width,
                               int height, int channels, int kernel_size,
                               float sigmaX, float sigmaY) {
  Image src(width, height, channels);
  CopyToImage(src, input);
  Image dst(width, height, channels);
  gaussian_blur(dst, src, kernel_size, sigmaX, sigmaY, BORDER_CLAMP);
  return ReadImageBytes(dst);
}

std::vector<uint8_t> RunOpenCvBlur(const std::vector<uint8_t>& input, int width,
                                   int height, int channels, int kernel_size,
                                   float sigmaX, float sigmaY) {
  cv::Mat src(height, width, CV_8UC3, const_cast<uint8_t*>(input.data()));
  cv::Mat src32;
  src.convertTo(src32, CV_32FC3);

  cv::Mat dst;
  cv::GaussianBlur(src32, dst, cv::Size(kernel_size, kernel_size), sigmaX,
                   sigmaY, cv::BORDER_REPLICATE);

  cv::Mat dst8;
  dst.convertTo(dst8, CV_8UC3);

  std::vector<uint8_t> output(dst8.total() * dst8.channels());
  std::memcpy(output.data(), dst8.data, output.size());
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

void CompareToOpenCv(const std::vector<uint8_t>& output,
                     const std::vector<uint8_t>& reference) {
  ASSERT_EQ(output.size(), reference.size());

  constexpr int kMaxDiff = 1;
  constexpr size_t kMaxErrors = 10;

  size_t errors = 0;
  for (size_t i = 0; i < output.size(); ++i) {
    const int diff =
        std::abs(static_cast<int>(output[i]) - static_cast<int>(reference[i]));
    if (diff <= kMaxDiff) continue;

    std::printf("  [idx=%zu] ref: %3d  actual: %3d  diff: %d\n", i,
                static_cast<int>(reference[i]), static_cast<int>(output[i]),
                diff);

    if (++errors > kMaxErrors) break;
  }

  EXPECT_EQ(errors, 0);
}

}  // namespace

TEST(GaussianBlurTest, MatchesOpenCvGaussianBlur) {
  constexpr int width = 5;
  constexpr int height = 5;
  constexpr int channels = 3;

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

  const std::vector<uint8_t> output =
      RunFilter(image, width, height, channels, 3, 1.0f, 1.2f);
  const std::vector<uint8_t> reference =
      RunOpenCvBlur(image, width, height, channels, 3, 1.0f, 1.2f);

  CompareToOpenCv(output, reference);
}

TEST(GaussianBlurTest, MatchesOpenCvGaussianBlurWithRandomShapes) {
  constexpr int channels = 3;
  std::mt19937 rng(98765u);

  const std::array<std::pair<int, int>, 6> shapes = {
      std::make_pair(1, 1),   std::make_pair(5, 7),   std::make_pair(16, 9),
      std::make_pair(32, 32), std::make_pair(63, 45), std::make_pair(128, 77),
  };

  for (const auto& shape : shapes) {
    const int width = shape.first;
    const int height = shape.second;
    std::vector<uint8_t> image =
        GenerateRandomImage(width, height, channels, rng);

    const std::vector<uint8_t> output =
        RunFilter(image, width, height, channels, 3, 1.0f, 1.2f);
    const std::vector<uint8_t> reference =
        RunOpenCvBlur(image, width, height, channels, 3, 1.0f, 1.2f);

    CompareToOpenCv(output, reference);
  }
}

namespace {

std::vector<uint8_t> RunFilterWithParams(const std::vector<uint8_t>& input,
                                         int width, int height, int channels,
                                         int kernel_size, float sigmaX,
                                         float sigmaY, BorderMode mode) {
  Image src(width, height, channels);
  CopyToImage(src, input);
  Image dst(width, height, channels);
  gaussian_blur(dst, src, kernel_size, sigmaX, sigmaY, mode);
  return ReadImageBytes(dst);
}

std::vector<uint8_t> RunOpenCvBlurWithParams(const std::vector<uint8_t>& input,
                                             int width, int height,
                                             int channels, int kernel_size,
                                             float sigmaX, float sigmaY,
                                             BorderMode mode) {
  cv::Mat src(height, width, CV_8UC3, const_cast<uint8_t*>(input.data()));

  cv::Mat src32;
  src.convertTo(src32, CV_32FC3);

  cv::Mat dst;

  if (mode != BORDER_WRAP) {
    cv::GaussianBlur(src32, dst, cv::Size(kernel_size, kernel_size), sigmaX,
                     sigmaY, ToCvBorderType(mode));
  } else {
    const int radius = kernel_size / 2;

    cv::Mat padded;
    cv::copyMakeBorder(src32, padded, radius, radius, radius, radius,
                       cv::BORDER_WRAP);

    cv::Mat blurred;
    cv::GaussianBlur(padded, blurred, cv::Size(kernel_size, kernel_size),
                     sigmaX, sigmaY, cv::BORDER_CONSTANT);

    dst = blurred(cv::Rect(radius, radius, width, height)).clone();
  }

  cv::Mat dst8;
  dst.convertTo(dst8, CV_8UC3);

  std::vector<uint8_t> output(dst8.total() * dst8.channels());
  std::memcpy(output.data(), dst8.data, output.size());

  return output;
}

void ExpectMatchesOpenCvForBorderMode(BorderMode mode) {
  constexpr int width = 7;
  constexpr int height = 5;
  constexpr int channels = 3;
  constexpr int kernel_size = 3;
  constexpr float sigmaX = 1.0f;
  constexpr float sigmaY = 1.5f;

  std::vector<uint8_t> image(width * height * channels);
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      const int index = (row * width + col) * channels;
      image[index + 0] = static_cast<uint8_t>((row * 23 + col * 17) % 256);
      image[index + 1] = static_cast<uint8_t>((row * 41 + col * 13) % 256);
      image[index + 2] = static_cast<uint8_t>((row * 67 + col * 29) % 256);
    }
  }

  const std::vector<uint8_t> output = RunFilterWithParams(
      image, width, height, channels, kernel_size, sigmaX, sigmaY, mode);
  const std::vector<uint8_t> reference = RunOpenCvBlurWithParams(
      image, width, height, channels, kernel_size, sigmaX, sigmaY, mode);

  CompareToOpenCv(output, reference);
}

}  // namespace

TEST(GaussianBlurTest, Clamp) {
  ExpectMatchesOpenCvForBorderMode(BORDER_CLAMP);
}

TEST(GaussianBlurTest, Reflect) {
  ExpectMatchesOpenCvForBorderMode(BORDER_REFLECT);
}

TEST(GaussianBlurTest, Mirror) {
  ExpectMatchesOpenCvForBorderMode(BORDER_MIRROR);
}

TEST(GaussianBlurTest, Wrap) {
  ExpectMatchesOpenCvForBorderMode(BORDER_WRAP);
}

TEST(GaussianBlurTest, Constant) {
  ExpectMatchesOpenCvForBorderMode(BORDER_CONSTANT);
}

TEST(GaussianBlurTest, MatchesOpenCvGaussianBlurWithRandomParams) {
  constexpr int channels = 3;
  constexpr int kNumTrials = 100;

  std::mt19937 rng(42u);

  std::uniform_int_distribution<int> dim_dist(1, 200);

  std::uniform_int_distribution<int> kernel_idx_dist(0, 4);

  std::uniform_real_distribution<float> sigma_x_dist(0.5f, 3.0f);
  std::uniform_real_distribution<float> sigma_y_dist(0.5f, 3.0f);

  const std::vector<BorderMode> modes = {BORDER_CLAMP, BORDER_REFLECT,
                                         BORDER_MIRROR, BORDER_WRAP,
                                         BORDER_CONSTANT};

  for (const auto mode : modes) {
    for (int trial = 0; trial < kNumTrials; ++trial) {
      const int width = dim_dist(rng);
      const int height = dim_dist(rng);
      const int kernel_size = 3 + kernel_idx_dist(rng) * 2;
      const float sigmaX = sigma_x_dist(rng);
      const float sigmaY = sigma_y_dist(rng);

      std::cout << "[Trial " << std::setw(2) << trial << "] "
                << "width=" << std::setw(4) << width << " "
                << "height=" << std::setw(4) << height << " "
                << "kernel=" << std::setw(2) << kernel_size << " "
                << "sigmaX=" << std::fixed << std::setprecision(4) << sigmaX
                << " sigmaY=" << std::fixed << std::setprecision(4) << sigmaY
                << " mode=" << mode << "\n";

      std::vector<uint8_t> image =
          GenerateRandomImage(width, height, channels, rng);

      const std::vector<uint8_t> output = RunFilterWithParams(
          image, width, height, channels, kernel_size, sigmaX, sigmaY, mode);
      const std::vector<uint8_t> reference = RunOpenCvBlurWithParams(
          image, width, height, channels, kernel_size, sigmaX, sigmaY, mode);

      CompareToOpenCv(output, reference);
    }
  }
}

TEST(GaussianBlurTest, debug) {
  constexpr int width = 49;
  constexpr int height = 4;
  constexpr int channels = 3;
  constexpr int kernel_size = 9;
  constexpr float sigmaX = 1.7285f;
  constexpr float sigmaY = 0.9f;
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

  const std::vector<uint8_t> output = RunFilterWithParams(
      image, width, height, channels, kernel_size, sigmaX, sigmaY, mode);
  const std::vector<uint8_t> reference = RunOpenCvBlurWithParams(
      image, width, height, channels, kernel_size, sigmaX, sigmaY, mode);

  CompareToOpenCv(output, reference);
}
