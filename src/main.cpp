#include <image.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "../stb/stb_image.h"
#include "../stb/stb_image_write.h"
#include "filters.h"

std::string get_extension(const std::string& file) {
  size_t p = file.rfind('.');
  if (p == std::string::npos || p == file.size() - 1) return "";
  return file.substr(p + 1);
}

int main() {
  auto image_path =
      "/home/lagan/projects/2906/parallel_image_processor/images/"
      "input/landscape.jpg";
  Image in_img;

  auto status = in_img.load(image_path);

  if (!status) {
    std::cout << "Error in loading image\n";
    exit(1);
  }

  std::cout << "start grayscale" << std::endl;

  auto start_time = std::chrono::high_resolution_clock::now();

  // Apply grayscale filter
  Image grayscaled_img(in_img.getWidth(), in_img.getHeight(), 1);
  ThreadPool pool(std::thread::hardware_concurrency());
  grayscale(grayscaled_img, in_img, pool, 12);
  std::cout << "end grayscale" << std::endl;

  auto output_path =
      "/home/lagan/projects/2906/parallel_image_processor/"
      "images/output/landscape.jpg";

  // status = out_img.save(output_path);
  status = grayscaled_img.save(output_path);

  auto end_time = std::chrono::high_resolution_clock::now();

  auto time_taken = std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time)
                        .count();

  std::cout << "Time taken for spawn join grayscale: " << time_taken << " ms"
            << std::endl;

  auto start_time2 = std::chrono::high_resolution_clock::now();

  grayscale(grayscaled_img, in_img, pool, 1);
  status = grayscaled_img.save(output_path);

  auto end_time2 = std::chrono::high_resolution_clock::now();
  auto time_taken2 = std::chrono::duration_cast<std::chrono::milliseconds>(
                         end_time2 - start_time2)
                         .count();
  std::cout << "Time taken for thread pool grayscale: " << time_taken2 << " ms"
            << std::endl;

  if (!status) {
    std::cout << "write failed\n";
  } else {
    std::cout << "write successful\n";
  }

  return status ? 0 : 1;
}
