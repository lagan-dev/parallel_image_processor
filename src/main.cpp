#include <cstdint>
#include <cstring>
#include <image.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include "../stb/stb_image.h"
#include "../stb/stb_image_write.h"
#include "filters.h"

std::string get_extension(const std::string &file) {
  size_t p = file.rfind('.');
  if (p == std::string::npos || p == file.size() - 1)
    return "";
  return file.substr(p + 1);
}

int main() {
  auto image_path = "/home/lagan/projects/2906/parallel_image_processor/images/"
                    "input/landscape.jpg";
  Image in_img;

  auto status = in_img.load(image_path);

  if (!status) {
    std::cout << "Error in loading image\n";
    exit(1);
  }

  std::cout << "start grayscale" << std::endl;

  // Apply grayscale filter
  Image grayscaled_img(in_img.getWidth(), in_img.getHeight(), 1);
  grayscale(grayscaled_img, in_img);
  std::cout << "end grayscale" << std::endl;

  // BorderMode mode = BORDER_CLAMP;
  Image img_blur(grayscaled_img.getWidth(), grayscaled_img.getHeight(),
                 grayscaled_img.getChannels());
  gaussian_blur(img_blur, grayscaled_img, 3, 0.8f);

  // Apply Sobel after grayscale
  std::vector<double> Gx(grayscaled_img.getWidth() *
                         grayscaled_img.getHeight() *
                         grayscaled_img.getChannels());
  std::vector<double> Gy(grayscaled_img.getWidth() *
                         grayscaled_img.getHeight() *
                         grayscaled_img.getChannels());
  sobel(Gx, img_blur, 1, 0, 3);
  sobel(Gy, img_blur, 0, 1, 3);

  std::vector<uint8_t> Gx_u8(grayscaled_img.getWidth() *
                             grayscaled_img.getHeight() *
                             grayscaled_img.getChannels());
  std::vector<uint8_t> Gy_u8(grayscaled_img.getWidth() *
                             grayscaled_img.getHeight() *
                             grayscaled_img.getChannels());

  for (size_t i = 0; i < Gx.size(); i++) {
    Gx_u8[i] = static_cast<uint8_t>(std::clamp(std::abs(Gx[i]), 0.0, 255.0));
    Gy_u8[i] = static_cast<uint8_t>(std::clamp(std::abs(Gy[i]), 0.0, 255.0));
  }
  // Calculate Euclidean distance
  std::vector<uint8_t> out_temp(grayscaled_img.getWidth() *
                                grayscaled_img.getHeight() *
                                grayscaled_img.getChannels());

  for (size_t i = 0; i < out_temp.size(); i++) {
    out_temp[i] = std::sqrt((Gx_u8[i] * Gx_u8[i]) + (Gy_u8[i] * Gy_u8[i]));
  }

  Image out_img(grayscaled_img.getWidth(), grayscaled_img.getHeight(),
                grayscaled_img.getChannels());
  std::memcpy(out_img.getDataMutable(), out_temp.data(), out_temp.size());

  auto output_path = "/home/lagan/projects/2906/parallel_image_processor/"
                     "images/output/landscape.jpg";

  status = out_img.save(output_path);

  if (!status) {
    std::cout << "write failed\n";
  } else {
    std::cout << "write successful\n";
  }

  return status ? 0 : 1;
}
