#include <filters.h>
#include <gtest/gtest.h>
#include <image.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <random>
#include <utility>
#include <vector>

#include "threadpool.h"
#define PRINT_LIMIT 10

namespace {

std::vector<uint8_t> ReferenceGrayscale(const std::vector<uint8_t>& input,
                                        int width, int height, int channels) {
  cv::Mat src(height, width, channels == 4 ? CV_8UC4 : CV_8UC3,
              const_cast<uint8_t*>(input.data()));

  cv::Mat gray;
  cv::cvtColor(src, gray,
               channels == 4 ? cv::COLOR_RGBA2GRAY : cv::COLOR_RGB2GRAY);

  std::vector<uint8_t> output(width * height);

  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      output[row * width + col] = gray.at<uchar>(row, col);
    }
  }

  return output;
}

std::vector<uint8_t> GenerateRandomImage(int width, int height, int channels,
                                         std::mt19937& rng) {
  std::vector<uint8_t> image(width * height * channels);
  std::uniform_int_distribution<int> dist(0, 255);

  for (uint8_t& value : image) {
    value = static_cast<uint8_t>(dist(rng));
  }

  return image;
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

void CompareToOpenCv(const std::vector<uint8_t>& output,
                     const std::vector<uint8_t>& reference) {
  ASSERT_EQ(output.size(), reference.size());

  size_t errors = 0;
  for (size_t i = 0; i < output.size(); ++i) {
    const int diff =
        std::abs(static_cast<int>(output[i]) - static_cast<int>(reference[i]));
    if (diff > 0) {
      ++errors;

      if (errors <= PRINT_LIMIT) {
        std::cout << "Mismatch at index " << i
                  << ": output = " << static_cast<int>(output[i])
                  << ", reference = " << static_cast<int>(reference[i])
                  << ", diff = " << diff << std::endl;
      }
    }
  }

  EXPECT_EQ(errors, 0);
}

}  // namespace

TEST(GrayscaleTest, MatchesOpenCvGrayscaleReference) {
  std::vector<uint8_t> image = {
      255, 0,   0,    // red
      0,   255, 0,    // green
      0,   0,   255,  // blue
      64,  128, 192   // mixed
  };

  Image src(2, 2, 3);
  CopyToImage(src, image);

  Image dst(2, 2, 1);
  ThreadPool pool(4);
  grayscale(dst, src, pool, 4);

  const std::vector<uint8_t> reference = ReferenceGrayscale(image, 2, 2, 3);
  const std::vector<uint8_t> output = ReadImageBytes(dst);

  CompareToOpenCv(output, reference);
}

TEST(GrayscaleTest, ProducesSingleChannelOutputForRgbaInput) {
  std::vector<uint8_t> image = {
      10, 20, 30, 40, 50, 60, 70, 80,
  };

  Image src(2, 1, 4);
  CopyToImage(src, image);

  Image dst(2, 1, 1);
  ThreadPool pool(4);
  grayscale(dst, src, pool, 4);

  const std::vector<uint8_t> reference = ReferenceGrayscale(image, 2, 1, 4);
  const std::vector<uint8_t> output = ReadImageBytes(dst);

  CompareToOpenCv(output, reference);
}

TEST(GrayscaleTest, MatchesOpenCvGrayscaleReferenceWithRandomShapes) {
  constexpr int channels = 3;
  std::mt19937 rng(42u);

  const std::array<std::pair<int, int>, 6> shapes = {
      std::make_pair(1, 1),   std::make_pair(5, 7),   std::make_pair(16, 9),
      std::make_pair(32, 32), std::make_pair(63, 45), std::make_pair(128, 77),
  };

  for (const auto& shape : shapes) {
    const int width = shape.first;
    const int height = shape.second;

    std::vector<uint8_t> image =
        GenerateRandomImage(width, height, channels, rng);

    Image src(width, height, channels);
    CopyToImage(src, image);

    Image dst(width, height, 1);
    ThreadPool pool(4);
    grayscale(dst, src, pool, 4);

    const std::vector<uint8_t> reference =
        ReferenceGrayscale(image, width, height, channels);
    const std::vector<uint8_t> output = ReadImageBytes(dst);

    CompareToOpenCv(output, reference);
  }
}

TEST(GrayscaleTest, RoundsRatherThanTruncatesWeightedSum) {
  // Pixels chosen so the weighted sum's fractional part is >= 0.5,
  // meaning truncation and correct rounding disagree by exactly 1.
  // yellow (255,255,0):  0.299*255 + 0.587*255 + 0.114*0   = 225.93 -> round 226, truncate 225
  // cyan   (0,255,255):  0.587*255 + 0.114*255             = 178.755 -> round 179, truncate 178
  std::vector<uint8_t> image = {
      255, 255, 0,    // yellow
      0,   255, 255,  // cyan
  };

  Image src(2, 1, 3);
  CopyToImage(src, image);

  Image dst(2, 1, 1);
  ThreadPool pool(4);
  grayscale(dst, src, pool, 4);

  const std::vector<uint8_t> output = ReadImageBytes(dst);

  // Exact match, not CompareToOpenCv's tolerant (diff > 1) check —
  // that tolerance is wide enough to hide a truncate-vs-round bug,
  // since the two can only ever differ by exactly 1.
  EXPECT_EQ(output[0], 226) << "yellow pixel: expected rounded value 226, "
                               "got " << static_cast<int>(output[0])
                            << " (looks truncated, not rounded)";
  EXPECT_EQ(output[1], 179) << "cyan pixel: expected rounded value 179, "
                               "got " << static_cast<int>(output[1])
                            << " (looks truncated, not rounded)";
}