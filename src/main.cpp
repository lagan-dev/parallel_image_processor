#include <iostream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb/stb_image_write.h"

#include "../include/filters.h"

std::string get_extension(const std::string &file) {
  size_t p = file.rfind('.');
  if (p == std::string::npos || p == file.size() - 1)
    return "";
  return file.substr(p + 1);
}

int main() {
  int width, height, channels;

  auto image_path = "/home/lagan/projects/parallel_image_processor/images/"
                    "input/landscape.jpg";
  auto img = stbi_load(image_path, &width, &height, &channels, 0);

  auto ext = get_extension(image_path);

  if (img == nullptr) {
    std::cout << "Error in loading image\n";
    exit(1);
  }

  size_t img_size = static_cast<size_t>(width) * static_cast<size_t>(height) *
                    static_cast<size_t>(channels);

  printf("Image dimensions: %d %d %d\n", width, height, channels);
  printf("Image size in mem: %zu\n", img_size);
  printf("extension: %s\n", ext.c_str());

  // Apply grayscale filter
  // grayscale(img, width, height, channels);

  BorderMode mode = BORDER_CLAMP;
  gaussian_blur(img, width, height, channels, 0, 8.0f, 8.0f, mode);

  int status = 0;
  if (ext == "jpg") {
    status = stbi_write_jpg("/home/lagan/projects/parallel_image_processor/"
                            "images/output/landscape.jpg",
                            width, height, channels, img, 100);
  }

  if (!status) {
    std::cout << "write failed\n";
  } else {
    std::cout << "write successful\n";
  }

  stbi_image_free(img);
}