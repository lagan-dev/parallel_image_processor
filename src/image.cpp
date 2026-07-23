#include "../include/image.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

// Provide stb implementation in this translation unit only
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <iostream>

#include "../stb/stb_image.h"
#include "../stb/stb_image_write.h"

Image::Image() : width(0), height(0), channels(0), data(nullptr) {}

Image::Image(int width_, int height_, int channels_)
    : width(width_), height(height_), channels(channels_) {
  size_t size = static_cast<size_t>(width_) * height_ * channels_;
  data = static_cast<uint8_t*>(STBI_MALLOC(size));

  if (!data) {
    throw std::bad_alloc();
  }
  std::memset(data, static_cast<uint8_t>(0), size);
}

Image::Image(int width_, int height_, int channels_, uint8_t* data_)
    : width(width_), height(height_), channels(channels_), data(data_) {}

Image::Image(Image&& other) noexcept
    : width(other.width),
      height(other.height),
      channels(other.channels),
      data(other.data),
      extension(std::move(other.extension)) {
  other.data = nullptr;
  other.width = 0;
  other.height = 0;
  other.channels = 0;
}

Image& Image::operator=(Image&& other) noexcept {
  if (this != &other) {
    clear();

    width = other.width;
    height = other.height;
    channels = other.channels;
    data = other.data;
    extension = std::move(other.extension);

    other.data = nullptr;
    other.width = 0;
    other.height = 0;
    other.channels = 0;
  }

  return *this;
}

Image::~Image() {
  clear();
}

void Image::clear() {
  if (data) {
    stbi_image_free(data);
    data = nullptr;
  }
}

bool Image::load(const std::string& path) {
  clear();
  data = nullptr;

  data = stbi_load(path.c_str(), &width, &height, &channels, 0);
  if (!data) {
    std::cerr << "stbi_load failed: " << stbi_failure_reason() << "\n";
  }

  extension = get_extension(path);

  return (data != nullptr);
}

bool Image::save(const std::string& path, const int quality) {
  int status = 0;
  auto filename = path.c_str();
  std::string out_extension = get_extension(path);
  if (out_extension.empty()) {
    out_extension = extension;
  } else {
    extension = out_extension;
  }

  if (out_extension == "jpg" || out_extension == "jpeg") {
    status = stbi_write_jpg(filename, width, height, channels, data, quality);
  } else if (out_extension == "png") {
    status = stbi_write_png(filename, width, height, channels, data,
                            width * channels);
  } else if (out_extension == "bmp") {
    status = stbi_write_bmp(filename, width, height, channels, data);
  } else if (out_extension == "tga") {
    status = stbi_write_tga(filename, width, height, channels, data);
    //   } else if (out_extension == "hdr") {
    //     status = stbi_write_hdr(filename, width, height, channels, data);
  } else {
    std::cout << "Unsupported image type: [" << out_extension << "]"
              << std::endl;
  }

  return (status != 0);
}

void Image::printImageInfo() const {
  std::cout << "Width: " << width << ", Height: " << height
            << ", Channels: " << channels << std::endl;
}

std::string Image::get_extension(const std::string& file) {
  // Convert to lowercase
  auto file_ = file;
  std::transform(file_.begin(), file_.end(), file_.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  size_t p = file_.rfind('.');
  if (p == std::string::npos || p == file_.size() - 1) return "";
  return file_.substr(p + 1);
}
