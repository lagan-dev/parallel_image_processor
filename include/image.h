#ifndef IMAGE_H
#define IMAGE_H

#include <cstdint>
#include <string>

// stb image headers are included in a single implementation file
// (src/image.cpp) to avoid multiple-definition linker errors. Do not define
// STB_IMAGE_IMPLEMENTATION or STB_IMAGE_WRITE_IMPLEMENTATION here.

class Image {
 public:
  Image();
  Image(int width, int height, int channels);
  Image(int width, int height, int channels, uint8_t* data);

  ~Image();

  bool load(const std::string &path);

  bool save(const std::string &path, const int quality = 100);

  void free();

  // Function to print the image details
  void printImageInfo() const;

  std::string get_extension(const std::string &file);

  int getWidth() const { return width; }
  int getHeight() const { return height; }
  int getChannels() const { return channels; }
  const uint8_t *getData() const { return data; }
  uint8_t *getDataMutable() { return data; }

 private:
  int width = 0;
  int height = 0;
  int channels = 0;
  uint8_t *data = nullptr;
  std::string extension = "";
};

#endif  // IMAGE_H
