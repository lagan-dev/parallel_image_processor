#include "../include/image.h"

#include <cstdint>

// Provide stb implementation in this translation unit only
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <iostream>

#include "../stb/stb_image.h"
#include "../stb/stb_image_write.h"

Image::Image() {}

Image::Image(int width_, int height_, int channels_)
    : width(width_), height(height_), channels(channels_) {
  size_t size = static_cast<size_t>(width_) * height_ * channels_;
  data = static_cast<uint8_t *>(std::calloc(size, sizeof(uint8_t)));
  if (!data) {
    throw std::bad_alloc();
  }
}

Image::Image(int width_, int height_, int channels_, uint8_t *data_)
    : width(width_), height(height_), channels(channels_), data(data_) {}

Image::~Image() { std::free(data); }

bool Image::load(const std::string &path) {
  std::free(data);
  data = nullptr;

  data = stbi_load(path.c_str(), &width, &height, &channels, 0);
  if (!data) {
    std::cerr << "stbi_load failed: " << stbi_failure_reason() << "\n";
  }

  extension = get_extension(path);

  return (data != nullptr);
}

bool Image::save(const std::string &path, const int quality) {
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

void Image::free() {
  if (data) {
    stbi_image_free(data);
    data = nullptr;
  }
}

void Image::printImageInfo() const {
  std::cout << "Width: " << width << ", Height: " << height
            << ", Channels: " << channels << std::endl;
}

std::string Image::get_extension(const std::string &file) {
  size_t p = file.rfind('.');
  if (p == std::string::npos || p == file.size() - 1)
    return "";
  return file.substr(p + 1);
}
