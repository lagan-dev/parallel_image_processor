#include "../include/image.h"

#include <cstdint>

// Provide stb implementation in this translation unit only
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <iostream>

#include "../stb/stb_image.h"
#include "../stb/stb_image_write.h"

Image::Image(int width_, int height_, int channels_)
    : width(width_), height(height_), channels(channels_) {
  size_t size = static_cast<size_t>(width_) * height_ * channels_;
  data = static_cast<uint8_t *>(std::malloc(size));
  if (!data) {
    throw std::bad_alloc();
  }
}

Image::~Image() { std::free(data); }

bool Image::load(const std::string &path) {
  data = stbi_load(path.c_str(), &width, &height, &channels, 0);
  extension = get_extension(path);

  return (data != nullptr);
}

bool Image::save(const std::string &path, const int quality = 100) {
  int status;
  auto filename = path.c_str();
  if (extension == "jpg") {
    status = stbi_write_jpg(filename, width, height, channels, data, quality);
  } else if (extension == "png") {
    status = stbi_write_png(filename, width, height, channels, data,
                            width * channels);
  } else if (extension == "bmp") {
    status = stbi_write_bmp(filename, width, height, channels, data);
  } else if (extension == "tga") {
    status = stbi_write_tga(filename, width, height, channels, data);
    //   } else if (extension == "hdr") {
    //     status = stbi_write_hdr(filename, width, height, channels, data);
  } else {
    std::cout << "Unsupported image type: [" << extension << "]" << std::endl;
  }

  return (status != 0);
}

void Image::free() { stbi_image_free(data); }

void Image::printImageInfo() const {
  std::cout << "Width: " << width << ", Height: " << height
            << ", Channels: " << channels << std::endl;
}

std::string Image::get_extension(const std::string &file) {
  size_t p = file.rfind('.');
  if (p == std::string::npos || p == file.size() - 1) return "";
  return file.substr(p + 1);
}
